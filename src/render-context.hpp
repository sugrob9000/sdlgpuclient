#pragma once
#include "canvas.hpp"
#include "common.hpp"
#include "gpu-pipelines.hpp"
#include "rect.hpp"
#include "sdl.hpp"
#include <cassert>
#include <fmt/base.h>

namespace sdl {

class render_context {
  constexpr static bool debug_mode = true;

  SDL_Window* window_handle;
  std::unique_ptr<SDL_GPUDevice, constant<SDL_DestroyGPUDevice>> device_handle;

  graphics_pipeline colored_rect_pipeline;
  graphics_pipeline textured_rect_pipeline;

public:
  explicit render_context(SDL_Window* w):
    window_handle(w),
    device_handle(check_sdl_result(
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debug_mode, "vulkan"),
      "SDL_CreateGPUDevice"))
  {
    check_sdl_result(SDL_ClaimWindowForGPUDevice(device(), window()));

    spirv_shader_module vertex_shader(device(), SDL_GPU_SHADERSTAGE_VERTEX,
      (uint8_t[]){
        #embed "rect.vert.spv"
      },
      {.n_uniform_buffers = 1});

    spirv_shader_module fragment_shader(device(), SDL_GPU_SHADERSTAGE_FRAGMENT, (uint8_t[]){
      #embed "rect.frag.spv"
    });

    single_color_target target(SDL_GetGPUSwapchainTextureFormat(device(), window()));

    colored_rect_pipeline = graphics_pipeline(device(), {
      .vertex_shader = vertex_shader,
      .fragment_shader = spirv_shader_module(device(), SDL_GPU_SHADERSTAGE_FRAGMENT, (uint8_t[]){
        #embed "fill.frag.spv"
      }),
      .vertex_input_state = vertex_input_state(&colored_vertex::pos, &colored_vertex::color),
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .target_info = target});

    textured_rect_pipeline = graphics_pipeline(device(), {
      .vertex_shader = vertex_shader,
      .fragment_shader = spirv_shader_module(device(), SDL_GPU_SHADERSTAGE_FRAGMENT, (uint8_t[]){
        #embed "textured.frag.spv"
      }),
      .vertex_input_state = vertex_input_state(&textured_vertex::pos, &textured_vertex::uv),
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .target_info = target});
  }

  ~render_context() {
    if (device()) {
      SDL_ReleaseWindowFromGPUDevice(device(), window_handle);
    }
  }

  SDL_GPUDevice* device() { return device_handle.get(); }
  SDL_Window* window() { return window_handle; }
};


struct [[nodiscard]] frame_in_flight {
  explicit frame_in_flight(render_context& rc):
    parent(rc.device()),
    repr{.cmdbuf = check_sdl_result(SDL_AcquireGPUCommandBuffer(parent))}
  {
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
          repr.cmdbuf, rc.window(), &repr.target,
          &repr.target_dimensions.w, &repr.target_dimensions.h)) {
      sdl_error e(SDL_GetError(), "SDL_WaitAndAcquireGPUSwapchainTexture");
      check_sdl_result(SDL_CancelGPUCommandBuffer(repr.cmdbuf), "SDL_CancelGPUCommandBuffer");
      throw e;
    }
    // past this point and until submit(), error handling is basically impossible...

    repr.pass = check_sdl_result(SDL_BeginGPURenderPass(repr.cmdbuf,
      &temporary(SDL_GPUColorTargetInfo{
        .texture = repr.target,
        .clear_color = {.r=1, .g=1, .b=1, .a=1},
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE }),
      1, nullptr));
  }

  //struct drawcall {
  //  struct texture* texture;
  //  unsigned num_vertices, num_indices;
  //  union {
  //    colored_vertex* colored_vertices;   // when texture is null
  //    textured_vertex* textured_vertices; // when texture is valid
  //  };
  //  uint16_t* indices;
  //};
  void push_drawcall(auto&&) {}

  void submit() {
    SDL_EndGPURenderPass(repr.pass);
    bool ok = SDL_SubmitGPUCommandBuffer(repr.cmdbuf);
    repr = {};
    check_sdl_result(ok, "SDL_SubmitGPUCommandBuffer");
  }

  ~frame_in_flight() {
    if (repr.cmdbuf) {
      // Once a swapchain image has been acquired, SDL_gpu provides no way to
      // get rid of it and of the command buffer without trying to submit it. WTF?
      fmt::println(stderr,
        "Warning: killing frame_in_flight without submitting. "
        "Leaking command buffer and swapchain image. "
        "Expect validation errors and eventual hang.");
    }
  }

  frame_in_flight(frame_in_flight&&) = delete;

private:
  SDL_GPUDevice* parent;
  struct {
    SDL_GPUCommandBuffer* cmdbuf;
    SDL_GPUTexture* target;
    SDL_GPURenderPass* pass;
    rect::dimensions<uint32_t> target_dimensions;
  } repr;
};

} // namespace sdl
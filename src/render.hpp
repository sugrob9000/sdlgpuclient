#pragma once
#include "common.hpp"
#include "sdl.hpp"

namespace sdl {

// ======================================================================================
// Wrappers/helpers for making graphics pipelines

template<typename O, typename I> struct vert_attr {
  I O::* ptr;
  SDL_GPUVertexElementFormat format;
};

template<size_t num_attributes> class vertex_input_state {
  SDL_GPUVertexBufferDescription buffer_description; // TODO: support 2+ buffers?
  SDL_GPUVertexAttribute vertex_attributes[num_attributes];

public:
  explicit vertex_input_state(uint32_t buffer_slot, auto... attr) noexcept {
    static_assert(sizeof...(attr) == num_attributes);
    using vertex_type = common_outer_type_t<decltype(attr.ptr)...>;
    buffer_description = {
      .slot = buffer_slot,
      .pitch = sizeof(vertex_type)
    };
    uint32_t i = 0;
    ([&](uint32_t i){
      vertex_attributes[i] = {
        .location = i,
        .format = attr.format,
        .offset = uint32_t(to_offset(attr.ptr))
      };
    }(i++), ...);
  }

  operator SDL_GPUVertexInputState() noexcept {
    return {
      .vertex_buffer_descriptions = &buffer_description,
      .num_vertex_buffers = 1,
      .vertex_attributes = vertex_attributes,
      .num_vertex_attributes = num_attributes,
    };
  }
};
vertex_input_state(uint32_t, auto... attr) -> vertex_input_state<sizeof...(attr)>;

// ======================================================================================

class render_context {
  constexpr static bool debug_enabled = true; // Validation layers, etc.

  std::unique_ptr<SDL_GPUDevice, constant<SDL_DestroyGPUDevice>> device_handle;
  auto device() { return device_handle.get(); }

  struct colored_vertex {
    struct{ float x,y; } position;
    struct{ uint8_t r,g,b,a; } color;
  };

  graphics_pipeline fill_rect_pipeline;

public:
  explicit render_context(SDL_Window* window):
    device_handle(check_sdl_call(
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debug_enabled, "vulkan"),
      "SDL_CreateGPUDevice"))
  {
    check_sdl_call(SDL_ClaimWindowForGPUDevice(device(), window));

    fill_rect_pipeline = graphics_pipeline(device(), SDL_GPUGraphicsPipelineCreateInfo{
      .vertex_shader = spirv_shader_module(device(), SDL_GPU_SHADERSTAGE_VERTEX, (uint8_t[]){
        #embed "rect.vert.spv"
      }),
      .fragment_shader = spirv_shader_module(device(), SDL_GPU_SHADERSTAGE_FRAGMENT, (uint8_t[]){
        #embed "rect.frag.spv"
      }),
      .vertex_input_state = vertex_input_state(0,
        vert_attr(&colored_vertex::position, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2),
        vert_attr(&colored_vertex::color, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM)),
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .target_info = {
        .color_target_descriptions = &temporary(SDL_GPUColorTargetDescription{
          .format = SDL_GetGPUSwapchainTextureFormat(device(), window)
        }),
        .num_color_targets = 1,
      },
    });
  }
};

} // namespace sdl
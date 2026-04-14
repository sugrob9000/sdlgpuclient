#pragma once
#include "common.hpp"
#include "rect.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <cassert>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>

namespace sdl {

// ================================================================================
// SDL error handling
// When SDL reports errors, it does so by writing a string to be retreived via SDL_GetError()
// and returning false, a negative int, or null. We turn those errors into exceptions.

struct sdl_error: std::runtime_error {
  explicit sdl_error(const char* msg):
    std::runtime_error(msg) {}
  explicit sdl_error(const char* msg, std::string_view what):
    std::runtime_error(fmt::format("{} failed: {}", what, msg)) {}
};

auto check_sdl_result(auto result, auto... what) {
  static_assert(SDL_VERSION_ATLEAST(3,0,0)); // due to SDL_bool etc.
  if (overloaded{
        [](bool b) { return !b; },
        [](int i) { return i < 0; },
        [](auto* p) { return !p; },
        [](auto&&) { static_assert(0, "Unexpected result type in check_sdl_call"); },
      }(result)) {
    throw sdl_error(SDL_GetError(), what...);
  }
  return result;
}


// RAII initializer for SDL
struct init_guard {
  explicit init_guard(SDL_InitFlags flags) {
    // Prevent attempts to double-initialize SDL (although SDL itself is more lenient
    // than we are being here). Check for any subsystems rather than those in flags
    // because we call SDL_Quit() at the end, not SDL_QuitSubSystem()
    assert_release(!SDL_WasInit(0));
    check_sdl_result(SDL_Init(flags));
  }
  init_guard(const init_guard&) = delete;
  ~init_guard() { SDL_Quit(); }
};


// Just enough context to talk to the window manager (have a window and receive events for it)
struct window_context {
  explicit window_context(const char* name, screen_dimensions size):
    window_handle(check_sdl_result(SDL_CreateWindow(name, size.w, size.h, 0))) {}

  static auto poll_event() {
    std::optional<SDL_Event> event(std::in_place);
    if (!SDL_PollEvent(&*event)) {
      event.reset();
    }
    return event;
  }

  SDL_Window* window() noexcept { return window_handle.get(); }

private:
  std::unique_ptr<SDL_Window, constant<SDL_DestroyWindow>> window_handle;
};

// ================================================================================
// SDL_gpu enums

enum class vertex_element_format {
  invalid = SDL_GPU_VERTEXELEMENTFORMAT_INVALID,
  int1 = SDL_GPU_VERTEXELEMENTFORMAT_INT,
  int2 = SDL_GPU_VERTEXELEMENTFORMAT_INT2,
  int3 = SDL_GPU_VERTEXELEMENTFORMAT_INT3,
  int4 = SDL_GPU_VERTEXELEMENTFORMAT_INT4,
  uint1 = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
  uint2 = SDL_GPU_VERTEXELEMENTFORMAT_UINT2,
  uint3 = SDL_GPU_VERTEXELEMENTFORMAT_UINT3,
  uint4 = SDL_GPU_VERTEXELEMENTFORMAT_UINT4,
  float1 = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,
  float2 = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
  float3 = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
  float4 = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
  byte2 = SDL_GPU_VERTEXELEMENTFORMAT_BYTE2,
  byte4 = SDL_GPU_VERTEXELEMENTFORMAT_BYTE4,
  ubyte2 = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2,
  ubyte4 = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4,
  byte2norm = SDL_GPU_VERTEXELEMENTFORMAT_BYTE2_NORM,
  byte4norm = SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM,
  ubyte2norm = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2_NORM,
  ubyte4norm = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
  short2 = SDL_GPU_VERTEXELEMENTFORMAT_SHORT2,
  short4 = SDL_GPU_VERTEXELEMENTFORMAT_SHORT4,
  ushort2 = SDL_GPU_VERTEXELEMENTFORMAT_USHORT2,
  ushort4 = SDL_GPU_VERTEXELEMENTFORMAT_USHORT4,
  short2norm = SDL_GPU_VERTEXELEMENTFORMAT_SHORT2_NORM,
  short4norm = SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM,
  ushort2norm = SDL_GPU_VERTEXELEMENTFORMAT_USHORT2_NORM,
  ushort4norm = SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM,
  half2 = SDL_GPU_VERTEXELEMENTFORMAT_HALF2,
  half4 = SDL_GPU_VERTEXELEMENTFORMAT_HALF4,
};

// ================================================================================
// Wrapping SDL_gpu resource handles

template<typename> struct gpu_resource_create_fn_traits;
template<typename r, typename c> struct gpu_resource_create_fn_traits<r* (*)(SDL_GPUDevice*, const c*)> {
  using create_info = c;
  using resource = r;
};

template<auto create, auto release> class gpu_resource {
  // Like in Vulkan, SDL GPU objects require a parent handle to both create and destroy them.
  // We just store the parent pointer with the resource. (vulkan.hpp does the same thing...)
  SDL_GPUDevice* parent = nullptr;

  using traits = gpu_resource_create_fn_traits<decltype(create)>;
  using resource_pointer = traits::resource*;
  using create_info = traits::create_info;

  resource_pointer resource = nullptr;

public:
  gpu_resource() = default;

  explicit gpu_resource(SDL_GPUDevice* parent, const create_info& ci):
    parent(parent),
    resource(check_sdl_result(create(parent, &ci))) {}

  gpu_resource(gpu_resource&& src) noexcept:
    parent(std::exchange(src.parent, nullptr)),
    resource(std::exchange(src.resource, nullptr)) {}

  gpu_resource& operator=(gpu_resource&& src) noexcept {
    gpu_resource tmp(std::move(src));
    std::swap(parent, tmp.parent);
    std::swap(resource, tmp.resource);
    return *this;
  }

  ~gpu_resource() {
    assert(bool(parent) == bool(resource));
    if (resource) {
      release(parent, resource);
    }
  }

  resource_pointer raw() const noexcept { return resource; }

  explicit operator bool() const noexcept { return resource; }
  operator resource_pointer() const noexcept { return resource; }
};

struct shader_module_binding_info {
  uint32_t n_samplers,
           n_storage_textures,
           n_storage_buffers,
           n_uniform_buffers;
};

struct spirv_shader_module: gpu_resource<SDL_CreateGPUShader, SDL_ReleaseGPUShader> {
  using gpu_resource::gpu_resource;
  explicit spirv_shader_module(SDL_GPUDevice* parent,
                               SDL_GPUShaderStage stage,
                               std::span<const uint8_t> spirv,
                               shader_module_binding_info bindings = {}):
    gpu_resource(parent, {
      .code_size = spirv.size(),
      .code = spirv.data(),
      .entrypoint = "main",
      .format = SDL_GPU_SHADERFORMAT_SPIRV,
      .stage = stage,
      .num_samplers = bindings.n_samplers,
      .num_storage_textures = bindings.n_storage_textures,
      .num_storage_buffers = bindings.n_storage_buffers,
      .num_uniform_buffers = bindings.n_uniform_buffers }) {}
};

struct graphics_pipeline: gpu_resource<SDL_CreateGPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline> {
  using gpu_resource::gpu_resource;
};

} // namespace sdl
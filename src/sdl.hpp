#pragma once
#include "common.hpp"
#include "rect.hpp"
#include <SDL3/SDL.h>
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

auto check_sdl_call(auto result, auto... what) {
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

// ================================================================================
// RAII initializer for SDL
struct init_guard {
  explicit init_guard(SDL_InitFlags flags) {
    // Prevent attempts to double-initialize SDL (although SDL itself is more lenient
    // than we are being here). Check for any subsystems rather than those in flags
    // because we call SDL_Quit() at the end, not SDL_QuitSubSystem()
    assert_release(!SDL_WasInit(0));
    check_sdl_call(SDL_Init(flags));
  }
  init_guard(const init_guard&) = delete;
  ~init_guard() { SDL_Quit(); }
};


// Just enough context to talk to the window manager (have a window and receive events for it)
struct window_context {
  explicit window_context(const char* name, screen_dimensions size):
    window_handle(check_sdl_call(SDL_CreateWindow(name, size.w, size.h, 0))) {}

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
// Wrapping SDL_gpu resource handles

// Like in Vulkan, SDL GPU objects require a parent handle to both create and destroy them.
// We just store the parent pointer with the resource. (vulkan.hpp does the same thing...)
template<auto create, auto release> class gpu_resource {
  using resource_pointer = decltype(create(nullptr, nullptr));
  SDL_GPUDevice* parent = nullptr;
  resource_pointer resource = nullptr;

public:
  gpu_resource() = default;

  explicit gpu_resource(SDL_GPUDevice* parent, const auto& create_info):
    parent(parent),
    resource(check_sdl_call(create(parent, &create_info))) {}

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

  explicit operator bool() const noexcept { return resource; }
  operator resource_pointer() const noexcept { return resource; }
};

struct spirv_shader_module: gpu_resource<SDL_CreateGPUShader, SDL_ReleaseGPUShader> {
  spirv_shader_module() noexcept = default;
  explicit spirv_shader_module(SDL_GPUDevice* parent,
                               SDL_GPUShaderStage stage,
                               std::span<const uint8_t> spirv):
    gpu_resource(parent, SDL_GPUShaderCreateInfo{
      .code_size = spirv.size(), .code = spirv.data(),
      .entrypoint = "main",
      .format = SDL_GPU_SHADERFORMAT_SPIRV,
      .stage = stage }) {}
};

using graphics_pipeline = gpu_resource<SDL_CreateGPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>;

} // namespace sdl
#pragma once
#include "common.hpp"
#include "rect.hpp"
#include <SDL3/SDL.h>
#include <chrono>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <stdexcept>

namespace sdl {

struct sdl_error: std::runtime_error {
  explicit sdl_error(const char* msg):
    std::runtime_error(msg) {}
  explicit sdl_error(std::string_view what, const char* msg):
    std::runtime_error(fmt::format("{} failed: {}", what, msg)) {}
};

// When SDL reports errors, it does so by writing a string to be retreived via SDL_GetError()
// and returning false, a negative int, or null. Turn those errors into exceptions.
auto check_sdl_call(auto result, auto... what) {
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
    check_sdl_call(SDL_Init(flags));
  }
  init_guard(const init_guard&) = delete;
  ~init_guard() { SDL_Quit(); }
};


// Just enough context to talk to the window manager
struct window_context {
  std::unique_ptr<SDL_Window, constant_t<SDL_DestroyWindow>> window;

  explicit window_context(const char* name, screen_dimensions size):
    window(check_sdl_call(SDL_CreateWindow(name, size.w, size.h, 0))) {}

  constexpr static struct immediate_t{} immediate;
  constexpr static struct indefinite_t{} indefinite;
  static auto next_event(auto manner) {
    std::optional<SDL_Event> event(std::in_place);
    if (!overloaded{
          [](immediate_t, SDL_Event* e) { return SDL_PollEvent(e); },
          [](indefinite_t, SDL_Event* e) { return SDL_WaitEvent(e); },
          [](std::chrono::milliseconds ms, SDL_Event* e) { return SDL_WaitEventTimeout(e, ms.count()); },
        }(manner, &*event)) {
      event.reset();
    }
    return event;
  }
};

} // namespace sdl
#include "common.hpp"
#include "rect.hpp"
#include "time.hpp"
#include <SDL3/SDL.h>
#include <chrono>
#include <fmt/base.h>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>

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

using unique_gpu_device = std::unique_ptr<SDL_GPUDevice, constant_t<SDL_DestroyGPUDevice>>;
using unique_window = std::unique_ptr<SDL_Window, constant_t<SDL_DestroyWindow>>;

struct window_context {
  unique_window window;

  explicit window_context(const char* name, screen_dimensions size):
    window(check_sdl_call(
      SDL_CreateWindow(name, size.w, size.h, 0),
      "SDL_CreateWindow")) {}

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

struct render_context {
  unique_gpu_device device;
  constexpr static bool debug = true;

  explicit render_context(SDL_Window* window):
    device(check_sdl_call(
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debug, "vulkan"),
      "SDL_CreateGPUDevice"))
  {
    check_sdl_call(SDL_ClaimWindowForGPUDevice(device.get(), window));
  }
};

} // namespace sdl

struct wm_event_result { bool quit_requested; };

static wm_event_result handle_events(sdl::window_context& wm) {
  wm_event_result result { .quit_requested = false };
  while (auto event = wm.next_event(wm.immediate)) {
    if (event->type == SDL_EVENT_QUIT
        || (event->type == SDL_EVENT_KEY_DOWN
            && event->key.key == SDLK_Q
            && (event->key.mod & SDL_KMOD_SHIFT))) {
      result.quit_requested = true;
    }
  }
  return result;
}

static void render_thread() {
  sdl::init_guard sdl(SDL_INIT_VIDEO);
  sdl::window_context wm("app (shift+Q to quit)", {640, 480});
  sdl::render_context rc(wm.window.get());
  frame_pacer pacer(60);
  while (!handle_events(wm).quit_requested) {
    pacer.wait_next();
  }
}

static void gen_thread(std::stop_token stop) {
  (void) stop;
}

int main() try {
  // SDL insists that window creation & event polling happen on the same thread.
  // Sometimes it mentions the requirement that this thread be the application's
  // main thread; this seems unnecessary on Linux/X11, but we oblige. Therefore,
  // the system main thread performs the duties of the "render thread", while
  // a separate thread that we spawn generates model changes and canvases.
  // We call the former a "gen thread".
  std::jthread gen(gen_thread);
  render_thread();
} catch (std::exception& e) {
  fmt::println(stderr, "Exception: {}", e.what());
  return 1;
}
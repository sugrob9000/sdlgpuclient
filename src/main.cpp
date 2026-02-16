#include "render.hpp"
#include "sdl.hpp"
#include "time.hpp"
#include <SDL3/SDL.h>
#include <thread>

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
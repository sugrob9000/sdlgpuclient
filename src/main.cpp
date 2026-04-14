#include "render-context.hpp"
#include "sdl.hpp"
#include "time.hpp"
#include <SDL3/SDL.h>
#include <fmt/base.h>

static bool handle_events(sdl::window_context& wm) {
  bool keep_going = true;
  while (auto event = wm.poll_event()) {
    if (event->type == SDL_EVENT_QUIT
        || (event->type == SDL_EVENT_KEY_DOWN
            && (event->key.mod & SDL_KMOD_SHIFT)
            && event->key.key == SDLK_Q)) {
      keep_going = false;
    }
  }
  return keep_going;
}

static void render_thread() {
  sdl::init_guard sdl(SDL_INIT_VIDEO);
  sdl::window_context wm("app (shift+Q to quit)", {640, 480});
  sdl::render_context rc(wm.window());
  frame_pacer pacer(60);
  while (handle_events(wm)) {
    pacer.wait_next();
    sdl::frame_in_flight frame(rc);
    frame.submit();
  }
}

int main() try {
  // SDL insists that window creation & event polling happen on the same thread.
  // Sometimes it mentions the requirement that this thread be the application's
  // main thread; this seems unnecessary on Linux/X11, but we oblige. Therefore,
  // the system main thread performs the duties of the "render thread", while
  // a separate thread that we spawn generates model changes and canvases.
  // We call the former a "gen thread".
#if 0
  std::jthread gen([](std::stop_token stop) {
    (void)stop;
    //while (stop.stop_possible() && !stop.stop_requested()) {}
  });
#endif
  render_thread();

} catch (std::exception& e) {
  fmt::println(stderr, "Exception: {}", e.what());
  return 1;
}
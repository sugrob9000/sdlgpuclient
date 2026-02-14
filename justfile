default: build-debug

configure:
  cmake --preset default

build-release:
  cmake --build build --config RelWithDebInfo
alias br := build-release

build-debug:
  cmake --build build --config Debug
alias b := build-debug

run-debug: build-debug
  VK_LOADER_DRIVERS_DISABLE='nvidia*' SDL_LOGGING='*=verbose' ./build/Debug/app
alias r := run-debug
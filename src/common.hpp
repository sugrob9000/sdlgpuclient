#pragma once
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>
#include <utility>

[[noreturn, gnu::always_inline, gnu::cold]]
inline void assertion_failure(const char* assertion, std::source_location loc = std::source_location::current()) {
  fprintf(stderr, "Assertion '%s' failed in '%s' (%s:%d:%d)\n",
          assertion, loc.function_name(), loc.file_name(), loc.line(), loc.column());
  fflush(stderr);
  std::abort();
}
#define assert_release(x) ({\
  if (!(x)) [[unlikely]] {\
    assertion_failure(#x);\
  }\
})


template<typename T> T& unmove(T&& x) { return (T&) x; }


template<auto x> struct constant_t {
  constexpr operator decltype(x)() const { return x; }
};
template<auto x> constexpr constant_t<x> constant;


// TODO: more robust overloaded with support for
// non-class callables, polymorphic class callables, etc.
template<typename... fs> struct overloaded: fs... {
  using fs::operator()...;
};
template<typename... fs> overloaded(fs&&...) -> overloaded<fs...>;


template<typename F> class [[nodiscard]] scope_exit {
  F f;
  bool active = true;

public:
  constexpr scope_exit(F&& f) noexcept(std::is_nothrow_constructible_v<F, F>):
    f(std::forward<F>(f)) {}

  constexpr scope_exit(scope_exit&& src) noexcept(std::is_nothrow_move_constructible_v<F>):
    f(std::move(src.f)),
    active(std::exchange(src.active, false)) {}

  constexpr ~scope_exit() noexcept(std::is_nothrow_destructible_v<F> && noexcept(f())) {
    fire();
  }

  void fire() noexcept(noexcept(f())) {
    if (active) {
      active = false;
      f();
    }
  }

  void release() noexcept {
    active = false;
  }
};
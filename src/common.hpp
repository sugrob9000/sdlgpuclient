#pragma once
#include <cstdio>
#include <cstdlib>
#include <source_location>

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
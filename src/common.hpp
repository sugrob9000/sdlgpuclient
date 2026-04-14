#pragma once
#include <cstdio>
#include <source_location>
#include <type_traits>
#include <utility>

// ======================================================================================
// Random utility...

[[noreturn, gnu::always_inline, gnu::cold]]
inline void assertion_failure(const char* assertion, std::source_location loc = std::source_location::current()) {
  fprintf(stderr, "Assertion '%s' failed in '%s' (%s:%d:%d)\n",
          assertion, loc.function_name(), loc.file_name(), loc.line(), loc.column());
  fflush(stderr);
  __builtin_trap();
}
#define assert_release(x) ({\
  if (!(x)) [[unlikely]] {\
    assertion_failure(#x);\
  }\
})

// For passing temporaries into places that want an lvalue reference or a pointer
template<typename t> t& temporary(t&& x) { return (t&)x; }

// ======================================================================================
// Member pointer traits

template<typename> struct member_ptr_traits;
template<typename parent, typename member> struct member_ptr_traits<member parent::*> {
  using parent_type = parent;
  using member_type = member;
};

// For a pack of member pointers, if they have the same parent type, extract it
template<typename...> struct common_parent_type;
template<typename parent, typename... member> struct common_parent_type<member parent::*...> {
  using type = parent;
};
template<typename... t> using common_parent_type_t = common_parent_type<t...>::type;

// Convert member pointer to byte offset. There still seems no constexpr way to do it.
template<typename parent, typename member> [[gnu::const]] size_t to_offset(member parent::* ptr) {
  static_assert(std::is_standard_layout_v<parent>);
  parent* zero = nullptr;
  return (size_t) &(zero->*ptr); // boost::intrusive does it this way...
}

// ======================================================================================
// Type <-> value mappings
template<auto x> struct constant {
  using type = decltype(x);
  constexpr static type value = x;
  constexpr operator type() const { return x; }
};
template<auto x> constexpr inline constant<x> constant_v;

template<typename t> struct type_t { using type = t; };
template<typename t> constexpr inline type_t<t> type_v;

// ======================================================================================
template<typename... f> struct overloaded: f... {
  using f::operator()...;
};
template<typename... f> overloaded(f...) -> overloaded<f...>;

// ======================================================================================
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
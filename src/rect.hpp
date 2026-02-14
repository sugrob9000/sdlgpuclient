#pragma once

namespace rect {

template<typename T> struct rect { T x, y, w, h; };

template<typename T> struct dimensions {
  constexpr static T x = 0, y = 0;
  T w, h;
};

} // namespace rect

using screen_rect = rect::rect<short>;
using screen_dimensions = rect::dimensions<short>;
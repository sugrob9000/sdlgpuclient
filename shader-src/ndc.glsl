layout(set=1, binding=0) uniform u { vec2 viewport_size; };
vec2 to_ndc(vec2 pixel_pos) { return pixel_pos / viewport_size * 2 - 1; }
#version 450

layout(location=0) in vec2 pos;
layout(location=1) in vec3 color;
layout(location=0) out vec3 vertex_color;
layout(set=1, binding=0) uniform u { vec2 viewport_size; };

void main() {
  vec2 ndc = pos / viewport_size * 2 - 1;
  gl_Position = vec4(ndc, 0, 1);
  vertex_color = color;
}
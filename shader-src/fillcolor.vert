#version 450
#include "ndc.glsl"

layout(location=0) in vec2 pos;
layout(location=1) in vec3 color;

layout(location=0) out vec3 out_color;

void main() {
  gl_Position = vec4(to_ndc(pos), 0, 1);
  out_color = color;
}
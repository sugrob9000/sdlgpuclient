#version 450
layout(location=0) in vec3 vertex_color;
layout(location=0) out vec3 result_color;
void main() { result_color = vertex_color; }
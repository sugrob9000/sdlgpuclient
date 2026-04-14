#pragma once
#include "sdl.hpp"

namespace sdl {
// ======================================================================================
// Helpers for making graphics pipelines

// Vertex input state for when there is only one buffer filled with one type of vertex.
// TODO: support 2+ buffers?
template<size_t n_attributes> class [[nodiscard]] vertex_input_state {
  constexpr static uint32_t buffer_slot = 0;
  SDL_GPUVertexBufferDescription buffer_description;
  SDL_GPUVertexAttribute vertex_attributes[n_attributes];

public:
  explicit vertex_input_state(auto... attr) noexcept {
    static_assert(sizeof...(attr) == n_attributes);
    using vertex_type = common_parent_type_t<decltype(attr)...>;
    auto format_of = [](auto attr) {
      using attr_type = member_ptr_traits<decltype(attr)>::member_type;
      return attr_type::vertex_format;
    };
    buffer_description = {.slot = buffer_slot, .pitch = sizeof(vertex_type)};
    uint32_t i = 0;
    ([&](uint32_t i) {
      vertex_attributes[i] = {
        .location = i,
        .buffer_slot = buffer_slot,
        .format = SDL_GPUVertexElementFormat(format_of(attr)),
        .offset = uint32_t(to_offset(attr)) };
    }(i++), ...);
  }

  operator SDL_GPUVertexInputState() const noexcept {
    return {.vertex_buffer_descriptions = &buffer_description,
            .num_vertex_buffers = 1,
            .vertex_attributes = vertex_attributes,
            .num_vertex_attributes = n_attributes};
  }
};
vertex_input_state(auto... attr) -> vertex_input_state<sizeof...(attr)>;

// Color target info for when there's only one color target
// (e.g. just rendering directly into the swapchain image)
struct [[nodiscard]] single_color_target {
  SDL_GPUColorTargetDescription color_target_description;

  explicit single_color_target(SDL_GPUTextureFormat texture_format) noexcept:
    color_target_description{.format = texture_format} {}

  operator SDL_GPUGraphicsPipelineTargetInfo() const noexcept {
    return {.color_target_descriptions = &color_target_description,
            .num_color_targets = 1};
  }
};

// ======================================================================================
// Known vertex formats

template<vertex_element_format x> struct with_attribute_format { constexpr static auto vertex_format = x; };

struct colored_vertex {
  using enum vertex_element_format;
  struct: with_attribute_format<float2> { float x,y; } pos;
  struct: with_attribute_format<ubyte4norm> { uint8_t r,g,b,a; } color;
};

struct textured_vertex {
  using enum vertex_element_format;
  struct: with_attribute_format<float2> { float x,y; } pos;
  struct: with_attribute_format<half2> { _Float16 u,v; } uv;
};

} // namespace sdl
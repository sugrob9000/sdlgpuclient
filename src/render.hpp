#include "sdl.hpp"

namespace sdl {

class render_context {
  SDL_GPUDevice* device() { return device_handle.get(); }

  class shader_module {
    SDL_GPUDevice* device;
    SDL_GPUShader* shader;
  public:
    explicit shader_module(SDL_GPUDevice* device,
                           SDL_GPUShaderStage stage,
                           std::span<const uint8_t> spirv):
      device(device)
    {
      SDL_GPUShaderCreateInfo ci = {
        .code_size = spirv.size(),
        .code = spirv.data(),
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = stage,
      };
      shader = check_sdl_call(SDL_CreateGPUShader(device, &ci), "create shader");
    }
    shader_module(const shader_module&) = delete;
    ~shader_module() { SDL_ReleaseGPUShader(device, shader); }
    operator SDL_GPUShader*() const { return shader; }
  };

  constexpr static bool debug_enabled = true; // Validation layers, etc.
  std::unique_ptr<SDL_GPUDevice, constant_t<SDL_DestroyGPUDevice>> device_handle;
  SDL_GPUGraphicsPipeline* fill_rect_pipeline = nullptr;

public:
  explicit render_context(SDL_Window* window):
    device_handle(check_sdl_call(
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debug_enabled, "vulkan"),
      "SDL_CreateGPUDevice"))
  {
    check_sdl_call(SDL_ClaimWindowForGPUDevice(device(), window));

    static constexpr uint8_t vert_code[] = {
      #embed "rect.vert.spv"
    };
    static constexpr uint8_t frag_code[] = {
      #embed "rect.frag.spv"
    };
    shader_module vert_shader(device(), SDL_GPU_SHADERSTAGE_VERTEX, vert_code);
    shader_module frag_shader(device(), SDL_GPU_SHADERSTAGE_FRAGMENT, frag_code);

    SDL_GPUVertexBufferDescription buffer_description = {.pitch = 12};
    SDL_GPUVertexAttribute vertex_attributes[] = {
      {.location = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = 0}, // position
      {.location = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, .offset = 8}, // color
    };
    SDL_GPUColorTargetDescription color_target = {
      .format = SDL_GetGPUSwapchainTextureFormat(device(), window)
    };

    SDL_GPUGraphicsPipelineCreateInfo pci = {
      .vertex_shader = vert_shader,
      .fragment_shader = frag_shader,
      .vertex_input_state = {
        .vertex_buffer_descriptions = &buffer_description,
        .num_vertex_buffers = 1,
        .vertex_attributes = vertex_attributes,
        .num_vertex_attributes = std::size(vertex_attributes)
      },
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .rasterizer_state = {
        .fill_mode = SDL_GPU_FILLMODE_FILL,
        .cull_mode = SDL_GPU_CULLMODE_NONE,
      },
      .target_info = {
        .color_target_descriptions = &color_target,
        .num_color_targets = 1
      },
    };
    fill_rect_pipeline = check_sdl_call(
      SDL_CreateGPUGraphicsPipeline(device(), &pci),
      "create rect pipeline");
  }

  render_context(const render_context&) = delete;

  ~render_context() {
    SDL_ReleaseGPUGraphicsPipeline(device(), fill_rect_pipeline);
  }
};

} // namespace sdl
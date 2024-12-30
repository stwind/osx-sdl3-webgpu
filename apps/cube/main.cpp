#include <SDL3/SDL.h>
#include "wgpu.hpp"

const char* shaderSource = R"(
struct VSOutput {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
};

@vertex fn vs_main(
  @location(0) position: vec3f,
  @location(1) normal: vec3f) -> VSOutput {

  return VSOutput(vec4f(position, 1), normal);
}

@fragment fn fs_main(@location(0) normal: vec3f) -> @location(0) vec4f {
	return vec4f(pow(normalize(normal) * .5 + .5, vec3f(2.2)), 1.);
}
)";

class Application {
public:
  WGPU wgpu = WGPU(1280, 720);
  WGPURenderPipeline pipeline;
  WGPUBuffer vertexBuffer;
  WGPUBuffer indexBuffer;

  Application() {
    float s = .5;
    std::vector<float> vertexData = {
      s, s, -s, 1, 0, 0,
      s, s, s, 1, 0, 0,
      s, -s, s, 1, 0, 0,
      s, -s, -s, 1, 0, 0,
      -s, s, s, -1, 0, 0,
      -s, s, -s, -1, 0, 0,
      -s, -s, -s, -1, 0, 0,
      -s, -s, s, -1, 0, 0,
      -s, s, s, 0, 1, 0,
      s, s, s, 0, 1, 0,
      s, s, -s, 0, 1, 0,
      -s, s, -s, 0, 1, 0,
      -s, -s, -s, 0, -1, 0,
      s, -s, -s, 0, -1, 0,
      s, -s, s, 0, -1, 0,
      -s, -s, s, 0, -1, 0,
      s, s, s, 0, 0, 1,
      -s, s, s, 0, 0, 1,
      -s, -s, s, 0, 0, 1,
      s, -s, s, 0, 0, 1,
      -s, s, -s, 0, 0, -1,
      s, s, -s, 0, 0, -1,
      s, -s, -s, 0, 0, -1,
      -s, -s, -s, 0, 0, -1,
    };
    WGPUBufferDescriptor bufferDesc = {
      .size = vertexData.size() * sizeof(float),
      .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
    };
    vertexBuffer = wgpuDeviceCreateBuffer(wgpu.device, &bufferDesc);
    wgpuQueueWriteBuffer(wgpu.queue, vertexBuffer, 0, vertexData.data(), bufferDesc.size);

    std::vector<uint16_t> indexData = {
      0, 1, 2,
      0, 2, 3,
      4, 5, 6,
      4, 6, 7,
      8, 9, 10,
      8, 10, 11,
      12, 13, 14,
      12, 14, 15,
      16, 17, 18,
      16, 18, 19,
      20, 21, 22,
      20, 22, 23
    };
    bufferDesc.size = indexData.size() * sizeof(uint16_t);
    bufferDesc.size = (bufferDesc.size + 3) & ~3; // round up to the next multiple of 4
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;;
    indexBuffer = wgpuDeviceCreateBuffer(wgpu.device, &bufferDesc);
    wgpuQueueWriteBuffer(wgpu.queue, indexBuffer, 0, indexData.data(), bufferDesc.size);

    WGPUShaderModule shaderModule = wgpu.createShaderModule(shaderSource);
    pipeline = wgpu.createRenderPipeline(new WGPURenderPipelineDescriptor{
      .vertex = {
        .bufferCount = 1,
        .buffers = new WGPUVertexBufferLayout[1]{
          {
            .attributeCount = 2,
            .attributes = new WGPUVertexAttribute[2]{
              {.shaderLocation = 0, .format = WGPUVertexFormat_Float32x3, .offset = 0 },
              {.shaderLocation = 1, .format = WGPUVertexFormat_Float32x3, .offset = 3 * sizeof(float) }
            },
            .arrayStride = 6 * sizeof(float),
            .stepMode = WGPUVertexStepMode_Vertex
          }
        },
        .module = shaderModule,
        .entryPoint = "vs_main",
        .constantCount = 0,
      },
      .primitive = {
        .topology = WGPUPrimitiveTopology_TriangleList,
        .stripIndexFormat = WGPUIndexFormat_Undefined,
        .frontFace = WGPUFrontFace_CCW,
        .cullMode = WGPUCullMode_None,
      },
      .fragment = new WGPUFragmentState{
        .module = shaderModule,
        .entryPoint = "fs_main",
        .constantCount = 0,
        .targetCount = 1,
        .targets = new WGPUColorTargetState{
          .format = wgpu.surfaceFormat,
          .blend = new WGPUBlendState{
            .color = {
              .srcFactor = WGPUBlendFactor_SrcAlpha,
              .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
              .operation = WGPUBlendOperation_Add
            },
            .alpha = {
              .srcFactor = WGPUBlendFactor_Zero,
              .dstFactor = WGPUBlendFactor_One,
              .operation = WGPUBlendOperation_Add
            }
          },
          .writeMask = WGPUColorWriteMask_All
        }
      },
      .multisample = {
        .count = 1,
        .mask = ~0u,
        .alphaToCoverageEnabled = false
      }
      });
    wgpuShaderModuleRelease(shaderModule);
  }

  ~Application() {
    wgpuRenderPipelineRelease(pipeline);
  }

  void render() {
    WGPUTextureView view = wgpu.surfaceTextureCreateView();

    WGPUCommandEncoder encoder = wgpu.createCommandEncoder(new WGPUCommandEncoderDescriptor{});
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, new WGPURenderPassDescriptor{
      .colorAttachmentCount = 1,
      .colorAttachments = new WGPURenderPassColorAttachment{
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
        .view = view,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = WGPUColor{ 0., 0., 0., 1. },
      }
      });
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertexBuffer, 0, wgpuBufferGetSize(vertexBuffer));
    wgpuRenderPassEncoderSetIndexBuffer(pass, indexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(indexBuffer));
    wgpuRenderPassEncoderDrawIndexed(pass, 36, 1, 0, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, new WGPUCommandBufferDescriptor{});
    wgpuCommandEncoderRelease(encoder);
    wgpu.queueSubmit(1, &command);
    wgpuCommandBufferRelease(command);

    wgpu.present();
    wgpu.surfaceTextureViewRelease(view);
  }
};

int main(int argc, char** argv) try {
  Application app;

  SDL_Event event;
  for (bool running = true; running;) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) running = false;
    }

    app.render();
  }

  SDL_Log("Quit");
}
catch (std::exception const& e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}

#include <SDL3/SDL.h>
#include "wgpu.hpp"
#include "imgui.hpp"

const char* shaderSource = R"(
struct Camera {
	view : mat4x4f,
	proj : mat4x4f,
}

struct VSOutput {
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
};

@group(0) @binding(0) var<uniform> camera : Camera;
@group(0) @binding(1) var<uniform> model : mat4x4f;

@vertex fn vs(
  @location(0) position: vec3f,
  @location(1) normal: vec3f) -> VSOutput {

  let pos = camera.proj * camera.view * model * vec4f(position, 1);
  return VSOutput(pos, normal);
}

@fragment fn fs(@location(0) normal: vec3f) -> @location(0) vec4f {
	return vec4f(pow(normalize(normal) * .5 + .5, vec3f(2.2)), 1.);
}
)";

struct CameraUniform {
  std::array<float, 16> view;
  std::array<float, 16> proj;
};

std::array<float, 16> perspective(float fov, float aspect, float near, float far) {
  std::array<float, 16> mat = { 0 };
  float f = std::tan(M_PI * 0.5 - 0.5 * fov);
  mat[0] = f / aspect; mat[1] = 0; mat[2] = 0; mat[3] = 0;
  mat[4] = 0; mat[5] = f; mat[6] = 0; mat[7] = 0;
  mat[8] = 0; mat[9] = 0; mat[11] = -1;
  mat[12] = 0; mat[13] = 0; mat[15] = 0;

  if (std::isfinite(far)) {
    float rangeInv = 1 / (near - far);
    mat[10] = far * rangeInv;
    mat[14] = far * near * rangeInv;
  }
  else {
    mat[10] = -1;
    mat[14] = -near;
  }
  return mat;
};

class CubeData {
public:
  std::vector<float> vertices;
  std::vector<uint16_t> indices{
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

  CubeData(float s = .5) {
    vertices = {
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
  }
};

using Vec3 = std::array<float, 3>;
using Mat44 = std::array<float, 16>;
using Quaternion = std::array<float, 4>;

inline Vec3& sph2cart(Vec3& out, const Vec3& v) {
  float az = v[0], el = v[1], r = v[2];
  float c = std::cos(el);
  out[0] = c * std::cos(az) * r;
  out[1] = c * std::sin(az) * r;
  out[2] = std::sin(el) * r;
  return out;
}

inline float dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline float norm(const Vec3& v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
inline float norm(const Quaternion& v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]); }

inline Vec3& normalize(Vec3& out, const Vec3& v, float eps = 1e-12) {
  float n = 1. / norm(v);
  out[0] = v[0] * n; out[1] = v[1] * n; out[2] = v[2] * n;
  return out;
}

inline Quaternion& normalize(Quaternion& out, const Quaternion& v, float eps = 1e-12) {
  float n = 1. / norm(v);
  out[0] = v[0] * n; out[1] = v[1] * n; out[2] = v[2] * n; out[3] = v[3] * n;
  return out;
}

inline Vec3& orthogonal(Vec3& out, const Vec3& v, float m = .5, float n = .5) {
  out[0] = m * -v[1] + n * -v[2];
  out[1] = m * v[0];
  out[2] = n * v[0];
  return normalize(out, out);
}

inline Quaternion& axisAngle(Quaternion& quat, const Vec3& axis, float rad) {
  rad *= .5;
  float s = std::sin(rad);
  quat[0] = s * axis[0];
  quat[1] = s * axis[1];
  quat[2] = s * axis[2];
  quat[3] = std::cos(rad);
  return quat;
}

inline Quaternion& between(Quaternion& out, const Vec3& a, const Vec3& b) {
  float w = dot(a, b);
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
  out[3] = w + std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2] + w * w);
  if (out[0] == 0. && out[1] == 0. && out[2] == 0. && out[3] == 0.) {
    Vec3 axis;
    return axisAngle(out, orthogonal(axis, a), M_PI);
  }

  return normalize(out, out);
}

inline Quaternion& betweenZ(Quaternion& out, const Vec3& b) {
  float w = b[2];
  out[0] = -b[1]; out[1] = b[0]; out[2] = 0.;
  out[3] = w + std::sqrt(out[0] * out[0] + out[1] * out[1] + w * w);
  if (out[0] == 0. && out[1] == 0. && out[2] == 0. && out[3] == 0.) {
    out[1] = 1;
    return out;
  }

  return normalize(out, out);
}

inline Mat44& rotation(Mat44& mat, const Quaternion& quat) {
  float x = quat[0], y = quat[1], z = quat[2], w = quat[3];
  float x2 = x + x, y2 = y + y, z2 = z + z;
  float xx = x * x2, xy = x * y2, xz = x * z2;
  float yy = y * y2, yz = y * z2, zz = z * z2;
  float wx = w * x2, wy = w * y2, wz = w * z2;

  mat[0] = 1 - (yy + zz); mat[1] = xy + wz; mat[2] = xz - wy; mat[3] = 0;
  mat[4] = xy - wz; mat[5] = 1 - (xx + zz); mat[6] = yz + wx; mat[7] = 0;
  mat[8] = xz + wy; mat[9] = yz - wx; mat[10] = 1 - (xx + yy); mat[11] = 0;
  mat[12] = 0; mat[13] = 0; mat[14] = 0; mat[15] = 1;
  return mat;
}

class CubeGeometry {
public:
  CubeData data;

  WGPU::Buffer vertexBuffer;
  WGPU::Buffer indexBuffer;
  WGPU::IndexedGeometry geom;

  WGPU::RenderPipeline pipeline;

  CubeGeometry(WGPU::Context& ctx, const std::vector<WGPU::RenderPipeline::BindGroupEntry>& bindGroups)
    : vertexBuffer(ctx, {
      .label = "vertex",
      .size = data.vertices.size() * sizeof(float),
      .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
      .mappedAtCreation = false
      }),
    indexBuffer(ctx, {
      .label = "index",
      .size = (data.indices.size() * sizeof(uint16_t) + 3) & ~3, // round up to the next multiple of 4
      .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
      .mappedAtCreation = false
      }),
    geom{
      .primitive = {
        .topology = WGPUPrimitiveTopology_TriangleList,
        .stripIndexFormat = WGPUIndexFormat_Undefined,
        .frontFace = WGPUFrontFace_CCW,
        .cullMode = WGPUCullMode_Back,
      },
      .vertexBuffers = {
        {
          .buffer = vertexBuffer,
          .attributes = {
            {.shaderLocation = 0, .format = WGPUVertexFormat_Float32x3, .offset = 0 },
            {.shaderLocation = 1, .format = WGPUVertexFormat_Float32x3, .offset = 3 * sizeof(float) }
          },
          .arrayStride = 6 * sizeof(float),
          .stepMode = WGPUVertexStepMode_Vertex
        }
      },
      .indexBuffer = indexBuffer,
      .count = static_cast<uint32_t>(data.indices.size()),
      },
    pipeline(ctx, {
        .source = shaderSource,
        .bindGroups = bindGroups,
        .vertex = {
          .entryPoint = "vs",
          .buffers = geom.vertexBuffers,
        },
        .primitive = geom.primitive,
        .fragment = {
          .entryPoint = "fs",
          .targets = {
            {
              .format = ctx.surfaceFormat,
              .blend = std::make_unique<WGPUBlendState>(WGPUBlendState{
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
              }).get(),
              .writeMask = WGPUColorWriteMask_All
            }
          }
        },
        .multisample = {
          .count = 1,
          .mask = ~0u,
          .alphaToCoverageEnabled = false
        }
      }
    )
  {
    geom.vertexBuffers[0].buffer.write(data.vertices.data());
    geom.indexBuffer.write(data.indices.data());
  }
};

class Application {
public:
  WGPU::Context ctx = WGPU::Context(1280, 720);

  WGPU::Buffer camera;
  WGPU::Buffer model;

  CubeGeometry cube;

  // WGPU::RenderPipeline pipeline;

  struct {
    bool isDown = false;
    ImVec2 downPos = { -1,-1 };
    ImVec2 delta = { 0,0 };

    float phi = 0;
    float theta = M_PI_2;
  } state;

  Application()
    : camera(ctx, {
      .label = "camera",
      .size = sizeof(CameraUniform),
      .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
      .mappedAtCreation = false,
      }),
      model(ctx, {
        .label = "model",
        .size = 16 * sizeof(float),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
        .mappedAtCreation = false,
        }),
        cube(ctx, {
          {
            .label = "camera",
            .entries = {
              {
                .binding = 0,
                .buffer = &camera,
                .offset = 0,
                .visibility = WGPUShaderStage_Vertex,
                .layout = {
                  .type = WGPUBufferBindingType_Uniform,
                  .hasDynamicOffset = false,
                  .minBindingSize = camera.size,
                  }
                },
                {
                  .binding = 1,
                  .buffer = &model,
                  .offset = 0,
                  .visibility = WGPUShaderStage_Vertex,
                  .layout = {
                    .type = WGPUBufferBindingType_Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = model.size,
                  }
                }
              }
              }
          })
  {
    if (!ImGui_init(&ctx)) throw std::runtime_error("ImGui_init failed");

    CameraUniform uniformData{
      .view = std::array<float, 16>{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,-5,1,
      },
      .proj = perspective(45 * M_PI / 180, ctx.aspect,.1,100),
    };
    camera.write(&uniformData);
  }

  ~Application() {
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }

  void processEvent(const SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);
  }

  void render() {
    Vec3 vec;
    Quaternion rot;
    betweenZ(rot, sph2cart(vec, { state.phi, state.theta,1. }));

    Mat44 m;
    rotation(m, rot);
    model.write(&m);

    WGPUTextureView view = ctx.surfaceTextureCreateView();
    std::vector<WGPUCommandBuffer> commands;

    {
      WGPUCommandEncoderDescriptor encoderDescriptor{};
      WGPU::CommandEncoder encoder(ctx, &encoderDescriptor);

      WGPURenderPassColorAttachment attachment{
       .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
       .view = view,
       .loadOp = WGPULoadOp_Clear,
       .storeOp = WGPUStoreOp_Store,
       .clearValue = WGPUColor{ 0., 0., 0., 1. }
      };
      WGPURenderPassDescriptor passDescriptor{
        .colorAttachmentCount = 1,
        .colorAttachments = &attachment
      };
      WGPU::RenderPass pass = encoder.renderPass(&passDescriptor);
      pass.setPipeline(cube.pipeline);
      pass.draw(cube.geom);
      pass.end();

      WGPUCommandBufferDescriptor commandDescriptor{};
      commands.push_back(encoder.finish(&commandDescriptor));
    }

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();

    if (state.isDown != ImGui::IsMouseDown(0)) {
      if (!state.isDown) {
        state.downPos.x = io.MousePos.x;
        state.downPos.y = io.MousePos.y;
      }
      else {
        state.downPos.x = -1.;
        state.downPos.y = -1.;
      }
    }
    state.isDown = ImGui::IsMouseDown(0);
    if (state.isDown) {
      state.delta.x = io.MousePos.x - state.downPos.x;
      state.delta.y = io.MousePos.y - state.downPos.y;
    }
    else {
      state.delta.x = 0.;
      state.delta.y = 0.;
    }

    {
      ImGui::SetNextWindowPos(ImVec2(10, 120), ImGuiCond_Once);
      ImGui::SetNextWindowSize(ImVec2(200, 0), ImGuiCond_Once);
      ImGui::Begin("Controls");
      ImGui::SliderFloat("phi", &state.phi, 0.0f, M_PI * 2.);
      ImGui::SliderFloat("theta", &state.theta, -M_PI_2, M_PI_2);

      ImGui::End();
    }

    {
      ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
      ImGui::SetNextWindowSize(ImVec2(200, 70), ImGuiCond_Once);
      ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

      if (ImGui::IsMousePosValid())
        ImGui::Text("Mouse pos: (%g, %g)", io.MousePos.x, io.MousePos.y);

      ImGui::Text("down pos: (%g, %g)", state.downPos.x, state.downPos.y);
      ImGui::Text("delta: (%g, %g)", state.delta.x, state.delta.y);

      ImGui::End();
    }
    ImGui::Render();
    commands.push_back(ImGui_command(ctx, view));

    ctx.submitCommands(commands);
    ctx.releaseCommands(commands);

    ctx.present();
    ctx.surfaceTextureViewRelease(view);
  }
};

int main(int argc, char** argv) try {
  Application app;

  SDL_Event event;
  for (bool running = true; running;) {
    while (SDL_PollEvent(&event)) {
      app.processEvent(&event);
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

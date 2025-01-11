#pragma once

#include "wgpu.hpp"
#include "math.hpp"
#include "imgui.hpp"

class WGPUApplication {
public:
  WGPU::Context ctx;

  WGPUApplication(int w, int h) : ctx(w, h) {
    if (!ImGui_init(&ctx)) throw std::runtime_error("ImGui_init failed");
  }

  ~WGPUApplication() {
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }

  void processEvent(const SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);
  }
};

struct Object3d {
  Eigen::Vector3f position;
  Eigen::Quaternionf rotation;
  Eigen::Vector3f up;
};

struct Perspective {
  float fov;
  float aspect;
  float near;
  float far;
};

inline void lookAt(Eigen::Ref<Eigen::Matrix4f> mat, const Object3d& obj) {
  Eigen::Vector3f dir;
  math::lookAt(mat, obj.position, math::mulVZ(dir, obj.rotation), obj.up);
}

struct Camera {
  Object3d object;
  Perspective perspective;
};

class ArcBall {
public:
  Eigen::Vector3f p0;

  void begin(const Eigen::Vector2f& p) {
    math::arcballHolroyd(p0, p);
  }

  Eigen::Quaternionf& end(Eigen::Quaternionf& out, const Eigen::Vector2f& p, const float speed = 2.) {
    Eigen::Vector3f p1;
    Eigen::Vector3f delta = p0 - math::arcballHolroyd(p1, p);
    float angle = delta.squaredNorm() * speed;
    Eigen::Vector3f axis(-delta.y(), delta.x(), 0);
    axis.normalize();
    return math::axisAngle(out, axis, angle);
  }
};

class OrbitControl {
public:
  Eigen::Vector3f t0;
  Eigen::Vector3f up;
  Eigen::Quaternionf r0;
  Eigen::Quaternionf inv;

  Eigen::Quaternionf rot;
  Eigen::Quaternionf ru;

  ArcBall arcball;
  Object3d& obj;

  OrbitControl(Object3d& obj) : obj(obj) {}

  void begin(const Eigen::Vector2f& p) {
    up = math::invert(inv, obj.rotation) * obj.up;
    math::betweenY(ru, up);

    arcball.begin(p);
    t0 = obj.position;
    r0.coeffs() = obj.rotation.coeffs();
  }

  void end(const Eigen::Vector2f& p, const Eigen::Vector3f& target) {
    obj.rotation = r0 * ru * arcball.end(rot, p);
    obj.position = (obj.rotation * inv) * (t0 - target) + target;
    obj.up = obj.rotation * up;
  }
};
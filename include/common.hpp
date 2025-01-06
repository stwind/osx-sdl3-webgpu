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

class Object3d {
public:
  Eigen::Vector3f position;
  Eigen::Quaternionf rotation;
  Eigen::Vector3f up;

  Object3d(
    const Eigen::Vector3f& position,
    const Eigen::Quaternionf& rotation,
    const Eigen::Vector3f& up) : position(position), rotation(rotation), up(up)
  {
  }
};

class ArcBall {
public:
  Eigen::Vector3f p0;
  Eigen::Quaternionf rot;

  void begin(const Eigen::Vector2f& p, const Eigen::Vector3f& up) {
    math::arcballHolroyd(p0, p);
    math::betweenY(rot, up);
  }

  Eigen::Quaternionf& end(Eigen::Quaternionf& out, const Eigen::Vector2f& p, const float speed = 2.) {
    Eigen::Vector3f p1;
    Eigen::Vector3f delta = p0 - math::arcballHolroyd(p1, p);
    float angle = delta.squaredNorm() * speed;
    Eigen::Vector3f axis(delta.y(), delta.x(), 0);
    axis.normalize();
    return math::axisAngle(out, rot * axis, angle);
  }
};

class ArcBallControl {
public:
  Eigen::Vector3f t0;
  Eigen::Vector3f up;
  Eigen::Quaternionf r0;
  Eigen::Quaternionf inv;

  ArcBall arcball;

  void begin(Object3d& obj, const Eigen::Vector2f& p) {
    up = math::invert(inv, obj.rotation) * obj.up;

    arcball.begin(p, up);
    t0 = obj.position;
    r0.coeffs() = obj.rotation.coeffs();
  }

  void end(Object3d& obj, const Eigen::Vector3f& target, const Eigen::Vector2f& p) {
    Eigen::Quaternionf rot;

    obj.rotation = r0 * arcball.end(rot, p);
    obj.position = (obj.rotation * inv) * (t0 - target) + target;
    obj.up = obj.rotation * up;
  }
};
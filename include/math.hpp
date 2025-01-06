#pragma once

#include <array>
#include <Eigen/Core>
#include <Eigen/Geometry>

namespace math {
  inline float radians(float x) { return x * M_PI / 180.0f; };

  inline Eigen::Ref<Eigen::Vector3f> sph2cart(Eigen::Ref<Eigen::Vector3f> out, Eigen::Ref<const Eigen::Vector3f> v) {
    float az = v(0), el = v(1), r = v(2);
    float c = std::cos(el);
    out(0) = c * std::cos(az) * r;
    out(1) = c * std::sin(az) * r;
    out(2) = std::sin(el) * r;
    return out;
  }

  inline void orthogonal(Eigen::Ref<Eigen::Vector3f> out, Eigen::Ref<const Eigen::Vector3f> v, const float m = .5, const float n = .5) {
    out(0) = m * -v(1) + n * -v(2);
    out(1) = m * v(0);
    out(2) = n * v(0);
    out.normalize();
  }

  inline Eigen::Quaternionf& axisAngle(Eigen::Quaternionf& quat, Eigen::Ref<const Eigen::Vector3f> axis, float rad) {
    rad *= .5;
    float s = std::sin(rad);
    quat.coeffs() << s * axis(0), s* axis(1), s* axis(2), std::cos(rad);
    return quat;
  }

  inline Eigen::Quaternionf& between(Eigen::Quaternionf& out, Eigen::Ref<const Eigen::Vector3f> a, Eigen::Ref<const Eigen::Vector3f> b) {
    float w = a.dot(b);
    out.x() = a.y() * b.z() - a.z() * b.y();
    out.y() = a.z() * b.x() - a.x() * b.z();
    out.z() = a.x() * b.y() - a.y() * b.x();
    out.w() = w + std::sqrt(out.x() * out.x() + out.y() * out.y() + out.z() * out.z() + w * w);
    if (out.x() == 0. && out.y() == 0. && out.z() == 0. && out.w() == 0.) {
      Eigen::Vector3f axis;
      orthogonal(axis, a);
      axisAngle(out, axis, M_PI);
    }
    out.normalize();
    return out;
  }

  inline Eigen::Quaternionf& betweenY(Eigen::Quaternionf& out, Eigen::Ref<const Eigen::Vector3f> b) {
    float w = b(1);
    out.x() = b.z(); out.y() = 0.; out.z() = -b.x();
    out.w() = w + std::sqrt(out.x() * out.x() + out.z() * out.z() + w * w);
    if (out.x() == 0. && out.y() == 0. && out.z() == 0. && out.w() == 0.)
      out.x() = 1;
    else
      out.normalize();
    return out;
  }

  inline Eigen::Quaternionf& betweenZ(Eigen::Quaternionf& out, Eigen::Ref<const Eigen::Vector3f> b) {
    float w = b(2);
    out.x() = -b(1); out.y() = b(0); out.z() = 0.;
    out.w() = w + std::sqrt(out.x() * out.x() + out.y() * out.y() + w * w);
    if (out.x() == 0. && out.y() == 0. && out.z() == 0. && out.w() == 0.)
      out.y() = 1;
    else
      out.normalize();
    return out;
  }

  inline Eigen::Ref<Eigen::Vector3f> mulVZ(Eigen::Ref<Eigen::Vector3f> out, const Eigen::Quaternionf quat) {
    out.x() = (quat.y() * quat.w() + quat.z() * quat.x()) * 2.f;
    out.y() = (quat.z() * quat.y() - quat.x() * quat.w()) * 2.f;
    out.z() = quat.w() * quat.w() + quat.z() * quat.z() - quat.y() * quat.y() - quat.x() * quat.x();
    return out;
  }

  inline Eigen::Quaternionf& invert(Eigen::Quaternionf& out, const Eigen::Quaternionf& quat) {
    float dot = quat.x() * quat.x() + quat.y() * quat.y() + quat.z() * quat.z() + quat.w() * quat.w();
    float invDot = dot == 0. ? 0 : 1. / dot;
    out.x() = -quat.x() * invDot;
    out.y() = -quat.y() * invDot;
    out.z() = -quat.z() * invDot;
    out.w() = quat.w() * invDot;
    return out;
  }

  inline void rotation(Eigen::Ref<Eigen::Matrix4f> mat, const Eigen::Quaternionf& quat) {
    float x = quat.x(), y = quat.y(), z = quat.z(), w = quat.w();
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    mat(0, 0) = 1 - (yy + zz); mat(0, 1) = xy + wz; mat(0, 2) = xz - wy; mat(0, 3) = 0;
    mat(1, 0) = xy - wz; mat(1, 1) = 1 - (xx + zz); mat(1, 2) = yz + wx; mat(1, 3) = 0;
    mat(2, 0) = xz + wy; mat(2, 1) = yz - wx; mat(2, 2) = 1 - (xx + yy); mat(2, 3) = 0;
    mat(3, 0) = 0; mat(3, 1) = 0; mat(3, 2) = 0; mat(3, 3) = 1;
  }

  inline void perspective(Eigen::Ref<Eigen::Matrix4f> mat, float fov, float aspect, float near, float far) {
    float f = std::tan(M_PI * 0.5 - 0.5 * fov);
    mat(0, 0) = f / aspect; mat(1, 1) = 0; mat(2, 2) = 0; mat(3, 3) = 0;
    mat(0, 1) = 0; mat(1, 1) = f; mat(2, 1) = 0; mat(3, 1) = 0;
    mat(0, 2) = 0; mat(1, 2) = 0; mat(3, 2) = -1;
    mat(0, 3) = 0; mat(1, 3) = 0; mat(3, 3) = 0;

    if (std::isfinite(far)) {
      float rangeInv = 1 / (near - far);
      mat(2, 2) = far * rangeInv;
      mat(2, 3) = far * near * rangeInv;
    }
    else {
      mat(2, 2) = -1;
      mat(2, 3) = -near;
    }
  };

  inline void lookAt(
    Eigen::Ref<Eigen::Matrix4f> mat,
    Eigen::Ref<const Eigen::Vector3f> eye,
    Eigen::Ref<const Eigen::Vector3f> dir,
    Eigen::Ref<const Eigen::Vector3f> up, const float eps = 1e-12) {
    float ex = eye(0), ey = eye(1), ez = eye(2);
    float ux = up(0), uy = up(1), uz = up(2);
    float z0 = -dir(0), z1 = -dir(1), z2 = -dir(2);

    float x0 = uy * z2 - uz * z1;
    float x1 = uz * z0 - ux * z2;
    float x2 = ux * z1 - uy * z0;
    float len = 1 / (std::sqrt(x0 * x0 + x1 * x1 + x2 * x2) + eps);
    x0 *= len; x1 *= len; x2 *= len;

    float y0 = z1 * x2 - z2 * x1;
    float y1 = z2 * x0 - z0 * x2;
    float y2 = z0 * x1 - z1 * x0;
    len = 1 / (std::sqrt(y0 * y0 + y1 * y1 + y2 * y2) + eps);
    y0 *= len; y1 *= len; y2 *= len;

    mat(0, 0) = x0; mat(1, 0) = y0; mat(2, 0) = z0; mat(3, 0) = 0;
    mat(0, 1) = x1; mat(1, 1) = y1; mat(2, 1) = z1; mat(3, 1) = 0;
    mat(0, 2) = x2; mat(1, 2) = y2; mat(2, 2) = z2; mat(3, 2) = 0;
    mat(0, 3) = -(x0 * ex + x1 * ey + x2 * ez);
    mat(1, 3) = -(y0 * ex + y1 * ey + y2 * ez);
    mat(2, 3) = -(z0 * ex + z1 * ey + z2 * ez);
    mat(3, 3) = 1;
  };

  inline Eigen::Ref<Eigen::Vector3f> arcballHolroyd(Eigen::Ref<Eigen::Vector3f> out, Eigen::Vector2f p, float radius = 2.) {
    float r2 = radius * radius, h = p.squaredNorm();
    float z = h <= r2 * .5f ? std::sqrt(r2 - h) : r2 / (2.f * std::sqrt(h));
    out << p.x(), p.y(), z;
    return out;
  }
}
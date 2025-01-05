#pragma once

#include <array>
#include <Eigen/Core>

namespace math {
  inline float radians(float x) { return x * M_PI / 180.0f; };

  using Vec3 = std::array<float, 3>;
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

  inline void rotation(Eigen::Ref<Eigen::Matrix4f> mat, const Quaternion& quat) {
    float x = quat[0], y = quat[1], z = quat[2], w = quat[3];
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    mat(0, 0) = 1 - (yy + zz); mat(1, 0) = xy + wz; mat(2, 0) = xz - wy; mat(3, 0) = 0;
    mat(0, 1) = xy - wz; mat(1, 1) = 1 - (xx + zz); mat(2, 1) = yz + wx; mat(3, 1) = 0;
    mat(0, 2) = xz + wy; mat(1, 2) = yz - wx; mat(3, 2) = 1 - (xx + yy); mat(3, 2) = 0;
    mat(0, 3) = 0; mat(1, 3) = 0; mat(2, 3) = 0; mat(3, 3) = 1;
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
}
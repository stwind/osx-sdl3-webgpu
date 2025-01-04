#pragma once

#include <array>

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
#pragma once

#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"

#include <algorithm>
#include <cmath>

// Plain arithmetic is done inline with the same formulas Unity uses in C#.
// Anything Unity implements natively (Euler, LookRotation, FromToRotation...) is
// forwarded to the game so results match PC EasyOffset bit for bit where possible.
namespace UnityEngine {
inline Vector3 operator+(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vector3 operator-(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vector3 operator-(Vector3 a) { return {-a.x, -a.y, -a.z}; }
inline Vector3 operator*(Vector3 a, float d) { return {a.x * d, a.y * d, a.z * d}; }
inline Vector3 operator*(float d, Vector3 a) { return {a.x * d, a.y * d, a.z * d}; }
inline Vector3 operator/(Vector3 a, float d) { return {a.x / d, a.y / d, a.z / d}; }
inline Vector3& operator+=(Vector3& a, Vector3 b) { return a = a + b; }
inline Vector3& operator-=(Vector3& a, Vector3 b) { return a = a - b; }

inline Quaternion operator*(Quaternion lhs, Quaternion rhs) {
    return {
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y + lhs.y * rhs.w + lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.w * rhs.z + lhs.z * rhs.w + lhs.x * rhs.y - lhs.y * rhs.x,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z
    };
}

inline Quaternion& operator*=(Quaternion& a, Quaternion b) { return a = a * b; }

inline Vector3 operator*(Quaternion rotation, Vector3 point) {
    float x = rotation.x * 2.0f;
    float y = rotation.y * 2.0f;
    float z = rotation.z * 2.0f;
    float xx = rotation.x * x;
    float yy = rotation.y * y;
    float zz = rotation.z * z;
    float xy = rotation.x * y;
    float xz = rotation.x * z;
    float yz = rotation.y * z;
    float wx = rotation.w * x;
    float wy = rotation.w * y;
    float wz = rotation.w * z;
    return {
        (1.0f - (yy + zz)) * point.x + (xy - wz) * point.y + (xz + wy) * point.z,
        (xy + wz) * point.x + (1.0f - (xx + zz)) * point.y + (yz - wx) * point.z,
        (xz - wy) * point.x + (yz + wx) * point.y + (1.0f - (xx + yy)) * point.z
    };
}
}

namespace EasyOffset {

using Vector2 = UnityEngine::Vector2;
using Vector3 = UnityEngine::Vector3;
using Quaternion = UnityEngine::Quaternion;

namespace Math {
constexpr float Deg2Rad = 0.017453292f;
constexpr float Rad2Deg = 57.29578f;

inline Vector3 Zero() { return {0, 0, 0}; }
inline Vector3 Forward() { return {0, 0, 1}; }
inline Vector3 Up() { return {0, 1, 0}; }
inline Vector3 Left() { return {-1, 0, 0}; }
inline Quaternion Identity() { return {0, 0, 0, 1}; }

inline float Get(Vector3 const& v, int i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }
inline float& At(Vector3& v, int i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }

inline float Dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float Dot(Quaternion a, Quaternion b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline float SqrMagnitude(Vector3 v) { return Dot(v, v); }
inline float Magnitude(Vector3 v) { return std::sqrt(SqrMagnitude(v)); }

inline Vector3 Normalized(Vector3 v) {
    float magnitude = Magnitude(v);
    return magnitude > 1e-5f ? v / magnitude : Zero();
}

inline Vector3 ClampMagnitude(Vector3 v, float maxLength) {
    float sqrMagnitude = SqrMagnitude(v);
    if (sqrMagnitude <= maxLength * maxLength) return v;
    return v / std::sqrt(sqrMagnitude) * maxLength;
}

inline Vector3 Lerp(Vector3 a, Vector3 b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return a + (b - a) * t;
}

// C# Equals (exact field comparison)
inline bool Equals(Vector3 a, Vector3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
inline bool Equals(Quaternion a, Quaternion b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }

// C# == operators (Unity tolerances)
inline bool ApproxEquals(Vector3 a, Vector3 b) { return SqrMagnitude(a - b) < 9.99999944e-11f; }
inline bool ApproxEquals(Quaternion a, Quaternion b) { return Dot(a, b) > 0.999999f; }

inline bool IsFinite(Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// Unity's native Quaternion.Inverse is the conjugate. Kept il2cpp-free because global
// ReeTransforms call it while the .so is being loaded, before il2cpp exists.
inline Quaternion Inverse(Quaternion q) { return {-q.x, -q.y, -q.z, q.w}; }
inline Quaternion Euler(Vector3 euler) { return Quaternion::Euler(euler); }
inline Quaternion Euler(float x, float y, float z) { return Quaternion::Euler(x, y, z); }
inline Vector3 EulerAngles(Quaternion q) { return q.get_eulerAngles(); }
inline Quaternion LookRotation(Vector3 forward, Vector3 up) { return Quaternion::LookRotation(forward, up); }
inline Quaternion FromToRotation(Vector3 from, Vector3 to) { return Quaternion::FromToRotation(from, to); }
inline Quaternion AngleAxis(float angle, Vector3 axis) { return Quaternion::AngleAxis(angle, axis); }
inline float Angle(Quaternion a, Quaternion b) { return Quaternion::Angle(a, b); }
inline Quaternion Lerp(Quaternion a, Quaternion b, float t) { return Quaternion::Lerp(a, b, t); }

// Quaternion.ToAngleAxis is stripped from the Quest build; this mirrors Unity's native implementation
inline void ToAngleAxis(Quaternion q, float& angle, Vector3& axis) {
    float norm = std::sqrt(Dot(q, q));
    if (norm > 1e-5f) {
        q = {q.x / norm, q.y / norm, q.z / norm, q.w / norm};
    } else {
        q = Identity();
    }
    float radians = 2.0f * std::acos(std::clamp(q.w, -1.0f, 1.0f));
    if (std::abs(radians) < 1e-6f) {
        angle = 0.0f;
        axis = {1, 0, 0};
        return;
    }
    float div = 1.0f / std::sqrt(1.0f - q.w * q.w);
    axis = {q.x * div, q.y * div, q.z * div};
    angle = radians * Rad2Deg;
}
}
}

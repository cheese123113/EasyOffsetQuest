#include "TransformUtils.hpp"
#include "PluginConfig.hpp"

namespace EasyOffset::TransformUtils {

#pragma region Mirror

Quaternion MirrorRotation(Quaternion rotation) {
    float angle;
    Vector3 axis;
    Math::ToAngleAxis(rotation, angle, axis);
    return Math::AngleAxis(-angle, MirrorVector(axis));
}

Vector3 MirrorVector(Vector3 vector) {
    return {-vector.x, vector.y, vector.z};
}

#pragma endregion

#pragma region Clamp

static float ClampRotation(float value) {
    if (value > 180) return -360 + value;
    if (value < -180) return 360 - value;
    return value;
}

Vector3 ClampEulerAngles(Vector3 eulerAngles) {
    return {ClampRotation(eulerAngles.x), ClampRotation(eulerAngles.y), ClampRotation(eulerAngles.z)};
}

#pragma endregion

#pragma region Conversions

Vector3 DirectionFromRotation(Quaternion rotation) {
    return rotation * Math::Forward();
}

Quaternion RotationFromDirection(Vector3 direction) {
    return Math::LookRotation(direction, Math::Up());
}

Quaternion RotationFromEuler(Vector3 eulerAngles) {
    return Math::Euler(eulerAngles);
}

Vector3 EulerFromRotation(Quaternion rotation) {
    return ClampEulerAngles(Math::EulerAngles(rotation));
}

#pragma endregion

#pragma region GetSaberTranslation

Vector3 GetSaberTranslation(Vector3 pivotPosition, Quaternion saberRotation, float zOffset) {
    auto rotationCausedOffset = saberRotation * Vector3{0.0f, 0.0f, zOffset};
    return pivotPosition + rotationCausedOffset;
}

#pragma endregion

#pragma region Spherical

Vector3 SphericalToOrthoDirection(Vector2 sphericalDirectionRadians) {
    float xzScale = std::cos(sphericalDirectionRadians.x);
    float x = std::sin(sphericalDirectionRadians.y) * xzScale;
    float z = std::cos(sphericalDirectionRadians.y) * xzScale;
    float y = -std::sin(sphericalDirectionRadians.x);
    return {x, y, z};
}

Vector2 OrthoToSphericalDirection(Vector3 orthoDirection) {
    float yDirX = orthoDirection.z;
    float yDirY = orthoDirection.x;
    float yAngleRad = std::atan2(yDirY, yDirX);
    float xAngleRad = -std::atan2(orthoDirection.y, std::sqrt(yDirX * yDirX + yDirY * yDirY));
    return {xAngleRad, yAngleRad};
}

#pragma endregion

#pragma region Controller <-> Reference

void ToReferenceSpace(Quaternion originalRotation, Quaternion rotationReference, float& curve, float& balance) {
    auto rotationDifference = Math::Inverse(rotationReference) * originalRotation;
    auto differenceDirection = DirectionFromRotation(rotationDifference);
    auto resultEulerRad = OrthoToSphericalDirection(differenceDirection);

    curve = -resultEulerRad.y * Math::Rad2Deg;
    balance = resultEulerRad.x * Math::Rad2Deg;
}

Vector3 FromReferenceSpace(Quaternion originalRotation, Quaternion rotationReference, float curve, float balance) {
    auto additionalRotation = Math::Euler(balance, -curve, 0.0f);
    auto resultRotation = rotationReference * additionalRotation;
    auto alignedRotation = AlignForwardVectors(originalRotation, resultRotation);
    return EulerFromRotation(alignedRotation);
}

#pragma endregion

#pragma region AlignForwardVectors

Quaternion AlignForwardVectors(Quaternion sourceRotation, Quaternion targetRotation) {
    auto sourceDirection = DirectionFromRotation(sourceRotation);
    auto targetDirection = DirectionFromRotation(targetRotation);
    auto relativeRotation = Math::FromToRotation(sourceDirection, targetDirection);
    return relativeRotation * sourceRotation;
}

#pragma endregion

#pragma region TrackingSpace

// Meta Touch grip pose relative to the native OVR controller pose, in Unity coordinates.
// Values from great-hb/EasyOffsetQuest (MIT); they make PC presets line up on Quest.
static constexpr Vector3 LinkBasisPosition{0.0f, -0.03f, -0.04f};
static constexpr Quaternion LinkBasisRotation{-0.5f, 0.0f, 0.0f, 0.8660253882408142f};

void ApplyTrackingSpace(Vector3& position, Quaternion& rotation) {
    if (PluginConfig::GetTrackingSpace() != TrackingSpace::PcLink) return;
    position = position + rotation * LinkBasisPosition;
    rotation = rotation * LinkBasisRotation;
}

#pragma endregion
}

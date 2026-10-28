#pragma once

#include "Math.hpp"

namespace EasyOffset {

class ReeTransform {
public:
    Vector3 Position;

    ReeTransform(Vector3 position, Quaternion rotation) : Position(position) { SetRotation(rotation); }

    Quaternion GetRotation() const { return rotation; }

    void SetRotation(Quaternion value) {
        rotation = value;
        inverseRotation = Math::Inverse(value);
    }

    Vector3 LocalToWorldPosition(Vector3 localPosition) const { return Position + rotation * localPosition; }
    Vector3 LocalToWorldDirection(Vector3 localDirection) const { return rotation * localDirection; }
    Quaternion LocalToWorldRotation(Quaternion localRotation) const { return rotation * localRotation; }

    Vector3 WorldToLocalPosition(Vector3 worldPosition) const { return inverseRotation * (worldPosition - Position); }
    Vector3 WorldToLocalDirection(Vector3 worldDirection) const { return inverseRotation * worldDirection; }
    Quaternion WorldToLocalRotation(Quaternion worldRotation) const { return inverseRotation * worldRotation; }

private:
    Quaternion rotation{0, 0, 0, 1};
    Quaternion inverseRotation{0, 0, 0, 1};
};

namespace TransformUtils {
Quaternion MirrorRotation(Quaternion rotation);
Vector3 MirrorVector(Vector3 vector);

Vector3 ClampEulerAngles(Vector3 eulerAngles);

Vector3 DirectionFromRotation(Quaternion rotation);
Quaternion RotationFromDirection(Vector3 direction);
Quaternion RotationFromEuler(Vector3 eulerAngles);
Vector3 EulerFromRotation(Quaternion rotation);

Vector3 GetSaberTranslation(Vector3 pivotPosition, Quaternion saberRotation, float zOffset);

Vector3 SphericalToOrthoDirection(Vector2 sphericalDirectionRadians);
Vector2 OrthoToSphericalDirection(Vector3 orthoDirection);

void ToReferenceSpace(Quaternion originalRotation, Quaternion rotationReference, float& curve, float& balance);
Vector3 FromReferenceSpace(Quaternion originalRotation, Quaternion rotationReference, float curve, float balance);

Quaternion AlignForwardVectors(Quaternion sourceRotation, Quaternion targetRotation);

// Quest native controller pose -> PC (Meta Link / OpenXR grip) controller pose
void ApplyTrackingSpace(Vector3& position, Quaternion& rotation);
}
}

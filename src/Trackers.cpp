#include "Trackers.hpp"

#include "UnityEngine/Matrix4x4.hpp"
#include "UnityEngine/Vector4.hpp"

#include <array>
#include <limits>

namespace EasyOffset {

#pragma region WeightedList

Vector3 WeightedList::GetAverage() const {
    Vector3 result = Math::Zero();
    float totalWeight = 0.0f;

    for (auto const& entry : Entries) {
        totalWeight += entry.Weight;
        result += entry.Value * entry.Weight;
    }

    return totalWeight > 0 ? result / totalWeight : Math::Zero();
}

float WeightedList::GetDeviationFromPoint(Vector3 point) const {
    if (Entries.empty()) return 0.0f;
    float sum = 0.0f;
    for (auto const& entry : Entries) {
        float distance = Math::Magnitude(entry.Value - point);
        sum += distance * distance;
    }
    return std::sqrt(sum / Entries.size());
}

float GetDeviationFromPlane(WeightedList const& list, Plane const& plane) {
    if (list.Entries.empty()) return 0.0f;
    float sum = 0.0f;
    for (auto const& entry : list.Entries) {
        float distance = plane.GetDistanceToPoint(entry.Value);
        sum += distance * distance;
    }
    return std::sqrt(sum / list.Entries.size());
}

using Matrix = std::array<std::array<float, 4>, 4>;
using Vector4 = std::array<float, 4>;

// Matrix4x4.inverse is native in Unity; use the game's implementation
static Matrix Inverse(Matrix const& m) {
    UnityEngine::Matrix4x4 unityMatrix(
        m[0][0], m[1][0], m[2][0], m[3][0],
        m[0][1], m[1][1], m[2][1], m[3][1],
        m[0][2], m[1][2], m[2][2], m[3][2],
        m[0][3], m[1][3], m[2][3], m[3][3]
    );
    auto inv = unityMatrix.get_inverse();
    return {{
        {inv.m00, inv.m01, inv.m02, inv.m03},
        {inv.m10, inv.m11, inv.m12, inv.m13},
        {inv.m20, inv.m21, inv.m22, inv.m23},
        {inv.m30, inv.m31, inv.m32, inv.m33},
    }};
}

static Vector4 Normalized(Vector4 v) {
    float magnitude = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
    if (magnitude <= 1e-5f) return {0, 0, 0, 0};
    return {v[0] / magnitude, v[1] / magnitude, v[2] / magnitude, v[3] / magnitude};
}

Plane CalculatePlane(WeightedList const& list, Vector3 initialNormal) {
    auto averagePoint = list.GetAverage();

    Matrix sum{};
    for (auto const& entry : list.Entries) {
        auto relative3 = entry.Value - averagePoint;
        Vector4 relative4{relative3.x, relative3.y, relative3.z, 1.0f};
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                sum[row][col] += relative4[row] * relative4[col];
            }
        }
    }

    auto convergenceMatrix = Inverse(sum);
    Vector4 normal{initialNormal.x, initialNormal.y, initialNormal.z, 0.0f};

    for (int i = 0; i < 20; i++) {
        Vector4 next{};
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                next[row] += convergenceMatrix[row][col] * normal[col];
            }
        }
        normal = Normalized(next);
    }

    return {Vector3{normal[0], normal[1], normal[2]}, averagePoint};
}

static float GetAverageAngle(std::vector<float> const& angles, float filterAnchor, float filterMargin) {
    float sum = 0.0f;
    int num = 0;

    for (float angle : angles) {
        float difference = std::abs(angle - filterAnchor);
        if (difference > filterMargin) continue;
        sum += angle;
        num += 1;
    }

    return num > 0 ? sum / num : 0.0f;
}

static void AnalyzeSwingData(
    std::vector<float> const& swingData,
    float& minimalSwingAngle,
    float& maximalSwingAngle,
    std::vector<float>& localMinimums,
    std::vector<float>& localMaximums
) {
    minimalSwingAngle = 0.0f;
    maximalSwingAngle = 0.0f;

    float previousAngle = 0.0f;
    bool isPreviousDirectionPositive = false;
    bool hasPreviousDirection = false;

    int count = 0;
    for (float angle : swingData) {
        if (count++ == 0) {
            minimalSwingAngle = angle;
            maximalSwingAngle = angle;
            localMinimums.push_back(angle);
            localMaximums.push_back(angle);
            previousAngle = angle;
        } else {
            if (angle > maximalSwingAngle) maximalSwingAngle = angle;
            if (angle < minimalSwingAngle) minimalSwingAngle = angle;

            float angularVelocity = angle - previousAngle;
            bool isDirectionPositive = angularVelocity >= 0;

            if (hasPreviousDirection && isDirectionPositive != isPreviousDirectionPositive) {
                if (isPreviousDirectionPositive) {
                    localMaximums.push_back(previousAngle);
                } else {
                    localMinimums.push_back(previousAngle);
                }
            }

            hasPreviousDirection = true;
            previousAngle = angle;
            isPreviousDirectionPositive = isDirectionPositive;
        }
    }

    if (localMaximums.size() > 1) localMaximums.erase(localMaximums.begin());
    if (localMinimums.size() > 1) localMinimums.erase(localMinimums.begin());

    float lastAngle = swingData.back();

    if (lastAngle >= maximalSwingAngle) {
        maximalSwingAngle = lastAngle;
        localMaximums.push_back(lastAngle);
    }

    if (lastAngle <= minimalSwingAngle) {
        minimalSwingAngle = lastAngle;
        localMinimums.push_back(lastAngle);
    }
}

void GetSwingAngles(WeightedList const& list, Vector3 origin, Quaternion rotation, float& minAngle, float& maxAngle) {
    minAngle = 0.0f;
    maxAngle = 0.0f;
    if (list.Entries.empty()) return;

    auto inverseRotation = Math::Inverse(rotation);

    std::vector<float> swingData;
    swingData.reserve(list.Entries.size());
    for (auto const& entry : list.Entries) {
        auto localPosition = inverseRotation * (entry.Value - origin);
        swingData.push_back(std::atan2(localPosition.y, localPosition.x));
    }

    float absoluteMinimumAngle, absoluteMaximumAngle;
    std::vector<float> localMinimums, localMaximums;
    AnalyzeSwingData(swingData, absoluteMinimumAngle, absoluteMaximumAngle, localMinimums, localMaximums);

    float angleMargin = std::abs(absoluteMaximumAngle - absoluteMinimumAngle) / 6;
    minAngle = GetAverageAngle(localMinimums, absoluteMinimumAngle, angleMargin);
    maxAngle = GetAverageAngle(localMaximums, absoluteMaximumAngle, angleMargin);
}

#pragma endregion

#pragma region RotationPointTracker

void RotationPointTracker::Clear() {
    positions.clear();
    directions.clear();
    weights.clear();
    hasPrevious = false;
}

static void WorldToLocalLine(
    Vector3 worldPosition, Quaternion worldRotation,
    Vector3 worldPoint, Vector3 worldDirection,
    Vector3& localPoint, Vector3& localDirection
) {
    auto inverseRotation = Math::Inverse(worldRotation);
    localPoint = inverseRotation * (worldPoint - worldPosition);
    localDirection = inverseRotation * worldDirection;
}

static void CalculateOriginLine(
    Vector3 positionA, Quaternion rotationA,
    Vector3 positionB, Quaternion rotationB,
    Vector3& worldPoint, Vector3& worldDirection
) {
    auto relativeRotation = rotationB * Math::Inverse(rotationA);
    float angle;
    Math::ToAngleAxis(relativeRotation, angle, worldDirection);

    auto diff = positionB - positionA;
    float halfAngle = angle * Math::Deg2Rad * 0.5f;
    auto rayOrigin = (positionA + positionB) * 0.5f;
    auto rayDirection = Math::Normalized(Math::AngleAxis(90.0f, worldDirection) * diff);
    float rayLength = std::cos(halfAngle) * Math::Magnitude(diff) / (std::sin(halfAngle) * 2);
    worldPoint = rayOrigin + rayDirection * rayLength;
}

void RotationPointTracker::Update(Vector3 position, Quaternion rotation, float deltaTime) {
    if (Math::ApproxEquals(position, previousPosition) || Math::ApproxEquals(rotation, previousRotation) || deltaTime <= 0) return;

    if (hasPrevious) {
        Vector3 worldPoint, worldDirection;
        CalculateOriginLine(previousPosition, previousRotation, position, rotation, worldPoint, worldDirection);

        float angularVelocity = Math::Angle(previousRotation, rotation) / deltaTime;

        Vector3 localPoint, localDirection;
        WorldToLocalLine(position, rotation, worldPoint, worldDirection, localPoint, localDirection);

        if (positions.size() < capacity) {
            positions.push_back(localPoint);
            directions.push_back(localDirection);
            weights.push_back(angularVelocity);
        } else {
            TryReplaceWorst(localPoint, localDirection, angularVelocity);
        }
    }

    previousPosition = position;
    previousRotation = rotation;
    hasPrevious = true;
}

bool RotationPointTracker::TryReplaceWorst(Vector3 position, Vector3 direction, float weight) {
    float worstWeight = std::numeric_limits<float>::max();
    size_t worstIndex = 0;

    for (size_t i = 0; i < positions.size(); i++) {
        if (weights[i] >= worstWeight) continue;
        worstWeight = weights[i];
        worstIndex = i;
    }

    if (weight < worstWeight) return false;

    positions[worstIndex] = position;
    directions[worstIndex] = direction;
    weights[worstIndex] = weight;
    return true;
}

// Least-squares closest point to all rotation axes
Vector3 RotationPointTracker::GetLocalOrigin() const {
    float m[3][3]{};
    float b[3]{};

    for (size_t i = 0; i < positions.size(); ++i) {
        auto const& a = positions[i];
        auto const& d = directions[i];
        float da = Math::Dot(d, a);

        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                m[j][k] += Math::Get(d, j) * Math::Get(d, k);
            }
            m[j][j] -= 1;
            b[j] += Math::Get(d, j) * da - Math::Get(a, j);
        }
    }

    constexpr int n = 3;

    for (int dia = 0; dia < n; dia++) {
        int maxRow = dia;
        float max = std::abs(m[dia][dia]);

        for (int row = dia + 1; row < n; row++) {
            float tmp = std::abs(m[row][dia]);
            if (!(tmp > max)) continue;
            maxRow = row;
            max = tmp;
        }

        if (maxRow != dia) {
            for (int i = 0; i < n; i++) std::swap(m[dia][i], m[maxRow][i]);
            std::swap(b[dia], b[maxRow]);
        }

        for (int row = dia + 1; row < n; row++) {
            float tmp = m[row][dia] / m[dia][dia];
            for (int col = dia + 1; col < n; col++) {
                m[row][col] -= tmp * m[dia][col];
            }
            m[row][dia] = 0;
            b[row] -= tmp * b[dia];
        }
    }

    float result[3]{};
    for (int row = n - 1; row >= 0; row--) {
        float tmp = b[row];
        for (int j = n - 1; j > row; j--) {
            tmp -= result[j] * m[row][j];
        }
        result[row] = tmp / m[row][row];
    }

    return {result[0], result[1], result[2]};
}

#pragma endregion

#pragma region RelativeRotationTracker

bool RelativeRotationTracker::Update(Quaternion rotation, Vector3 positiveDirection, float deltaTime, float& angularVelocity, Vector3& axis) {
    if (!initialized || Math::ApproxEquals(rotation, previousRotation)) {
        angularVelocity = 0.0f;
        axis = Math::Zero();
        return false;
    }

    angularVelocity = Math::Angle(previousRotation, rotation) / deltaTime;
    auto relativeRotation = rotation * Math::Inverse(previousRotation);
    float angle;
    Math::ToAngleAxis(relativeRotation, angle, axis);
    previousRotation = rotation;

    if (Math::Dot(positiveDirection, axis) < 0) axis = -axis;
    return true;
}

#pragma endregion

#pragma region SwingAnalyzer

SwingAnalyzer::SwingAnalyzer(size_t maximalCapacity)
    : maximalCapacity(maximalCapacity), tipPositions(maximalCapacity), pivotPositions(maximalCapacity), localNormals(maximalCapacity) {}

void SwingAnalyzer::SetCapacity(size_t capacity) {
    maximalCapacity = capacity;
    tipPositions.SetCapacity(capacity);
    pivotPositions.SetCapacity(capacity);
    localNormals.SetCapacity(capacity);
}

void SwingAnalyzer::Reset() {
    tipPositions.Clear();
    pivotPositions.Clear();
    localNormals.Clear();
    hasPreviousTipPosition = false;
}

void SwingAnalyzer::Update(
    ReeTransform const& controllerTransform,
    Vector3 tipWorldPosition,
    Vector3 pivotWorldPosition,
    Vector3 initialNormal,
    float deltaTime,
    Vector3& planePosition,
    Quaternion& planeRotation,
    float& tipDeviation,
    float& pivotDeviation,
    float& pivotHeight,
    float& minimalSwingAngle,
    float& maximalSwingAngle
) {
    tipPositions.Add(tipWorldPosition, 1.0f);
    pivotPositions.Add(pivotWorldPosition, 1.0f);

    auto averagePivotPosition = pivotPositions.GetAverage();
    pivotDeviation = pivotPositions.GetDeviationFromPoint(averagePivotPosition);
    auto swingPlane = CalculatePlane(tipPositions, initialNormal);
    tipDeviation = GetDeviationFromPlane(tipPositions, swingPlane);
    pivotHeight = swingPlane.GetDistanceToPoint(averagePivotPosition);

    planePosition = swingPlane.ClosestPointOnPlane(averagePivotPosition);
    planeRotation = Math::LookRotation(swingPlane.normal, Math::Up());

    GetSwingAngles(tipPositions, planePosition, planeRotation, minimalSwingAngle, maximalSwingAngle);

    UpdateLocalNormals(controllerTransform, tipWorldPosition, swingPlane.normal, deltaTime);
}

// Positions are already in tracking space here, so no room offset has to be removed
void SwingAnalyzer::UpdateLocalNormals(ReeTransform const& controllerTransform, Vector3 tipWorldPosition, Vector3 planeNormal, float deltaTime) {
    if (hasPreviousTipPosition && deltaTime > 0) {
        auto localNormal = controllerTransform.WorldToLocalDirection(planeNormal);
        float tipVelocity = Math::Magnitude(tipWorldPosition - previousTipPosition) / deltaTime;
        float dataSetRatio = static_cast<float>(localNormals.Entries.size()) / maximalCapacity;
        localNormals.Add(localNormal, std::pow(dataSetRatio, 0.5f) * std::pow(tipVelocity, 2.0f));
    }

    previousTipPosition = tipWorldPosition;
    hasPreviousTipPosition = true;
}

Vector3 SwingAnalyzer::GetAverageLocalPlaneNormal() const {
    return hasPreviousTipPosition ? localNormals.GetAverage() : Math::Forward();
}

#pragma endregion
}

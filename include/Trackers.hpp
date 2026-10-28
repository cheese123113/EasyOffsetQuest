#pragma once

#include "TransformUtils.hpp"

#include <deque>
#include <vector>

namespace EasyOffset {

class Range {
public:
    float Start;
    float End;

    Range(float start, float end) : Start(start), End(end), amplitude(end - start) {}

    float GetRatio(float value) const { return (value - Start) / amplitude; }

    float GetRatioClamped(float value) const {
        float ratio = GetRatio(value);
        if (ratio <= 0) return 0;
        if (ratio >= 1) return 1;
        return ratio;
    }

    float SlideBy(float ratio) const { return Start + amplitude * ratio; }

private:
    float amplitude;
};

class WeightedList {
public:
    struct Entry {
        Vector3 Value;
        float Weight;
    };

    std::deque<Entry> Entries;

    explicit WeightedList(size_t maximalCapacity) : maximalCapacity(maximalCapacity) {}

    void SetCapacity(size_t capacity) { maximalCapacity = capacity; }
    void Clear() { Entries.clear(); }

    void Add(Vector3 value, float weight) {
        Entries.push_back({value, weight});
        if (Entries.size() > maximalCapacity) Entries.pop_front();
    }

    Vector3 GetAverage() const;
    float GetDeviationFromPoint(Vector3 point) const;

private:
    size_t maximalCapacity;
};

// UnityEngine.Plane equivalent
struct Plane {
    Vector3 normal;
    float distance;

    Plane(Vector3 inNormal, Vector3 inPoint) : normal(Math::Normalized(inNormal)), distance(-Math::Dot(normal, inPoint)) {}

    float GetDistanceToPoint(Vector3 point) const { return Math::Dot(normal, point) + distance; }
    Vector3 ClosestPointOnPlane(Vector3 point) const { return point - normal * GetDistanceToPoint(point); }
};

float GetDeviationFromPlane(WeightedList const& list, Plane const& plane);
Plane CalculatePlane(WeightedList const& list, Vector3 initialNormal);
void GetSwingAngles(WeightedList const& list, Vector3 origin, Quaternion rotation, float& minAngle, float& maxAngle);

class RotationPointTracker {
public:
    explicit RotationPointTracker(size_t capacity) : capacity(capacity) {}

    void Clear();
    void Update(Vector3 position, Quaternion rotation, float deltaTime);
    Vector3 GetLocalOrigin() const;

private:
    size_t capacity;
    std::vector<Vector3> positions;
    std::vector<Vector3> directions;
    std::vector<float> weights;

    Vector3 previousPosition{0, 0, 0};
    Quaternion previousRotation{0, 0, 0, 0};
    bool hasPrevious = false;

    bool TryReplaceWorst(Vector3 position, Vector3 direction, float weight);
};

class RelativeRotationTracker {
public:
    void Initialize(Quaternion rotation) {
        previousRotation = rotation;
        initialized = true;
    }

    void Reset() { initialized = false; }

    bool Update(Quaternion rotation, Vector3 positiveDirection, float deltaTime, float& angularVelocity, Vector3& axis);

private:
    bool initialized = false;
    Quaternion previousRotation{0, 0, 0, 1};
};

class SwingAnalyzer {
public:
    explicit SwingAnalyzer(size_t maximalCapacity);

    void SetCapacity(size_t capacity);
    void Reset();

    void Update(
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
    );

    Vector3 GetAverageLocalPlaneNormal() const;

private:
    size_t maximalCapacity;
    WeightedList tipPositions;
    WeightedList pivotPositions;
    WeightedList localNormals;

    Vector3 previousTipPosition{0, 0, 0};
    bool hasPreviousTipPosition = false;

    void UpdateLocalNormals(ReeTransform const& controllerTransform, Vector3 tipWorldPosition, Vector3 planeNormal, float deltaTime);
};
}

#pragma once

#include "Abomination.hpp"
#include "Trackers.hpp"

#include <optional>

namespace EasyOffset {

class AbstractAdjustmentModeManager {
public:
    AbstractAdjustmentModeManager(AdjustmentMode adjustmentMode, float positionalSmoothing, float rotationalSmoothing)
        : adjustmentMode(adjustmentMode), positionalSmoothing(positionalSmoothing), rotationalSmoothing(rotationalSmoothing) {}

    virtual ~AbstractAdjustmentModeManager() = default;

    void Initialize();

protected:
    virtual void OnGrabStarted(Hand adjustmentHand, ReeTransform const& adjustmentHandTransform, ReeTransform const& freeHandTransform) = 0;
    virtual void OnGrabUpdated(Hand adjustmentHand, ReeTransform const& adjustmentHandTransform, ReeTransform const& freeHandTransform) = 0;
    virtual void OnGrabFinished(Hand adjustmentHand, ReeTransform const& adjustmentHandTransform, ReeTransform const& freeHandTransform) = 0;

private:
    AdjustmentMode adjustmentMode;
    float positionalSmoothing;
    float rotationalSmoothing;

    bool pressed = false;
    Hand adjustmentHand = Hand::Left;

    void StartSmoothing();
    static void StopSmoothing();

    void OnAssignedButtonPressed(Hand hand);
    void OnTransformsUpdated();
    void OnAssignedButtonReleased(Hand hand);
    void ForceRelease();
};

#pragma region SwingBenchmark

struct SwingBenchmarkResult {
    bool HasResults = false;
    bool IsTracking = false;
    bool IsSwingGood = false;
    Hand SelectedHand = Hand::Left;
    float SwingCurveAngle = 0;  // radians, signed per hand like PC
    float TipDeviation = 0;
    float PivotDeviation = 0;
    float MinimalSwingAngle = 0;
    float MaximalSwingAngle = 0;
};

namespace SwingBenchmark {
extern Event<SwingBenchmarkResult const&> UpdatedEvent;
SwingBenchmarkResult const& GetResult();
void Reset();
void AutoFix();
void SetAsReference();
}

#pragma endregion

#pragma region Direct

enum class SliderValueType { ZOffset, PositionX, PositionY, PositionZ, RotationX, RotationY, RotationZ, Curve, Balance };

class DirectModeVariable {
public:
    DirectModeVariable(SliderValueType sliderValueType, Hand hand) : sliderValueType(sliderValueType), hand(hand) {}

    Event<float> ChangedFromCodeEvent;

    void SetValueFromUI(float value);
    void SetValueFromCode(float value);
    float GetValue() const { return value; }
    SliderValueType GetValueType() const { return sliderValueType; }
    Hand GetHand() const { return hand; }

private:
    SliderValueType sliderValueType;
    Hand hand;
    float value = 0;
};

namespace DirectMode {
DirectModeVariable& Get(Hand hand, SliderValueType type);
}

#pragma endregion

namespace AdjustmentModes {
void Initialize();
}
}

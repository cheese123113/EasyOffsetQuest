#include "AdjustmentModes.hpp"
#include "PluginConfig.hpp"
#include "main.hpp"

#include "GlobalNamespace/OVRPlugin.hpp"
#include "UnityEngine/Time.hpp"

#include <memory>

namespace EasyOffset {

static float DeltaTime() { return UnityEngine::Time::get_deltaTime(); }

static std::string Describe(std::string_view action, Hand hand, std::string_view what = "") {
    std::string result(action);
    result += " ";
    result += HandName(hand);
    if (!what.empty()) {
        result += " ";
        result += what;
    }
    return result;
}

// XRDevice.refreshRate is stripped on Quest, OVRPlugin reports the same value
static size_t ProbeCapacity() {
    constexpr float ProbeTime = 2.0f;
    float refreshRate = 72.0f;
    try {
        refreshRate = GlobalNamespace::OVRPlugin::get_systemDisplayFrequency();
    } catch (...) {}
    if (!(refreshRate > 0)) refreshRate = 72.0f;
    return static_cast<size_t>(std::clamp(refreshRate * ProbeTime, 60.0f, 300.0f));
}

#pragma region AbstractAdjustmentModeManager

void AbstractAdjustmentModeManager::StartSmoothing() {
    PluginConfig::SmoothingEnabled = true;
    PluginConfig::PositionalSmoothing = positionalSmoothing;
    PluginConfig::RotationalSmoothing = rotationalSmoothing;
}

void AbstractAdjustmentModeManager::StopSmoothing() {
    PluginConfig::SmoothingEnabled = false;
    PluginConfig::PositionalSmoothing = 0.0f;
    PluginConfig::RotationalSmoothing = 0.0f;
}

static Hand GetAdjustmentHand(Hand buttonPressedHand) {
    if (!PluginConfig::GetUseFreeHand()) return buttonPressedHand;
    return buttonPressedHand == Hand::Left ? Hand::Right : Hand::Left;
}

static ReeTransform const& FreeHandTransform(Hand adjustmentHand) {
    return Abomination::GetControllerTransform(adjustmentHand == Hand::Left ? Hand::Right : Hand::Left);
}

void AbstractAdjustmentModeManager::OnAssignedButtonPressed(Hand hand) {
    if (pressed || PluginConfig::GetAdjustmentMode() != adjustmentMode) return;

    StartSmoothing();
    adjustmentHand = GetAdjustmentHand(hand);
    pressed = true;

    OnGrabStarted(adjustmentHand, Abomination::GetControllerTransform(adjustmentHand), FreeHandTransform(adjustmentHand));
}

void AbstractAdjustmentModeManager::OnTransformsUpdated() {
    if (!pressed) return;
    OnGrabUpdated(adjustmentHand, Abomination::GetControllerTransform(adjustmentHand), FreeHandTransform(adjustmentHand));
}

void AbstractAdjustmentModeManager::OnAssignedButtonReleased(Hand hand) {
    if (!pressed || adjustmentHand != GetAdjustmentHand(hand)) return;
    StopSmoothing();
    pressed = false;

    OnGrabFinished(adjustmentHand, Abomination::GetControllerTransform(adjustmentHand), FreeHandTransform(adjustmentHand));
    PluginConfig::Save();
}

void AbstractAdjustmentModeManager::ForceRelease() {
    OnAssignedButtonReleased(Hand::Left);
    OnAssignedButtonReleased(Hand::Right);
}

void AbstractAdjustmentModeManager::Initialize() {
    Abomination::AssignedButtonPressedEvent.Subscribe([this](Hand hand) { OnAssignedButtonPressed(hand); });
    Abomination::AssignedButtonReleasedEvent.Subscribe([this](Hand hand) { OnAssignedButtonReleased(hand); });
    Abomination::TransformsUpdatedEvent.Subscribe([this]() { OnTransformsUpdated(); });
    PluginConfig::AdjustmentModeChangedEvent.Subscribe([this](AdjustmentMode) { ForceRelease(); });
    PluginConfig::AssignedButtonChangedEvent.Subscribe([this](ControllerButton) { ForceRelease(); });
}

#pragma endregion

#pragma region Basic

class BasicAdjustmentModeManager : public AbstractAdjustmentModeManager {
public:
    BasicAdjustmentModeManager() : AbstractAdjustmentModeManager(AdjustmentMode::Basic, 4.0f, 4.0f) {}

protected:
    void OnGrabStarted(Hand hand, ReeTransform const& adjustmentHandTransform, ReeTransform const&) override {
        grabWorldRotation = adjustmentHandTransform.LocalToWorldRotation(PluginConfig::GetSaberRotation(hand));
        grabPosition = adjustmentHandTransform.LocalToWorldPosition(PluginConfig::GetSaberPivotPosition(hand));
        PluginConfig::CreateUndoStep(Describe("Change", hand, "Offset"));
    }

    void OnGrabUpdated(Hand hand, ReeTransform const& adjustmentHandTransform, ReeTransform const&) override {
        PluginConfig::SetSaberRotation(hand, adjustmentHandTransform.WorldToLocalRotation(grabWorldRotation));
        PluginConfig::SetSaberPivotPosition(hand, adjustmentHandTransform.WorldToLocalPosition(grabPosition));
    }

    void OnGrabFinished(Hand, ReeTransform const&, ReeTransform const&) override {}

private:
    Quaternion grabWorldRotation;
    Vector3 grabPosition;
};

#pragma endregion

#pragma region Position

class PositionAdjustmentModeManager : public AbstractAdjustmentModeManager {
public:
    PositionAdjustmentModeManager() : AbstractAdjustmentModeManager(AdjustmentMode::Position, 3.0f, 6.0f) {}

protected:
    void OnGrabStarted(Hand hand, ReeTransform const& adjustmentHandTransform, ReeTransform const&) override {
        grabWorldPosition = adjustmentHandTransform.LocalToWorldPosition(PluginConfig::GetSaberPivotPosition(hand));
        PluginConfig::CreateUndoStep(Describe("Change", hand, "Position"));
    }

    void OnGrabUpdated(Hand hand, ReeTransform const& adjustmentHandTransform, ReeTransform const&) override {
        PluginConfig::SetSaberPivotPosition(hand, adjustmentHandTransform.WorldToLocalPosition(grabWorldPosition));
    }

    void OnGrabFinished(Hand, ReeTransform const&, ReeTransform const&) override {}

private:
    Vector3 grabWorldPosition;
};

#pragma endregion

#pragma region Rotation

class RotationAdjustmentModeManager : public AbstractAdjustmentModeManager {
public:
    static constexpr float ZoomedOutSmoothing = 3.0f;
    static constexpr float ZoomedInSmoothing = 1.0f;

    RotationAdjustmentModeManager() : AbstractAdjustmentModeManager(AdjustmentMode::Rotation, 6.0f, ZoomedOutSmoothing) {}

protected:
    void OnGrabStarted(Hand hand, ReeTransform const& adjustmentHandTransform, ReeTransform const& freeHandTransform) override {
        grabWorldRotation = adjustmentHandTransform.LocalToWorldRotation(PluginConfig::GetSaberRotation(hand));
        grabFreeY = freeHandTransform.Position.y;
        PluginConfig::CreateUndoStep(Describe("Change", hand, "Rotation"));
    }

    // Raising the free hand "zooms in": slower smoothing for precise changes
    void OnGrabUpdated(Hand hand, ReeTransform const& adjustmentHandTransform, ReeTransform const& freeHandTransform) override {
        auto finalLocalRotation = adjustmentHandTransform.WorldToLocalRotation(grabWorldRotation);

        float heightDifference = freeHandTransform.Position.y - grabFreeY;
        float zoomRatio = heightRange.GetRatioClamped(heightDifference);
        PluginConfig::RotationalSmoothing = smoothingRange.SlideBy(zoomRatio);

        PluginConfig::SetSaberRotation(hand, finalLocalRotation);
    }

    void OnGrabFinished(Hand, ReeTransform const&, ReeTransform const&) override {}

private:
    Range heightRange{0.05f, 0.4f};
    Range smoothingRange{ZoomedOutSmoothing, ZoomedInSmoothing};
    Quaternion grabWorldRotation;
    float grabFreeY = 0;
};

#pragma endregion

#pragma region PositionAuto

class PositionAutoAdjustmentModeManager : public AbstractAdjustmentModeManager {
public:
    static constexpr size_t MaxMeasurementsCount = 120;

    PositionAutoAdjustmentModeManager() : AbstractAdjustmentModeManager(AdjustmentMode::PositionAuto, 0.0f, 0.0f) {}

protected:
    void OnGrabStarted(Hand hand, ReeTransform const&, ReeTransform const&) override {
        tracker.Clear();
        PluginConfig::CreateUndoStep(Describe("Auto", hand, "Position"));
    }

    void OnGrabUpdated(Hand hand, ReeTransform const& adjustmentHandTransform, ReeTransform const&) override {
        tracker.Update(adjustmentHandTransform.Position, adjustmentHandTransform.GetRotation(), DeltaTime());
        auto origin = tracker.GetLocalOrigin();
        if (!Math::IsFinite(origin)) return;
        PluginConfig::SetSaberPivotPosition(hand, origin);
    }

    void OnGrabFinished(Hand, ReeTransform const&, ReeTransform const&) override {}

private:
    RotationPointTracker tracker{MaxMeasurementsCount};
};

#pragma endregion

#pragma region RotationAuto

class RotationAutoAdjustmentModeManager : public AbstractAdjustmentModeManager {
public:
    static constexpr float MinimalAngularVelocity = 45.0f;
    static constexpr float MaximalAngularVelocity = 360.0f;

    RotationAutoAdjustmentModeManager() : AbstractAdjustmentModeManager(AdjustmentMode::RotationAuto, 0.0f, 0.0f) {}

protected:
    void OnGrabStarted(Hand hand, ReeTransform const& adjustmentHandTransform, ReeTransform const&) override {
        measurements.SetCapacity(ProbeCapacity());
        grabLocalRotation = PluginConfig::GetSaberRotation(hand);
        grabLocalDirection = TransformUtils::DirectionFromRotation(grabLocalRotation);
        rotationTracker.Initialize(adjustmentHandTransform.GetRotation());
        measurements.Clear();
        PluginConfig::CreateUndoStep(Describe("Auto", hand, "Rotation"));
    }

    void OnGrabUpdated(Hand hand, ReeTransform const& adjustmentHandTransform, ReeTransform const&) override {
        if (!Measure(adjustmentHandTransform)) return;

        auto localSaberDirection = Math::Normalized(measurements.GetAverage());
        auto finalRotation = Math::FromToRotation(grabLocalDirection, localSaberDirection) * grabLocalRotation;
        PluginConfig::SetSaberRotation(hand, finalRotation);
    }

    void OnGrabFinished(Hand, ReeTransform const&, ReeTransform const&) override {}

private:
    RelativeRotationTracker rotationTracker;
    Range angularVelocityRange{MinimalAngularVelocity, MaximalAngularVelocity};
    WeightedList measurements{60};
    Quaternion grabLocalRotation;
    Vector3 grabLocalDirection;

    bool Measure(ReeTransform const& controllerTransform) {
        auto positiveDirection = Abomination::HeadRotation * Math::Forward();

        float angularVelocity;
        Vector3 axis;
        bool updated = rotationTracker.Update(controllerTransform.GetRotation(), positiveDirection, DeltaTime(), angularVelocity, axis);
        if (!updated || angularVelocity <= MinimalAngularVelocity) return false;

        auto axisLocal = controllerTransform.WorldToLocalDirection(axis);
        measurements.Add(axisLocal, angularVelocityRange.GetRatio(angularVelocity));
        return true;
    }
};

#pragma endregion

#pragma region RoomOffset

class RoomOffsetAdjustmentModeManager : public AbstractAdjustmentModeManager {
public:
    RoomOffsetAdjustmentModeManager() : AbstractAdjustmentModeManager(AdjustmentMode::RoomOffset, 6.0f, 6.0f) {}

protected:
    void OnGrabStarted(Hand, ReeTransform const& adjustmentHandTransform, ReeTransform const&) override {
        grabPosition = adjustmentHandTransform.Position;
        grabRoomCenter = PluginConfig::GetBaseGameRoomCenter();
    }

    void OnGrabUpdated(Hand, ReeTransform const& adjustmentHandTransform, ReeTransform const&) override {
        auto positionChange = adjustmentHandTransform.Position - grabPosition;
        auto result = grabRoomCenter - positionChange;

        PluginConfig::SetBaseGameRoomCenter({
            PluginConfig::GetAllowRoomXChange() ? result.x : grabRoomCenter.x,
            PluginConfig::GetAllowRoomYChange() ? result.y : grabRoomCenter.y,
            PluginConfig::GetAllowRoomZChange() ? result.z : grabRoomCenter.z
        });
    }

    void OnGrabFinished(Hand, ReeTransform const&, ReeTransform const&) override {
        auto beforeChange = grabRoomCenter;
        auto afterChange = PluginConfig::GetBaseGameRoomCenter();

        PluginConfig::CreateUndoStep(
            "Room Offset",
            [beforeChange]() { PluginConfig::SetBaseGameRoomCenter(beforeChange); },
            [afterChange]() { PluginConfig::SetBaseGameRoomCenter(afterChange); }
        );
    }

private:
    Vector3 grabPosition;
    Vector3 grabRoomCenter;
};

#pragma endregion

#pragma region SwingBenchmark

namespace SwingBenchmark {
static constexpr float FullSwingAngleRequirement = 140 * Math::Deg2Rad;

Event<SwingBenchmarkResult const&> UpdatedEvent;

static SwingAnalyzer swingAnalyzer(60);
static SwingBenchmarkResult result;
static float swingCurveAngleDeg = 0;
static Quaternion leftReferenceRotation = Math::Identity();
static Quaternion rightReferenceRotation = Math::Identity();

SwingBenchmarkResult const& GetResult() { return result; }

static void Start(Hand hand) {
    swingAnalyzer.SetCapacity(ProbeCapacity());
    result.SelectedHand = hand;
    result.HasResults = true;
    result.IsTracking = true;
    swingAnalyzer.Reset();
    UpdatedEvent.Invoke(result);
}

static void Update(ReeTransform const& controllerTransform, Vector3 pivotWorldPosition, Quaternion saberWorldRotation) {
    auto tipWorldPosition = pivotWorldPosition + saberWorldRotation * Math::Forward();

    Vector3 planePosition;
    Quaternion planeRotation;
    float tipDeviation, pivotDeviation, pivotHeight, minimalSwingAngle, maximalSwingAngle;

    swingAnalyzer.Update(
        controllerTransform, tipWorldPosition, pivotWorldPosition,
        Abomination::HeadRotation * Math::Left(), DeltaTime(),
        planePosition, planeRotation, tipDeviation, pivotDeviation, pivotHeight, minimalSwingAngle, maximalSwingAngle
    );

    bool isLeft = result.SelectedHand == Hand::Left;
    float swingCurveAngle = std::asin(std::clamp(pivotHeight, -1.0f, 1.0f));
    float fullSwingAngle = maximalSwingAngle - minimalSwingAngle;

    result.IsSwingGood = fullSwingAngle > FullSwingAngleRequirement;
    swingCurveAngleDeg = swingCurveAngle * Math::Rad2Deg;

    result.SwingCurveAngle = isLeft ? -swingCurveAngle : swingCurveAngle;
    result.TipDeviation = tipDeviation;
    result.PivotDeviation = pivotDeviation;
    result.MinimalSwingAngle = minimalSwingAngle;
    result.MaximalSwingAngle = maximalSwingAngle;
    UpdatedEvent.Invoke(result);
}

static Quaternion CalculateReferenceRotation(Vector3 averageNormal, Quaternion currentLocalRotation) {
    auto currentLocalDirection = TransformUtils::DirectionFromRotation(currentLocalRotation);
    auto referenceRotation = Math::LookRotation(currentLocalDirection, averageNormal);
    referenceRotation *= Math::Euler(-swingCurveAngleDeg, 0.0f, 0.0f);
    referenceRotation *= Math::Euler(0.0f, 0.0f, 90.0f);
    return referenceRotation;
}

static void Finish() {
    auto averageNormal = swingAnalyzer.GetAverageLocalPlaneNormal();
    auto reference = CalculateReferenceRotation(averageNormal, PluginConfig::GetSaberRotation(result.SelectedHand));
    if (result.SelectedHand == Hand::Left) {
        leftReferenceRotation = reference;
    } else {
        rightReferenceRotation = reference;
    }
    result.IsTracking = false;
    UpdatedEvent.Invoke(result);
}

void Reset() {
    result = {};
    UpdatedEvent.Invoke(result);
}

void AutoFix() {
    if (!result.HasResults || !result.IsSwingGood) return;
    auto hand = result.SelectedHand;
    auto reference = hand == Hand::Left ? leftReferenceRotation : rightReferenceRotation;
    PluginConfig::CreateUndoStep(std::string(HandName(hand)) + " Auto-fix");
    PluginConfig::SetSaberRotation(hand, TransformUtils::AlignForwardVectors(PluginConfig::GetSaberRotation(hand), reference));
    PluginConfig::Save();
}

void SetAsReference() {
    if (!result.HasResults || !result.IsSwingGood) return;
    auto hand = result.SelectedHand;
    PluginConfig::CreateUndoStep(Describe("Set", hand, "Reference"));
    PluginConfig::SetSaberReference(hand, true, hand == Hand::Left ? leftReferenceRotation : rightReferenceRotation);
    PluginConfig::Save();
}
}

class SwingBenchmarkAdjustmentModeManager : public AbstractAdjustmentModeManager {
public:
    SwingBenchmarkAdjustmentModeManager() : AbstractAdjustmentModeManager(AdjustmentMode::SwingBenchmark, 0.0f, 0.0f) {}

protected:
    void OnGrabStarted(Hand hand, ReeTransform const&, ReeTransform const&) override {
        SwingBenchmark::Start(hand);
    }

    void OnGrabUpdated(Hand hand, ReeTransform const& adjustmentHandTransform, ReeTransform const&) override {
        auto pivotWorldPosition = adjustmentHandTransform.LocalToWorldPosition(PluginConfig::GetSaberPivotPosition(hand));
        auto saberWorldRotation = adjustmentHandTransform.LocalToWorldRotation(PluginConfig::GetSaberRotation(hand));
        SwingBenchmark::Update(adjustmentHandTransform, pivotWorldPosition, saberWorldRotation);
    }

    void OnGrabFinished(Hand, ReeTransform const&, ReeTransform const&) override {
        SwingBenchmark::Finish();
    }
};

#pragma endregion

#pragma region Direct

namespace DirectMode {
static constexpr SliderValueType AllTypes[] = {
    SliderValueType::ZOffset, SliderValueType::PositionX, SliderValueType::PositionY, SliderValueType::PositionZ,
    SliderValueType::RotationX, SliderValueType::RotationY, SliderValueType::RotationZ, SliderValueType::Curve, SliderValueType::Balance
};

static std::vector<std::unique_ptr<DirectModeVariable>> MakeVariables(Hand hand) {
    std::vector<std::unique_ptr<DirectModeVariable>> result;
    for (auto type : AllTypes) result.push_back(std::make_unique<DirectModeVariable>(type, hand));
    return result;
}

static auto leftVariables = MakeVariables(Hand::Left);
static auto rightVariables = MakeVariables(Hand::Right);

DirectModeVariable& Get(Hand hand, SliderValueType type) {
    return *(hand == Hand::Left ? leftVariables : rightVariables)[static_cast<int>(type)];
}

static float Value(Hand hand, SliderValueType type) { return Get(hand, type).GetValue(); }

static Vector3 PivotPosition(Hand hand) {
    return {Value(hand, SliderValueType::PositionX), Value(hand, SliderValueType::PositionY), Value(hand, SliderValueType::PositionZ)};
}

static void SetPivotPosition(Hand hand, Vector3 value) {
    Get(hand, SliderValueType::PositionX).SetValueFromCode(value.x);
    Get(hand, SliderValueType::PositionY).SetValueFromCode(value.y);
    Get(hand, SliderValueType::PositionZ).SetValueFromCode(value.z);
}

static Vector3 RotationEuler(Hand hand) {
    return {Value(hand, SliderValueType::RotationX), Value(hand, SliderValueType::RotationY), Value(hand, SliderValueType::RotationZ)};
}

static void SetRotationEuler(Hand hand, Vector3 value) {
    Get(hand, SliderValueType::RotationX).SetValueFromCode(value.x);
    Get(hand, SliderValueType::RotationY).SetValueFromCode(value.y);
    Get(hand, SliderValueType::RotationZ).SetValueFromCode(value.z);
}

static Quaternion Rotation(Hand hand) { return TransformUtils::RotationFromEuler(RotationEuler(hand)); }

static bool skipControllerSpaceRecalculation = false;
static bool skipReferenceSpaceRecalculation = false;
static bool applyingDirectConfig = false;

static void RecalculateControllerSpaceRotations() {
    if (skipControllerSpaceRecalculation) return;
    for (auto hand : {Hand::Left, Hand::Right}) {
        if (!PluginConfig::GetSaberHasReference(hand)) continue;
        SetRotationEuler(hand, TransformUtils::FromReferenceSpace(
            Rotation(hand), PluginConfig::GetSaberReferenceRotation(hand),
            Value(hand, SliderValueType::Curve), Value(hand, SliderValueType::Balance)
        ));
    }
}

static void RecalculateReferenceSpaceRotations() {
    if (skipReferenceSpaceRecalculation) return;
    for (auto hand : {Hand::Left, Hand::Right}) {
        float curve = 0.0f, balance = 0.0f;
        if (PluginConfig::GetSaberHasReference(hand)) {
            TransformUtils::ToReferenceSpace(Rotation(hand), PluginConfig::GetSaberReferenceRotation(hand), curve, balance);
        }
        Get(hand, SliderValueType::Curve).SetValueFromCode(curve);
        Get(hand, SliderValueType::Balance).SetValueFromCode(balance);
    }
}

static void ApplyDirectConfig() {
    applyingDirectConfig = true;
    PluginConfig::SetSaberOffsets(
        PivotPosition(Hand::Left), Rotation(Hand::Left), Value(Hand::Left, SliderValueType::ZOffset),
        PivotPosition(Hand::Right), Rotation(Hand::Right), Value(Hand::Right, SliderValueType::ZOffset)
    );
    applyingDirectConfig = false;
}

static void ApplyMainConfig() {
    for (auto hand : {Hand::Left, Hand::Right}) {
        SetPivotPosition(hand, PluginConfig::GetSaberPivotPosition(hand));
        SetRotationEuler(hand, PluginConfig::GetSaberRotationEuler(hand));
        Get(hand, SliderValueType::ZOffset).SetValueFromCode(PluginConfig::GetSaberZOffset(hand));
    }
    RecalculateReferenceSpaceRotations();
}

static void OnValueChangedFromUI(SliderValueType valueType) {
    switch (valueType) {
        case SliderValueType::ZOffset:
        case SliderValueType::PositionX:
        case SliderValueType::PositionY:
        case SliderValueType::PositionZ:
            break;
        case SliderValueType::RotationX:
        case SliderValueType::RotationY:
        case SliderValueType::RotationZ:
            skipControllerSpaceRecalculation = true;
            RecalculateReferenceSpaceRotations();
            break;
        case SliderValueType::Curve:
        case SliderValueType::Balance:
            skipReferenceSpaceRecalculation = true;
            RecalculateControllerSpaceRotations();
            break;
    }

    ApplyDirectConfig();

    skipControllerSpaceRecalculation = false;
    skipReferenceSpaceRecalculation = false;
}

static void Initialize() {
    PluginConfig::ConfigWasChangedEvent.Subscribe([]() {
        // Re-reading our own change would round-trip euler angles and fight the sliders
        if (applyingDirectConfig) return;
        ApplyMainConfig();
    });
    ApplyMainConfig();
}
}

static std::string_view SliderValueTypeName(SliderValueType type) {
    switch (type) {
        case SliderValueType::ZOffset: return "ZOffset";
        case SliderValueType::PositionX: return "PositionX";
        case SliderValueType::PositionY: return "PositionY";
        case SliderValueType::PositionZ: return "PositionZ";
        case SliderValueType::RotationX: return "RotationX";
        case SliderValueType::RotationY: return "RotationY";
        case SliderValueType::RotationZ: return "RotationZ";
        case SliderValueType::Curve: return "Curve";
        case SliderValueType::Balance: return "Balance";
    }
    return "";
}

// BSML sliders have no press/release events, so one undo step covers a burst of changes
static DirectModeVariable const* lastUndoVariable = nullptr;
static float lastUndoTime = -100.0f;
static constexpr float UndoMergeTime = 1.0f;

void DirectModeVariable::SetValueFromUI(float newValue) {
    if (value == newValue) return;

    float now = UnityEngine::Time::get_time();
    if (lastUndoVariable != this || now - lastUndoTime > UndoMergeTime) {
        PluginConfig::CreateUndoStep(Describe("Change", hand, SliderValueTypeName(sliderValueType)));
        lastUndoVariable = this;
    }
    lastUndoTime = now;

    value = newValue;
    DirectMode::OnValueChangedFromUI(sliderValueType);
}

void DirectModeVariable::SetValueFromCode(float newValue) {
    if (value == newValue) return;
    value = newValue;
    ChangedFromCodeEvent.Invoke(newValue);
}

#pragma endregion

#pragma region AdjustmentBlockManager

// Adjustments only run while the mod panel is on screen
static AdjustmentMode lastAdjustmentMode = AdjustmentMode::None;
static bool adjustmentsEnabled = false;

static void OnIsModPanelVisibleChanged(bool visible) {
    if (visible) {
        if (adjustmentsEnabled) return;
        PluginConfig::SetAdjustmentMode(lastAdjustmentMode);
        adjustmentsEnabled = true;
    } else {
        if (!adjustmentsEnabled) return;
        lastAdjustmentMode = PluginConfig::GetAdjustmentMode();
        PluginConfig::SetAdjustmentMode(AdjustmentMode::None);
        adjustmentsEnabled = false;
    }
}

#pragma endregion

namespace AdjustmentModes {
static std::vector<std::unique_ptr<AbstractAdjustmentModeManager>> managers;

void Initialize() {
    if (!managers.empty()) return;

    managers.push_back(std::make_unique<BasicAdjustmentModeManager>());
    managers.push_back(std::make_unique<PositionAdjustmentModeManager>());
    managers.push_back(std::make_unique<RotationAdjustmentModeManager>());
    managers.push_back(std::make_unique<SwingBenchmarkAdjustmentModeManager>());
    managers.push_back(std::make_unique<PositionAutoAdjustmentModeManager>());
    managers.push_back(std::make_unique<RotationAutoAdjustmentModeManager>());
    managers.push_back(std::make_unique<RoomOffsetAdjustmentModeManager>());
    for (auto& manager : managers) manager->Initialize();

    DirectMode::Initialize();

    PluginConfig::IsModPanelVisibleChangedEvent.Subscribe(OnIsModPanelVisibleChanged);
    OnIsModPanelVisibleChanged(PluginConfig::GetIsModPanelVisible());
}
}
}

#include "PluginConfig.hpp"
#include "TransformUtils.hpp"
#include "main.hpp"

#include "GlobalNamespace/SettingsApplicatorSO.hpp"
#include "GlobalNamespace/SettingsManager.hpp"
#include "BeatSaber/Settings/Settings.hpp"
#include "BeatSaber/Settings/RoomSettings.hpp"
#include "Unity/Mathematics/float3.hpp"

#include "beatsaber-hook/shared/config/config-utils.hpp"

#include <chrono>

namespace EasyOffset::PluginConfig {

#pragma region Events

Event<> ConfigWasChangedEvent;
Event<bool> OnEnabledChange;
Event<AdjustmentMode> AdjustmentModeChangedEvent;
Event<ControllerButton> AssignedButtonChangedEvent;
Event<bool> IsModPanelVisibleChangedEvent;
Event<bool, std::string> UndoAvailableChangedEvent;
Event<bool, std::string> RedoAvailableChangedEvent;

static bool disableChangeEvent = false;

static void NotifyConfigWasChanged() {
    MarkDirty();
    if (disableChangeEvent) return;
    ConfigWasChangedEvent.Invoke();
}

static void DisableConfigChangeEvent() { disableChangeEvent = true; }
static void EnableConfigChangeEvent() { disableChangeEvent = false; }

#pragma endregion

#pragma region State

struct HandConfig {
    Vector3 PivotPosition = ConfigDefaults::SaberPivotPosition();
    Quaternion Rotation = Math::Identity();
    float ZOffset = ConfigDefaults::ZOffset;
    bool HasReference = false;
    Quaternion ReferenceRotation = Math::Identity();
    Vector3 Translation = Math::Zero();

    void UpdateTranslation() {
        Translation = TransformUtils::GetSaberTranslation(PivotPosition, Rotation, ZOffset);
    }
};

static HandConfig leftHand;
static HandConfig rightHand;

static HandConfig& HandRef(Hand hand) { return hand == Hand::Left ? leftHand : rightHand; }

static bool enabled = true;
static TrackingSpace trackingSpace = TrackingSpace::PcLink;
static bool useFreeHand = true;
static ControllerButton assignedButton = ControllerButton::SecondaryButton;
static bool allowRoomXChange = true;
static bool allowRoomYChange = true;
static bool allowRoomZChange = true;
static AdjustmentMode adjustmentMode = AdjustmentMode::None;
static bool isModPanelVisible = false;
static bool bundledPresetApplied = false;

bool SmoothingEnabled = false;
float PositionalSmoothing = 0.0f;
float RotationalSmoothing = 0.0f;

static bool dirty = false;

#pragma endregion

#pragma region Persistence

// PC EasyOffset stores positions in centimetres
static constexpr float PositionUnitScale = 0.01f;
static constexpr char const* CurrentConfigVersion = "2.0";

static Configuration& GetConfiguration() {
    static Configuration configuration(GetModInfo());
    return configuration;
}

using Value = rapidjson::Value;
using Allocator = rapidjson::Document::AllocatorType;

static Value SerializeVector(Vector3 v, Allocator& allocator) {
    Value obj(rapidjson::kObjectType);
    obj.AddMember("x", v.x, allocator);
    obj.AddMember("y", v.y, allocator);
    obj.AddMember("z", v.z, allocator);
    return obj;
}

static Value SerializeQuaternion(Quaternion q, Allocator& allocator) {
    Value obj(rapidjson::kObjectType);
    obj.AddMember("x", q.x, allocator);
    obj.AddMember("y", q.y, allocator);
    obj.AddMember("z", q.z, allocator);
    obj.AddMember("w", q.w, allocator);
    return obj;
}

static float GetFloat(Value const& obj, char const* key, float fallback) {
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsNumber()) return fallback;
    return it->value.GetFloat();
}

static bool GetBool(Value const& obj, char const* key, bool fallback) {
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsBool()) return fallback;
    return it->value.GetBool();
}

static std::string GetString(Value const& obj, char const* key, std::string fallback) {
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsString()) return fallback;
    return it->value.GetString();
}

static Vector3 GetVector(Value const& obj, char const* key, Vector3 fallback) {
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsObject()) return fallback;
    auto const& v = it->value;
    return {GetFloat(v, "x", fallback.x), GetFloat(v, "y", fallback.y), GetFloat(v, "z", fallback.z)};
}

static Quaternion GetQuaternion(Value const& obj, char const* key, Quaternion fallback) {
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsObject()) return fallback;
    auto const& v = it->value;
    return {GetFloat(v, "x", fallback.x), GetFloat(v, "y", fallback.y), GetFloat(v, "z", fallback.z), GetFloat(v, "w", fallback.w)};
}

static void LoadHand(Value const& root, std::string const& prefix, HandConfig& hand) {
    auto key = [&](char const* name) { return prefix + name; };

    auto pivotCm = GetVector(root, key("SaberPivotPosition").c_str(), ConfigDefaults::SaberPivotPosition() / PositionUnitScale);
    hand.PivotPosition = Math::ClampMagnitude(pivotCm * PositionUnitScale, ConfigDefaults::MaximalPositionOffset);
    hand.Rotation = TransformUtils::RotationFromEuler(GetVector(root, key("SaberRotationEuler").c_str(), Math::Zero()));
    hand.ZOffset = GetFloat(root, key("SaberZOffset").c_str(), ConfigDefaults::ZOffset / PositionUnitScale) * PositionUnitScale;
    hand.HasReference = GetBool(root, key("SaberHasReference").c_str(), false);
    hand.ReferenceRotation = GetQuaternion(root, key("SaberReference").c_str(), Math::Identity());
    hand.UpdateTranslation();
}

static void WriteHand(Value& root, std::string const& prefix, HandConfig const& hand, Allocator& allocator) {
    auto key = [&](char const* name) { return Value(prefix + name, allocator); };

    root.AddMember(key("SaberPivotPosition"), SerializeVector(hand.PivotPosition / PositionUnitScale, allocator), allocator);
    root.AddMember(key("SaberRotationEuler"), SerializeVector(TransformUtils::EulerFromRotation(hand.Rotation), allocator), allocator);
    root.AddMember(key("SaberZOffset"), Value(hand.ZOffset / PositionUnitScale), allocator);
    root.AddMember(key("SaberHasReference"), Value(hand.HasReference), allocator);
    root.AddMember(key("SaberReference"), SerializeQuaternion(hand.ReferenceRotation, allocator), allocator);
}

void Load() {
    auto& configuration = GetConfiguration();
    try {
        configuration.Load();
    } catch (std::exception const& e) {
        Logger.error("Config file unreadable, using defaults: {}", e.what());
        configuration.config.SetObject();
    }
    auto& doc = configuration.config;

    if (doc.IsObject()) {
        enabled = GetBool(doc, "Enabled", true);
        trackingSpace = GetString(doc, "TrackingSpace", "PcLink") == "QuestNative" ? TrackingSpace::QuestNative : TrackingSpace::PcLink;
        useFreeHand = GetBool(doc, "UseFreeHand", true);
        assignedButton = ControllerButtonFromName(GetString(doc, "AssignedButton", "SecondaryButton"));
        allowRoomXChange = GetBool(doc, "AllowRoomXChange", true);
        allowRoomYChange = GetBool(doc, "AllowRoomYChange", true);
        allowRoomZChange = GetBool(doc, "AllowRoomZChange", true);
        bundledPresetApplied = GetBool(doc, "BundledPresetApplied", false);
        LoadHand(doc, "Left", leftHand);
        LoadHand(doc, "Right", rightHand);
    } else {
        leftHand.UpdateTranslation();
        rightHand.UpdateTranslation();
    }

    // Writes defaults on first launch and upgrades older files
    Save();
}

void Save() {
    auto& configuration = GetConfiguration();
    auto& doc = configuration.config;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

    doc.AddMember("ConfigVersion", Value(CurrentConfigVersion, allocator), allocator);
    doc.AddMember("Enabled", enabled, allocator);
    doc.AddMember("TrackingSpace", Value(trackingSpace == TrackingSpace::QuestNative ? "QuestNative" : "PcLink", allocator), allocator);
    doc.AddMember("AllowRoomXChange", allowRoomXChange, allocator);
    doc.AddMember("AllowRoomYChange", allowRoomYChange, allocator);
    doc.AddMember("AllowRoomZChange", allowRoomZChange, allocator);
    doc.AddMember("AssignedButton", Value(std::string(ControllerButtonName(assignedButton)), allocator), allocator);
    doc.AddMember("ControllerType", Value("Quest 2", allocator), allocator);
    doc.AddMember("UseFreeHand", useFreeHand, allocator);
    doc.AddMember("BundledPresetApplied", bundledPresetApplied, allocator);
    WriteHand(doc, "Left", leftHand, allocator);
    WriteHand(doc, "Right", rightHand, allocator);

    configuration.Write();
    dirty = false;
}

void MarkDirty() { dirty = true; }

void SaveIfDirty() {
    if (dirty) Save();
}

#pragma endregion

#pragma region Preferences

bool GetEnabled() { return enabled; }

void SetEnabled(bool value) {
    if (enabled == value) return;
    enabled = value;
    MarkDirty();
    OnEnabledChange.Invoke(value);
}

TrackingSpace GetTrackingSpace() { return trackingSpace; }

void SetTrackingSpace(TrackingSpace value) {
    if (trackingSpace == value) return;
    trackingSpace = value;
    MarkDirty();
}

bool GetUseFreeHand() { return useFreeHand; }

void SetUseFreeHand(bool value) {
    useFreeHand = value;
    MarkDirty();
}

ControllerButton GetAssignedButton() { return assignedButton; }

void SetAssignedButton(ControllerButton value) {
    if (assignedButton == value) return;
    assignedButton = value;
    MarkDirty();
    AssignedButtonChangedEvent.Invoke(value);
}

AdjustmentMode GetAdjustmentMode() { return adjustmentMode; }

void SetAdjustmentMode(AdjustmentMode value) {
    adjustmentMode = value;
    AdjustmentModeChangedEvent.Invoke(value);
}

bool GetIsModPanelVisible() { return isModPanelVisible; }

void SetIsModPanelVisible(bool value) {
    if (isModPanelVisible == value) return;
    isModPanelVisible = value;
    IsModPanelVisibleChangedEvent.Invoke(value);
}

bool GetBundledPresetApplied() { return bundledPresetApplied; }
void SetBundledPresetApplied(bool value) { bundledPresetApplied = value; MarkDirty(); }

bool GetAllowRoomXChange() { return allowRoomXChange; }
void SetAllowRoomXChange(bool value) { allowRoomXChange = value; MarkDirty(); }
bool GetAllowRoomYChange() { return allowRoomYChange; }
void SetAllowRoomYChange(bool value) { allowRoomYChange = value; MarkDirty(); }
bool GetAllowRoomZChange() { return allowRoomZChange; }
void SetAllowRoomZChange(bool value) { allowRoomZChange = value; MarkDirty(); }

#pragma endregion

#pragma region Hand config

Vector3 GetSaberTranslation(Hand hand) { return HandRef(hand).Translation; }

Quaternion GetSaberRotation(Hand hand) { return HandRef(hand).Rotation; }

void SetSaberRotation(Hand hand, Quaternion value) {
    auto& config = HandRef(hand);
    if (Math::Equals(config.Rotation, value)) return;
    config.Rotation = value;
    config.UpdateTranslation();
    NotifyConfigWasChanged();
}

Vector3 GetSaberPivotPosition(Hand hand) { return HandRef(hand).PivotPosition; }

void SetSaberPivotPosition(Hand hand, Vector3 value) {
    auto& config = HandRef(hand);
    auto clamped = Math::ClampMagnitude(value, ConfigDefaults::MaximalPositionOffset);
    if (Math::Equals(config.PivotPosition, clamped)) return;
    config.PivotPosition = clamped;
    config.UpdateTranslation();
    NotifyConfigWasChanged();
}

float GetSaberZOffset(Hand hand) { return HandRef(hand).ZOffset; }

void SetSaberZOffset(Hand hand, float value) {
    auto& config = HandRef(hand);
    if (config.ZOffset == value) return;
    config.ZOffset = value;
    config.UpdateTranslation();
    NotifyConfigWasChanged();
}

Vector3 GetSaberRotationEuler(Hand hand) { return TransformUtils::EulerFromRotation(HandRef(hand).Rotation); }

bool GetSaberHasReference(Hand hand) { return HandRef(hand).HasReference; }

Quaternion GetSaberReferenceRotation(Hand hand) { return HandRef(hand).ReferenceRotation; }

static void SetSaberHasReferenceRaw(Hand hand, bool value) {
    HandRef(hand).HasReference = value;
    MarkDirty();
}

static void SetSaberReferenceRotationRaw(Hand hand, Quaternion value) {
    HandRef(hand).ReferenceRotation = value;
    MarkDirty();
}

#pragma endregion

#pragma region Actions

static Hand OtherHand(Hand hand) { return hand == Hand::Left ? Hand::Right : Hand::Left; }

void Mirror(Hand mirrorSource, bool mirrorPosition, bool mirrorRotation, bool mirrorReference) {
    DisableConfigChangeEvent();

    auto target = OtherHand(mirrorSource);

    if (mirrorPosition) {
        SetSaberPivotPosition(target, TransformUtils::MirrorVector(GetSaberPivotPosition(mirrorSource)));
        SetSaberZOffset(target, GetSaberZOffset(mirrorSource));
    }

    if (mirrorRotation) {
        SetSaberRotation(target, TransformUtils::MirrorRotation(GetSaberRotation(mirrorSource)));
    }

    if (mirrorReference) {
        SetSaberHasReferenceRaw(target, GetSaberHasReference(mirrorSource));
        SetSaberReferenceRotationRaw(target, TransformUtils::MirrorRotation(GetSaberReferenceRotation(mirrorSource)));
    }

    EnableConfigChangeEvent();
    NotifyConfigWasChanged();
}

void ResetOffsets(Hand hand, bool resetPosition, bool resetRotation, bool resetReference) {
    DisableConfigChangeEvent();

    if (resetPosition) {
        SetSaberPivotPosition(hand, ConfigDefaults::SaberPivotPosition());
        SetSaberZOffset(hand, ConfigDefaults::ZOffset);
    }

    if (resetRotation) {
        SetSaberRotation(hand, Math::Identity());
    }

    if (resetReference) {
        SetSaberHasReferenceRaw(hand, false);
        SetSaberReferenceRotationRaw(hand, Math::Identity());
    }

    EnableConfigChangeEvent();
    NotifyConfigWasChanged();
}

void ApplyPreset(ConfigPreset const& preset) {
    DisableConfigChangeEvent();

    SetSaberPivotPosition(Hand::Left, preset.LeftSaberPivotPosition);
    SetSaberRotation(Hand::Left, preset.LeftSaberRotation);
    SetSaberZOffset(Hand::Left, preset.LeftSaberZOffset);
    SetSaberHasReferenceRaw(Hand::Left, preset.LeftSaberHasReference);
    SetSaberReferenceRotationRaw(Hand::Left, preset.LeftSaberReferenceRotation);

    SetSaberPivotPosition(Hand::Right, preset.RightSaberPivotPosition);
    SetSaberRotation(Hand::Right, preset.RightSaberRotation);
    SetSaberZOffset(Hand::Right, preset.RightSaberZOffset);
    SetSaberHasReferenceRaw(Hand::Right, preset.RightSaberHasReference);
    SetSaberReferenceRotationRaw(Hand::Right, preset.RightSaberReferenceRotation);

    EnableConfigChangeEvent();
    NotifyConfigWasChanged();
}

ConfigPreset GeneratePreset() {
    ConfigPreset preset;
    preset.UnixTimestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    preset.LeftSaberPivotPosition = leftHand.PivotPosition;
    preset.LeftSaberRotation = leftHand.Rotation;
    preset.LeftSaberZOffset = leftHand.ZOffset;
    preset.LeftSaberHasReference = leftHand.HasReference;
    preset.LeftSaberReferenceRotation = leftHand.ReferenceRotation;

    preset.RightSaberPivotPosition = rightHand.PivotPosition;
    preset.RightSaberRotation = rightHand.Rotation;
    preset.RightSaberZOffset = rightHand.ZOffset;
    preset.RightSaberHasReference = rightHand.HasReference;
    preset.RightSaberReferenceRotation = rightHand.ReferenceRotation;
    return preset;
}

void SetSaberOffsets(
    Vector3 leftPivotPosition, Quaternion leftRotation, float leftZOffset,
    Vector3 rightPivotPosition, Quaternion rightRotation, float rightZOffset
) {
    DisableConfigChangeEvent();

    SetSaberPivotPosition(Hand::Left, leftPivotPosition);
    SetSaberRotation(Hand::Left, leftRotation);
    SetSaberZOffset(Hand::Left, leftZOffset);

    SetSaberPivotPosition(Hand::Right, rightPivotPosition);
    SetSaberRotation(Hand::Right, rightRotation);
    SetSaberZOffset(Hand::Right, rightZOffset);

    EnableConfigChangeEvent();
    NotifyConfigWasChanged();
}

void ResetSaberReference(Hand hand) {
    SetSaberReference(hand, false, Math::Identity());
}

void SetSaberReference(Hand hand, bool hasReference, Quaternion referenceRotation) {
    SetSaberHasReferenceRaw(hand, hasReference);
    SetSaberReferenceRotationRaw(hand, referenceRotation);
    NotifyConfigWasChanged();
}

void AlignReferenceToCurrent(Hand hand) {
    if (!GetSaberHasReference(hand)) return;
    SetSaberReferenceRotationRaw(hand, TransformUtils::AlignForwardVectors(GetSaberReferenceRotation(hand), GetSaberRotation(hand)));
    NotifyConfigWasChanged();
}

#pragma endregion

#pragma region Undo

static constexpr size_t MaxUndoStepsCount = 30;

struct ConfigSnapshot {
    std::string Description;
    std::function<void()> OnUndoAction;
    std::function<void()> OnRedoAction;
    HandConfig Left;
    HandConfig Right;
};

static std::vector<ConfigSnapshot> undoSteps;
static std::vector<ConfigSnapshot> redoSteps;

static ConfigSnapshot GenerateSnapshot(std::string description, std::function<void()> onUndoAction, std::function<void()> onRedoAction) {
    return {std::move(description), std::move(onUndoAction), std::move(onRedoAction), leftHand, rightHand};
}

static void ApplySnapshot(ConfigSnapshot const& snapshot) {
    DisableConfigChangeEvent();

    for (auto hand : {Hand::Left, Hand::Right}) {
        auto const& source = hand == Hand::Left ? snapshot.Left : snapshot.Right;
        SetSaberPivotPosition(hand, source.PivotPosition);
        SetSaberRotation(hand, source.Rotation);
        SetSaberZOffset(hand, source.ZOffset);
        SetSaberHasReferenceRaw(hand, source.HasReference);
        SetSaberReferenceRotationRaw(hand, source.ReferenceRotation);
    }

    EnableConfigChangeEvent();
    NotifyConfigWasChanged();
}

static void NotifyUndoListChanged() {
    UndoAvailableChangedEvent.Invoke(UndoAvailable(), UndoDescription());
}

static void NotifyRedoListChanged() {
    RedoAvailableChangedEvent.Invoke(RedoAvailable(), RedoDescription());
}

void CreateUndoStep(std::string description, std::function<void()> onUndoAction, std::function<void()> onRedoAction) {
    undoSteps.push_back(GenerateSnapshot(std::move(description), std::move(onUndoAction), std::move(onRedoAction)));
    redoSteps.clear();
    if (undoSteps.size() > MaxUndoStepsCount) undoSteps.erase(undoSteps.begin());
    NotifyUndoListChanged();
    NotifyRedoListChanged();
}

void UndoLastChange() {
    if (undoSteps.empty()) return;

    auto undoSnapshot = std::move(undoSteps.back());
    undoSteps.pop_back();

    redoSteps.push_back(GenerateSnapshot(undoSnapshot.Description, undoSnapshot.OnUndoAction, undoSnapshot.OnRedoAction));

    ApplySnapshot(undoSnapshot);
    if (undoSnapshot.OnUndoAction) undoSnapshot.OnUndoAction();
    Save();

    NotifyUndoListChanged();
    NotifyRedoListChanged();
}

void RedoLastChange() {
    if (redoSteps.empty()) return;

    auto redoSnapshot = std::move(redoSteps.back());
    redoSteps.pop_back();

    undoSteps.push_back(GenerateSnapshot(redoSnapshot.Description, redoSnapshot.OnUndoAction, redoSnapshot.OnRedoAction));

    ApplySnapshot(redoSnapshot);
    if (redoSnapshot.OnRedoAction) redoSnapshot.OnRedoAction();
    Save();

    NotifyUndoListChanged();
    NotifyRedoListChanged();
}

bool UndoAvailable() { return !undoSteps.empty(); }
std::string UndoDescription() { return undoSteps.empty() ? "" : undoSteps.back().Description; }
bool RedoAvailable() { return !redoSteps.empty(); }
std::string RedoDescription() { return redoSteps.empty() ? "" : redoSteps.back().Description; }

#pragma endregion

#pragma region Game settings

static GlobalNamespace::SettingsManager* mainSettingsManager = nullptr;
static GlobalNamespace::SettingsApplicatorSO* settingsApplicator = nullptr;

void SetGameSettings(GlobalNamespace::SettingsManager* settingsManager, GlobalNamespace::SettingsApplicatorSO* applicator) {
    mainSettingsManager = settingsManager;
    settingsApplicator = applicator;
}

bool HasGameSettings() { return mainSettingsManager != nullptr; }

Vector3 GetBaseGameRoomCenter() {
    if (!mainSettingsManager) return Math::Zero();
    auto center = mainSettingsManager->settings.room.center;
    return {center.x, center.y, center.z};
}

void SetBaseGameRoomCenter(Vector3 value) {
    if (!mainSettingsManager) return;
    mainSettingsManager->__cordl_internal_get_settings().room.center = Unity::Mathematics::float3(value.x, value.y, value.z);
    if (settingsApplicator) settingsApplicator->NotifyRoomTransformOffsetWasUpdated();
}

#pragma endregion
}

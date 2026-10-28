#pragma once

#include "Enums.hpp"
#include "Math.hpp"

#include <functional>
#include <string>

namespace GlobalNamespace {
class SettingsManager;
class SettingsApplicatorSO;
}

namespace EasyOffset {

struct ConfigPreset {
    std::string Version = "2.0";
    int64_t UnixTimestamp = 0;
    std::string ControllerType = "Quest 2";

    Vector3 LeftSaberPivotPosition;
    Quaternion LeftSaberRotation;
    float LeftSaberZOffset = 0;
    bool LeftSaberHasReference = false;
    Quaternion LeftSaberReferenceRotation;

    Vector3 RightSaberPivotPosition;
    Quaternion RightSaberRotation;
    float RightSaberZOffset = 0;
    bool RightSaberHasReference = false;
    Quaternion RightSaberReferenceRotation;
};

namespace ConfigDefaults {
constexpr float MaximalPositionOffset = 0.4f;
constexpr float ZOffset = 0.155f;
inline Vector3 SaberPivotPosition() { return {0.0f, 0.0f, -ZOffset}; }
}

// Static state mirroring PC EasyOffset's PluginConfig partial class
namespace PluginConfig {

void Load();
void Save();
void MarkDirty();
void SaveIfDirty();

#pragma region Events
extern Event<> ConfigWasChangedEvent;
extern Event<bool> OnEnabledChange;
extern Event<AdjustmentMode> AdjustmentModeChangedEvent;
extern Event<ControllerButton> AssignedButtonChangedEvent;
extern Event<bool> IsModPanelVisibleChangedEvent;
extern Event<bool, std::string> UndoAvailableChangedEvent;
extern Event<bool, std::string> RedoAvailableChangedEvent;
#pragma endregion

#pragma region Preferences
bool GetEnabled();
void SetEnabled(bool value);

TrackingSpace GetTrackingSpace();
void SetTrackingSpace(TrackingSpace value);

bool GetUseFreeHand();
void SetUseFreeHand(bool value);

ControllerButton GetAssignedButton();
void SetAssignedButton(ControllerButton value);

AdjustmentMode GetAdjustmentMode();
void SetAdjustmentMode(AdjustmentMode value);

bool GetIsModPanelVisible();
void SetIsModPanelVisible(bool value);

bool GetBundledPresetApplied();
void SetBundledPresetApplied(bool value);

bool GetAllowRoomXChange();
void SetAllowRoomXChange(bool value);
bool GetAllowRoomYChange();
void SetAllowRoomYChange(bool value);
bool GetAllowRoomZChange();
void SetAllowRoomZChange(bool value);
#pragma endregion

#pragma region Smoothing
extern bool SmoothingEnabled;
extern float PositionalSmoothing;
extern float RotationalSmoothing;
#pragma endregion

#pragma region Hand config
Vector3 GetSaberTranslation(Hand hand);

Quaternion GetSaberRotation(Hand hand);
void SetSaberRotation(Hand hand, Quaternion value);

Vector3 GetSaberPivotPosition(Hand hand);
void SetSaberPivotPosition(Hand hand, Vector3 value);

float GetSaberZOffset(Hand hand);
void SetSaberZOffset(Hand hand, float value);

Vector3 GetSaberRotationEuler(Hand hand);

bool GetSaberHasReference(Hand hand);
Quaternion GetSaberReferenceRotation(Hand hand);
#pragma endregion

#pragma region Actions
void Mirror(Hand mirrorSource, bool mirrorPosition, bool mirrorRotation, bool mirrorReference);
void ResetOffsets(Hand hand, bool resetPosition, bool resetRotation, bool resetReference);
void ApplyPreset(ConfigPreset const& preset);
ConfigPreset GeneratePreset();
void SetSaberOffsets(
    Vector3 leftPivotPosition, Quaternion leftRotation, float leftZOffset,
    Vector3 rightPivotPosition, Quaternion rightRotation, float rightZOffset
);
void ResetSaberReference(Hand hand);
void SetSaberReference(Hand hand, bool hasReference, Quaternion referenceRotation);
void AlignReferenceToCurrent(Hand hand);
#pragma endregion

#pragma region Undo
void CreateUndoStep(std::string description, std::function<void()> onUndoAction = nullptr, std::function<void()> onRedoAction = nullptr);
void UndoLastChange();
void RedoLastChange();
bool UndoAvailable();
std::string UndoDescription();
bool RedoAvailable();
std::string RedoDescription();
#pragma endregion

#pragma region Game settings
void SetGameSettings(GlobalNamespace::SettingsManager* settingsManager, GlobalNamespace::SettingsApplicatorSO* settingsApplicator);
bool HasGameSettings();
Vector3 GetBaseGameRoomCenter();
void SetBaseGameRoomCenter(Vector3 value);
#pragma endregion
}
}

#pragma once

#include "Enums.hpp"
#include "TransformUtils.hpp"

namespace GlobalNamespace {
class IVRPlatformHelper;
}

namespace EasyOffset {

// Raw (tracking space, PC-equivalent) controller poses and button events
namespace Abomination {
extern ReeTransform LeftControllerTransform;
extern ReeTransform RightControllerTransform;
extern Quaternion HeadRotation;
extern Event<> TransformsUpdatedEvent;

ReeTransform& GetControllerTransform(Hand hand);

void UpdateTransforms(Vector3 leftPosition, Quaternion leftRotation, Vector3 rightPosition, Quaternion rightRotation, float deltaTime);

extern Event<Hand, ControllerButton> ButtonPressedEvent;
extern Event<Hand, ControllerButton> ButtonReleasedEvent;
extern Event<Hand> AssignedButtonPressedEvent;
extern Event<Hand> AssignedButtonReleasedEvent;

void OnButtonPressed(Hand hand, ControllerButton controllerButton);
void OnButtonReleased(Hand hand, ControllerButton controllerButton);
}

// Per-frame driver: polls poses and buttons (stand-in for Zenject ITickables)
namespace Tick {
void SetVRPlatformHelper(GlobalNamespace::IVRPlatformHelper* helper);
GlobalNamespace::IVRPlatformHelper* GetVRPlatformHelper();
bool GetRawControllerPose(Hand hand, Vector3& position, Quaternion& rotation);
void Update();
}

enum class ReeTriggerState { Released, LeftPressed, RightPressed };

namespace InputManager {
ReeTriggerState GetTriggerState();
void Update();
}
}

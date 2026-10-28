#include "Abomination.hpp"
#include "AdjustmentModes.hpp"
#include "PluginConfig.hpp"
#include "main.hpp"

#include "GlobalNamespace/IVRPlatformHelper.hpp"
#include "GlobalNamespace/OVRInput.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/XR/XRNode.hpp"

namespace EasyOffset {

#pragma region Abomination

namespace Abomination {
ReeTransform LeftControllerTransform(Math::Zero(), Math::Identity());
ReeTransform RightControllerTransform(Math::Zero(), Math::Identity());
Quaternion HeadRotation = Math::Identity();
Event<> TransformsUpdatedEvent;

Event<Hand, ControllerButton> ButtonPressedEvent;
Event<Hand, ControllerButton> ButtonReleasedEvent;
Event<Hand> AssignedButtonPressedEvent;
Event<Hand> AssignedButtonReleasedEvent;

ReeTransform& GetControllerTransform(Hand hand) {
    return hand == Hand::Left ? LeftControllerTransform : RightControllerTransform;
}

void UpdateTransforms(Vector3 leftPosition, Quaternion leftRotation, Vector3 rightPosition, Quaternion rightRotation, float deltaTime) {
    if (PluginConfig::PositionalSmoothing > 0.0f) {
        float tPos = deltaTime * PluginConfig::PositionalSmoothing;
        LeftControllerTransform.Position = Math::Lerp(LeftControllerTransform.Position, leftPosition, tPos);
        RightControllerTransform.Position = Math::Lerp(RightControllerTransform.Position, rightPosition, tPos);
    } else {
        LeftControllerTransform.Position = leftPosition;
        RightControllerTransform.Position = rightPosition;
    }

    if (PluginConfig::RotationalSmoothing > 0.0f) {
        float tRot = deltaTime * PluginConfig::RotationalSmoothing;
        LeftControllerTransform.SetRotation(Math::Lerp(LeftControllerTransform.GetRotation(), leftRotation, tRot));
        RightControllerTransform.SetRotation(Math::Lerp(RightControllerTransform.GetRotation(), rightRotation, tRot));
    } else {
        LeftControllerTransform.SetRotation(leftRotation);
        RightControllerTransform.SetRotation(rightRotation);
    }

    TransformsUpdatedEvent.Invoke();
}

void OnButtonPressed(Hand hand, ControllerButton controllerButton) {
    ButtonPressedEvent.Invoke(hand, controllerButton);
    if (controllerButton != PluginConfig::GetAssignedButton()) return;
    AssignedButtonPressedEvent.Invoke(hand);
}

void OnButtonReleased(Hand hand, ControllerButton controllerButton) {
    ButtonReleasedEvent.Invoke(hand, controllerButton);
    if (controllerButton != PluginConfig::GetAssignedButton()) return;
    AssignedButtonReleasedEvent.Invoke(hand);
}
}

#pragma endregion

#pragma region InputManager

namespace InputManager {
using GlobalNamespace::OVRInput;
using RawButton = GlobalNamespace::OVRInput_RawButton;
using Controller = GlobalNamespace::OVRInput_Controller;

struct ButtonTracker {
    Hand hand;
    ControllerButton button;
    RawButton rawButton;
    bool flag = false;

    void Update() {
        bool value = OVRInput::Get(rawButton, Controller::Touch);
        if (value == flag) return;
        flag = value;
        if (value) {
            Abomination::OnButtonPressed(hand, button);
        } else {
            Abomination::OnButtonReleased(hand, button);
        }
    }
};

static ButtonTracker trackers[] = {
    {Hand::Left, ControllerButton::PrimaryButton, RawButton::X},
    {Hand::Left, ControllerButton::SecondaryButton, RawButton::Y},
    {Hand::Left, ControllerButton::GripButton, RawButton::LHandTrigger},
    {Hand::Left, ControllerButton::Primary2DAxisClick, RawButton::LThumbstick},
    {Hand::Right, ControllerButton::PrimaryButton, RawButton::A},
    {Hand::Right, ControllerButton::SecondaryButton, RawButton::B},
    {Hand::Right, ControllerButton::GripButton, RawButton::RHandTrigger},
    {Hand::Right, ControllerButton::Primary2DAxisClick, RawButton::RThumbstick},
};

static constexpr float TriggerThreshold = 0.1f;
static bool leftTriggerPressed = false;
static bool rightTriggerPressed = false;
static ReeTriggerState triggerState = ReeTriggerState::Released;

ReeTriggerState GetTriggerState() { return triggerState; }

static void UpdateTriggerState(bool leftWasPressed, bool leftWasReleased, bool rightWasPressed, bool rightWasReleased) {
    switch (triggerState) {
        case ReeTriggerState::Released:
            if (rightWasPressed) {
                triggerState = ReeTriggerState::RightPressed;
            } else if (leftWasPressed) {
                triggerState = ReeTriggerState::LeftPressed;
            }
            break;
        case ReeTriggerState::LeftPressed:
            if (rightWasPressed) {
                triggerState = ReeTriggerState::RightPressed;
            } else if (leftWasReleased) {
                triggerState = ReeTriggerState::Released;
            }
            break;
        case ReeTriggerState::RightPressed:
            if (leftWasPressed) {
                triggerState = ReeTriggerState::LeftPressed;
            } else if (rightWasReleased) {
                triggerState = ReeTriggerState::Released;
            }
            break;
    }
}

static void UpdateTriggers() {
    auto* helper = Tick::GetVRPlatformHelper();
    if (!helper) return;

    bool isLeftTriggerDown = helper->GetTriggerValue(UnityEngine::XR::XRNode::LeftHand) > TriggerThreshold;
    bool isRightTriggerDown = helper->GetTriggerValue(UnityEngine::XR::XRNode::RightHand) > TriggerThreshold;

    bool leftWasPressed = !leftTriggerPressed && isLeftTriggerDown;
    bool leftWasReleased = leftTriggerPressed && !isLeftTriggerDown;
    bool rightWasPressed = !rightTriggerPressed && isRightTriggerDown;
    bool rightWasReleased = rightTriggerPressed && !isRightTriggerDown;

    leftTriggerPressed = isLeftTriggerDown;
    rightTriggerPressed = isRightTriggerDown;

    UpdateTriggerState(leftWasPressed, leftWasReleased, rightWasPressed, rightWasReleased);
}

void Update() {
    if (!PluginConfig::GetIsModPanelVisible()) return;
    for (auto& tracker : trackers) tracker.Update();
    UpdateTriggers();
}
}

#pragma endregion

#pragma region Tick

namespace Tick {
static GlobalNamespace::IVRPlatformHelper* vrPlatformHelper = nullptr;
static float saveTimer = 0.0f;
static constexpr float SaveInterval = 2.0f;

void SetVRPlatformHelper(GlobalNamespace::IVRPlatformHelper* helper) { vrPlatformHelper = helper; }

GlobalNamespace::IVRPlatformHelper* GetVRPlatformHelper() { return vrPlatformHelper; }

bool GetRawControllerPose(Hand hand, Vector3& position, Quaternion& rotation) {
    if (!vrPlatformHelper) return false;
    auto node = hand == Hand::Left ? UnityEngine::XR::XRNode::LeftHand : UnityEngine::XR::XRNode::RightHand;
    Vector3 nodePosition;
    Quaternion nodeRotation;
    if (!vrPlatformHelper->GetNodePose(node, 0, byref(nodePosition), byref(nodeRotation))) return false;
    TransformUtils::ApplyTrackingSpace(nodePosition, nodeRotation);
    position = nodePosition;
    rotation = nodeRotation;
    return true;
}

void Update() {
    float deltaTime = UnityEngine::Time::get_deltaTime();

    if (vrPlatformHelper) {
        auto leftPosition = Abomination::LeftControllerTransform.Position;
        auto leftRotation = Abomination::LeftControllerTransform.GetRotation();
        auto rightPosition = Abomination::RightControllerTransform.Position;
        auto rightRotation = Abomination::RightControllerTransform.GetRotation();

        // Keep the previous pose when tracking is lost, as PC does
        GetRawControllerPose(Hand::Left, leftPosition, leftRotation);
        GetRawControllerPose(Hand::Right, rightPosition, rightRotation);

        Vector3 headPosition;
        Quaternion headRotation;
        if (vrPlatformHelper->GetNodePose(UnityEngine::XR::XRNode::Head, 0, byref(headPosition), byref(headRotation))) {
            Abomination::HeadRotation = headRotation;
        }

        Abomination::UpdateTransforms(leftPosition, leftRotation, rightPosition, rightRotation, deltaTime);
    }

    InputManager::Update();

    saveTimer += deltaTime;
    if (saveTimer >= SaveInterval) {
        saveTimer = 0.0f;
        PluginConfig::SaveIfDirty();
    }
}
}

#pragma endregion
}

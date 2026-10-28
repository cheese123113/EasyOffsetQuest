#include "main.hpp"
#include "Abomination.hpp"
#include "Components.hpp"
#include "PluginConfig.hpp"

#include "GlobalNamespace/IVRPlatformHelper.hpp"
#include "GlobalNamespace/MainSettingsMenuViewControllersInstaller.hpp"
#include "GlobalNamespace/SettingsApplicatorSO.hpp"
#include "GlobalNamespace/SettingsManager.hpp"
#include "GlobalNamespace/VRController.hpp"
#include "GlobalNamespace/VRControllerTransformOffset.hpp"
#include "UnityEngine/Pose.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/XR/XRNode.hpp"
#include "Zenject/DiContainer.hpp"

using namespace GlobalNamespace;
using UnityEngine::XR::XRNode;

namespace EasyOffset {

static Vector3 const DefaultLeftPosition{-0.2f, 0.05f, 0.0f};
static Vector3 const DefaultRightPosition{0.2f, 0.05f, 0.0f};

// Base game offsets are replaced by EasyOffset's own (PC: VRControllerGetControllerOffsetPatch)
MAKE_HOOK_MATCH(
    VRController_TryGetControllerOffset,
    static_cast<bool (*)(IVRPlatformHelper*, VRControllerTransformOffset*, ByRef<XRNode>, ByRef<UnityEngine::Pose>)>(&VRController::TryGetControllerOffset),
    bool, IVRPlatformHelper* vrPlatformHelper, VRControllerTransformOffset* transformOffset, ByRef<XRNode> node, ByRef<UnityEngine::Pose> poseOffset
) {
    static bool loggedFirstCall = false;
    if (!loggedFirstCall) {
        loggedFirstCall = true;
        Logger.info("First TryGetControllerOffset call");
    }
    EnsureInitialized();
    if (!PluginConfig::GetEnabled()) {
        return VRController_TryGetControllerOffset(vrPlatformHelper, transformOffset, node, poseOffset);
    }
    *poseOffset = UnityEngine::Pose(Math::Zero(), Math::Identity());
    return false;
}

// PC: VRControllerUpdatePatch. The original still runs for its bookkeeping
// (pose validity, frame counter, anchor events), then the pose is replaced.
MAKE_HOOK_MATCH(VRController_Update, &VRController::Update, void, VRController* self) {
    VRController_Update(self);

    static bool loggedFirstCall = false;
    if (!loggedFirstCall) {
        loggedFirstCall = true;
        Logger.info("First VRController.Update call");
    }
    EnsureInitialized();
    if (!PluginConfig::GetEnabled() || self->_mouseMode) return;

    auto node = self->_node;
    Hand hand;
    if (node == XRNode::LeftHand) {
        hand = Hand::Left;
    } else if (node == XRNode::RightHand) {
        hand = Hand::Right;
    } else {
        return;
    }

    auto* vrPlatformHelper = self->_vrPlatformHelper;
    if (!vrPlatformHelper) return;

    Tick::SetVRPlatformHelper(vrPlatformHelper);
    Ticker::EnsureCreated();

    Vector3 position;
    Quaternion rotation;

    if (PluginConfig::SmoothingEnabled) {
        auto const& smoothed = Abomination::GetControllerTransform(hand);
        position = smoothed.Position;
        rotation = smoothed.GetRotation();
    } else {
        if (!vrPlatformHelper->GetNodePose(node, self->_nodeIdx, byref(position), byref(rotation))) {
            auto lastPosition = self->_lastTrackedPosition;
            auto lastRotation = self->_lastTrackedRotation;
            position = !Math::ApproxEquals(lastPosition, Math::Zero()) ? lastPosition : (hand == Hand::Left ? DefaultLeftPosition : DefaultRightPosition);
            rotation = !Math::ApproxEquals(lastRotation, Math::Identity()) ? lastRotation : Math::Identity();
        }
        TransformUtils::ApplyTrackingSpace(position, rotation);
    }

    // transform.Translate(translation) followed by transform.rotation *= saberRotation
    position = position + rotation * PluginConfig::GetSaberTranslation(hand);
    rotation = rotation * PluginConfig::GetSaberRotation(hand);

    auto transform = self->get_transform();
    transform->SetLocalPositionAndRotation(position, rotation);

    static bool loggedHand[2]{};
    if (!loggedHand[static_cast<int>(hand)]) {
        loggedHand[static_cast<int>(hand)] = true;
        bool separateAnchor = self->_viewAnchorTransform && self->_viewAnchorTransform.ptr() != transform.ptr();
        Logger.info("{} controller hooked: pos=({:.3f}, {:.3f}, {:.3f}) separate view anchor={}", HandName(hand), position.x, position.y, position.z, separateAnchor);
    }

    // The game moves its offset onto the view anchor; keep it neutral
    auto anchor = self->_viewAnchorTransform;
    if (anchor && anchor.ptr() != transform.ptr()) {
        anchor->SetLocalPositionAndRotation(Math::Zero(), Math::Identity());
    }
}

// PC: MenuInstallerPermanentPatch
MAKE_HOOK_MATCH(
    MainSettingsMenuViewControllersInstaller_InstallBindings,
    &MainSettingsMenuViewControllersInstaller::InstallBindings,
    void, MainSettingsMenuViewControllersInstaller* self
) {
    MainSettingsMenuViewControllersInstaller_InstallBindings(self);

    auto container = self->get_Container();
    if (!container) return;
    auto settingsManager = container->TryResolve<SettingsManager*>();
    auto settingsApplicator = container->TryResolve<SettingsApplicatorSO*>();
    PluginConfig::SetGameSettings(settingsManager, settingsApplicator);
    Logger.info("Game settings resolved: manager={}, applicator={}", fmt::ptr(settingsManager), fmt::ptr(settingsApplicator));
}

// Restore the base game offsets on every controller when the mod gets disabled
static void OnEnabledChanged(bool enabled) {
    if (enabled) return;
    auto controllers = UnityEngine::Resources::FindObjectsOfTypeAll<VRController*>();
    for (auto controller : controllers) {
        if (controller) controller->UpdateAnchorOffsetPose();
    }
}

void InstallHooks() {
    INSTALL_HOOK(Logger, VRController_TryGetControllerOffset);
    INSTALL_HOOK(Logger, VRController_Update);
    INSTALL_HOOK(Logger, MainSettingsMenuViewControllersInstaller_InstallBindings);
    PluginConfig::OnEnabledChange.Subscribe(OnEnabledChanged);
}
}

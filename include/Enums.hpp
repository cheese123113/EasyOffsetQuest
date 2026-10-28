#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace EasyOffset {

enum class Hand { Left, Right };

enum class AdjustmentMode {
    None,
    Basic,
    Position,
    Rotation,
    SwingBenchmark,
    Direct,
    PositionAuto,
    RotationAuto,
    RoomOffset
};

// Quest controllers only: no Index/Vive layouts on this platform
enum class ControllerButton {
    TriggerButton,
    PrimaryButton,
    SecondaryButton,
    GripButton,
    Primary2DAxisClick
};

enum class TrackingSpace {
    PcLink,
    QuestNative
};

inline std::string_view HandName(Hand hand) { return hand == Hand::Left ? "Left" : "Right"; }

inline constexpr std::array AllAdjustmentModes{
    AdjustmentMode::None,
    AdjustmentMode::Basic,
    AdjustmentMode::Position,
    AdjustmentMode::Rotation,
    AdjustmentMode::SwingBenchmark,
    AdjustmentMode::Direct,
    AdjustmentMode::PositionAuto,
    AdjustmentMode::RotationAuto,
    AdjustmentMode::RoomOffset
};

inline std::string_view AdjustmentModeName(AdjustmentMode mode) {
    switch (mode) {
        case AdjustmentMode::None: return "None";
        case AdjustmentMode::Basic: return "Basic";
        case AdjustmentMode::Position: return "Position";
        case AdjustmentMode::Rotation: return "Rotation";
        case AdjustmentMode::SwingBenchmark: return "Swing Benchmark";
        case AdjustmentMode::Direct: return "Direct";
        case AdjustmentMode::PositionAuto: return "Position Auto";
        case AdjustmentMode::RotationAuto: return "Rotation Auto";
        case AdjustmentMode::RoomOffset: return "Room offset";
    }
    return "None";
}

inline std::string_view AdjustmentModeHint(AdjustmentMode mode) {
    switch (mode) {
        case AdjustmentMode::None: return "Select an adjustment mode";
        case AdjustmentMode::Basic: return "Drag and drop adjustment mode\nHold the button and move your hand";
        case AdjustmentMode::Position: return "Pivot position only\nHold the button and move your hand";
        case AdjustmentMode::Rotation: return "Saber rotation only\nHold the button and move your hand\nRaise the free hand to zoom in";
        case AdjustmentMode::SwingBenchmark: return "Swing analysis tool\nHold the button and swing as you do in game";
        case AdjustmentMode::Direct: return "<color=#FF3333>For dialing exact values only!</color>\nTo find your settings use Rotation/Position modes";
        case AdjustmentMode::PositionAuto: return "Automatic position\nHold the button and rotate your wrist randomly";
        case AdjustmentMode::RotationAuto: return "Automatic rotation\nHold the button and rotate your wrist pointing in one direction";
        case AdjustmentMode::RoomOffset: return "World pulling locomotion\nHold the button and move your hand";
    }
    return "";
}

inline constexpr std::array AvailableButtons{
    ControllerButton::GripButton,
    ControllerButton::PrimaryButton,
    ControllerButton::SecondaryButton,
    ControllerButton::Primary2DAxisClick
};

// Config file names, identical to PC EasyOffset
inline std::string_view ControllerButtonName(ControllerButton button) {
    switch (button) {
        case ControllerButton::TriggerButton: return "TriggerButton";
        case ControllerButton::PrimaryButton: return "PrimaryButton";
        case ControllerButton::SecondaryButton: return "SecondaryButton";
        case ControllerButton::GripButton: return "GripButton";
        case ControllerButton::Primary2DAxisClick: return "Primary2DAxisClick";
    }
    return "SecondaryButton";
}

inline std::string_view ControllerButtonLabel(ControllerButton button) {
    switch (button) {
        case ControllerButton::TriggerButton: return "Trigger";
        case ControllerButton::PrimaryButton: return "X/A button";
        case ControllerButton::SecondaryButton: return "Y/B button";
        case ControllerButton::GripButton: return "Grip button";
        case ControllerButton::Primary2DAxisClick: return "Joystick click";
    }
    return "";
}

inline ControllerButton ControllerButtonFromName(std::string_view name) {
    for (auto button : AvailableButtons) {
        if (ControllerButtonName(button) == name) return button;
    }
    return ControllerButton::SecondaryButton;
}

// Minimal multicast event, the C++ stand-in for C# `event Action<...>`
template <typename... Args>
class Event {
public:
    using Callback = std::function<void(Args...)>;

    int Subscribe(Callback callback) {
        int id = nextId++;
        callbacks.emplace_back(id, std::move(callback));
        return id;
    }

    void Unsubscribe(int id) {
        std::erase_if(callbacks, [id](auto const& entry) { return entry.first == id; });
    }

    void Invoke(Args... args) {
        auto copy = callbacks;
        for (auto& [id, callback] : copy) callback(args...);
    }

private:
    int nextId = 1;
    std::vector<std::pair<int, Callback>> callbacks;
};
}

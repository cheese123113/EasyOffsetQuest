#include "UI.hpp"
#include "AdjustmentModes.hpp"
#include "Components.hpp"
#include "PluginConfig.hpp"
#include "Presets.hpp"
#include "main.hpp"

#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML/Components/CustomListTableData.hpp"
#include "bsml/shared/BSML/Components/Settings/DropdownListSetting.hpp"
#include "bsml/shared/BSML/Components/Settings/SliderSetting.hpp"
#include "bsml/shared/BSML/Components/Settings/ToggleSetting.hpp"

#include "HMUI/CurvedTextMeshPro.hpp"
#include "HMUI/InputFieldView.hpp"
#include "HMUI/TableView.hpp"
#include "HMUI/TextSegmentedControl.hpp"
#include "HMUI/ViewController.hpp"
#include "TMPro/FontStyles.hpp"
#include "TMPro/TextAlignmentOptions.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/ContentSizeFitter.hpp"
#include "UnityEngine/UI/HorizontalLayoutGroup.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "UnityEngine/UI/VerticalLayoutGroup.hpp"

#include <array>
#include <optional>

using namespace BSML::Lite;

namespace EasyOffset::UI {

static constexpr char const* TabName = "Easy Offset";

#pragma region Widgets

template <typename T>
using W = UnityW<T>;

struct SliderBinding {
    W<BSML::SliderSetting> slider;
    SliderValueType type;
    float displayScale;  // config units -> slider units
};

static W<HMUI::CurvedTextMeshPro> modeHintText;
static W<UnityEngine::GameObject> directContainer;
static W<UnityEngine::GameObject> referenceContainer;
static W<UnityEngine::GameObject> swingContainer;
static W<UnityEngine::GameObject> roomContainer;
static W<HMUI::CurvedTextMeshPro> swingText;
static W<HMUI::CurvedTextMeshPro> undoText;
static W<HMUI::CurvedTextMeshPro> presetStatusText;
static W<BSML::DropdownListSetting> modeDropdown;
static W<BSML::CustomListTableData> presetList;
static W<HMUI::InputFieldView> presetNameInput;
static std::vector<SliderBinding> directSliders;
static std::vector<Presets::StoredConfigPreset> storedPresets;
static std::optional<size_t> selectedPreset;
static Hand directHand = Hand::Left;
static bool updatingFromCode = false;

#pragma endregion

#pragma region Helpers

static W<UnityEngine::GameObject> MakeVertical(TransformWrapper parent) {
    auto layout = CreateVerticalLayoutGroup(parent);
    layout->set_spacing(1.0f);
    auto fitter = layout->get_gameObject()->GetComponent<UnityEngine::UI::ContentSizeFitter*>();
    if (fitter) fitter->set_verticalFit(UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize);
    return layout->get_gameObject();
}

static UnityEngine::UI::HorizontalLayoutGroup* MakeRow(TransformWrapper parent) {
    auto row = CreateHorizontalLayoutGroup(parent);
    row->set_spacing(1.0f);
    row->set_childForceExpandWidth(true);
    row->set_childControlWidth(true);
    return row;
}

static void SetPreferredHeight(UnityEngine::Component* component, float height) {
    auto element = component->get_gameObject()->GetComponent<UnityEngine::UI::LayoutElement*>();
    if (!element) element = component->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>();
    element->set_preferredHeight(height);
}

static HMUI::CurvedTextMeshPro* MakeText(TransformWrapper parent, std::string_view text, float fontSize = 3.2f) {
    auto result = CreateText(parent, text, TMPro::FontStyles::Normal, fontSize, {0, 0}, {0, 0});
    result->set_alignment(TMPro::TextAlignmentOptions::Center);
    result->set_enableWordWrapping(true);
    return result;
}

static std::string FormatPosition(float cm) { return fmt::format("{:.1f} cm", cm); }
static std::string FormatRotation(float degrees) { return fmt::format("{:.1f}°", degrees); }

#pragma endregion

#pragma region Updates

static void UpdateDirectSliders();

static void UpdateModeVisibility() {
    auto mode = PluginConfig::GetAdjustmentMode();
    if (modeHintText) modeHintText->set_text(std::string(AdjustmentModeHint(mode)));
    if (directContainer) directContainer->SetActive(mode == AdjustmentMode::Direct);
    if (swingContainer) swingContainer->SetActive(mode == AdjustmentMode::SwingBenchmark);
    if (roomContainer) roomContainer->SetActive(mode == AdjustmentMode::RoomOffset);
    UpdateDirectSliders();
}

static void UpdateDirectSliders() {
    if (PluginConfig::GetAdjustmentMode() != AdjustmentMode::Direct) return;
    if (referenceContainer) referenceContainer->SetActive(PluginConfig::GetSaberHasReference(directHand));
    updatingFromCode = true;
    for (auto& binding : directSliders) {
        if (!binding.slider) continue;
        binding.slider->set_Value(DirectMode::Get(directHand, binding.type).GetValue() * binding.displayScale);
    }
    updatingFromCode = false;
}

static void UpdateUndoText() {
    if (!undoText) return;
    std::string text;
    text += PluginConfig::UndoAvailable() ? "Undo: " + PluginConfig::UndoDescription() : "Nothing to undo";
    text += "    ";
    text += PluginConfig::RedoAvailable() ? "Redo: " + PluginConfig::RedoDescription() : "";
    undoText->set_text(text);
}

static void UpdateSwingText(SwingBenchmarkResult const& result) {
    if (!swingText) return;
    if (!result.HasResults) {
        swingText->set_text("Hold the button and swing as you do in game.\nRelease it to finish.");
        return;
    }

    auto hand = HandName(result.SelectedHand);
    float curve = result.SwingCurveAngle * Math::Rad2Deg;
    float swingRange = (result.MaximalSwingAngle - result.MinimalSwingAngle) * Math::Rad2Deg;
    std::string status = result.IsTracking
        ? "<color=#AAAAFF>Tracking...</color>"
        : (result.IsSwingGood ? "<color=#88FF88>Good swing</color>" : "<color=#FF8888>Swing wider (140° needed)</color>");

    swingText->set_text(fmt::format(
        "{} hand  {}\nSwing curve: {:.1f}° {}\nTip wobble: {:.1f} cm   Pivot wobble: {:.1f} cm\nSwing range: {:.0f}°",
        hand, status,
        std::abs(curve), curve >= 0 ? "outward" : "inward",
        result.TipDeviation * 100.0f, result.PivotDeviation * 100.0f,
        swingRange
    ));
}

static void SetPresetStatus(std::string_view text) {
    if (presetStatusText) presetStatusText->set_text(std::string(text));
}

static void RefreshPresets() {
    storedPresets = Presets::GetAllStoredPresets();
    selectedPreset.reset();
    if (!presetList) return;

    auto data = presetList->data;
    data->Clear();
    for (auto const& preset : storedPresets) {
        data->Add(BSML::CustomCellInfo::construct(Presets::GetPresetCellString(preset)));
    }
    presetList->tableView->ReloadData();
    presetList->tableView->ClearSelection();
}

#pragma endregion

#pragma region Sections

static void BuildModeSection(UnityEngine::Transform* parent) {
    std::vector<std::string_view> modeNames;
    for (auto mode : AllAdjustmentModes) modeNames.push_back(AdjustmentModeName(mode));

    modeDropdown = CreateDropdown(parent, "Adjustment mode", std::string(AdjustmentModeName(PluginConfig::GetAdjustmentMode())), modeNames, [](StringW value) {
        if (updatingFromCode) return;
        std::string name = value;
        for (auto mode : AllAdjustmentModes) {
            if (AdjustmentModeName(mode) == name) PluginConfig::SetAdjustmentMode(mode);
        }
    });

    modeHintText = MakeText(parent, "");
    SetPreferredHeight(modeHintText.ptr(), 10.0f);

    std::vector<std::string_view> buttonNames;
    for (auto button : AvailableButtons) buttonNames.push_back(ControllerButtonLabel(button));
    CreateDropdown(parent, "Assigned button", std::string(ControllerButtonLabel(PluginConfig::GetAssignedButton())), buttonNames, [](StringW value) {
        std::string label = value;
        for (auto button : AvailableButtons) {
            if (ControllerButtonLabel(button) == label) PluginConfig::SetAssignedButton(button);
        }
    });

    auto freeHandToggle = CreateToggle(parent, "Use free hand", PluginConfig::GetUseFreeHand(), [](bool value) { PluginConfig::SetUseFreeHand(value); });
    AddHoverHint(freeHandToggle, "On: press the button on one hand to adjust the other hand");
}

static void AddDirectSlider(UnityEngine::Transform* parent, std::string_view label, SliderValueType type, float min, float max, float increment, bool isPosition) {
    float scale = isPosition ? 100.0f : 1.0f;
    auto slider = CreateSliderSetting(parent, label, increment, 0.0f, min, max, 0.0f, true, {0, 0}, [type, scale](float value) {
        if (updatingFromCode) return;
        DirectMode::Get(directHand, type).SetValueFromUI(value / scale);
    });
    slider->formatter = isPosition ? [](float value) -> StringW { return FormatPosition(value); }
                                   : [](float value) -> StringW { return FormatRotation(value); };
    directSliders.push_back({slider, type, scale});
}

static void BuildDirectSection(UnityEngine::Transform* parent) {
    directContainer = MakeVertical(parent);
    auto container = directContainer->get_transform();

    std::array<std::string_view, 2> hands{"Left hand", "Right hand"};
    auto handSelector = CreateTextSegmentedControl(container, {0, 0}, {60, 6}, hands, [](int index) {
        directHand = index == 0 ? Hand::Left : Hand::Right;
        UpdateDirectSliders();
    });
    SetPreferredHeight(handSelector, 6.0f);

    directSliders.clear();
    // Ranges and steps from PC DirectConfigSliders
    AddDirectSlider(container, "Z Offset", SliderValueType::ZOffset, -20.0f, 25.0f, 0.1f, true);
    AddDirectSlider(container, "Pivot X", SliderValueType::PositionX, -15.0f, 15.0f, 0.1f, true);
    AddDirectSlider(container, "Pivot Y", SliderValueType::PositionY, -15.0f, 15.0f, 0.1f, true);
    AddDirectSlider(container, "Pivot Z", SliderValueType::PositionZ, -20.0f, 15.0f, 0.1f, true);
    AddDirectSlider(container, "Rotation X", SliderValueType::RotationX, -89.5f, 89.5f, 0.1f, false);
    AddDirectSlider(container, "Rotation Y", SliderValueType::RotationY, -180.0f, 180.0f, 0.1f, false);
    AddDirectSlider(container, "Rotation Z", SliderValueType::RotationZ, -180.0f, 180.0f, 0.1f, false);

    referenceContainer = MakeVertical(container);
    AddDirectSlider(referenceContainer->get_transform(), "Curve", SliderValueType::Curve, -40.0f, 40.0f, 0.1f, false);
    AddDirectSlider(referenceContainer->get_transform(), "Balance", SliderValueType::Balance, -40.0f, 40.0f, 0.1f, false);
}

static void BuildSwingSection(UnityEngine::Transform* parent) {
    swingContainer = MakeVertical(parent);
    auto container = swingContainer->get_transform();

    swingText = MakeText(container, "");
    SetPreferredHeight(swingText.ptr(), 16.0f);

    auto row = MakeRow(container);
    auto autoFix = CreateUIButton(row, "Auto-fix", []() { SwingBenchmark::AutoFix(); });
    AddHoverHint(autoFix, "Rotate the saber to remove the measured swing curve");
    auto setReference = CreateUIButton(row, "Set reference", []() {
        SwingBenchmark::SetAsReference();
        UpdateDirectSliders();
    });
    AddHoverHint(setReference, "Use this swing plane as the reference: unlocks Curve and Balance sliders in Direct mode");
    CreateUIButton(row, "Reset", []() { SwingBenchmark::Reset(); });

    UpdateSwingText(SwingBenchmark::GetResult());
}

static void BuildRoomSection(UnityEngine::Transform* parent) {
    roomContainer = MakeVertical(parent);
    auto container = roomContainer->get_transform();

    if (!PluginConfig::HasGameSettings()) {
        MakeText(container, "<color=#FF8888>Room settings are not available yet, reopen the menu</color>");
    }
    CreateToggle(container, "Allow X change", PluginConfig::GetAllowRoomXChange(), [](bool value) { PluginConfig::SetAllowRoomXChange(value); });
    CreateToggle(container, "Allow Y change", PluginConfig::GetAllowRoomYChange(), [](bool value) { PluginConfig::SetAllowRoomYChange(value); });
    CreateToggle(container, "Allow Z change", PluginConfig::GetAllowRoomZChange(), [](bool value) { PluginConfig::SetAllowRoomZChange(value); });
    CreateUIButton(container, "Reset room offset", []() {
        auto before = PluginConfig::GetBaseGameRoomCenter();
        Vector3 after = Math::Zero();
        PluginConfig::CreateUndoStep(
            "Reset Room Offset",
            [before]() { PluginConfig::SetBaseGameRoomCenter(before); },
            [after]() { PluginConfig::SetBaseGameRoomCenter(after); }
        );
        PluginConfig::SetBaseGameRoomCenter(after);
    });
}

static void BuildToolsSection(UnityEngine::Transform* parent) {
    MakeText(parent, "<color=#AAAAAA>Mirror / reset</color>", 3.0f);

    auto mirrorRow = MakeRow(parent);
    CreateUIButton(mirrorRow, "Left -> Right", []() {
        PluginConfig::CreateUndoStep("Mirror Left to Right");
        PluginConfig::Mirror(Hand::Left, true, true, true);
        PluginConfig::Save();
    });
    CreateUIButton(mirrorRow, "Right -> Left", []() {
        PluginConfig::CreateUndoStep("Mirror Right to Left");
        PluginConfig::Mirror(Hand::Right, true, true, true);
        PluginConfig::Save();
    });

    auto resetRow = MakeRow(parent);
    CreateUIButton(resetRow, "Reset Left", []() {
        PluginConfig::CreateUndoStep("Reset Left");
        PluginConfig::ResetOffsets(Hand::Left, true, true, true);
        PluginConfig::Save();
    });
    CreateUIButton(resetRow, "Reset Right", []() {
        PluginConfig::CreateUndoStep("Reset Right");
        PluginConfig::ResetOffsets(Hand::Right, true, true, true);
        PluginConfig::Save();
    });

    auto undoRow = MakeRow(parent);
    CreateUIButton(undoRow, "Undo", []() { PluginConfig::UndoLastChange(); });
    CreateUIButton(undoRow, "Redo", []() { PluginConfig::RedoLastChange(); });

    undoText = MakeText(parent, "", 2.8f);
    UpdateUndoText();
}

static void BuildPresetsSection(UnityEngine::Transform* parent) {
    MakeText(parent, "<color=#AAAAAA>Presets</color>", 3.0f);

    presetNameInput = CreateStringSetting(parent, "Preset name", "", nullptr);

    auto saveRow = MakeRow(parent);
    CreateUIButton(saveRow, "Save new", []() {
        std::string name = presetNameInput ? std::string(presetNameInput->get_text()) : "";
        if (name.empty()) name = fmt::format("Preset {}", PluginConfig::GeneratePreset().UnixTimestamp);
        bool success = Presets::SaveCurrentPreset(name);
        SetPresetStatus(success ? "Saved " + name : "<color=#FF8888>Save failed</color>");
        RefreshPresets();
    });
    CreateUIButton(saveRow, "Refresh", []() {
        RefreshPresets();
        SetPresetStatus(fmt::format("{} presets found", storedPresets.size()));
    });

    presetList = CreateList(parent, {0, 0}, {80, 30}, [](int index) {
        if (index < 0 || static_cast<size_t>(index) >= storedPresets.size()) return;
        selectedPreset = index;
        SetPresetStatus("Selected " + storedPresets[index].Name);
    });
    SetPreferredHeight(presetList.ptr(), 30.0f);

    auto loadRow = MakeRow(parent);
    CreateUIButton(loadRow, "Load", []() {
        if (!selectedPreset) return SetPresetStatus("Select a preset first");
        auto const& preset = storedPresets[*selectedPreset];
        if (preset.LoadFailed) return SetPresetStatus("<color=#FF8888>This preset could not be read</color>");
        PluginConfig::CreateUndoStep("Load " + preset.Name);
        PluginConfig::ApplyPreset(preset.Preset);
        PluginConfig::Save();
        SetPresetStatus("Loaded " + preset.Name);
    });
    CreateUIButton(loadRow, "Overwrite", []() {
        if (!selectedPreset) return SetPresetStatus("Select a preset first");
        auto name = storedPresets[*selectedPreset].Name;
        bool success = Presets::SaveCurrentPreset(name);
        SetPresetStatus(success ? "Overwrote " + name : "<color=#FF8888>Save failed</color>");
        RefreshPresets();
    });
    CreateUIButton(loadRow, "Delete", []() {
        if (!selectedPreset) return SetPresetStatus("Select a preset first");
        auto name = storedPresets[*selectedPreset].Name;
        Presets::DeletePreset(name);
        SetPresetStatus("Deleted " + name);
        RefreshPresets();
    });

    presetStatusText = MakeText(parent, "PC presets: copy .json files to\nModData/com.beatgames.beatsaber/Mods/easyoffset/Presets", 2.6f);
    RefreshPresets();
}

#pragma endregion

#pragma region Tab

static void BuildModPanel(UnityEngine::GameObject* tab) {
    tab->AddComponent<VisibilityTracker*>();

    auto container = CreateScrollableSettingsContainer(tab)->get_transform();

    BuildModeSection(container);
    BuildDirectSection(container);
    BuildSwingSection(container);
    BuildRoomSection(container);
    BuildToolsSection(container);
    BuildPresetsSection(container);

    UpdateModeVisibility();
    UpdateDirectSliders();
}

static void OnTabActivated(UnityEngine::GameObject* tab, bool firstActivation) {
    EnsureInitialized();
    if (firstActivation) BuildModPanel(tab);
    if (modeDropdown) {
        updatingFromCode = true;
        StringW modeName(std::string(AdjustmentModeName(PluginConfig::GetAdjustmentMode())));
        modeDropdown->set_Value(reinterpret_cast<System::Object*>(modeName.convert()));
        updatingFromCode = false;
    }
    UpdateModeVisibility();
    UpdateDirectSliders();
    UpdateUndoText();
}

#pragma endregion

#pragma region Settings menu

static void OnSettingsActivated(HMUI::ViewController* viewController, bool firstActivation, bool, bool) {
    EnsureInitialized();
    if (!firstActivation) return;
    auto container = CreateScrollableSettingsContainer(viewController)->get_transform();

    CreateToggle(container, "Enabled", PluginConfig::GetEnabled(), [](bool value) {
        PluginConfig::SetEnabled(value);
        PluginConfig::Save();
    });

    std::array<std::string_view, 2> spaces{"PC (Meta Link)", "Quest native"};
    auto current = PluginConfig::GetTrackingSpace() == TrackingSpace::PcLink ? spaces[0] : spaces[1];
    CreateDropdown(container, "Tracking space", std::string(current), spaces, [](StringW value) {
        PluginConfig::SetTrackingSpace(std::string(value) == "Quest native" ? TrackingSpace::QuestNative : TrackingSpace::PcLink);
        PluginConfig::Save();
    });

    MakeText(container,
        "PC (Meta Link): offsets mean the same as in PC EasyOffset played over Link,\n"
        "so PC presets can be loaded as they are.\n"
        "Quest native: offsets are relative to the raw Quest controller pose.\n\n"
        "The adjustment tools are in the \"Easy Offset\" tab next to the song list modifiers.",
        3.0f);
}

#pragma endregion

void Register() {
    PluginConfig::AdjustmentModeChangedEvent.Subscribe([](AdjustmentMode) { UpdateModeVisibility(); });
    PluginConfig::ConfigWasChangedEvent.Subscribe([]() { UpdateDirectSliders(); });
    PluginConfig::UndoAvailableChangedEvent.Subscribe([](bool, std::string) { UpdateUndoText(); });
    PluginConfig::RedoAvailableChangedEvent.Subscribe([](bool, std::string) { UpdateUndoText(); });
    SwingBenchmark::UpdatedEvent.Subscribe(UpdateSwingText);

    BSML::Register::RegisterGameplaySetupTab(TabName, OnTabActivated, BSML::MenuType::All);
    BSML::Register::RegisterSettingsMenu(TabName, OnSettingsActivated, false);
}
}

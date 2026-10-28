#include "Presets.hpp"
#include "BundledPresets.hpp"
#include "TransformUtils.hpp"
#include "main.hpp"

#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/prettywriter.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/stringbuffer.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace EasyOffset::Presets {

using Value = rapidjson::Value;

static constexpr float PositionUnitScale = 0.01f;

std::string GetPresetsFolderPath() {
    return getDataDir(GetModInfo()) + "Presets/";
}

static void CreateDirectoryIfNecessary() {
    std::error_code error;
    std::filesystem::create_directories(GetPresetsFolderPath(), error);
}

#pragma region Json helpers

// Newtonsoft lookups in PC EasyOffset are case-insensitive
static Value const& GetValueUnsafe(Value const& obj, std::string_view key) {
    for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it) {
        std::string_view name(it->name.GetString(), it->name.GetStringLength());
        if (name.size() != key.size()) continue;
        bool match = std::equal(name.begin(), name.end(), key.begin(), [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        });
        if (match) return it->value;
    }
    throw std::runtime_error("Missing key: " + std::string(key));
}

static float GetFloat(Value const& obj, std::string_view key) {
    auto const& value = GetValueUnsafe(obj, key);
    if (!value.IsNumber()) throw std::runtime_error("Not a number: " + std::string(key));
    return value.GetFloat();
}

static int64_t GetInt64(Value const& obj, std::string_view key) {
    auto const& value = GetValueUnsafe(obj, key);
    if (value.IsInt64()) return value.GetInt64();
    if (value.IsNumber()) return static_cast<int64_t>(value.GetDouble());
    throw std::runtime_error("Not an integer: " + std::string(key));
}

static std::string GetString(Value const& obj, std::string_view key) {
    auto const& value = GetValueUnsafe(obj, key);
    if (!value.IsString()) throw std::runtime_error("Not a string: " + std::string(key));
    return value.GetString();
}

static bool GetBool(Value const& obj, std::string_view key) {
    auto const& value = GetValueUnsafe(obj, key);
    if (!value.IsBool()) throw std::runtime_error("Not a bool: " + std::string(key));
    return value.GetBool();
}

static Vector3 ParseVector(Value const& obj, std::string_view key) {
    auto const& tmp = GetValueUnsafe(obj, key);
    return {GetFloat(tmp, "x"), GetFloat(tmp, "y"), GetFloat(tmp, "z")};
}

static Quaternion ParseQuaternion(Value const& obj, std::string_view key) {
    auto const& tmp = GetValueUnsafe(obj, key);
    return {GetFloat(tmp, "x"), GetFloat(tmp, "y"), GetFloat(tmp, "z"), GetFloat(tmp, "w")};
}

static Vector3 ParseFlatVector(Value const& obj, std::string const& key) {
    return {GetFloat(obj, key + "X"), GetFloat(obj, key + "Y"), GetFloat(obj, key + "Z")};
}

#pragma endregion

#pragma region Read

static ConfigPreset DeserializeV2(Value const& obj) {
    ConfigPreset preset;
    preset.Version = "2.0";
    preset.UnixTimestamp = GetInt64(obj, "unixTimestamp");
    preset.ControllerType = GetString(obj, "controllerType");

    preset.LeftSaberPivotPosition = ParseVector(obj, "LeftSaberPivotPosition") * PositionUnitScale;
    preset.LeftSaberRotation = TransformUtils::RotationFromEuler(ParseVector(obj, "LeftSaberRotationEuler"));
    preset.LeftSaberZOffset = GetFloat(obj, "LeftSaberZOffset") * PositionUnitScale;

    preset.RightSaberPivotPosition = ParseVector(obj, "RightSaberPivotPosition") * PositionUnitScale;
    preset.RightSaberRotation = TransformUtils::RotationFromEuler(ParseVector(obj, "RightSaberRotationEuler"));
    preset.RightSaberZOffset = GetFloat(obj, "RightSaberZOffset") * PositionUnitScale;

    try {
        preset.LeftSaberHasReference = GetBool(obj, "LeftSaberHasReference");
        preset.LeftSaberReferenceRotation = ParseQuaternion(obj, "LeftSaberReference");
    } catch (std::exception const&) {
        preset.LeftSaberHasReference = false;
        preset.LeftSaberReferenceRotation = Math::Identity();
    }

    try {
        preset.RightSaberHasReference = GetBool(obj, "RightSaberHasReference");
        preset.RightSaberReferenceRotation = ParseQuaternion(obj, "RightSaberReference");
    } catch (std::exception const&) {
        preset.RightSaberHasReference = false;
        preset.RightSaberReferenceRotation = Math::Identity();
    }

    return preset;
}

static ConfigPreset DeserializeV1(Value const& obj) {
    ConfigPreset preset;
    preset.Version = "1.0";
    preset.UnixTimestamp = GetInt64(obj, "unixTimestamp");
    preset.ControllerType = GetString(obj, "controllerType");

    preset.LeftSaberPivotPosition = ParseFlatVector(obj, "leftHandPivotPosition");
    preset.LeftSaberRotation = TransformUtils::RotationFromDirection(ParseFlatVector(obj, "leftHandSaberDirection"));
    preset.LeftSaberZOffset = GetFloat(obj, "leftHandZOffset");
    preset.LeftSaberHasReference = false;
    preset.LeftSaberReferenceRotation = Math::Identity();

    preset.RightSaberPivotPosition = ParseFlatVector(obj, "rightHandPivotPosition");
    preset.RightSaberRotation = TransformUtils::RotationFromDirection(ParseFlatVector(obj, "rightHandSaberDirection"));
    preset.RightSaberZOffset = GetFloat(obj, "rightHandZOffset");
    preset.RightSaberHasReference = false;
    preset.RightSaberReferenceRotation = Math::Identity();
    return preset;
}

std::optional<ConfigPreset> ParsePresetJson(std::string json) {
    // PC writes UTF-8 with a BOM
    if (json.starts_with("\xEF\xBB\xBF")) json.erase(0, 3);

    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError() || !doc.IsObject()) return std::nullopt;

    std::string version = GetString(doc, "version");
    if (version == "2.0") return DeserializeV2(doc);
    if (version == "1.0") return DeserializeV1(doc);
    return std::nullopt;
}

std::optional<ConfigPreset> ReadPresetFromFile(std::string const& filePath) {
    try {
        return ParsePresetJson(readfile(filePath));
    } catch (std::exception const& e) {
        Logger.warn("Failed to read preset {}: {}", filePath, e.what());
        return std::nullopt;
    }
}

#pragma endregion

#pragma region Write

bool WritePresetToFile(std::string const& filePath, ConfigPreset const& preset) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

    auto vector = [&](Vector3 v) {
        Value obj(rapidjson::kObjectType);
        obj.AddMember("x", v.x, allocator);
        obj.AddMember("y", v.y, allocator);
        obj.AddMember("z", v.z, allocator);
        return obj;
    };

    auto quaternion = [&](Quaternion q) {
        Value obj(rapidjson::kObjectType);
        obj.AddMember("x", q.x, allocator);
        obj.AddMember("y", q.y, allocator);
        obj.AddMember("z", q.z, allocator);
        obj.AddMember("w", q.w, allocator);
        return obj;
    };

    doc.AddMember("Version", "2.0", allocator);
    doc.AddMember("UnixTimestamp", preset.UnixTimestamp, allocator);
    doc.AddMember("ControllerType", Value(preset.ControllerType, allocator), allocator);

    doc.AddMember("LeftSaberPivotPosition", vector(preset.LeftSaberPivotPosition / PositionUnitScale), allocator);
    doc.AddMember("LeftSaberRotationEuler", vector(TransformUtils::EulerFromRotation(preset.LeftSaberRotation)), allocator);
    doc.AddMember("LeftSaberZOffset", preset.LeftSaberZOffset / PositionUnitScale, allocator);
    doc.AddMember("LeftSaberHasReference", preset.LeftSaberHasReference, allocator);
    doc.AddMember("LeftSaberReference", quaternion(preset.LeftSaberReferenceRotation), allocator);

    doc.AddMember("RightSaberPivotPosition", vector(preset.RightSaberPivotPosition / PositionUnitScale), allocator);
    doc.AddMember("RightSaberRotationEuler", vector(TransformUtils::EulerFromRotation(preset.RightSaberRotation)), allocator);
    doc.AddMember("RightSaberZOffset", preset.RightSaberZOffset / PositionUnitScale, allocator);
    doc.AddMember("RightSaberHasReference", preset.RightSaberHasReference, allocator);
    doc.AddMember("RightSaberReference", quaternion(preset.RightSaberReferenceRotation), allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return writefile(filePath, buffer.GetString());
}

#pragma endregion

#pragma region Storage

bool SaveCurrentPreset(std::string const& fileName) {
    CreateDirectoryIfNecessary();
    return WritePresetToFile(GetPresetsFolderPath() + fileName + ".json", PluginConfig::GeneratePreset());
}

bool LoadPreset(std::string const& fileName) {
    auto preset = ReadPresetFromFile(GetPresetsFolderPath() + fileName + ".json");
    if (!preset) return false;
    PluginConfig::ApplyPreset(*preset);
    return true;
}

bool DeletePreset(std::string const& fileName) {
    std::error_code error;
    return std::filesystem::remove(GetPresetsFolderPath() + fileName + ".json", error);
}

std::vector<StoredConfigPreset> GetAllStoredPresets() {
    CreateDirectoryIfNecessary();

    std::vector<StoredConfigPreset> allPresets;
    std::error_code error;

    for (auto const& entry : std::filesystem::directory_iterator(GetPresetsFolderPath(), error)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        auto path = entry.path().string();
        auto preset = ReadPresetFromFile(path);
        allPresets.push_back({path, entry.path().stem().string(), !preset.has_value(), preset.value_or(ConfigPreset{})});
    }

    // Newest first, broken files last (same order as PC)
    std::sort(allPresets.begin(), allPresets.end(), [](auto const& a, auto const& b) {
        if (a.LoadFailed != b.LoadFailed) return !a.LoadFailed;
        return a.Preset.UnixTimestamp > b.Preset.UnixTimestamp;
    });
    return allPresets;
}

#pragma endregion

#pragma region Bundled presets

void InstallBundledPresets() {
    CreateDirectoryIfNecessary();

    for (auto const& bundled : BundledPresets) {
        auto path = GetPresetsFolderPath() + std::string(bundled.Name) + ".json";
        if (!fileexists(path)) writefile(path, bundled.Json);
    }

    if (PluginConfig::GetBundledPresetApplied()) return;

    try {
        auto preset = ParsePresetJson(std::string(BundledPresets[0].Json));
        if (!preset) throw std::runtime_error("bundled preset did not parse");
        PluginConfig::ApplyPreset(*preset);
        Logger.info("Applied bundled preset {}", BundledPresets[0].Name);
    } catch (std::exception const& e) {
        Logger.error("Failed to apply bundled preset: {}", e.what());
    }

    PluginConfig::SetBundledPresetApplied(true);
    PluginConfig::Save();
}

#pragma endregion

#pragma region GetPresetCellString

static constexpr int64_t SecondsInMinute = 60;
static constexpr int64_t SecondsInHour = 60 * SecondsInMinute;
static constexpr int64_t SecondsInDay = 24 * SecondsInHour;
static constexpr int64_t VeryOldTimeDifference = 365 * SecondsInDay;

static std::string GetTimeString(int64_t unixTimestamp) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto timeDifference = now - unixTimestamp;

    if (timeDifference < 0) return "o_O";
    if (timeDifference > VeryOldTimeDifference) return "-";

    if (auto fullDays = timeDifference / SecondsInDay; fullDays > 0) return fmt::format("{} d ago", fullDays);
    if (auto fullHours = timeDifference / SecondsInHour; fullHours > 0) return fmt::format("{} h ago", fullHours);
    auto fullMinutes = timeDifference / SecondsInMinute;
    return fullMinutes > 0 ? fmt::format("{} m ago", fullMinutes) : fmt::format("{} s ago", timeDifference);
}

std::string GetPresetCellString(StoredConfigPreset const& preset) {
    std::string result = "<align=\"left\">";
    if (preset.LoadFailed) {
        result += "<color=red>load error";
    } else {
        result += GetTimeString(preset.Preset.UnixTimestamp);
    }
    result += "<pos=30%>";
    result += preset.Name;
    return result;
}

#pragma endregion
}

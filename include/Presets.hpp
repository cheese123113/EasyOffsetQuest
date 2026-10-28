#pragma once

#include "PluginConfig.hpp"

#include <optional>
#include <string>
#include <vector>

namespace EasyOffset::Presets {

struct StoredConfigPreset {
    std::string AbsoluteFilePath;
    std::string Name;
    bool LoadFailed;
    ConfigPreset Preset;
};

std::string GetPresetsFolderPath();

bool SaveCurrentPreset(std::string const& fileName);
bool LoadPreset(std::string const& fileName);
bool DeletePreset(std::string const& fileName);
std::vector<StoredConfigPreset> GetAllStoredPresets();

std::optional<ConfigPreset> ParsePresetJson(std::string json);
std::optional<ConfigPreset> ReadPresetFromFile(std::string const& filePath);

// Copies the presets shipped inside the mod into the presets folder and applies the first one on first launch
void InstallBundledPresets();
bool WritePresetToFile(std::string const& filePath, ConfigPreset const& preset);

std::string GetPresetCellString(StoredConfigPreset const& preset);
}

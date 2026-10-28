#pragma once

#include <array>
#include <string_view>

namespace EasyOffset {

struct BundledPreset {
    std::string_view Name;
    std::string_view Json;
};

// PC preset exported from UserData/EasyOffset/Presets/main.json; the first entry is applied on first launch
inline constexpr std::array BundledPresets{
    BundledPreset{"main", R"json({
  "Version": "2.0",
  "UnixTimestamp": 1791523116,
  "ControllerType": "Quest 2",
  "LeftSaberPivotPosition": {
    "x": -1.48692954,
    "y": 1.10690534,
    "z": -4.42550659
  },
  "LeftSaberRotationEuler": {
    "x": 20.5107174,
    "y": 4.122686,
    "z": 2.76797819
  },
  "LeftSaberZOffset": 15.5,
  "LeftSaberHasReference": true,
  "LeftSaberReference": {
    "x": -0.116196938,
    "y": -0.139307961,
    "z": 0.6227426,
    "w": -0.76110667
  },
  "RightSaberPivotPosition": {
    "x": 1.48692954,
    "y": 1.10690534,
    "z": -4.42550659
  },
  "RightSaberRotationEuler": {
    "x": 33.0766449,
    "y": -5.60168457,
    "z": -3.35693359
  },
  "RightSaberZOffset": 15.5,
  "RightSaberHasReference": true,
  "RightSaberReference": {
    "x": 0.166714758,
    "y": -0.235024139,
    "z": 0.690296054,
    "w": 0.6636725
  }
}
)json"},
};
}

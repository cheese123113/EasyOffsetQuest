#pragma once

#include "custom-types/shared/macros.hpp"
#include "UnityEngine/MonoBehaviour.hpp"

// Drives EasyOffset's per-frame logic, survives scene changes
DECLARE_CLASS_CODEGEN(EasyOffset, Ticker, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_METHOD(void, Update);

public:
    static void EnsureCreated();
};

// Reports whether the mod panel is on screen (PC: VisibilityTracker)
DECLARE_CLASS_CODEGEN(EasyOffset, VisibilityTracker, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_METHOD(void, OnEnable);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
};

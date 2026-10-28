#include "Components.hpp"
#include "Abomination.hpp"
#include "PluginConfig.hpp"

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Object.hpp"

DEFINE_TYPE(EasyOffset, Ticker);
DEFINE_TYPE(EasyOffset, VisibilityTracker);

namespace EasyOffset {

static UnityW<Ticker> tickerInstance;

void Ticker::EnsureCreated() {
    if (tickerInstance) return;
    auto gameObject = UnityEngine::GameObject::New_ctor("EasyOffsetTicker");
    UnityEngine::Object::DontDestroyOnLoad(gameObject);
    tickerInstance = gameObject->AddComponent<Ticker*>();
}

void Ticker::Update() {
    Tick::Update();
}

void VisibilityTracker::OnEnable() {
    PluginConfig::SetIsModPanelVisible(true);
}

void VisibilityTracker::OnDisable() {
    PluginConfig::SetIsModPanelVisible(false);
}
}

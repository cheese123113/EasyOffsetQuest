#include "main.hpp"
#include "AdjustmentModes.hpp"
#include "PluginConfig.hpp"
#include "Presets.hpp"
#include "UI.hpp"

#include "custom-types/shared/register.hpp"

static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

namespace EasyOffset {
modloader::ModInfo const& GetModInfo() { return modInfo; }

// Loading converts stored euler angles through Unity, so it waits for the first frame
void EnsureInitialized() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    Logger.info("Init: loading config");
    PluginConfig::Load();
    Logger.info("Init: installing bundled presets");
    Presets::InstallBundledPresets();
    Logger.info("Init: creating adjustment modes");
    AdjustmentModes::Initialize();
    Logger.info("Config loaded");
}
}

MOD_EXTERN_FUNC void setup(CModInfo* info) noexcept {
    *info = modInfo.to_c();
    Paper::Logger::RegisterFileContextId(EasyOffset::Logger.tag);
    EasyOffset::Logger.info("Completed setup!");
}

MOD_EXTERN_FUNC void late_load() noexcept {
    EasyOffset::Logger.info("late_load: il2cpp init");
    il2cpp_functions::Init();
    EasyOffset::Logger.info("late_load: registering custom types");
    custom_types::Register::AutoRegister();

    EasyOffset::Logger.info("late_load: registering UI");
    EasyOffset::UI::Register();

    EasyOffset::Logger.info("Installing hooks...");
    EasyOffset::InstallHooks();
    EasyOffset::Logger.info("Installed all hooks!");
}

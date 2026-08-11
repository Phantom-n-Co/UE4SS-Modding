#include <Mod/CppUserModBase.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <Unreal/Hooks/Hooks.hpp>
#include <Unreal/UnrealInitializer.hpp>

#include "config.hpp"
#include "RandomTweaks.hpp"

#include <filesystem>
#include <string>

extern "C" IMAGE_DOS_HEADER __ImageBase;

static std::wstring getModDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW((HMODULE)&__ImageBase, buf, MAX_PATH);
    std::wstring path(buf, n);
    auto pos = path.find_last_of(L"\\/");
    path = path.substr(0, pos); // strip RandomTweaks.dll
    pos = path.find_last_of(L"\\/");
    path = path.substr(0, pos); // strip dlls -> mod dir
    return path;
}

// Runs each engine tick until the player class resolves (like the Lua mod's
// ExecuteInGameThread + retry, but event-driven with no fixed attempt cap).
static void initOnTick(Unreal::Hook::TCallbackIterationData<void>&,
                       Unreal::UEngine*, float, bool) {
    if (!Unreal::IsGameThreadInitialized()) {
        return; // not on the game thread yet; retry next tick
    }
    static std::wstring configPath;
    static TweakConfig config;
    static bool configLoaded = false;
    if (!configLoaded) {
        configLoaded = true;
        std::wstring modDir = getModDir();
        configPath = (std::filesystem::path(modDir) / L"config.lua").wstring();
        config = loadConfig(configPath);
        RC::Output::send<RC::LogLevel::Verbose>(
            STR("[RandomTweaks] Loaded config from {}\n"), configPath.c_str());
    }
    if (tryInit(config)) {
        return;
    }
}

class RandomTweaks : public RC::CppUserModBase {
public:
    RandomTweaks() : CppUserModBase() {
        ModName = STR("RandomTweaks");
        ModVersion = STR("1.0");
        ModDescription = STR("Random tweaks");
        ModAuthors = STR("WhitePhantom02");
    }

    ~RandomTweaks() override {}

    auto on_update() -> void override {}

    auto on_unreal_init() -> void override {
        Unreal::Hook::RegisterEngineTickPreCallback(
            [](Unreal::Hook::TCallbackIterationData<void>& info,
               Unreal::UEngine* engine, float delta, bool idle) {
                initOnTick(info, engine, delta, idle);
            },
            {false, false, STR("RandomTweaks"), STR("InitOnTick")});
    }
};

#define RANDOM_TWEAKS_API __declspec(dllexport)

extern "C" {
RANDOM_TWEAKS_API RC::CppUserModBase* start_mod() { return new RandomTweaks(); }

RANDOM_TWEAKS_API void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}

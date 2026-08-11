#include <Mod/CppUserModBase.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <Unreal/Hooks/Hooks.hpp>
#include <Unreal/UnrealInitializer.hpp>

#include "config.hpp"
#include "StackAnything.hpp"

#include <limits>
#include <filesystem>
#include <string>

extern "C" IMAGE_DOS_HEADER __ImageBase;

static std::wstring getModDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW((HMODULE)&__ImageBase, buf, MAX_PATH);
    std::wstring path(buf, n);
    auto pos = path.find_last_of(L"\\/");
    path = path.substr(0, pos);                  // strip StackAnything.dll
    pos = path.find_last_of(L"\\/");
    path = path.substr(0, pos);                  // strip dlls -> mod dir
    return path;
}

// Runs once on the first engine tick, when the game thread ID is initialized.
// on_unreal_init fires before the first UGameEngine::Tick, so any call that
// asserts IsInGameThread (e.g. asset loading in applyStackTweaks) would crash
// there. This mirrors the Lua mod's ExecuteInGameThread deferral.
static void initOnFirstTick(Unreal::Hook::TCallbackIterationData<void>&,
                            Unreal::UEngine*, float, bool) {
    if (!Unreal::IsGameThreadInitialized()) {
        return; // not on the game thread yet; retry next tick
    }
    static bool s_initialized = false;
    if (s_initialized) {
        return;
    }
    s_initialized = true;

    std::wstring modDir = getModDir();
    std::wstring configPath = (std::filesystem::path(modDir) / L"config.lua").wstring();

    auto config = loadConfig(configPath, kDefaultStackSizes);
    RC::Output::send<RC::LogLevel::Verbose>(STR("[StackAnything] Loaded {} stack patterns from {}\n"),
                                            config.size(), configPath.c_str());

    applyStackTweaks(config);

    int hooked = registerHooks();
    RC::Output::send<RC::LogLevel::Verbose>(STR("[StackAnything] Registered {} hooks\n"), hooked);
}

class StackAnything : public RC::CppUserModBase {
public:
    StackAnything() : CppUserModBase() {
        ModName = STR("StackAnything");
        ModVersion = STR("3.0");
        ModDescription = STR("Stack anything");
        ModAuthors = STR("WhitePhantom02");
    }

    ~StackAnything() override {}

    auto on_update() -> void override {}

    auto on_unreal_init() -> void override {
        Unreal::Hook::RegisterEngineTickPreCallback(
            [](Unreal::Hook::TCallbackIterationData<void>& info,
               Unreal::UEngine* engine, float delta, bool idle) {
                initOnFirstTick(info, engine, delta, idle);
            },
            {false, false, STR("StackAnything"), STR("InitOnFirstTick")});
    }
};

#define STACK_ANYTHING_API __declspec(dllexport)

extern "C" {
STACK_ANYTHING_API RC::CppUserModBase* start_mod() { return new StackAnything(); }

STACK_ANYTHING_API void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}

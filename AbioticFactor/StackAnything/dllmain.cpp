#include <Mod/CppUserModBase.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

// Unreal's wrapper: raw <Windows.h> macros (InterlockedCompareExchangePointer, MemoryBarrier) break later Unreal headers under clang-cl
#include <Unreal/Core/Windows/WindowsHWrapper.hpp>

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
    const DWORD n = GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), buf, MAX_PATH);
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

    const std::wstring modDir = getModDir();
    const std::wstring configPath = (std::filesystem::path(modDir) / L"config.lua").wstring();

    const auto config = loadConfig(configPath, kDefaultStackSizes);
    RC::Output::send<RC::LogLevel::Verbose>(STR("[StackAnything] Loaded {} stack patterns from {}\n"),
                                            config.size(), configPath.c_str());

    initGameLayout();
    const bool dumpItems = loadDumpItemsFlag(configPath);
    const std::wstring dumpPath =
        dumpItems ? (std::filesystem::path(modDir) / L"item_dump.lua").wstring() : std::wstring{};
    applyStackTweaks(config, dumpPath);

    const int hooked = registerHooks();
    RC::Output::send<RC::LogLevel::Verbose>(STR("[StackAnything] Registered {} hooks\n"), hooked);
}

class StackAnything : public RC::CppUserModBase {
public:
    StackAnything() {
        ModName = STR("StackAnything");
        ModVersion = STR("3.1.1");
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

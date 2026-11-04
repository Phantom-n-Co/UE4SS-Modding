#include <Mod/CppUserModBase.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

#include "AFTestBridge.hpp"

class AFTestBridgeMod : public RC::CppUserModBase {
public:
    AFTestBridgeMod() : CppUserModBase() {
        ModName = STR("AFTestBridge");
        ModVersion = STR("0.1.0");
        ModDescription = STR("Dev-only loopback command bridge for automated tests");
        ModAuthors = STR("WhitePhantom02");
    }

    ~AFTestBridgeMod() override { AFTestBridge::stop(); }

    auto on_update() -> void override {}

    auto on_unreal_init() -> void override { AFTestBridge::start(); }
};

#define AF_TEST_BRIDGE_API __declspec(dllexport)

extern "C" {
AF_TEST_BRIDGE_API RC::CppUserModBase* start_mod() { return new AFTestBridgeMod(); }

AF_TEST_BRIDGE_API void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}

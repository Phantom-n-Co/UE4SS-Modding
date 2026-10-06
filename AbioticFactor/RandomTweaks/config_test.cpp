#include "config.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>

static void test_parse_full_config() {
    std::wstring text = L"-- RandomTweaks config\n"
                        L"return {\n"
                        L"  instantButcher = true,\n"
                        L"  debugLogging = false,\n"
                        L"  wishingShelfTopUp = false,\n"
                        L"  transmogHideSuitAndBackpack = false,\n"
                        L"  player = {\n"
                        L"    walkSpeed = 550,\n"
                        L"    sprintSpeed = 822.0,  -- trailing comment\n"
                        L"    staminaDrainRate = 0.25,\n"
                        L"    jumpHeight = nil,\n"
                        L"    disableWatchLight = true,\n"
                        L"  },\n"
                        L"}\n";
    auto cfg = parseConfig(text);
    assert(cfg.instantButcher);
    assert(!cfg.debugLogging);
    assert(!cfg.wishingShelfTopUp);
    assert(!cfg.transmogHideSuitAndBackpack);
    assert(cfg.walkSpeed.has_value() && *cfg.walkSpeed == 550.0);
    assert(cfg.sprintSpeed.has_value() && *cfg.sprintSpeed == 822.0);
    assert(cfg.staminaDrainRate.has_value() && *cfg.staminaDrainRate == 0.25);
    assert(!cfg.jumpHeight.has_value()); // nil = skip
    assert(cfg.disableWatchLight);
    assert(!cfg.lanternIntensity.has_value()); // untouched by default
}

static void test_parse_missing_returns_all_skipped() {
    auto cfg = parseConfig(L"");
    assert(!cfg.instantButcher);
    assert(!cfg.walkSpeed.has_value());
    assert(!cfg.disableWatchLight);
    assert(cfg.wishingShelfTopUp); // default on when line missing
    assert(cfg.transmogHideSuitAndBackpack); // default on when line missing
}

static void test_defaults_match_shipped_lua() {
    const TweakConfig& d = kDefaultTweaks;
    assert(d.instantButcher);
    assert(!d.debugLogging);
    assert(*d.walkSpeed == 550.0);
    assert(*d.sprintSpeed == 825.0);
    assert(*d.staminaDrainRate == 0.25);
    assert(*d.staminaRegainRate == 0.8);
    assert(*d.interactionRange == 320.0);
    assert(*d.ladderClimbSpeed == 350.0);
    assert(*d.jumpHeight == 500.0);
    assert(*d.lightIntensity == 8.4);
    assert(*d.lanternIntensity == 5.45);
    assert(!d.disableWatchLight);
    assert(d.wishingShelfTopUp);
    assert(d.transmogHideSuitAndBackpack);
}

static void test_write_and_reload() {
    const std::wstring path = L"randomtweaks_config_test_tmp.lua";
    assert(writeDefaultConfig(path));
    auto cfg = loadConfig(path);
    assert(cfg.instantButcher);
    assert(*cfg.walkSpeed == 550.0);
    assert(*cfg.lanternIntensity == 5.45);
    assert(cfg.transmogHideSuitAndBackpack);
    std::remove("randomtweaks_config_test_tmp.lua");
}

static void test_load_fallback_on_unwritable_path() {
    auto cfg = loadConfig(L"Z:\\nonexistent\\dir\\config.lua");
    assert(cfg.instantButcher);
    assert(*cfg.walkSpeed == 550.0);
}

static void test_parse_bad_numeric_value_is_skipped() {
    std::wstring text = L"return {\n  player = {\n    walkSpeed = garbage,\n  },\n}\n";
    auto cfg = parseConfig(text);
    assert(!cfg.walkSpeed.has_value());
}

int main() {
    test_parse_full_config();
    test_parse_missing_returns_all_skipped();
    test_defaults_match_shipped_lua();
    test_write_and_reload();
    test_load_fallback_on_unwritable_path();
    test_parse_bad_numeric_value_is_skipped();
    std::printf("RandomTweaksConfigTest: ALL PASS\n");
    return 0;
}

#include "config.hpp"
#include "teleporter_names.hpp"
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
                        L"  nameableTeleporters = false,\n"
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
    assert(!cfg.nameableTeleporters);
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
    assert(cfg.nameableTeleporters); // default on when line missing
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
    assert(d.nameableTeleporters);
}

static void test_write_and_reload() {
    const std::wstring path = L"randomtweaks_config_test_tmp.lua";
    assert(writeDefaultConfig(path));
    auto cfg = loadConfig(path);
    assert(cfg.instantButcher);
    assert(*cfg.walkSpeed == 550.0);
    assert(*cfg.lanternIntensity == 5.45);
    assert(cfg.transmogHideSuitAndBackpack);
    assert(cfg.nameableTeleporters);
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

static void test_teleporter_names() {
    using namespace teleporter_names;
    assert(normalizeName(L"\tMain Base \t ") == L"Main Base"); // tabs are control chars
    assert(normalizeName(L"  Main Base  ") == L"Main Base");
    assert(normalizeName(L"a\x01" L"b") == L"ab");
    assert(normalizeName(L"   ").empty());
    assert(normalizeName(std::wstring(40, L'x')).size() == kMaxNameLength);
    assert(normalizeName(std::wstring(31, L'x') + L"  y") == std::wstring(31, L'x'));

    // case-insensitive, stable, never a tag id (0..25)
    assert(nameToId(L"Main Base") == nameToId(L"MAIN BASE"));
    assert(nameToId(L"Main Base") != nameToId(L"Main Base 2"));
    assert(isCustomId(nameToId(L"")) && isCustomId(nameToId(L"x")));
    assert(!isCustomId(25) && !isCustomId(0) && !isCustomId(-1));
    assert(nameToId(L"a") == 1000 + static_cast<int32_t>(((2166136261u ^ 0x61u) * 16777619u * 16777619u) & 0x3fffffffu));

    // payload round trip; vanilla int parsing of the payload yields the id
    std::wstring name;
    const std::wstring msg = encodeNameMessage(L"Lab: West");
    assert(decodeNameMessage(msg, name) && name == L"Lab: West");
    assert(std::stoi(msg) == nameToId(L"Lab: West"));
    assert(!decodeNameMessage(L"12", name));
    assert(!decodeNameMessage(L":x", name));
    assert(!decodeNameMessage(L"1a:x", name));
    assert(!decodeNameMessage(L"12:   ", name));

    int32_t id = 0;
    assert(parsePlainId(L"7", id) && id == 7);
    assert(parsePlainId(L"-1", id) && id == -1);
    assert(parsePlainId(L"2147483647", id) && id == 2147483647);
    assert(!parsePlainId(L"2147483648", id));
    assert(!parsePlainId(L"", id) && !parsePlainId(L"-", id) && !parsePlainId(L"3:x", id));
    assert(equalsIgnoreCase(L"ALPHA", L"alpha") && !equalsIgnoreCase(L"ALPHA", L"alph"));
}

int main() {
    test_parse_full_config();
    test_parse_missing_returns_all_skipped();
    test_defaults_match_shipped_lua();
    test_write_and_reload();
    test_load_fallback_on_unwritable_path();
    test_parse_bad_numeric_value_is_skipped();
    test_teleporter_names();
    std::printf("RandomTweaksConfigTest: ALL PASS\n");
    return 0;
}

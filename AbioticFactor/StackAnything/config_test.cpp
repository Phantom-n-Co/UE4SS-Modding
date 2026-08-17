#include "config.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

static void test_parse_basic() {
    const std::wstring text = L"-- comment\nreturn {\n    [\"fish_.+\"] = 20,\n    [\"soup_.+\"] = 10,  -- trailing comment\n    [\"^TV$\"] = 5,\n}\n";
    const auto raw = parseConfigText(text);
    assert(raw.size() == 3);
    assert(raw[0].first == L"fish_.+");
    assert(raw[2].second == 5);
}

static void test_sort_longest_first() {
    const std::vector<std::pair<std::wstring, int>> raw = {
        {L"Container_Locker", 10}, {L"Container_Locker_Security", 10}, {L"zz", 5}
    };
    auto cfg = buildConfig(raw);
    assert(cfg.size() == 3);
    assert(cfg[0].pattern == L"Container_Locker_Security");
    assert(cfg[2].pattern == L"zz");
}

static void test_match_longest_wins() {
    const std::vector<std::pair<std::wstring, int>> raw = {
        {L"Container_Locker", 10}, {L"Container_Locker_Security", 10}
    };
    auto cfg = buildConfig(raw);
    [[maybe_unused]] int maxStack = 0;
    assert(matchRowName(cfg, L"Container_Locker_Security_Key", maxStack));
    assert(maxStack == 10);
    assert(matchRowName(cfg, L"Container_Locker_2", maxStack));
    assert(maxStack == 10);
    assert(!matchRowName(cfg, L"SomethingElse", maxStack));
}

static void test_invalid_pattern_dropped() {
    const std::vector<std::pair<std::wstring, int>> raw = {{L"[unclosed", 10}, {L"ok.+", 5}};
    const auto cfg = buildConfig(raw);
    assert(cfg.size() == 1);
    assert(cfg[0].pattern == L"ok.+");
}

static void test_defaults_load_and_match() {
    auto cfg = buildConfig(std::vector<std::pair<std::wstring, int>>{
        {L"fish_.+", 20}, {L".*[Bb]arrel.*", 10}, {L"^TV$", 5}
    });
    [[maybe_unused]] int maxStack = 0;
    assert(matchRowName(cfg, L"fish_is98", maxStack) && maxStack == 20);
    assert(matchRowName(cfg, L"Deployable_Barrel_01", maxStack) && maxStack == 10);
    assert(matchRowName(cfg, L"TV", maxStack) && maxStack == 5);
    assert(!matchRowName(cfg, L"notTV", maxStack)); // ^TV$ is anchored
}

static void test_write_and_reload() {
    const std::wstring path = L"config_test_tmp.lua";
    const std::vector<std::pair<const wchar_t*, int>> defaults = {{L"fish_.+", 20}, {L"^TV$", 5}};
    assert(writeDefaultConfig(path, defaults));
    std::wifstream in(path);
    const auto content = std::wstring(std::istreambuf_iterator<wchar_t>(in), std::istreambuf_iterator<wchar_t>());
    in.close();
    auto raw = parseConfigText(content);
    assert(raw.size() == 2);
    // sorted alphabetically, like the Lua generator
    assert(raw[0].first == L"^TV$");
    std::remove("config_test_tmp.lua");
}

static void test_load_fallback_on_unwritable_path() {
    const std::wstring path = L"Z:\\nonexistent\\dir\\config.lua";
    const std::vector<std::pair<const wchar_t*, int>> defaults = {{L"fish_.+", 20}};
    auto cfg = loadConfig(path, defaults);
    assert(!cfg.empty());
    assert(cfg[0].pattern == L"fish_.+");
    assert(cfg[0].maxStack == 20);
}

static void test_load_fallback_on_degenerate_file() {
    const std::wstring path = L"config_test_tmp2.lua";
    {
        std::wofstream out(path);
        out << L"-- only comments, no entries\n-- [\"fish_.+\"] = 20\n";
    }
    const std::vector<std::pair<const wchar_t*, int>> defaults = {{L"fish_.+", 20}, {L"^TV$", 5}};
    auto cfg = loadConfig(path, defaults);
    assert(!cfg.empty());
    [[maybe_unused]] int maxStack = 0;
    assert(matchRowName(cfg, L"fish_is98", maxStack) && maxStack == 20);
    assert(matchRowName(cfg, L"TV", maxStack) && maxStack == 5);
    std::remove("config_test_tmp2.lua");
}

int main() {
    test_parse_basic();
    test_sort_longest_first();
    test_match_longest_wins();
    test_invalid_pattern_dropped();
    test_defaults_load_and_match();
    test_write_and_reload();
    test_load_fallback_on_unwritable_path();
    test_load_fallback_on_degenerate_file();
    std::printf("config_test: ALL PASS\n");
    return 0;
}

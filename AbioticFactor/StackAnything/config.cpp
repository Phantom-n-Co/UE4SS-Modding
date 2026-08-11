#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

// Built-in defaults, grouped by category. Patterns use ECMAScript regex syntax.
const std::vector<std::pair<const wchar_t*, int>> kDefaultStackSizes = {
    // Food and consumables
    { L"fish_.+", 20 }, { L"food_.+", 20 }, { L"soup_.+", 20 }, { L"icecream.*", 20 },
    { L"cookingpot", 20 }, { L"FULLWATERPOT", 20 },

    // Collectibles, lore and special items
    { L"bobblehead_.+", 20 }, { L"fig.+", 20 },
    { L"taxidermy_.+", 20 }, { L"wallmount_.+", 20 }, { L"arrivalbell", 20 }, { L"device_garden", 20 },
    { L".*[Ss]kull.*", 20 },
    { L"^(book|book_journal|cookbook|salem_book_.+|sg_book_.+|sydyk_book_.+)$", 20 },
    { L".*[Tt]ablet.*", 20 }, { L"HotwireKit.+", 20 },

    // Tools and equipment
    { L"tramkey", 20 }, { L"lodestone.*", 20 }, { L"anvil", 20 }, { L"Salem_SalvageAnvil", 20 },
    { L"fireextinguisher", 20 }, { L"batterycharger", 20 }, { L"aircompressor", 20 }, { L"pocketwatch", 20 },
    { L"crossbow_broken", 20 }, { L"queenbee", 20 }, { L"^headlamp_.*_broken$", 20 }, { L"votv_journal.+", 20 },

    // Fluid containers
    { L"^barrel_.+", 10 }, { L"Deployable_.*[Bb]arrel.*", 10 }, { L"^bucket_.+", 10 },
    { L"Deployable_Water.+", 10 }, { L"Deployed_WaterTank", 10 }, { L"canteen", 10 }, { L"waterbottle", 10 },
    { L"SoupBowl", 10 },

    // Furniture and storage
    { L".*[Cc]ouch.*", 10 }, { L".*[Cc]hair.*", 10 }, { L".*[Aa]rmchair.*", 10 },
    { L".*[Ss]tool.*", 10 }, { L"^[Bb]ench_", 10 }, { L"^ChurchPew_.+", 10 }, { L"tacklebox", 10 },
    { L".*[Bb]ed.*", 10 }, { L".*[Mm]attress.*", 10 }, { L".*[Dd]esk_", 10 },
    { L"^([Tt]able_.+|Furniture_VOTV_CafeteriaTable|votv_.*[Tt]able.*)$", 10 },
    { L".*[Ss]helf.*", 10 }, { L".*[Cc]rate.*", 10 }, { L"^[Cc]hest_.+", 10 }, { L".*[Ff]ridge.*", 10 },
    { L".*[Ff]reezer.*", 10 }, { L".*[Tt]rash.*", 10 }, { L".*[Bb]ox.*", 10 }, { L".*[Cc]abinet.*", 10 },
    { L".*[Tt]oolbox.*", 10 }, { L"ItemStand", 10 }, { L"armor_?stand", 10 }, { L"^cart", 10 },
    { L"fishtrap", 10 }, { L"Aquarium.+", 10 }, { L"WarmingDrawer", 10 }, { L"rug_.+", 10 },
    { L"Container_KitchenCounter", 10 }, { L"Container_Locker", 10 }, { L"Container_Locker_Security", 10 },
    { L"Container_Locker_VOTV", 10 }, { L"gardenplot_digital", 5 }, { L"constructionsign", 5 },

    // Deployable furniture and storage
    { L"Deployable_.*[Cc]ouch.*", 10 }, { L"Deployable_.*[Cc]hair.*", 10 }, { L"Deployable_.*[Ss]tool.*", 10 },
    { L"Deployable_.*[Dd]esk_", 10 }, { L"Deployable_.*[Tt]able.*", 10 }, { L"Deployable_.*[Cc]abinet.*", 10 },
    { L"Deployable_.*[Tt]oolbox.*", 10 }, { L"Deployable_.*[Tt]rash.*", 10 }, { L"Deployable_.*[Ff]ridge.*", 10 },
    { L"Deployable_.*[Cc]ot.*", 10 }, { L"Deployable_.*[Ss]tand.*", 10 }, { L"Deployable_Medkit", 10 },
    { L"Deployable_ArmoryLocker", 10 },
    { L"^(standing_lamp|Lamp_Standing_Office_01|Deployable_DeskLamp_01|desklamp_elegant)$", 5 },

    // Lighting, decoration and machines
    { L"^(christmaslights_.+|crackedlight|flashlight_.+|makeshift_flashlight|megalight.*|Plant_Antelight.*|seed_antelight.*|wall_light)$", 5 },
    { L"^lantern.*", 5 }, { L"cauldron", 5 }, { L"Deployable_.*[Ll]amp.*", 5 },
    { L"Deployable_.*[Ll]ight.*", 5 }, { L"Deployable_WarningSign", 5 }, { L"Painting_.+", 5 },
    { L"Poster", 5 }, { L"^TV$", 5 }, { L"^ArcadeMachine$", 5 }, { L"VendingMachine.+", 5 },
    { L"CoffeeMachine.+", 5 }, { L"Slushie_Machine.+", 5 }, { L"Deployable_.*[Bb]ench.*", 5 },

    // Electrical, utility and miscellaneous deployables
    { L"Deployed_CraftingBench_SIGNAL", 5 }, { L"battery.+", 5 }, { L"^laser_.+", 5 }, { L".*[Pp]lug.*", 5 },
    { L"ElectricFan", 5 }, { L"^(heater|tutorialheater)$", 5 }, { L".*[Cc]harging.*", 5 }, { L"TeslaCoil", 5 },
    { L"oven", 5 }, { L"autosalvager", 5 }, { L".*[Bb]arricade.*", 5 }, { L"toilet.+", 5 },
    { L"[Tt]oilet_.+", 5 }, { L"Deployed_VOTV_Toilet", 5 }, { L"Faucet_.+", 5 }, { L"[Ff]aucetSink", 5 },
    { L"lever", 5 }, { L"^bridge_hardlight_.+", 5 },
    { L"^(JumpPad|charging_pad|distribution_pad|teleporter_pad)$", 5 },
    { L"^(pestdummy|PestTeleporter|PestTrap|PestWheel)$", 5 },
    { L"sync", 5 }, { L"scarecrow_symph", 5 }, { L"neutrino_emitter", 5 }, { L"decon_shower", 5 },
    { L"exercisebike", 5 }, { L"benchpress", 5 }, { L"moisture_teleporter", 5 }, { L"database", 5 },
    { L"stove.+", 5 }, { L"[Ss]tove_.+", 5 }, { L"Leyak_Containment", 5 }, { L".*[Tt]urret.*", 5 },
    { L".*[Cc]lock.*", 5 }, { L"[Dd]eployable_.+", 5 }, { L"^votv_.+", 5 }, { L"inhibitor.+", 5 },
    { L"AlienThermite", 5 }, { L"Tripmine", 5 }, { L"trap_shock", 5 }, { L"SeeSaw", 5 },
    { L"^Ramp", 5 }, { L"^cushion$", 5 }, { L"tmog", 5 }, { L"pumpkin_carved", 5 }, { L"beehive", 5 },
    { L"xmastree", 5 }, { L"beacon", 5 }, { L"snowman_crafted", 5 }, { L"PlantRope", 5 },
    { L"Sign_WetFloor", 5 }, { L"ReaperSigil", 5 }, { L"holly_01", 5 }, { L"rog_torii", 5 },
    { L"mushroomfarm", 5 }, { L"human_skull_clean", 5 }, { L"Banner.+", 5 }, { L"tapestry_order", 5 },
    { L"DropShield", 5 }, { L"key_.+", 20}
};

std::vector<ConfigEntry> buildConfig(const std::vector<std::pair<std::wstring, int>>& raw) {
    std::vector<ConfigEntry> out;
    for (const auto& [pattern, size] : raw) {
        try {
            std::wregex re(pattern);
            out.push_back(ConfigEntry{pattern, std::move(re), size});
        } catch (const std::regex_error&) {
            // invalid pattern: skipped (mirrors the Lua mod's validation)
        }
    }
    std::sort(out.begin(), out.end(), [](const ConfigEntry& a, const ConfigEntry& b) {
        return a.pattern.size() > b.pattern.size() ||
               (a.pattern.size() == b.pattern.size() && a.pattern < b.pattern);
    });
    return out;
}

std::vector<std::pair<std::wstring, int>> parseConfigText(const std::wstring& content) {
    std::vector<std::pair<std::wstring, int>> out;
    std::wregex lineRe(L"^\\s*\\[\"(.*)\"\\]\\s*=\\s*(\\d+)\\s*,?\\s*$");
    std::wistringstream ss(content);
    std::wstring line;
    while (std::getline(ss, line)) {
        auto comment = line.find(L"--");
        if (comment != std::wstring::npos) {
            line = line.substr(0, comment);
        }
        std::wsmatch m;
        if (std::regex_match(line, m, lineRe)) {
            int value = 0;
            try { value = std::stoi(m[2].str()); } catch (const std::exception&) { continue; }
            out.emplace_back(m[1].str(), value);
        }
    }
    return out;
}

bool matchRowName(const std::vector<ConfigEntry>& config, const std::wstring& rowName, int& outMaxStack) {
    for (const auto& entry : config) {
        if (std::regex_search(rowName, entry.regex)) {
            outMaxStack = entry.maxStack;
            return true;
        }
    }
    return false;
}

bool writeDefaultConfig(const std::wstring& path, const std::vector<std::pair<const wchar_t*, int>>& defaults) {
    std::vector<std::pair<std::wstring, int>> sorted;
    for (const auto& [p, s] : defaults) {
        sorted.emplace_back(p, s);
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::wofstream out(path);
    if (!out) {
        return false;
    }
    out << L"-- StackAnything configuration\n";
    out << L"-- Generated from defaults on first run. Edit this file to customize.\n";
    out << L"-- Format: [pattern] = maxStackSize, longest pattern wins.\n";
    out << L"-- Items not matched by any pattern keep their vanilla stack size.\n";
    out << L"return {\n";
    for (const auto& [p, s] : sorted) {
        out << L"    [\"" << p << L"\"] = " << s << L",\n";
    }
    out << L"}\n";
    return true;
}

std::vector<std::pair<std::wstring, int>> toWStringPairs(const std::vector<std::pair<const wchar_t*, int>>& defaults) {
    std::vector<std::pair<std::wstring, int>> out;
    for (const auto& [p, s] : defaults) {
        out.emplace_back(p, s);
    }
    return out;
}

std::vector<ConfigEntry> loadConfig(const std::wstring& path, const std::vector<std::pair<const wchar_t*, int>>& defaults) {
    std::wifstream in(path);
    std::wstring content;
    if (in) {
        content.assign(std::istreambuf_iterator<wchar_t>(in), std::istreambuf_iterator<wchar_t>());
    }
    auto hasContent = [&content]() {
        return std::any_of(content.begin(), content.end(), [](wchar_t c) { return !std::isspace(c); });
    };
    if (!hasContent()) {
        if (!writeDefaultConfig(path, defaults)) {
            return buildConfig(toWStringPairs(defaults));
        }
        in.clear();
        in.open(path);
        content.assign(std::istreambuf_iterator<wchar_t>(in), std::istreambuf_iterator<wchar_t>());
        if (!hasContent()) {
            return buildConfig(toWStringPairs(defaults));
        }
    }
    std::vector<std::pair<std::wstring, int>> parsed = parseConfigText(content);
    if (parsed.empty()) {
        return buildConfig(toWStringPairs(defaults));
    }
    return buildConfig(parsed);
}

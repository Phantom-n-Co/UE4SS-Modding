#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <sstream>

// Built-in defaults, matching the shipped config.lua of the Lua mod.
const TweakConfig kDefaultTweaks = {
    /*instantButcher =*/true,
    /*debugLogging =*/false,
    /*walkSpeed =*/550.0,
    /*sprintSpeed =*/825.0,
    /*staminaDrainRate =*/0.25,
    /*staminaRegainRate =*/0.8,
    /*interactionRange =*/320.0,
    /*ladderClimbSpeed =*/350.0,
    /*jumpHeight =*/500.0,
    /*lightIntensity =*/8.4,
    /*lanternIntensity =*/5.45,
    /*disableWatchLight =*/false,
    /*wishingShelfTopUp =*/true,
};

namespace {

struct OptionalField {
    const wchar_t* name;
    std::optional<double> TweakConfig::* field;
    const wchar_t* vanilla;
};

const OptionalField kOptionalFields[] = {
    {L"walkSpeed",         &TweakConfig::walkSpeed,         L"vanilla 500"},
    {L"sprintSpeed",       &TweakConfig::sprintSpeed,       L"vanilla 685"},
    {L"staminaDrainRate",  &TweakConfig::staminaDrainRate,  L"vanilla 0.2"},
    {L"staminaRegainRate", &TweakConfig::staminaRegainRate, L"vanilla 0.2"},
    {L"interactionRange",  &TweakConfig::interactionRange,  L"vanilla 200"},
    {L"ladderClimbSpeed",  &TweakConfig::ladderClimbSpeed,  L"vanilla 275"},
    {L"jumpHeight",        &TweakConfig::jumpHeight,        L"vanilla 425"},
    {L"lightIntensity",    &TweakConfig::lightIntensity,    L"vanilla 8.0"},
    {L"lanternIntensity",  &TweakConfig::lanternIntensity,  L"vanilla 4.75"},
};

std::wstring trim(const std::wstring& s) {
    const auto b = s.find_first_not_of(L" \t\r\n");
    if (b == std::wstring::npos) {
        return {};
    }
    const auto e = s.find_last_not_of(L" \t\r\n");
    return s.substr(b, e - b + 1);
}

void parseAssignment(const std::wstring& line, TweakConfig& cfg, bool inTweaks) {
    const auto eq = line.find(L'=');
    if (eq == std::wstring::npos) {
        return;
    }
    std::wstring key = trim(line.substr(0, eq));
    std::wstring value = trim(line.substr(eq + 1));
    if (!value.empty() && value.back() == L',') {
        value.pop_back();
    }
    value = trim(value);
    if (key.empty() || value.empty()) {
        return;
    }

    const bool isBool = (value == L"true" || value == L"false" || value == L"nil");
    if (inTweaks) {
        if (key == L"disableWatchLight") {
            cfg.disableWatchLight = (value == L"true");
            return;
        }
        for (const auto& f : kOptionalFields) {
            if (key == f.name) {
                if (value == L"nil") {
                    cfg.*(f.field) = std::nullopt; // nil = skip tweak
                } else if (!isBool) {
                    try {
                        cfg.*(f.field) = std::stod(value);
                    } catch (...) {
                    }
                }
                return;
            }
        }
        return;
    }

    if (key == L"instantButcher") {
        cfg.instantButcher = (value == L"true");
    } else if (key == L"debugLogging") {
        cfg.debugLogging = (value == L"true");
    } else if (key == L"wishingShelfTopUp") {
        cfg.wishingShelfTopUp = (value == L"true");
    }
}

} // namespace

TweakConfig parseConfig(const std::wstring& content) {
    TweakConfig cfg{}; // nothing set until explicitly present in the file
    bool inTweaks = false;
    std::wistringstream ss(content);
    std::wstring line;
    while (std::getline(ss, line)) {
        const auto comment = line.find(L"--");
        if (comment != std::wstring::npos) {
            line = line.substr(0, comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        if (line.find(L"player") != std::wstring::npos && line.find(L'{') != std::wstring::npos) {
            inTweaks = true;
            continue;
        }
        if (line[0] == L'}') {
            inTweaks = false;
            continue;
        }
        parseAssignment(line, cfg, inTweaks);
    }
    return cfg;
}

bool writeDefaultConfig(const std::wstring& path) {
    std::wofstream out(path);
    if (!out) {
        return false;
    }
    out << L"-- RandomTweaks configuration\n";
    out << L"-- Generated from defaults on first run. Edit this file to customize.\n";
    out << L"-- Remove a line or set it to nil to skip that tweak.\n";
    out << L"return {\n";
    out << L"  debugLogging = " << (kDefaultTweaks.debugLogging ? L"true" : L"false") << L",\n";
    out << L"  instantButcher = " << (kDefaultTweaks.instantButcher ? L"true" : L"false") << L",\n";
    out << L"  wishingShelfTopUp = " << (kDefaultTweaks.wishingShelfTopUp ? L"true" : L"false")
        << L",  -- also fill a stocked stocking slot when it already holds the shelf slot's item\n";
    out << L"\n  player = {\n";
    for (const auto& f : kOptionalFields) {
        const double v = (kDefaultTweaks.*(f.field)).value();
        out << L"    " << f.name << L" = " << v << L",  -- " << f.vanilla << L"\n";
    }
    out << L"    disableWatchLight = " << (kDefaultTweaks.disableWatchLight ? L"true" : L"false")
        << L",  -- moves the wristwatch point light far away\n";
    out << L"  },\n";
    out << L"}\n";
    return true;
}

TweakConfig loadConfig(const std::wstring& path) {
    std::wifstream in(path);
    std::wstring content;
    if (in) {
        content.assign(std::istreambuf_iterator<wchar_t>(in), std::istreambuf_iterator<wchar_t>());
    }
    const bool hasContent = std::any_of(content.begin(), content.end(), [](wchar_t c) { return !std::iswspace(c); });
    if (!hasContent) {
        writeDefaultConfig(path); // best effort; fall back to defaults below
        return kDefaultTweaks;
    }
    return parseConfig(content);
}

#pragma once
#include <optional>
#include <string>

struct TweakConfig {
    bool instantButcher = false;
    bool debugLogging = false;
    std::optional<double> walkSpeed;
    std::optional<double> sprintSpeed;
    std::optional<double> staminaDrainRate;
    std::optional<double> staminaRegainRate;
    std::optional<double> interactionRange;
    std::optional<double> ladderClimbSpeed;
    std::optional<double> jumpHeight;
    std::optional<double> lightIntensity;
    std::optional<double> lanternIntensity;
    bool disableWatchLight = false;
    bool wishingShelfTopUp = true;
};

// Built-in defaults, matching the Lua mod's config.lua. Only used when
// config.lua is missing or empty (defaults are written to disk then).
extern const TweakConfig kDefaultTweaks;

// Parse Lua-style config text. Missing keys stay skipped (nil-to-skip semantics).
TweakConfig parseConfig(const std::wstring& content);

// Load <path> if present and non-empty; otherwise write defaults and use them.
TweakConfig loadConfig(const std::wstring& path);

// Write the built-in defaults to <path> in Lua format, for user editing.
bool writeDefaultConfig(const std::wstring& path);

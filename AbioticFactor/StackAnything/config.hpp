#pragma once
#include <regex>
#include <string>
#include <utility>
#include <vector>

struct ConfigEntry {
    std::wstring pattern;   // original Lua pattern (also valid ECMAScript)
    std::wregex regex;      // compiled matcher
    int maxStack;
};

// Built-in defaults, verbatim from the Lua mod's defaults.lua (defined in config.cpp).
extern const std::vector<std::pair<const wchar_t*, int>> kDefaultStackSizes;

// Build a config list from raw (pattern, maxStack) pairs.
// Sorted longest-pattern-first (ties by lexical order), like the Lua sort.
// Patterns that fail to compile as ECMAScript regex are dropped.
std::vector<ConfigEntry> buildConfig(const std::vector<std::pair<std::wstring, int>>& raw);

// Parse Lua-style config text: lines of ["pattern"] = N, with -- comments.
std::vector<std::pair<std::wstring, int>> parseConfigText(const std::wstring& content);

// First match wins (config must be longest-first). Returns true and sets outMaxStack on match.
bool matchRowName(const std::vector<ConfigEntry>& config, const std::wstring& rowName, int& outMaxStack);

// Write defaults to <path> in Lua format (sorted by pattern), for user editing.
bool writeDefaultConfig(const std::wstring& path, const std::vector<std::pair<const wchar_t*, int>>& defaults);

// Load <path> if present and parseable; otherwise write defaults and load those.
std::vector<ConfigEntry> loadConfig(const std::wstring& path, const std::vector<std::pair<const wchar_t*, int>>& defaults);

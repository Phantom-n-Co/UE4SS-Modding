#pragma once
#include <vector>
#include "config.hpp"

// Resolves item-row field offsets and checks the GameTypes.hpp struct mirrors
// against the game's reflection data. Logs every mismatch; returns false if any
// mirror is wrong (the inventory hooks are then not registered).
bool initGameLayout();

// Raises StackSize per config for every matched non-pet item row, in every
// loaded item table (base and mod tables). Re-applied on each player spawn.
// A non-empty dumpPath also writes every item row there after each apply.
void applyStackTweaks(const std::vector<ConfigEntry>& config, const std::wstring& dumpPath);

// Registers the re-apply hook plus, if the layout check passed, all
// liquid-split, variant-guard/sort and upgrade-batch hooks. Returns number registered.
int registerHooks();

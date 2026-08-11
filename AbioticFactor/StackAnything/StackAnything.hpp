#pragma once
#include <vector>
#include "config.hpp"

// Raises StackSize_47 per config for every matched non-pet item row.
void applyStackTweaks(const std::vector<ConfigEntry>& config);

// Registers all liquid-split and variant-guard/sort hooks. Returns number registered.
int registerHooks();

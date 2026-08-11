#pragma once
#include <string>
#include "config.hpp"

// Loads config, resolves the player class, applies tweaks to its defaults and
// registers hooks. Returns false until the player class resolves (call every
// engine tick). Safe to call repeatedly once initialized.
bool tryInit(const TweakConfig& config);

// Registers all hooks. Returns number registered. Only call once.
int registerHooks();

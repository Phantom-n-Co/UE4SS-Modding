#pragma once

#include <cstdint>

namespace AFTestBridge {

constexpr uint16_t kPort = 27931;

// Starts the loopback listener thread and registers the game-thread dispatcher.
void start();
// Stops the listener, unregisters the dispatcher. Safe to call twice.
void stop();

} // namespace AFTestBridge

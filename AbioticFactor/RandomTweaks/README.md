# RandomTweaks

A handful of quality-of-life tweaks: instant butchering, a wish-list stocking top-up for the Wishing Shelf, hiding the full suit and backpack through transmog, custom teleporter pad names, and configurable player stat adjustments.

## Requirements

Built against UE4SS 3.0.1 (commit `1ba38db4`) for Abiotic Factor 1.5.0 (Modding Update).

## Installation

This mod is part of a C++ monorepo. Build it with CMake + Ninja (see [../README.md](../README.md) for build commands), then deploy via Amethyst Mod Manager. The deployed path will be somewhere under your UE4SS-Modding mod folder — the config file lives at `config.lua` inside that directory.

## Configuration

Edit `config.lua` in the deployed mod directory.

On first run the mod writes a default config. Delete `config.lua` and restart to regenerate it; it is never overwritten while it exists. Remove a line or set it to `nil` to skip that tweak.

## Features

### Instant Butcher

Every damage event with a valid damage type gibs the target instantly.

### Wishing Shelf Top-Up

When the Wishing Shelf spawns gifts, a stocked stocking slot that already holds the same item as the shelf slot is topped up to its max stack instead of being left alone. The amount is rolled from the same quantity range the shelf uses, and items gated behind world flags stay gated.

### Hide Full Suit and Backpack

The full suit and backpack can be hidden through the Transmog Dresser, the same way as other armor. Config key: `transmogHideSuitAndBackpack`.

### Nameable Teleporters

Teleporter pads can be given any name again, as before the fixed tag list. The tag box at the top of the pad's tag menu is editable: type a name (up to 32 characters) and confirm. Pads with the same name link to each other; names ignore upper/lower case. Names are saved with the pad. Config key: `nameableTeleporters`.

The premade tags are gone from the menu. The list shows the names in use instead, each with the number of pads that have it (e.g. "Main Base (2)"), so an unpaired pad stands out; click one to use it. Pads that still have a tag keep it until renamed.

The menu is only as tall as its list: it shrinks with the number of names in use and stops growing at eight rows; the scrollbar shows up only when there are more names than that.

- **Multiplayer** — the host (or dedicated server) needs the mod to store and show names. Players without the mod see the pad without a name. On a host without the mod, pads named by a modded player still link by name, but the name is not shown or kept. The name list of the host lists every named pad of the map; other players only see the names of pads near them (the parts of the facility currently loaded for them).
- **Removing the mod** — on the next load the game converts the names to tags with its own upgrade for old pre-tag names. Check the pads' tags afterwards.

### Player Tweaks

Adjustable player stats, each independent:

- **Movement** — walkSpeed, sprintSpeed, ladderClimbSpeed, jumpHeight.
- **Stamina** — staminaDrainRate, staminaRegainRate.
- **Comfort** — interactionRange.
- **Lighting** — lightIntensity, lanternIntensity, disableWatchLight.

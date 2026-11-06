# StackAnything

Modify any items to stack to any amount you wish — furniture, fish, deployables, or anything else. Fully configurable via regular expressions.

## Requirements

Built against UE4SS 3.0.1 (commit `1ba38db4`) for Abiotic Factor 1.5.0 (Modding Update).

## Installation

This mod is part of a C++ monorepo. Build it with CMake + Ninja (see [../README.md](../README.md) for build commands), then deploy via Amethyst Mod Manager. The deployed path will be somewhere under your UE4SS-Modding mod folder — the config file lives at `config.lua` inside that directory.

## Configuration

Edit `config.lua` in the deployed mod directory.

On first run (or if `config.lua` is missing or empty) the mod writes a default config covering built-in item categories, generated from internal defaults. Delete `config.lua` and restart to regenerate it; it is never overwritten while it exists. Edit `config.lua` to customize.

The config maps regular expressions to max stack sizes. Longer (more specific) patterns win; items not matched by any pattern keep their vanilla stack size. Items added by official (mod.io) mods are matched too.

```lua
return {
  ["fish_.+"] = 20,      -- all fish
  [".*[Cc]hair.*"] = 2,  -- any chair
  ["paintbucket"] = 10,  -- exact match
}
```

The basics: `.` matches any single character, `+` one or more of the preceding, `*` zero or more, and `[Cc]` matches any character in the class. Regular expressions support many more constructs — for a full reference see [regex101](https://regex101.com/) or [RegExr](https://regexr.com/).

## Safe Stacking

Stacking is always safe, even for items that normally duplicate or lose data when merged.

### Fluid Containers

Fillable fluid containers (bottles, buckets, pots, soups, etc.) can be stacked without any fluid or food duplication.

Stacking them is safe because the mod auto-splits a stack whenever its liquid state changes:

- **Filling** — one fill cost only fills *one* container. The single filled container moves to a free slot (or joins an existing identical stack); the rest of the stack stays empty. If no slot is available the fill is reverted and the container's fluid is refunded, so nothing is duplicated or lost.
- **Emptying** — only one container drains. The rest of the stack keeps its fluid and stays in your hand, ready to empty again. The single drained container moves out to a free slot (or an existing identical stack).
- **Stacking rules** — drag-and-drop and shift-click will not merge containers with different liquid states (filled vs empty, or different liquids); identical containers merge normally. Inventory sort groups containers by their liquid state so filled and empty ones never combine.

Batteries and laser cells keep their vanilla stack-charge behavior.

### Texture-Variant Items

Paintings, TVs, arcade machines, and rare fish share a base item row, but each instance carries its own texture variant (different painting art, TV channel, arcade game, or fish variant). Stacking them is safe because the mod treats each variant as a distinct item:

- **Stacking rules** — drag-and-drop and shift-click will not merge two items of the same row that have different texture variants (or a base item with a variant). Identical variants merge normally.
- **Sorting** — inventory sort groups items by their variant, so different variants never combine while identical ones still do.

Yes, this also applies to fish caught from fishing.

### Upgrade Bench

Stacked items can be placed into the upgrade bench and upgraded in batches:

- **Costs** — upgrade requirements scale with the inserted stack size, so upgrading *N* items costs *N* times each required item.
- **Display** — the upgrade screen shows the scaled requirements and availability.
- **Batch upgrades** — the entire inserted stack upgrades at once after the scaled costs are paid.

## Finding Item IDs

To find the item IDs used in the config patterns, the mod can dump every item it knows about:

1. Open `config.lua` in the deployed mod directory
2. Set `dumpItems = true` near the top (add the line inside `return {` if your config predates it)
3. Restart the game and load a world (the dump is rewritten on startup and on each world load, so items from official mods are included)
4. Read `item_dump.lua` in the same directory — it lists every item's ID (row name), display name, current stack size, and more
5. Set `dumpItems = false` and restart to stop dumping

## Default Overrides

| Category | Stack Size |
|----------|-----------|
| Fish | 20 |
| Food & soups | 20 |
| Collectables | 20 |
| Lore & unsorted | 20 |
| Fluid containers | 10 |
| Furniture & storage | 10 |
| Deployable variants | 10 |
| Lighting | 5 |
| Paintings & posters | 5 |
| Noisy (TVs, arcades) | 5 |
| Crafting benches | 5 |
| Electric | 5 |
| Misc deployable | 5 |

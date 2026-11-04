# StackAnything changelog

## 3.0.2
- Built against UE4SS 1.23.0 (35d1795d), fixing crash on launch.

## 3.0.1
- Made default patterns more specific to avoid unintentional stack increases.

## 3.0.0
- Ported the mod to C++.
- Added stacking support for weapons and gear.
- Added upgrade bench support for stacked items, including scaled upgrade costs.
- Added scaled upgrade requirement display in the upgrade bench.

## 2.2.0
- Fixed an error in sorting.
- Added default overrides for items added by "kleptomania" mod.

## 2.1.1
- Removed unecessary files.

## 2.1.0
- Include texture variant items in stacking/sorting guards, preventing different Paintings/TVs/(Rare)Fish/etc from stacking with base/other variants

## 2.0.0
- New stacking behaviour for fluid containers.
- Separates default overrides from user defined ones. Delete "config.lua" to regenerate it from defaults.
- Also a much bigger list of default stacking overrides.

# UE4SS Mods for Abiotic Factor

C++ source monorepo of UE4SS (Unreal Engine 5) mods, built on Linux and deployed to Windows via Proton.

## Mods

| Mod | Description | License |
|-----|-------------|---------|
| [RandomTweaks](AbioticFactor/RandomTweaks/) | Gameplay tweaks | OSL v3.0 (see [LICENSE](AbioticFactor/RandomTweaks/LICENSE)) |
| [StackAnything](AbioticFactor/StackAnything/) | Unlimited item stacking | OSL v3.0 (see [LICENSE](AbioticFactor/StackAnything/LICENSE)) |

## Dependencies

- **RE-UE4SS** — the UE4SS engine framework. MIT licensed (© 2022 Narknon). See `RE-UE4SS/LICENSE`.

## Build & deploy

```
cmake --preset game-shipping-xwin               # configure (once / after CMake changes)
cmake --build --preset game-shipping-xwin       # all targets
```

## Licensing

Individual mod source code is licensed under each mod's own LICENSE file (OSL v3.0).

All other files in this repository (build scripts, tooling, configuration, documentation) are © 2025–2026 Phantom and all rights reserved. See [LICENSE](LICENSE).

Submodule dependencies carry their own licenses as noted above.

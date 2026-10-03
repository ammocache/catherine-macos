# Catherine (Xbox 360) — native macOS recompilation

A static recompilation of *Catherine* (Atlus, 2011, Xbox 360) into a native
Apple Silicon macOS app, built with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

> **This repository contains no game code or game data.** You must own the game
> and provide your own dump. The recompiled code is generated on your machine
> from your copy during the build.

## Status

- Boots, menus, cutscenes and gameplay work (tested into the first stage).
- Known issues: faint outlines around some menu text, HUD glitch bars,
  audio stutter, fullscreen mode is broken (windowed is the default).

## Requirements

- Apple Silicon Mac, macOS 15+
- Homebrew, CMake, Ninja, LLVM/clang
- The ReXGlue SDK v0.10.0 (commit `c94f5eb`) checked out at `../tools/rexglue-sdk`
- Your own extracted retail **Catherine (USA)** Xbox 360 game folder
  (containing `default.xex`), placed at `../retail-game`

Expected layout:

```
catherine-project/
├── rex-retail/          <- this repository
├── retail-game/         <- your game files (default.xex, Data/, ...)
└── tools/rexglue-sdk/   <- ReXGlue SDK
```

## Build

```
scripts/apply_sdk_patches.sh   # once, applies our SDK fixes from patches/
scripts/build.sh               # configures (first time), recompiles and builds
```

## Play

```
scripts/play.sh
```

Default controls (keyboard): WASD = left stick, arrows = right stick,
Space = A, Backspace = B, L = X, P = Y, Return = Start, Tab = Back.
In-game overlays (hold fn on Mac keyboards): F3 performance, F4 settings,
F7 achievements, ` console. Settings are saved to `catherine.toml` next to the
executable.

## SDK patches

| Patch | Purpose |
|---|---|
| `0001-unclipped-draw-extent-default-on.patch` | Fixes the black screen when gameplay starts (EDRAM ownership of full-screen clears). Also set as an app default in `src/catherine_app.h`. |
| `0002-debug-env-switches-sampler-centroid.patch` | Debug-only switches (`CATH_SAMPLER_DBG`, `CATH_CENTROID` environment variables); no effect unless set. |

## Legal

Catherine is © Atlus / SEGA. This project is not affiliated with or endorsed by
them. It distributes only original code and configuration; it does not
distribute any part of the game.

## Credits

ReXGlue SDK, Xenia (graphics backend this SDK's GPU plugin derives from),
MoltenVK, XenonRecomp / XenonAnalyse (hedge-dev).

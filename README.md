# Afterglow 2

Source code of Afterglow 2, a Skyrim SE/AE SKSE plugin that paints cum overlays after orgasms in SexLab P+ (also
SexLab Classic and OStim Standalone) scenes. Everything is native: painting, drying, fading and the settings page run
in the DLL; one small bridge script turns SexLab's and OStim's events into native calls. 3BA/CBBE and UBE bodies,
no SexLab file replaced, no RaceCompatibilityCondition.

The released mod (DLL, plugin, scripts and the texture archives) is distributed separately; this repository holds
the code that builds it.

## How it works

- `src/core` holds the game-independent rules, covered by the offline tests in `tests/`: which zones a cum tag mask
  paints, who may be painted (men and creatures as sources, male targets optional), free-slot selection that never
  touches other mods' overlays, texture index choice without repeats, drying by game time, the save format.
- `Manager` turns orgasms into layers on the main thread, keeps the per-actor state in the SKSE co-save, dries and
  fades layers with one sleeping ticker thread (no per-frame polling) and checks textures through the game's resource
  system, so they may live in BSA archives.
- `Skee` talks to RaceMenu through its plugin interface (`lib/skee/SKEE_IPluginInterface.h`): overlay nodes are
  created immediately for NPCs and every layer is stored as a node override (survives armor swaps, 3D rebuilds and
  save/load). Each layer gets the `NoDynamicNormalMap` extra data, Mu Dynamic NormalMap's documented opt-out, so its
  generated skin normal map does not replace the layer's own normal map; the mark is restored after 3D rebuilds.
- `Body` tells UBE from CBBE/3BA by the actor itself: a skin body mesh under `meshes\!UBE\` or a UBE race.
- `SexLabAlpha` can hide SexLab P+'s own cum overlays (`fCumAlpha` = 0) and restores the old value when turned off.
- `Menu` is the SKSE Menu Framework page; `Settings` reads and writes `Afterglow.ini` (defaults in `data/`).
- `papyrus/Source/Scripts` are the bridge scripts (`Afterglow_SexLab` for SexLab P+ 2.18.1/2.19 and Classic,
  `Afterglow_OStim` for OStim Standalone); `papyrus/stubs` are signature-only stubs to compile them without those
  frameworks installed. `plugin/Afterglow.esp` is the light plugin with the bridge quest, `data/Afterglow.seq` its
  SEQ file.

## Building

Requires [xmake](https://xmake.io) 3.x and Visual Studio 2022, plus two libraries placed in `lib/` (not included):

- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng) (alandtse's `ng` branch) at
  `lib/CommonLibSSE-NG`
- [SKSE-Menu-Framework-3-API](https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API) at
  `lib/SKSE-Menu-Framework-3-API`

```
xmake f -m releasedbg --skyrim_vr=n
xmake build Afterglow
xmake build tests && xmake run tests
```

Before building, apply `commonlib-unk8c.patch` inside `lib/CommonLibSSE-NG` (`git apply ../../commonlib-unk8c.patch`).
Older revisions of the `ng` branch declare an extra `TESObjectREFR` virtual that Skyrim SE/AE do not have, so in an
SE/AE-only build every later virtual (`IsDead`, `IsInCombat`, ...) is called one slot too far.

The Papyrus scripts compile with the game's Papyrus compiler against the vanilla and SKSE script sources plus
`papyrus/stubs`.

## Credits

- CoverOne - SLACS 2 + SCO, the original idea and the classic textures (not in this repository).
- expired6978 - RaceMenu / SKEE; `lib/skee/SKEE_IPluginInterface.h` declares its plugin interface.
- Scrab and contributors - SexLab P+. Thiago099 and contributors - SKSE Menu Framework.
- CommonLibSSE-NG and its contributors; the SKSE team.

## License

MIT, see `LICENSE`.

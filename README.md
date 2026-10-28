# EasyOffset for Quest (Beat Saber 1.40.8)

A Quest port of [EasyOffset](https://github.com/Reezonate/EasyOffset) by Reezonate, built with
beatsaber-hook 6.4.2, bs-cordl 4008.0.0 and BSML 0.4.55.

## Install

Install `EasyOffset.qmod` with [ModsBeforeFriday](https://mbf.bsquest.xyz/) on Beat Saber **1.40.8**.
The core mods (beatsaber-hook, BSML, custom-types, paperlog) are downloaded automatically if missing.

- **Easy Offset tab** (in the gameplay setup panel next to the song list): all adjustment tools.
- **Mod Settings → Easy Offset**: enable/disable and tracking space.

## What is ported

| PC feature | Quest |
|---|---|
| Controller offset (pivot, rotation, Z offset) | yes |
| Basic / Position / Rotation modes | yes |
| Position Auto / Rotation Auto | yes |
| Swing Benchmark | yes, results shown as text (no 3D trail/plane) |
| Direct mode sliders incl. Curve/Balance | yes |
| Room Offset | yes |
| Undo / Redo | yes |
| Mirror / Reset | yes |
| Presets (PC V1 and V2 files) | yes |
| 3D gizmos, controller models, video user guide | no: they come from a PC-only Unity asset bundle |
| Controller type picker / non-Quest button layouts | no: Quest Touch controllers only |

## Using it

1. Open the Easy Offset tab and pick an adjustment mode.
2. Hold the assigned button (Y/B by default) and move your hand. With *Use free hand* on,
   pressing the button on one controller adjusts the other one, just like on PC.
3. Changes save automatically to `/sdcard/ModData/com.beatgames.beatsaber/Configs/easyoffset.json`.

## PC presets

Copy preset `.json` files from `Beat Saber/UserData/EasyOffset/Presets/` on PC to
`/sdcard/ModData/com.beatgames.beatsaber/Mods/easyoffset/Presets/`, press **Refresh**, select one and press **Load**.

The **Tracking space** setting decides what the offsets are relative to:

- **PC (Meta Link)** (default): the Quest controller pose is converted to the pose PC Beat Saber sees over Link,
  so PC presets mean the same thing here. The conversion constant comes from
  [great-hb/EasyOffsetQuest](https://github.com/great-hb/EasyOffsetQuest) (MIT).
- **Quest native**: offsets are relative to the raw Quest controller pose.

## Building

Needs Android NDK r27, CMake 3.22+, Ninja and [qpm](https://github.com/QuestPackageManager/QPM.CLI).
Put the NDK path in `ndkpath.txt` (no spaces in it), then:

```
qpm restore
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build
cmake --build build
qpm qmod manifest
qpm qmod zip --skip_build EasyOffset.qmod
```

## Logs

`/sdcard/ModData/com.beatgames.beatsaber/logs2/EasyOffset.log` (paperlog). On first launch it should contain
`Config loaded` and `Left/Right controller hooked`.

## Credits

- [EasyOffset](https://github.com/Reezonate/EasyOffset) by Reezonate: everything this port is based on (MIT).
- [great-hb/EasyOffsetQuest](https://github.com/great-hb/EasyOffsetQuest): Meta Link basis constant (MIT).
- [BeatSaberOffsetMigrator](https://github.com/qe201020335/BeatSaberOffsetMigrator): reference for how 1.40 places the view anchor.

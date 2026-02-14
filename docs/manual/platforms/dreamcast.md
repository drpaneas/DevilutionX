# devilutionX Dreamcast port

## How To Play:

- Build a CDI image using `Packaging/dreamcast/build.sh`.
- Copy `DIABDAT.MPQ` from your CD or GoG installation to `Packaging/dreamcast/cd_root/` before building (or [extract it from the GoG installer](https://github.com/diasurgical/devilutionX/wiki/Extracting-MPQs-from-the-GoG-installer)).
- `spawn.mpq` is optional for shareware mode.
- Use your own data files. Diablo MPQs are proprietary Blizzard assets.
- Boot the resulting CDI on hardware or in an emulator such as Flycast.

## Building from Source

The Dreamcast build applies a bundled `libfmt` SH4 patch automatically, so no external KOS `fmt` patch file is required.

```bash
./Packaging/dreamcast/build.sh
```

See also: `Packaging/dreamcast/README.md`

## Controls

- D-pad or analog stick: move hero
- A: attack, interact, confirm
- B: spell select, back
- X: pickup and use nearby interactables
- Y: cast selected spell
- Start: menu

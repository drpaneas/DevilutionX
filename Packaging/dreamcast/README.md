# Dreamcast Build

This folder contains the Dreamcast packaging flow for DevilutionX.

## Prerequisites

- KallistiOS (KOS) and kos-ports installed.
- `mkdcdisc` available (from antiruins tools).
- `patch` utility available on the host system.
- `cd_root/` prepared with `IP.BIN`.
- No external `fmt` patch setup is needed. The Dreamcast build applies the bundled SH4 `libfmt` patch automatically.

## Game Data (Required)

- You must provide your own MPQ files. Diablo data files are proprietary assets owned by Blizzard.
- Put `DIABDAT.MPQ` in `Packaging/dreamcast/cd_root/`.
- `spawn.mpq` is optional for shareware mode.
- For installer extraction methods, see [Extracting MPQs from the GoG installer](https://github.com/diasurgical/devilutionX/wiki/Extracting-MPQs-from-the-GoG-installer).

If you only have the GoG installer, run:

```sh
cd Packaging/dreamcast
innoextract -s -d /tmp/dvx-gog-extract /path/to/setup_diablo_*.exe
cp "$(find /tmp/dvx-gog-extract -type f \( -iname 'DIABDAT.MPQ' -o -iname 'diabdat.mpq' \) -print -quit)" cd_root/DIABDAT.MPQ
# Optional (shareware):
SPAWN_MPQ="$(find /tmp/dvx-gog-extract -type f -iname 'spawn.mpq' -print -quit)"; [ -n "$SPAWN_MPQ" ] && cp "$SPAWN_MPQ" cd_root/spawn.mpq
```

## Build

From repo root:

```sh
./Packaging/dreamcast/build.sh
```

## Output

- Intermediate ELF: `build-dreamcast/devilutionx.elf`
- Bootable output: `Packaging/dreamcast/devilutionx-playable.cdi`

## Notes

- `build.sh` deletes and recreates `build-dreamcast/`.
- Override tool paths with env vars if needed:
  - `KOS_BASE`
  - `KOS_ENV`
  - `MKDCDISC`

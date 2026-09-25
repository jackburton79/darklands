# OpenDarklands (open-source reimplementation)

A modern reimplementation of MicroProse's classic 1992 RPG **Darklands**,
built by reading and interpreting the original game's data files.

The goal is twofold:

1. **Reimplement** the game by parsing the original DOS data directly
   (catalogs, graphics, sounds, locations...) rather than requiring
   pre-converted assets.
2. **Improve** on the original — modern resolutions, quality-of-life
   features, and fixes for long-standing quirks of the 1992 release.

## Status

**Very early in development.** So far, the project can:

- Open the game's `.CAT` catalog (archive) files and list their contents
- Decode the game's custom `.PIC` image format, display the images and
  export them as BMP files
- Decode the enemy palette file (`ENEMYPAL.DAT`)
- Parse the world map (`DARKLAND.MAP`) and render it to a BMP file with
  the game's own map tiles and palette
- List the map locations (`DARKLAND.LOC`): cities, castles, villages,
  caves... with their map coordinates

Everything else — locations, sound, text, the game
itself — is not implemented yet. See the roadmap below.

## Screenshots

<!-- TODO: add a screenshot of the PIC viewer once it's stable -->

## Roadmap

- [x] Parse `.CAT` catalogs and decode `.PIC` images
- [x] Export decoded images (`--extract`)
- [x] Decode enemy palette files (`ENEMYPAL.DAT`)
- [x] Parse and render the world map (`DARKLAND.MAP`)
- [x] Decode the map icon sheets (`MAPICONS.PIC` / `MAPICON2.PIC`,
      PIC files with an embedded palette) for real map graphics
- [x] Solve the map tile column rule (transition/connection variants)
- [x] Parse `DARKLAND.LOC` (`--locations`)
- [ ] Label cities on the map (needs the game font, `FONTS.FNT`)
- [ ] Command-line map export at full resolution (`--map`, done;
      zoomable/pannable viewer)
- [ ] Decode more resource types (fonts, text, sound archives)
- [ ] City screens, text and dialogue display
- [ ] Sound playback
- [ ] Character creation, combat, the actual game loop

## Building

The external dependencies are **SDL2** and **zlib** (the latter is used by
libjgame), plus a C++11 compiler, `make` and `pkg-config`.

```sh
git clone --recurse-submodules https://github.com/jackburton79/darklands.git
cd darklands
make
```

If you already cloned without `--recurse-submodules`, fetch libjgame with
`git submodule update --init`.

Installing the dependencies, per platform:

```sh
# Debian/Ubuntu
sudo apt install build-essential pkg-config libsdl2-dev zlib1g-dev

# Fedora
sudo dnf install gcc-c++ make pkgconf-pkg-config SDL2-devel zlib-devel

# Haiku
pkgman install libsdl2_devel zlib_devel
```

## Running

You must supply your own copy of the original game data — this repository
contains none. *Darklands* is available for purchase on GOG.com.

The program currently expects the game data under `data/DARKLAND/`,
relative to the working directory (it loads `data/DARKLAND/ENEMYPAL.DAT`
at startup):

```sh
# Browse the images of a catalog (left/right arrow keys)
./darklands data/DARKLAND/PICS/EINFO.CAT

# Export every image of a catalog as BMP (the output directory must exist)
./darklands --extract data/DARKLAND/PICS/EINFO.CAT out/

# Render the world map to <prefix>.bmp (default prefix: map)
./darklands --map data/DARKLAND/DARKLAND.MAP [data dir] [output prefix]

# List the map locations (cities, castles, caves...) with coordinates
./darklands --locations data/DARKLAND/DARKLAND.LOC
```

## Project layout

```
darklands.cpp     Program entry point: image viewer, --extract, --map,
                  --locations
Catalog.*         Reader for the game's .CAT archive/catalog files
PICImage.*        Decoder for the game's .PIC image format
Palette.*         Reader for palette chunk files (ENEMYPAL.DAT)
MapFile.*         Reader for the world map (DARKLAND.MAP)
LocationFile.*    Reader for the map locations (DARKLAND.LOC)
docs/formats.md   Reverse-engineered data format notes
```

The repository also uses, as a git submodule, [libjgame](https://github.com/jackburton79/libjgame), a game library by the same author
that provides streams, graphics, and audio support (it has its own README
and license). It is built automatically by the top-level Makefile.

## Documentation

Reverse-engineered data format notes live in [docs/formats.md](docs/formats.md).

## Legal

This project contains no original game assets or code. It is an independent
reimplementation that reads data files from a copy of the game the user must
own. *Darklands* is a trademark of its respective owners; this project is not
affiliated with or endorsed by them.

## Acknowledgments

- MicroProse, for one of the most atmospheric RPGs ever made
- The Darklands fan community, for reverse-engineering efforts and
  documentation of the data formats

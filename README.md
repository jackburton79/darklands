````markdown
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
- Decode the game's custom `.PIC` image format and display the images

Everything else — maps, locations, sound, text, the game itself — is not
implemented yet. See the roadmap below.

## Screenshots

<!-- TODO: add a screenshot of the PIC viewer once it's stable -->

## Roadmap

- [ ] Command-line tools: list catalog contents, extract entries to disk
- [ ] Export decoded `.PIC` images to PNG (useful for testing and reference)
- [ ] Decode more resource types found in the catalogs (fonts, palettes, ...)
- [ ] Parse map and location data (`DARKLAND.MAP`, `DARKLAND.LOC`, `DARKLAND.CTY`)
- [ ] Render the main map / city screens
- [ ] Text and dialogue display
- [ ] Sound playback
- [ ] Character creation, combat, the actual game loop

## Building

The only external dependency is **zlib** (plus a C++ compiler and `make`).

```sh
git clone --recurse-submodules https://github.com/jackburton79/darklands.git
git 
cd darklands
make
```

Installing zlib, per platform:

```sh
# Debian/Ubuntu
sudo apt install build-essential zlib1g-dev

# Fedora
sudo dnf install gcc-c++ make zlib-devel

# Haiku
pkgman install zlib_devel
```

## Running

You must supply your own copy of the original game data — this repository
contains none. *Darklands* is available for purchase on GOG.com.

```sh
./darklands /path/to/DARKLAND/
```

## Project layout

```
darklands.cpp     Program entry point
Catalog.*         Reader for the game's .CAT archive/catalog files
PICImage.*        Decoder for the game's .PIC image format
```

The repository also use, as a git submodule, [libjgame](https://github.com/jackburton79/libjgame), a game library by the same author
that provides streams, graphics, and audio support (it has its own README
and license). It is built automatically by the top-level Makefile.

## Legal

This project contains no original game assets or code. It is an independent
reimplementation that reads data files from a copy of the game the user must
own. *Darklands* is a trademark of its respective owners; this project is not
affiliated with or endorsed by them.

## Acknowledgments

- MicroProse, for one of the most atmospheric RPGs ever made
- The Darklands fan community, for reverse-engineering efforts and
  documentation of the data formats
````

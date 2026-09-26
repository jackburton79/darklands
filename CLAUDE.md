# CLAUDE.md

Notes for Claude Code sessions on this repository: an open-source
reimplementation of MicroProse's *Darklands* (1992), built by reverse
engineering the original DOS data files. See `README.md` for the user-facing
overview and `docs/formats.md` for everything known about the file formats.

## Working with the maintainer

- The maintainer writes in Italian: answer in Italian. Code, comments,
  commit messages, PR descriptions and docs are in English.
- One pull request per step; the maintainer merges them. **Before pushing
  more work, check whether the branch's PR is already merged** (it has
  happened that a PR was merged while commits were still being added, and
  those commits never reached `master`). If it is merged, restart the branch
  from the latest `master`.
- Only open a PR when asked.

## Building

```sh
git submodule update --init    # libjgame, the maintainer's game library
make                           # needs SDL2 (pkg-config sdl2) and zlib
```

- In a fresh cloud container SDL2 is usually missing:
  `apt-get install -y libsdl2-dev zlib1g-dev`.
- The Makefile tracks header dependencies (`-MMD -MP`). If objects ever look
  stale (odd crashes after changing a class layout), `make clean && make`.
- New `.cpp` files must be added to `SOURCES` in the `Makefile`.

## Game data

- The original game files are **never** committed (copyright). They live in
  `data/DARKLAND/` (ignored by git), with images in `data/DARKLAND/PICS/`.
  The maintainer uploads the files needed for a task; copy them there.
- `GameData` opens files by their game name from the data directory, then
  from `PICS/`. The program takes `--data <dir>` (default `data/DARKLAND`).

## Running

```sh
./darklands [--start Köln]           # the game (Game): city cards + map
./darklands EINFO.CAT                # browse a catalog's images
./darklands --extract EINFO.CAT out/ # export them as BMP (out/ must exist)
./darklands --map [prefix]           # full map render, with city names
./darklands --locations              # dump DARKLAND.LOC
./darklands --cities                 # dump DARKLAND.CTY
./darklands --messages [PARTY02]     # list MSGFILES / dump a card deck
./darklands --card PARTY02 0 Köln    # show a card (CardView)
```

## Code layout

- Readers, one per format: `Catalog` (.CAT), `PICImage` (.PIC, chunked:
  `M0` palette + `X0` image), `Palette` (ENEMYPAL.DAT, `NearestColor()`),
  `MapFile`, `LocationFile`, `CityFile`, `FontFile`, `MsgFile` (.MSG
  card decks, read from the MSGFILES catalog via `GameData::Messages()`),
  `DescriptionFile` (DARKLAND.DSC, the `$PlaceDesc` of each city).
- `WorldMap`: map tiles + icon sheets + palette; tile geometry, the column
  rule, `Draw(bitmap, origin)` for any part of the map, `TileAtPixel()`.
- `GameData`: lazy access to all game files. `TextSupport` (`Font`): text
  with the game fonts, `ToGameCharset()` for UTF-8 input.
- `MapViewer`: the interactive map (320x200 8-bit buffer, scaled 2x);
  `Travel`: A* paths on the map; `CityLabels`: city names over the map.
- `Game`: runs `CityVisit` and `MapViewer` in turn, in one `GameWindow`.
  `CityVisit`: the table of city screens (deck, card, scene, what each
  option does, which options need a place). `MapViewer::Run()` returns
  when the party reaches a city.
- `CardView`: a .MSG card on screen (frame, capital, text, options),
  same structure as `MapViewer`. `ScreenSupport`: `GameWindow` (shows a
  320x200 8-bit buffer) and the mouse cursor, shared by both.
- `darklands.cpp`: command line only.

## Code style

Follow the existing files (Haiku-like style):

- 4-space indentation in this repo (libjgame uses tabs; don't touch it
  unless asked). Return type on its own line in definitions, two blank lines
  between functions.
- Members `fName`, private methods `_Name()`, constants `kName`, file-local
  helpers `static`.
- Readers throw `std::runtime_error` on invalid data and validate every
  offset/length against the file size before reading.
- Keep comments short and factual, as in the existing code.

## Pitfalls found so far

- `GFX::Palette` (libjgame) has an empty user-provided constructor:
  `GFX::Palette p = {};` does **not** clear it. Always
  `memset(p.colors, 0, sizeof(p.colors))`.
- `GFX::point` / `GFX::rect` coordinates are 16-bit: do arithmetic and
  clamping in `int` before storing.
- libjgame's `GraphicsEngine` hides the system cursor (the viewer draws
  its own), forces "linear" scaling, and ties the window size to the
  logical size. `SDL_SoftStretch` (`BlitBitmapScaled`) needs source and
  destination in the same pixel format: convert 8-bit to 32-bit first.
- Blitting between 8-bit bitmaps with different palettes may remap
  colors: to compose screens, copy raw indices (`PICImage::RawBytes()`).
- With `SDL_VIDEODRIVER=dummy`, SDL sends a mouse motion to the window
  center at startup: tests that count keypresses must allow for it.
- String fields in the game files are in the game's character set:
  `0x1F [ \ ] _ { |` = `ä Ä Ö Ü ß ö ü` (`FONTS.UTL` has `ë` at `_`).
  Bytes after a string's NUL are garbage, not data.

## Reverse engineering and docs

- `docs/formats.md` is the reference. Mark every fact **verified** (checked
  against the game data, e.g. sizes that add up exactly, or a render that
  is visibly right) or *inferred*; keep the "Open questions" list current.
- Prototype decoders in Python in the scratchpad first (PIL is available
  after `pip install pillow`), then port to C++ and compare outputs.
- Look for numbers that must add up (file size = count x record size,
  offsets that line up) and cross-check files against each other
  (`DARKLAND.CTY` record *i* is `DARKLAND.LOC` location *i*).

## Verifying changes

There are no unit tests yet; verify by comparing outputs.

- Before a refactor, save the current outputs (`--map`, `--locations`,
  `--cities`, `--messages <name>`, `--extract EINFO.CAT`) and `cmp` them
  afterwards: they should be byte-identical unless the change is meant to alter them.
- `einfo_cat_reference.md5`: `md5sum -c` over the `--extract EINFO.CAT`
  output must pass (60/60).
- There is no display in the cloud container. Test `MapViewer` by driving
  its public methods (`Clicked`, `Tick`, `Escape`, `Draw()` ...) from a small
  program in the scratchpad and looking at the saved buffer, and test
  `Run()` with `SDL_VIDEODRIVER=dummy` plus events pushed from an SDL timer.
  Say clearly what could not be tested on a real screen.

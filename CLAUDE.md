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
- The Makefile builds every `.cpp` under `src/` (objects go to `obj/`);
  a new file only needs to be in the right folder.

## Game data

- The original game files are **never** committed (copyright). They live in
  `data/DARKLAND/` (ignored by git), with images in `data/DARKLAND/PICS/`.
  The maintainer uploads the files needed for a task; copy them there.
- `GameData` opens files by their game name from the data directory, then
  from `PICS/`. The program takes `--data <dir>` (default `data/DARKLAND`).

## Running

```sh
./darklands [--start Köln]           # the game (Game): city cards + map
./darklands --load DKSAVE1.SAV       # the game, from a saved game
./darklands EINFO.CAT                # browse a catalog's images
./darklands --extract EINFO.CAT out/ # export them as BMP (out/ must exist)
./darklands --extract E00C.CAT out/  # battle sprites, as sheets
./darklands --map [prefix]           # full map render, with city names
./darklands --locations              # dump DARKLAND.LOC
./darklands --cities                 # dump DARKLAND.CTY
./darklands --enemies                # dump DARKLAND.ENM
./darklands --saints                 # the saints' rules (DARKLAND.EXE)
./darklands --events [DKSAVE0.SAV]   # the game's events (EVENTS.TMP)
./darklands --battlemap ICITY.000    # a battlefield map, as text
./darklands --battle IWILDGEN.101 M03 DKSAVE0.SAV  # seen from above,
                                     # with the party and 4 skeletons
./darklands --messages [PARTY02]     # list MSGFILES / dump a card deck
./darklands --card PARTY02 0 Köln    # show a card (CardView)
```

## Code layout

The sources are in `src/`: `src/formats/` the readers of the game's
files (no SDL), `src/game/` the game state and rules (`GameData`,
`Character`, `GameTime`, `Travel`, `BattlePath`, `Combat`), `src/ui/` the screens and drawing,
`src/darklands.cpp` the command line. The folders are all on the
include path: include headers by name (`#include "CityFile.h"`).

- Readers, one per format (`src/formats/`): `Catalog` (.CAT), `PICImage` (.PIC, chunked:
  `M0` palette + `X0` image), `Palette` (ENEMYPAL.DAT, `NearestColor()`),
  `MapFile`, `LocationFile`, `CityFile`, `FontFile`, `MsgFile` (.MSG
  card decks, read from the MSGFILES catalog via `GameData::Messages()`),
  `DescriptionFile` (DARKLAND.DSC, the `$PlaceDesc` of each city),
  `EnemyFile` (DARKLAND.ENM, the enemy types and enemies),
  `ImcFile` (the battle sprites, `GameData::SpritePalette()` for their
  colors), `ImgFile` (BATTLEGR.IMG, COMMONSP.IMG; both use `Sprite`),
  `BattleMap` (the IMAPS.CAT maps; it and `ImcFile` use `Lzexe`),
  `ExeData` (the people's names, the jobs, the weapon table, read
  from DARKLAND.EXE),
  `CharacterFile` (CHARACTR.TMP) and `SaveFile` (SAVES/*.SAV), both made
  of `Character` records (554 bytes) and giving a `party`; `SaveFile`
  also writes a saved game (`Game::Save()`, Ctrl+S) over the bytes of
  the one read, and `WriteCharacter()` keeps each record's unknown
  bytes;
  `EventFile` (EVENTS.TMP, the saves' events too: `world_event`).
- `WorldMap`: map tiles + icon sheets + palette; tile geometry, the column
  rule, `Draw(bitmap, origin)` for any part of the map, `TileAtPixel()`.
- `GameData`: lazy access to all game files. `TextSupport` (`Font`): text
  with the game fonts, `ToGameCharset()` for UTF-8 input.
- `MapViewer`: the interactive map (320x200 8-bit buffer, scaled 2x);
  `Travel`: A* paths on the map; `CityLabels`: city names over the map.
- `Game`: runs `CityVisit` and `MapViewer` in turn, in one `GameWindow`,
  with one `GameTime` clock (traveling advances it by whole hours, as
  DARKLAND.EXE does, see `TravelHours()`; some options too;
  `CityVisit` has night variants of its screens, `kNightScreens`).
  `CityVisit`: the tables of city screens, by day and by night (deck,
  card, scene, what each option does, which options need a place); a
  constructor check catches a miscounted table. The class is split by
  subject over several files: `CityVisit.cpp` (running, choosing an
  option, the card shown, the hidden options), `CityScreens.cpp` (the
  tables and their macros; `CityVisitInternal.h` has what the files
  share: the option actions and the conditions' constants), and
  `CityChurch` (church, monastery, saints), `CityServices` (inn, banks,
  physician, alchemist), `CityGate` (night market, gate, walls),
  `CityGuards` (the watch, guards, chases), `CityPrison` (dungeon,
  court, execution, the priest), `CityWorld` (news, events, places),
  `CityQuests` (the banks' tasks, the robber knight's tower) and
  `CityEncounters` (the slum, thieves, the grove). A new screen goes in
  the enum, in both tables and in the file of its subject. `CardView` hides the
  cards' placeholder options itself. `MapViewer::Run()` returns
  when the party reaches a place of DARKLAND.LOC; `CityVisit` also
  runs the other places (the castles' robber knight's tower) from
  SCREEN_OUTSIDE, by the location's enter state.
- `CardView`: a .MSG card on screen (frame, capital, text, options),
  same structure as `MapViewer`. `PartySidebar`: the character boxes.
  `InfoView`: the F6 party and F1..F5 character screens, opened from
  `CardView`, `MapViewer`, `ResidenceView` and `TradeView` (modal
  `Run(window, page)`).
- `BattleView`: a provisional top view of a battlefield map (cells,
  walls, objects) and figures (party and enemies, with their sprites
  and colors; members walk where one clicks, `BattlePath`, with their
  walking animation; the enemies walk up to the nearest member; the
  figures next to a foe fight it, `Combat`; `AfterBattle()` then
  applies the wounds and the deaths), until the game's own drawing is
  decoded.
- `ListFile` (DARKLAND.LST): item definitions; a character's item code
  indexes it, its equipment slots hold item *types*; `item_flag` for
  the categories.
- `ResidenceView`: living at the inn (CAMPCITY.PIC), the members'
  activities and days; `CityVisit` opens it like the trade screen.
- `TradeView`: the item exchange scrolls (BUYSELL.PIC); `CityVisit`
  opens it for `ACTION_TRADE` options (and for a battle's loot,
  `SetLoot()`), from `Run()` (`PendingTrade()`
  in step-by-step tests). Stock and prices follow DARKLAND.EXE. The
  church's options (`ACTION_MASS`, `ACTION_CONFESSION`,
  `ACTION_DONATION`) change the party and the clock in `CityVisit`, as
  DARKLAND.EXE does (docs/exe.md, "The church"). `ScreenSupport`: `GameWindow` (shows a
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

- `docs/exe.md`: DARKLAND.EXE is a Microsoft C 6 program with RTLink
  overlays, code uncompressed. It explains how to map `seg:off` to file
  offsets and how to find the code that uses a string; `ndisasm -b 16`
  is installed. Cite the functions (`18E7:355E`) when porting a rule.

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

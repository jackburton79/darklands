# Darklands data format notes

Working notes from reverse engineering the original game's data files,
as implemented in this project. Corrections and additions are welcome —
please open an issue or a pull request.

References:

- The executable: see [exe.md](exe.md) for its structure and the rules
  decoded from it.
- File formats overview: <https://wendigo.online-siesta.com/darklands/file_formats/up-to-date/>
- PIC decompression algorithm: <https://github.com/ogamespec/PicDecoder>

Conventions:

- Multi-byte integers are **little-endian**, with the exception of the
  `DARKLAND.MAP` header, whose dimension words are **big-endian**
  (see the world map section).
- Offsets are hexadecimal, relative to the start of the file/resource.
- Facts marked **verified** were confirmed against original game data
  (all 60 entries of `EINFO.CAT`, `ENEMYPAL.DAT`, `BKGNDPAL.DAT`,
  `MAPICONS.PIC`/`MAPICON2.PIC`, all 931 rows of `DARKLAND.MAP`) and by
  visual comparison with rendered output.
  Anything else is marked *inferred* or *unverified*.

## Catalog archives (`.CAT`)

Catalogs bundle related resources (images, sounds, text) into one file.
`EINFO.CAT`, for example, holds the 60 bestiary portraits.

    offset  size  description
    0x00    2     entry count (N)
    0x02    24·N  entry table, 24 bytes per entry:

    entry (relative offsets):
    +0x00   12    resource name: DOS 8.3 style, up to 12 characters,
                  padded with spaces (strip trailing spaces)
    +0x0C   4     timestamp (uint32): DOS FAT packed date/time,
                  date in the high word, time in the low word
    +0x10   4     data length in bytes (uint32)
    +0x14   4     data offset (uint32; absolute, from start of the .CAT file)

- Timestamp: date = `(year − 1980) << 9 | month << 5 | day`,
  time = `hour << 11 | minute << 5 | second / 2`. **verified**: all 60
  entries of `EINFO.CAT` decode to plausible dates between 1992-05-14
  and 1992-07-09 (e.g. `0x18AEA417` = 1992-05-14 20:32:46).
- Resource data follows the entry table; each blob is located with its
  entry's offset/length pair.
- Offsets and lengths must be validated against the file size before use:
  a corrupt catalog must never produce out-of-bounds reads.

## PIC images (`.PIC`)

Palettized bitmap images, compressed with a two-stage scheme:
an LZW-like adaptive code followed by run-length encoding.
Decoding is *output-driven*: exactly `width × height` bytes are produced;
any unread bytes after the declared data end are ignored.

### Chunk structure

A PIC file is a **sequence of chunks**, each one:

    offset  size  description
    0x00    2     tag: two ASCII characters
    0x02    2     length (uint16): number of data bytes that follow
    0x04    ...   data

Known chunks (**verified** on `MAPICONS.PIC`/`MAPICON2.PIC` and on all
60 entries of `EINFO.CAT`):

- `M0` — **palette** (optional; precedes the image chunk):

      +0x04   1     first palette index
      +0x05   1     last palette index
      +0x06   3·n   n = last − first + 1 RGB triplets, 6-bit components

  Both map icon sheets carry a full palette (`00`..`FF`, length
  770 = 2 + 768). The bestiary portraits in `EINFO.CAT` have none.
- `X0` / `X1` — **image**, described below. The "M0 format" previously
  listed as an open question for the map icon sheets is just an `M0`
  chunk followed by an ordinary `X0` image chunk.

### Image chunk header (10 bytes)

Offsets relative to the start of the chunk.

    offset  size  description
    0x00    2     magic: low byte always 'X' (0x58);
                  high byte '0' (0x30) or '1' (0x31) — see "BCD packing"
    0x02    2     chunk length (uint16): number of bytes from 0x04 to the
                  end of the image data. For a file holding only the image
                  chunk, file size == this value + 4.
                  **verified** across all 60 entries of EINFO.CAT.
                  (A uint16 implies an image chunk is at most 65539 bytes.)
    0x04    2     width in pixels (uint16)
    0x06    2     height in pixels (uint16)
    0x08    2     "magic word" (uint16): low byte = maximum LZW code length
                  in bits, clamped to 11; high byte = initial bit buffer
                  content (always 0x00 in the files seen so far)

The compressed bitstream starts at `0x0A` and is
`length − 6` bytes long (everything up to the end of the chunk).

### Stage 1 — LZW-like adaptive compression

As reverse engineered in ogamespec/PicDecoder; see `PICImage.cpp` for a
reference implementation.

- The decoder keeps a translation table (LUT) of `2^maxCodeLength`
  entries, 3 bytes each: a 2-byte "next entry" link (0xFFFF = end of
  chain) and a 1-byte output value.
- Initial state: all links 0xFFFF; entries 0..255 hold output value = index.
- Codes start at **9 bits** and grow by one bit each time the dictionary
  fills past the current power of two. The code length never exceeds the
  header's maximum (byte at 0x08, low byte, clamped to 11); when it
  would, the table is **reset** to its initial state and the code length
  returns to 9 bits.
- The bitstream is consumed as 16-bit little-endian words.
- Otherwise standard LZW behavior:
  - Code == next unused dictionary slot is the KwKwK case: the output is
    the string for the previously emitted code followed by its own
    first byte.
  - Strings are emitted by walking the entry chain and pushing output
    values onto a stack, then popping them (strings come out back-to-front).
- **Odd-length bitstreams**: if the compressed data has an odd number of
  bytes, the final 16-bit word consists of a single byte, which is the
  **low** byte of that word (the high byte is implicitly zero).
  **verified** visually against the original game.

### Stage 2 — run-length encoding

Applied to the byte stream produced by stage 1. RLE state (last byte,
pending repeat count) **persists across scanlines**: an image decodes as
one continuous stream of `width × height` bytes, not per line.

- Byte `0x90` is the escape:
  - `0x90 0x00` — a literal `0x90` (which also counts as "the previous
    byte" for any following run).
  - `0x90 n`, n ≥ 2 — extends the previous byte's run: it appears
    **n times in total** in the output, original occurrence included.
- `0x90 0x01` is degenerate ("previous byte once" needs no encoding) and
  would loop forever in the reference decoder (repeat count underflows).
  Valid files presumably never contain it; encoder implementations must
  not emit it.

### BCD packing (inferred, unverified)

All files examined so far use magic high byte `'0'`. If bit 0 of the
high byte is set (`'1'`), decoded bytes are packed two pixels per byte:
low nibble = even (left) pixel, high nibble = odd pixel; the decoder
consumes `(width + 1) / 2` bytes per line. No sample with this flag has
been found yet.

### Palette

- Images are 8-bit **indexed**. The indices are full 8-bit palette
  indices — they are **not** masked to 0..15 and do **not** refer to the
  standard EGA palette. (An earlier revision of this document — and the
  original viewer, whose `% 16` masked the problem — incorrectly assumed
  16-color EGA indexing; the wrong assumption produces plausible-looking
  but wrongly colored images.)
- The colors come either from the file's own `M0` chunk (see "Chunk
  structure") or from **external palette files** — e.g. enemy
  graphics index into the 256-color palette assembled from
  `ENEMYPAL.DAT` (see next section). Observed index ranges in one
  sprite: 0, 33–44, 72–73; each range falls inside a palette chunk's
  16-entry slice.
- Index 0 is most likely the **transparency/color key**: it typically
  accounts for ~2/3 of a sprite's pixels (the uniform background).
  *inferred* for sprites; **verified** for the map icon sheets (drawing
  tiles with index 0 transparent produces a seamless map).

## Palette chunk files (`ENEMYPAL.DAT`)

Arrays of small palette chunks, each patching a 16-entry slice of a
shared 256-color palette. See `Palette.cpp` for a reference
implementation.

    chunk (relative offsets), stride 53 bytes:
    +0x00   1     start offset (byte): divide by 3 to obtain the palette
                  index of the chunk's first entry
                  (observed: 0x60 -> index 32, 0xC0 -> index 64)
    +0x01   48    16 RGB triplets; components are 6-bit (0..63)
    +0x31   4     unknown purpose

- **File size: 3763 = 71 × 53** (**verified**; both factors prime, so the
  factorization is unique). The chunk count 71 = 0x47 matches the public
  format reference. Note that the same reference states a 52-byte chunk
  (16 triplets + 3 tail bytes) — the file size only works with a
  53-byte stride, i.e. a **4-byte** trailing field.
- 6-bit components are standard VGA DAC values; scale to 8 bits with
  `(value << 2) | (value >> 4)` (0 -> 0, 63 -> 255).
- Chunk slices match observed image indices: one sample sprite used
  indices 33–44 and 72–73, covered by chunks starting at index 32
  (`0x60`) and 64 (`0xC0`) respectively. **verified** for one sample.
- Applying **all** chunks in file order produces plausible enemy
  graphics for the whole bestiary catalog; multiple chunks may claim the
  same palette range with different colors, so per-enemy chunk selection
  must happen elsewhere: in the palette fields of the enemy types of
  `DARKLAND.ENM` (see "Enemies"). **verified**
- `BKGNDPAL.DAT` does **not** follow this layout — see next section.

## Background palettes (`BKGNDPAL.DAT`)

2343 bytes = 3 · 11 · 71: **11 palettes of 71 colors each**, plain RGB
triplets with 6-bit components, no header and no index byte.
The battles copy one of them to palette indices 164..234 (see "Battle
sprites", Colors). **verified** (code)

    palette k (k = 0..10) at offset 213 · k:
    +0x00   3·71  71 RGB triplets, 6-bit components

- **verified**: every byte is ≤ 0x3F (so there are no offset/index
  bytes); the same color sequences recur at a 213-byte (71-color) period,
  and most entries are identical or near-identical across the 11 palettes.
- Entries 12..59 hold six 8-step color ramps (yellow, green, red,
  blue-grey, grey, brown); entry 60 (`00 00 0B`) is the same in all 11.
- `3F 00 3F` (magenta) and runs of `3F 3F 3F` (white) look like
  placeholders for slots a given palette leaves unused. *inferred*
- Not the map palette: no palette matches any 71-entry range of the
  palette embedded in the map icon sheets. What selects one of the 11
  (location type? time of day?) is unresolved.

## World map (`DARKLAND.MAP`)

The wilderness map of the Holy Roman Empire: a grid of 327 × 931 tiles,
stored RLE-compressed, one stream per row. See `MapFile.cpp` for a
reference implementation.

**Mixed endianness — the trap.** The two dimension words at the start of
the file are **big-endian**, but the row offset table that follows them is
**little-endian**. Both differ from the rest of the game's formats
(catalogs and PIC images are little-endian throughout). Getting this wrong
produces offsets that look almost plausible and then dissolve into garbage;
validate `rows[0] == end of table` before trusting anything else.

    offset  size       description
    0x00    2          max_x = 0x0147 (327), word, **big-endian**
    0x02    2          max_y = 0x03A3 (931), word, **big-endian**
    0x04    4 · max_y  row_offsets[max_y], dwords, **little-endian**:
                       file offset of each row's RLE data
    0x0E90  ...        row data; the first row begins immediately after
                       the table (**verified**: rows[0] == 0x04 + 4 · max_y)

Row sizes vary (observed: 60..210 bytes, average ~155; the theoretical
RLE bounds for a 327-tile row are 47..327). The last row's length is
implicit: it extends to the end of the file.

### Row encoding (RLE)

Each row is a stream of bytes:

    bit:   7 6 5   4   3 2 1 0
           R R R   P   T T T T

- bits 7..5 (`R`): repeat count, values 1..7 — a count of 0 is invalid
  (**verified**: not a single such byte occurs in the entire file)
- bit 4 (`P`): palette set: 0 = `MAPICONS.PIC`, 1 = `MAPICON2.PIC`
- bits 3..0 (`T`): tile row within that icon sheet

Each byte expands to `repeat` identical tiles. **Every row decodes to
exactly max_x = 327 tiles** (**verified** for all 931 rows; 304,437 tiles
total, exactly max_x · max_y).

### Icon sheets (`PICS/MAPICONS.PIC`, `PICS/MAPICON2.PIC`)

Regular PIC files: an `M0` chunk with the full 256-color map palette,
then a 320 × 200 `X0` image (**verified**; the two palettes are
identical). They are developer atlases:

- a grid of 16 columns × 16 rows of **16 × 12 pixel cells** in the
  top-left 256 × 192 pixels;
- text labels to the right of the grid (terrain name per row) and column
  numbers 0..15 below it — not tile data.

Row labels as written on the sheets (sheet 1 numbers its rows 16..31):

| Row | Sheet 0 (`P` = 0)       | Row | Sheet 1 (`P` = 1)                  |
|-----|-------------------------|-----|------------------------------------|
| 0   | `plains` (empty cells)  | 16  | `Frst2`                            |
| 1   | `ocean`                 | 17  | `Frst3`                            |
| 2   | `MjRvr` (major river)   | 18  | `Rck0`                             |
| 3   | `MnRvr` (minor river)   | 19  | `Rck1`                             |
| 4   | `TdlMrs` (tidal marsh)  | 20  | `Rck2`                             |
| 5   | `Marsh`                 | 21  | `Rck3`                             |
| 6   | `Geest`                 | 22  | `Alp4`                             |
| 7   | `Gst1`                  | 23  | `Alp5`                             |
| 8   | `Farm0`                 | 24  | *(unlabeled; road pieces)*         |
| 9   | `Farm1`                 | 25  | `Ford`                             |
| 10  | `F/W0`                  | 26  | *(unlabeled; river pieces)*        |
| 11  | `F/W1`                  | 27  | `Brdge`                            |
| 12  | `LtW1`                  | 28  | `Cstle` (castles, other landmarks) |
| 13  | `LtW2`                  | 29  | `Cty`                              |
| 14  | `Frst0`                 | 30  | *(unlabeled; blue flags)*          |
| 15  | `Frst1`                 | 31  | *(unlabeled; red flags)*           |

Within a cell, ground tiles are "lens" shapes about 16 × 8 pixels in the
lower part of the cell; tall features (trees, mountains, buildings) rise
into the upper part.

### Tile geometry — **verified by rendering**

- Cells are **16 × 12** pixels; pixel value 0 is transparent.
- Odd rows are drawn shifted right by 8 px (half a tile).
- Consecutive rows are only **4 px** apart, so tiles overlap: draw rows
  top to bottom so that lower rows cover the upper parts of the rows
  behind them.
- With this geometry the 16 × 8 ground lenses tile seamlessly. Rendered
  map: `327 · 16 + 8` × `930 · 4 + 12` = 5240 × 3732 pixels.
- **A full-map render using only sheet column 0 produces a correct map of
  the Holy Roman Empire with real graphics**: North Sea, Baltic, Jutland,
  the Rhine and Elbe, the Alps, all with plausible proportions.
  (Rendered by `darklands --map`.)
- An earlier revision of this document stated 16 × 16 tiles and an 8 px
  row step; that was inferred from a synthetic-color render and produces
  a map stretched vertically by 2×.

### What the tile byte means — structural finding

Since column-0 rendering yields sensible terrain regions and continuous
road networks, the byte's palette bit and tile-row nibble must encode the
tile **type** (ocean, plain, forest, road, ...), while the sheet
**column** — which does *not* come from the map file at all — selects the
connection/transition variant of that type (e.g. which diagonals a road
piece links, which side of a coast tile is water).

### Tile column rule — **solved**

The sheet column is a **4-bit mask of the diagonal neighbors the tile
joins**:

    bit 0 (1)  NW neighbor      even row y: (x−1, y−1)   odd row y: (x,   y−1)
    bit 1 (2)  NE neighbor      even row y: (x,   y−1)   odd row y: (x+1, y−1)
    bit 2 (4)  SW neighbor      even row y: (x−1, y+1)   odd row y: (x,   y+1)
    bit 3 (8)  SE neighbor      even row y: (x,   y+1)   odd row y: (x+1, y+1)

(odd rows are the ones shifted right by half a tile). Horizontal and
vertical neighbors play no part.

How it was found: the road row of `MAPICON2.PIC` (type 24) shows, for
each column, which cell edges the road leaves from. Every multi-exit
column matches the mask exactly (3 = NW+NE, 5 = NW+SW, 6 = NE+SW,
9 = NW+SE, 10 = NE+SE, 12 = SW+SE, 7 = NW+NE+SW, 11 = NW+NE+SE,
13 = NW+SW+SE, 14 = NE+SW+SE, 15 = all four). **verified** by pixel
inspection of the sheet. The remaining columns are the horizontal
cases: 0 is a straight W–E road, and 1/2/4/8 join their single diagonal
to the opposite horizontal side (1 = NW+E, 2 = NE+W, 4 = SW+E,
8 = SE+W): a horizontal run of road tiles has no diagonal road
neighbors. The map data agrees: road and river tiles have on average
~1.6–1.9 same-type diagonal neighbors but only ~0.2–0.4 same-type
horizontal ones, i.e. they form diagonal chains.

What "joins" means:

- tiles of the **same type** always join (**verified** visually: with
  this rule coastlines, lakes and terrain regions get smooth outlines);
- roads (24) also join fords (25), bridges (27) and cities (29);
  rivers (2, 3) also join each other, fords, bridges and the ocean (1).
  Fords and bridges never have a same-type neighbor, so without these
  groups roads and rivers would break at every crossing. *inferred*
  (the render shows continuous roads and rivers, but the exact groups
  are not proven: e.g. whether roads join castles or rivers join tidal
  marshes);
- off-map neighbors are treated as joined. *inferred*

wendigo's recipe used the same bit positions but derived each bit from
bits of the neighbor's *row number* instead of from its type, which is
why it produced broken rivers and roads.

Rows never used by the map data: 26, 28, 30, 31 (and 0, "plains", only 5
tiles). Castles, flags and the like are presumably drawn by the game on
top of the map from other data (e.g. `DARKLAND.LOC`).

## Locations (`DARKLAND.LOC`)

The places on the world map: cities, castles, villages, monasteries,
caves, shrines, lairs... See `LocationFile.cpp` for a reference
implementation.

    offset  size    description
    0x00    2       location count N (uint16) — 414 in the game data
    0x02    58·N    records, 58 bytes each

- **File size: 24014 = 2 + 414 · 58** (**verified**).

Record (relative offsets, little-endian):

    +0x00   2     type (uint16), see below
    +0x02   2     unknown (uint16; 0 for cities, 8..14 for most others —
                  perhaps a map icon)
    +0x04   2     x: map tile column (DARKLAND.MAP coordinates)
    +0x06   2     y: map tile row
    +0x08   2     unknown (uint16; values 1, 5, 9, 10)
    +0x0A   2     unknown, varies
    +0x0C   2     the game's state on arriving there: 4 for the
                  cities (before the walls), 0x93 the castles (the
                  robber knight's tower), 0x85 type 8, 0xFA caves, 0x100
                  tombs...; the saved games may change it (**verified**:
                  DARKLAND.EXE returns it to its state machine, file
                  0x5E6C3)
    +0x0E   2     the state of its territory, met on the way: 0x92 the
                  castles, else 0x62 (none) (**verified**: file
                  0x5E6A5, 0x5F139)
    +0x10   1     unknown
    +0x11   1     cities: size, 3..8; other locations: 1 (see below)
    +0x12   20    unknown; constant in the game data except +0x1C
                  (0x19 0x19 0x19 at +0x15, 0xFF 0xFF at +0x18)
    +0x26   20    name, NUL-padded

- **Coordinates** — **verified**: 86 of the 92 cities (type 0) sit
  exactly on a city map tile (type 29); the other 6 (e.g. Berlin,
  Kassel, Frankfurt) are right next to one — large cities span several
  tiles. One cave has y = 931, one past the last map row.
- **Types**, from the names (*inferred*): 0 city (92), 1 and 8 mostly
  villages (73 and 112), 2 castles (18: DARKLAND.EXE looks for the
  nearest location of type 2 for a robber knight, and all enter state
  0x93, the robber knight's tower: **verified**), 3 monasteries/abbeys?,
  5 and 19 "Cave", 13 "Tomb", 15 "Lair", 16 "Spring", 17 "Lake",
  18 "Shrine", 20 "Pagan Altar", 21 and up named special sites
  (e.g. "Brocken", "Hochk{nig"). The exact meaning of types 1..4, 6
  and 8 is unresolved.
- **City size** at +0x11 — *inferred*: Köln is the only 8; Hamburg,
  Lübeck, Nürnberg, Ulm, Strassburg and Danzig are 7; small towns such
  as Groningen and Flensburg are 3. Every non-city has 1.
- **Name character set**: the game's character set (see "Character
  set" below): `L|beck` = Lübeck, `K{ln` = Köln, `J\x1Fgerndorf` =
  Jägerndorf.

## Cities (`DARKLAND.CTY`)

Details of the 92 cities. See `CityFile.cpp` for a reference
implementation.

    offset  size     description
    0x00    1        city count N (byte) — 92
    0x01    622·N    records, 622 bytes each

- **File size: 57225 = 1 + 92 · 622** (**verified**).
- **Record i describes location i of `DARKLAND.LOC`** (the cities are
  its first 92 entries): **verified** — the size matches for all 92
  and the coordinates for 91 (Strassburg is one tile off).
- All strings are **32-byte fields**, NUL-terminated, in the game's
  character set. The bytes after the NUL are garbage (fragments of x86
  code: leftover memory from the tool that wrote the file). A lone
  space means "none".

Record (relative offsets, little-endian):

    +0x00   32    short name, e.g. "Frankfurt"
    +0x20   32    full name, e.g. "Frankfurt am Main" (DARKLAND.LOC
                  abbreviates long names to fit 20 bytes: "Frankfurt M")
    +0x40   2     size, 3..8 (same as DARKLAND.LOC +0x11)
    +0x42   2     x: map tile column (same as DARKLAND.LOC)
    +0x44   2     y: map tile row
    +0x46   2     x2 \  a map tile close to the city (at most 3 columns
    +0x48   2     y2 /   and 8 rows away — rows are only 4 px apart);
                         purpose unknown — entrance? docks?
    +0x4A   2·4   neighboring cities: indices into this file,
                  0xFFFF = unused
    +0x52   2     harbor: 0 = North Sea port, 1 = Baltic port,
                  0xFFFF = inland
    +0x54   2     4 in every record
    +0x56   2     the seed of the city's people (**verified**: DARKLAND.EXE
                  adds a global to it for their names and skills, see
                  exe.md); equals the record index or index + 1
    +0x58   2     who rules it: 0 its ruler's seat (a capital), 1 ruled
                  for its lord, 2 a free city (**verified**: DARKLAND.EXE
                  picks the approach's card and $CityLordTitle by it;
                  the values fit: Schleswig, München 0, Flensburg 1,
                  Hamburg, Lübeck 2)
    +0x5A   2     the same while the location's flags have bit 0x80
                  (a war? *inferred*); values 0..3
    +0x5C   2     unknown (small values)
    +0x5E   2     flags: 0x200 = the market has a Leihhaus (**verified**:
                  DARKLAND.EXE 0E76:1A8E reads it, 57 cities); the
                  other bits are read too, not decoded
    +0x60   2     unknown
    +0x62   9     the quality of the city's shops, 0 if it has none:
                  blacksmith, goods merchant, swordsmith, armorer,
                  gunsmith, bowyer, artificer, jeweler, clothmaker
    +0x6B   1     unknown
    +0x6C   2     a day of the year (0-based), 42 in 61 of the 92 cities: the
                  city's feast (*inferred*); DARKLAND.EXE 0E76:1A8E
                  (property 0x23) is true within 14 days of it, counting
                  from the first of the current month, which brings the
                  shell game man to the square and the market
                  (**verified**: code)
    +0x6E   32·16 names of the city's places, by slot (see below)

- **Neighbors** — **verified** as a network of nearby cities: of 186
  links, 176 are symmetric; linked cities are a median 42 tiles apart,
  against 189 for arbitrary pairs (e.g. Hamburg: Lüneburg, Brandenburg,
  Magdeburg, Bremen; Köln: Duisburg, Koblenz). Some cities have none.
- **Shops** — **verified**: DARKLAND.EXE takes a shop's quality from
  +0x62 + its type (see exe.md). Dortmund, "famous for its gunsmiths",
  has 43 for the gunsmith; Köln has no blacksmith, Groningen no
  swordsmith, armorer or gunsmith.
- **Harbor** — **verified** from the names: 0 = Groningen, Hamburg,
  Bremen, Leer, Zwolle, Elburg; 1 = Flensburg, Vordingborg, Nakskov,
  Schleswig, Lübeck, Wismar, Rostock, Stralsund, Stettin, Danzig.

Place slots (roles inferred from the names across all 92 cities; the
counts say how many cities leave the slot empty):

| Slot | Role | Examples | Empty |
|------|------|----------|-------|
| 0 | ruler / overlord | Rat of the Reichstädte, King of Dänemark, Teutonic Knights | 0 |
| 1 | second authority | Archbishop of Köln, Hanseatic League, Duke of Burgundy | 10 |
| 2 | always "Famous Place" | | 0 |
| 3 | main square | Stadtplatz, Domplatz, Rathausmarkt | 0 |
| 4 | town hall | Rathaus, Stadthaus | 26 |
| 5 | castle | Burg, Schloss | 20 |
| 6 | cathedral | Dom, Minster | 44 |
| 7 | church | Kirche, St.Marienkirche | 0 |
| 8 | market | Markt, Marktplatz, Neumarkt | 0 |
| 9 | mint square | Munzenplatz | 86 |
| 10 | slums | Elendsviertel | 32 |
| 11 | armory, or a gate/tower | Zeughaus, Hahnentor | 61 |
| 12 | pawnshop | Leihhaus | 34 |
| 13 | monastery | Kloster, Deutschherrenhaus | 2 |
| 14 | inn | Gasthaus, Goldener Löwe, Drei Raben | 0 |
| 15 | university | Universitat, Berg-Akademie | 78 |

## Fonts (`FONTS.FNT`, `FONTS.UTL`)

Bitmap fonts, 1 bit per pixel. `FONTS.FNT` holds 3 fonts, `FONTS.UTL` 4
(its first three are the same fonts with small differences — see
"Character set" — the fourth is a plain ASCII font). See `FontFile.cpp` for a reference
implementation.

    offset  size    description
    0x00    2       font count N (uint16)
    0x02    2·N     offsets (uint16) of each font's glyph bitmap

Each font is stored as a width table, a header, then the bitmap; the
offset in the file header points at the **bitmap**, so the other two
parts are found *before* it:

    offset − 8 − G   G     glyph widths, one byte per glyph
    offset − 8       8     header:
                             +0  first character code
                             +1  last character code
                                 (G = last − first + 1 glyphs)
                             +2  bytes per glyph row (1 or 2)
                             +3  0
                             +4  height
                             +5  1 in every font — most likely the gap
                                 between glyphs in pixels *(inferred)*
                             +6  1 (0 in the ASCII font of FONTS.UTL)
                             +7  0
    offset           ...   bitmap, G · bytes-per-row · rows bytes

- **Rows = byte +4 + byte +6** — *inferred*, but the only rule that
  makes the bitmap size match the data for all 7 fonts (e.g. the
  first font of `FONTS.FNT`: 128 glyphs · 2 bytes · 7 rows = 1792 bytes,
  exactly the space up to the next font's width table). The meaning of
  +6 (an extra row? a descender?) is unknown.
- The bitmap is **row-major**: all glyphs' row 0, then all glyphs'
  row 1, ...; each glyph row is `bytes per row` bytes, most significant
  bit = leftmost pixel. **verified** by rendering all glyphs.
- Widths are **ink widths**: in almost every glyph the rightmost pixel
  column is set. Text therefore needs a gap between glyphs, presumably
  header byte +5 (1 pixel). Space is 1 pixel wide.
- Fonts in `FONTS.FNT`: 0 = characters 0x00..0x7F, 2 bytes/row, 7 rows;
  1 = 0x1F..0x7F, 1 byte/row, 9 rows; 2 = 0x1F..0x7F, 2 bytes/row,
  8 rows.

### Character set

ASCII, with German letters replacing a few codes. **verified** from the
glyphs in the fonts and consistent with the location names:

| Code | 0x1F | `[` 0x5B | `\` 0x5C | `]` 0x5D | `_` 0x5F | `{` 0x7B | `\|` 0x7C |
|------|------|----------|-----------|----------|----------|----------|------------|
| Char | ä    | Ä        | Ö         | Ü        | ß        | ö        | ü          |

Exception: in the first three fonts of `FONTS.UTL`, `_` is **ë**
instead of ß (the other substitutions are the same). `}` is a closing
parenthesis-like glyph, `~` a dash, 0x7F a checkered block. The ASCII
font of `FONTS.UTL` keeps the standard ASCII glyphs. Apart from these,
the first three fonts of the two files differ only in a few glyph
shapes (e.g. `I`, `O`, `F`, `+`).

## Menu cards (`MSGFILES`, `.MSG`)

Almost all of the game's narrative text: the menus of city places,
encounters, quests... See `MsgFile.cpp` for a reference implementation
(`darklands --messages` lists them, `--messages PARTY02` dumps one).

`MSGFILES` is an ordinary catalog (see "Catalog archives") of 419 `.MSG`
files, 3756 cards in all. Entry names are `$` + a five-letter topic + a
two-digit number + `.MSG`, e.g. `$PARTY02.MSG`, `$MAINS01.MSG`,
`$CITYG00.MSG` (the name field of `$NOGO00.MSG` has a NUL, then a space,
before the padding).

A `.MSG` file is a deck of **cards**, each a text box with options:

    offset  size  description
    0x00    1     card count N
    0x01    ...   N cards, one after the other:

    card (relative offsets):
    +0x00   1     text top
    +0x01   1     text left
    +0x02   1     unknown, 0
    +0x03   1     text width + 10 (the right limit when left is 10)
    +0x04   1     unknown, 0
    +0x05   ...   text, NUL-terminated (game character set + control codes)

- **verified**: all 419 files parse to exactly their last byte.
- Header meaning: top/left relative to the inner edge of the card
  frame; the column is `+0x03 − 10` wide (the wendigo reference says
  `right − left`: the same for the standard left of 10). **verified**
  in DARKLAND.EXE: the card's text routine (0265:0134) gets the x as
  left + 5 and the width as `+0x03 − 10` (file 0x8D110 + 0x587), and the
  gate card (`$SELEC00.MSG` card 0) breaks its lines exactly as on the
  manual's screenshot (see "Card screen"). Only `$MEETB00.MSG` (the boars)
  has cards with another left, which it sizes to the text and centers
  around x 140: read as `right − left` they came out 78 pixels wide and
  ran off the screen. The exact origin is only measured (±1 pixel). 3248 of the 3756 cards have
  `0A 0A 00 F0 00` (10, 10, 240); 3344 have a right limit of 240, the
  others range from 3 to 255.
- Bytes +2 and +4 are 0 in every card except the 76 of `$MCGUF07.MSG`
  (placeholders: header bytes that look like garbage, text `16 0A 0A`)
  and card 7 of `$RAUBI05.MSG` (`02 00 00 03 01`).

### Text control codes

| Code | Meaning |
|------|---------|
| `0x0A` | line break |
| `0x14` | paragraph: follows a `0x0A`; each one adds about 2 pixels between lines (measured on the manual's screenshot); two in a row before some option lists |
| `0x15` | starts an option: `0x15 "..." 0x1D` option text `0x0A` |
| `0x16` | same, an option that opens the saint list (wendigo) |
| `0x10` | same, an option that opens the potion list (wendigo) |
| `0x06` | same, an option that starts a battle at once (wendigo) |
| `0x1D` | ends the option's `...` prefix |
| `0x13`, `0x01` | *inferred*: delimit alternative sentences that the game shows or hides (42 of each, almost all in `$REVOL03`/`04` and `$SITUA00`, around one sentence per guild or faction, e.g. "The armorers $Support1 change.") |
| `0x19` | only in `$MONAS00`/`01`, in place of the `Y` of "You decide": most likely a typo in the data |

`0x1F` is not a code: it is `ä` (see "Character set").

### Variables

The text contains 67 distinct `$Name` variables that the game replaces,
e.g. `$PlaceName` (the current city), `$ChosenOneName` ...
`$ChosenFiveName` and `$NamedOneName` ... (party members and other
characters), `$he`/`$his`/`$him` (pronouns of the character), `$Money`,
`$Number1`, `$Text1`, `$CityLordName`.

`DARKLAND.EXE` holds the list of the variable names as plain strings
(`PlaceName`, `PlaceDesc`, `Inn`, `Money1`..`Money5`, `LeaderName`,
`ChosenOneName`..., `citySquare`, `councilHall`, `imperialMint`,
`cityBarracks`, `university`, `marketplace`, `fortress`, `pawnshop`,
`hospital`, `docks`, `poorhouse`, `slum`, `whorehouse`, `warehouse`,
`monastery`, `cathedral`, `cityChurch`, `NamedOneName`..., `CityLordName`,
`CityLordTitle`, `Support1`..., `SabbatTime`, ...).

- `$PlaceDesc` is the city's line of `DARKLAND.DSC` (see below):
  "Before you lies the $PlaceName, $PlaceDesc." **verified** for the
  record-to-city match.
- The place variables match the place slots of `DARKLAND.CTY`
  (*inferred* from the names and the texts): `$citySquare` 3,
  `$councilHall` 4, `$fortress` 5, `$cathedral` 6, `$cityChurch` 7,
  `$marketplace` 8, `$imperialMint` 9, `$slum` 10, `$cityBarracks` 11
  (the Zeughaus), `$pawnshop` 12, `$monastery` 13, `$Inn` 14,
  `$university` 15. `$CityLordName` is slot 0, the ruler, and
  `$CityLordTitle` is computed (exe.md, "Arriving at a city").
  **verified** (code)

### Where the cards are used

- Which card is shown, with which picture, and what an option does is
  decided by `DARKLAND.EXE`: the card file names do not appear in it as
  plain text. Only the texts tell what a card is for.
- The city cards, as far as the texts tell (*inferred*; `CityVisit.cpp`
  links them this way):
  - `$PARTY02.MSG`: the start of the game, at the inn. Card 0 for a
    party ("You gather around the comfortable fire at the $Inn...",
    with a leftover "test mines" option), card 1 for a single
    character; cards 2..6 look like a longer version of card 0.
  - `$OUTSI00.MSG` looks like the arrival at a city ("Before you lies
    the $PlaceName, $PlaceDesc."), but DARKLAND.EXE never uses it: the
    arrival is `$CITYE00.MSG`, then the gates `$CITYG00.MSG` (at night)
    and `$CITYG01.MSG` (by day); see exe.md, "Arriving at a city".
  - `$URBAN00.MSG` / `$URBAN01.MSG`: the inn (news, meal, residence,
    stables, storage...), by day / by night?
  - `$MAINS01.MSG` / `$MAINS02.MSG`: the main street by day / by night
    ("Darkness covers the main street... the watchmen, enforcing the
    curfew"), same options in the same order (cards 1..3: handbills,
    fair, war); `$MAINS00.MSG` is a designer's note ("This card
    describes options as you look down the main streets of the city").
    The night picture is presumably `XNMAIN.PIC`, `MAIN-ST.PIC` at
    night.
  - `$SIDES00.MSG` / `$SIDES01.MSG`: the side streets by day / by night
    ("Tiny gleams from occasional windows..."): the night card lists
    the fortress before the square.
  - `$SELEC00.MSG`: the city gate, to leave (card 0 by day, card 13 at
    night: "The gate is closed for the night"); `$SELEC01.MSG`: the
    city walls. Card 13 has options such as "1 not available", "2 alc
    not available", "4 combat not available": without the `...` code,
    placeholders the game presumably swaps for the day card's options
    or hides.
  - `$URBAN00.MSG` card 2: "you eat a hearty meal, then take eight hours
    of well-deserved sleep" (after the inn's "relax with a good meal and
    get eight hours sleep for $Money1"; `$Money1` is the price).
  - The places, by day / by night (the night card is card 0 of the
    `01` deck, or card 1 of the same deck):
    `$CITYS00`/`01` the city square, `$CITYF00`/`01` the fortress,
    `$MARKE00`/`01` the market, `$CHURC00`/`01` the religious quarter
    (a hub: cathedral, church, monastery, university, hospital),
    `$CATHE00`/`01` the cathedral, `$CITYC00`/`01` the city church,
    `$MONAS00`/`01` the monastery, `$UNIVE00` the university,
    `$COUNC00`/`01` the town hall, `$CITYB00` the barracks, `$BUSIN00`
    the crafts district (a hub: guilds, inn, physician, grove, slum,
    wall, gate), `$CIVCR00` the civil and `$MILCR00` the arms-making
    guilds (card 1 at night), `$CITYG05`/`06` a grove where the party
    waits (an hour, a bell of three hours, until nightfall / morning),
    `$SLUMD00` the slum, `$DOCKS00`/`01` the docks, `$OTHER00` "other
    locations you remember" (the homes of people met). `$CITYE00`..
    `03` are the arrival at a walled city (by day, at night, at war,
    during the fair), `$CITYG00`..`04` its gate, `$CITYW00`..`05` its
    walls, `$CITYH00` the hospice, `$CITYM00` inside the monastery.
  - Some are stubs: "City barracks, at night." (`$CITYB01`), "This is
    the slum at night. It isn't done yet." (`$SLUMN00`), "Sorry, this
    option isn't working yet." (`$NOGO00`), and the pawnshop, the
    poorhouse, the stables.
  - **Placeholder options**: many option lists have slots the game
    fills in or hides: a bare number ("5", "...3"), an empty option,
    "...this option should be hidden", or no `...` prefix at all ("1 not
    available"). Other options only apply in some situations ("accept a
    relic as reward for your services", "ask $NamedOneName to join your
    party", the boats of the docks with their destinations and fares).
- Most options of a menu only make sense in some cities (e.g. the
  main street lists `$fortress`, the docks...): the game must hide the
  ones that do not apply. How is unknown; `CityVisit` hides the ones
  whose place slot is empty, and the docks of inland cities.

### Card screen

The manual has two screenshots of cards: p. 28 ("Interaction Menus"),
the city gate card `$SELEC00.MSG` card 0, and p. 17 ("Character Boxes
and Universal Controls"), the main street card over its scene. Measured
on them and on the pictures (see `CardView.cpp` for a reference
implementation):

- **Layout**: a 60-pixel party sidebar on the left, the card on the
  right, from x = 60 to the right edge, full height. The frame is made
  of four pictures named in `DARKLAND.EXE`: `RPBDRTOP`/`RPBDRBTM`
  (245 × 6), `RPBDRLFT` (7 × 200), `RPBDRRGT` (8 × 200): 7 + 245 + 8 =
  260 = 320 − 60. **verified** against the screenshot.
- **Font**: font 2 of `FONTS.FNT` (8 rows), lines 9 pixels apart.
  **verified**: same glyphs, and the same line breaks with the header's
  column width.
- **Text origin**: about (inner left + left − 2, inner top + top + 1).
  *measured*
- **Illuminated capital**: the first letter of the card text is drawn
  from `ILLMCAPS.PIC`, a sheet of 20 × 20 cells 21 pixels apart, A..O on
  the first row and P..Z on the second. The first line of text follows
  it, aligned with its bottom; the next lines are below it. **verified**
  (screenshot and sheet).
- **Options**: "..." then the option text; its continuation lines are
  indented to the text, past the "...". **verified** (screenshot).
- **Palette**: `ILLMCAPS.PIC` is the only picture whose `M0` chunk
  covers 128..159, and that range is the card's: the borders use
  129..142 and 255, the capitals 139, 140 and 5, `SIDEBAR.PIC`
  (54 × 198, a blue gradient) 149..159. 184 of the 185 scene pictures
  examined never use 128..159 (the exception: `XMS024.PIC`). The same
  32 colors are entries 24..55 of `COMNCLRS.DAT` (72 colors), except
  the first (128). **verified**
- **Paper**: index 255. It is the blank background of the scene
  pictures (e.g. 21953 of the 64000 pixels of `MAIN-ST.PIC`) and the
  border dots; the scene palettes set it to (63, 57, 54), a warm white,
  in 356 of the 389 that cover it. **verified** (pictures and palettes)
- **Other colors**, *inferred* from the screenshots, which are almost
  black and white: dark text, a dark capital box with a light letter
  (index 140). Index 5, the capitals' lattice, prints as dark as the
  box: the EGA magenta of the default VGA palette fits, the gold of
  `COMNCLRS.DAT` entry 5 does not.
- **Scene pictures** (e.g. `MAIN-ST.PIC`, `XMS001.PIC`) are 320 × 200
  with a palette for 16..255, painted on the paper (index 255) inside
  the card's interior only: the left 60 or so columns and the edges
  are blank. The p. 17 screenshot shows the main street picture *under*
  the card text, very faded: the game presumably lightens the scene's
  palette while a card is on it. **verified** that the scene is behind
  the text; the fading is *inferred*.
- **Character boxes** (the sidebar, manual p. 17): one box per party
  member, about 39 pixels apart: the nickname at the top (the leader
  "in special colored text"), the character's picture on the left,
  three bars and their values beneath: current endurance, strength and
  divine favor, the bar showing the current value as a share of the
  maximum. *measured* on the screenshot, ±1 pixel. The picture is
  `<image>STAT.PIC` (10 × 19, e.g. `F60STAT.PIC`; the image code comes
  from the party table, see "Characters"): it uses the card palette
  range and the EGA colors, index 0 transparent. **verified** (indices);
  the name pattern `F??stat.PIC` is in `DARKLAND.EXE`.
- `TEXTBACK.PIC` (158 × 50) is not the card background: it is a small
  framed panel, used elsewhere.

## Characters (`CHARACTR.TMP`, character records)

`CHARACTR.TMP` holds the characters of a new game: in the game data,
the Quickstart party of the manual (p. 11): Gretchen Wilburg ("Gretch"),
Gunther Langer, Hans Muller, Ebhard of Achdorf. See `CharacterFile.cpp`
and `Character.cpp` for a reference implementation.

    offset  size       description
    0x00    2          character count N
    0x02    2·5        party: character indices, walking order (0xFFFF: none)
    0x0C    4·5        party: image codes, e.g. "F60\0" (picks F60STAT.PIC,
                       F60C.CAT...), per party slot
    0x20    554·N      character records

- **File size: 2248 = 32 + 4 · 554** (**verified**). The party table is
  the one of the saved games (0xF3 and 0xFD there), without the colors.

Character record, 554 (0x22A) bytes, the same in the saved games. The
layout is wendigo's; checked on the four characters:

    +0x12   2     age (35, 40, 40, 45)
    +0x15   1     heraldic shield, 'A'..'O' (A, B, C, D)
    +0x17   1     sex: 1 = female (inferred: 1 for Gretchen only)
    +0x22   1     missile weapon in use: item type, 0xFF = none
    +0x25   25    full name, NUL-terminated
    +0x3E   11    nickname ("Gretch"), NUL-terminated
    +0x4B   1     vitals armor in use: item type
    +0x4C   1     limbs armor in use: item type
    +0x4F   1     vitals armor quality
    +0x50   1     limbs armor quality
    +0x51   1     weapon in use: item type
    +0x5C   1     shield in use: item type
    +0x5D   7     current attributes: END STR AGL PER INT CHR DF
    +0x64   7     maximum attributes
    +0x6B   19    skills (order below)
    +0x7E   2     item count (at most 64)
    +0x80   20    saints known, 160 bits: saint i is bit 0x80 >> (i & 7)
                  of byte i >> 3 (DARKLAND.EXE 0E76:1260, **verified**
                  in the code)
    +0x94   22    alchemical formulae known
    +0xAA   6·64  items: code (word), type, quality, quantity, weight

- **verified**: names, nicknames and ages are right for all four; the
  attributes are plausible (divine favor 99 for all) and the current
  ones fall below the maximum after fights in the saved games; the
  item count matches the non-empty item records.
- **Items** — **verified** on all the characters of `CHARACTR.TMP` and
  two saved games: the code is the item's index in `DARKLAND.LST`, the
  type byte is that item's type there, and the equipment in use (0x22,
  0x4B, 0x4C, 0x51, 0x5C) is the type of one of the carried items
  (e.g. Gretchen: weapon 3, a Short Sword; vitals 75, V:Plate Armor;
  limbs 82, L:Chainmail). The weight is per carried item.
- **Skills**: 19, in the order and with the abbreviations that
  `DARKLAND.EXE` lists: Edged, Impact, Flail, Polearm, Thrown, Bow
  weapons, Missile devices, Alchemy, Religion, Virtue, Speak Common,
  Speak Latin, Read & Write, Healing, Artifice, Stealth, Streetwise,
  Riding, Woodwise (wEdg wImp wFll wPol wThr wBow wMsD / Alch Relg Virt
  SpkC SpkL R&W / Heal Artf Stlh StrW Ride WdWs, the three skill boards
  of the character screen).
- No money: the characters' funds are pooled when the adventure begins
  (manual p. 15). Where they come from is unknown.

## The inns' caches (`CACHE.TMP`)

The items left with innkeepers (see exe.md, "The inn's cache"): a word
per cache, the offset of its data; there, a count byte and 4-byte
entries (item code word, quality, count). The file is 198 bytes and
starts with 99 and the number of caches (byte 1); the table words are at
2n for the cache n from 1, and the data of the caches is appended after
the first 198 bytes in the order they were made (DARKLAND.EXE: reading at
file 0x6E900, writing at 0x6EA18: a new cache takes the next number and is
appended; the others are copied again when one changes). A location's
word +0x18 is its cache's number, -1 for none. **verified** for the empty
case: the last 198 bytes of every saved game and of SAVES/DEFAULT are
exactly the game's empty CACHE.TMP, and +0x18 is -1 everywhere; the rest
is *inferred* from the code (no sample has items). The saved games hold the
caches: they end with the 198 bytes, longer with items in them
(`SaveFile::Caches()`, `saved_game::caches`; the program numbers them in
the order of their locations).

## Saved games (`SAVES/DKSAVEn.SAV`, `SAVES/DEFAULT`)

The layout is wendigo's; only the fields the game uses so far, checked
on `DEFAULT` and two saved games (see `SaveFile.cpp`):

    0x00    12    location name ("Olm|tz", "Wilderness")
    0x15    23    label ("Darklands"; "new default" in DEFAULT)
    0x64    2     the game's seed global, DS:9C4A (**verified**: the
                  save code writes 21 bytes of name, 79 of label, then
                  it; 33994 in both saved games, 11183 in DEFAULT)
    0x68    8     date: year, month, day, hour (words)
    0x70    6     money: florins, groschen, pfennigs (words)
    0x7A    2     party fame (wendigo: "global reputation")
    0x8C    2     letter of credit, in florins
    0x92    2     philosopher's stone quality
    0x7C    2     location: index into DARKLAND.LOC, 0xFFFF = wilderness
    0x7E    4     map tile: x, y (words)
    0x82    2     the state the game goes on from, DS:A772 (0x0C the map
                  in DKSAVE0, 0x1D the inn in DKSAVE1; **verified**:
                  the save code writes it here)
    0x84    8     DS:E898, E7D8 (the state to return to), E896 (a
                  quest's location: 120, Grötsch, in DKSAVE0), A88D (the
                  previous state)
    0x96    1     the difficulty, DS:906A (0 basic, 1 standard, 2
                  expert: 1 in both saved games); the game's menu
                  changes it and Game::Save() writes it
    0xA1    1     party leader: party slot
    0xA4    2     DS:A891: 3 on the map, 0 in a city
    0xEF    2     characters in the party
    0xF1    2     character count N (the party's first, then the other
                  characters of the world: `Game::Save()` writes the
                  members who retired there)
    0xF3    2·5   party: character indices (as in CHARACTR.TMP)
    0xFD    4·5   party: image codes
    0x111   24·5  party: colors, per slot 8 RGB triplets (6-bit): the
                  colors 235..242 of its battle figure (see "Battle
                  sprites")
    0x189   554·N character records
    ...     2     event count E
            48·E  event records (see "Events")
    ...     2     location count L (414; 405 in DEFAULT)
            58·L  location records, as in DARKLAND.LOC, with the state
                  of the game: +0x12 the party's reputation there
                  (−99..99), +0x14 flags (war, fair...: not decoded)

- **Date**: the words are year, month, day, hour (1401, 0, 13, 6 in
  `DKSAVE1.SAV`), the reverse of the order given by wendigo. **verified**
  (plausible values in three files); month 0 = January is *inferred*
  (the saved games are in month 0 of 1401, `DEFAULT` in month 4 of
  1400; the running game keeps the month 0-based too, see exe.md). The
  game shows the time as one of eight monastic hours of three hours
  each, Matins at midnight to Compline at 9 PM, on the Julian calendar
  without leap years, and hides the year (manual pp. 20-21).
- **Location** — **verified**: `DKSAVE1.SAV` is at location 73, Olmütz,
  tile 295,659; `DARKLAND.LOC` puts Olmütz at 296,659, the next tile.
  `DKSAVE0.SAV` is in the wilderness nearby (289,654).
- **Money** — plausible: 70 fl 18 gr 10 pf, then 70 fl 18 gr 2 pf two
  days later.
- **Leader**: 1 with the party order Hans, Gretchen, Gunther, Ebhard:
  a party slot, Gretchen, as in the new game. *inferred*
- **Events and locations** — **verified**: the counts and sizes add up
  to the file's end (198 bytes remain: 0x63, then zeros: CACHE.TMP, see
  "The inns' caches"); only 10
  location records differ from DARKLAND.LOC in `DKSAVE0.SAV`, in the
  fields +0x0, +0x8, +0xC..+0xE, +0x12 and +0x14; Olmütz, where the
  party is, has a reputation of 64. DARKLAND.EXE reads the reputation at +0x12 and the
  flags at +0x14 of its location records (see exe.md).
- **The save code** (file 0x751A8...) writes the header field by field
  with `fwrite`; the offsets it gives match all the fields above
  (**verified**). 0xA7..0xEE are 36 words of a local variable, perhaps
  padding. The party's standings with the banks (DS:4BB6, DS:4BB8) are
  not written. It asks for a comment ("Save Game Comment:", 23
  characters, "Darklands" to start with, file 0x74CD6), and names the
  file `saves\dksaveN.sav` with the first N not taken (file 0x7505A);
  the location name is the location's, or "Wilderness".
- **Writing** (`SaveFile::Write()`): the bytes of the saved game the
  party came from (`DEFAULT` for a new game) with the fields above, the
  party's characters (their records as read, with the attributes,
  skills, equipment, saints and items), the events and the locations'
  state; `DEFAULT` has 405 location records, the others come from
  DARKLAND.LOC. Rewriting `DKSAVE0.SAV` and `DKSAVE1.SAV` gives the same
  fields back (**verified**); the bytes differ in the garbage after the
  strings and in the order of the character records (2, 0, 1, 3 in the
  game's files, the party's order here).
- `DEFAULT` is the new game template: the four Quickstart characters,
  no party members, Rottweil, 28 April 1400 (month 4?), 0 fl 10 gr
  10 pf.

## Events (`EVENTS.TMP`, and the saved games)

What happens in the world: the game keeps up to 300 events (far
pointers at 2E38:1B2C); `EVENTS.TMP` is a word, the count of the used
slots, then their 48-byte records in slot order, as the game writes it
(file 0x6141E: **verified**, 2 + 28 · 48 = 1346, the file's size). The
saved games hold the same after the characters. See `EventFile.cpp`
and `darklands --events [save]`.

    offset  size  description
    +0x00   2     a number the queries compare (0E76:360C, 3C28): the
                  subject within the kind (0..10, 171, 172 seen)
    +0x02   8     a date: hour, day, month, year (words); the creation
                  (*inferred*: at or before the start)
    +0x0A   8     the start: the event counts from then on (0E76:3180
                  compares year, month, day, hour with the game's date)
    +0x12   8     the end: over from then on (0E76:31E6); 31/12/1499 23h
                  (month 12: past December) for "never"
    +0x1A   2     compared by 0E76:3C28; −2 in many records
    +0x1C   2     location, index into DARKLAND.LOC (0E76:3470, 360C)
    +0x1E   6     not known (73, 74, 84, 99, 100 ... ; 95; 0)
    +0x22   2     category: the queries walk those of one category
                  (0E76:324C; asked for 28, it takes 8 too); 8 the
                  world's events, 28 the people tied to them, 36, 38,
                  42, 43, 90, 99 seen
    +0x24   4     not known
    +0x28   2     kind, within the category: for category 8, 4 a dragon
                  (at "Lair", $AFFAI00 card 5: "Some even talk of a
                  dragon"), 3 a robber knight (at a castle, subjects 171,
                  172), 10 special sites (Lake, Shrine, Tomb, Cave,
                  Spring), 1, 12; 2 political unrest (the notices' and
                  gossip's queries; none in these files)
    +0x2A   2     compared by 0E76:3C28
    +0x2C   4     not known

- **Dates** — **verified**: month 0 exists (28/0/1402), so the months
  are 0-based like the game's clock; the comparisons are the code's.
- **Locations** — plausible: the dragon at "Lair" (138), the robber
  knights at Grötsch (120), a castle.
- **Kinds** — *inferred* from the locations and the cards the queries
  choose, except kind 2 (political unrest: the queries of the news,
  exe.md "News and rumors").

## Item and name lists (`DARKLAND.LST`)

See `ListFile.cpp` for a reference implementation.

    offset  size     description
    0x00    1        item slots (200)
    0x01    1        saints (136)
    0x02    1        alchemical formulae (66)
    0x03    46·200   item definitions
    ...              NUL-terminated strings: the saints' names, their
                     short names, the formulae's names, their short names

    item definition (wendigo's layout):
    +0x00   20    name ("Hand Axe", "V:Plate Armor"), NUL-terminated
    +0x14   10    short name ("Hnd Axe", "V:Plate")
    +0x1E   2     type: what the characters' equipment refers to
    +0x20   5     flags (weapon kinds, component, potion, relic...); byte
                  4, bit 7: merchants neither sell nor buy it (exe.md)
    +0x25   1     weight
    +0x26   1     default quality
    +0x27   1     rarity
    +0x28   4     unknown (relics, permits)
    +0x2C   2     value

- **verified**: 3 + 200 · 46 bytes, then exactly 2 · 136 + 2 · 66
  strings to the end of the file ("St.Adrian"/"S.Adrian" first,
  "al-Razi's Noxious Aroma"/"aR NoxAro" for the formulae); 172 of the
  200 item slots are used.
- **Flags** — **verified** by the names of the items that set them:
  byte 0: edged (1), impact (2), polearm (4), flail (8), thrown (0x10),
  bow (0x20), metal armor (0x40), shield (0x80); byte 1: tools (1:
  torch, rope, lantern...), gear (2: clock, grappling hook, lockpick),
  alchemical component (4), potion (8), relic (0x10), horse (0x20),
  document (0x40: letter of credit, residency permit...); byte 2:
  lockpick (1), light (2), arrow (4), quarrel (0x10), ball (0x20),
  special/quest item (0x80); byte 3: throwable potion (1), non-metal
  armor (4), missile device (8: crossbows, guns), music (0x20).
- The item types of the armor run in order: 67..75 V:Clothing, Padded,
  Leather, Studded Leather, Cuirbouilli, Scale, Chainmail, Brigandine,
  Plate; 76..84 the same for the limbs; 95..97 Small, Medium, Large
  Shield.

## Trade (item exchange scrolls)

The rules (what merchants sell, the qualities and the prices) are
decoded from DARKLAND.EXE: see [exe.md](exe.md). The screen:

The manual (pp. 28-29) has a screenshot of the swordsmith's shop.
`BUYSELL.PIC` is the whole screen: the character boxes, the card frame
and two scrolls whose interiors are (80, 84)-(267, 116) and
(104, 146)-(291, 178) (**verified**: the picture), four 8-pixel rows
each. The texts are in `DARKLAND.EXE`: "%s holds the purse of %dfl,
%dgr, %dpf (%lupf).", "%s barters for %s to the %Fs", " P|urchase an
item", " S|ell an item", " B|arter for another person", " L|eave",
"The %Fs offers...", "%s has...", "%5upf  %Fs  %3dq  %3dlbs" (the
merchant's items), "%5upf  %Fs  (%3d) %2dq" (the party's), "Not enough
money".

- The screenshot's prices (the swordsmith selling items worth 125 for
  394 pf) do not follow the game's rules: it shows a pre-release
  version.
- The arms-making guilds' shops are `$SWORD00`, `$BLACK00`, `$ARMOR00`,
  `$BOWYE00` (by day: "visit the street-level shopfronts to buy and
  sell goods") and their `01` decks (at night: "awaken somebody to
  make a purchase or a sale"); the other options belong to the guild
  quests.

## Information screens

The party information (F6) and character information (F1..F5, or a
click on a character box) screens of the manual (pp. 20, 22), with
their screenshots. See `InfoView.cpp` for a reference implementation.

- **The interface palette**: 128..159 is the same in every picture
  checked (`ILLMCAPS.PIC`, `PTYSTATS.PIC`, `ARMBACK.PIC`, the scenes):
  31 of the 32 colors are identical, all but 128. **verified**
- **Character boxes**: 40 pixels high (**verified**: the box borders of
  the left column of `PTYSTATS.PIC` and `ARMBACK.PIC`).
- **Party information**: the background is `PTYSTATS.PIC`, with the
  panels, the character boxes and a small map of the Empire with a red
  dot per city: 92 dots, 16 of them 3 × 3 (mostly the big cities), in
  EGA colors 4 and 12. A linear fit from the world map's pixel
  coordinates puts the cities on their dots with a mean error of 1.8
  pixels (max 4.6): x = 0.02172 · X + 202.93, y = 0.02867 · Y + 72.49.
  The party is marked with `MAPLOCTR.PIC`, a 9 × 9 cross-hair. The
  labels are in `DARKLAND.EXE` next to "pics\ptystats.pic": PARTY
  INFORMATION, MAP INFORMATION, PARTY FAME, TIME, DATE, LOCATION,
  WEALTH, NOTES, LOCAL REP, Small-/Moderate-/Large-Sized, "Rep: ",
  "%d Florins", "%d Groschen", "%d Pfenniges", "%d PhStone". The
  reputation words, from best to worst: a local hero (over 50),
  respected (11..50), unknown (−9..10), suspected (−39..−10), wanted
  (−74..−40), hunted (−75 and less) (**verified**: DARKLAND.EXE, see
  exe.md). The local reputation is the saved games'. The city
  size words (*inferred*: 3..4 small, 5..6 moderate, 7..8 large; Kassel,
  size 5, is "Moderate-Sized" on the screenshot).
- **Character information**: the background is `ARMBACK.PIC`, with the
  boards and the buttons (Equipment, Formulae, Saints). The archway
  alone is `ARMSBACK.PIC`, at (59, 13) in it (**verified**: pixel
  match). The figure is made of 137 × 170 pictures drawn in the archway,
  index 0 transparent: limb armor (`PAD-LIMS`, `LEAT-LIM`, `STUDLIM`,
  `CUIRBLIM`, `SCALELIM`, `CHAINLIM`, `BRIGLIM`, `PLATELIM`), vitals
  armor (`PAD-VIT` ... `PLATEVIT`), shield (`SMALLSH`, `MEDIUMSH`) and
  weapons (`WEAPONn`). The armor and shield lists are in `DARKLAND.EXE`
  in the item type order (clothing and padded share `PAD-*`, medium and
  large shields `MEDIUMSH`); the weapons' n is not the item type: two jump
  tables (file 0x61F02, **verified**) give it, for the weapon in use
  type 0 (Two-hand Sword) 2, 1 (Long Sword) 3, 2 Falchion 4, 3 Short
  Sword 5, 4 Poniard 6, 5 Dagger 7, 6 Battle Axe 8, 7 Hand Axe 0, 8 Field
  Axe 9, 10..21 the type + 1, 22 (Quarterstaff) 24, and for the missile
  weapon types 23, 24, 25, 26, 27..33 the pictures 23, 26, 27, 1, 28..34.
  They are drawn in the order limb armor, vitals armor, weapon in use,
  shield, missile weapon, both weapons at once.
  The encumbrance board needs the carrying capacity, whose formula the
  manual does not give.

One line per city, the `$PlaceDesc` of the cards. See
`DescriptionFile.cpp` for a reference implementation.

    offset  size     description
    0x00    1        0x5E (not the count: there are 92 records)
    0x01    80·92    records: text, NUL-terminated, NUL-padded

- **File size: 7361 = 1 + 92 · 80** (**verified**); every record is
  zero after its NUL.
- **Record i describes city i of `DARKLAND.CTY`** (**verified** on the
  names: 0 Groningen "a small North Sea port controlled by Dutch
  nobles", 5 Lübeck "a wealthy Imperial Free City, center of the
  Hanseatic League", 26 Köln "the largest city in the Empire, center
  of trade and craftsmanship", 40 Marienburg "fortress-capital for the
  Hochmeister of the Teutonic Order").

## Messages (`DARKLAND.MSG`)

Not a card deck: **4001 = 1 + 10 · 400** (**verified**). Byte 0 is the
record count (10), then 10 records of 400 bytes, each a NUL-padded text
in the game character set. Only three are used: "Error Message" and
two travel messages (entering a robber knight's territory, a blighted
land).

## Battle data family (`DARKLAND.ENM`, `IMAPS.CAT`, `*.IMG`, `*.IMC`, ...)

The tactical (combat) side of the game is data-driven: the enemies
(`DARKLAND.ENM`), the battlefield maps (`IMAPS.CAT`), graphics
(`BATTLEGR.IMG`, `COMMONSP.IMG`) and the battle sprites of enemies and
party (`*.IMC` in `E00C`, `M00C`, `A00C`, `C00C`, `F01C`, `F60C.CAT`,
indexed by `TACANIM.DB`) live in separate files. A debug dump of the tactical module ships
with the game (`TAC.TXT`, dated 09/15/92 — the debug code is still in
DARKLAND.EXE) and names most of the structures; see also
[exe.md](exe.md).

### TAC.TXT — the game's own structure dump

One battle's parameters, printed by the game's own debug code:

```
Tac params are
	mode: 0
	bfldtype: 43          battlefield type
	rseed: 40077          random seed for the battlefield generation
	ftype1: 3	fqual1: 1	fnum1: 4    three foe groups:
	ftype2: 0	fqual2: 2	fnum2: 1    enemy type, quality, count
	ftype3: -1	fqual3: 0	fnum3: 0    (-1: group absent)
	terrn: 0	mnth: 7   terrain and month (seasonal battlefields)

Enemy Activation Record for level 0
	NumActivationSpots: 1
	strtx: 624
	strty: 56
	sprd: 16
	rm: -1
	AllowReenf: 0
	type[0]: 0  type[1]: 1  type[2]: -1
	num/type[0]: 4  num/type[1]: 1  num/type[2]: 0
	F: 0	G: 1	Pf: 7            loot in the spot
Level Record:0
V:-2 S:-2 R:-2 D-2 E:-2     five stat modifiers
FurnRemove / WallRemove     x/y lists (5 / 7 entries max)
```

The debug text gives the field names and print order of the TacParams,
ActivationRecord and LevelRecord structures; their exact binary layout
is not decoded yet. The seed and the battlefield type probably choose
among the stored maps of `IMAPS.CAT` (see below) rather than generate a
field from nothing. *inferred*

`ftype` is an index into the 82 enemies of `DARKLAND.ENM` and `fqual`
the variant within the enemy's group: `ftype1: 3` is the "Guard" (types
5..9, `fqual1: 1` → "Guard2"), `ftype2: 0` the "Sergeant". **verified**:
the night watch's battle in DARKLAND.EXE passes exactly these foes
(exe.md, "The market at night and the night watch"). TAC.TXT's
battlefield type, 43, is a city's but not one the watch uses (0x11,
0x13, 0x15, 0x56): another city fight with the same foes, perhaps. It is not the skeleton of `LEVEL0.ENM`,
which an earlier revision of this document took for the same fight.

### Enemies (`DARKLAND.ENM`)

16452 bytes, no header: 71 enemy types, then 82 enemies. **verified**:
71 × 204 + 82 × 24 = 16452 exactly, and every record's name falls at the
same offset. The counts are not stored in the file. See `EnemyFile.cpp`
and `./darklands --enemies`.

    enemy type, 204 (0xCC) bytes:
    +0x00   4     image code, NUL-terminated ("E00", "M03"): the sprite
                  set in E00C.CAT / M00C.CAT
    +0x04   10    name ("Sergeant1", "Skeleton2", "7 Hd Drag"),
                  NUL-padded
    +0x0E   1     in a group's first type: the number of types in the
                  group (its variants); 0xFF in the others
    +0x0F   1     palette count P: the type's alternative palettes
    +0x10   1     chunks per palette C (1..3)
    +0x11   1     first chunk in ENEMYPAL.DAT: palette p is chunks
                  first + p·C .. first + p·C + C - 1
    +0x12   2     unknown (0..2)
    +0x14   7     attributes, in the character order (End, Str, Agl,
                  Per, Int, Chr, DF)
    +0x1B   19    skills, in the character order (EdgW, ImpW, FlaW,
                  PolW, Thrw, Bows, Msle, Alch, ...)
    +0x2E   2     word, 8..20: unknown
    +0x30   ...   mostly zero; bytes at +0x92..+0xB8 look like two
                  groups of 6 bytes (weapons? `06 13 01 14 ff ff`) and
                  runs of 0xFE / 0xFF. Not decoded, but for:
    +0x92   2     armor, vitals and limbs: item types (72 V:Scale and 77
                  L:Padded for Sergeant1), 85..91 for the monsters' hides
                  (DARKLAND.EXE's armor strengths have 1..6 for them)
    +0x96   1     armor quality (25; 99 for the dragons)
    +0x97   1     shield: item type 95..97, 0xFF none; +0x99 its quality
                  (the kinds and ranges fit; *inferred*)
    +0xA0   1     the weapon, a type of DARKLAND.EXE's weapon table
                  (exe.md, "Battles"): 6 (battle axe) for Sergeant1,
                  whose sprites include E00CBA2; 48 (the skeleton's own
                  weapon, SK) for the skeletons. **verified**: in 64 of
                  the 70 types (Baphomet has no sprites) the weapon's
                  code names one of the type's sprite files; +0xA6 is
                  often a weapon too, with no sprites of its own
    +0xC0   6     the cash a foe carries: three words, the sides of a
                  die of florins, groschen and pfennigs (0: none)
    +0xC6   6     three signed words added to them (mostly 0, -1 for
                  the bandits' groschen and the knights' florins)
                  **verified** (code, exe.md "The foes' cash"); the
                  words are 0 for animals and the undead

    enemy, 24 (0x18) bytes:
    +0x00   2     the first type of its group
    +0x02   12    name as the game shows it ("Guard", "Raubritter");
                  "Castle Guard" fills the field, with no NUL
    +0x0E   8     zero
    +0x16   2     flags (0x00, 0x01, 0x03, 0x07, 0x3F, 0x4F, 0x81):
                  not decoded

- **verified**: the `variants` byte splits the 71 types into 38 groups
  that cover them exactly; the types of a group share the image code;
  every enemy points at the first type of a group. Several enemies share
  a group ("Guard", "Schulz", "Hussite", "Trooper" are all types 5..9).
- The variants are stronger versions of the type: Sergeant1..5 have
  End 22, 26, 30... and weapon skills 20, 28... *inferred* to be
  TAC.TXT's `fqual`.
- The values look right for the creatures: the skeletons have 75 in the
  four melee weapon skills, the dragons 80 in all seven weapon skills,
  Baphomet 99 in the three missile skills; DF is 99 for everyone.
  *inferred*
- The palette fields (named by vvendigo's `reader_enm.py`) tile
  `ENEMYPAL.DAT`: each group's chunks start where the previous group's
  end, except a one-chunk gap before the Kobold; the types of a group
  share them. **verified**. The chunks' start indices fit the sprites'
  colors (the skeleton's chunk starts at 32 and its sprites use 32..45).
  Baphomet's chunks (71, 72) are past the end of the file, which has 71.
  Which of its palettes an enemy wears is not known.


### Level files (`LEVEL0/1/2.ENM`)

Small files (291, 871, 581 bytes), probably left over from testing (like
TAC.TXT) or per-level battle records: they hold copies of
`DARKLAND.ENM` types (the bytes of "Skeleton2" — attributes
`28 28 28 19 14 14 63`, skills `4b 4b 4b 4b ...` — appear in
`LEVEL0.ENM`), rearranged. *inferred*. *Partially decoded — exact
stat offsets unknown.*

    offset  size  description
    0x00    1     enemy record count N (1, 3, 2 in the three files)
    0x01    ...   N records, each starting with a size byte
                  (0x30, 0x40, 0x80, 0x10 observed) — semantics unclear:
                  not a flat record length
    ...     ...   string table after the records:

    per entry:
    +0x00   1     count (?) — 01 or 04 observed
    +0x01   1     NUL (padding?)
    +0x02   4     image code, NUL-terminated ("M03", "E04", "E10", "PE04")
    ...     ...   enemy name, NUL-terminated ("Skeleton", "Brigand Sgt",
                  "Raubritter"), in the game character set

- **Image codes** map to sprite catalogs by first letter: `E##` →
  entries of `E00C.CAT` (human enemies), `M##` → `M00C.CAT`
  (monsters/undead). The number is the enemy index used in the catalog
  entry names (`E10` → `E10WKS2.IMC` etc.).
- Some enemies have **two image codes** ("Brigand Sgt" appears as both
  `E04` and `PE04` in LEVEL2.ENM): a foot and a mounted variant.
  *inferred*
- The codes match weapon/attack suffixes used by the IMC files (see
  below): E10 (a mounted raubritter) only has lance (`S2`) files.
- Byte values 0x14, 0x19, 0x28, 0x63 (20, 25, 40, 99) recur in the stat
  areas — plausible stats/thresholds (99 = maximum). *inferred*
- The byte `2a` appears at fixed positions in the records
  (marker/separator, *inferred*).
- The three bytes `a0 7c b5` appear at nearly the same offset in all
  three files (format constant, purpose unknown).
- The high-entropy data after the structured area is **byte-identical
  between LEVEL1.ENM and LEVEL2.ENM** — template/filler garbage from the
  tool that wrote the files, not data. (`ff ff ff 19 00 00` runs likewise.)
- **verified**: the image-code/name pairs parse cleanly in all three
  files. (The `$RAUBI0` found in BATTLEGR.IMG, once taken to match
  "Raubritter", is leftover garbage there.)

### Battlefield maps (`IMAPS.CAT`)

An ordinary catalog of 143 maps, named by setting:
`ICITY.000`..`ICITY.801` (cities), `IMINEGEN.*` / `IMINSPEC.*` (mines),
`IWILDGEN`, `IWILDMTN`, `IWILDMSH`, `IWILDSPC`, `IWILDSAB`, `IWILDGAT`,
`IWILDWAL` (the wilderness: plains, mountains, marshes...),
`IFORTMON.*` (fortresses and monasteries), `IMISCTOM.*` (tombs).
**verified** (the catalog listing).

Each entry is compressed like the `.IMC` sprites (LZEXE's scheme, see
"Battle sprites"; `Lzexe.cpp`): all 143 decompress to exactly 13308 bytes, ending on
the end mark at their last byte. **verified**. *Partially decoded*:

    0x0000  6400  40 x 40 cells of 4 bytes, row by row
    0x1900  1600  40 x 40 bytes
    0x1F40  5308  records, not decoded (0x1F40: 00 00 fe ff fe ff ...
                  in every map; the code reads cell coordinates, y and
                  x, from bytes 0x1F5C and 0x1F5D of a record)
                  in every map; then 14-byte records?)

- The first and last rows and columns of both grids are the same in
  every map: a border. **verified**
- See `BattleMap.cpp`; `./darklands --battlemap ICITY.000` prints a map
  as text (walls as their type numbers, `##` closed cells, objects as a
  hex digit) and the bytes of its cells. How the game picks a map is in
  [exe.md](exe.md), "Battles".
- **Cells**: the 4 bytes are the cell's 4 sides. DARKLAND.EXE (file
  0x5222A, while it removes walls: TAC.TXT's "WallRemove") clears the
  low nibble of byte 0 of cell (x, y) with byte 1 of (x, y + 1), byte 1
  with byte 0 of (x, y - 1), byte 2 with byte 3 of (x - 1, y), byte 3
  with byte 2 of (x + 1, y). **verified** (code). Which way is north on
  the screen is not known yet.
- **Walls**: the low nibble of a side is its wall, 0 for none. The two
  cells of a side agree in 94% of the cases; otherwise the wall is on
  one side only, and the maps store some walls on one face only (a
  fortress room has walls on the sides toward y - 1 and x + 1 but not on
  the others). *inferred*. The types follow the settings (counts over
  all maps): 1 in the mines (163000 sides), the mountains and the
  wilderness: rock; 2 in the cities (8800), the town gates and walls:
  houses; 6 in the fortresses, monasteries and tombs (3400): masonry;
  3, 7, 8, 9 are rare (with 2 or 6: doors, gates?); 4 and 5 appear 2..5
  times. The meanings are *inferred* from where they appear.
- **Closed cells**: bit 7 of byte 3 is set exactly when the ground byte
  is 0 (in all 229000 cells of the 143 maps). **verified**. They are the
  insides of houses, the rock: the cells one cannot enter (*inferred*).
- **Objects**: the high nibble of byte 0, only on open cells.
  **verified**. 11, 12, 13 are common in the wilderness (trees?), 14 in
  the marshes, 1..4 in the wilderness too, where they line up from one
  edge of the map to another (paths?); the cities and fortresses
  use most values a few times (furniture: TAC.TXT's "FurnRemove"?).
  *inferred*. The high nibbles of bytes 1 and 2 are almost always 0.
- **Ground** (the second grid): 0 in closed cells; 0x10 open ground,
  with variants 0x11..0x19 in the wilderness; 0x40..0x44 inside the
  fortresses (floors?), 0x20 and 0x30 rare. *inferred*
- See `BattleMap.cpp`; `./darklands --battlemap ICITY.000` prints a map
  as text (walls as their type numbers, `##` closed cells, objects as a
  hex digit) and the bytes of its cells. How the game picks a map is in
  [exe.md](exe.md), "Battles".
- Printed as text, `ICITY.000` is a town: blocks of houses (cells whose
  byte 3 has bit 7 set, 0x80 / 0x81) with walls on one side (byte 3 =
  1..8), streets between them (byte 3 = 0), some street cells with a
  byte 0 of 0xB0..0xE0 (objects?). The second grid is 0x10 on the
  streets and 0 in the houses: the ground one can walk on. *inferred*
- The maps hold no pictures: how the game draws walls and ground from
  them is to be found in DARKLAND.EXE.
- Not every battle uses them: the castle's levels are in `LCASTLE`, a
  catalog of its own, and the dragon's cave has its own code (see
  [exe.md](exe.md), "Battles").

### Battle pictures (`BATTLEGR.IMG`, `COMMONSP.IMG`)

Pictures in the same format as the sprites' (see "Battle sprites"), not
compressed. See `ImgFile.cpp`; `./darklands --extract BATTLEGR.IMG
<dir>` exports them.

    0x00    2     data size S
    0x02    2·N   the offset of each picture, in paragraphs from the
                  start of the data (N = (file size - S - 2) / 2)
    ...     S     the pictures: width (byte), height (byte), then per
                  row pixel count, blank pixels on the left, the pixels

- **verified**: `BATTLEGR.IMG` (21236 bytes) has 201 pictures,
  `COMMONSP.IMG` (4078 bytes) 14; every row of every picture fits.
- `BATTLEGR.IMG` holds the battle's overlays, not the ground: damage
  numbers (-1..-42, white and red), the arrows of the direction cursor,
  the body outlines of the hit locations, clouds (fire and other
  spells), dithered patches in colors 245..254 (pictures 105..112, 18 x
  32 to 83 x 55: spell areas? not ground textures: no picture uses the
  terrain's colors 164..234), missiles in 8 directions (arrows,
  bolts, stones...), sparks, blood. `COMMONSP.IMG`: pieces of a frame
  (borders, a blue panel, a skull). **verified** visually
- Between the pictures (they start at paragraph boundaries) the files
  have leftover bytes, e.g. `$RAUBI0`, `$VILLA0`, `OVEEM`, `CORIDO`:
  memory garbage, not data. An earlier revision of this document read
  them as encounter tables.

### Battle sprites (`*.IMC`, `TACANIM.DB`)

The battle animations. `E00C.CAT` holds the human enemies, `M00C.CAT`
the monsters/undead; `A00C`, `C00C`, `F01C`, `F60C.CAT` hold the party's
figures (same naming: `A00CBA2.IMC`, ...). All are ordinary `.CAT`
catalogs whose entries are `.IMC` files: 333 in all. See `ImcFile.cpp`;
`./darklands --extract E00C.CAT <dir>` exports each one as a sheet (a
row per frame, a column per direction). The format was worked out with
the help of vvendigo's notes and Python readers
(https://github.com/vvendigo/Darklands, `reader_drle.py`,
`reader_imc.py`), then checked against all the files.

**Naming**: `E10WKS2.IMC` = enemy E10, animation set `WK`, weapon `S2`;
`M03DY.IMC` = monster M03 (Skeleton), death animation.

- `E##`/`M##` — the sprite set, the image codes of `DARKLAND.ENM`
- `WK` / `CB` — walking and combat (*inferred*). Enemies have one file
  per weapon they can use (sword, axe, mace, flail, hammer, club,
  crossbow, bow, polearm, lance...); E10 (a mounted raubritter) has
  only lance (`S2`) files; E07 has six weapons.
- `DY` — one per set, the death animation. *inferred* from the name
- Some files are the same animation under several names: `M73WKD7`,
  `M73WKP7` and `M73CBD7` are byte-identical. **verified**
- There are no `M99` (Baphomet) files.

**Compression**: the whole file is compressed with the LZ77 scheme of
LZEXE. **verified**: the decoder ends on the end mark exactly at the last
byte of all 333 files.

    flag bits: 16-bit little-endian words, lowest bit first; the next
    word is read as soon as the last bit of one is taken
    1           a literal byte follows
    0 0 b1 b0   copy (b1 b0) + 2 bytes from (256 - next byte) back
    0 1         two bytes lo, hi: copy from
                0x2000 - (lo | (hi & 0xF8) << 5) back;
                length (hi & 7) + 2, or if that is 2, a length byte n:
                n = 0 the end, n = 1 nothing (a segment change in
                LZEXE), else n + 1 bytes

**Decompressed data**:

    0x00    60 or 80  header, not decoded (80 bytes for WK and CB, 60 for
                  DY; words 0 and 1 are 5, 2 for the humans, like
                  TACANIM.DB)
    H+0x00  2     frame count F
    H+0x02  2     data size: the bytes after the frame table
    H+0x04  16·F  8·F words: the offset of each picture, in 16-byte
                  paragraphs from the end of the table; picture
                  frame · 8 + direction
    ...           the pictures, each at a paragraph boundary:
                  width (byte), height (byte), then per row: pixel
                  count (byte), blank pixels on the left (byte), the
                  pixels

- **verified**: in every file exactly one of the two header sizes makes
  the data size match (60 or 80 bytes decide nothing else), every row
  fits in its picture's width, and the pictures follow one another with
  under 16 bytes of padding.
- Frame counts: 9 for most walks, 7 for most combat sets, 2..4 for the
  deaths. The 8 pictures of a frame are the 8 directions (the sheets
  show one figure turning; *verified* visually, which direction comes
  first is not).
- Index 0 is transparent (the pixels a row skips).
- **Colors**: the battle's palette (DARKLAND.EXE, file 0x1265E, into
  the buffer at [DS:CA53]): `COMNCLRS.DAT`'s 72 colors go to 16..31
  (entries 0..15), 120..163 (16..59) and 243..254 (60..71); one of the
  11 palettes of `BKGNDPAL.DAT` to 164..234, the palette [DS:C7DD]
  (copied from DS:9085; what sets it is not known); 40 colors from the
  level's data (segment 20A5, offset 0x33FC, right after the map) to
  80..119. **verified** (code). These 40 colors are 120 bytes, the size
  of the party's colors in the saved games (0x111, 5 slots · 8 colors):
  presumably member k's colors at 80 + 8k, the figures' pixels 235..242
  drawn with them (*inferred*; with that, the saved party's figures get
  plausible clothes, **verified** visually). The enemies' chunks of `ENEMYPAL.DAT`
  fill 32..79 (see "Enemies"); 0..15 are taken as the EGA colors
  (*inferred*), index 5 being the figures' dark outline and shadow,
  probably drawn as a darkening (*inferred*). The party's figures use
  235..242, their colors (see above). With this
  palette the dragons (16..31) and the spell clouds of `BATTLEGR.IMG`
  (243..254: fire) look right.
  **verified** visually

`TACANIM.DB` (546 bytes = 39 × 14): per sprite set, a 4-byte code
(`A00`, `C00`, `F01`, `F60`, `E00`..`E17`, `M00`..), three words and
four bytes (`05 02 06 03` for the humans, `0a 04 0a 04` for M00, the
wolf). Not decoded.

## Sound

The game's sound is in two kinds of files (README.TXT of the game: "digital
speech" and the music of the sound boards; the INSTALL program offers no
sound, AdLib, Covox Sound Master, Sound Blaster (original, Pro early and
later) and Roland MT-32 / LAPC-1; the README adds the Pro Audio
Spectrum, Thunderboard and ATI F/X among the boards that use the
digital speech).

### Digitized speech (`*.DGT`)

`OPENDARK.DGT` (61,754 bytes), `ENDDARK1.DGT` (60,252) and `ENDDARK2.DGT`
(100,311): raw unsigned 8-bit mono samples, no header. **verified**: the
silence is the value 0x80 (runs of 0x80 and 0x81 at the start) and the sizes
have no room for a header. They go with the animations (`.PAN`): the names
sit next to each other in DARKLAND.EXE's strings, `opendark.dgt` before
`opening2`.., `enddark1.dgt` before `fin0a`..`fin2a` and `enddark2.dgt`
before `fin3a`..`fin5a` (*inferred*: the files each belong to). The
sampling rate is not stored and not found in the code. *Inferred*: 8000 Hz:
the spectra of the three files have no energy above 0.75..0.8 of the
Nyquist frequency, 3.0..3.2 kHz at 8000 Hz, the band of digitized speech
(README.TXT calls them "speech"); the program takes the rate as a parameter
(`--wav`, `--play`; `DigitalSound`).

The game plays them itself, not through the sound drivers (README.TXT: with
the Roland's music files copied over the other drivers the speech still
plays on the board set by INSTALL): CONFIG.DRK (8 bytes, words `2, 0x220,
5, 1`: *inferred* the board's number, its I/O port, its IRQ and a fourth
value, for a Sound Blaster at its default 0x220 and IRQ 5) holds INSTALL's
choice; DARKLAND.CFG (96 bytes) is `nsound.dl` and the path of
`mgraphic.exe` (the sound driver's name and the graphics driver's).

### Sound drivers (`?SOUND.DLB`, `?SOUND.DLC`)

DARKLAND.EXE has the strings `%csound.dlb` and `%csound.dlc` (DS:0FEE and
DS:1140): it loads the driver of the board by its letter, the battle one
(`.DLB`) and the one of the cards (`.DLC`, the map and the cities). The
letters: `A` (name "AdLib"), `I` ("IBM"), `P`, `R` ("RLND": the Roland), `N`
("No Sound", only a `.DLC`); README.TXT says `A` and `P` are the files the
non-Roland boards use and the Roland's `R` files are copied over them to get
its music with the digital speech of another board. **verified** (names,
sizes): each is a DOS executable (MZ, 0x200 bytes of header, relocations
at 0x1E) used as an overlay; its image has: the driver's name and date at
+0x10 (`DKBttleAdLib06-19-92`, `DrkCardAdLib07-29-92`, `DarkBattlIBM
6-18-92`, `P DarkBattle 7-29-92`, `RLND DrkLand06-19-92`, `No Sound
1-04-91`), the segment of its data at +0x2A (relocated), a word count 0x0B
at +0x30 and **eleven far entry points** (words, offsets in the image) from
+0x32: six are code and the last five are `retf` in every driver. The
"No Sound" driver (810 bytes of image) shows their shape: the first
sets a flag at `cs:0x4F` (with the data segment in DS), the third clears it
and returns a word, the fourth returns 0 and the sixth returns a counter
that a near routine (`inc [cs:0x54]`, not in the table: a timer's) increments;
the second and the fifth are a bare `retf`. *Inferred*: start, stop, status
and tick entries. Sizes: the
`.DLC` are 24..61 KB and the `.DLB` 7..13 KB (the music and the effects'
data and the board's code, the battle ones smaller). The music's data
format, the effects ("WFX" strings) and the calls the game makes are
*not decoded*.

## Open questions

- [x] `.CAT`: timestamp encoding — DOS FAT date/time (verified)
- [ ] `.PIC`: does the BCD-packed variant (`'X1'` magic) occur anywhere
      in the game data? In which files?
- [x] `.PIC`: do any images embed a palette? Yes: the `M0` chunk
- [ ] `.PIC`: is the high byte of the magic word at 0x08 always 0x00?
- [x] Palettes: which palette chunks apply to a given enemy — the
      palette fields of its `DARKLAND.ENM` type
- [ ] `DARKLAND.ENM`: the unknown fields of the types (+0x12, +0x13,
      +0x2E, +0x30..) and the enemies' flags
- [ ] Battles: the maps of `IMAPS.CAT` (the wall, object and ground
      values, the records after the grids) and how the game draws them; `TACANIM.DB`; the `.IMC`
      header; which of the 8 directions is which
- [x] Palettes: the format of `BKGNDPAL.DAT` — 11 palettes × 71 colors
- [x] Palettes: which indices `BKGNDPAL.DAT` patches — 164..234, in
      the battles
- [ ] Palettes: what selects one of the 11 palettes of `BKGNDPAL.DAT`
- [ ] `.PIC`: are there chunk types other than `M0` and `X0`/`X1`?
- [ ] Palettes: is index 0 always the color key, across all resource
      types?
- [x] `.MAP`: the tile column-selection rule — diagonal-neighbor mask
- [ ] `.MAP`: the exact "joins" groups for roads and rivers (castles?
      tidal marshes?)
- [ ] `.MAP`: what is drawn under tiles whose cells are partly empty
      (index 0) — e.g. the sparse `F/W` rows render over black; the game
      probably fills a background color first
- [x] `.MAP`: the icon sheet format — regular PIC with an `M0` chunk
- [x] `.MAP`: which palette applies to the map tiles — the one embedded
      in the icon sheets
- [ ] `.LOC`: the meaning of location types 1..4, 6, 8 and of the
      unknown record fields
- [ ] Fonts: meaning of header bytes +5 and +6 (glyph gap and extra
      row are inferred from data sizes and glyph shapes)
- [ ] `.CTY`: the second map tile (+0x46), the fields from +0x54 to
      +0x6D
- [x] `.MSG`: the card format (`MSGFILES`) and `DARKLAND.MSG`
- [ ] `.MSG`: codes `0x13` and `0x01`; which picture goes with a card;
      how options that do not apply to a city are hidden; how the game
      chooses between cards (day/night, fair, war...)
- [ ] `.DSC`: the meaning of byte 0 (0x5E)
- [ ] Characters: the unknown fields of the record (0x00..0x11, 0x49:
      grows during the game), the sex byte (one female sample), where
      the starting money comes from
- [ ] Saved games: 0x76..0x79, 0x84..0x9F (the quests' state), 0xA2,
      0xA3, 0xA6; the layout of the saved caches with items in them
      (read and written as DARKLAND.EXE's code has it, but no sample has
      any)
- [ ] Information screens: the words for fame, the carrying capacity
- [ ] Trade: which shop call is which place (two masks for some
      guilds), what the location flags mean
- [x] DARKLAND.EXE: the travel speeds (see exe.md)
- [ ] DARKLAND.EXE: the divine favor rules, the seed of the merchants'
      stock, the item that lets the party enter water
- [ ] Card screen: the real colors (paper, text, highlight), the
      crimson option letters, the party sidebar
- [ ] The sound drivers' music and effects data (`.DLB`/`.DLC`, see
  "Sound"); the sampling rate of the `.DGT` speech (8000 Hz is a guess)

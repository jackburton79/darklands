# Darklands data format notes

Working notes from reverse engineering the original game's data files,
as implemented in this project. Corrections and additions are welcome —
please open an issue or a pull request.

References:

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
  must happen elsewhere — probably the `.ENM` enemy files. *unresolved*
- `BKGNDPAL.DAT` does **not** follow this layout — see next section.

## Background palettes (`BKGNDPAL.DAT`)

2343 bytes = 3 · 11 · 71: **11 palettes of 71 colors each**, plain RGB
triplets with 6-bit components, no header and no index byte.

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
  palette embedded in the map icon sheets. Which palette indices the
  71 entries patch, and what selects one of the 11 (location type? time
  of day?) is unresolved.

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
    +0x0A   7     unknown, varies
    +0x11   1     cities: size, 3..8; other locations: 1 (see below)
    +0x12   20    unknown; constant in the game data except +0x1C
                  (0x19 0x19 0x19 at +0x15, 0xFF 0xFF at +0x18)
    +0x26   20    name, NUL-padded

- **Coordinates** — **verified**: 86 of the 92 cities (type 0) sit
  exactly on a city map tile (type 29); the other 6 (e.g. Berlin,
  Kassel, Frankfurt) are right next to one — large cities span several
  tiles. One cave has y = 931, one past the last map row.
- **Types**, from the names (*inferred*): 0 city (92), 1 and 8 mostly
  villages/castles (73 and 112), 2 and 3 monasteries/abbeys?,
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
    +0x56   2     unknown; equals the record index or index + 1
    +0x58   ...   unknown (small values, then larger words)
    +0x6E   32·16 names of the city's places, by slot (see below)

- **Neighbors** — **verified** as a network of nearby cities: of 186
  links, 176 are symmetric; linked cities are a median 42 tiles apart,
  against 189 for arbitrary pairs (e.g. Hamburg: Lüneburg, Brandenburg,
  Magdeburg, Bremen; Köln: Duisburg, Koblenz). Some cities have none.
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
    +0x03   1     text right limit
    +0x04   1     unknown, 0
    +0x05   ...   text, NUL-terminated (game character set + control codes)

- **verified**: all 419 files parse to exactly their last byte.
- Header meaning from the wendigo reference (top/left relative to the
  inner edge of the card frame, the column is `right − left` wide):
  *unverified* until cards are rendered. 3248 of the 3756 cards have
  `0A 0A 00 F0 00` (10, 10, 240); 3344 have a right limit of 240, the
  others range from 3 to 255.
- Bytes +2 and +4 are 0 in every card except the 76 of `$MCGUF07.MSG`
  (placeholders: header bytes that look like garbage, text `16 0A 0A`)
  and card 7 of `$RAUBI05.MSG` (`02 00 00 03 01`).

### Text control codes

| Code | Meaning |
|------|---------|
| `0x0A` | line break |
| `0x14` | paragraph: follows a `0x0A`; two in a row before the options |
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
`$Number1`, `$Text1`, `$CityLordName`. The ones named after a place
match the place slots of `DARKLAND.CTY` (*inferred* from the names and
the texts): `$citySquare` 3, `$councilHall` 4, `$fortress` 5,
`$cathedral` 6, `$cityChurch` 7, `$marketplace` 8, `$slum` 10,
`$pawnshop` 12, `$monastery` 13, `$Inn` 14, `$university` 15.

### Where the cards are used

- Which card is shown, with which picture, and what an option does is
  decided by `DARKLAND.EXE`: the card file names do not appear in it as
  plain text. Only the texts tell what a card is for.
- The game starts with `$PARTY02.MSG`: card 0 for a party ("You gather
  around the comfortable fire at the $Inn...", with a leftover "test
  mines" option), card 1 for a single character. `$MAINS01.MSG` is the
  main street menu. *inferred* from the texts and the manual.
- Card frame pictures named in `DARKLAND.EXE`: `TEXTBACK.PIC` (158 × 50,
  text background), `RPBDRTOP`/`RPBDRBTM` (245 × 6), `RPBDRLFT` (7 × 200),
  `RPBDRRGT` (8 × 200) and `ILLMCAPS.PIC` (320 × 200 with a palette,
  presumably the illuminated capitals). *inferred*

## Messages (`DARKLAND.MSG`)

Not a card deck: **4001 = 1 + 10 · 400** (**verified**). Byte 0 is the
record count (10), then 10 records of 400 bytes, each a NUL-padded text
in the game character set. Only three are used: "Error Message" and
two travel messages (entering a robber knight's territory, a blighted
land).

## Open questions

- [x] `.CAT`: timestamp encoding — DOS FAT date/time (verified)
- [ ] `.PIC`: does the BCD-packed variant (`'X1'` magic) occur anywhere
      in the game data? In which files?
- [x] `.PIC`: do any images embed a palette? Yes: the `M0` chunk
- [ ] `.PIC`: is the high byte of the magic word at 0x08 always 0x00?
- [ ] Palettes: which palette chunk(s) apply to a given enemy, and where
      is that mapping stored? (probably `.ENM`)
- [x] Palettes: the format of `BKGNDPAL.DAT` — 11 palettes × 71 colors
- [ ] Palettes: which indices `BKGNDPAL.DAT` patches and what selects
      one of its 11 palettes
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
- [ ] `.MSG`: the card header bytes (check by rendering), codes `0x13`
      and `0x01`, which picture goes with a card
- [ ] Other resource formats: `.DLB`/`.DLC` sound archives, ...

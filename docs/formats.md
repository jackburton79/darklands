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

### Tile column rule — **unresolved**

Which column of the icon sheet to use is derived from neighboring tiles.
The wendigo reference describes a recipe using the four diagonal
neighbors' tile-row bits (with odd/even row coordinate shifts), but notes
itself that it does not produce correct results. Our rendering confirms
terrain *types* are right without any column logic, so all column
information must come from context. This remains the main open problem
for map rendering; roads and coastlines are the best test cases (their
correct connectivity is visually unambiguous).

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
- [ ] `.MAP`: the tile column-selection rule — how the sheet column
      derives from neighboring tiles (wendigo's diagonal-neighbor recipe
      is acknowledged broken; our renders confirm terrain *types* need no
      column data, so it is purely contextual — roads and coasts are the
      best test cases)
- [x] `.MAP`: the icon sheet format — regular PIC with an `M0` chunk
- [x] `.MAP`: which palette applies to the map tiles — the one embedded
      in the icon sheets
- [ ] Other resource formats: `.DLB`/`.DLC` sound archives, `FONTS.FNT`,
      `DARKLAND.MSG`, `.LOC`/`.CTY`, ...

# Darklands data format notes

Working notes from reverse engineering the original game's data files,
as implemented in this project. Corrections and additions are welcome —
please open an issue or a pull request.

References:

- File formats overview: <https://wendigo.online-siesta.com/darklands/file_formats/up-to-date/>
- PIC decompression algorithm: <https://github.com/ogamespec/PicDecoder>

Conventions:

- All multi-byte integers are **little-endian**.
- Offsets are hexadecimal, relative to the start of the file/resource.
- Facts marked **verified** were confirmed against original game data
  (all 60 entries of `EINFO.CAT`, `ENEMYPAL.DAT`) and by visual
  comparison with rendered output. Anything else is marked *inferred*
  or *unverified*.

## Catalog archives (`.CAT`)

Catalogs bundle related resources (images, sounds, text) into one file.
`EINFO.CAT`, for example, holds the 60 bestiary portraits.

    offset  size  description
    0x00    2     entry count (N)
    0x02    24·N  entry table, 24 bytes per entry:

    entry (relative offsets):
    +0x00   12    resource name: DOS 8.3 style, up to 12 characters,
                  padded with spaces (strip trailing spaces)
    +0x0C   4     timestamp (uint32; encoding unverified,
                  probably DOS FAT packed date/time)
    +0x10   4     data length in bytes (uint32)
    +0x14   4     data offset (uint32; absolute, from start of the .CAT file)

- Resource data follows the entry table; each blob is located with its
  entry's offset/length pair.
- Offsets and lengths must be validated against the file size before use:
  a corrupt catalog must never produce out-of-bounds reads.

## PIC images (`.PIC`)

Palettized bitmap images, compressed with a two-stage scheme:
an LZW-like adaptive code followed by run-length encoding.
Decoding is *output-driven*: exactly `width × height` bytes are produced;
any unread bytes after the declared data end are ignored.

### Header (10 bytes)

    offset  size  description
    0x00    2     magic: low byte always 'X' (0x58);
                  high byte '0' (0x30) or '1' (0x31) — see "BCD packing"
    0x02    2     size (uint16): number of bytes from 0x04 to the end of
                  the image data. Equivalently: file size == this value + 4.
                  **verified** across all 60 entries of EINFO.CAT.
                  (A uint16 implies PIC resources are at most 65539 bytes.)
    0x04    2     width in pixels (uint16)
    0x06    2     height in pixels (uint16)
    0x08    2     "magic word" (uint16): low byte = maximum LZW code length
                  in bits, clamped to 11; high byte = initial bit buffer
                  content (always 0x00 in the files seen so far)

The compressed bitstream starts at `0x0A` and is
`size − 6` bytes long (everything up to the end of the resource).

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
- The actual colors come from **external palette files** — e.g. enemy
  graphics index into the 256-color palette assembled from
  `ENEMYPAL.DAT` (see next section). Observed index ranges in one
  sprite: 0, 33–44, 72–73; each range falls inside a palette chunk's
  16-entry slice.
- Index 0 is most likely the **transparency/color key**: it typically
  accounts for ~2/3 of a sprite's pixels (the uniform background).
  *inferred*
- Whether any PIC files embed a palette is an open question.

## Palette chunk files (`ENEMYPAL.DAT`, probably `BKGNDPAL.DAT`)

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
- `BKGNDPAL.DAT` presumably follows the same layout (*unverified* — if
  its size is not a multiple of 53, it does not).

## World map (DARKLAND.MAP)

The wilderness map of the Holy Roman Empire: a grid of 327 × 931 tiles,stored RLE-compressed, one stream per row. See MapFile.cpp for areference implementation.

Mixed endianness — the trap. The two dimension words at the start ofthe file are big-endian, but the row offset table that follows them islittle-endian. Both differ from the rest of the game's formats(catalogs and PIC images are little-endian throughout). Getting this wrongproduces offsets that look almost plausible and then dissolve into garbage;validate rows[0] == end of table before trusting anything else.

offset  size       description0x00    2          max_x = 0x0147 (327), word, **big-endian**0x02    2          max_y = 0x03A3 (931), word, **big-endian**0x04    4 · max_y  row_offsets[max_y], dwords, **little-endian**:                   file offset of each row's RLE data0x0E90  ...        row data; the first row begins immediately after                   the table (**verified**: rows[0] == 0x04 + 4 · max_y)

Row sizes vary (observed: 60..210 bytes, average ~155; the theoreticalRLE bounds for a 327-tile row are 47..327). The last row's length isimplicit: it extends to the end of the file.
Row encoding (RLE)

Each row is a stream of bytes:

bit:   7 6 5   4   3 2 1 0       R R R   P   T T T T

    bits 7..5 (R): repeat count, values 1..7 — a count of 0 is invalid(verified: not a single such byte occurs in the entire file)
    bit 4 (P): palette set: 0 = MAPICONS.PIC, 1 = MAPICON2.PIC
    bits 3..0 (T): tile row within that icon sheet

Each byte expands to repeat identical tiles. Every row decodes toexactly max_x = 327 tiles (verified for all 931 rows; 304,437 tilestotal, exactly max_x · max_y).
Tile geometry and appearance

Tiles are 16 px wide; rows are vertically offset by half a tile(hexagonal-style layout): odd rows are drawn shifted right by 8 px, andeach row is vertically half a tile below the previous one.

The tile's row within the icon sheet comes directly from the byte(bits 3..0). Which column of the sheet to use is derived from thesurrounding tiles: the wendigo reference describes a recipe based on thefour diagonally adjacent tiles (with odd/even row coordinate shifts), butnotes themselves that it does not produce correct results ("The bitsseems to be set by adjacent tile similarity ... river binds to bridgetile, wet tiles bind together, etc."). This remains unresolved —see open questions.
## Open questions

- [ ] `.CAT`: timestamp encoding — DOS FAT date/time? (decode a few and
      check for plausible 1992–1995 dates)
- [ ] `.PIC`: does the BCD-packed variant (`'X1'` magic) occur anywhere
      in the game data? In which files?
- [ ] `.PIC`: do any images embed a palette? If so, where in the file?
- [ ] `.PIC`: is the high byte of the magic word at 0x08 always 0x00?
- [ ] Palettes: which palette chunk(s) apply to a given enemy, and where
      is that mapping stored? (probably `.ENM`)
- [ ] Palettes: does `BKGNDPAL.DAT` use the same 53-byte chunk layout?
- [ ] Palettes: is index 0 always the color key, across all resource
      types?
- [ ] Other resource formats: `.DLB`/`.DLC` sound archives, `FONTS.FNT`,
      `DARKLAND.MSG`, `.MAP`/`.LOC`/`.CTY`, ...


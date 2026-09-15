# Darklands data format notes

Working notes from reverse engineering the original game's data files,
as implemented in this project. Corrections and additions are welcome —
please open an issue or a pull request.

References:

- File formats overview: <https://wendigo.online-siesta.com/darklands/file_formats/up-to-date/>
- PIC decompression algorithm: <https://github.com/ogamespec/PicDecoder>
- EGA palette values: <https://moddingwiki.shikadi.net/wiki/EGA_Palette>

Conventions:

- All multi-byte integers are **little-endian**.
- Offsets are hexadecimal, relative to the start of the file/resource.
- Facts marked **verified** were confirmed against original game data
  (all 60 entries of `EINFO.CAT`) and by visual comparison with the
  original game. Anything else is marked *inferred* or *unverified*.

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

- Images are 8-bit indexed; color indexes are masked to 0..15 and map to
  the standard 16-color EGA palette (see link above).
- Whether any PIC files embed a custom palette is an open question.

## Open questions

- [ ] `.CAT`: timestamp encoding — DOS FAT date/time? (decode a few and
      check for plausible 1992–1995 dates)
- [ ] `.PIC`: does the BCD-packed variant (`'X1'` magic) occur anywhere
      in the game data? In which files?
- [ ] `.PIC`: do any images embed a palette? If so, where in the file?
- [ ] `.PIC`: is the high byte of the magic word at 0x08 always 0x00?
- [ ] `.PIC`: can pre-mask decoded bytes ever exceed 15 in non-BCD images?
- [ ] Other resource formats: `.DLB`/`.DLC` sound archives, `FONTS.FNT`,
      `DARKLAND.MSG`, `.MAP`/`.LOC`/`.CTY`, ...

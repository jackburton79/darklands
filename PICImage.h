/*
 * PICImage.h
 * Decoder for the game's .PIC image format.
 *
 * A sequence of chunks (little-endian): uint16 tag, uint16 length, data.
 * An optional "M0" palette chunk may precede the image chunk.
 *
 * Image chunk header (offsets relative to the chunk):
 *   0x00  uint16  magic "X0" / "X1" (low byte 'X', high byte: bit 0 = BCD packed)
 *   0x02  uint16  chunk length (bytes from 0x04 to the end of the data)
 *   0x04  uint16  width
 *   0x06  uint16  height
 *   0x08  uint16  magic word: high byte = initial bit-stream bits,
 *                 low byte = initial LUT code length (clamped to 11)
 *   0x0A  ...     compressed data
 */

#pragma once

#include "SupportDefs.h"

#include <vector>

class Bitmap;
class Stream;

namespace GFX {
    struct Palette;
};

/**
   The constructor parses and validates the PIC header; it throws
   std::runtime_error if the stream does not contain a valid one.
   The stream is NOT owned and must stay alive as long as the PICImage does;
   Image() reads via ReadAt() only and never moves the stream's position.
   The stream must support Size().

   Image() returns a newly allocated Bitmap owned by the caller
   (Bitmap is reference-counted: call Release() when done).
*/
class PICImage {
public:
    explicit		PICImage(Stream* stream);
                    ~PICImage();

    uint16			Width() const	{ return fWidth; }
    uint16			Height() const	{ return fHeight; }

    // With a NULL palette, uses the embedded palette if there is one
    // (EGA colors otherwise).
    Bitmap*			Image(const GFX::Palette* palette = nullptr) const;

    // Embedded "M0" palette chunk: HasPalette() tells whether the file has
    // one; ApplyPalette() overwrites the range it covers in `palette`.
    bool			HasPalette() const;
    void			ApplyPalette(GFX::Palette& palette) const;

    // Decodes and returns the raw 8-bit pixel data, row-major,
    // Width() * Height() bytes. No Bitmap, no palette involved.
    std::vector<uint8>	RawBytes() const;

    // Convenience: parse + decode in one call.
    static Bitmap*	Decode(Stream* stream,
                        const GFX::Palette* palette = nullptr);

private:
    Stream*			fStream;
    size_t			fImageOffset;
    uint16			fCompressedSize;
    uint16			fWidth;
    uint16			fHeight;
    uint16			fMagicWord;
    bool			fBCDPacked;
    uint8			fPaletteFirst;
    std::vector<uint8>	fPalette;		// raw 6-bit RGB triplets
};

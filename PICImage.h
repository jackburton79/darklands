/*
 * PICImage.h
 * Decoder for the game's .PIC image format.
 *
 * Header (little-endian):
 *   0x00  uint16  magic "X0" / "X1" (low byte 'X', high byte: bit 0 = BCD packed)
 *   0x02  uint16  compressed data size
 *   0x04  uint16  width
 *   0x06  uint16  height
 *   0x08  uint16  magic word: high byte = initial bit-stream bits,
 *                 low byte = initial LUT code length (clamped to 11)
 *   0x0A  ...     compressed data
 */

#pragma once

#include "SupportDefs.h"

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

    Bitmap*			Image(const GFX::Palette* palette = nullptr) const;

    // Convenience: parse + decode in one call.
    static Bitmap*	Decode(Stream* stream,
                        const GFX::Palette* palette = nullptr);

private:
    Stream*			fStream;
    uint16			fCompressedSize;
    uint16			fWidth;
    uint16			fHeight;
    uint16			fMagicWord;
    bool			fBCDPacked;
};

/*
 * ImcFile.h
 * Decoder for the battle sprites (.IMC entries of E00C.CAT, M00C.CAT,
 * A00C.CAT...): one animation of a figure, a few frames seen from 8
 * directions.
 *
 * The file is compressed with the LZ77 scheme of LZEXE; the data holds
 * a header, a frame table and the pictures, row by row (skip, pixels).
 * See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <vector>

class Stream;

// One picture; index 0 is transparent
struct imc_sprite {
    uint16 width;
    uint16 height;
    std::vector<uint8> pixels;	// width * height
};

class ImcFile {
public:
    static const int	kDirectionCount = 8;

    // Reads the whole stream (not owned; it must support Size()).
    // Throws std::runtime_error on invalid data.
    explicit		ImcFile(Stream* stream);

    int				CountFrames() const;
    // Throws std::out_of_range on invalid arguments.
    const imc_sprite& SpriteAt(int frame, int direction) const;

    // LZEXE-style decompression, exposed for testing.
    static std::vector<uint8> Decompress(const std::vector<uint8>& data);

private:
    std::vector<imc_sprite> fSprites;	// frame * kDirectionCount + direction
};

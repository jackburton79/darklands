/*
 * ImcFile.h
 * Decoder for the battle sprites (.IMC entries of E00C.CAT, M00C.CAT,
 * A00C.CAT...): one animation of a figure, a few frames seen from 8
 * directions.
 *
 * The file is compressed with the LZ77 scheme of LZEXE; the data holds
 * a header, a frame table and the pictures (Sprite.h).
 * See docs/formats.md.
 */
#pragma once

#include "Sprite.h"
#include "SupportDefs.h"

#include <vector>

class Stream;

class ImcFile {
public:
    static const int	kDirectionCount = 8;

    // Reads the whole stream (not owned; it must support Size()).
    // Throws std::runtime_error on invalid data.
    explicit		ImcFile(Stream* stream);

    int				CountFrames() const;
    // Throws std::out_of_range on invalid arguments.
    const sprite&	SpriteAt(int frame, int direction) const;

private:
    std::vector<sprite> fSprites;	// frame * kDirectionCount + direction
};

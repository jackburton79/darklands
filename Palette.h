/*
 * Palette.h
 * Reader for Darklands palette chunk files (ENEMYPAL.DAT, presumably
 * BKGNDPAL.DAT as well).
 *
 * Format: array of 53-byte chunks, each patching 16 entries of a shared
 * 256-color palette. See docs/formats.md for details.
 */

#pragma once

#include "SupportDefs.h"
#include "GraphicsDefs.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace GFX {
    struct Palette;
};

struct palette_chunk {
    uint8		startOffset;	// divide by 3: first of this chunk's
                                // 16 palette indices
    GFX::Color	colors[16];		// scaled to 0..255
};

class PaletteFile {
public:
    explicit		PaletteFile(const std::string& fileName);

    uint32			CountChunks() const;
                    // Throws std::out_of_range on invalid index.
    const palette_chunk& ChunkAt(uint32 index) const;

                    // Patches one chunk into the palette.
    void			ApplyChunk(GFX::Palette& palette, uint32 index) const;
                    // Patches all chunks, in file order (later chunks
                    // overwrite earlier ones where ranges overlap).
    void			ApplyAll(GFX::Palette& palette) const;

    void			Dump(std::ostream& output) const;

private:
    typedef std::vector<palette_chunk> ChunkList;
    ChunkList		fChunks;
};

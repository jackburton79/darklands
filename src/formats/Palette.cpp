#include "Palette.h"

#include "FileStream.h"
#include "GraphicsDefs.h"
#include "Stream.h"

#include <climits>
#include <ostream>
#include <stdexcept>

static const size_t kChunkSize		= 53;		// 1 + 48 + 4; verified: file
                                                // size == chunk count * 53
static const size_t kChunkColors	= 16;

// Scale a 6-bit VGA DAC value to 8 bits: 0 -> 0, 63 -> 255.
static inline uint8
Scale6To8(uint8 value)
{
    return uint8((value << 2) | (value >> 4));
}


uint8
NearestColor(const GFX::Palette& palette, int r, int g, int b)
{
    int best = 0;
    int bestDistance = INT_MAX;
    for (int i = 0; i < 256; i++) {
        const GFX::Color& c = palette.colors[i];
        const int distance = (c.r - r) * (c.r - r) + (c.g - g) * (c.g - g)
            + (c.b - b) * (c.b - b);
        if (distance < bestDistance) {
            best = i;
            bestDistance = distance;
        }
    }
    return uint8(best);
}


PaletteFile::PaletteFile(const std::string& fileName)
{
    Stream* stream = new FileStream(fileName.c_str(), FileStream::READ_ONLY);
    try {
        const size_t size = stream->Size();
        if (size == 0 || size % kChunkSize != 0) {
            throw std::runtime_error("PaletteFile: " + fileName
                + ": unexpected size (" + std::to_string(size) + ")");
        }

        fChunks.reserve(size / kChunkSize);
        for (size_t c = 0; c < size / kChunkSize; c++) {
            palette_chunk chunk;
            chunk.startOffset = stream->ReadByte();
            for (int i = 0; i < int(kChunkColors); i++) {
                // RGB triplets, 6-bit components
                const uint8 r = stream->ReadByte();
                const uint8 g = stream->ReadByte();
                const uint8 b = stream->ReadByte();
                chunk.colors[i] = GFX::Color{
                    Scale6To8(r), Scale6To8(g), Scale6To8(b), 0 };
            }
            stream->Seek(4, SEEK_CUR);		// unknown trailing bytes
            fChunks.push_back(chunk);
        }
    } catch (...) {
        delete stream;
        throw;
    }
    delete stream;
}


uint32
PaletteFile::CountChunks() const
{
    return uint32(fChunks.size());
}


const palette_chunk&
PaletteFile::ChunkAt(uint32 index) const
{
    if (index >= fChunks.size())
        throw std::out_of_range("PaletteFile::ChunkAt(): invalid index");
    return fChunks[index];
}


void
PaletteFile::ApplyChunk(GFX::Palette& palette, uint32 index) const
{
    const palette_chunk& chunk = ChunkAt(index);
    const int base = chunk.startOffset / 3;
    for (int i = 0; i < int(kChunkColors) && base + i < 256; i++)
        palette.colors[base + i] = chunk.colors[i];
}


void
PaletteFile::ApplyAll(GFX::Palette& palette) const
{
    for (uint32 i = 0; i < fChunks.size(); i++)
        ApplyChunk(palette, i);
}


void
PaletteFile::Dump(std::ostream& output) const
{
    for (uint32 i = 0; i < fChunks.size(); i++) {
        const palette_chunk& chunk = fChunks[i];
        output << "chunk " << i << ": start " << int(chunk.startOffset)
            << " -> index " << int(chunk.startOffset / 3)
            << (chunk.startOffset % 3 != 0 ? "   <-- NOT multiple of 3!" : "")
            << std::endl;
    }
}
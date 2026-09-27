#include "BattleMap.h"

#include "Lzexe.h"
#include "Stream.h"

#include <stdexcept>

// verified: all 143 maps decompress to this size
static const size_t kMapSize		= 13308;
static const size_t kCellSize		= 4;
static const size_t kGroundOffset	= 0x1900;	// after 40 x 40 cells


BattleMap::BattleMap(Stream* stream)
{
    std::vector<uint8> compressed(stream->Size());
    if (stream->ReadAt(0, compressed.data(), compressed.size())
            != (ssize_t)compressed.size()) {
        throw std::runtime_error("BattleMap: read error");
    }
    const std::vector<uint8> data = LzexeDecompress(compressed);
    if (data.size() != kMapSize)
        throw std::runtime_error("BattleMap: unexpected size");

    fCells.resize(kSize * kSize);
    for (size_t i = 0; i < fCells.size(); i++) {
        for (size_t b = 0; b < kCellSize; b++)
            fCells[i].bytes[b] = data[i * kCellSize + b];
        fCells[i].ground = data[kGroundOffset + i];
    }
}


const battle_cell&
BattleMap::CellAt(int x, int y) const
{
    if (x < 0 || x >= kSize || y < 0 || y >= kSize)
        throw std::out_of_range("BattleMap::CellAt(): invalid cell");
    return fCells[y * kSize + x];
}

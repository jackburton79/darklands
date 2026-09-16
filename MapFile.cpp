#include "MapFile.h"

#include "FileStream.h"
#include "Stream.h"

#include <stdexcept>

// map_tile from a raw RLE payload byte: RRR P TTTT
static map_tile
DecodeTileByte(uint8 byte)
{
    map_tile tile;
    tile.row = byte & 0x0F;
    tile.secondPalette = (byte >> 4) & 1;
    return tile;
}


MapFile::MapFile(const std::string& fileName)
    :
    fStream(NULL),
    fWidth(0),
    fHeight(0)
{
    fStream = new FileStream(fileName.c_str(), FileStream::READ_ONLY);
    try {
        // dimension words are BIG-endian
        fWidth = fStream->ReadWordBEAt(0x00);
        fHeight = fStream->ReadWordBEAt(0x02);
        if (fWidth == 0 || fHeight == 0)
            throw std::runtime_error("MapFile: empty map");

        // row offset table is LITTLE-endian (see docs/formats.md)
        fRowOffsets.resize(fHeight);
        for (uint32 i = 0; i < fHeight; i++)
            fRowOffsets[i] = fStream->ReadDWordLEAt(0x04 + 4 * i);

        // validation: table must end at or before the first row,
        // offsets must be sane and increasing
        const uint32 tableEnd = 0x04 + 4 * fHeight;
        if (fRowOffsets[0] != tableEnd)
            throw std::runtime_error("MapFile: first row does not follow table");
        for (uint32 i = 1; i < fHeight; i++) {
            if (fRowOffsets[i] <= fRowOffsets[i - 1])
                throw std::runtime_error("MapFile: row offsets not increasing");
        }
        if (fRowOffsets[fHeight - 1] >= fStream->Size())
            throw std::runtime_error("MapFile: last row out of bounds");
    } catch (...) {
        delete fStream;
        fStream = NULL;
        throw;
    }
}


MapFile::~MapFile()
{
    delete fStream;
}


std::vector<uint8>
MapFile::RawRow(uint16 y) const
{
    if (y >= fHeight)
        throw std::out_of_range("MapFile::RawRow(): invalid row");

    const uint32 offset = fRowOffsets[y];
    const uint32 end = (y + 1 < fHeight) ? fRowOffsets[y + 1]
                                           : (uint32)fStream->Size();
    if (end <= offset)
        throw std::runtime_error("MapFile: empty row");

    std::vector<uint8> data(end - offset);
    if (fStream->ReadAt(offset, data.data(), data.size())
            != (ssize_t)data.size()) {
        throw std::runtime_error("MapFile: truncated row");
    }
    return data;
}


std::vector<map_tile>
MapFile::Row(uint16 y) const
{
    const std::vector<uint8> data = RawRow(y);

    std::vector<map_tile> tiles;
    tiles.reserve(fWidth);
    for (size_t i = 0; i < data.size(); i++) {
        const uint8 repeat = data[i] >> 5;
        if (repeat == 0)
            throw std::runtime_error("MapFile: invalid RLE byte");
        const map_tile tile = DecodeTileByte(data[i]);
        for (uint8 r = 0; r < repeat; r++)
            tiles.push_back(tile);
    }
    if (tiles.size() != fWidth)
        throw std::runtime_error("MapFile: row decodes to wrong tile count");
    return tiles;
}

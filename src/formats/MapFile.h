/*
 * MapFile.h
 * Reader for DARKLAND.MAP, the wilderness map.
 * See docs/formats.md. NOTE the mixed endianness (header BE, table LE).
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

class Stream;

// One decoded map tile.
struct map_tile {
    uint8 row;				// tile row within the icon sheet (0..15)
    bool secondPalette;		// false: MAPICONS.PIC, true: MAPICON2.PIC
};

class MapFile {
public:
    explicit		MapFile(const std::string& fileName);	// throws on error
                    ~MapFile();

    uint16			Width() const	{ return fWidth; }
    uint16			Height() const	{ return fHeight; }

    // Decodes row y (RLE) into exactly Width() tiles.
    // Throws std::runtime_error on invalid RLE data or truncated rows.
    std::vector<map_tile> Row(uint16 y) const;

                    // Raw alternative: undecoded RLE bytes of row y.
    std::vector<uint8> RawRow(uint16 y) const;

private:
                    MapFile(const MapFile&);			// not copyable
    MapFile&		operator=(const MapFile&);

    Stream*				fStream;
    uint16				fWidth;
    uint16				fHeight;
    std::vector<uint32>	fRowOffsets;
};

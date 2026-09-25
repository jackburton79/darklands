/*
 * LocationFile.h
 * Reader for DARKLAND.LOC, the list of places on the world map
 * (cities, castles, villages, caves, shrines...). See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

// One decoded location record.
struct location {
    uint16 type;			// 0 = city; see docs/formats.md
    uint16 x;				// map tile coordinates (DARKLAND.MAP)
    uint16 y;
    uint8 size;				// cities: 3..8 (inferred); others: 1
    std::string name;		// UTF-8
};

class LocationFile {
public:
    explicit		LocationFile(const std::string& fileName);	// throws on error

    uint32			CountLocations() const;
    const location&	LocationAt(uint32 index) const;

    // Converts a name from the game's character set (ASCII with '|' = ü,
    // '{' = ö, 0x1F = ä) to UTF-8.
    static std::string	DecodeName(const char* name, size_t length);

private:
    std::vector<location>	fLocations;
};

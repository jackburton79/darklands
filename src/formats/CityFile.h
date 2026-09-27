/*
 * CityFile.h
 * Reader for DARKLAND.CTY, the city descriptions. Record i describes
 * location i of DARKLAND.LOC (the cities come first there).
 * See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

// Named places of a city, by slot. Empty names mean "not present".
enum city_place {
    CITY_RULER = 0,			// e.g. "King of Dänemark", "Rat of the Reichstädte"
    CITY_SECOND_POWER,		// second authority (may be empty)
    CITY_FAMOUS_PLACE,		// always "Famous Place"
    CITY_SQUARE,			// main square
    CITY_TOWN_HALL,
    CITY_CASTLE,
    CITY_CATHEDRAL,
    CITY_CHURCH,
    CITY_MARKET,
    CITY_MINT_SQUARE,		// "Munzenplatz", only 6 cities
    CITY_SLUMS,
    CITY_ARMORY,			// "Zeughaus", or a gate/tower
    CITY_PAWNSHOP,
    CITY_MONASTERY,
    CITY_INN,
    CITY_UNIVERSITY,
    CITY_PLACE_COUNT
};

// The shops a city may have (DARKLAND.EXE lists their names in this
// order; see docs/formats.md)
enum city_shop {
    SHOP_BLACKSMITH = 0,
    SHOP_GOODS_MERCHANT,
    SHOP_SWORDSMITH,
    SHOP_ARMORER,
    SHOP_GUNSMITH,
    SHOP_BOWYER,
    SHOP_ARTIFICER,
    SHOP_JEWELER,
    SHOP_CLOTHMAKER,
    CITY_SHOP_COUNT
};

// Bits of city::flags (DARKLAND.EXE, 0E76:1A8E); the others are not
// decoded
enum city_flag {
    CITY_HAS_PAWNSHOP		= 0x0200	// the market's Leihhaus
};

// Who rules a city (DARKLAND.EXE: the card of the approach, and
// $CityLordTitle)
enum city_rule {
    CITY_CAPITAL = 0,			// its ruler's seat
    CITY_RULED = 1,				// ruled for its lord by a Vogt, a Burggraf...
    CITY_FREE = 2				// a free city, with its council
};

// Which sea a port city is on.
enum city_harbor {
    CITY_HARBOR_NORTH_SEA	= 0,
    CITY_HARBOR_BALTIC		= 1,
    CITY_HARBOR_NONE		= 0xFFFF	// inland
};

// One decoded city record. Names are UTF-8.
struct city {
    std::string shortName;
    std::string fullName;		// e.g. "Frankfurt am Main"
    uint8 size;					// 3..8, same as DARKLAND.LOC
    uint16 x;					// map tile, same as DARKLAND.LOC
    uint16 y;
    uint16 x2;					// a map tile close to the city (purpose unknown)
    uint16 y2;
    std::vector<uint16> neighbors;	// indices of nearby cities
    uint16 harbor;				// see city_harbor
    uint16 flags;				// see city_flag
    uint16 peopleSeed;			// +0x56: DARKLAND.EXE seeds the names and
                                // skills of the city's people with it
    uint16 rule;				// +0x58: city_rule
    uint16 ruleFlagged;			// +0x5A: the same, while the location's
                                // flags have bit 0x80 (0E76:1A7E(0x28))
    uint8 shopQuality[CITY_SHOP_COUNT];	// the quality of its goods; 0: the
                                        // city has no such shop
    std::string places[CITY_PLACE_COUNT];
};

class CityFile {
public:
    explicit		CityFile(const std::string& fileName);	// throws on error

    uint32			CountCities() const;
    const city&		CityAt(uint32 index) const;

private:
    std::vector<city>	fCities;
};

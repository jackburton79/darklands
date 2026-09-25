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

/*
 * GameData.h
 * Access to the original game's data files, from one data directory
 * (the game's install directory, e.g. DARKLAND/). Each file is loaded
 * on first use and kept; the getters throw if a file is missing or
 * invalid.
 */
#pragma once

#include "GraphicsDefs.h"

#include <memory>
#include <string>

class Catalog;
class CityFile;
class FontFile;
class LocationFile;
class WorldMap;

class GameData {
public:
    explicit		GameData(const std::string& dataDir);
                    ~GameData();

    const std::string& DataDir() const	{ return fDataDir; }

    // Path of a data file: looked up in the data directory, then in its
    // PICS subdirectory. Names are the game's (upper case) file names.
    // Returns the data directory path if the file is not found anywhere.
    std::string		PathFor(const std::string& fileName) const;

    const WorldMap&		Map();				// DARKLAND.MAP + icon sheets
    const LocationFile&	Locations();		// DARKLAND.LOC
    const CityFile&		Cities();			// DARKLAND.CTY
    const FontFile&		Fonts();			// FONTS.FNT
    const GFX::Palette&	EnemyPalette();		// ENEMYPAL.DAT, all chunks

    // Opens a catalog: `name` is a game file name (e.g. "EINFO.CAT") or
    // a path. The caller owns the result.
    Catalog*		OpenCatalog(const std::string& name) const;

private:
                    GameData(const GameData&);
    GameData&		operator=(const GameData&);

    std::string		fDataDir;
    std::unique_ptr<WorldMap>		fMap;
    std::unique_ptr<LocationFile>	fLocations;
    std::unique_ptr<CityFile>		fCities;
    std::unique_ptr<FontFile>		fFonts;
    std::unique_ptr<GFX::Palette>	fEnemyPalette;
};

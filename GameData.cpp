#include "GameData.h"

#include "Catalog.h"
#include "CityFile.h"
#include "FontFile.h"
#include "LocationFile.h"
#include "Palette.h"
#include "WorldMap.h"

#include <cstring>
#include <sys/stat.h>


static bool
FileExists(const std::string& path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}


GameData::GameData(const std::string& dataDir)
    :
    fDataDir(dataDir)
{
}


GameData::~GameData()
{
}


std::string
GameData::PathFor(const std::string& fileName) const
{
    static const char* kSubdirectories[] = { "", "/PICS" };
    for (const char* subdirectory : kSubdirectories) {
        const std::string path = fDataDir + subdirectory + "/" + fileName;
        if (FileExists(path))
            return path;
    }
    return fDataDir + "/" + fileName;
}


const WorldMap&
GameData::Map()
{
    if (!fMap) {
        const std::string sheets[2] = {
            PathFor("MAPICONS.PIC"), PathFor("MAPICON2.PIC")
        };
        fMap.reset(new WorldMap(PathFor("DARKLAND.MAP"), sheets));
    }
    return *fMap;
}


const LocationFile&
GameData::Locations()
{
    if (!fLocations)
        fLocations.reset(new LocationFile(PathFor("DARKLAND.LOC")));
    return *fLocations;
}


const CityFile&
GameData::Cities()
{
    if (!fCities)
        fCities.reset(new CityFile(PathFor("DARKLAND.CTY")));
    return *fCities;
}


const FontFile&
GameData::Fonts()
{
    if (!fFonts)
        fFonts.reset(new FontFile(PathFor("FONTS.FNT")));
    return *fFonts;
}


const GFX::Palette&
GameData::EnemyPalette()
{
    if (!fEnemyPalette) {
        // black base: the chunk file only patches some ranges.
        // GFX::Palette's constructor leaves colors uninitialized.
        std::unique_ptr<GFX::Palette> palette(new GFX::Palette);
        memset(palette->colors, 0, sizeof(palette->colors));
        PaletteFile(PathFor("ENEMYPAL.DAT")).ApplyAll(*palette);
        fEnemyPalette = std::move(palette);
    }
    return *fEnemyPalette;
}


Catalog*
GameData::OpenCatalog(const std::string& name) const
{
    return new Catalog(FileExists(name) ? name : PathFor(name));
}

#include "GameData.h"

#include "Catalog.h"
#include "CityFile.h"
#include "EnemyFile.h"
#include "DescriptionFile.h"
#include "FileStream.h"
#include "FontFile.h"
#include "ListFile.h"
#include "LocationFile.h"
#include "MsgFile.h"
#include "PICImage.h"
#include "Palette.h"
#include "Stream.h"
#include "WorldMap.h"

#include <cctype>
#include <cstring>
#include <stdexcept>
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


const EnemyFile&
GameData::Enemies()
{
    if (!fEnemies)
        fEnemies.reset(new EnemyFile(PathFor("DARKLAND.ENM")));
    return *fEnemies;
}


const DescriptionFile&
GameData::CityDescriptions()
{
    if (!fCityDescriptions)
        fCityDescriptions.reset(new DescriptionFile(PathFor("DARKLAND.DSC")));
    return *fCityDescriptions;
}


const FontFile&
GameData::Fonts()
{
    if (!fFonts)
        fFonts.reset(new FontFile(PathFor("FONTS.FNT")));
    return *fFonts;
}


const ListFile&
GameData::Lists()
{
    if (!fLists)
        fLists.reset(new ListFile(PathFor("DARKLAND.LST")));
    return *fLists;
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


GFX::Palette
GameData::SpritePalette(const std::string& image, int palette)
{
    GFX::Palette colors = PICImage::EGAPalette();
    // Index 5 is the figures' outline and shadow: probably drawn darker
    // than the ground rather than as a color (inferred), dark here
    colors.colors[5].r = colors.colors[5].g = colors.colors[5].b = 16;
    // 16..31: the gray ramp of the VGA default palette (inferred)
    static const uint8 kGrays[16] = {
        0, 5, 8, 11, 14, 17, 20, 24, 28, 32, 36, 40, 45, 50, 56, 63
    };
    for (int i = 0; i < 16; i++) {
        GFX::Color& color = colors.colors[16 + i];
        color.r = color.g = color.b = uint8((kGrays[i] << 2) | (kGrays[i] >> 4));
    }

    // COMNCLRS.DAT: 72 colors from index 104 (the cards use 128..159 of
    // them, docs/formats.md)
    std::unique_ptr<Stream> stream(new FileStream(PathFor("COMNCLRS.DAT").c_str(),
        FileStream::READ_ONLY));
    uint8 common[72 * 3];
    if (stream->ReadAt(0, common, sizeof(common)) != (ssize_t)sizeof(common))
        throw std::runtime_error("COMNCLRS.DAT: truncated file");
    for (int i = 0; i < 72; i++) {
        GFX::Color& color = colors.colors[104 + i];
        color.r = uint8((common[3 * i] << 2) | (common[3 * i] >> 4));
        color.g = uint8((common[3 * i + 1] << 2) | (common[3 * i + 1] >> 4));
        color.b = uint8((common[3 * i + 2] << 2) | (common[3 * i + 2] >> 4));
    }

    const EnemyFile& enemies = Enemies();
    for (uint32 i = 0; i < enemies.CountTypes(); i++) {
        const enemy_type& type = enemies.TypeAt(i);
        if (type.image != image)
            continue;
        if (palette < 0 || palette >= type.paletteCount)
            palette = 0;
        PaletteFile chunks(PathFor("ENEMYPAL.DAT"));
        for (int c = 0; c < type.paletteChunks; c++) {
            // Baphomet's chunks are past the end of the file
            const uint32 chunk = type.firstPaletteChunk
                + palette * type.paletteChunks + c;
            if (chunk < chunks.CountChunks())
                chunks.ApplyChunk(colors, chunk);
        }
        break;
    }
    return colors;
}


const Catalog&
GameData::MessageCatalog()
{
    if (!fMessageCatalog)
        fMessageCatalog.reset(new Catalog(PathFor("MSGFILES")));
    return *fMessageCatalog;
}


const MsgFile&
GameData::Messages(const std::string& name)
{
    // catalog entry names look like "$PARTY02.MSG"
    std::string entryName;
    for (char c : name)
        entryName += char(std::toupper(uint8(c)));
    if (entryName.compare(0, 1, "$") != 0)
        entryName = "$" + entryName;
    if (entryName.find('.') == std::string::npos)
        entryName += ".MSG";

    std::unique_ptr<MsgFile>& messages = fMessages[entryName];
    if (!messages) {
        std::unique_ptr<Stream> stream(MessageCatalog().GetStream(entryName));
        if (!stream) {
            fMessages.erase(entryName);
            throw std::runtime_error("no " + entryName + " in MSGFILES");
        }
        messages.reset(new MsgFile(stream.get()));
    }
    return *messages;
}


Catalog*
GameData::OpenCatalog(const std::string& name) const
{
    return new Catalog(FileExists(name) ? name : PathFor(name));
}

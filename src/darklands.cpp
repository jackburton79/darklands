#include "BattleMap.h"
#include "Bitmap.h"
#include "CardView.h"
#include "CharacterFile.h"
#include "Catalog.h"
#include "CityFile.h"
#include "CityLabels.h"
#include "CityVisit.h"
#include "EnemyFile.h"
#include "Game.h"
#include "GameData.h"
#include "GraphicsDefs.h"
#include "GraphicsEngine.h"
#include "ImcFile.h"
#include "ImgFile.h"
#include "LocationFile.h"
#include "MsgFile.h"
#include "PICImage.h"
#include "Stream.h"
#include "TextSupport.h"
#include "WorldMap.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include <SDL.h>


static const char* kDefaultDataDir = "data/DARKLAND";

// City of --card, by default
static const char* kDefaultCardCity = "K\xC3\xB6ln";	// Köln

// Font used for the city names: index into FONTS.FNT
static const uint32 kLabelFont	= 2;


static int
DoMapMode(GameData& data, const std::string& outPrefix)
{
    const WorldMap& map = data.Map();
    std::cerr << "map: " << map.Width() << " x " << map.Height()
        << " tiles, " << map.PixelWidth() << " x " << map.PixelHeight()
        << " px" << std::endl;

    Bitmap* bitmap = map.Render();
    try {
        DrawCityLabels(bitmap, GFX::point(0, 0), map, data.Locations(),
            Font(data.Fonts(), kLabelFont));
    } catch (const std::exception& e) {
        std::cerr << "note: no city labels (" << e.what() << ")" << std::endl;
    }
    bitmap->Save((outPrefix + ".bmp").c_str());
    bitmap->Release();
    std::cerr << "wrote " << outPrefix << ".bmp" << std::endl;
    return 0;
}


static void
DumpCities(const CityFile& cities)
{
    static const char* kPlaceNames[CITY_PLACE_COUNT] = {
        "ruler", "second power", "famous place", "square", "town hall",
        "castle", "cathedral", "church", "market", "mint square", "slums",
        "armory", "pawnshop", "monastery", "inn", "university"
    };
    for (uint32 i = 0; i < cities.CountCities(); i++) {
        const city& c = cities.CityAt(i);
        std::cout << std::setw(2) << i << "  " << c.fullName
            << "  size " << int(c.size) << "  x " << c.x << "  y " << c.y;
        if (c.harbor == CITY_HARBOR_NORTH_SEA)
            std::cout << "  port (North Sea)";
        else if (c.harbor == CITY_HARBOR_BALTIC)
            std::cout << "  port (Baltic)";
        std::cout << std::endl << "    neighbors:";
        for (uint16 n : c.neighbors) {
            std::cout << " " << (n < cities.CountCities()
                ? cities.CityAt(n).shortName : std::to_string(n));
        }
        std::cout << std::endl;
        for (int p = 0; p < CITY_PLACE_COUNT; p++) {
            if (!c.places[p].empty())
                std::cout << "    " << kPlaceNames[p] << ": " << c.places[p] << std::endl;
        }
    }
}


static void
DumpEnemies(const EnemyFile& enemies)
{
    for (uint32 i = 0; i < enemies.CountTypes(); i++) {
        const enemy_type& t = enemies.TypeAt(i);
        std::cout << std::setw(2) << i << "  " << t.image << "  "
            << std::left << std::setw(10) << t.name << std::right;
        if (t.variants != 0)
            std::cout << "  variants " << int(t.variants);
        std::cout << std::endl << "    attributes";
        for (int a = 0; a < ATTRIBUTE_COUNT; a++)
            std::cout << " " << int(t.attributes[a]);
        std::cout << std::endl << "    skills";
        for (int s = 0; s < kSkillCount; s++)
            std::cout << " " << int(t.skills[s]);
        std::cout << std::endl;
    }
    for (uint32 i = 0; i < enemies.CountEnemies(); i++) {
        const enemy& e = enemies.EnemyAt(i);
        std::cout << std::setw(2) << i << "  " << std::left << std::setw(12)
            << e.name << std::right << "  type " << std::setw(2) << e.type
            << " (" << enemies.TypeAt(e.type).name << ")  flags 0x"
            << std::hex << e.flags << std::dec << std::endl;
    }
}


// A battlefield map as text: three characters per cell, the walls
// between them as their types (a wall is on the side of one cell or
// both); "##" a closed cell, a hex digit an object. Then the cells' bytes.
static void
DumpBattleMap(const BattleMap& map)
{
    static const char* kDigits = "0123456789ABCDEF";
    for (int y = 0; y <= BattleMap::kSize; y++) {
        std::string walls;
        for (int x = 0; x < BattleMap::kSize; x++) {
            int wall = 0;
            if (y > 0)
                wall = map.CellAt(x, y - 1).Wall(SIDE_NEXT_ROW);
            if (y < BattleMap::kSize && wall == 0)
                wall = map.CellAt(x, y).Wall(SIDE_PREVIOUS_ROW);
            walls += '+';
            walls += std::string(2, wall != 0 ? kDigits[wall] : ' ');
        }
        std::cout << walls << '+' << std::endl;
        if (y == BattleMap::kSize)
            break;

        std::string cells;
        for (int x = 0; x <= BattleMap::kSize; x++) {
            int wall = 0;
            if (x > 0)
                wall = map.CellAt(x - 1, y).Wall(SIDE_NEXT_COLUMN);
            if (x < BattleMap::kSize && wall == 0)
                wall = map.CellAt(x, y).Wall(SIDE_PREVIOUS_COLUMN);
            cells += wall != 0 ? kDigits[wall] : ' ';
            if (x == BattleMap::kSize)
                break;
            const battle_cell& cell = map.CellAt(x, y);
            if (cell.Closed())
                cells += "##";
            else if (cell.Object() != 0)
                cells += std::string(1, kDigits[cell.Object()]) + " ";
            else
                cells += "  ";
        }
        std::cout << cells << std::endl;
    }
    std::cout << std::endl << "cells (x y: bytes 0..3, ground):" << std::endl;
    for (int y = 0; y < BattleMap::kSize; y++) {
        for (int x = 0; x < BattleMap::kSize; x++) {
            const battle_cell& cell = map.CellAt(x, y);
            char line[64];
            snprintf(line, sizeof(line), "%2d %2d: %02x %02x %02x %02x  %02x",
                x, y, cell.bytes[0], cell.bytes[1], cell.bytes[2],
                cell.bytes[3], cell.ground);
            std::cout << line << std::endl;
        }
    }
}


static void
DumpLocations(const LocationFile& locations)
{
    for (uint32 i = 0; i < locations.CountLocations(); i++) {
        const location& loc = locations.LocationAt(i);
        std::cout << std::setw(3) << i << "  type " << std::setw(2) << loc.type
            << "  x " << std::setw(3) << loc.x << "  y " << std::setw(3) << loc.y
            << "  size " << int(loc.size) << "  " << loc.name << std::endl;
    }
}


// Card text as UTF-8, with the control codes written as tags.
static std::string
DescribeCardText(const std::string& text)
{
    std::string result;
    for (char c : text) {
        switch (uint8(c)) {
            case MSG_CODE_NEWLINE:			result += "\n"; break;
            case MSG_CODE_PARAGRAPH:		result += "<p>"; break;
            case MSG_CODE_OPTION:			result += "<option>"; break;
            case MSG_CODE_SAINT_OPTION:		result += "<saint>"; break;
            case MSG_CODE_POTION_OPTION:	result += "<potion>"; break;
            case MSG_CODE_BATTLE_OPTION:	result += "<battle>"; break;
            case MSG_CODE_OPTION_TEXT:		result += "<text>"; break;
            default:
                if (uint8(c) < 0x20 && c != 0x1F) {	// 0x1F is 'ä'
                    char tag[8];
                    snprintf(tag, sizeof(tag), "<%02X>", uint8(c));
                    result += tag;
                } else
                    result += LocationFile::DecodeName(&c, 1);
                break;
        }
    }
    return result;
}


static void
DumpMessages(const MsgFile& messages)
{
    for (uint32 i = 0; i < messages.CountCards(); i++) {
        const msg_card& card = messages.CardAt(i);
        std::cout << "card " << i << "  top " << int(card.textTop)
            << "  left " << int(card.textLeft)
            << "  right " << int(card.textRight);
        if (card.unknown1 != 0 || card.unknown2 != 0) {
            std::cout << "  unknown " << int(card.unknown1) << " "
                << int(card.unknown2);
        }
        std::cout << std::endl << DescribeCardText(card.text) << std::endl
            << std::endl;
    }
}


static void
ListMessages(const Catalog& catalog)
{
    for (int32 i = 0; i < catalog.CountEntries(); i++) {
        std::unique_ptr<Stream> stream(catalog.GetStreamAt(uint32(i)));
        std::cout << catalog.EntryAt(i).filename << "  "
            << MsgFile(stream.get()).CountCards() << " cards" << std::endl;
    }
}


// City by index or (short) name; -1 if there is none.
static int
FindCity(const CityFile& cities, const std::string& name)
{
    char* end = NULL;
    const long index = strtol(name.c_str(), &end, 10);
    if (!name.empty() && *end == '\0')
        return index >= 0 && index < long(cities.CountCities()) ? int(index) : -1;
    for (uint32 i = 0; i < cities.CountCities(); i++) {
        if (cities.CityAt(i).shortName == name)
            return int(i);
    }
    return -1;
}


// Shows a card; prints the chosen option.
static int
ShowCard(GameData& data, const std::string& deck, uint32 cardIndex,
    const std::string& cityName, const std::string& scene)
{
    const MsgFile& messages = data.Messages(deck);
    const CityFile& cities = data.Cities();
    const int cityIndex = FindCity(cities, cityName);
    if (cityIndex < 0)
        throw std::runtime_error("no city " + cityName);

    card_variables variables;
    const CharacterFile characters(data.PathFor("CHARACTR.TMP"));
    CityVisit::AddCityVariables(data, cityIndex, variables);
    CityVisit::AddPartyVariables(characters.Party(), variables);

    CardView view(data);
    view.SetCard(messages.CardAt(cardIndex), variables);
    view.SetScene(scene);
    view.SetParty(&characters.Party());
    const int option = view.Run();
    if (option >= 0)
        std::cout << "chosen option: " << option << std::endl;
    return 0;
}


static void
DrawSprite(Bitmap* bitmap, const sprite& picture, int left, int top)
{
    for (int y = 0; y < picture.height; y++) {
        for (int x = 0; x < picture.width; x++) {
            const uint8 pixel = picture.pixels[y * picture.width + x];
            if (pixel != 0)
                bitmap->PutPixel(uint16(left + x), uint16(top + y), pixel);
        }
    }
}


// index 0 is transparent: show it as a green ground
static Bitmap*
SpriteBitmap(int width, int height, GFX::Palette palette)
{
    palette.colors[0] = GFX::Color{ 40, 90, 40, 0 };
    Bitmap* bitmap = new Bitmap(uint16(width), uint16(height), 8);
    bitmap->SetColors(palette.colors, 0, 256);
    bitmap->Clear(0);
    return bitmap;
}


// A battle sprite as one sheet: a row per frame, a column per direction
static Bitmap*
SpriteSheet(const ImcFile& sprites, const GFX::Palette& palette)
{
    int cellWidth = 0;
    int cellHeight = 0;
    for (int f = 0; f < sprites.CountFrames(); f++) {
        for (int d = 0; d < ImcFile::kDirectionCount; d++) {
            const sprite& picture = sprites.SpriteAt(f, d);
            cellWidth = std::max(cellWidth, picture.width + 2);
            cellHeight = std::max(cellHeight, picture.height + 2);
        }
    }
    Bitmap* bitmap = SpriteBitmap(cellWidth * ImcFile::kDirectionCount,
        cellHeight * sprites.CountFrames(), palette);
    for (int f = 0; f < sprites.CountFrames(); f++) {
        for (int d = 0; d < ImcFile::kDirectionCount; d++) {
            DrawSprite(bitmap, sprites.SpriteAt(f, d), d * cellWidth + 1,
                f * cellHeight + 1);
        }
    }
    return bitmap;
}


// The pictures of an .IMG file (BATTLEGR.IMG), one BMP each
static void
ExtractPictures(GameData& data, const std::string& fileName,
    const std::string& outputDir)
{
    const ImgFile pictures(data.PathFor(fileName));
    const GFX::Palette palette = data.SpritePalette("");
    const std::string base = fileName.substr(0, fileName.rfind('.'));
    for (uint32 i = 0; i < pictures.CountPictures(); i++) {
        const sprite& picture = pictures.PictureAt(i);
        Bitmap* bitmap = SpriteBitmap(std::max(1, int(picture.width)),
            std::max(1, int(picture.height)), palette);
        DrawSprite(bitmap, picture, 0, 0);
        char number[16];
        snprintf(number, sizeof(number), ".%03u", unsigned(i));
        const std::string output = outputDir + "/" + base + number + ".bmp";
        bitmap->Save(output.c_str());
        bitmap->Release();
        std::cout << fileName << " " << i << " -> " << output << std::endl;
    }
}


static void
ExtractAll(GameData& data, const Catalog& catalog,
    const std::string& outputDir)
{
    for (int32 i = 0; i < catalog.CountEntries(); i++) {
        Stream* stream = NULL;
        try {
            const catalog_entry& entry = catalog.EntryAt(i);
            stream = catalog.GetStreamAt(uint32(i));
            Bitmap* bitmap;
            const std::string& name = entry.filename;
            if (name.size() > 4 && name.compare(name.size() - 4, 4, ".IMC") == 0) {
                // "E00CBA2.IMC": the sprite set is "E00"
                bitmap = SpriteSheet(ImcFile(stream),
                    data.SpritePalette(name.substr(0, 3)));
            } else {
                PICImage image(stream);
                bitmap = image.Image(&data.EnemyPalette());
            }
            const std::string output = outputDir + "/" + name + ".bmp";
            bitmap->Save(output.c_str());
            bitmap->Release();
            std::cout << name << " -> " << output << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "entry " << i << ": " << e.what() << std::endl;
        }
        delete stream;
    }
}


static void
Usage()
{
    std::cerr << "usage: darklands [--data <dir>] [<command>]\n"
        "  (no command)                  play, from a random city\n"
        "  --start <city>                play, from a city (name or index)\n"
        "  --load <save>                 play, from a saved game (e.g. DKSAVE0.SAV)\n"
        "  <catalog>                     dump a catalog's entries\n"
        "  --extract <catalog> <outdir>  export a catalog's images as BMP (the\n"
        "                                battle sprites as sheets, e.g. E00C.CAT;\n"
        "                                also BATTLEGR.IMG, COMMONSP.IMG)\n"
        "  --map [prefix]                render the world map to <prefix>.bmp\n"
        "  --locations                   list DARKLAND.LOC\n"
        "  --cities                      list DARKLAND.CTY\n"
        "  --enemies                     list DARKLAND.ENM\n"
        "  --battlemap <name>            dump a battlefield map of IMAPS.CAT\n"
        "                                (e.g. ICITY.000)\n"
        "  --messages [name]             list MSGFILES, or dump a card deck\n"
        "                                (e.g. PARTY02, or a path to a .MSG file)\n"
        "  --card <name> [card] [city] [picture]\n"
        "                                show a card (default: card 0, in "
        << kDefaultCardCity << "),\n"
        "                                on a scene picture (e.g. MAIN-ST.PIC)\n"
        "<dir> is the game's data directory (default: " << kDefaultDataDir
        << ")" << std::endl;
}


int main(int argc, char **argv)
{
    std::string dataDir = kDefaultDataDir;
    int arg = 1;
    if (arg + 1 < argc && std::string(argv[arg]) == "--data") {
        dataDir = argv[arg + 1];
        arg += 2;
    }
    GameData data(dataDir);
    if (arg >= argc) {
        try {
            Game game(data);
            game.NewGame();
            game.Run();
        } catch (const std::exception& e) {
            std::cerr << "game: " << e.what() << std::endl;
            Usage();
            return 1;
        }
        return 0;
    }
    const std::string command = argv[arg];
    const int extra = argc - arg - 1;	// arguments after the command

    try {
        if (command == "--map")
            return DoMapMode(data, extra > 0 ? argv[arg + 1] : "map");
        if (command == "--cities") {
            DumpCities(data.Cities());
            return 0;
        }
        if (command == "--enemies") {
            DumpEnemies(data.Enemies());
            return 0;
        }
        if (command == "--battlemap") {
            if (extra < 1) {
                Usage();
                return 1;
            }
            std::unique_ptr<Catalog> maps(data.OpenCatalog("IMAPS.CAT"));
            std::unique_ptr<Stream> stream(maps->GetStream(argv[arg + 1]));
            if (!stream)
                throw std::runtime_error(std::string("no map ") + argv[arg + 1]);
            DumpBattleMap(BattleMap(stream.get()));
            return 0;
        }
        if (command == "--locations") {
            DumpLocations(data.Locations());
            return 0;
        }
        if (command == "--messages") {
            if (extra < 1)
                ListMessages(data.MessageCatalog());
            else if (std::string(argv[arg + 1]).find('/') != std::string::npos)
                DumpMessages(MsgFile(argv[arg + 1]));	// a path
            else
                DumpMessages(data.Messages(argv[arg + 1]));
            return 0;
        }
        if (command == "--start") {
            if (extra < 1) {
                Usage();
                return 1;
            }
            const int city = FindCity(data.Cities(), argv[arg + 1]);
            if (city < 0)
                throw std::runtime_error(std::string("no city ") + argv[arg + 1]);
            Game game(data);
            game.NewGame(city);
            game.Run();
            return 0;
        }
        if (command == "--load") {
            if (extra < 1) {
                Usage();
                return 1;
            }
            Game game(data);
            game.LoadGame(argv[arg + 1]);
            game.Run();
            return 0;
        }
        if (command == "--card") {
            if (extra < 1) {
                Usage();
                return 1;
            }
            return ShowCard(data, argv[arg + 1],
                extra > 1 ? uint32(atoi(argv[arg + 2])) : 0,
                extra > 2 ? argv[arg + 3] : kDefaultCardCity,
                extra > 3 ? argv[arg + 4] : "");
        }
        if (command == "--extract") {
            if (extra < 2) {
                Usage();
                return 1;
            }
            const std::string name = argv[arg + 1];
            if (name.size() > 4 && name.compare(name.size() - 4, 4, ".IMG") == 0) {
                ExtractPictures(data, name, argv[arg + 2]);
                return 0;
            }
            std::unique_ptr<Catalog> catalog(data.OpenCatalog(name));
            // output dir must exist
            ExtractAll(data, *catalog, argv[arg + 2]);
            return 0;
        }
    } catch (const std::exception& e) {
        std::cerr << command << ": " << e.what() << std::endl;
        return 1;
    }
    if (command.compare(0, 2, "--") == 0) {
        Usage();
        return 1;
    }

    const std::string catalogName = command;
    std::unique_ptr<Catalog> catalogPtr;
    GFX::Palette palette;
    try {
        catalogPtr.reset(data.OpenCatalog(catalogName));
        palette = data.EnemyPalette();
    } catch (const std::exception& e) {
        std::cerr << catalogName << ": " << e.what() << std::endl;
        return 1;
    }
    const Catalog& catalog = *catalogPtr;

    std::cout << "Requested catalog " << catalogName << std::endl;

    catalog.Dump(std::cout);

    return 0;
}

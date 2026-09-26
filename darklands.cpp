#include "Bitmap.h"
#include "Catalog.h"
#include "CityFile.h"
#include "CityLabels.h"
#include "GameData.h"
#include "GraphicsDefs.h"
#include "GraphicsEngine.h"
#include "LocationFile.h"
#include "MapViewer.h"
#include "MsgFile.h"
#include "PICImage.h"
#include "Stream.h"
#include "TextSupport.h"
#include "WorldMap.h"

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include <SDL.h>


static const char* kDefaultDataDir = "data/DARKLAND";

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


static void
ExtractAll(const Catalog& catalog, const std::string& outputDir,
    const GFX::Palette& palette)
{
    for (int32 i = 0; i < catalog.CountEntries(); i++) {
        Stream* stream = NULL;
        try {
            const catalog_entry& entry = catalog.EntryAt(i);
            stream = catalog.GetStreamAt(uint32(i));
            PICImage image(stream);
            Bitmap* bitmap = image.Image(&palette);
            const std::string name = outputDir + "/" + entry.filename + ".bmp";
            bitmap->Save(name.c_str());
            bitmap->Release();
            std::cout << entry.filename << " -> " << name << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "entry " << i << ": " << e.what() << std::endl;
        }
        delete stream;
    }
}


static Bitmap*
DecodeImage(const Catalog* catalog, uint32 index, const GFX::Palette& palette)
{
    Stream* stream = NULL;
    Bitmap* image = NULL;
    try {
        stream = catalog->GetStreamAt(index);
        image = PICImage::Decode(stream, &palette);
    } catch (const std::exception& e) {
        std::cerr << "Cannot decode entry " << index << ": " << e.what() << std::endl;
    }
    delete stream;	// sub-stream: delete before the Catalog dies
    return image;
}


static void
Usage()
{
    std::cerr << "usage: darklands [--data <dir>] [<command>]\n"
        "  (no command)                  explore the world map\n"
        "  <catalog>                     browse a catalog's images\n"
        "  --extract <catalog> <outdir>  export a catalog's images as BMP\n"
        "  --map [prefix]                render the world map to <prefix>.bmp\n"
        "  --locations                   list DARKLAND.LOC\n"
        "  --cities                      list DARKLAND.CTY\n"
        "  --messages [name]             list MSGFILES, or dump a card deck\n"
        "                                (e.g. PARTY02, or a path to a .MSG file)\n"
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
            MapViewer(data).Run();
        } catch (const std::exception& e) {
            std::cerr << "map viewer: " << e.what() << std::endl;
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
        if (command == "--extract") {
            if (extra < 2) {
                Usage();
                return 1;
            }
            std::unique_ptr<Catalog> catalog(data.OpenCatalog(argv[arg + 1]));
            // output dir must exist
            ExtractAll(*catalog, argv[arg + 2], data.EnemyPalette());
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

    if (!GraphicsEngine::Initialize()) {
        std::cerr << "Cannot initialize graphics engine!" << std::endl;
        return 1;
    }

    GraphicsEngine::Get()->SetVideoMode(320, 200, 16,
            GraphicsEngine::VIDEOMODE_WINDOWED);

    int32 i = 0;
    bool quitting = false;
    SDL_Event event;
    Bitmap* bitmap = DecodeImage(&catalog, i, palette);
    while (!quitting) {
        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
                case SDL_KEYDOWN: {
                    switch (event.key.keysym.sym) {
                        case SDLK_RIGHT:
                            if (i + 1 < catalog.CountEntries()) {
                                i++;
                                if (bitmap != NULL)
                                    bitmap->Release();
                                bitmap = DecodeImage(&catalog, i, palette);
                            }
                            break;
                        case SDLK_LEFT:
                            if (i > 0) {
                                i--;
                                if (bitmap != NULL)
                                    bitmap->Release();
                                bitmap = DecodeImage(&catalog, i, palette);
                            }
                            break;
                        default:
                            break;
                    }
                }
                break;
                case SDL_QUIT:
                    quitting = true;
                    break;
                default:
                    break;
            }
        }

        if (bitmap != NULL) {
            GFX::rect screenFrame = GraphicsEngine::Get()->ScreenFrame();
            GFX::rect bitmapFrame = bitmap->Frame();
            GraphicsEngine::Get()->BlitToScreen(bitmap, &bitmapFrame, &screenFrame);
        }

        GraphicsEngine::Get()->Update();
        SDL_Delay(100);
    }
    if (bitmap != NULL)
        bitmap->Release();
    return 0;
}

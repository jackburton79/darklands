#include "Bitmap.h"
#include "Catalog.h"
#include "GraphicsDefs.h"
#include "GraphicsEngine.h"
#include "FileStream.h"
#include "Palette.h"
#include "PICImage.h"
#include "Stream.h"

#include <cstring>
#include <iostream>
#include <string>

#include <SDL.h>

#include "MapFile.h"


static const char* kSheetNames[2] = { "MAPICONS.PIC", "MAPICON2.PIC" };

// Map tile geometry (see docs/formats.md): the icon sheets are grids of
// 16 x 12 pixel cells; map rows are staggered by half a tile horizontally
// and are 4 pixels apart, so tiles overlap. Index 0 is transparent.
static const uint16 kTileW		= 16;
static const uint16 kTileH		= 12;
static const uint16 kRowStep	= 4;

// Tile types: sheet 0 rows are types 0..15, sheet 1 rows types 16..31.
enum {
    kTileOcean		= 1,
    kTileMajorRiver	= 2,
    kTileMinorRiver	= 3,
    kTileRoad		= 24,
    kTileFord		= 25,
    kTileBridge		= 27,
    kTileCity		= 29
};


static int
TileType(const map_tile& tile)
{
    return (tile.secondPalette ? 16 : 0) + tile.row;
}


// Whether a tile of type `type` joins its diagonal neighbor of type
// `neighbor`: tiles of the same type join; roads also join fords,
// bridges and cities, rivers also join fords, bridges and the sea.
static bool
Connects(int type, int neighbor)
{
    if (neighbor == type)
        return true;
    switch (type) {
        case kTileRoad:
            return neighbor == kTileFord || neighbor == kTileBridge
                || neighbor == kTileCity;
        case kTileMajorRiver:
        case kTileMinorRiver:
            return neighbor == kTileMajorRiver || neighbor == kTileMinorRiver
                || neighbor == kTileFord || neighbor == kTileBridge
                || neighbor == kTileOcean;
        default:
            return false;
    }
}


// Sheet column of tile (x, y): a bit mask of the diagonal neighbors it
// joins -- NW = 1, NE = 2, SW = 4, SE = 8 (see docs/formats.md).
// Odd rows are shifted right by half a tile, so the diagonal neighbors'
// x depends on the row parity. Off-map neighbors count as joined.
static int
ColumnFor(const std::vector<std::vector<map_tile> >& rows, int x, int y)
{
    const int mw = (int)rows[0].size();
    const int mh = (int)rows.size();
    const int type = TileType(rows[y][x]);

    const int west = (y & 1) ? x : x - 1;
    const int neighbors[4][2] = {
        { west, y - 1 }, { west + 1, y - 1 },	// NW, NE
        { west, y + 1 }, { west + 1, y + 1 }	// SW, SE
    };

    int col = 0;
    for (int i = 0; i < 4; i++) {
        const int nx = neighbors[i][0];
        const int ny = neighbors[i][1];
        if (nx < 0 || nx >= mw || ny < 0 || ny >= mh
                || Connects(type, TileType(rows[ny][nx]))) {
            col |= 1 << i;
        }
    }
    return col;
}


static Bitmap*
RenderMap(const MapFile& map,
    const std::vector<std::vector<map_tile> >& rows,
    const std::vector<uint8> sheets[2], uint16 sheetW,
    const GFX::Palette& palette, bool synthetic)
{
    const uint16 mw = map.Width();
    const uint16 mh = map.Height();
    const uint16 tileW = kTileW;
    const uint16 tileH = kTileH;
    const uint16 halfW = tileW / 2;
    const uint32 outW = uint32(mw) * tileW + halfW;
    const uint32 outH = uint32(mh - 1) * kRowStep + tileH;

    std::cerr << "rendering " << outW << " x " << outH << " px..." << std::endl;

    Bitmap* out = new Bitmap(outW, outH, 8);
    try {
        out->SetColors(palette.colors, 0, 256);
        for (uint16 y = 0; y < mh; y++) {
            const int xoff = (y & 1) ? halfW : 0;
            const int yoff = y * kRowStep;
            for (uint16 x = 0; x < mw; x++) {
                const map_tile& t = rows[y][x];
                const int col = ColumnFor(rows, x, y);

                if (synthetic) {
                    // no sheets available: color by palette set + tile row,
                    // so geometry and column artifacts are still visible
                    const uint8 value = t.secondPalette
                        ? uint8(128 + t.row * 8)
                        : uint8(16 + t.row * 8);
                    for (uint16 py = 0; py < tileH; py++)
                        for (uint16 px = 0; px < tileW; px++)
                            out->PutPixel(xoff + x * tileW + px,
                                yoff + py, value);
                    continue;
                }

                const std::vector<uint8>& sheet = sheets[t.secondPalette ? 1 : 0];
                const int sx = col * tileW;
                const int sy = t.row * tileH;
                for (uint16 py = 0; py < tileH; py++) {
                    const uint8* src = &sheet[(size_t)(sy + py) * sheetW + sx];
                    for (uint16 px = 0; px < tileW; px++) {
                        // later (lower) rows overlap earlier ones
                        if (src[px] != 0)
                            out->PutPixel(xoff + x * tileW + px, yoff + py,
                                src[px]);
                    }
                }
            }
        }
    } catch (...) {
        out->Release();
        throw;
    }
    return out;
}


static int
DoMapMode(const std::string& mapPath, const std::string& dataDir,
    const std::string& outPrefix)
{
    // --- icon sheets: PIC files with an embedded "M0" palette chunk,
    //     which also provides the map palette ---
    GFX::Palette palette = {};
    std::vector<uint8> sheets[2];
    uint16 sheetW = 0, sheetH = 0;
    bool haveSheets = true;
    for (int i = 0; i < 2 && haveSheets; i++) {
        const std::string path = dataDir + "/PICS/" + kSheetNames[i];
        Stream* stream = NULL;
        try {
            stream = new FileStream(path.c_str(), FileStream::READ_ONLY);
            PICImage image(stream);
            if (sheetW == 0) {
                sheetW = image.Width();
                sheetH = image.Height();
            } else if (image.Width() != sheetW || image.Height() != sheetH) {
                throw std::runtime_error("sheet size mismatch");
            }
            if (sheetW < 16 * kTileW || sheetH < 16 * kTileH)
                throw std::runtime_error("sheet too small");
            if (i == 0)
                image.ApplyPalette(palette);
            sheets[i] = image.RawBytes();
            std::cerr << kSheetNames[i] << ": " << image.Width() << "x"
                << image.Height() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "note: cannot load " << kSheetNames[i] << " ("
                << e.what() << ") -- rendering with synthetic colors"
                << std::endl;
            haveSheets = false;
        }
        delete stream;
    }
    if (!haveSheets) {
        // grayscale ramp, so the synthetic render is visible
        for (int i = 0; i < 256; i++)
            palette.colors[i] = GFX::Color{ uint8(i), uint8(i), uint8(i), 0 };
    }

    // --- decode all rows, render ---
    MapFile map(mapPath);
    std::cerr << "map: " << map.Width() << " x " << map.Height() << std::endl;

    std::vector<std::vector<map_tile> > rows(map.Height());
    for (uint16 y = 0; y < map.Height(); y++)
        rows[y] = map.Row(y);

    const bool synthetic = !haveSheets;

    Bitmap* bitmap = RenderMap(map, rows, sheets, sheetW, palette, synthetic);
    bitmap->Save((outPrefix + ".bmp").c_str());
    bitmap->Release();
    std::cerr << "wrote " << outPrefix << ".bmp" << std::endl;

    return 0;
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


int main(int argc, char **argv)
{
    GFX::Palette palette = {};
    //memset(&palette, 0, sizeof(palette));	// black base; chunk files only
                                            // patch some ranges
    PaletteFile paletteFile("data/DARKLAND/ENEMYPAL.DAT");
    paletteFile.ApplyAll(palette);

    if (argc > 2 && std::string(argv[1]) == "--map") {
        try {
            return DoMapMode(argv[2],
                argc > 3 ? argv[3] : "data/DARKLAND",
                argc > 4 ? argv[4] : "map");
        } catch (const std::exception& e) {
            std::cerr << "map error: " << e.what() << std::endl;
            return 1;
        }
    }
    if (argc > 3 && std::string(argv[1]) == "--extract") {
        Catalog catalog(argv[2]);
        ExtractAll(catalog, argv[3], palette);	// output dir must exist
        return 0;
    }

    if (argc < 2) {
        std::cerr << "usage: darklands <catalog file>" << std::endl;
        return 1;
    }
    const std::string catalogName = argv[1];

    std::cout << "Requested catalog " << catalogName << std::endl;
    Catalog catalog(catalogName);

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

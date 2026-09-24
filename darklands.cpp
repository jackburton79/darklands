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

// The documented wendigo recipe (known imperfect — that's what we're testing).
// Column bits come from the tile-ROW values of the four diagonal neighbors:
//   col bit 0 (1) = NW neighbor row bit 3 (8)
//   col bit 1 (2) = NE neighbor row bit 2 (4)
//   col bit 2 (4) = SW neighbor row bit 1 (2)
//   col bit 3 (8) = SE neighbor row bit 0 (1)
// Even rows: NW=(x-1,y-1) NE=(x,y-1); odd rows shifted: NW=(x,y-1) NE=(x+1,y-1).
static int
ColumnFor(const std::vector<std::vector<map_tile> >& rows, int x, int y)
{
    const int mw = (int)rows[0].size();
    const int mh = (int)rows.size();

    auto rowAt = [&](int ny) -> const std::vector<map_tile>& {
        static const std::vector<map_tile> kEmpty;
        return (ny < 0 || ny >= mh) ? kEmpty : rows[(size_t)ny];
    };
    auto tileAt = [&](int nx, int ny) -> uint8 {
        if (nx < 0 || nx >= mw)
            return 0;
        const std::vector<map_tile>& r = rowAt(ny);
        return r.empty() ? 0 : r[(size_t)nx].row;
    };

    int nwx, nex, swx, sex;
    if ((y & 1) == 0) { nwx = x - 1; nex = x;     swx = x - 1; sex = x;     }
    else              { nwx = x;     nex = x + 1; swx = x;     sex = x + 1; }

    const uint8 nw = tileAt(nwx, y - 1);
    const uint8 ne = tileAt(nex, y - 1);
    const uint8 sw = tileAt(swx, y + 1);
    const uint8 se = tileAt(sex, y + 1);

    int col = 0;
    col |= ((nw >> 3) & 1) << 0;
    col |= ((ne >> 2) & 1) << 1;
    col |= ((sw >> 1) & 1) << 2;
    col |= ((se     ) & 1) << 3;
    return col;
}


static Bitmap*
RenderMap(const MapFile& map,
    const std::vector<std::vector<map_tile> >& rows,
    const std::vector<uint8> sheets[2], uint16 sheetW,
    const GFX::Palette& palette, bool useRecipe, bool synthetic)
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
                const int col = useRecipe ? ColumnFor(rows, x, y) : 0;

                if (synthetic) {
                    // no sheets available: color by palette set + tile row,
                    // so geometry and recipe artifacts are still visible
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

    // --- decode all rows once, render twice ---
    MapFile map(mapPath);
    std::cerr << "map: " << map.Width() << " x " << map.Height() << std::endl;

    std::vector<std::vector<map_tile> > rows(map.Height());
    for (uint16 y = 0; y < map.Height(); y++)
        rows[y] = map.Row(y);

    const bool synthetic = !haveSheets;

    Bitmap* plain = RenderMap(map, rows, sheets, sheetW, palette, /*useRecipe*/ false, synthetic);
    plain->Save((outPrefix + "_plain.bmp").c_str());
    plain->Release();
    std::cerr << "wrote " << outPrefix << "_plain.bmp" << std::endl;

    Bitmap* recipe = RenderMap(map, rows, sheets, sheetW, palette, /*useRecipe*/ true, synthetic);
    recipe->Save((outPrefix + "_recipe.bmp").c_str());
    recipe->Release();
    std::cerr << "wrote " << outPrefix << "_recipe.bmp" << std::endl;

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

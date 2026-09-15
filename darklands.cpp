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
    GFX::Palette palette;
    memset(&palette, 0, sizeof(palette));	// black base; chunk files only
                                            // patch some ranges
    PaletteFile paletteFile("data/DARKLAND/ENEMYPAL.DAT");
    paletteFile.ApplyAll(palette);

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

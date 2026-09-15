#include "Bitmap.h"
#include "Catalog.h"
#include "GraphicsEngine.h"
#include "FileStream.h"
#include "Stream.h"

#include <iostream>

#include <SDL.h>
#include "PICImage.h"


/*static void
SetBitmap(Bitmap*& oldBitmap, Bitmap* newBitmap)
{
    if (oldBitmap != NULL)
        oldBitmap->Release();
    oldBitmap = newBitmap;
}*/


static Bitmap*
DecodeImage(const Catalog* catalog, uint32 index)
{
    Stream* stream = NULL;
    Bitmap* image = NULL;
    try {
        stream = catalog->GetStreamAt(index);
        image = PICImage::Decode(stream);
    } catch (const std::exception& e) {
        std::cerr << "Cannot decode entry " << index << ": " << e.what() << std::endl;
    }
    delete stream;	// sub-stream: delete before the Catalog dies
    return image;
}

int main(int argc, char **argv)
{
	std::string catalogName;
	std::string fileName;
	std::cout << "argc: " << argc << std::endl;
	if (argc > 1) {
		catalogName = argv[1];
	}

	Catalog* catalog = NULL;
	Stream* stream = NULL;
	if (!catalogName.empty()) {
		std::cout << "Requested catalog " << catalogName << std::endl;
		catalog = new Catalog(catalogName);
		//catalog->ListEntries();
	} else {
		std::cout << "TODO: Usage: " << std::endl;
		return 1;
	}

	for (int32 i = 0; i < catalog->CountEntries(); i++) {
    	Stream* s = catalog->GetStreamAt(i);
    	std::cerr << s->Size() << "\t" << s->ReadWordLEAt(0x02) << std::endl;
    	delete s;
	}
	if (!GraphicsEngine::Initialize()) {
		std::cerr << "Cannot initialize graphics engine!" << std::endl;
		exit(-1);
	}

	GraphicsEngine::Get()->SetVideoMode(320, 200, 16,
			GraphicsEngine::VIDEOMODE_WINDOWED);

	int32 i = 0;
	bool quitting = false;
	SDL_Event event;
	Bitmap* bitmap = DecodeImage(catalog, i);
	while (!quitting) {
		while (SDL_PollEvent(&event) != 0) {
			switch (event.type) {
				case SDL_KEYDOWN: {
					switch (event.key.keysym.sym) {
						case SDLK_RIGHT:
							if (i + 1 < catalog->CountEntries()) {
								i++;
								if (bitmap != NULL)
									bitmap->Release();
								bitmap = DecodeImage(catalog, i);
							}
							break;
						case SDLK_LEFT:
							if (i > 0) {
								i--;
								if (bitmap != NULL)
									bitmap->Release();
								bitmap = DecodeImage(catalog, i);
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
	delete stream;
	delete catalog;
	return 0;
}

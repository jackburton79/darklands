#include "Bitmap.h"
#include "Catalog.h"
#include "GraphicsEngine.h"
#include "FileStream.h"
#include "PicDecoder.h"
#include "Stream.h"

#include <iostream>

#include <SDL.h>


static
Bitmap*
DecodeImage(Catalog* catalog, uint32 index)
{
	try {
		Stream* stream = catalog->GetStreamAt(index);
		PicDecoder decoder;
		return decoder.GetImage(stream);
	} catch (...) {
		return NULL;
	}
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
		catalog = new Catalog(catalogName);
		catalog->ListEntries();
	} else
		exit(-1);

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
							i++;
							bitmap = DecodeImage(catalog, i);
							break;
						case SDLK_LEFT:
							if (i > 0)
								i--;
							bitmap = DecodeImage(catalog, i);
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

			if (bitmap != NULL) {
				GraphicsEngine::Get()->BlitToScreen(bitmap, NULL, NULL);
			}
		}
		GraphicsEngine::Get()->Update();
		SDL_Delay(100);
	}
	delete stream;
	delete catalog;
	return 0;
}

#include "Bitmap.h"
#include "Catalog.h"
#include "FileStream.h"
#include "PicDecoder.h"
#include "Stream.h"

#include <iostream>


int main(int argc, char **argv)
{
	std::string catalogName;
	if (argc > 1)
		catalogName = argv[1];
	std::string fileName;
	if (argc > 2)
		fileName = argv[2];

	if (catalogName.empty())
		return 0;

	Catalog catalog(catalogName);
	catalog.ListEntries();
	Stream* stream = catalog.GetStream(fileName);

	if (stream != nullptr) {
		//stream->DumpToFile(fileName.c_str());
		PicDecoder decoder;
		Bitmap* image = decoder.GetImage(stream);
		image->Dump();
		image->Save(fileName.c_str());
	}
	delete stream;

	return 0;
}

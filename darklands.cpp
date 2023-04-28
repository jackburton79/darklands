#include "Catalog.h"
#include "FileStream.h"
#include "PicDecoder.h"
#include "Stream.h"

#include <iostream>


int main(int argc, char **argv)
{
	std::string catalogName = argv[1];
	std::string fileName = argv[2];

	if (catalogName == "")
		return 0;

	Catalog catalog(catalogName);
	catalog.ListEntries();
	Stream* stream = catalog.GetStream(fileName);

	if (stream != nullptr) {
		stream->DumpToFile(fileName.c_str());
		PicDecoder decoder;
		decoder.GetImage(stream);
	}
	delete stream;

	return 0;
}

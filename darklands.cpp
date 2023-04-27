#include "Catalog.h"
#include "FileStream.h"
#include "Stream.h"

#include <iostream>


int main(int argc, char **argv)
{
	std::string catalogName = argv[1];
	std::string fileName = argv[2];

	Catalog catalog(catalogName);
	catalog.ListEntries();
	Stream* stream = catalog.GetStream(fileName);

	stream->DumpToFile(fileName.c_str());
	delete stream;


	// PIC file:
	// first 11 bytes are always the same, except for byte at 0x2
	// 130 060 254 000 035 000 036 000 013 000 040

	return 0;
}

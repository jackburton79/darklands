#include "Catalog.h"
#include "FileStream.h"
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

	if (fileName != "")
		stream->DumpToFile(fileName.c_str());
	delete stream;


	// PIC file:
	// first 11 bytes are always the same, except for byte at 0x02 and 0x03
	// sometimes
	// example E05SWST.PIC:
	// octal: 130 060 251 000 035 000 036 000 013 000 040
	// decimal: 88  48 169   0  29   0  30   0  11   0  32
	// third byte is size - 4. Maybe size of actual data ?

	return 0;
}

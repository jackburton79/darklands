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

	// example E05SWST.PIC:
	// decimal: 88  48 169   0  29   0  30   0  11   0  32
	// E03CLST.PIC:
	// decimal: 88  48 148   0  29   0  30   0  11   0  32

	// 0x00 byte, unknown, always 88
	// 0x01 byte, unknown, always 48
	// 0x02 unsigned, dword, size of file from here on
	// 0x04 ??? always 29
	// 0x05 ??? always 0
	// 0x06 ??? always 30
	// 0x07 ??? always 0
	// 0x08 ??? always 11
	// 0x09 ??? always 0
	// 0x10 ??? always 32
	return 0;
}

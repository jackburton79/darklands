#include "Catalog.h"
#include "Stream.h"

#include <iostream>


int main(int argc, char **argv)
{
	std::string fileName = argv[1];

	Catalog catalog(fileName);
	catalog.ListEntries();
	Stream* stream = catalog.GetStream("IMISCTOM.002");

	stream->Dump();

	return 0;
}

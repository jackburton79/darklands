#include "Catalog.h"

#include <iostream>

/* File format info here
/ https://wendigo.online-siesta.com/darklands/file_formats/up-to-date/
*/

/*
 * 0x00 word LE = numentries
 *
 * entry (24 bytes)
 * 0x02 12	string
 * 0x0c 4	timestamp
 * 0x10 4 length
 * 0x14 4 offset
 */


int main(int argc, char **argv)
{
	std::string fileName = argv[1];

	Catalog catalog(fileName);
	catalog.ListEntries();

	return 0;
}

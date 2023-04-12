#include "FileStream.h"

#include <iostream>


/*
 * 0x00 byte = numentries
 *
 * entry (24 bytes)
 * 0x01 byte = ??
 * 0x02 12	string
 * 0x14 ??
 */


int main(int argc, char **argv)
{
	FileStream map("/home/stefano.ceccherini/git/dk/data/DARKLAND/IMAPS.CAT");

	uint8 numEntries = map.ReadByte();
	std::cout << "num entries: " << (unsigned int)numEntries << std::endl;
	for (auto i = 0; i < numEntries;  i++) {
		off_t position = map.Position();
		std::cout << "position: " << position << std::endl;

		unsigned char byte;
		map.Read(&byte, sizeof(byte));
		std::cout << std::dec << (unsigned int)byte << std::endl;

		char name[16];
		map.Read(name, 12);
		name[11] = '\0';
		std::cout << name << std::endl;

		for (auto c = 0; c < 11; c++) {
			std::cout << (unsigned int)map.ReadByte() << std::endl;
		}
		//map.Seek(position + 24, SEEK_SET);
	}

	return 0;
}

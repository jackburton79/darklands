#include "FileStream.h"

#include <iostream>


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

	FileStream map(fileName.c_str(), FileStream::IGNORE_CASE);

	uint16 numEntries = map.ReadWordLE();
	std::cout << "num entries: " << (unsigned int)numEntries << std::endl;
	for (auto i = 0; i < numEntries;  i++) {
		off_t position = map.Position();
		std::cout << "position: " << std::dec << position << std::endl;

		char name[16];
		map.Read(name, 12);
		name[12] = '\0';
		std::cout << name << std::endl;

		uint32 timestamp = map.ReadDWordLE();
		uint32 length = map.ReadDWordLE();
		uint32 offset = map.ReadDWordLE();


		std::cout << "timestamp:" << timestamp << std::endl;
		std::cout << "offset: " << offset << std::endl;
		std::cout << "length: " << length << std::endl;

		std::cout << "---" << std::endl;
		//map.Seek(position + 24, SEEK_SET);
	}

	uint8 byte = 0;
	while ((byte = map.ReadByte()))
		std::cout << byte << std::endl;

	return 0;
}

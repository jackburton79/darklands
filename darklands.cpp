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
	std::string fileName = argv[1];

	FileStream map(fileName.c_str(), FileStream::IGNORE_CASE);

	uint16 numEntries = map.ReadWordLE();
	std::cout << "num entries: " << (unsigned int)numEntries << std::endl;
	for (auto i = 0; i < numEntries;  i++) {
		off_t position = map.Position();
		std::cout << "position: " << position << std::endl;

		/*unsigned char byte;
		map.Read(&byte, sizeof(byte));
		std::cout << std::dec << (unsigned int)byte << std::endl;
*/
		char name[16];
		map.Read(name, 12);
		name[12] = '\0';
		std::cout << name << std::endl;

		for (auto c = 0; c < 12; c++) {
			std::cout << (unsigned int)map.ReadByte() << std::endl;
		}
		std::cout << "---" << std::endl;
		//map.Seek(position + 24, SEEK_SET);
	}

	return 0;
}

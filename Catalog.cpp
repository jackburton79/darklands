/*
 * Catalog.cpp
 *
 *  Created on: 26 apr 2023
 *      Author: Stefano Ceccherini
 */

#include "Catalog.h"

#include "MemoryStream.h"
#include "FileStream.h"

#include <iostream>

Catalog::Catalog(const std::string& fileName)
	:
	fStream(NULL)
{
	SetTo(fileName);
}


Catalog::~Catalog()
{
	delete fStream;
}


int
Catalog::SetTo(const std::string& fileName)
{
	try {
		fStream = new FileStream(fileName.c_str(), FileStream::READ_ONLY | FileStream::IGNORE_CASE);
	} catch (...) {
		return -1;
	}

	uint16 numEntries = fStream->ReadWordLE();
	for (auto i = 0; i < numEntries;  i++) {
		catalog_entry entry;
		char name[16];
		fStream->Read(name, 12);
		name[12] = '\0';
		entry.filename = name;

		entry.timestamp = fStream->ReadDWordLE();
		entry.length = fStream->ReadDWordLE();
		entry.offset = fStream->ReadDWordLE();
		fEntries.push_back(entry);
	}
	return 0;
}


void
Catalog::ListEntries() const
{
	entry_list::const_iterator i;
	for (i = fEntries.begin(); i != fEntries.end(); i++) {
		std::cout << (*i).filename << std::endl;
		std::cout << "timestamp:" << (*i).timestamp << std::endl;
		std::cout << "offset: " << (*i).offset << std::endl;
		std::cout << "length: " << (*i).length << std::endl;
		std::cout << "---" << std::endl;
	}
}


Stream*
Catalog::GetStream(const std::string& name)
{
	// Allocates a new stream and read the "parent" stream
	// into it.
	// TODO: find a way to attach to the parent stream without
	entry_list::iterator i;
	for (i = fEntries.begin(); i != fEntries.end(); i++) {
		if (name == (*i).filename)
			break;
	}
	if (i == fEntries.end())
		return NULL;

	MemoryStream* stream = new MemoryStream(i->length);
	return stream;
}

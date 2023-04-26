/*
 * Catalog.h
 *
 *  Created on: 26 apr 2023
 *      Author: Stefano Ceccherini
 */

#ifndef CATALOG_H_
#define CATALOG_H_


#include "SupportDefs.h"

#include <list>
#include <string>

struct catalog_entry {
	std::string filename;
	uint32 timestamp;
	uint32 offset;
	uint32 length;
};

class Stream;
class Catalog {
public:
	Catalog(const std::string& fileName);
	virtual ~Catalog();
	void ListEntries() const;
	int SetTo(const std::string& fileName);
	Stream* GetStream(const std::string& name);
private:
	Stream* fStream;

	typedef std::list<catalog_entry> entry_list;
	entry_list fEntries;
};

#endif /* CATALOG_H_ */

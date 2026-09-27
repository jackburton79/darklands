/*
 * Catalog.h
 * Reader for Darklands .CAT catalog files.
 *
 * Format (little-endian):
 *   0x00  uint16      entry count
 *   0x02  entry[24]:  char name[12]; uint32 timestamp, length, offset;
 */

#pragma once

#include "SupportDefs.h"

#include <iosfwd>
#include <string>
#include <vector>

struct catalog_entry {
    std::string filename;
    uint32 timestamp;
    uint32 offset;
    uint32 length;
};

class Stream;

class Catalog {
public:
    explicit			Catalog(const std::string& fileName);	// throws on error
                        ~Catalog();

    // Replaces any previously open file. Throws on error;
    // on failure the Catalog keeps its previous state.
    void				SetTo(const std::string& fileName);

    int32				CountEntries() const;

    // Throws std::out_of_range on invalid index.
    const catalog_entry& EntryAt(int32 index) const;

    // The returned streams are *views* into the catalog's file: the caller
    // owns them and must delete them, but they must be deleted *before*
    // the Catalog itself (they hold a pointer to its internal stream).
    // GetStreamAt() throws std::out_of_range on invalid index;
    // GetStream() returns NULL when no entry has that name
    // (comparison is case-insensitive).
    Stream*				GetStreamAt(uint32 index) const;
    Stream*				GetStream(const std::string& name) const;

    // Debug helper: prints the entry table.
    void				Dump(std::ostream& output) const;

private:
                        Catalog(const Catalog&);			// not copyable
    Catalog&			operator=(const Catalog&);

    Stream*				fStream;

    typedef std::vector<catalog_entry> EntryList;
    EntryList			fEntries;
};

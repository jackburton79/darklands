#include "Catalog.h"

#include "FileStream.h"
#include "Stream.h"

#include <cctype>
#include <ostream>
#include <stdexcept>

static const uint32 kHeaderSize		= sizeof(uint16);
static const uint32 kEntryNameLen	= 12;
static const uint32 kEntrySize		= kEntryNameLen + 3 * sizeof(uint32);

static catalog_entry
ReadEntry(Stream* stream)
{
    char name[kEntryNameLen + 1];
    if (stream->Read(name, kEntryNameLen) != (ssize_t)kEntryNameLen)
        throw std::runtime_error("Catalog: unexpected end of file in entry table");
    name[kEntryNameLen] = '\0';

    catalog_entry entry;
    entry.filename = name;
    // strip trailing space padding
    std::string::size_type last = entry.filename.find_last_not_of(" ");
    if (last == std::string::npos)
        entry.filename.clear();
    else
        entry.filename.erase(last + 1);

    entry.timestamp = stream->ReadDWordLE();
    entry.length = stream->ReadDWordLE();
    entry.offset = stream->ReadDWordLE();
    return entry;
}


static bool
EqualsNoCase(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (std::tolower(uint8(a[i])) != std::tolower(uint8(b[i])))
            return false;
    }
    return true;
}


// #pragma mark - Catalog


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


void
Catalog::SetTo(const std::string& fileName)
{
    Stream* stream = NULL;
    try {
        stream = new FileStream(fileName.c_str(), FileStream::READ_ONLY);
    } catch (...) {
        throw std::runtime_error("Catalog: cannot open " + fileName);
    }

    try {
        uint16 numEntries = stream->ReadWordLEAt(0);

        // Sanity check: the entry table must fit inside the file.
        if (size_t(kHeaderSize) + size_t(numEntries) * kEntrySize
                > stream->Size()) {
            throw std::runtime_error("Catalog: entry table larger than file");
        }

        EntryList entries;
        entries.reserve(numEntries);
        stream->Seek(kHeaderSize, SEEK_SET);
        for (uint16 i = 0; i < numEntries; i++)
            entries.push_back(ReadEntry(stream));

        delete fStream;			// commit point: everything succeeded
        fStream = stream;
        fEntries.swap(entries);
    } catch (...) {
        delete stream;			// previous state survives
        throw;
    }
}


int32
Catalog::CountEntries() const
{
    return int32(fEntries.size());
}


const catalog_entry&
Catalog::EntryAt(int32 index) const
{
    if (index < 0 || size_t(index) >= fEntries.size())
        throw std::out_of_range("Catalog::EntryAt(): invalid index");
    return fEntries[size_t(index)];
}


Stream*
Catalog::GetStreamAt(uint32 index) const
{
    if (index >= fEntries.size())
        throw std::out_of_range("Catalog::GetStreamAt(): invalid index");
    const catalog_entry& entry = fEntries[index];

    // SubStreamAdapter only asserts these in debug builds, so validate here:
    // corrupt catalogs must not produce sub-streams reaching past the file.
    const size_t fileSize = fStream->Size();
    if (size_t(entry.offset) >= fileSize
            || size_t(entry.length) > fileSize - size_t(entry.offset)) {
        throw std::runtime_error("Catalog: entry \"" + entry.filename
            + "\" exceeds file size");
    }

    return fStream->SubStream(entry.offset, entry.length);
}


Stream*
Catalog::GetStream(const std::string& name) const
{
    for (uint32 i = 0; i < fEntries.size(); i++) {
        if (EqualsNoCase(name, fEntries[i].filename))
            return GetStreamAt(i);
    }
    return NULL;
}


void
Catalog::Dump(std::ostream& output) const
{
    for (uint32 i = 0; i < fEntries.size(); i++) {
        const catalog_entry& entry = fEntries[i];
        output << entry.filename
            << "\toffset: " << entry.offset
            << " (0x" << std::hex << entry.offset << std::dec << ")"
            << ", length: " << entry.length
            << ", timestamp: " << entry.timestamp
            << std::endl;
    }
}

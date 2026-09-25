#include "LocationFile.h"

#include "FileStream.h"
#include "Stream.h"

#include <cstring>
#include <stdexcept>

static const size_t kRecordSize		= 58;	// verified: 2 + 414 * 58 == file size
static const size_t kNameOffset		= 0x26;
static const size_t kNameLength		= 20;


LocationFile::LocationFile(const std::string& fileName)
{
    Stream* stream = new FileStream(fileName.c_str(), FileStream::READ_ONLY);
    try {
        const size_t size = stream->Size();
        if (size < 2)
            throw std::runtime_error("LocationFile: file too small");
        const uint16 count = stream->ReadWordLEAt(0);
        if (size < 2 + size_t(count) * kRecordSize)
            throw std::runtime_error("LocationFile: truncated file");

        fLocations.reserve(count);
        for (uint16 i = 0; i < count; i++) {
            uint8 record[kRecordSize];
            if (stream->ReadAt(2 + i * kRecordSize, record, kRecordSize)
                    != (ssize_t)kRecordSize) {
                throw std::runtime_error("LocationFile: truncated record");
            }
            location loc;
            loc.type = record[0x00] | (record[0x01] << 8);
            loc.x = record[0x04] | (record[0x05] << 8);
            loc.y = record[0x06] | (record[0x07] << 8);
            loc.size = record[0x11];
            const char* name = (const char*)&record[kNameOffset];
            loc.name = DecodeName(name, strnlen(name, kNameLength));
            fLocations.push_back(loc);
        }
    } catch (...) {
        delete stream;
        throw;
    }
    delete stream;
}


uint32
LocationFile::CountLocations() const
{
    return uint32(fLocations.size());
}


const location&
LocationFile::LocationAt(uint32 index) const
{
    if (index >= fLocations.size())
        throw std::out_of_range("LocationFile::LocationAt(): invalid index");
    return fLocations[index];
}


/* static */
std::string
LocationFile::DecodeName(const char* name, size_t length)
{
    std::string result;
    for (size_t i = 0; i < length; i++) {
        switch (name[i]) {
            case '|':	result += "\xC3\xBC"; break;	// ü
            case '{':	result += "\xC3\xB6"; break;	// ö
            case 0x1F:	result += "\xC3\xA4"; break;	// ä
            default:	result += name[i]; break;
        }
    }
    return result;
}

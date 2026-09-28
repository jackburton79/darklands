#include "LocationFile.h"

#include "FileStream.h"
#include "Stream.h"

#include <cstring>
#include <stdexcept>

// kRecordSize: verified, 2 + 414 * 58 == file size
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
            loc.enterState = uint16(record[0x0C] | (record[0x0D] << 8));
            loc.territoryState = uint16(record[0x0E] | (record[0x0F] << 8));
            const char* name = (const char*)&record[kNameOffset];
            loc.name = DecodeName(name, strnlen(name, kNameLength));
            fLocations.push_back(loc);
            fRecords.insert(fRecords.end(), record, record + kRecordSize);
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


const uint8*
LocationFile::RecordAt(uint32 index) const
{
    if (index >= fLocations.size())
        throw std::out_of_range("LocationFile::RecordAt(): invalid index");
    return &fRecords[size_t(index) * kRecordSize];
}


/* static */
std::string
LocationFile::DecodeName(const char* name, size_t length)
{
    std::string result;
    for (size_t i = 0; i < length; i++) {
        switch (name[i]) {
            case 0x1F:	result += "\xC3\xA4"; break;	// ä
            case '{':	result += "\xC3\xB6"; break;	// ö
            case '|':	result += "\xC3\xBC"; break;	// ü
            case '[':	result += "\xC3\x84"; break;	// Ä
            case '\\':	result += "\xC3\x96"; break;	// Ö
            case ']':	result += "\xC3\x9C"; break;	// Ü
            case '_':	result += "\xC3\x9F"; break;	// ß
            default:	result += name[i]; break;
        }
    }
    return result;
}


/* static */
std::string
LocationFile::EncodeName(const std::string& utf8)
{
    static const struct {
        const char* utf8;
        char game;
    } kLetters[] = {
        { "\xC3\xA4", 0x1F }, { "\xC3\xB6", '{' }, { "\xC3\xBC", '|' },
        { "\xC3\x84", '[' }, { "\xC3\x96", '\\' }, { "\xC3\x9C", ']' },
        { "\xC3\x9F", '_' }
    };
    std::string result;
    for (size_t i = 0; i < utf8.size(); i++) {
        const uint8 c = uint8(utf8[i]);
        if (c < 0x80) {
            result += char(c);
            continue;
        }
        char game = '?';
        for (const auto& letter : kLetters) {
            if (utf8.compare(i, 2, letter.utf8) == 0)
                game = letter.game;
        }
        result += game;
        while (i + 1 < utf8.size() && (uint8(utf8[i + 1]) & 0xC0) == 0x80)
            i++;
    }
    return result;
}

#include "CityFile.h"

#include "FileStream.h"
#include "LocationFile.h"
#include "Stream.h"

#include <cstring>
#include <stdexcept>

static const size_t kRecordSize		= 622;	// verified: 1 + 92 * 622 == file size
static const size_t kNameLength		= 32;	// all strings are 32-byte fields
static const size_t kFullNameOffset	= 0x20;
static const size_t kPlacesOffset	= 0x6E;
static const int kNeighborCount		= 4;


static inline uint16
WordAt(const uint8* data, size_t offset)
{
    return data[offset] | (data[offset + 1] << 8);
}


// String fields are NUL-terminated; the bytes after the NUL are leftover
// garbage from the tool that wrote the file. A lone space means "none".
static std::string
StringAt(const uint8* data, size_t offset)
{
    const char* string = (const char*)&data[offset];
    std::string result = LocationFile::DecodeName(string,
        strnlen(string, kNameLength));
    if (result == " ")
        result.clear();
    return result;
}


CityFile::CityFile(const std::string& fileName)
{
    Stream* stream = new FileStream(fileName.c_str(), FileStream::READ_ONLY);
    try {
        const size_t size = stream->Size();
        uint8 count = 0;
        if (size < 1 || stream->ReadAt(0, &count, 1) != 1)
            throw std::runtime_error("CityFile: file too small");
        if (size < 1 + size_t(count) * kRecordSize)
            throw std::runtime_error("CityFile: truncated file");

        fCities.reserve(count);
        for (uint8 i = 0; i < count; i++) {
            uint8 record[kRecordSize];
            if (stream->ReadAt(1 + i * kRecordSize, record, kRecordSize)
                    != (ssize_t)kRecordSize) {
                throw std::runtime_error("CityFile: truncated record");
            }
            city c;
            c.shortName = StringAt(record, 0);
            c.fullName = StringAt(record, kFullNameOffset);
            c.size = uint8(WordAt(record, 0x40));
            c.x = WordAt(record, 0x42);
            c.y = WordAt(record, 0x44);
            c.x2 = WordAt(record, 0x46);
            c.y2 = WordAt(record, 0x48);
            for (int n = 0; n < kNeighborCount; n++) {
                const uint16 neighbor = WordAt(record, 0x4A + 2 * n);
                if (neighbor != 0xFFFF)
                    c.neighbors.push_back(neighbor);
            }
            c.harbor = WordAt(record, 0x52);
            for (int p = 0; p < CITY_PLACE_COUNT; p++)
                c.places[p] = StringAt(record, kPlacesOffset + p * kNameLength);
            fCities.push_back(c);
        }
    } catch (...) {
        delete stream;
        throw;
    }
    delete stream;
}


uint32
CityFile::CountCities() const
{
    return uint32(fCities.size());
}


const city&
CityFile::CityAt(uint32 index) const
{
    if (index >= fCities.size())
        throw std::out_of_range("CityFile::CityAt(): invalid index");
    return fCities[index];
}

#include "ListFile.h"

#include "FileStream.h"
#include "LocationFile.h"
#include "Stream.h"

#include <cstring>
#include <memory>
#include <stdexcept>

// verified: 3 + 200 * 46 bytes, then 2 * 136 + 2 * 66 strings
static const size_t kHeaderSize		= 3;
static const size_t kItemSize		= 0x2E;
static const size_t kNameLength		= 20;
static const size_t kShortNameOffset = 0x14;
static const size_t kShortNameLength = 10;
static const size_t kTypeOffset		= 0x1E;
static const size_t kFlagsOffset	= 0x20;
static const size_t kWeightOffset	= 0x25;
static const size_t kQualityOffset	= 0x26;
static const size_t kRarityOffset	= 0x27;
static const size_t kValueOffset	= 0x2C;


static std::string
StringAt(const std::vector<uint8>& data, size_t offset, size_t length)
{
    const char* string = (const char*)&data[offset];
    return LocationFile::DecodeName(string, strnlen(string, length));
}


ListFile::ListFile(const std::string& fileName)
{
    std::unique_ptr<Stream> stream(
        new FileStream(fileName.c_str(), FileStream::READ_ONLY));
    const size_t size = stream->Size();
    std::vector<uint8> data(size);
    if (size < kHeaderSize
            || stream->ReadAt(0, data.data(), size) != (ssize_t)size)
        throw std::runtime_error("ListFile: file too small");

    const size_t itemCount = data[0];
    const size_t saintCount = data[1];
    const size_t formulaCount = data[2];
    size_t offset = kHeaderSize + itemCount * kItemSize;
    if (size < offset)
        throw std::runtime_error("ListFile: truncated items");
    for (size_t i = 0; i < itemCount; i++) {
        const size_t record = kHeaderSize + i * kItemSize;
        item_definition item;
        item.name = StringAt(data, record, kNameLength);
        item.shortName = StringAt(data, record + kShortNameOffset,
            kShortNameLength);
        item.type = uint16(data[record + kTypeOffset]
            | (data[record + kTypeOffset + 1] << 8));
        item.flags = uint32(data[record + kFlagsOffset])
            | (uint32(data[record + kFlagsOffset + 1]) << 8)
            | (uint32(data[record + kFlagsOffset + 2]) << 16)
            | (uint32(data[record + kFlagsOffset + 3]) << 24);
        item.unsellable = (data[record + kFlagsOffset + 4] & 0x80) != 0;
        item.weight = data[record + kWeightOffset];
        item.quality = data[record + kQualityOffset];
        item.rarity = data[record + kRarityOffset];
        item.value = uint16(data[record + kValueOffset]
            | (data[record + kValueOffset + 1] << 8));
        fItems.push_back(item);
    }

    // then NUL-terminated strings: saints, their short names, formulae,
    // their short names
    std::vector<std::string>* lists[4] = { &fSaints, &fSaintShortNames,
        &fFormulae, &fFormulaShortNames };
    const size_t counts[4] = { saintCount, saintCount, formulaCount,
        formulaCount };
    for (int list = 0; list < 4; list++) {
        for (size_t i = 0; i < counts[list]; i++) {
            const void* end = offset < size
                ? memchr(&data[offset], 0, size - offset) : NULL;
            if (end == NULL)
                throw std::runtime_error("ListFile: truncated names");
            const size_t length = (const uint8*)end - &data[offset];
            lists[list]->push_back(StringAt(data, offset, length));
            offset += length + 1;
        }
    }
}

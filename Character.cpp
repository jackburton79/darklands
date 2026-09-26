#include "Character.h"

#include "LocationFile.h"

#include <cstring>
#include <stdexcept>

// Record layout (see docs/formats.md)
static const size_t kAgeOffset			= 0x12;
static const size_t kShieldOffset		= 0x15;
static const size_t kSexOffset			= 0x17;	// 1: female (inferred)
static const size_t kFullNameOffset		= 0x25;
static const size_t kFullNameLength		= 25;
static const size_t kShortNameOffset	= 0x3E;
static const size_t kShortNameLength	= 11;
static const size_t kAttributesOffset	= 0x5D;
static const size_t kMaxAttributesOffset = 0x64;
static const size_t kSkillsOffset		= 0x6B;
static const size_t kItemCountOffset	= 0x7E;
static const size_t kItemsOffset		= 0xAA;
static const size_t kItemSize			= 6;
static const size_t kMaxItems			= 64;	// fills the record


static std::string
StringAt(const uint8* record, size_t offset, size_t length)
{
    const char* string = (const char*)&record[offset];
    return LocationFile::DecodeName(string, strnlen(string, length));
}


character
ReadCharacter(const uint8* record)
{
    character c;
    c.fullName = StringAt(record, kFullNameOffset, kFullNameLength);
    c.shortName = StringAt(record, kShortNameOffset, kShortNameLength);
    c.age = uint16(record[kAgeOffset] | (record[kAgeOffset + 1] << 8));
    c.female = record[kSexOffset] == 1;
    c.shield = char(record[kShieldOffset]);
    memcpy(c.attributes, &record[kAttributesOffset], ATTRIBUTE_COUNT);
    memcpy(c.maxAttributes, &record[kMaxAttributesOffset], ATTRIBUTE_COUNT);
    memcpy(c.skills, &record[kSkillsOffset], kSkillCount);

    const size_t count = record[kItemCountOffset]
        | (record[kItemCountOffset + 1] << 8);
    if (count > kMaxItems)
        throw std::runtime_error("character: too many items");
    for (size_t i = 0; i < count; i++) {
        const uint8* data = &record[kItemsOffset + i * kItemSize];
        c.items.push_back(item{ uint16(data[0] | (data[1] << 8)), data[2],
            data[3], data[4], data[5] });
    }
    return c;
}


party
MakeParty(const std::vector<character>& characters, const uint8* indices,
    const uint8* images, int leader)
{
    party p;
    p.leader = 0;
    p.cash = money{ 0, 0, 0 };
    for (int slot = 0; slot < kMaxPartySize; slot++) {
        const uint16 index = uint16(indices[slot * 2]
            | (indices[slot * 2 + 1] << 8));
        if (index == 0xFFFF)
            continue;
        if (index >= characters.size())
            throw std::runtime_error("party: invalid character index");
        if (slot == leader)
            p.leader = int(p.members.size());
        p.members.push_back(characters[index]);
        const char* image = (const char*)&images[slot * 4];
        p.images.push_back(std::string(image, strnlen(image, 4)));
    }
    return p;
}

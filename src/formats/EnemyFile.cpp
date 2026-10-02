#include "EnemyFile.h"

#include "FileStream.h"
#include "LocationFile.h"
#include "Stream.h"

#include <cstring>
#include <stdexcept>

// verified: 71 * 204 + 82 * 24 == file size, and every record's name
// falls at the same offset
static const uint32 kTypeCount		= 71;
static const size_t kTypeSize		= 204;
static const uint32 kEnemyCount		= 82;
static const size_t kEnemySize		= 24;

static const size_t kImageLength	= 4;
static const size_t kTypeNameOffset	= 0x04;
static const size_t kTypeNameLength	= 10;
static const size_t kVariantsOffset	= 0x0E;
static const size_t kPaletteOffset	= 0x0F;	// count, chunks, first chunk
static const size_t kAttributesOffset = 0x14;
static const size_t kSkillsOffset	= 0x1B;
// verified: in 64 of the 70 types it names an existing sprite of theirs
static const size_t kWeaponOffset	= 0xA0;
// inferred: armor types of the right kinds (67..75 then 76..84, or the
// monsters' hides 85..91), shields 95..97
static const size_t kArmorOffset	= 0x92;
static const size_t kArmorQualityOffset = 0x96;
static const size_t kShieldOffset	= 0x97;
static const size_t kShieldQualityOffset = 0x99;

// verified (code, file 0x18DDB): per foe, a die of the word at +0xC0 (+0xC2,
// +0xC4) sides plus the word at +0xC6 (+0xC8, +0xCA) florins (groschen,
// pfennigs)
static const size_t kCashDiceOffset	= 0xC0;
static const size_t kCashPlusOffset	= 0xC6;

static const size_t kEnemyNameOffset = 0x02;
static const size_t kEnemyNameLength = 12;	// "Castle Guard" fills it, no NUL
static const size_t kFlagsOffset	= 0x16;


static inline uint16
WordAt(const uint8* data, size_t offset)
{
    return data[offset] | (data[offset + 1] << 8);
}


static std::string
StringAt(const uint8* data, size_t offset, size_t length)
{
    const char* string = (const char*)&data[offset];
    return LocationFile::DecodeName(string, strnlen(string, length));
}


EnemyFile::EnemyFile(const std::string& fileName)
{
    Stream* stream = new FileStream(fileName.c_str(), FileStream::READ_ONLY);
    try {
        const size_t typesSize = kTypeCount * kTypeSize;
        if (stream->Size() != typesSize + kEnemyCount * kEnemySize)
            throw std::runtime_error("EnemyFile: unexpected file size");

        fTypes.reserve(kTypeCount);
        for (uint32 i = 0; i < kTypeCount; i++) {
            uint8 record[kTypeSize];
            if (stream->ReadAt(i * kTypeSize, record, kTypeSize)
                    != (ssize_t)kTypeSize) {
                throw std::runtime_error("EnemyFile: truncated type");
            }
            enemy_type t;
            t.image = StringAt(record, 0, kImageLength);
            t.name = StringAt(record, kTypeNameOffset, kTypeNameLength);
            // 0xFF in the group's other types
            t.variants = record[kVariantsOffset] == 0xFF
                ? 0 : record[kVariantsOffset];
            t.paletteCount = record[kPaletteOffset];
            t.paletteChunks = record[kPaletteOffset + 1];
            t.firstPaletteChunk = record[kPaletteOffset + 2];
            memcpy(t.attributes, &record[kAttributesOffset],
                sizeof(t.attributes));
            memcpy(t.skills, &record[kSkillsOffset], sizeof(t.skills));
            t.weapon = record[kWeaponOffset];
            t.armor[0] = record[kArmorOffset];
            t.armor[1] = record[kArmorOffset + 1];
            t.armorQuality = record[kArmorQualityOffset];
            t.shield = record[kShieldOffset];
            t.shieldQuality = record[kShieldQualityOffset];
            for (int k = 0; k < 3; k++) {
                t.cashDice[k] = WordAt(record, kCashDiceOffset + k * 2);
                t.cashPlus[k] = int16(WordAt(record, kCashPlusOffset + k * 2));
            }
            fTypes.push_back(t);
        }

        fEnemies.reserve(kEnemyCount);
        for (uint32 i = 0; i < kEnemyCount; i++) {
            uint8 record[kEnemySize];
            if (stream->ReadAt(typesSize + i * kEnemySize, record, kEnemySize)
                    != (ssize_t)kEnemySize) {
                throw std::runtime_error("EnemyFile: truncated enemy");
            }
            enemy e;
            e.type = WordAt(record, 0);
            if (e.type >= kTypeCount)
                throw std::runtime_error("EnemyFile: invalid enemy type");
            e.name = StringAt(record, kEnemyNameOffset, kEnemyNameLength);
            e.flags = WordAt(record, kFlagsOffset);
            fEnemies.push_back(e);
        }
    } catch (...) {
        delete stream;
        throw;
    }
    delete stream;
}


uint32
EnemyFile::CountTypes() const
{
    return uint32(fTypes.size());
}


const enemy_type&
EnemyFile::TypeAt(uint32 index) const
{
    if (index >= fTypes.size())
        throw std::out_of_range("EnemyFile::TypeAt(): invalid index");
    return fTypes[index];
}


uint32
EnemyFile::CountEnemies() const
{
    return uint32(fEnemies.size());
}


const enemy&
EnemyFile::EnemyAt(uint32 index) const
{
    if (index >= fEnemies.size())
        throw std::out_of_range("EnemyFile::EnemyAt(): invalid index");
    return fEnemies[index];
}

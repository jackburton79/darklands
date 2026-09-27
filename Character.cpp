#include "Character.h"

#include "LocationFile.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

// Record layout (see docs/formats.md)
static const size_t kAgeOffset			= 0x12;
static const size_t kHeraldryOffset		= 0x15;
static const size_t kSexOffset			= 0x17;	// 1: female (inferred)
static const size_t kMissileOffset		= 0x22;
static const size_t kVitalsOffset		= 0x4B;
static const size_t kLimbsOffset		= 0x4C;
static const size_t kWeaponOffset		= 0x51;
static const size_t kShieldOffset		= 0x5C;
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
    c.heraldry = char(record[kHeraldryOffset]);
    memcpy(c.attributes, &record[kAttributesOffset], ATTRIBUTE_COUNT);
    memcpy(c.maxAttributes, &record[kMaxAttributesOffset], ATTRIBUTE_COUNT);
    memcpy(c.skills, &record[kSkillsOffset], kSkillCount);
    c.equipment[EQUIPMENT_WEAPON] = record[kWeaponOffset];
    c.equipment[EQUIPMENT_VITALS] = record[kVitalsOffset];
    c.equipment[EQUIPMENT_LIMBS] = record[kLimbsOffset];
    c.equipment[EQUIPMENT_SHIELD] = record[kShieldOffset];
    c.equipment[EQUIPMENT_MISSILE] = record[kMissileOffset];

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
    p.fame = 0;
    p.bankNotes = 0;
    p.philosopherStone = 0;
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


uint32
TotalPfennigs(const money& amount)
{
    return uint32(amount.florins) * 240 + uint32(amount.groschen) * 12
        + amount.pfennigs;
}


money
MoneyFromPfennigs(uint32 pfennigs)
{
    return money{ uint16(pfennigs / 240), uint16(pfennigs % 240 / 12),
        uint16(pfennigs % 12) };
}


std::string
MoneyText(uint32 pfennigs)
{
    const money amount = MoneyFromPfennigs(pfennigs);
    std::vector<std::string> parts;
    if (amount.florins > 0) {
        parts.push_back(amount.florins == 1 ? std::string("1 florin")
            : std::to_string(amount.florins) + " florins");
    }
    if (amount.groschen > 0)
        parts.push_back(std::to_string(amount.groschen) + " groschen");
    if (amount.pfennigs > 0) {
        parts.push_back(amount.pfennigs == 1 ? std::string("1 pfennig")
            : std::to_string(amount.pfennigs) + " pfennigs");
    }
    std::string text;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0)
            text += i + 1 == parts.size() ? " and " : ", ";
        text += parts[i];
    }
    return text;
}


void
AddToAttribute(character& member, int attribute, int amount)
{
    int value = member.attributes[attribute] + amount;
    value = std::max(1, std::min(value, 99));
    value = std::min(value, int(member.maxAttributes[attribute]));
    member.attributes[attribute] = uint8(value);
}


// Each member, in party order: endurance comes back to 4 under its
// maximum at once, then by a point while 3 or 4 are missing. When a day
// begins: divine favor + clamp(1, 5, Religion / (20 + random(5))),
// strength + 1 if random(150) is at most the party's best Healing, and
// agility, perception and charisma + 1 each if random(100) is at most
// 10. Then divine favor, strength, perception, agility and charisma are
// kept under their maximum (plus the bonus of a potion: none here).
void
PassTime(party& members, bool newDay, const std::function<int(int)>& random)
{
    int bestHealing = 0;
    for (const character& member : members.members)
        bestHealing = std::max(bestHealing, int(member.skills[kSkillHealing]));
    for (character& member : members.members) {
        uint8* current = member.attributes;
        const uint8* maximum = member.maxAttributes;
        const int missing = maximum[ATTRIBUTE_ENDURANCE]
            - current[ATTRIBUTE_ENDURANCE];
        if (missing > 4)
            current[ATTRIBUTE_ENDURANCE] = uint8(maximum[ATTRIBUTE_ENDURANCE] - 4);
        else if (missing > 2)
            current[ATTRIBUTE_ENDURANCE]++;
        if (newDay) {
            const int religion = member.skills[kSkillReligion];
            current[ATTRIBUTE_DIVINE_FAVOR] = uint8(current[ATTRIBUTE_DIVINE_FAVOR]
                + std::max(1, std::min(religion / (random(5) + 20), 5)));
            if (bestHealing >= random(150))
                current[ATTRIBUTE_STRENGTH]++;
            if (random(100) <= 10)
                current[ATTRIBUTE_AGILITY]++;
            if (random(100) <= 10)
                current[ATTRIBUTE_PERCEPTION]++;
            if (random(100) <= 10)
                current[ATTRIBUTE_CHARISMA]++;
        }
        static const int kCapped[] = { ATTRIBUTE_DIVINE_FAVOR,
            ATTRIBUTE_STRENGTH, ATTRIBUTE_PERCEPTION, ATTRIBUTE_AGILITY,
            ATTRIBUTE_CHARISMA };
        for (int attribute : kCapped)
            current[attribute] = std::min(current[attribute], maximum[attribute]);
    }
}


bool
TrainSkill(character& member, int skill, int amount,
    const std::function<int(int)>& random)
{
    if (amount * 33 / 10 < random(100) || member.skills[skill] >= 99)
        return false;
    member.skills[skill]++;
    return true;
}

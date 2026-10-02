#include "Equipment.h"

#include <algorithm>

static const size_t kWeightOffset = 0x49;
// Item types of the armors and shields (DARKLAND.LST)
static const uint8 kFirstVitals		= 67;
static const uint8 kFirstLimbs		= 76;
static const uint8 kFirstShield		= 95;
static const uint8 kEndShield		= 98;


int
SlotForItem(const item_definition& definition, uint8 type)
{
    const uint32 flags = definition.flags;
    if ((flags & 0x0F) != 0)
        return EQUIPMENT_WEAPON;
    if ((flags & 0x30) != 0 || (flags & ITEM_MISSILE_DEVICE) != 0)
        return EQUIPMENT_MISSILE;
    if (type >= kFirstVitals && type < kFirstLimbs)
        return EQUIPMENT_VITALS;
    if (type >= kFirstLimbs && type < kFirstShield)
        return EQUIPMENT_LIMBS;
    if (type >= kFirstShield && type < kEndShield)
        return EQUIPMENT_SHIELD;
    return -1;
}


const item*
ItemInUse(const character& member, int slot)
{
    if (member.equipment[slot] == kNoEquipment)
        return NULL;
    for (const item& carried : member.items) {
        if (carried.type == member.equipment[slot]
                && carried.quality == member.equipmentQuality[slot])
            return &carried;
    }
    return NULL;
}


int
SlotInUse(const character& member, const item& carried)
{
    for (int slot = 0; slot < EQUIPMENT_COUNT; slot++) {
        if (member.equipment[slot] == carried.type
                && member.equipmentQuality[slot] == carried.quality)
            return slot;
    }
    return -1;
}


int
WeightInUse(character& member)
{
    int weight = 0;
    for (int slot = 0; slot < EQUIPMENT_COUNT; slot++) {
        const item* inUse = ItemInUse(member, slot);
        if (inUse != NULL)
            weight += inUse->weight;
    }
    weight = std::min(weight, kMaxWeightInUse);
    if (member.record.size() == kCharacterRecordSize)
        member.record[kWeightOffset] = uint8(weight & 0xFF);
    return weight;
}


int
EncumbranceClass(character& member)
{
    const int capacity = member.attributes[ATTRIBUTE_ENDURANCE]
        + member.attributes[ATTRIBUTE_STRENGTH];
    // the byte is read as signed, then compared as unsigned
    const unsigned weight = unsigned(sint16(sint8(WeightInUse(member) & 0xFF)))
        & 0xFFFF;
    if (unsigned(capacity) >= weight)
        return 0;
    return weight > unsigned(capacity * 3 / 2) ? 3 : 2;
}


int
MemberSpeed(character& member)
{
    const int agility = member.attributes[ATTRIBUTE_AGILITY];
    switch (EncumbranceClass(member)) {
        case 0:
            return agility;
        case 3:
            return 1;
        default:
            return agility * 2 / 3;
    }
}


bool
ReadyItem(character& member, size_t index,
    const std::vector<item_definition>& definitions)
{
    if (index >= member.items.size())
        return false;
    const item& carried = member.items[index];
    if (carried.code >= definitions.size())
        return false;
    const int slot = SlotForItem(definitions[carried.code], carried.type);
    if (slot < 0)
        return false;
    member.equipment[slot] = carried.type;
    member.equipmentQuality[slot] = carried.quality;
    WeightInUse(member);
    return true;
}


bool
UnreadyItem(character& member, size_t index)
{
    if (index >= member.items.size())
        return false;
    const int slot = SlotInUse(member, member.items[index]);
    if (slot < 0)
        return false;
    member.equipment[slot] = kNoEquipment;
    WeightInUse(member);
    return true;
}


bool
AddItem(character& member, const item& added)
{
    for (item& have : member.items) {
        if (have.code == added.code && have.type == added.type
                && have.quality == added.quality
                && have.quantity + added.quantity <= kMaxItemQuantity) {
            have.quantity = uint8(have.quantity + added.quantity);
            return true;
        }
    }
    if (member.items.size() >= kMaxCarriedItems)
        return false;
    member.items.push_back(added);
    return true;
}


bool
DropItem(character& member, size_t index, bool all)
{
    if (index >= member.items.size())
        return false;
    if (!all && member.items[index].quantity > 1) {
        member.items[index].quantity--;
        return true;
    }
    UnreadyItem(member, index);
    member.items.erase(member.items.begin() + long(index));
    return true;
}


bool
GiveItem(character& from, size_t index, character& to, bool all)
{
    if (&from == &to || index >= from.items.size()
            || to.items.size() >= kMaxCarriedItems)
        return false;
    item given = from.items[index];
    if (!all)
        given.quantity = 1;
    if (!AddItem(to, given))
        return false;
    return DropItem(from, index, all);
}


bool
MoveItem(character& member, size_t from, size_t to)
{
    if (from == to || from >= member.items.size()
            || to >= member.items.size())
        return false;
    const item moved = member.items[from];
    member.items.erase(member.items.begin() + long(from));
    member.items.insert(member.items.begin() + long(to), moved);
    return true;
}

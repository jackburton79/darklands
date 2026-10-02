#include "Combat.h"

#include "Character.h"
#include "EnemyFile.h"
#include "ExeData.h"

#include <algorithm>

static const int kEdgedSkill	= 0;	// the weapon skills, by category
static const int kMaxEnduranceLoss = 42;	// per strike (file 0x4462E)

// The damage roll for small values (DS:1B90 and DS:1B88)
static const int kDivisors[8]	= { 13, 7, 5, 7, 5, 4, 3, 3 };
static const int kAdditions[8]	= { 1, 1, 1, 2, 2, 2, 2, 3 };

// The shields (item types)
static const int kSmallShield	= 95;
static const int kMediumShield	= 96;
static const int kLargeShield	= 97;


// die(n), 0000:0C8C: 1..n, n at least 2
static int
Die(std::mt19937& random, int n)
{
    n = std::max(n, 2);
    return int(random() % uint32(n)) + 1;
}


static const exe_weapon*
WeaponOf(const fighter& f, const ExeData& exe)
{
    if (f.weaponType < 0 || size_t(f.weaponType) >= exe.Weapons().size())
        return NULL;
    return &exe.Weapons()[size_t(f.weaponType)];
}


// The quality of the carried item of that type
static int
QualityOf(const character& member, int type)
{
    for (const item& i : member.items) {
        if (i.type == type)
            return i.quality;
    }
    return 0;
}


static int
Positive(int value)
{
    return std::max(value, 0);
}


// Whether the Orders have all of the bits
static bool
Has(int orders, int bits)
{
    return (orders & bits) == bits;
}


int
MeleeAttack(const fighter& f, const exe_weapon& weapon)
{
    const int weak = Positive(weapon.minStrength - f.maxStrength);
    const int strong = Positive(f.maxStrength - weapon.maxStrength);
    const int unskilled = Positive(weapon.skill - f.weaponSkill);
    return std::min(255, Positive(f.weaponSkill + 2 * strong - 3 * weak
        - 2 * unskilled));
}


fighter
FighterFromCharacter(const character& member, const ExeData& exe)
{
    fighter f = {};
    f.orders = STANCE_STANDARD;
    f.status = FIGHTER_ACTIVE;
    f.endurance = member.attributes[ATTRIBUTE_ENDURANCE];
    f.strength = member.attributes[ATTRIBUTE_STRENGTH];
    f.maxStrength = member.maxAttributes[ATTRIBUTE_STRENGTH];
    f.agility = member.attributes[ATTRIBUTE_AGILITY];
    const int weapon = member.equipment[EQUIPMENT_WEAPON];
    f.weaponType = weapon < int(exe.Weapons().size()) ? weapon : -1;
    f.weaponQuality = weapon != kNoEquipment ? QualityOf(member, weapon) : 0;
    for (int l = 0; l < 2; l++) {
        const int armor = member.equipment[l == HIT_VITALS
            ? EQUIPMENT_VITALS : EQUIPMENT_LIMBS];
        f.armor[l] = armor != kNoEquipment ? exe.ArmorStrength(armor) : 0;
        f.armorQuality[l] = armor != kNoEquipment
            ? QualityOf(member, armor) : 0;
    }
    const int shield = member.equipment[EQUIPMENT_SHIELD];
    f.shieldType = shield != kNoEquipment ? shield : 0;
    f.shieldQuality = shield != kNoEquipment ? QualityOf(member, shield) : 0;
    const exe_weapon* w = WeaponOf(f, exe);
    f.weaponSkill = member.skills[w != NULL ? w->category : kEdgedSkill];
    f.attack = w != NULL ? MeleeAttack(f, *w) : 0;
    return f;
}


fighter
FighterFromEnemy(const enemy_type& type, const ExeData& exe)
{
    fighter f = {};
    f.orders = STANCE_STANDARD;
    f.status = FIGHTER_ACTIVE;
    f.endurance = type.attributes[ATTRIBUTE_ENDURANCE];
    f.strength = type.attributes[ATTRIBUTE_STRENGTH];
    f.maxStrength = type.attributes[ATTRIBUTE_STRENGTH];
    f.agility = type.attributes[ATTRIBUTE_AGILITY];
    f.weaponType = type.weapon < exe.Weapons().size() ? type.weapon : -1;
    f.weaponQuality = type.armorQuality;
    for (int l = 0; l < 2; l++) {
        f.armor[l] = type.armor[l] != 0xFF ? exe.ArmorStrength(type.armor[l])
            : 0;
        f.armorQuality[l] = type.armorQuality;
    }
    f.shieldType = type.shield != 0xFF ? type.shield : 0;
    f.shieldQuality = type.shieldQuality;
    const exe_weapon* w = WeaponOf(f, exe);
    f.weaponSkill = type.skills[w != NULL ? w->category : kEdgedSkill];
    f.attack = w != NULL ? MeleeAttack(f, *w) : 0;
    return f;
}


int
StrikeRate(const fighter& f, const ExeData& exe)
{
    const exe_weapon* w = WeaponOf(f, exe);
    const int speed = w != NULL ? w->speed : 0;
    const int d = (200 - f.weaponSkill - f.agility) / 2 + (speed + 15) * 2;
    int rate = d > 0 ? 6000 / d : 100;
    // the stance (file 0x43CA2)
    if (Has(f.orders, 0x06))
        rate -= 30;
    else if (Has(f.orders, 0x22))
        rate += 60;
    else if (Has(f.orders, 0x0A))
        rate += 120;
    return rate;
}


int
ChanceToHit(const fighter& attacker, const fighter& defender,
    const ExeData& exe, int helpers, int threats)
{
    // the shield: against all but flails, with a one-handed weapon
    int shield = -5;
    const exe_weapon* a = WeaponOf(attacker, exe);
    const exe_weapon* d = WeaponOf(defender, exe);
    if ((a == NULL || a->category != WEAPON_FLAIL) && d != NULL
        && d->hands == 1) {
        switch (defender.shieldType) {
            case kSmallShield:
                shield = defender.shieldQuality * 2 / 7;
                break;
            case kMediumShield:
                shield = defender.shieldQuality * 2 / 5;
                break;
            case kLargeShield:
                shield = defender.shieldQuality / 2;
                break;
        }
    }
    const int defense = Positive(defender.attack + shield);
    // the stances (file 0x44232): the attacker's care or wildness, then
    // the defender's guard or exposure
    int m = 0;
    if (Has(attacker.orders, 0x0A))
        m = -5;
    else if (Has(attacker.orders, 0x22))
        m = -2 - attacker.weaponSkill / 4;
    else if (Has(attacker.orders, 0x06))
        m = std::max(attacker.weaponSkill / 4, 10);
    if (Has(defender.orders, 0x22))
        m -= defender.weaponSkill / 4 + 5;
    else if (Has(defender.orders, 0x06))
        m = std::min(-(attacker.weaponSkill / 4), -10);
    int chance = (attacker.attack - defense) * 2 / 3 + 50 + m
        + 10 * helpers - 10 * threats;
    if (Has(attacker.orders, 0x06))
        chance = std::max(chance, 10);
    return chance;
}


// file 0x4456A
static void
RollDamage(int value, int armor, int penetration, std::mt19937& random,
    strike& blow)
{
    const int s = std::min(std::max(value, 0), 40);
    const int r = Die(random, 6) + Die(random, 6);
    int damage;
    if (s <= 7)
        damage = r / kDivisors[s] + kAdditions[s];
    else if (s <= 20)
        damage = r / 2 + s - 6;
    else
        damage = s + r - 10;
    blow.endurance = std::min(damage, kMaxEnduranceLoss);

    int times;
    if (armor < penetration)
        times = Die(random, 6) + 3;
    else if (armor == penetration)
        times = Die(random, 4) + 1;
    else
        times = int(random() % 3);
    blow.strength = times * damage / 10;
}


strike
Strike(const fighter& attacker, const fighter& defender, const ExeData& exe,
    int helpers, int threats, std::mt19937& random)
{
    strike blow = { STRIKE_NONE, HIT_VITALS, 0, 0 };
    const int roll = Die(random, 100);
    if (StrikeRate(attacker, exe) < roll)
        return blow;
    blow.location = (roll & 1) != 0 ? HIT_LIMBS : HIT_VITALS;

    const int chance = ChanceToHit(attacker, defender, exe, helpers, threats);
    const exe_weapon* w = WeaponOf(attacker, exe);
    if (roll <= chance - 10)
        blow.result = STRIKE_HIT;
    else if (roll <= chance)
        blow.result = STRIKE_WEAK_HIT;
    else if (roll <= 5 && w != NULL
        && attacker.weaponSkill - w->skill >= 2 * roll)
        blow.result = STRIKE_HIT;
    else
        blow.result = STRIKE_MISS;
    if (blow.result == STRIKE_MISS || w == NULL)
        return blow;

    // file 0x44418
    const int armor = defender.armor[blow.location];
    int penetration = w->penetration;
    if (Has(attacker.orders, 0x0A))
        penetration += Die(random, 4);
    if (blow.result == STRIKE_WEAK_HIT)
        penetration -= Die(random, 4);
    int value;
    if (armor < penetration)
        value = w->damage;
    else if (armor == penetration)
        value = w->damage / 2;
    else
        value = w->damage / 8;
    if (attacker.maxStrength > w->maxStrength)
        value += (attacker.maxStrength - w->maxStrength) / 5 + 1;
    else if (attacker.maxStrength < w->minStrength)
        value += (attacker.maxStrength - w->minStrength) / 5 - 1;
    const int quality = std::min(defender.armorQuality[blow.location], 99);
    value += (attacker.weaponQuality - quality) / 10;
    RollDamage(value, armor, penetration, random, blow);
    return blow;
}


void
TakeStrike(fighter& defender, const strike& blow)
{
    if (blow.result != STRIKE_HIT && blow.result != STRIKE_WEAK_HIT)
        return;
    defender.endurance -= blow.endurance;
    if (defender.endurance <= 0) {
        defender.endurance = 0;
        defender.status = FIGHTER_UNCONSCIOUS;
    }
    defender.strength -= blow.strength;
    if (defender.strength <= 0) {
        defender.strength = 0;
        defender.status = FIGHTER_DEAD;
    }
}


int
AfterBattle(party& members, const std::vector<fighter>& fighters)
{
    int dead = 0;
    for (size_t i = members.members.size(); i-- > 0;) {
        if (i >= fighters.size())
            continue;
        character& member = members.members[i];
        member.attributes[ATTRIBUTE_ENDURANCE]
            = uint8(std::max(fighters[i].endurance, 0));
        member.attributes[ATTRIBUTE_STRENGTH]
            = uint8(std::max(fighters[i].strength, 0));
        if (fighters[i].status != FIGHTER_DEAD)
            continue;
        RemoveMember(members, i);
        dead++;
    }
    if (members.leader >= int(members.members.size()))
        members.leader = 0;
    return dead;
}

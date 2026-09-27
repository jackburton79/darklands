/*
 * Combat.h
 * Melee combat, as DARKLAND.EXE resolves a strike (docs/exe.md,
 * "Battles"): how often a fighter strikes, the chance to hit, the hit
 * location, the damage through armor and what it does.
 *
 * Some terms of the game's formulas come from fields that are not
 * decoded yet (the fighters' orders, the load they carry, record fields
 * +0x23..+0x55): they are left out, as if 0.
 */
#pragma once

#include "SupportDefs.h"

#include <random>
#include <vector>

class ExeData;
struct character;
struct party;
struct enemy_type;
struct exe_weapon;

enum fighter_status {
    FIGHTER_ACTIVE = 1,
    FIGHTER_UNCONSCIOUS = 2,	// Endurance at 0 (inferred)
    FIGHTER_DEAD = 3			// Strength at 0 (inferred)
};

enum hit_location {
    HIT_VITALS = 0,
    HIT_LIMBS = 1
};

// What a combatant's record holds, as the game sets it up (file 0x446E0)
struct fighter {
    int status;					// fighter_status
    int endurance;
    int strength;
    int maxStrength;
    int agility;
    int weaponType;				// ExeData::Weapons(), -1: none
    int weaponSkill;
    int weaponQuality;
    int armor[2];				// strength, by hit_location
    int armorQuality[2];
    int shieldType;				// 95..97, 0: none
    int shieldQuality;
    int attack;					// PCMeleeAttack
};

fighter FighterFromCharacter(const character& member, const ExeData& exe);
// Enemies' qualities are the type's armor quality (inferred)
fighter FighterFromEnemy(const enemy_type& type, const ExeData& exe);

// PCMeleeAttack (file 0x446E0): the skill, less for a weapon too heavy or
// too hard for the fighter, more for a strong one
int MeleeAttack(const fighter& f, const exe_weapon& weapon);

// HitChance (file 0x43CA2), for a melee fighter: the chance in 100 that
// it strikes at a given moment
int StrikeRate(const fighter& f, const ExeData& exe);

// The chance to hit (file 0x44232); `helpers` fight the defender too,
// `threats` fight the attacker
int ChanceToHit(const fighter& attacker, const fighter& defender,
    const ExeData& exe, int helpers, int threats);

enum strike_result {
    STRIKE_NONE = 0,			// not this time
    STRIKE_MISS,
    STRIKE_HIT,
    STRIKE_WEAK_HIT
};

struct strike {
    strike_result result;
    hit_location location;
    int endurance;				// lost, at most 42
    int strength;
};

// One moment of a melee between the attacker and the defender: whether
// the attacker strikes, and the damage. Nothing is changed.
strike Strike(const fighter& attacker, const fighter& defender,
    const ExeData& exe, int helpers, int threats, std::mt19937& random);

// Takes a strike's damage; the defender may fall
void TakeStrike(fighter& defender, const strike& blow);

// After a battle (0E76:2278, file 0x59B9C): the members keep the
// Endurance and Strength they fought with (`fighters`, by member; an
// unconscious one stays at 0 Endurance, standing again), and the dead
// leave the party. Returns how many died; if none is left, the game is
// over.
int AfterBattle(party& members, const std::vector<fighter>& fighters);

/*
 * Combat.h
 * Melee combat, as DARKLAND.EXE resolves a strike (docs/exe.md,
 * "Battles"): how often a fighter strikes, the chance to hit, the hit
 * location, the damage through armor and what it does.
 *
 * Some terms of the game's formulas come from fields that are not
 * decoded yet (the load they carry, record fields +0x23..+0x55): they are
 * left out, as if 0. The fighters' orders (the Attack menu) are used.
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

// A fighter's way of fighting, the bits of its Orders word (record +0x10)
// that the strike's formulas test (file 0x43CA2, 0x44232, 0x44418): all
// have bit 0x02, "fighting". The values are the game's: the function at
// file 0x4E55E shows the order's key, 0x12 'A', 0x0A 'V', 0x06 'B', 0x22
// 'P' (also 0x80 'M', Use Missile, and 0x100 'T', Throw) **verified**
enum battle_stance {
    STANCE_STANDARD = 0x12,		// Std Attack
    STANCE_VULNERABLE = 0x0A,	// slower, deeper blows
    STANCE_BERSERK = 0x06,		// faster, wilder
    STANCE_PARRY = 0x22,		// hard to hit, less sure
    STANCE_MISSILE = 0x80		// Use Missile (M): shoots, does not fight
};

// What a combatant's record holds, as the game sets it up (file 0x446E0)
struct fighter {
    int orders;					// a battle_stance
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
    // The missile weapon (record +0x22, +0x5A, +0x21): its weapon type
    // (-1 none), quality, the skill it uses and PCMissileAttack
    int missileType;
    int missileQuality;
    int missileSkill;
    int missileAttack;
    // What it shoots: the item type (a thrown weapon is its own), how
    // many pieces it carries and how many it has shot in the battle
    int ammoType;
    int ammo;
    int shots;
};

fighter FighterFromCharacter(const character& member, const ExeData& exe);
// Enemies' qualities are the type's armor quality (inferred)
fighter FighterFromEnemy(const enemy_type& type, const ExeData& exe);

// PCMeleeAttack (file 0x446E0): the skill, less for a weapon too heavy or
// too hard for the fighter, more for a strong one
int MeleeAttack(const fighter& f, const exe_weapon& weapon);

// PCMissileAttack (file 0x446E0): S - weak - 2 * unskilled, 0..255
int MissileAttack(const fighter& f, const exe_weapon& weapon, int skill);

// Whether the fighter can shoot now: a missile weapon and a piece to shoot
bool CanShoot(const fighter& f);
// The range in cells. Provisional: the weapon table's range (file
// 0x77E3) is in units the program does not know; a quarter of it fits
// the arms (bows 9..13 cells, crossbows 33..41, guns 45..48)
int MissileRange(const fighter& f, const ExeData& exe);

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

// One shot at a defender `distance` cells away, nothing is changed but
// the shot's piece (ammo and shots of the attacker). Provisional: the
// game's rules for missiles are not decoded; the chance is the melee
// one with PCMissileAttack, less two for each cell, the damage the
// melee's with the missile weapon.
strike Shoot(fighter& attacker, const fighter& defender, const ExeData& exe,
    int distance, std::mt19937& random);

// Takes a strike's damage; the defender may fall
void TakeStrike(fighter& defender, const strike& blow);

// After a battle (0E76:2278, file 0x59B9C): the members keep the
// Endurance and Strength they fought with (`fighters`, by member; an
// unconscious one stays at 0 Endurance, standing again), and the dead
// leave the party. Returns how many died; if none is left, the game is
// over.
int AfterBattle(party& members, const std::vector<fighter>& fighters);

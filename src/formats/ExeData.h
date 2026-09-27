/*
 * ExeData.h
 * Tables of DARKLAND.EXE that are in no data file, read from the
 * executable: the names it gives the people the party meets (and its
 * name generator, 1367:0DB4), the jobs a member can earn money at, the
 * weapons' figures in battle. See docs/exe.md.
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

// A job, as the residence offers it (DS:3ACE, 18-byte records): the
// best paid one a member qualifies for
struct exe_job {
    std::string name;			// "Beggar", "Clerk"... (UTF-8)
    uint32 cityFlags;			// the city record's +0x5E must share a bit
    uint32 excluded;			// not 0: never offered (as the game does)
    uint8 locationFlags;		// the location's +0x14 must share a bit
    uint8 attribute;			// an attribute and its threshold
    uint8 attributeBase;
    uint8 skills[2];			// two skills and their thresholds
    uint8 skillBases[2];
    uint8 multiplier;
};

// The weapon categories, in the order of the weapon skills
enum weapon_category {
    WEAPON_EDGED = 0,
    WEAPON_IMPACT,
    WEAPON_FLAIL,
    WEAPON_POLEARM,
    WEAPON_THROWN,
    WEAPON_BOW,
    WEAPON_MISSILE_DEVICE
};

// A weapon type in battle (segment 20A5, one array per field): the item
// type of a weapon; 35.. are the monsters' natural weapons
struct exe_weapon {
    uint8 category;				// weapon_category
    std::string code;			// of its sprites: "SW" in E02CBSW.IMC
    uint8 speed;
    uint8 hands;				// 1 or 2
    uint8 penetration;
    uint8 damage;
    uint8 skill;				// the skill it needs
    uint8 minStrength;			// the strength it is used at best with
    uint8 maxStrength;
    uint8 range;				// missiles only
};

class ExeData {
public:
    explicit		ExeData(const std::string& exePath);	// throws on error

    // A man's name, "Albrecht Behaim", as the game makes it for `seed`
    // (the game's seed global plus a number for the person)
    std::string		MaleName(uint16 seed) const;

    const std::vector<std::string>& MaleNames() const	{ return fMale; }
    const std::vector<std::string>& FemaleNames() const	{ return fFemale; }
    const std::vector<std::string>& Surnames() const	{ return fSurnames; }
    const std::vector<exe_job>& Jobs() const	{ return fJobs; }
    const std::vector<exe_weapon>& Weapons() const	{ return fWeapons; }

private:
    std::vector<std::string>	fMale;		// UTF-8
    std::vector<std::string>	fFemale;
    std::vector<std::string>	fSurnames;
    std::vector<exe_job>		fJobs;
    std::vector<exe_weapon>		fWeapons;
};

// The Microsoft C runtime's rand(), as DARKLAND.EXE uses it
class MscRandom {
public:
    explicit		MscRandom(uint32 seed) : fSeed(seed) {}

    int				Next();				// 0..32767
    int				Below(int n);		// 0..n-1, the game's random(n)

private:
    uint32			fSeed;
};

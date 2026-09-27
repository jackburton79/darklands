/*
 * Character.h
 * A character of the party, as stored in CHARACTR.TMP and in the saved
 * games (554-byte records), and the party itself. See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <functional>
#include <string>
#include <vector>

enum character_attribute {
    ATTRIBUTE_ENDURANCE = 0,
    ATTRIBUTE_STRENGTH,
    ATTRIBUTE_AGILITY,
    ATTRIBUTE_PERCEPTION,
    ATTRIBUTE_INTELLIGENCE,
    ATTRIBUTE_CHARISMA,
    ATTRIBUTE_DIVINE_FAVOR,
    ATTRIBUTE_COUNT
};

static const int kSkillCount = 19;
// Skills the game's rules use, in the record's order (see docs/formats.md)
static const int kSkillReligion		= 8;
static const int kSkillVirtue		= 9;
static const int kSkillSpeakCommon	= 10;
static const int kSkillSpeakLatin	= 11;
static const int kSkillHealing		= 13;

// What a character has in use: item types (item_definition::type in
// DARKLAND.LST), 0xFF for none
enum equipment_slot {
    EQUIPMENT_WEAPON = 0,
    EQUIPMENT_VITALS,			// armor on the vitals (head, torso)
    EQUIPMENT_LIMBS,			// armor on the arms and legs
    EQUIPMENT_SHIELD,
    EQUIPMENT_MISSILE,			// missile weapon
    EQUIPMENT_COUNT
};
static const uint8 kNoEquipment = 0xFF;

struct item {
    uint16 code;
    uint8 type;
    uint8 quality;
    uint8 quantity;
    uint8 weight;
};

struct character {
    std::string fullName;		// UTF-8, e.g. "Gretchen Wilburg"
    std::string shortName;		// nickname, e.g. "Gretch"
    uint16 age;
    bool female;
    char heraldry;				// heraldic shield, 'A'..'O'
    uint8 attributes[ATTRIBUTE_COUNT];		// current values
    uint8 maxAttributes[ATTRIBUTE_COUNT];
    uint8 skills[kSkillCount];
    uint8 equipment[EQUIPMENT_COUNT];		// item types, see equipment_slot
    std::vector<item> items;
};

struct money {
    uint16 florins;
    uint16 groschen;
    uint16 pfennigs;
};

// The characters traveling together, in walking order.
struct party {
    std::vector<character> members;
    std::vector<std::string> images;	// per member, e.g. "F60": picks the
                                        // pictures (F60STAT.PIC...)
    int leader;							// index into members
    money cash;
    uint16 fame;
    uint16 bankNotes;					// letter of credit, in florins
    uint16 philosopherStone;			// quality, 0: none
};

// 1 florin = 20 groschen = 240 pfennigs; 1 groschen = 12 pfennigs
// (manual p. 21)
uint32 TotalPfennigs(const money& amount);
money MoneyFromPfennigs(uint32 pfennigs);
// "2 florins, 3 groschen and 1 pfennig", as the cards' $Money variables
// (DARKLAND.EXE 1367:0376)
std::string MoneyText(uint32 pfennigs);

// Adds to a character's current attribute, as DARKLAND.EXE does
// (0E76:0A72): the result stays within 1..99 and at most the maximum.
void AddToAttribute(character& member, int attribute, int amount);
// A chance to improve a skill by a point (DARKLAND.EXE 1462:0132 for
// one member, mode 1): if random(100) is at most amount · 33 / 10. The
// game's gain (0E76:18A8) is 0 or 1, raised to 1. Returns whether it
// improved.
bool TrainSkill(character& member, int skill, int amount,
    const std::function<int(int)>& random);
// What the passing of time does to the party (DARKLAND.EXE 0E76:255A,
// at every AddHours()): endurance comes back, and when a day begins
// divine favor, strength and some attributes too. `random(n)` is 0..n-1.
void PassTime(party& members, bool newDay,
    const std::function<int(int)>& random);

static const size_t kCharacterRecordSize = 554;	// 0x22A
static const int kMaxPartySize = 5;

// Decodes a 554-byte character record. Throws if it is inconsistent.
character ReadCharacter(const uint8* record);

// Builds a party from the party table of CHARACTR.TMP or a saved game:
// `indices` are kMaxPartySize little-endian words (0xFFFF: none),
// `images` kMaxPartySize 4-byte codes.
party MakeParty(const std::vector<character>& characters,
    const uint8* indices, const uint8* images, int leader);

/*
 * Character.h
 * A character of the party, as stored in CHARACTR.TMP and in the saved
 * games (554-byte records), and the party itself. See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

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

static const size_t kCharacterRecordSize = 554;	// 0x22A
static const int kMaxPartySize = 5;

// Decodes a 554-byte character record. Throws if it is inconsistent.
character ReadCharacter(const uint8* record);

// Builds a party from the party table of CHARACTR.TMP or a saved game:
// `indices` are kMaxPartySize little-endian words (0xFFFF: none),
// `images` kMaxPartySize 4-byte codes.
party MakeParty(const std::vector<character>& characters,
    const uint8* indices, const uint8* images, int leader);

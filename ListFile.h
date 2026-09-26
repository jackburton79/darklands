/*
 * ListFile.h
 * Reader for DARKLAND.LST, the general lists: the item definitions,
 * the saints and the alchemical formulae. See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

// Item categories: the bits of item_definition::flags (verified by the
// item names, see docs/formats.md)
enum item_flag {
    ITEM_EDGED			= 0x0001,	// flags byte 0
    ITEM_IMPACT			= 0x0002,
    ITEM_POLEARM		= 0x0004,
    ITEM_FLAIL			= 0x0008,
    ITEM_THROWN			= 0x0010,
    ITEM_BOW			= 0x0020,
    ITEM_METAL_ARMOR	= 0x0040,
    ITEM_SHIELD			= 0x0080,
    ITEM_COMPONENT		= 0x0400,	// flags byte 1
    ITEM_POTION			= 0x0800,
    ITEM_RELIC			= 0x1000,
    ITEM_HORSE			= 0x2000,
    ITEM_DOCUMENT		= 0x4000
};
// flags byte 2 and 3, as the upper 16 bits
static const uint32 ITEM_ARROW			= 0x00040000;
static const uint32 ITEM_QUARREL		= 0x00100000;
static const uint32 ITEM_BALL			= 0x00200000;
static const uint32 ITEM_SPECIAL		= 0x00800000;	// quest items
static const uint32 ITEM_ARMOR			= 0x04000000;	// non-metal armor
static const uint32 ITEM_MISSILE_DEVICE	= 0x08000000;	// crossbows, guns

struct item_definition {
    std::string name;			// UTF-8, e.g. "Hand Axe", "V:Plate Armor"
    std::string shortName;		// e.g. "Hnd Axe", "V:Plate"
    uint16 type;				// what the characters' equipment refers to
    uint32 flags;				// flags bytes 0..3, see item_flag
    uint8 weight;
    uint8 quality;				// default quality
    uint8 rarity;				// 0..12
    uint16 value;
};

class ListFile {
public:
    explicit		ListFile(const std::string& fileName);	// throws on error

    // Indexed by item code (the first word of a character's item).
    // Unused slots have an empty name.
    const std::vector<item_definition>& Items() const	{ return fItems; }
    const std::vector<std::string>& Saints() const		{ return fSaints; }
    const std::vector<std::string>& SaintShortNames() const
                                                    { return fSaintShortNames; }
    const std::vector<std::string>& Formulae() const	{ return fFormulae; }
    const std::vector<std::string>& FormulaShortNames() const
                                                    { return fFormulaShortNames; }

private:
    std::vector<item_definition>	fItems;
    std::vector<std::string>	fSaints;
    std::vector<std::string>	fSaintShortNames;
    std::vector<std::string>	fFormulae;
    std::vector<std::string>	fFormulaShortNames;
};

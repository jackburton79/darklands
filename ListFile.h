/*
 * ListFile.h
 * Reader for DARKLAND.LST, the general lists: the item definitions,
 * the saints and the alchemical formulae. See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

struct item_definition {
    std::string name;			// UTF-8, e.g. "Hand Axe", "V:Plate Armor"
    std::string shortName;		// e.g. "Hnd Axe", "V:Plate"
    uint16 type;				// what the characters' equipment refers to
    uint8 weight;
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

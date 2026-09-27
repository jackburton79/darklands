/*
 * EnemyFile.h
 * Reader for DARKLAND.ENM, the enemies: 71 enemy types (image, name,
 * attributes, skills) and 82 enemy names, each naming the first of a
 * group of types. See docs/formats.md.
 */
#pragma once

#include "Character.h"
#include "SupportDefs.h"

#include <string>
#include <vector>

// One enemy type ("Sergeant1".."Sergeant5" are five types of one group,
// its variants). Names are UTF-8.
struct enemy_type {
    std::string image;			// the sprite set: "E00", "M03"...
    std::string name;
    uint8 variants;				// in a group's first type: the group's
                                // type count; 0 in the others
    uint8 attributes[ATTRIBUTE_COUNT];
    uint8 skills[kSkillCount];
};

// One enemy name, as the game shows it ("Guard", "Raubritter"...)
struct enemy {
    uint16 type;				// the first type of its group
    std::string name;
    uint16 flags;				// not decoded
};

class EnemyFile {
public:
    explicit		EnemyFile(const std::string& fileName);	// throws on error

    uint32			CountTypes() const;
    const enemy_type& TypeAt(uint32 index) const;
    uint32			CountEnemies() const;
    const enemy&	EnemyAt(uint32 index) const;

private:
    std::vector<enemy_type> fTypes;
    std::vector<enemy>	fEnemies;
};

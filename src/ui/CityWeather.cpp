// The weather and the ground of the map: a blizzard, a peat bog and a
// flood. The states are 0x117, 0x116 and 0x11B of DARKLAND.EXE (see
// docs/exe.md, "Meetings on the map").

#include "CityVisitInternal.h"

#include "Character.h"
#include "Equipment.h"
#include "GameData.h"
#include "GameTime.h"
#include "ListFile.h"

#include <algorithm>

static int
Clamp(int value, int low, int high)
{
    return std::max(low, std::min(high, value));
}


// The average of an attribute over the party (0E76:1800)
static int
AverageAttribute(const party& members, int attribute)
{
    if (members.members.empty())
        return 0;
    int sum = 0;
    for (const character& member : members.members)
        sum += member.attributes[attribute];
    return sum / int(members.members.size());
}


// The best attribute of the party (0E76:16FE)
static int
BestAttribute(const party& members, int attribute)
{
    int best = 0;
    for (const character& member : members.members)
        best = std::max(best, int(member.attributes[attribute]));
    return best;
}


// The best skill of the party and who has it, the first (0E76:14A4); nobody
// over 0: the first member
static int
BestSkill(const party& members, int skill, int* who)
{
    int best = 0;
    *who = 0;
    for (size_t i = 0; i < members.members.size(); i++) {
        if (members.members[i].skills[skill] > best) {
            best = members.members[i].skills[skill];
            *who = int(i);
        }
    }
    return best;
}


// The average speed of the party (0E76:060E)
static int
AverageSpeed(party& members)
{
    if (members.members.empty())
        return 0;
    int sum = 0;
    for (character& member : members.members)
        sum += MemberSpeed(member);
    return sum / int(members.members.size());
}


// A blizzard (state 0x117, file 0x1470EA): keep going, a saint, camp
int
CityVisit::_MeetBlizzard()
{
    return SCREEN_BLIZZARD;
}


// Keep going (file 0x147322): random(4) + 2 days ($Number1); each member
// loses 1 Endurance and twice the days of Strength (not below 1); of the
// items the alchemical components (flags exactly 0x400) are ruined with a
// chance of 16 in 100 each and the cloth armor (0x04000000) loses a point
// of quality; card 1, the days pass
int
CityVisit::_BlizzardOnward()
{
    const int days = int(fRandom() % 4) + 2;
    fVariables["Number1"] = std::to_string(days);
    if (fParty == NULL)
        return SCREEN_BLIZZARD_ONWARD;
    const std::vector<item_definition>& definitions = fData.Lists().Items();
    for (character& member : fParty->members) {
        AddToAttribute(member, ATTRIBUTE_ENDURANCE, -1);
        AddToAttribute(member, ATTRIBUTE_STRENGTH, -2 * days);
        // the game goes on to the item after the next one when it loses one
        for (size_t i = 0; i < member.items.size(); i++) {
            item& carried = member.items[i];
            const size_t code = carried.code & 0x0FFF;
            const uint32 flags = code < definitions.size()
                ? definitions[code].flags : 0;
            if (flags == ITEM_COMPONENT) {
                if (int(fRandom() % 100) <= 15)
                    member.items.erase(member.items.begin() + i);
            } else if (flags == ITEM_ARMOR) {
                carried.quality = uint8(Clamp(int(carried.quality) - 1, 1, 99));
            }
        }
        ClearGoneEquipment(member);
    }
    if (fClock != NULL)
        fClock->AddHours(uint32(days * 24));
    return SCREEN_BLIZZARD_ONWARD;
}


// Camp (file 0x1475B4): the same days, each member loses 5 Endurance and
// random(3) Strength; the days and 12 hours pass; card 4
int
CityVisit::_BlizzardCamp()
{
    const int days = int(fRandom() % 4) + 2;
    fVariables["Number1"] = std::to_string(days);
    if (fParty != NULL) {
        for (character& member : fParty->members) {
            AddToAttribute(member, ATTRIBUTE_ENDURANCE, -5);
            AddToAttribute(member, ATTRIBUTE_STRENGTH, -int(fRandom() % 3));
        }
    }
    if (fClock != NULL)
        fClock->AddHours(uint32(days * 24 + 12));
    return SCREEN_BLIZZARD_CAMP;
}


// A peat bog (state 0x116, file 0x14649C): the slowest member (0E76:0656)
// falls in. Alone (card 5) only a saint can help. The rope needs something
// to make it of: a polearm, a bow or a metal armor in the party
// (0E76:0DD8(-2, 0x64))
int
CityVisit::_MeetBog()
{
    for (bool& failed : fBogFailed)
        failed = false;
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_BOG;
    fWeatherMember = _Slowest();
    _SetChosen(fWeatherMember);
    const std::vector<item_definition>& definitions = fData.Lists().Items();
    int material = 0;
    for (const character& member : fParty->members) {
        for (const item& carried : member.items) {
            const size_t code = carried.code & 0x0FFF;
            if (code < definitions.size() && (definitions[code].flags
                    & (ITEM_POLEARM | ITEM_BOW | ITEM_METAL_ARMOR)) != 0)
                material += carried.quantity;
        }
    }
    fBogFailed[0] = material == 0;
    if (fParty->members.size() > 1)
        return SCREEN_BOG;
    // nothing is offered to a lone member without a saint (the game shows
    // no option then: *inferred* that he drowns)
    return _SaintKnown(SCREEN_BOG_ALONE) ? SCREEN_BOG_ALONE : _BogDrown();
}


// The chances of the three ways (files 0x1467B0, 0x146874, 0x14691E): the
// party's average Agility + 3 (2, 3) times its average Strength, less the
// weight the one in the bog carries (a signed byte) for the first two
int
CityVisit::_BogChance(int way)
{
    if (fParty == NULL || fParty->members.empty())
        return 1;
    const int agility = AverageAttribute(*fParty, ATTRIBUTE_AGILITY);
    const int strength = AverageAttribute(*fParty, ATTRIBUTE_STRENGTH);
    const int weight = int(sint8(WeightInUse(fParty->members[
        size_t(fWeatherMember)]) & 0xFF));
    switch (way) {
        case 0:
            return Clamp(agility + 3 * strength - weight, 1, 99);
        case 1:
            return Clamp(agility + 2 * strength - weight, 1, 99);
        default:
            return Clamp(agility + 3 * strength, 1, 99);
    }
}


// A way to get him out (files 0x14672A, 0x1467FC, 0x1468BC): if
// random(100) is at most the chance, he is out an hour later and the items
// he carries are lost but for a part: the percent is Agility + 50 (rope
// and hands, at most 95), 5 for the possessions; cards 1, 4, 6. Else an
// hour passes and the way is closed: the rope also closes the hands (card
// 2, then the options again), the possessions all three
int
CityVisit::_BogPull(int way)
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_BOG;
    const int chance = _BogChance(way);
    const character& victim = fParty->members[size_t(fWeatherMember)];
    const bool freed = int(fRandom() % 100) <= chance;
    if (freed) {
        const int keep = way == 2 ? 5
            : Clamp(victim.attributes[ATTRIBUTE_AGILITY] + 50, 1, 95);
        _LoseItems(keep, fWeatherMember);
    }
    if (fClock != NULL)
        fClock->AddHours(1);
    if (freed)
        return way == 0 ? SCREEN_BOG_ROPE : way == 1 ? SCREEN_BOG_HAND
            : SCREEN_BOG_DUMPED;
    if (way == 0) {
        fBogFailed[0] = fBogFailed[1] = true;
        fMeetBack = SCREEN_BOG_AGAIN;
        return SCREEN_BOG_ROPED;
    }
    fBogFailed[1] = true;
    if (way == 2)
        fBogFailed[0] = fBogFailed[2] = true;
    return SCREEN_BOG_AGAIN;
}


// Leave him to his fate (file 0x146B4C): if the hands were tried he dies
// (card 7); else (card 8) his curse takes 99 Divine Favor and 2 of its
// maximum, 5 Charisma and 3 Strength and 1 of its maximum from everybody
int
CityVisit::_BogAbandon()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_BOG;
    const bool cursed = !fBogFailed[1];
    if (cursed) {
        for (character& member : fParty->members) {
            AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR, -99);
            AddToMaximum(member, ATTRIBUTE_DIVINE_FAVOR, -2);
            AddToAttribute(member, ATTRIBUTE_CHARISMA, -5);
            AddToAttribute(member, ATTRIBUTE_STRENGTH, -3);
            AddToMaximum(member, ATTRIBUTE_STRENGTH, -1);
        }
    }
    RemoveMember(*fParty, size_t(fWeatherMember));
    return cursed ? SCREEN_BOG_CURSED : SCREEN_BOG_DEAD;
}


// A lone member without a saint sinks
int
CityVisit::_BogDrown()
{
    if (fParty != NULL && !fParty->members.empty())
        RemoveMember(*fParty, size_t(fWeatherMember));
    return SCREEN_BOG_DEAD;
}


// A saint answered (file 0x14695A): the first three give card 13, the
// fourth card 14; an hour passes and the items he carries are lost but 90
// in a hundred
int
CityVisit::_BogSaint(int index)
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_BOG;
    _SetChosen(fWeatherMember);
    if (fClock != NULL)
        fClock->AddHours(1);
    _LoseItems(90, fWeatherMember);
    return index <= 2 ? SCREEN_BOG_MIRACLE : SCREEN_BOG_PRAYER;
}


// No answer: card 16, an hour, then the options of `menu` again
int
CityVisit::_BogUnheard(int menu)
{
    if (fParty != NULL && !fParty->members.empty())
        _SetChosen(fWeatherMember);
    if (fClock != NULL)
        fClock->AddHours(1);
    fMeetBack = menu;
    return SCREEN_BOG_UNHEARD;
}


// A flood (state 0x11B, file 0x149F5A): the member with the best Woodwise
// leads (0E76:14A4(18)); the raft needs a rope (0E76:0C76(-2, 59))
int
CityVisit::_MeetFlood()
{
    fFloodRope = false;
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_FLOOD;
    BestSkill(*fParty, kSkillWoodwise, &fWeatherMember);
    _SetChosen(fWeatherMember);
    for (const character& member : fParty->members) {
        for (const item& carried : member.items) {
            if ((carried.code & 0x0FFF) == kRopeCode)
                fFloodRope = true;
        }
    }
    return SCREEN_FLOOD;
}


// Higher ground (file 0x14A1C2): the average speed (0E76:060E) + the best
// Perception, 1..99. If random(100) is at most that, card 2; else card 1
// and each item is lost but for (the guide's Agility + 10, at most 95) in a
// hundred. 23 to 25 hours pass
int
CityVisit::_FloodSearch()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_FLOOD;
    const int chance = Clamp(AverageSpeed(*fParty)
        + BestAttribute(*fParty, ATTRIBUTE_PERCEPTION), 1, 99);
    int screen = SCREEN_FLOOD_SAFE;
    if (int(fRandom() % 100) > chance) {
        _LoseItems(Clamp(fParty->members[size_t(fWeatherMember)].attributes[
            ATTRIBUTE_AGILITY] + 10, 1, 95));
        screen = SCREEN_FLOOD_LOST;
    }
    if (fClock != NULL)
        fClock->AddHours(23 + fRandom() % 3);
    return screen;
}


// The raft (file 0x14A260): twice the best Woodwise + a third of the best
// Artifice, 1..99. Made (card 4), a lesson in Woodwise (10) and Artifice
// (5); not (card 3), the items lost but for the guide's Agility + 20 (at
// most 95) in a hundred and the same lessons by the lower chance (mode 0).
// 23 to 25 hours pass
int
CityVisit::_FloodRaft()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_FLOOD;
    int who = 0;
    const int chance = Clamp(2 * BestSkill(*fParty, kSkillWoodwise, &who)
        + BestSkill(*fParty, kSkillArtifice, &who) / 3, 1, 99);
    const bool made = int(fRandom() % 100) <= chance;
    if (!made) {
        _LoseItems(Clamp(fParty->members[size_t(fWeatherMember)].attributes[
            ATTRIBUTE_AGILITY] + 20, 1, 95));
    }
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    TrainParty(*fParty, kSkillWoodwise, made ? 1 : 0, 10, random);
    TrainParty(*fParty, kSkillArtifice, made ? 1 : 0, 5, random);
    if (fClock != NULL)
        fClock->AddHours(23 + fRandom() % 3);
    return made ? SCREEN_FLOOD_RAFT : SCREEN_FLOOD_RAFT_FAILED;
}

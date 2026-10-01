// Random encounters in the city: the slum, the thieves, the shell game, the
// grove

#include "CityVisitInternal.h"

#include "BattleMap.h"
#include "BattleView.h"
#include "Catalog.h"
#include "Character.h"
#include "CityFile.h"
#include "DescriptionFile.h"
#include "GameData.h"
#include "GameTime.h"
#include "InfoView.h"
#include "ExeData.h"
#include "ListFile.h"
#include "LocationFile.h"
#include "ScreenSupport.h"
#include "EnemyFile.h"
#include "Stream.h"
#include "TextSupport.h"

#include <stdexcept>


// Living very cheaply in the slum (file 0xAB49E): city size / 3 hours,
// a room (card 2) where the city's property 0x21 is odd, else a shanty
// (card 3), a lesson in Streetwise of mode 7 for all; then the camp
// (ACTION_SLUM_CAMP)
int
CityVisit::_SlumLodging()
{
    if (fClock != NULL)
        fClock->AddHours(uint32(_City().size / 3));
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (fParty != NULL)
        TrainParty(*fParty, kSkillStreetwise, 7, 10, random);
    return _PeopleSeed() % 2 != 0 ? SCREEN_SLUM_ROOM : SCREEN_SLUM_SHANTY;
}


// After the camp (file 0xAB4F8): the thieves came (mark 0x52): random(2)
// + 2 hours, card 7, then state 0x24, back to the slum after; else the
// slum (the alchemy's accident, mark 0x51, state 0xB5, is not
// reproduced)
int
CityVisit::_AfterSlumCamp()
{
    fSlumCamp = false;
    if (!fResidence.Interrupted())
        return SCREEN_SLUM;
    if (fClock != NULL)
        fClock->AddHours(fRandom() % 2 + 2);
    fThievesReturn = SCREEN_SLUM;
    return SCREEN_SLUM_DISTURBED;
}


// The thieves (state 0x24, file 0xAC140): an hour; the most perceptive
// member (0E76:16FE(3)) is $ChosenOneName. Unless an event of category
// 0x4F runs (0E76:32CE), if random(100) is over his Perception they
// strike first: in a city without a card, the fight. Else card 1.
int
CityVisit::_MeetThieves()
{
    if (fClock != NULL)
        fClock->AddHours(1);
    int best = 0;
    for (size_t i = 1; fParty != NULL && i < fParty->members.size(); i++) {
        if (fParty->members[i].attributes[ATTRIBUTE_PERCEPTION]
                > fParty->members[size_t(best)].attributes[ATTRIBUTE_PERCEPTION])
            best = int(i);
    }
    const int perception = fParty != NULL && !fParty->members.empty()
        ? fParty->members[size_t(best)].attributes[ATTRIBUTE_PERCEPTION] : 0;
    bool warned = false;
    for (size_t i = 0; fEvents != NULL && i < fEvents->size(); i++) {
        if ((*fEvents)[i].category == 0x4F && _EventRunning((*fEvents)[i]))
            warned = true;
    }
    if (!warned && int(fRandom() % 100) > perception) {
        // in a city the fight at once, on the map card 0 first
        if (fOnMap)
            return SCREEN_THIEVES_STRUCK;
        _FightThieves();
        return fScreen;
    }
    _SetChosen(best);
    return fOnMap ? SCREEN_THIEVES_MAP : SCREEN_THIEVES;
}


// Talking (file 0xAC822): the leader's Intelligence or Charisma, the
// higher, + (his Woodwise + Speak Common) / 2; the game reads Woodwise
// (skill 18) in a city and Streetwise on the map, the reverse of its
// lessons
int
CityVisit::_ThievesTalkChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[size_t(fParty->leader)];
    return std::max(leader.attributes[ATTRIBUTE_INTELLIGENCE],
            leader.attributes[ATTRIBUTE_CHARISMA])
        + (leader.skills[fOnMap ? kSkillStreetwise : kSkillWoodwise]
            + leader.skills[kSkillSpeakCommon]) / 2;
}


// File 0xAC722: if random(100) is at most the chance, lessons of mode 1
// in Streetwise and Speak Common for the leader, card 4, an hour; else
// card 5 and the fight
int
CityVisit::_TalkToThieves()
{
    if (fParty == NULL || fParty->members.empty())
        return fThievesReturn;
    if (int(fRandom() % 100) > _ThievesTalkChance())
        return SCREEN_THIEVES_UNCONVINCED;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    character& leader = fParty->members[size_t(fParty->leader)];
    TrainSkill(leader, kSkillStreetwise, 10, random);
    TrainSkill(leader, kSkillSpeakCommon, 10, random);
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_THIEVES_TALKED;
}


// The leader's best weapon skill (0E76:01C0(-1))
int
CityVisit::_ThievesScareChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[size_t(fParty->leader)];
    int best = 0;
    for (int s = 0; s < kWeaponSkillCount; s++)
        best = std::max(best, int(leader.skills[s]));
    return best;
}


// Showing off (file 0xAC88E): $NamedOneName the leader's weapon
// (09C0:1E9B, taken as the one in hand; without one, item 7, a dagger);
// if random(100) is at most the chance, a lesson in that skill for the
// leader, card 6; else card 7 and the fight
int
CityVisit::_ScareThieves()
{
    if (fParty == NULL || fParty->members.empty())
        return fThievesReturn;
    character& leader = fParty->members[size_t(fParty->leader)];
    const std::vector<item_definition>& items = fData.Lists().Items();
    const int weapon = leader.equipment[EQUIPMENT_WEAPON];
    std::string name = items.size() > size_t(kDaggerCode)
        ? items[size_t(kDaggerCode)].name : "";
    for (size_t code = 0; weapon != kNoEquipment && code < items.size();
            code++) {
        if (!items[code].name.empty() && items[code].type == weapon) {
            name = items[code].name;
            break;
        }
    }
    fVariables["NamedOneName"] = name;
    if (int(fRandom() % 100) > _ThievesScareChance())
        return SCREEN_THIEVES_UNIMPRESSED;
    int skill = 0;
    for (int s = 1; s < kWeaponSkillCount; s++) {
        if (leader.skills[s] > leader.skills[skill])
            skill = s;
    }
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    TrainSkill(leader, skill, 10, random);
    return SCREEN_THIEVES_SCARED;
}


// Running (file 0xACA66): on horseback, card 10; else if random(100) is
// at most the slowest member's speed + 5 (0E76:0656, Agility here), an
// hour, card 11; else an hour, card 12 and the fight
int
CityVisit::_RunFromThieves()
{
    if (_HasHorses())
        return SCREEN_THIEVES_OUTRIDDEN;
    if (fClock != NULL)
        fClock->AddHours(1);
    const int speed = fParty != NULL && !fParty->members.empty()
        ? fParty->members[size_t(_Slowest())].attributes[ATTRIBUTE_AGILITY] : 0;
    return int(fRandom() % 100) <= speed + 5 ? SCREEN_THIEVES_ELUDED
        : SCREEN_THIEVES_CAUGHT;
}


// The fight (file 0xAC416): enemy 7 (the bandits) at variant s / 4 + 1,
// clamp(party size, 8, random(s)) of them, s the party's strength
// (09C0:1C1B, PartyStrength()); the battlefield (by the street, and
// whether the thieves struck first) is not reproduced
void
CityVisit::_FightThieves()
{
    fFoes.clear();
    const int size = fParty != NULL ? int(fParty->members.size()) : 1;
    const int strength = fParty != NULL ? PartyStrength(*fParty) : 1;
    const int count = std::max(size, std::min(8, int(fRandom() % uint32(strength))));
    fFoes.push_back(foes{ 7, strength / 4 + 1, count });
    fBattleKind = BATTLE_WITH_THIEVES;
    fPendingBattle = true;
}


// Won (file 0xAC512): the reputation up by 1..5 (0E76:19D0), card 15, 17
// or 18 at random; fled, card 11 (0E76:251A before it, not decoded); lost, the reputation down by 1..2, the
// party robbed (card 16), two hours. Then back (DS:E7D8).
int
CityVisit::_ResolveThievesBattle(int outcome)
{
    if (outcome == BATTLE_WON) {
        _ChangeReputation(1, 5);
        if (fOnMap)
            return SCREEN_THIEVES_SLAIN;
        static const int kThanks[3] = { SCREEN_THIEVES_SLAIN,
            SCREEN_THIEVES_THANKED, SCREEN_THIEVES_BLESSED };
        return kThanks[fRandom() % 3];
    }
    if (outcome != BATTLE_LOST)
        return SCREEN_THIEVES_ELUDED;
    _ChangeReputation(-2, -1);
    _Robbed();
    if (fClock != NULL)
        fClock->AddHours(2);
    return SCREEN_THIEVES_LEFT_FOR_DEAD;
}


// Robbed (09C0:1EB9(-2), the search 18E7:0854), then a club each
// (09C0:2067(-2, 15))
void
CityVisit::_Robbed()
{
    _Search();
    _GiveEach(kClubCode);
}


// The bandits of the map (states 0x102, 0x103, files 0x137C26 and
// 0x138BD8; the same code for both, in MEETB01 / MEETB02). Like the
// thieves, they are met in ambush (card 1) unless a member's Perception
// (the best one, as above) warns the party (card 0): *inferred* from the
// thieves, the chance is not decoded.
int
CityVisit::_MeetBandits()
{
    fBanditsReturn = SCREEN_BANDITS_WARNING;
    int best = 0;
    for (size_t i = 1; fParty != NULL && i < fParty->members.size(); i++) {
        if (fParty->members[i].attributes[ATTRIBUTE_PERCEPTION]
                > fParty->members[size_t(best)].attributes[ATTRIBUTE_PERCEPTION])
            best = int(i);
    }
    const int perception = fParty != NULL && !fParty->members.empty()
        ? fParty->members[size_t(best)].attributes[ATTRIBUTE_PERCEPTION] : 0;
    _SetChosen(best);
    return int(fRandom() % 100) > perception ? SCREEN_BANDITS_AMBUSH
        : SCREEN_BANDITS_WARNING;
}


// Bluffing (*inferred*): if random(100) is at most the leader's Charisma
// + Speak Common, the bandits leave (card 2, a lesson in Speak Common,
// an hour); else card 3 and the fight
int
CityVisit::_BanditsTalk()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_BANDITS_UNHEARD;
    character& leader = fParty->members[size_t(fParty->leader)];
    const int chance = leader.attributes[ATTRIBUTE_CHARISMA]
        + leader.skills[kSkillSpeakCommon];
    if (int(fRandom() % 100) > chance)
        return SCREEN_BANDITS_UNHEARD;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    TrainSkill(leader, kSkillSpeakCommon, 10, random);
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_BANDITS_TALKED;
}


// Sneaking away (*inferred*): if random(100) is at most the party's
// average Stealth and Woodwise, card 12 and two hours; else the bandits
// notice (the charge, card 17)
int
CityVisit::_BanditsSneak()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_BANDITS_CHARGE;
    int total = 0;
    for (const character& member : fParty->members)
        total += (member.skills[kSkillStealth] + member.skills[kSkillWoodwise]) / 2;
    if (int(fRandom() % 100) > total / int(fParty->members.size()))
        return SCREEN_BANDITS_CHARGE;
    if (fClock != NULL)
        fClock->AddHours(2);
    return SCREEN_BANDITS_ELUDED;
}


// The fight (files 0x137F4A and 0x138EDC, 0E76:2278), s the party's
// strength (PartyStrength()). The bandits: enemy 7 at variant
// random(2) + s / 4 + 1, clamp(party size, 7, random(5) + s / 3 + 1) of
// them, and enemy 0x16 (a robber captain) if s is over 5, else 0x12 (a
// brigand sergeant) at variant s % 3 + 1. The soldiers: enemy 0xF (a
// mercenary) at variant random(3) + s / 4 + 1, clamp(size, 7, random(4)
// + s / 3 + 1) of them, and a captain, enemy 0x25 if s is over 6, else
// 0x16, at variant s / 4 + 1. **verified** (code)
void
CityVisit::_FightBandits()
{
    fFoes.clear();
    const int size = fParty != NULL ? int(fParty->members.size()) : 1;
    const int s = fParty != NULL ? PartyStrength(*fParty) : 1;
    if (fBanditsSoldiers) {
        const int variant = int(fRandom() % 3) + s / 4 + 1;
        const int count = std::max(size, std::min(7,
            int(fRandom() % 4) + s / 3 + 1));
        fFoes.push_back(foes{ 0xF, variant, count });
        fFoes.push_back(foes{ s > 6 ? 0x25 : 0x16, s / 4 + 1, 1 });
    } else {
        const int variant = int(fRandom() % 2) + s / 4 + 1;
        const int count = std::max(size, std::min(7,
            int(fRandom() % 5) + s / 3 + 1));
        fFoes.push_back(foes{ 7, variant, count });
        fFoes.push_back(foes{ s > 5 ? 0x16 : 0x12, s % 3 + 1, 1 });
    }
    fBattleKind = BATTLE_WITH_BANDITS;
    fPendingBattle = true;
}


// Won (file 0x137F80): the reputation of the nearest place up by 1..5
// (0E76:19D0(place, 0, 5)), card 11; fled, card 12; lost, the party
// robbed, the reputation down by 1..2 (*inferred*, as the thieves), two
// hours, card 13
int
CityVisit::_ResolveBanditsBattle(int outcome)
{
    if (outcome == BATTLE_WON) {
        _ChangeReputation(1, 5);
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_BANDITS_BEATEN;
    }
    if (outcome != BATTLE_LOST)
        return SCREEN_BANDITS_ELUDED;
    _ChangeReputation(-2, -1);
    _Robbed();
    if (fClock != NULL)
        fClock->AddHours(2);
    return SCREEN_BANDITS_LEFT_FOR_DEAD;
}


// The battlefield of a meeting on the map (the wilderness maps of
// IMAPS.CAT, IWILDGEN.1xx; which one the game picks is not decoded)
std::string
CityVisit::_WildMap()
{
    static const int kMaps[12] = { 101, 102, 103, 104, 105, 106, 111, 112,
        113, 114, 115, 116 };
    return "IWILDGEN." + std::to_string(kMaps[fRandom() % 12]);
}


// The city's feast near (0E76:1A8E(location, 0x23)): within 14 days of
// its day (DARKLAND.CTY +0x6C), counted from the first of this month
bool
CityVisit::_FeastNear() const
{
    if (!_InCity() || fClock == NULL)
        return false;
    static const int kMonthDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30,
        31, 30, 31 };
    int start = 0;
    for (int m = 0; m < int(fClock->Month()) && m < 12; m++)
        start += kMonthDays[m];
    return std::abs(int(_City().feastDay) - start) <= 14;
}


// The grove (states 0x21, 0x22). By day (file 0xAA50A): an hour, card
// 1; a bell, card 2; until nightfall, 1367:086A(18) + 1 hours, card 3,
// then card 4 with $Number1 the hours when they are 8 or more. At night
// (file 0xAAAEA): an hour, card 1; a bell, card 2; until morning, card 3,
// 1367:086A(5) hours, the reputation down by 1 (0E76:19D0(location, -1,
// -1)), card 5 with $Number1 the hours. Option 3: after the nap.
int
CityVisit::_Grove(int option)
{
    static const int kHoursUntil[2] = { 18, 5 };
    if (option == 3) {
        if (fGroveHours < 8)
            return SCREEN_GROVE;
        fVariables["Number1"] = std::to_string(fGroveHours);
        return SCREEN_GROVE_AWAKE;
    }
    if (option < 2) {
        if (fClock != NULL)
            fClock->AddHours(option == 0 ? 1 : 3);
        if (fNight)
            return option == 0 ? SCREEN_GROVE_MOONLIGHT : SCREEN_GROVE_DOZE;
        return option == 0 ? SCREEN_GROVE_HOUR : SCREEN_GROVE_BELL;
    }
    int hours = 0;
    if (fClock != NULL) {
        hours = (kHoursUntil[fNight ? 1 : 0] - int(fClock->Hour()) + 24) % 24;
        if (!fNight)
            hours++;
        fClock->AddHours(uint32(hours));
    }
    fGroveHours = hours;
    if (!fNight)
        return SCREEN_GROVE_NAP;
    _ChangeReputation(-1, -1);
    fVariables["Number1"] = std::to_string(hours);
    return SCREEN_GROVE_CAMP;
}


// A shell picked (file 0x110E92): the man lets a party win only once
// (DS:8E1C): with a chance of 1 in 3 for the right-hand shell, 1 in 4
// for the others, three groschen (card 7); else the pea was under one of
// the other two (cards 1..3)
int
CityVisit::_PlayShells(int shell)
{
    if (!fShellWon && int(fRandom() % uint32(shell == 0 ? 3 : 4)) == 1) {
        fShellWon = true;
        if (fParty != NULL)
            fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) + 36);
        return SCREEN_SHELL_WON;
    }
    const bool even = fRandom() % 2 == 0;
    switch (shell) {
        case 0:
            return even ? SCREEN_SHELL_LOST_LEFT : SCREEN_SHELL_LOST_MIDDLE;
        case 1:
            return even ? SCREEN_SHELL_LOST_LEFT : SCREEN_SHELL_LOST_RIGHT;
        default:
            return even ? SCREEN_SHELL_LOST_MIDDLE : SCREEN_SHELL_LOST_RIGHT;
    }
}

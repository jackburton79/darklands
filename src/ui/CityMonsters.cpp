// The monsters in ambush on the map: the tatzelwurms and the giant spiders.
// The states are 0x111 and 0x112 of DARKLAND.EXE (files 0x141E0E and
// 0x142FD6; see docs/exe.md, "Meetings on the map").

#include "CityVisitInternal.h"

#include "BattleView.h"
#include "Character.h"
#include "Equipment.h"
#include "GameData.h"
#include "GameTime.h"
#include "ListFile.h"

#include <algorithm>

static const int kSkillRiding		= 17;
static const int kOrpimentCode		= 79;	// "Orpiment": no surprise

enum { MONSTER_TATZELWURMS = 0, MONSTER_SPIDERS };


static int
Clamp(int value, int low, int high)
{
    return std::max(low, std::min(high, value));
}


// The best skill of the party and who has it, the first (0E76:14A4)
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


// The party's average of a skill (0E76:1600)
static int
AverageSkill(const party& members, int skill)
{
    if (members.members.empty())
        return 0;
    int sum = 0;
    for (const character& member : members.members)
        sum += member.skills[skill];
    return sum / int(members.members.size());
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


// The ambush (files 0x141E0E and 0x142FD6): the member with the best
// Woodwise (DS:EE8C) is on the watch. The party is warned (card 0) if
// random(100) is at most his Perception + random(10) + Woodwise / 2; else,
// unless the party carries Orpiment (item 79), it is ambushed (card 1)
int
CityVisit::_MeetAmbushers(int kind)
{
    fMonster = kind;
    fPrayerFailed = false;
    fMonsterSaintsFirst = false;
    const int warned = kind == MONSTER_TATZELWURMS ? SCREEN_TATZEL : SCREEN_SPIDERS;
    if (fParty == NULL || fParty->members.empty())
        return warned;
    int who;
    BestSkill(*fParty, kSkillWoodwise, &who);
    _SetChosen(who);
    const character& watcher = fParty->members[size_t(who)];
    const int chance = watcher.attributes[ATTRIBUTE_PERCEPTION]
        + int(fRandom() % 10) + watcher.skills[kSkillWoodwise] / 2;
    bool orpiment = false;
    for (const character& member : fParty->members) {
        for (const item& carried : member.items)
            orpiment = orpiment || (carried.code & 0x0FFF) == kOrpimentCode;
    }
    if (int(fRandom() % 100) > chance && !orpiment) {
        // the tatzelwurms' first menu keeps the saints of the warning
        fMonsterSaintsFirst = kind == MONSTER_TATZELWURMS;
        return kind == MONSTER_TATZELWURMS ? SCREEN_TATZEL_AMBUSH
            : SCREEN_SPIDERS_AMBUSH;
    }
    return warned;
}


// The ambush menu (card 1), the member on the watch speaking again
int
CityVisit::_AmbushScreen()
{
    fMonsterSaintsFirst = false;
    if (fParty != NULL && !fParty->members.empty()) {
        int who;
        BestSkill(*fParty, kSkillWoodwise, &who);
        _SetChosen(who);
    }
    return fMonster == MONSTER_TATZELWURMS ? SCREEN_TATZEL_AMBUSH
        : SCREEN_SPIDERS_AMBUSH;
}


// Surrender (files 0x1426BA, 0x1438CA): the tatzelwurms do not care: the
// ambush. A hail is not answered by the spiders (card 8), the options again
int
CityVisit::_AmbushSurrender()
{
    if (fMonster == MONSTER_TATZELWURMS)
        return _AmbushScreen();
    fMeetBack = SCREEN_SPIDERS;
    return SCREEN_SPIDERS_HAIL;
}


// Sneaking round them (files 0x14277C, 0x14393E): the tatzelwurms' option
// has no chance (its function gives -1, the roll is over it: a slip of the
// original); the spiders': the average Woodwise + two thirds of the average
// speed + 5, 10..85. Success: the time to hour 18, card 6, a lesson in
// Woodwise (mode 7); else an hour, a lesson (mode 0) and the ambush
int
CityVisit::_AmbushAround()
{
    if (fParty == NULL || fParty->members.empty())
        return _AmbushScreen();
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int chance = fMonster == MONSTER_TATZELWURMS ? -1
        : Clamp(AverageSkill(*fParty, kSkillWoodwise)
            + AverageSpeed(*fParty) * 2 / 3 + 5, 10, 85);
    if (int(fRandom() % 100) <= chance) {
        if (fClock != NULL)
            fClock->AddHours(uint32(_HoursUntil(*fClock, 18)));
        TrainParty(*fParty, kSkillWoodwise, 7, 10, random);
        return fMonster == MONSTER_TATZELWURMS ? SCREEN_TATZEL_AROUND
            : SCREEN_SPIDERS_AROUND;
    }
    if (fClock != NULL)
        fClock->AddHours(1);
    TrainParty(*fParty, kSkillWoodwise, 0, 10, random);
    return _AmbushScreen();
}


// Sneaking up to see (files 0x14290C, 0x1434DE): the tatzelwurms': the average
// Woodwise + two thirds of the average speed, 1..89; the spiders' option
// has no chance (-1, the same slip). Success: a lesson (mode 7) and card 2,
// the member with the best Woodwise speaking; else a lesson (mode 0) and the
// ambush
int
CityVisit::_AmbushUp()
{
    if (fParty == NULL || fParty->members.empty())
        return _AmbushScreen();
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int chance = fMonster == MONSTER_SPIDERS ? -1
        : Clamp(AverageSkill(*fParty, kSkillWoodwise)
            + AverageSpeed(*fParty) * 2 / 3, 1, 89);
    if (int(fRandom() % 100) <= chance) {
        TrainParty(*fParty, kSkillWoodwise, 7, 10, random);
        int who;
        BestSkill(*fParty, kSkillWoodwise, &who);
        _SetChosen(who);
        fMonsterSaintsFirst = false;
        return fMonster == MONSTER_TATZELWURMS ? SCREEN_TATZEL_HIDDEN
            : SCREEN_SPIDERS_HIDDEN;
    }
    TrainParty(*fParty, kSkillWoodwise, 0, 10, random);
    return _AmbushScreen();
}


// Running (files 0x142A5C, 0x14363C): with mounts the chance is twice the
// lowest Riding, 25..99; without, the tatzelwurms' is 0 (card 7, the fight)
// and the spiders' the number of the member on the watch (*inferred*: what
// 0E76:088E leaves in AX).
// The tatzelwurms: success, card 9 and a lesson in Riding (mode 1); else
// the member with the lowest Riding loses 4 Endurance and the one on the
// watch 2 Strength; with a roll of 50 or less the mounts are lost, two
// hours, card 8; else card 13 and the fight. Both with a lesson (mode 0).
// The spiders: success, a lesson (mode 7, with mounts), three hours, card
// 9; else a lesson (mode 0, with mounts), card 7 and the fight
int
CityVisit::_AmbushRun()
{
    if (fParty == NULL || fParty->members.empty())
        return _AmbushScreen();
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    int watch;
    BestSkill(*fParty, kSkillWoodwise, &watch);
    int lowest = 0;
    for (size_t i = 1; i < fParty->members.size(); i++) {
        if (fParty->members[i].skills[kSkillRiding]
                < fParty->members[size_t(lowest)].skills[kSkillRiding])
            lowest = int(i);
    }
    const bool mounts = _HasHorses();
    int chance = 0;
    if (mounts) {
        chance = Clamp(2 * int(fParty->members[size_t(lowest)]
            .skills[kSkillRiding]), 25, 99);
    } else if (fMonster == MONSTER_SPIDERS) {
        chance = watch;
    }
    const int roll = int(fRandom() % 100);
    if (fMonster == MONSTER_SPIDERS) {
        if (roll <= chance) {
            if (mounts)
                TrainParty(*fParty, kSkillRiding, 7, 10, random);
            if (fClock != NULL)
                fClock->AddHours(3);
            return SCREEN_SPIDERS_ESCAPED;
        }
        if (mounts)
            TrainParty(*fParty, kSkillRiding, 0, 10, random);
        return SCREEN_SPIDERS_HOPELESS;
    }
    if (chance == 0)
        return SCREEN_TATZEL_HOPELESS;
    if (roll <= chance) {
        TrainParty(*fParty, kSkillRiding, 1, 10, random);
        return SCREEN_TATZEL_ESCAPED;
    }
    AddToAttribute(fParty->members[size_t(lowest)], ATTRIBUTE_ENDURANCE, -4);
    AddToAttribute(fParty->members[size_t(watch)], ATTRIBUTE_STRENGTH, -2);
    TrainParty(*fParty, kSkillRiding, 0, 10, random);
    if (roll <= 50) {
        _DropItems(ITEM_HORSE);
        if (fClock != NULL)
            fClock->AddHours(2);
        return SCREEN_TATZEL_BOLTED;
    }
    return SCREEN_TATZEL_THROWN;
}


// The fight (files 0x142122, 0x143320), s the party's
// strength: the tatzelwurms, enemy 0x3B at variant 1, s / 3 + 1 of them; the
// spiders, enemy 0x39 at variant 1, random(3) + 3 · (s / 5) + 1. **verified**
void
CityVisit::_FightAmbushers()
{
    fFoes.clear();
    const int s = fParty != NULL ? PartyStrength(*fParty) : 1;
    if (fMonster == MONSTER_TATZELWURMS)
        fFoes.push_back(foes{ 0x3B, 1, s / 3 + 1 });
    else
        fFoes.push_back(foes{ 0x39, 1, int(fRandom() % 3) + 3 * (s / 5) + 1 });
    fBattleKind = BATTLE_WITH_AMBUSHERS;
    fPendingBattle = true;
}


// The fight's end (the game's results: 0 won, 1 and 2 a retreat, 3 and 4
// beaten; the program's BATTLE_LEFT and BATTLE_LOST): won, an hour, the
// reputation of the nearest place up by 1 (card 11); a retreat, the time to
// hour 5 (card 6, as sneaking round); beaten, the party is searched (the
// spiders) and the weakest member (the lowest Strength, 0E76:164A(1)) is
// eaten or dragged off, three to five hours (card 10). A sound party is the
// game's own 0E76:2434 afterwards: not decoded
int
CityVisit::_ResolveAmbushBattle(int outcome)
{
    const bool tatzel = fMonster == MONSTER_TATZELWURMS;
    if (outcome == BATTLE_WON) {
        if (fClock != NULL)
            fClock->AddHours(1);
        _ChangeReputation(1, 1);
        return tatzel ? SCREEN_TATZEL_WON : SCREEN_SPIDERS_WON;
    }
    if (outcome != BATTLE_LOST) {
        if (fClock != NULL)
            fClock->AddHours(uint32(_HoursUntil(*fClock, 5)));
        return tatzel ? SCREEN_TATZEL_AROUND : SCREEN_SPIDERS_AROUND;
    }
    if (!tatzel)
        _Search();
    if (fParty != NULL && !fParty->members.empty()) {
        int weakest = 0;
        for (size_t i = 1; i < fParty->members.size(); i++) {
            if (fParty->members[i].attributes[ATTRIBUTE_STRENGTH]
                    < fParty->members[size_t(weakest)].attributes[ATTRIBUTE_STRENGTH])
                weakest = int(i);
        }
        _SetChosen(weakest);
        RemoveMember(*fParty, size_t(weakest));
    }
    if (fClock != NULL)
        fClock->AddHours(3 + fRandom() % 3);
    return tatzel ? SCREEN_TATZEL_EATEN : SCREEN_SPIDERS_TAKEN;
}

// The monsters in ambush on the map: the tatzelwurms, the giant spiders and
// the schrats. The states are 0x111, 0x112 and 0x113 of DARKLAND.EXE (files
// 0x141E0E, 0x142FD6 and 0x1440DA; see docs/exe.md, "Meetings on the map").

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

enum { MONSTER_TATZELWURMS = 0, MONSTER_SPIDERS, MONSTER_SCHRATS };

// The screens of each monster
enum { SCR_WARNED = 0, SCR_AMBUSH, SCR_HIDDEN, SCR_AROUND, SCR_ESCAPED, SCR_WON,
    SCR_BEATEN, SCR_HOPELESS, SCR_COUNT };
static const int kScreens[3][SCR_COUNT] = {
    { CityVisit::SCREEN_TATZEL, CityVisit::SCREEN_TATZEL_AMBUSH,
        CityVisit::SCREEN_TATZEL_HIDDEN, CityVisit::SCREEN_TATZEL_AROUND,
        CityVisit::SCREEN_TATZEL_ESCAPED, CityVisit::SCREEN_TATZEL_WON,
        CityVisit::SCREEN_TATZEL_EATEN, CityVisit::SCREEN_TATZEL_HOPELESS },
    { CityVisit::SCREEN_SPIDERS, CityVisit::SCREEN_SPIDERS_AMBUSH,
        CityVisit::SCREEN_SPIDERS_HIDDEN, CityVisit::SCREEN_SPIDERS_AROUND,
        CityVisit::SCREEN_SPIDERS_ESCAPED, CityVisit::SCREEN_SPIDERS_WON,
        CityVisit::SCREEN_SPIDERS_TAKEN, CityVisit::SCREEN_SPIDERS_HOPELESS },
    { CityVisit::SCREEN_SCHRATS, CityVisit::SCREEN_SCHRATS_AMBUSH,
        CityVisit::SCREEN_SCHRATS_HIDDEN, CityVisit::SCREEN_SCHRATS_AROUND,
        CityVisit::SCREEN_SCHRATS_ESCAPED, CityVisit::SCREEN_SCHRATS_WON,
        CityVisit::SCREEN_SCHRATS_BEATEN, CityVisit::SCREEN_SCHRATS_WINDED }
};


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


// The ambush (files 0x141E0E, 0x142FD6, 0x1440DA): the member with the best
// Woodwise (DS:EE8C) is on the watch. The party is warned (card 0) if
// random(100) is at most his Perception + random(10) (the schrats: random(20))
// + Woodwise / 2; else, unless the party carries Orpiment (item 79, not the
// schrats' check), it is ambushed (card 1)
int
CityVisit::_MeetAmbushers(int kind)
{
    fMonster = kind;
    fPrayerFailed = false;
    fMonsterSaintsFirst = false;
    fPleadOffered = false;
    if (fParty == NULL || fParty->members.empty())
        return kScreens[kind][SCR_WARNED];
    int who;
    BestSkill(*fParty, kSkillWoodwise, &who);
    _SetChosen(who);
    const character& watcher = fParty->members[size_t(who)];
    const int chance = watcher.attributes[ATTRIBUTE_PERCEPTION]
        + int(fRandom() % (kind == MONSTER_SCHRATS ? 20 : 10))
        + watcher.skills[kSkillWoodwise] / 2;
    bool orpiment = false;
    for (const character& member : fParty->members) {
        for (const item& carried : member.items)
            orpiment = orpiment || (carried.code & 0x0FFF) == kOrpimentCode;
    }
    if (int(fRandom() % 100) > chance && (!orpiment || kind == MONSTER_SCHRATS)) {
        // the tatzelwurms' first menu keeps the saints of the warning
        fMonsterSaintsFirst = kind == MONSTER_TATZELWURMS;
        return kScreens[kind][SCR_AMBUSH];
    }
    return kScreens[kind][SCR_WARNED];
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
    return kScreens[fMonster][SCR_AMBUSH];
}


// The member the schrats want (file 0x144852, the plea's function): the
// woman standing with the best Charisma (0E76:007C, 174A(5)), else the
// member with the best Charisma (0E76:16FE(5)). Sets $ChosenOneName and
// gives the plea's chance: four times her Charisma; otherwise (Speak Common
// + Charisma / 2) / 3; 1..99
static int
Desired(party& members, int woman, int* chance)
{
    int who = woman;
    if (woman >= 0) {
        *chance = 4 * members.members[size_t(woman)].attributes[ATTRIBUTE_CHARISMA];
    } else {
        who = 0;
        for (size_t i = 1; i < members.members.size(); i++) {
            if (members.members[i].attributes[ATTRIBUTE_CHARISMA]
                    > members.members[size_t(who)].attributes[ATTRIBUTE_CHARISMA])
                who = int(i);
        }
        const character& best = members.members[size_t(who)];
        *chance = (best.skills[kSkillSpeakCommon]
            + best.attributes[ATTRIBUTE_CHARISMA] / 2) / 3;
    }
    *chance = Clamp(*chance, 1, 99);
    return who;
}


// Pleading, or bluffing (the schrats, file 0x14471E): with the chance of
// the member they want. Success: card 13, a menu (they want him: a potion,
// a saint, the fight, running); else, after the bluff, the ambush; after a
// plea, the charge (card 8) and the fight
int
CityVisit::_AmbushPlead()
{
    if (fParty == NULL || fParty->members.empty())
        return _AmbushScreen();
    int chance;
    const int who = Desired(*fParty, _Seductress(), &chance);
    _SetChosen(who);
    if (int(fRandom() % 100) <= chance)
        return SCREEN_SCHRATS_DEMAND;
    if (fScreen == SCREEN_SCHRATS)
        return _AmbushScreen();
    return SCREEN_SCHRATS_CHARGE;
}


// Surrender (files 0x1426BA, 0x1438CA, 0x144BD0): the tatzelwurms do not
// care: the ambush. A hail is not answered by the spiders (card 8), the
// options again. The schrats: on card 0 they want a member (card 13); else
// everybody is beaten to Endurance 1 and Strength random(5) + 1
// (0E76:0988 sets an attribute, within its maximum), searched, card 10
int
CityVisit::_AmbushSurrender()
{
    if (fMonster == MONSTER_SPIDERS) {
        fMeetBack = SCREEN_SPIDERS;
        return SCREEN_SPIDERS_HAIL;
    }
    if (fMonster == MONSTER_TATZELWURMS)
        return _AmbushScreen();
    if (fScreen == SCREEN_SCHRATS) {
        if (fParty != NULL && !fParty->members.empty()) {
            int chance;
            _SetChosen(Desired(*fParty, _Seductress(), &chance));
        }
        return SCREEN_SCHRATS_DEMAND;
    }
    if (fParty != NULL) {
        for (character& member : fParty->members) {
            member.attributes[ATTRIBUTE_ENDURANCE] = uint8(std::min(1,
                int(member.maxAttributes[ATTRIBUTE_ENDURANCE])));
            member.attributes[ATTRIBUTE_STRENGTH] = uint8(std::min(
                int(fRandom() % 5) + 1, int(member.maxAttributes[ATTRIBUTE_STRENGTH])));
        }
    }
    _Search();
    return SCREEN_SCHRATS_BEATEN;
}


// Sneaking round them (files 0x14277C, 0x14393E, 0x144CF6): the
// tatzelwurms' option has no chance (its function gives -1, the roll is over
// it: a slip of the original); the spiders': the average Woodwise + two
// thirds of the average speed + 5, 10..85; the schrats': the same without
// the 5, 5..85. Success: the time to hour 18, card 6, a lesson in Woodwise
// (mode 7); else an hour, a lesson (mode 0) and the ambush
int
CityVisit::_AmbushAround()
{
    if (fParty == NULL || fParty->members.empty())
        return _AmbushScreen();
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int walk = AverageSkill(*fParty, kSkillWoodwise)
        + AverageSpeed(*fParty) * 2 / 3;
    const int chance = fMonster == MONSTER_TATZELWURMS ? -1
        : fMonster == MONSTER_SPIDERS ? Clamp(walk + 5, 10, 85)
        : Clamp(walk, 5, 85);
    if (int(fRandom() % 100) <= chance) {
        if (fClock != NULL)
            fClock->AddHours(uint32(_HoursUntil(*fClock, 18)));
        TrainParty(*fParty, kSkillWoodwise, 7, 10, random);
        return kScreens[fMonster][SCR_AROUND];
    }
    if (fClock != NULL)
        fClock->AddHours(1);
    TrainParty(*fParty, kSkillWoodwise, 0, 10, random);
    return _AmbushScreen();
}


// Sneaking up to see (files 0x14290C, 0x1434DE, 0x144E7E): the tatzelwurms':
// the average Woodwise + two thirds of the average speed, 1..89; the
// spiders' option has no chance (-1, the same slip); the schrats': 5..85.
// Success: a lesson (mode 7) and card 2, the member with the best Woodwise
// speaking; else a lesson (mode 0) and the ambush
int
CityVisit::_AmbushUp()
{
    if (fParty == NULL || fParty->members.empty())
        return _AmbushScreen();
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int walk = AverageSkill(*fParty, kSkillWoodwise)
        + AverageSpeed(*fParty) * 2 / 3;
    const int chance = fMonster == MONSTER_SPIDERS ? -1
        : fMonster == MONSTER_TATZELWURMS ? Clamp(walk, 1, 89)
        : Clamp(walk, 5, 85);
    if (int(fRandom() % 100) <= chance) {
        TrainParty(*fParty, kSkillWoodwise, 7, 10, random);
        int who;
        BestSkill(*fParty, kSkillWoodwise, &who);
        _SetChosen(who);
        fMonsterSaintsFirst = false;
        return kScreens[fMonster][SCR_HIDDEN];
    }
    TrainParty(*fParty, kSkillWoodwise, 0, 10, random);
    return _AmbushScreen();
}


// Running (files 0x142A5C, 0x14363C, 0x144FD2): with mounts the chance is
// twice the lowest Riding, 25..99 (the schrats: 95); without, the
// tatzelwurms' is 0 (card 7, the fight), the spiders' the number of the
// member on the watch (*inferred*: what 0E76:088E leaves in AX), the
// schrats' the speed of the slowest member, 10..95.
// The tatzelwurms: success, card 9 and a lesson in Riding (mode 1); else
// the member with the lowest Riding loses 4 Endurance and the one on the
// watch 2 Strength; with a roll of 50 or less the mounts are lost
// (09C0:202B(-2, 0x2000)), two hours, card 8; else card 13 and the fight.
// Both with a lesson (mode 0).
// The spiders and the schrats: success, a lesson (mode 7, with mounts),
// three hours, card 9; else a lesson (mode 0, with mounts) and card 7: the
// spiders' fight, the schrats' menu (the plea, half of the time)
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
    if (fMonster == MONSTER_SCHRATS) {
        chance = mounts ? 95 : Clamp(MemberSpeed(fParty->members[size_t(_Slowest())]),
            10, 95);
    } else if (mounts) {
        chance = Clamp(2 * int(fParty->members[size_t(lowest)]
            .skills[kSkillRiding]), 25, 99);
    } else if (fMonster == MONSTER_SPIDERS) {
        chance = watch;
    }
    const int roll = int(fRandom() % 100);
    if (fMonster != MONSTER_TATZELWURMS) {
        if (roll <= chance) {
            if (mounts)
                TrainParty(*fParty, kSkillRiding, 7, 10, random);
            if (fClock != NULL)
                fClock->AddHours(3);
            return kScreens[fMonster][SCR_ESCAPED];
        }
        if (mounts)
            TrainParty(*fParty, kSkillRiding, 0, 10, random);
        fPleadOffered = fRandom() % 2 != 0;
        return kScreens[fMonster][SCR_HOPELESS];
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


// The fight (files 0x142122, 0x143320, 0x1443E6), s the party's strength:
// the tatzelwurms, enemy 0x3B at variant 1, s / 3 + 1 of them; the
// spiders, enemy 0x39 at variant 1, random(3) + 3 · (s / 5) + 1; the
// schrats, enemy 0x3E at variant random(3) + s / 4 + 1, random(5) + 3 of
// them. **verified**
void
CityVisit::_FightAmbushers()
{
    fFoes.clear();
    const int s = fParty != NULL ? PartyStrength(*fParty) : 1;
    if (fMonster == MONSTER_TATZELWURMS)
        fFoes.push_back(foes{ 0x3B, 1, s / 3 + 1 });
    else if (fMonster == MONSTER_SPIDERS)
        fFoes.push_back(foes{ 0x39, 1, int(fRandom() % 3) + 3 * (s / 5) + 1 });
    else
        fFoes.push_back(foes{ 0x3E, int(fRandom() % 3) + s / 4 + 1,
            int(fRandom() % 5) + 3 });
    fBattleKind = BATTLE_WITH_AMBUSHERS;
    fPendingBattle = true;
}


// The fight's end (the game's results: 0 won, 1 and 2 a retreat, 3 and 4
// beaten; the program's BATTLE_LEFT and BATTLE_LOST): won, an hour, the
// reputation of the nearest place up by 1 (card 11); a retreat, the time to
// hour 5 (card 6, as sneaking round; the schrats' card 18 if members had
// fallen: they come back); beaten, the tatzelwurms and the spiders (the
// latter searched) lose the weakest member (the lowest Strength,
// 0E76:164A(1)), eaten or dragged off, three to five hours (card 10). The
// schrats pound everybody but the woman they want (Endurance and Strength
// set to 1, each stack of items kept two times in ten) and search her (card
// 10), the time to hour 18; without a woman card 18
int
CityVisit::_ResolveAmbushBattle(int outcome)
{
    if (outcome == BATTLE_WON) {
        if (fClock != NULL)
            fClock->AddHours(1);
        _ChangeReputation(1, 1);
        return kScreens[fMonster][SCR_WON];
    }
    if (outcome != BATTLE_LOST) {
        if (fClock != NULL)
            fClock->AddHours(uint32(_HoursUntil(*fClock, 5)));
        return fMonster == MONSTER_SCHRATS && fFallen > 0
            ? SCREEN_SCHRATS_RECOVERED : kScreens[fMonster][SCR_AROUND];
    }
    if (fMonster == MONSTER_SCHRATS) {
        const int woman = fParty != NULL ? _Seductress() : -1;
        for (size_t i = 0; fParty != NULL && i < fParty->members.size(); i++) {
            if (int(i) == woman)
                continue;
            character& member = fParty->members[i];
            member.attributes[ATTRIBUTE_ENDURANCE] = uint8(std::min(1,
                int(member.maxAttributes[ATTRIBUTE_ENDURANCE])));
            member.attributes[ATTRIBUTE_STRENGTH] = uint8(std::min(1,
                int(member.maxAttributes[ATTRIBUTE_STRENGTH])));
            _LoseItems(20, int(i));
        }
        if (fClock != NULL)
            fClock->AddHours(uint32(_HoursUntil(*fClock, 18)));
        if (woman < 0)
            return SCREEN_SCHRATS_RECOVERED;
        _SetChosen(woman);
        _Search(woman);
        return SCREEN_SCHRATS_BEATEN;
    }
    if (fMonster == MONSTER_SPIDERS)
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
    return kScreens[fMonster][SCR_BEATEN];
}

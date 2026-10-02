// The animals of the map: a pack of wolves and a sounder of wild boars. The
// states are 0xA0 and 0xA5 of DARKLAND.EXE (see docs/exe.md, "Meetings on
// the map").

#include "CityVisitInternal.h"

#include "BattleView.h"
#include "Character.h"
#include "Equipment.h"
#include "GameData.h"
#include "GameTime.h"
#include "ListFile.h"

#include <algorithm>

static const int kSkillRiding		= 17;
static const int kTuskCode			= 146;	// "Tusk of a Boar"


static int
Clamp(int value, int low, int high)
{
    return std::max(low, std::min(high, value));
}


// A wound (09C0:2143 = 1462:026A of overlay 0x27, file 0x80C0A): the
// member loses Strength, then Endurance. The strength lost is random(S ·
// amount / 40 + 1) - 1, at least `minStrength` and at most its maximum;
// the endurance lost is random(E · amount / 20 + 1) - 1, at least the
// strength lost and `minEndurance`, at most its maximum. -2: everybody.
void
CityVisit::_Wound(int member, int minStrength, int minEndurance, int amount)
{
    if (fParty == NULL)
        return;
    for (size_t m = 0; m < fParty->members.size(); m++) {
        if (member >= 0 && int(m) != member)
            continue;
        character& c = fParty->members[m];
        const int strength = c.attributes[ATTRIBUTE_STRENGTH];
        int lost = int(fRandom() % uint32(strength * amount / 40 + 1)) - 1;
        const int strengthLost = std::min(int(c.maxAttributes[ATTRIBUTE_STRENGTH]),
            std::max(lost, minStrength));
        AddToAttribute(c, ATTRIBUTE_STRENGTH, -strengthLost);
        const int endurance = c.attributes[ATTRIBUTE_ENDURANCE];
        lost = int(fRandom() % uint32(endurance * amount / 20 + 1)) - 1;
        AddToAttribute(c, ATTRIBUTE_ENDURANCE, -std::min(
            int(c.maxAttributes[ATTRIBUTE_ENDURANCE]),
            std::max(std::max(lost, strengthLost), minEndurance)));
    }
}


// Wolves (state 0xA0, file 0x1083A0): the member with the best Woodwise
// speaks (DS:EE8C), card 0 by day and 1 at night
int
CityVisit::_MeetWolves()
{
    if (fParty != NULL && !fParty->members.empty()) {
        int best = 0;
        for (size_t i = 1; i < fParty->members.size(); i++) {
            if (fParty->members[i].skills[kSkillWoodwise]
                    > fParty->members[size_t(best)].skills[kSkillWoodwise])
                best = int(i);
        }
        _SetChosen(best);
    }
    return SCREEN_WOLVES;
}


// Woods lore (file 0x87DC): the best Woodwise + 30 by day, at most 80 at
// night, 1..99. If random(100) is at most that: by day card 2, at night
// three hours and card 3; else the fight (card 6)
int
CityVisit::_WolvesLore()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_WOLVES;
    const bool night = fClock != NULL && fClock->IsNight();
    int chance = 0;
    for (const character& member : fParty->members)
        chance = std::max(chance, int(member.skills[kSkillWoodwise]));
    chance += night ? 0 : 30;
    if (night && chance > 80)
        chance = 80;
    chance = Clamp(chance, 1, 99);
    if (int(fRandom() % 100) > chance)
        return SCREEN_WOLVES_FIGHT;
    if (!night)
        return SCREEN_WOLVES_SHOUTED;
    if (fClock != NULL)
        fClock->AddHours(3);
    return SCREEN_WOLVES_FIRE;
}


// A ride (file 0x88D6): the lowest Riding (0E76:1396) + 18E7:171C(member,
// 0, 0x200), which finds nothing in the list (no item has the flag
// 0x02000000): so the lowest Riding, 1..99. Success: three hours, card 4, a
// lesson in Riding (mode 7, 10); else two hours and card 5, then the fight
int
CityVisit::_WolvesRide()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_WOLVES;
    int lowest = 0x7FFF;
    for (const character& member : fParty->members)
        lowest = std::min(lowest, int(member.skills[kSkillRiding]));
    if (int(fRandom() % 100) <= Clamp(lowest, 1, 99)) {
        if (fClock != NULL)
            fClock->AddHours(3);
        const std::function<int(int)> random
            = [this](int n) { return int(fRandom() % uint32(n)); };
        TrainParty(*fParty, kSkillRiding, 7, 10, random);
        return SCREEN_WOLVES_OUTRAN;
    }
    if (fClock != NULL)
        fClock->AddHours(2);
    return SCREEN_WOLVES_CAUGHT;
}


// The fight (file 0x108592, field 0x2F): enemy 42 at variant s / 4 + 1,
// random(5) + 3 of them, s the party's strength
int
CityVisit::_WolvesBattle()
{
    fFoes.clear();
    const int s = fParty != NULL ? PartyStrength(*fParty) : 1;
    fFoes.push_back(foes{ 42, s / 4 + 1, int(fRandom() % 5) + 3 });
    fBattleKind = BATTLE_WITH_WOLVES;
    fPendingBattle = true;
    return fScreen;
}


// The fight's end: won, an hour, card 10; fled, two hours and card 11, the
// wolves follow (the options again); beaten, a member picked at random is
// eaten (card 12 if everybody had a mount, else 13), the horses are lost
int
CityVisit::_ResolveWolvesBattle(int outcome)
{
    if (outcome == BATTLE_WON) {
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_WOLVES_WON;
    }
    if (outcome != BATTLE_LOST) {
        if (fClock != NULL)
            fClock->AddHours(2);
        fMeetBack = SCREEN_WOLVES;
        return SCREEN_WOLVES_PURSUED;
    }
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_WOLVES_EATEN;
    const int victim = int(fRandom() % fParty->members.size());
    _SetChosen(victim);
    const bool mounted = _AllMounted();
    _DropItems(ITEM_HORSE);
    RemoveMember(*fParty, size_t(victim));
    return mounted ? SCREEN_WOLVES_EATEN_MOUNTED : SCREEN_WOLVES_EATEN;
}


// Wild boars (state 0xA5, file 0x10B390)
int
CityVisit::_MeetBoars()
{
    return SCREEN_BOARS;
}


// Dodging (file 0x10B638): each member is hit, and wounded (1, 1, 10), if
// random(100) is over twice his Agility + 5. Nobody (card 9) or one (card
// 10) hit: the boars go on; else card 11 and the fight
int
CityVisit::_BoarsDodge()
{
    if (fParty == NULL)
        return SCREEN_BOARS;
    int hits = 0;
    for (size_t m = 0; m < fParty->members.size(); m++) {
        const int agility = fParty->members[m].attributes[ATTRIBUTE_AGILITY];
        if ((agility + 5) * 2 < int(fRandom() % 100)) {
            _Wound(int(m), 1, 1, 10);
            hits++;
        }
    }
    if (hits == 0)
        return SCREEN_BOARS_NONE;
    return hits == 1 ? SCREEN_BOARS_ONE : SCREEN_BOARS_SEVERAL;
}


// A ride (file 0x10B7E4): the game computes clamp(1, 99, 0), a slip of the
// original (the best Riding is asked for and not used): 1 in 100. Success:
// three hours, card 1, a lesson in Riding (mode 7, 10); else two hours, a
// lesson (mode 0), card 2 and the fight
int
CityVisit::_BoarsRide()
{
    if (fParty == NULL)
        return SCREEN_BOARS;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (int(fRandom() % 100) <= 1) {
        if (fClock != NULL)
            fClock->AddHours(3);
        TrainParty(*fParty, kSkillRiding, 7, 10, random);
        return SCREEN_BOARS_OUTRAN;
    }
    if (fClock != NULL)
        fClock->AddHours(2);
    TrainParty(*fParty, kSkillRiding, 0, 10, random);
    return SCREEN_BOARS_TRAMPLE;
}


// The fight (file 0x10B4E4, field 0x2F): enemy 43 at variant 1, clamp(3,
// 8, random(3) + s / 2 + 1) of them
int
CityVisit::_BoarsBattle()
{
    fFoes.clear();
    const int s = fParty != NULL ? PartyStrength(*fParty) : 1;
    fFoes.push_back(foes{ 43, 1,
        Clamp(int(fRandom() % 3) + s / 2 + 1, 3, 8) });
    fBattleKind = BATTLE_WITH_BOARS;
    fPendingBattle = true;
    return fScreen;
}


// The fight's end: won, an hour, card 6 and a boar's tusk for the leader;
// fled, on with the journey (-1: no card); beaten, card 7 with horses in
// the party, else 8, and everybody is wounded (1, 1, 10)
int
CityVisit::_ResolveBoarsBattle(int outcome)
{
    if (outcome == BATTLE_WON) {
        if (fClock != NULL)
            fClock->AddHours(1);
        if (fParty != NULL && !fParty->members.empty()) {
            const std::vector<item_definition>& definitions
                = fData.Lists().Items();
            if (size_t(kTuskCode) < definitions.size()) {
                const item_definition& tusk = definitions[size_t(kTuskCode)];
                character& leader = fParty->members[
                    size_t(std::min(fParty->leader,
                        int(fParty->members.size()) - 1))];
                AddItem(leader, item{ uint16(kTuskCode), uint8(tusk.type),
                    tusk.quality, 1, tusk.weight });
            }
        }
        return SCREEN_BOARS_WON;
    }
    if (outcome != BATTLE_LOST)
        return -1;
    bool horses = false;
    if (fParty != NULL) {
        const std::vector<item_definition>& definitions = fData.Lists().Items();
        for (const character& member : fParty->members) {
            for (const item& carried : member.items) {
                const size_t code = carried.code & 0x0FFF;
                if (code < definitions.size()
                        && (definitions[code].flags & ITEM_HORSE) != 0)
                    horses = true;
            }
        }
    }
    _Wound(-2, 1, 1, 10);
    return horses ? SCREEN_BOARS_RAMPAGE_MOUNTED : SCREEN_BOARS_RAMPAGE;
}

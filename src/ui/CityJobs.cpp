// The special jobs: the spiders' warehouse (DARKLAND.EXE, state 0x168,
// $DPOST00, file 0x17A374; reached from the first rumor of the special
// jobs, 0xE3FED)

#include "CityVisitInternal.h"

#include "BattleView.h"
#include "Character.h"
#include "GameTime.h"

#include <algorithm>


// The poster (file 0x17A374): the pay is random(5) + 2 florins ($Number1,
// DS:ED4C), the poster's man is the one the rumor named ($NamedOneName,
// the city's people seed + 12, as at file 0xE3F7A)
int
CityVisit::_JobStart()
{
    fJobFlorins = int(fRandom() % 5) + 2;
    fJobAsked = false;
    fJobNight = false;
    fJobVariant = 3;
    _JobVariables();
    return SCREEN_JOB_POSTER;
}


void
CityVisit::_JobVariables()
{
    fVariables["Number1"] = std::to_string(fJobFlorins);
    fVariables["NamedOneName"] = _PersonName(uint16(_City().peopleSeed + 12));
}


// Ask the locals (file 0x17A68E): the option is gone after it. The leader's
// Speak Common twice, less 10 by day, within 1..99: random(100) over it,
// card 4 (nobody knows), else card 3
int
CityVisit::_JobAsk()
{
    fJobAsked = true;
    int chance = 0;
    if (fParty != NULL && !fParty->members.empty())
        chance = 2 * fParty->members[size_t(fParty->leader)].skills[kSkillSpeakCommon];
    if (fClock != NULL && !fClock->IsNight())
        chance -= 10;
    chance = std::max(1, std::min(99, chance));
    _JobVariables();
    return int(fRandom() % 100) > chance ? SCREEN_JOB_NOBODY : SCREEN_JOB_LOCALS;
}


// A skill changed for everybody (0E76:02EA(-2, skill, amount))
static void
ChangeSkills(party* members, int skill, int amount)
{
    if (members == NULL)
        return;
    for (character& member : members->members) {
        member.skills[skill] = uint8(std::max(0, std::min(99,
            int(member.skills[skill]) + amount)));
    }
}


// The advance is paid (1367:0130) and the job is taken (mark 0x65, 9999
// hours: no more rumor of it). Going on (file 0x17A7C8): the warehouse,
// "wait until daytime" only at night. Keeping the money (0x17A836): the
// reputation falls by 20..40, Virtue by 1 for all, back to the street
int
CityVisit::_JobAccept(bool keep)
{
    _Mark(0x65, 9999);
    if (fParty != NULL) {
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash)
            + uint32(fJobFlorins) * 240);
    }
    if (keep) {
        _ChangeReputation(-40, -20);
        ChangeSkills(fParty, kSkillVirtue, -1);
        return SCREEN_SQUARE;
    }
    fJobNight = fClock != NULL && fClock->IsNight();
    return SCREEN_JOB_WAREHOUSE;
}


// Into the warehouse (files 0x17A8C0 and 0x17A9FA). Waiting for daytime:
// until hour 5, and two hours more; then one time in four the spiders are
// giant (variant 3); at once: one time in two, giant at variant 5. The
// others are small ones, swept away (card 9). The code adds random(10)
// Endurance to all of them (0E76:0A72(-2, 0, random(10))), where the card
// speaks of bites
int
CityVisit::_JobEnter(bool wait)
{
    if (wait && fClock != NULL)
        fClock->AddHours(uint32(_HoursUntil(*fClock, 5) + 2));
    fJobVariant = wait ? 3 : 5;
    _JobVariables();
    if (int(fRandom() % (wait ? 4 : 2)) == 0)
        return SCREEN_JOB_SPIDERS;
    const int bites = int(fRandom() % 10);
    if (fParty != NULL) {
        for (character& member : fParty->members)
            AddToAttribute(member, ATTRIBUTE_ENDURANCE, bites);
    }
    return SCREEN_JOB_SWEPT;
}


// Changing one's mind (file 0x17AB4C): the reputation falls by 2..4,
// Virtue by 1 for all, back to the street
int
CityVisit::_JobAbandon()
{
    _ChangeReputation(-4, -2);
    ChangeSkills(fParty, kSkillVirtue, -1);
    return SCREEN_SQUARE;
}


// The giant spiders (field 0x13): random(5) + 1 of enemy 0x39
void
CityVisit::_FightSpiders()
{
    fFoes.clear();
    fFoes.push_back(foes{ 0x39, fJobVariant, int(fRandom() % 5) + 1 });
    fBattleKind = BATTLE_WITH_SPIDERS;
    fPendingBattle = true;
}


// Won: the reputation up by 4, card 11. Lost (the party wakes in the
// webs): the night passes until hour 5, the reputation down by 2, card 12.
// Fled: the reputation down by 4, back to the choice (card 13).
// BattleView does not tell the game's two kinds of defeat apart
int
CityVisit::_ResolveSpidersBattle(int outcome)
{
    _JobVariables();
    if (outcome == BATTLE_WON) {
        _ChangeReputation(4, 4);
        return SCREEN_JOB_DONE;
    }
    if (outcome == BATTLE_LOST) {
        if (fClock != NULL)
            fClock->AddHours(uint32(_HoursUntil(*fClock, 5)));
        _ChangeReputation(-2, -2);
        return SCREEN_JOB_WEBBED;
    }
    _ChangeReputation(-4, -4);
    return SCREEN_JOB_FLED;
}

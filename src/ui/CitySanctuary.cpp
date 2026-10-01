// Sanctuary in a church: the party waits, with the guards outside

#include "CityVisitInternal.h"

#include "BattleView.h"
#include "Character.h"
#include "CityFile.h"
#include "GameTime.h"

#include <algorithm>


// Resting (file 0xF6098, 0xF60F6): until nightfall (19 o'clock) by day,
// until daybreak (6) at night
int
CityVisit::_SanctuaryRest(int hour)
{
    if (fClock != NULL) {
        fClock->AddHours(uint32((hour - int(fClock->Hour()) + 24) % 24));
    }
    return SCREEN_SANCTUARY;
}


// The captain's word (file 0xF62E2): an hour passes at most two, and by
// the reputation (DS:EE74, 0E76:199C): over -10 card 1 ("You are not
// outlaws"), over -75 card 2, else card 3
int
CityVisit::_SanctuaryWord()
{
    if (fClock != NULL)
        fClock->AddHours(fRandom() % 3);
    const int reputation = _Reputation();
    return reputation > -10 ? SCREEN_SANCTUARY_FREE
        : reputation > -75 ? SCREEN_SANCTUARY_STERN : SCREEN_SANCTUARY_HARSH;
}


// Sneaking out through the graveyard (file 0xF636C): the chance is the
// lowest sum of a member's Stealth and Streetwise (0E76:1446(15, 16)) within
// 10..99; he is $ChosenOneName. If random(100) is at most it, card 10, two
// hours, the side streets; else card 9 and an hour
int
CityVisit::_SanctuarySneak()
{
    int weakest = 0;
    int chance = 199;
    if (fParty != NULL) {
        for (size_t i = 0; i < fParty->members.size(); i++) {
            const character& member = fParty->members[i];
            const int sum = member.skills[kSkillStealth]
                + member.skills[kSkillStreetwise];
            if (sum < chance) {
                chance = sum;
                weakest = int(i);
            }
        }
        _SetChosen(weakest);
    }
    chance = std::max(10, std::min(chance, 99));
    if (int(fRandom() % 100) <= chance) {
        if (fClock != NULL)
            fClock->AddHours(2);
        return SCREEN_SANCTUARY_ESCAPED;
    }
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_SANCTUARY_STUMBLE;
}


// Giving up (file 0xF6154): with a reputation over -75, card 11 and the
// dungeon (state 0xD); else a fight with the guards (the battlefield type
// 0x18, the seed location + 0x6F, as the guardroom's foes: random(5) + 4 of
// enemy 3 and the sergeant)
int
CityVisit::_SanctuarySurrender()
{
    if (_Reputation() > -75)
        return SCREEN_SANCTUARY_SURRENDER;
    fBattleKind = BATTLE_AT_SANCTUARY;
    fFoes.clear();
    fFoes.push_back(foes{ 3, int(fRandom() % 3) + 1, int(fRandom() % 5) + 4 });
    fFoes.push_back(foes{ 0, int(fRandom() % 3) + 1, 1 });
    fPendingBattle = true;
    return SCREEN_SANCTUARY;
}


// The fight's result (file 0xF61F3...): the reputation falls by 1..5 with
// a chance; won, the guards are nervous (mark 0x12) for 2000 / the city's
// size hours and an hour passes, the churches; a retreat, an hour and the
// sanctuary again; lost, three hours and the dungeon. The party is then
// wanted (mark 0x11) for 240 hours if the reputation was -75 or less, else
// 120
int
CityVisit::_ResolveSanctuaryBattle(int outcome)
{
    const int before = _Reputation();
    _ChangeReputation(-5, -1);
    const int size = std::max(1, int(_City().size));
    int next = SCREEN_SANCTUARY;
    if (outcome == BATTLE_WON) {
        _Mark(kMarkAlert, uint32(2000 / size));
        if (fClock != NULL)
            fClock->AddHours(1);
        next = SCREEN_CHURCHES;
    } else if (outcome == BATTLE_LOST) {
        if (fClock != NULL)
            fClock->AddHours(3);
        next = _EnterPrison();
    } else if (fClock != NULL) {
        fClock->AddHours(1);
    }
    _Mark(kMarkWanted, before <= -75 ? 240 : 120);
    return next;
}

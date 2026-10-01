// The city's lord: an audience, a clerk, a saint's help, at the fortress
// and at the town hall

#include "CityVisitInternal.h"

#include "Character.h"
#include "CityFile.h"
#include "GameData.h"
#include "GameTime.h"
#include "LocationFile.h"

#include <algorithm>


// 1367:086A(hour): the hours until it, through midnight
/* static */
int
CityVisit::_HoursUntil(const GameTime& clock, int hour)
{
    return (hour - int(clock.Hour()) + 24) % 24;
}


// The location's reputation, as 0E76:1DFE(location, 0, value) stores it
void
CityVisit::_SetReputation(int value)
{
    if (fReputations != NULL && fCity >= 0
            && fCity < int(fReputations->size()))
        (*fReputations)[size_t(fCity)] = int16(value);
}


// The chance of the audience and of the clerk (file 0xA5018, 0xA5300,
// 0xB223C): 0 with a reputation of -40 or less, else the leader's Speak
// Common / 2 + the reputation + his Charisma, halved, 1 while mark 0x3C
// (an audience lately), 99 with mark 0x3B (never made), within 0..99
int
CityVisit::_LordChance() const
{
    const int reputation = _Reputation();
    if (reputation <= -40)
        return 0;
    int chance = 0;
    if (fParty != NULL && !fParty->members.empty()) {
        const character& leader = fParty->members[size_t(std::min(
            int(fParty->leader), int(fParty->members.size()) - 1))];
        chance = (int(leader.skills[kSkillSpeakCommon]) / 2 + reputation
            + int(leader.attributes[ATTRIBUTE_CHARISMA])) / 2;
    }
    if (_Marked(kMarkAudienceTaken))
        chance = 1;
    if (_Marked(kMarkAudienceSure))
        chance = 99;
    return std::max(0, std::min(chance, 99));
}


// A card and then the screen after it (the cards lead to each other by
// ACTION_LORD_NEXT)
int
CityVisit::_LordCard(int screen, int next)
{
    fLordQueue.clear();
    fLordQueue.push_back(next);
    return screen;
}


// The audience (file 0xA4D6C; 0xB2038 in the town hall) or the clerk
// (0xA50B8; 0xB2318); with a chance of 0, the guards' challenge. With an
// appointment made (mark 0x3A) card 1 and out. Else, with the roll
// random(100) at most the chance:
//  - the audience: with a reputation + fame / 10 (0E76:1326(4)) at least
//    the roll, an hour and card 4, else a wait of min(the hours until 18,
//    random(3) + 1) hours (+ 1 in the fortress; random(3) + 2 in the town
//    hall) and card 2 ($Number1); then mark 0x3C for 168 hours and the
//    lord's offer (_LordGrant);
//  - the clerk: with a reputation / 3 + fame / 20 at least the roll, an
//    hour, card 4 and the offer; else two hours, an appointment (mark
//    0x3A until 5 o'clock in the fortress, 18 in the town hall, made by
//    0E76:2C4E(-2, location, 0x5A, 0x3A, 0, 99, 4, hours...)) and card 7.
// Over the chance: mark 0x3C for 168 hours; at most twice the chance, the
// audience card 3 (the hours until 19, the reputation − 1) or the clerk
// card 6 (also if the reputation is 10 or more); else the reputation −
// 10 and card 5, the side streets.
int
CityVisit::_LordRequest(bool clerk)
{
    const int here = fLordHall ? SCREEN_TOWN_HALL : SCREEN_FORTRESS;
    // the audience's exits: the square; the clerk's: the main street in the
    // fortress, the square in the town hall
    const int out = clerk && !fLordHall ? SCREEN_MAIN_STREET : SCREEN_SQUARE;
    const int chance = _LordChance();
    if (fParty != NULL && !fParty->members.empty())
        _SetChosen(std::min(int(fParty->leader), int(fParty->members.size()) - 1));
    if (chance == 0)
        return _Challenge(here);
    if (_Marked(kMarkAppointment))
        return _LordCard(SCREEN_LORD_NOT_TODAY, out);
    const int roll = int(fRandom() % 100);
    const int reputation = _Reputation();
    const int fame = fParty != NULL ? fParty->fame : 0;
    if (roll <= chance) {
        const int liked = clerk ? reputation / 3 + fame / 20
            : reputation + fame / 10;
        if (liked >= roll) {
            if (fClock != NULL)
                fClock->AddHours(1);
            if (!clerk)
                _Mark(kMarkAudienceTaken, 168);
            return _LordGrant(SCREEN_LORD_FAMOUS);
        }
        if (clerk) {
            if (fClock != NULL) {
                fClock->AddHours(2);
                _Mark(kMarkAppointment,
                    uint32(_HoursUntil(*fClock, fLordHall ? 18 : 5)));
            }
            return _LordCard(SCREEN_LORD_TOMORROW, here);
        }
        if (fClock != NULL) {
            const int hours = std::min(_HoursUntil(*fClock, 18),
                int(fRandom() % 3) + (fLordHall ? 2 : 1));
            fVariables["Number1"] = std::to_string(hours);
            fClock->AddHours(uint32(hours + (fLordHall ? 0 : 1)));
        }
        _Mark(kMarkAudienceTaken, 168);
        return _LordGrant(SCREEN_LORD_WAITED);
    }
    _Mark(kMarkAudienceTaken, 168);
    if (2 * chance >= roll || (clerk && reputation >= 10)) {
        if (clerk)
            return _LordCard(SCREEN_LORD_NO_USE, out);
        if (fClock != NULL)
            fClock->AddHours(uint32(_HoursUntil(*fClock, 19)));
        _SetReputation(reputation - 1);
        return _LordCard(SCREEN_LORD_NOT_CONVINCING, out);
    }
    // the town hall's clerk does not take a positive reputation under 0
    _SetReputation(clerk && fLordHall && reputation >= 0
        ? std::max(0, reputation - 10) : reputation - 10);
    return _LordCard(SCREEN_LORD_LAUGHED_AT, SCREEN_SIDE_STREET);
}


// What the lord gives a party he has received (the town hall's 0xB1D28,
// the fortress's code in line): in the town hall the reward for the robber
// knight if one is due (7 times the city's size in florins, no tasks for
// 72 hours, and the lord's thanks); else, in the fortress always and in
// the town hall if random(100) < 50, an hour, `card` and the offer of the
// robber knight; the town hall's other half, another task (state 0x151),
// is not implemented
int
CityVisit::_LordGrant(int card)
{
    if (fLordHall && _RewardDue(3, 10)) {
        const uint32 florins = uint32(_City().size) * 7;
        if (fParty != NULL) {
            fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash)
                + florins * 240);
        }
        _AddEvent(-2, int16(fCity), 0x5F, 7, 10, 0, 0, 72, 0, 0);
        fQuestPatron = 10;
        return _PatronThanks();
    }
    if (fLordHall && fRandom() % 100 >= 50) {
        if (fClock != NULL)
            fClock->AddHours(1);
        fPreviousScreen = SCREEN_SQUARE;
        return _LordCard(card, SCREEN_NOT_IMPLEMENTED);
    }
    const int offer = _LordOffer(SCREEN_SQUARE);
    return _LordCard(card, offer);
}


// The robber knight's task for the lord (patron 10): the castle nearest
// the city, 1462:10B6(10, castle, the city's seed + 0x62, 15, 2, 0), an
// hour, and the offer (state 0x90, card 1) with the way back
int
CityVisit::_LordOffer(int returnTo)
{
    const int castle = _NearestCastle();
    const uint16 seed = uint16(_City().peopleSeed + 0x62);
    _HireAgainstRobber(10, castle, seed, 15, 2, 0);
    if (fClock != NULL)
        fClock->AddHours(1);
    fQuestPatron = 10;
    fQuestPlace = castle;
    fQuestReturn = returnTo;
    fQuestRobber = true;
    fVariables["NamedOneName"] = _PersonName(seed);
    fVariables["NamedTwoName"] = _PersonName(uint16(castle + 1100));
    _SetPlaceVariables(castle, fCity);
    return SCREEN_ROBBER_LORD;
}


// A saint answered (file 0xA53A0; 0xB2638 in the town hall): Alcuin,
// Raymond Penafort and Wolfgang bring a wise old man (card 9), Wenceslaus
// a regal one (card 10), then card 8, an hour, and the offer
int
CityVisit::_LordSaint(int index)
{
    const int offer = _LordOffer(SCREEN_SQUARE);
    fLordQueue.clear();
    fLordQueue.push_back(SCREEN_LORD_SPEECH);
    fLordQueue.push_back(offer);
    return index == 3 ? SCREEN_LORD_SAINT_REGAL : SCREEN_LORD_SAINT_WISE;
}

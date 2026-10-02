// The night watch, the gate and the walls, by day and at night

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


// The guards' price (file 0xA0CDC): max(4, city size - reputation / 10)
// · the party's size · 24 pfennigs
uint32
CityVisit::_Bribe() const
{
    const int size = _City().size;
    const int each = std::max(4, size - _Reputation() / 10);
    const int count = fParty != NULL ? int(fParty->members.size()) : 1;
    return uint32(std::max(each * count * 24, 48));
}


// The watch's fine (file 0xBF160): (city size - reputation / 50 + the
// florins in the purse + 1) · the party's size, in pfennigs
uint32
CityVisit::_Fine() const
{
    const int size = _City().size;
    const int florins = fParty != NULL ? fParty->cash.florins : 0;
    const int count = fParty != NULL ? int(fParty->members.size()) : 1;
    return uint32(std::max(1, (size - _Reputation() / 50 + florins + 1) * count));
}


// Sneaking's chance (file 0xA109A): from 100, each member in turn brings
// it down to his Stealth if lower, then adds 30; 20 less when the market
// is watched
int
CityVisit::_SneakChance() const
{
    int chance = 100;
    if (fParty != NULL) {
        for (const character& member : fParty->members) {
            chance = std::min(chance, int(member.skills[kSkillStealth]));
            chance += 30;
        }
    }
    if (_Marked(kMarkGuarded))
        chance -= 20;
    return std::max(0, std::min(chance, 99));
}


// Sneaking past the guards (file 0xA0F96): into the offices (state
// 0x101, not implemented) with a lesson in Stealth; else, if the roll is
// under twice the chance and 95, the watch only hears you (card 2, an
// hour, the side streets, the market watched 72 hours more); else the
// guards come (card 1, then the watch), no sneaking for 12 hours
int
CityVisit::_Sneak()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int chance = _SneakChance();
    const int roll = random(100);
    if (roll <= chance) {
        if (fParty != NULL)
            TrainParty(*fParty, kSkillStealth, 1, 10, random);
        return SCREEN_NOT_IMPLEMENTED;
    }
    if (fParty != NULL)
        TrainParty(*fParty, kSkillStealth, 0, 10, random);
    if (roll < 2 * chance && roll < 95) {
        _Mark(kMarkGuarded, 72, true);
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_MARKET_STUMBLE;
    }
    _Mark(kMarkSneakFailed, 12);
    _Mark(kMarkGuarded, 32, true);
    fWatchReturn = SCREEN_NOT_IMPLEMENTED;		// the offices
    if (fParty != NULL && !fParty->members.empty())
        _SetChosen(_Slowest());
    return SCREEN_MARKET_ALARM;
}


// Bribing (file 0xA10FC): taken unless the local reputation is -10 or
// less (card 9, into the offices); else refused (card 10), the market
// watched 72 hours more, and the watch
int
CityVisit::_BribeGuards()
{
    if (_Reputation() > -10 && fParty != NULL) {
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) - _Bribe());
        return SCREEN_MARKET_BRIBED;
    }
    _Mark(kMarkGuarded, 72, true);
    fWatchReturn = SCREEN_NOT_IMPLEMENTED;
    return SCREEN_MARKET_REFUSED;
}


// Paying the fine (file 0xBF5C0): an hour, then on as before
int
CityVisit::_PayFine()
{
    if (fParty != NULL)
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) - _Fine());
    if (fClock != NULL)
        fClock->AddHours(1);
    return fWatchReturn;
}


// Running (file 0xBF61A): if random(100) is at most (the slowest member's
// agility + the best Streetwise) / 2, the party escapes (the local
// reputation falls by 1 with a chance of 100 - |reputation| %, an hour,
// card 3, the side streets); else the slowest falls behind (card 4, no
// running again). Either way a small chance of Streetwise.
int
CityVisit::_RunFromWatch()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int slowest = _Slowest();
    const int chance = fParty == NULL || fParty->members.empty() ? 0
        : (fParty->members[slowest].attributes[ATTRIBUTE_AGILITY]
            + _BestSkill(kSkillStreetwise)) / 2;
    if (random(100) <= chance) {
        if (fReputations != NULL && fCity >= 0
                && fCity < int(fReputations->size())) {
            int16& reputation = (*fReputations)[fCity];
            if (random(100) <= 100 - std::abs(int(reputation)))
                reputation = int16(std::max(-99, reputation - 1));
        }
        if (fParty != NULL)
            TrainParty(*fParty, kSkillStreetwise, 7, 5, random);
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_WATCH_ESCAPED;
    }
    if (fParty != NULL) {
        TrainParty(*fParty, kSkillStreetwise, 0, 5, random);
        if (!fParty->members.empty())
            _SetChosen(slowest);
    }
    return SCREEN_NIGHT_WATCH_CAUGHT;
}


// To the main gate (file 0x94398, 0x94444): by day, at night waiting
// for dawn (7 o'clock, card 1); at night, by day waiting for the night
// (20 o'clock, card 2)
int
CityVisit::_GoToGate(bool byDay)
{
    const bool day = fClock == NULL || IsGameDay(*fClock);
    if (byDay == day || fClock == NULL)
        return byDay ? SCREEN_DAY_GATE : SCREEN_NIGHT_GATE;
    const int now = fClock->Hour() * 60 + fClock->Minute();
    const int until = (byDay ? 7 : 20) * 60;
    fClock->AddMinutes(uint32((until - now + 24 * 60) % (24 * 60)));
    return byDay ? SCREEN_WAIT_DAWN : SCREEN_WAIT_NIGHT;
}


// The toll (file 0x92642): (city size / 3 + 1) pfennigs per member
uint32
CityVisit::_Toll() const
{
    const int size = _City().size;
    const int count = fParty != NULL ? int(fParty->members.size()) : 1;
    return uint32((size / 3 + 1) * count);
}


// Paying's chance (file 0x92916): none for the wanted (mark 0x11) or a
// reputation of -10 or less; else 100, or 100 + the (negative)
// reputation, twice while the guards are nervous (mark 0x12), within
// 1..99
int
CityVisit::_TollChance() const
{
    const int reputation = _Reputation();
    if (reputation <= -10 || _Marked(kMarkWanted))
        return 0;
    if (reputation >= 0)
        return 100;
    const int factor = _Marked(kMarkAlert) ? 2 : 1;
    return std::max(1, std::min(99, 100 + factor * reputation));
}


// Befriending the guards' chance (file 0x92A62): none as for paying;
// else the reputation / 2 + the leader's Charisma or Speak Common,
// whichever is higher (1367:0084), within 1..99
int
CityVisit::_CharmChance() const
{
    if (_Reputation() <= -10 || _Marked(kMarkWanted) || fParty == NULL
            || fParty->members.empty()) {
        return 0;
    }
    const character& leader = fParty->members[size_t(fParty->leader)];
    const int best = std::max(int(leader.attributes[ATTRIBUTE_CHARISMA]),
        int(leader.skills[kSkillSpeakCommon]));
    return std::max(1, std::min(99, _Reputation() / 2 + best));
}


// Slipping in's chance (file 0x92B8A): (the party's average speed,
// 0E76:060E, + average Streetwise, 0E76:1600) / 2, halved for the
// wanted, within 1..99. The speed is the agility lowered by the load,
// which is not kept here: the agility.
int
CityVisit::_SlipChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 1;
    int agility = 0;
    int streetwise = 0;
    for (const character& member : fParty->members) {
        agility += member.attributes[ATTRIBUTE_AGILITY];
        streetwise += member.skills[kSkillStreetwise];
    }
    const int count = int(fParty->members.size());
    int chance = (agility / count + streetwise / count) / 2;
    if (_Marked(kMarkWanted))
        chance /= 2;
    return std::max(1, std::min(99, chance));
}


// Paying (file 0x928B8): if random(100) is at most the chance, the toll,
// an hour, card 1 and the main street; else the guards recognize the
// party (state 1, $CHALL00)
int
CityVisit::_PayToll()
{
    if (int(fRandom() % 100) > _TollChance() || _Marked(kMarkWanted))
        return _Challenge();
    if (fParty != NULL)
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) - _Toll());
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_TOLL_PAID;
}


// Befriending the guards (file 0x9298A): the wanted or disliked are
// recognized (state 1); if random(100) is at most the
// chance, card 2, a lesson in Speak Common for the leader (1462:0132
// mode 1), the reputation up by 1, two hours and the main street; else
// no more tries for 12 hours (mark 0x0A), card 3, an hour, the gate
// (the leader's lesson of mode 0 then is not reproduced)
int
CityVisit::_CharmGuards()
{
    if (_Reputation() <= -10 || _Marked(kMarkWanted))
        return _Challenge();
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (random(100) <= _CharmChance()) {
        if (fParty != NULL && !fParty->members.empty()) {
            TrainSkill(fParty->members[size_t(fParty->leader)],
                kSkillSpeakCommon, 10, random);
        }
        _ChangeReputation(1, 1);
        if (fClock != NULL)
            fClock->AddHours(2);
        return SCREEN_GUARDS_CHARMED;
    }
    _Mark(kMarkCharmFailed, 12);
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_GUARDS_UNMOVED;
}


// Slipping in with the crowd (file 0x92ADE): if random(100) is at most
// the chance, card 4, a lesson in Streetwise for all (1462:0132(-2, 16,
// 1, 10)), an hour, the main street; else no more tries for 12 hours
// (mark 0x0B), a lesson of mode 0, and card 5 back before the walls, or
// for the wanted an hour and state 1
int
CityVisit::_SlipIn()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (random(100) <= _SlipChance()) {
        if (fParty != NULL)
            TrainParty(*fParty, kSkillStreetwise, 1, 10, random);
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_SLIPPED_IN;
    }
    _Mark(kMarkSlipFailed, 12);
    if (fParty != NULL)
        TrainParty(*fParty, kSkillStreetwise, 0, 10, random);
    if (_Marked(kMarkWanted)) {
        if (fClock != NULL)
            fClock->AddHours(1);
        return _Challenge();
    }
    return SCREEN_SLIP_NOTICED;
}


// The gate at night's bribe (file 0x934C6): (city size / 3 + 1) · the
// party's size (DS:A67E) · 18 / 10 pfennigs; for a party with a
// negative local reputation (100 - reputation) / 33 pfennigs instead, as
// the game has it
uint32
CityVisit::_NightBribe() const
{
    const int reputation = _Reputation();
    if (reputation < 0)
        return uint32((100 - reputation) / 33);
    const int size = _City().size;
    const int count = fParty != NULL ? int(fParty->members.size()) : 1;
    return uint32((size / 3 + 1) * count * 18 / 10);
}


// Hailing the watch (file 0x936C0): recognized (card 3, no time) with a
// reputation of -10 or less; else let in (card 1, no time, the main
// street) if random(100) is under the reputation / 2 + the fame within
// 0..100 (file 0x937A8, 1367:0028); else no more tries for 12 hours
// (mark 0x0C), card 2 and an hour
int
CityVisit::_HailWatch()
{
    const int reputation = _Reputation();
    if (reputation <= -10)
        return SCREEN_NIGHT_GATE_ALARM;
    const int fame = fParty != NULL ? std::min(100, int(fParty->fame)) : 0;
    const int chance = std::max(0, reputation / 2 + fame);
    if (int(fRandom() % 100) < chance)
        return SCREEN_NIGHT_GATE_OPENED;
    _Mark(kMarkHailFailed, 12);
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_NIGHT_GATE_SHUT;
}


// Talking the way inside (file 0x9380A): recognized with a reputation of
// -40 or less; else through a sally port (card 4, a lesson in Speak
// Common for the leader, an hour, the side streets) if random(100) is
// under (the leader's Speak Common + 2 · Intelligence) / 2 (file
// 0x938EE); else no more tries for 12 hours (mark 0x0D), card 2 and an
// hour. The game's lesson here has mode 7 (and 0 on failure), taken as
// the usual one; the failure's is not reproduced.
int
CityVisit::_TalkToWatch()
{
    if (_Reputation() <= -40)
        return SCREEN_NIGHT_GATE_ALARM;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    int chance = 0;
    if (fParty != NULL && !fParty->members.empty()) {
        const character& leader = fParty->members[size_t(fParty->leader)];
        chance = (leader.skills[kSkillSpeakCommon]
            + 2 * leader.attributes[ATTRIBUTE_INTELLIGENCE]) / 2;
    }
    if (random(100) < chance) {
        if (fParty != NULL && !fParty->members.empty()) {
            TrainSkill(fParty->members[size_t(fParty->leader)],
                kSkillSpeakCommon, 10, random);
        }
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_NIGHT_GATE_TALKED;
    }
    _Mark(kMarkTalkFailed, 12);
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_NIGHT_GATE_SHUT;
}


// Bribing the watchman (file 0x93948): recognized with a reputation of
// -10 or less; else paid (card 5, an hour, the side streets)
int
CityVisit::_BribeWatch()
{
    if (_Reputation() <= -10)
        return SCREEN_NIGHT_GATE_ALARM;
    if (fParty != NULL) {
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash)
            - _NightBribe());
    }
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_NIGHT_GATE_BRIBED;
}


// To the wall (file 0x944E6, 0x9457A): searching for the best spot
// takes city size / 2 + 1 hours by day (card 3 and a wait for dawn if
// the night came meanwhile), size / 2 + 2 at night (the game waits for
// midnight when it is before it, card 4 by day)
int
CityVisit::_GoToWall(bool byDay)
{
    const int size = _City().size;
    if (fClock == NULL)
        return byDay ? SCREEN_DAY_WALL : SCREEN_NIGHT_WALL;
    if (byDay) {
        fClock->AddHours(uint32(size / 2 + 1));
        if (IsGameDay(*fClock))
            return SCREEN_DAY_WALL;
        const int now = fClock->Hour() * 60 + fClock->Minute();
        fClock->AddMinutes(uint32((7 * 60 - now + 24 * 60) % (24 * 60)));
        return SCREEN_WALL_DAWN;
    }
    const int hours = size / 2 + 2;
    const int hour = fClock->Hour();
    if (hour + hours <= 24 && hour != 0) {
        const int now = hour * 60 + fClock->Minute();
        fClock->AddMinutes(uint32(24 * 60 - now));
        return SCREEN_WALL_DUSK;
    }
    fClock->AddHours(uint32(hours));
    return SCREEN_NIGHT_WALL;
}


// The wall's bribe (file 0x99C1E): (city size / 3 + 1) · the party's
// size · 14 / 10 pfennigs, or (100 - reputation) / 33 for a negative
// reputation, as at the gate by night
uint32
CityVisit::_WallBribe() const
{
    const int reputation = _Reputation();
    if (reputation < 0)
        return uint32((100 - reputation) / 33);
    const int size = _City().size;
    const int count = fParty != NULL ? int(fParty->members.size()) : 1;
    return uint32((size / 3 + 1) * count * 14 / 10);
}


// A climber's score (file 0x99BCE): (2 · speed + Stealth) / 2, the speed
// being the agility (the load is not kept)
int
CityVisit::_ClimberScore(int member) const
{
    const character& c = fParty->members[size_t(member)];
    return (2 * c.attributes[ATTRIBUTE_AGILITY] + c.skills[kSkillStealth]) / 2;
}


// The best climber from `first` on (file 0x99BB9, 0x9A87C), the first
// member being the default
int
CityVisit::_BestClimber(int first) const
{
    int best = 0;
    if (fParty == NULL || fParty->members.empty())
        return 0;
    int score = _ClimberScore(0);
    for (int i = first; i < int(fParty->members.size()); i++) {
        if (_ClimberScore(i) > score) {
            score = _ClimberScore(i);
            best = i;
        }
    }
    return best;
}


// Climbing alone's chance (file 0x9A210): the weakest climber's score
int
CityVisit::_WeakestClimber() const
{
    int weakest = 0;
    if (fParty == NULL)
        return 0;
    for (int i = 1; i < int(fParty->members.size()); i++) {
        if (_ClimberScore(i) < _ClimberScore(weakest))
            weakest = i;
    }
    return weakest;
}


// The strongest member (file 0x9AF58), who tries the sewer's grate
int
CityVisit::_Strongest() const
{
    int strongest = 0;
    if (fParty == NULL)
        return 0;
    for (int i = 1; i < int(fParty->members.size()); i++) {
        if (fParty->members[size_t(i)].attributes[ATTRIBUTE_STRENGTH]
                > fParty->members[size_t(strongest)].attributes[ATTRIBUTE_STRENGTH])
            strongest = i;
    }
    return strongest;
}


// A fall (1462:026A(member, 0, minWounds, amount), file 0x80C0A; amount
// 10 for a fall, 20 for the dungeon's beatings, 5 with no minimum for
// the wall from inside): Strength loses random(amount · Strength / 40 +
// 1) - 1, at least 0; Endurance random(amount · Endurance / 20 + 1) - 1,
// at least that and at least minWounds; neither more
// than the attribute's maximum (0E76:0B64); AddToAttribute() keeps them
// at 1 at least
void
CityVisit::_Fall(int member, int amount, int minWounds)
{
    character& c = fParty->members[size_t(member)];
    const int strength = c.attributes[ATTRIBUTE_STRENGTH];
    int lost = int(fRandom() % uint32(amount * strength / 40 + 1)) - 1;
    lost = std::min(std::max(lost, 0), int(c.maxAttributes[ATTRIBUTE_STRENGTH]));
    AddToAttribute(c, ATTRIBUTE_STRENGTH, -lost);
    const int endurance = c.attributes[ATTRIBUTE_ENDURANCE];
    int wounds = int(fRandom() % uint32(amount * endurance / 20 + 1)) - 1;
    wounds = std::min(std::max(std::max(wounds, lost), minWounds),
        int(c.maxAttributes[ATTRIBUTE_ENDURANCE]));
    AddToAttribute(c, ATTRIBUTE_ENDURANCE, -wounds);
}


// Bribing a guard at a door (file 0x99E76): paid, card 1, an hour, the
// side streets
int
CityVisit::_BribeWall()
{
    if (fParty != NULL)
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) - _WallBribe());
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_DAY_WALL_BRIBED;
}


// Up the rope (file 0x99F0E, 0x9AAC4): the best climber
// ($ChosenOneName) climbs if random(100) is under his score: card 3 (1
// at night), a lesson in Stealth for him, an hour, the side streets;
// else he falls (card 4, or 2), a lesson of mode 0 (not reproduced), an
// hour, and no more climbing this stay
int
CityVisit::_ClimbWithRope(bool byDay)
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_OUTSIDE;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    // by day the game looks from the first member, at night from the
    // second with the first as the default: the same one
    const int climber = _BestClimber(1);
    _SetChosen(climber);
    if (fClock != NULL)
        fClock->AddHours(1);
    if (random(100) < _ClimberScore(climber)) {
        TrainSkill(fParty->members[size_t(climber)], kSkillStealth, 10, random);
        return byDay ? SCREEN_DAY_WALL_ROPE : SCREEN_NIGHT_WALL_ROPE;
    }
    _Fall(climber);
    fWallFailed = true;
    return byDay ? SCREEN_DAY_WALL_FALL : SCREEN_NIGHT_WALL_FALL;
}


// Everybody climbing alone (file 0x9A024, 0x9ABD4): if random(100) is
// under the weakest one's score, all are up (card 11, 3 at night), a
// lesson in Stealth for all, an hour, the side streets; else each whose
// score is at most that roll falls (card 12, 11 at night; hurt as by
// _Fall()), those up come back down to help (card 13, 12), an hour, and
// no more climbing this stay. The game shows the card of every fallen
// and every climber; here the first fallen's, and one for those up.
int
CityVisit::_ClimbAlone(bool byDay)
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_OUTSIDE;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int weakest = _WeakestClimber();
    const int roll = random(100);
    if (fClock != NULL)
        fClock->AddHours(1);
    if (roll < _ClimberScore(weakest)) {
        TrainParty(*fParty, kSkillStealth, 7, 10, random);
        return byDay ? SCREEN_DAY_WALL_CLIMBED : SCREEN_NIGHT_WALL_CLIMBED;
    }
    int fallen = -1;
    int up = -1;
    for (int i = 0; i < int(fParty->members.size()); i++) {
        if (_ClimberScore(i) <= roll) {
            _Fall(i);
            if (fallen < 0)
                fallen = i;
        } else if (up < 0)
            up = i;
    }
    fWallFailed = true;
    _SetChosen(fallen >= 0 ? fallen : weakest);
    if (up < 0)
        return byDay ? SCREEN_DAY_WALL_SLIP_ALONE : SCREEN_NIGHT_WALL_SLIP_ALONE;
    return byDay ? SCREEN_DAY_WALL_SLIP : SCREEN_NIGHT_WALL_SLIP;
}


// The sewer's grate (file 0x9AE3A): the strongest ($ChosenOneName) loses
// 3 Endurance; if random(100) is under 1.6 · his Strength (file
// 0x9AF58): card 4, a positive local reputation falls by 2..4 (the
// smell?), city size / 3 hours, the side streets; else card 5, an hour,
// and a mark 0x10 made by 0E76:2D5C(..., 0x10, 3, 99...) (taken as 3
// hours without trying again)
int
CityVisit::_ForceGrate()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_OUTSIDE;
    const int strongest = _Strongest();
    character& c = fParty->members[size_t(strongest)];
    _SetChosen(strongest);
    AddToAttribute(c, ATTRIBUTE_ENDURANCE, -3);
    if (int(fRandom() % 100) < c.attributes[ATTRIBUTE_STRENGTH] * 16 / 10) {
        if (_Reputation() > 0)
            _ChangeReputation(-4, -2);
        if (fClock != NULL)
            fClock->AddHours(uint32(_City().size / 3));
        return SCREEN_SEWER;
    }
    _Mark(kMarkGrateFailed, 500);
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_SEWER_STUCK;
}


// Walking out of the gate (file 0xBCD30): an hour and out of the city;
// after a fight at the gate lately (mark 0x13), four times in ten card 1
// and the fight. 09C0:20F3 (1462:00BA), not decoded, would lead there
// too (taken as false).
int
CityVisit::_ExitWalk()
{
    if (_Marked(kMarkGateFought) && fRandom() % 100 < 40) {
        fGateShoutFight = true;
        return SCREEN_GATE_SHOUT;
    }
    if (fClock != NULL)
        fClock->AddHours(1);
    return -1;
}


// Hiding among the people's chance (file 0xBCEBE): the party's average
// Agility + Streetwise (0E76:05EE(m, 2), 01A0(m, 16)), 25 less after a
// fight at the gate lately, within 0..100
int
CityVisit::_ExitHideChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    int sum = 0;
    for (const character& member : fParty->members) {
        sum += member.attributes[ATTRIBUTE_AGILITY]
            + member.skills[kSkillStreetwise];
    }
    int chance = sum / int(fParty->members.size());
    if (_Marked(kMarkGateFought))
        chance -= 25;
    return std::max(0, std::min(100, chance));
}


// Hiding among the people (file 0xBCDDE): if random(100) is at most the
// chance, card 2, a lesson in Streetwise for all (09C0:1F63(-2, 16, 1,
// 5)), an hour, out of the city; else a lesson of mode 0, an hour, card
// 1, the reputation -1..-4, the gate again
int
CityVisit::_ExitHide()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (fClock != NULL)
        fClock->AddHours(1);
    if (random(100) <= _ExitHideChance()) {
        if (fParty != NULL)
            TrainParty(*fParty, kSkillStreetwise, 1, 5, random);
        return SCREEN_GATE_SLIPPED;
    }
    _ChangeReputation(-4, -1);
    fGateShoutFight = false;
    return SCREEN_GATE_SHOUT;
}


// The fight at the gate (file 0xBCAB4 after card 1, 0xBD27A attacking):
// battlefield 0x2B, random(4) + |s| / 4 + 3 of enemy 3 at variant |s| /
// 4 + 1 and the sergeant at variant random(3) + 1 (s: see
// _FightJailGuards(), 0 here); after card 1 while the guards are nervous
// (mark 0x12) 7 of them at variant 5 and the sergeant at 5. The
// reputation falls by 40, or at -40 or less by 3, or 3..8 with a chance
// of 100 - |reputation| % (0E76:18A8(reputation, 3, 9)).
void
CityVisit::_FightAtGate(bool nervous)
{
    fFoes.clear();
    if (nervous && _Marked(kMarkAlert)) {
        fFoes.push_back(foes{ 3, 5, 7 });
        fFoes.push_back(foes{ 0, 5, 1 });
    } else {
        fFoes.push_back(foes{ 3, 1, int(fRandom() % 4) + 3 });
        fFoes.push_back(foes{ 0, int(fRandom() % 3) + 1, 1 });
    }
    if (fReputations != NULL && fCity >= 0
            && fCity < int(fReputations->size())) {
        int16& reputation = (*fReputations)[fCity];
        int loss = 40;
        if (reputation <= -40) {
            loss = int(fRandom() % 100) <= std::abs(100 - int(reputation))
                ? 3 + int(fRandom() % 6) : 3;
        }
        reputation = int16(std::max(-99, reputation - loss));
    }
    fBattleKind = BATTLE_AT_GATE;
    fPendingBattle = true;
}


// Its result (file 0xBCBA9): a fight at the gate is remembered for 24
// hours (mark 0x13); won, the guards nervous (mark 0x12) for 120 hours,
// card 9, an hour, out of the city; fled, card 10, an hour, the gate;
// lost, card 11 (the bodies dumped in an alley), the gate (the game's
// result 4, card 12 and the dungeon, is its surrender: no BattleView
// outcome). Then wanted (mark 0x11) for 120 hours, 240 at -75 or less.
int
CityVisit::_ResolveGateBattle(int outcome)
{
    _Mark(kMarkGateFought, 24);
    int next = SCREEN_GATE_DUMPED;
    if (outcome == BATTLE_WON) {
        _Mark(kMarkAlert, 120);
        next = SCREEN_GATE_DASHED;
    } else if (outcome != BATTLE_LOST)
        next = SCREEN_GATE_FLED;
    if (fClock != NULL && next != SCREEN_GATE_DUMPED)
        fClock->AddHours(1);
    _Mark(kMarkWanted, _Reputation() <= -75 ? 240 : 120);
    return next;
}


// Horses among the party's items (the item flag 0x2000, 0E76:0DD8)
bool
CityVisit::_HasHorses() const
{
    if (fParty == NULL)
        return false;
    const std::vector<item_definition>& items = fData.Lists().Items();
    for (const character& member : fParty->members) {
        for (const item& carried : member.items) {
            const size_t code = carried.code & 0x0FFF;
            if (code < items.size() && (items[code].flags & ITEM_HORSE) != 0)
                return true;
        }
    }
    return false;
}


// What was in use and is gone is no longer in use
void
ClearGoneEquipment(character& member)
{
    for (uint8& slot : member.equipment) {
        if (slot == kNoEquipment)
            continue;
        bool left = false;
        for (const item& carried : member.items)
            left = left || carried.type == slot;
        if (!left)
            slot = kNoEquipment;
    }
}


// The items with any of the flags are left behind, whole stacks
// (09C0:202B(-2, lo, hi): the horses are 0x2000)
void
CityVisit::_DropItems(uint32 flags)
{
    if (fParty == NULL)
        return;
    const std::vector<item_definition>& items = fData.Lists().Items();
    for (character& member : fParty->members) {
        std::vector<item> kept;
        for (const item& carried : member.items) {
            const size_t code = carried.code & 0x0FFF;
            if (code >= items.size() || (items[code].flags & flags) == 0)
                kept.push_back(carried);
        }
        member.items = kept;
        ClearGoneEquipment(member);
    }
}


// The horses left behind (09C0:202B(-2, 0x2000, 0))
void
CityVisit::_LeaveHorses()
{
    _DropItems(ITEM_HORSE);
}


// Each item (a whole stack) is lost if random(100) is over the percent
// (09C0:2021(member, percent), -2 for all: file 0x666E8, the loop of
// 18E7:0B02)
void
CityVisit::_LoseItems(int keepPercent, int only)
{
    if (fParty == NULL)
        return;
    for (size_t m = 0; m < fParty->members.size(); m++) {
        if (only >= 0 && int(m) != only)
            continue;
        character& member = fParty->members[m];
        for (int i = int(member.items.size()) - 1; i >= 0; i--) {
            if (int(fRandom() % 100) > keepPercent)
                member.items.erase(member.items.begin() + i);
        }
        ClearGoneEquipment(member);
    }
}


// The river is frozen, or nearly, from November to May (file 0xA9893:
// the month, 0-based, is 10 or more or 4 or less)
bool
CityVisit::_ColdWater() const
{
    if (fClock == NULL)
        return false;
    return fClock->Month() >= 10 || fClock->Month() <= 4;
}


// Swimming away from the docks at night (file 0xA9ABC, the options 1 and
// 2 of $DOCKS01 card 0; the same code): the horses, the armor (the item
// flags 0x40 and 0x04000000) and then each item but three in ten (the
// 2021 call with 30) are left; four to seven hours pass ($Number1).
// If the weakest member has a Strength of 10 or more, card 2 and all are
// out. Else each member in turn: with a Strength under 10 and random(100)
// over it, card 3 and the member is lost (09C0:18B5); else card 4 for the
// first one ashore, card 5 for the others. Then the map (state 0xC).
// The members lost leave the party when the cards are over.
int
CityVisit::_Swim()
{
    fSwimSteps.clear();
    fSwimLost.clear();
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_SWIM_ALL;
    _DropItems(ITEM_HORSE);
    _DropItems(ITEM_METAL_ARMOR | ITEM_ARMOR);
    _LoseItems(30);
    const int hours = 4 + int(fRandom() % 4);
    if (fClock != NULL)
        fClock->AddHours(uint32(hours));
    fVariables["Number1"] = std::to_string(hours);

    int weakest = 99;
    for (const character& member : fParty->members)
        weakest = std::min<int>(weakest, member.attributes[ATTRIBUTE_STRENGTH]);
    if (weakest >= 10) {
        fSwimSteps.push_back({ SCREEN_SWIM_ALL, "", "", "", false });
        return SCREEN_SWIM_ALL;
    }
    int first = -1;
    for (int i = 0; i < int(fParty->members.size()); i++) {
        const character& member = fParty->members[size_t(i)];
        const int strength = member.attributes[ATTRIBUTE_STRENGTH];
        if (strength < 10 && int(fRandom() % 100) > strength) {
            fSwimSteps.push_back({ SCREEN_SWIM_LOST, "", "", member.shortName,
                member.female });
            fSwimLost.push_back(i);
            continue;
        }
        if (first < 0)
            first = i;
        const character& leader = fParty->members[size_t(first)];
        fSwimSteps.push_back({ first == i ? SCREEN_SWIM_ONE : SCREEN_SWIM_FOLLOW,
            leader.shortName, member.shortName, "", leader.female });
    }
    return fSwimSteps.front().screen;
}


// A card of the swim is over: the next one, or the members lost leave the
// party and the party is on the map (false); true if nobody is left
bool
CityVisit::_SwimNext()
{
    if (!fSwimSteps.empty())
        fSwimSteps.erase(fSwimSteps.begin());
    if (!fSwimSteps.empty()) {
        _Show(fSwimSteps.front().screen);
        return true;
    }
    for (int i = int(fSwimLost.size()) - 1; i >= 0; i--)
        RemoveMember(*fParty, size_t(fSwimLost[size_t(i)]));
    fSwimLost.clear();
    return fParty != NULL && fParty->members.empty();
}


// The sally port's guard (file 0xBD9E3): (city size / 3 + 1) · the
// party's size · 2 pfennigs, or (100 - reputation) / 33 for a negative
// reputation; twice that after a fight at the gate lately
uint32
CityVisit::_InnerWallBribe() const
{
    const int size = _City().size;
    const int count = fParty != NULL ? int(fParty->members.size()) : 1;
    int bribe = (size / 3 + 1) * count * 2;
    if (_Reputation() < 0)
        bribe = (100 - _Reputation()) / 33;
    if (_Marked(kMarkGateFought))
        bribe *= 2;
    return uint32(bribe);
}


// The sewer's chance (file 0xBDD9E): the member with the best Agility +
// Strength, that sum · 8 / 10 within 0..99
int
CityVisit::_SewerChance(int* member) const
{
    int best = 0;
    int chosen = 0;
    for (int i = 0; fParty != NULL && i < int(fParty->members.size()); i++) {
        const character& c = fParty->members[size_t(i)];
        const int sum = c.attributes[ATTRIBUTE_AGILITY]
            + c.attributes[ATTRIBUTE_STRENGTH];
        if (sum > best) {
            best = sum;
            chosen = i;
        }
    }
    if (member != NULL)
        *member = chosen;
    return std::max(0, std::min(99, best * 8 / 10));
}


// The rope's and the climb's chance (file 0xBE1A8): the lowest Stealth
// of the party (0E76:1396(15)), halved after a fight at the gate lately
int
CityVisit::_WallStealth(int* member) const
{
    int lowest = 199;
    int chosen = 0;
    for (int i = 0; fParty != NULL && i < int(fParty->members.size()); i++) {
        const int stealth = fParty->members[size_t(i)].skills[kSkillStealth];
        if (stealth < lowest) {
            lowest = stealth;
            chosen = i;
        }
    }
    if (member != NULL)
        *member = chosen;
    if (_Marked(kMarkGateFought))
        lowest /= 2;
    return lowest;
}


// The sewer (file 0xBDC88, 0xBDE1A with the horses): if random(100) is
// under the chance, card 1 (the grate broken by $ChosenOneName), the
// horses left, out of the city; else mark 0x10 for 500 hours
// (0E76:2C4E), a positive reputation -2..-4, city size / 3 hours, card 2
int
CityVisit::_Sewer(bool horses)
{
    (void)horses;						// both leave the horses
    int member = 0;
    const int chance = _SewerChance(&member);
    if (fParty != NULL && !fParty->members.empty())
        _SetChosen(member);
    if (int(fRandom() % 100) < chance) {
        _LeaveHorses();
        return SCREEN_SEWER_OUT;
    }
    _Mark(kMarkGrateFailed, 500);
    if (_Reputation() > 0)
        _ChangeReputation(-4, -2);
    if (fClock != NULL)
        fClock->AddHours(uint32(_City().size / 3));
    return SCREEN_SEWER_BLOCKED;
}


// The sally port (file 0xBDF9A): over -10 and not wanted, the bribe
// paid, card 3, an hour, the horses left, out of the city; else card 4,
// mark 0x22 for 48 hours (0E76:2C4E), no time, and the guards' challenge
int
CityVisit::_BribeSally()
{
    if (_Reputation() > -10 && !_Marked(kMarkWanted)) {
        if (fParty != NULL) {
            const uint32 purse = TotalPfennigs(fParty->cash);
            const uint32 bribe = _InnerWallBribe();
            fParty->cash = MoneyFromPfennigs(purse > bribe ? purse - bribe : 0);
        }
        if (fClock != NULL)
            fClock->AddHours(1);
        _LeaveHorses();
        return SCREEN_SALLY_BRIBED;
    }
    _Mark(kMarkSallyAlarm, 48);
    return SCREEN_SALLY_ALARM;
}


// Down a rope (file 0xBE0B8, 0xBE1EC with the horses: by day card 12 and
// a wait until 19 o'clock first): if random(100) is at most the chance,
// a rope used (18E7:04D4(-2, 59)), a lesson in Stealth for all (mode 7),
// card 5, the horses left, three hours, out of the city; else card 6,
// an hour, the walls guarded for 24 hours (mark 0x0F)
int
CityVisit::_RopeDown(bool afterDark)
{
    if (afterDark && fClock != NULL && IsGameDay(*fClock)) {
        const int hour = fClock->Hour();
        fClock->AddHours(uint32(hour > 19 ? 19 - hour + 24 : 19 - hour));
        fAfterDark = 4;
        return SCREEN_WAIT_FOR_DARK;
    }
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (random(100) > _WallStealth(NULL)) {
        if (fClock != NULL)
            fClock->AddHours(1);
        _Mark(kMarkWallAlert, 24);
        return SCREEN_WALL_SPOTTED;
    }
    bool used = false;
    for (character& member : fParty->members) {
        for (size_t i = 0; !used && i < member.items.size(); i++) {
            if ((member.items[i].code & 0x0FFF) != kRopeCode)
                continue;
            if (member.items[i].quantity > 1)
                member.items[i].quantity--;
            else
                member.items.erase(member.items.begin() + long(i));
            used = true;
        }
    }
    TrainParty(*fParty, kSkillStealth, 7, 10, random);
    _LeaveHorses();
    if (fClock != NULL)
        fClock->AddHours(3);
    return SCREEN_ROPE_DOWN;
}


// Over the wall (file 0xBE35A, 0xBE562 with the horses: the roll first,
// then by day card 12 and the wait): if the roll, random(100), is at
// most the chance, a lesson in Stealth for all (mode 1), every member
// whose speed · 3 (0E76:06DE; the agility here) is under the roll falls
// (1462:026A(m, 0, 0, 5)), card 7 (nobody), 8 (one) or 9, the horses
// left, three hours, out of the city; else card 6, an hour, a lesson of
// mode 0, the walls guarded for 24 hours
int
CityVisit::_ClimbOver(bool afterDark)
{
    if (afterDark && fClock != NULL && IsGameDay(*fClock)) {
        const int hour = fClock->Hour();
        fClock->AddHours(uint32(hour > 19 ? 19 - hour + 24 : 19 - hour));
        fAfterDark = 6;
        return SCREEN_WAIT_FOR_DARK;
    }
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int roll = random(100);
    if (roll > _WallStealth(NULL) || fParty == NULL) {
        if (fClock != NULL)
            fClock->AddHours(1);
        _Mark(kMarkWallAlert, 24);
        return SCREEN_WALL_SPOTTED;
    }
    TrainParty(*fParty, kSkillStealth, 1, 10, random);
    int fallen = 0;
    for (int i = 0; i < int(fParty->members.size()); i++) {
        if (3 * fParty->members[size_t(i)].attributes[ATTRIBUTE_AGILITY] < roll) {
            _Fall(i, 5, 0);
            fallen++;
        }
    }
    _LeaveHorses();
    if (fClock != NULL)
        fClock->AddHours(3);
    return fallen == 0 ? SCREEN_OVER_WALL
        : fallen == 1 ? SCREEN_OVER_WALL_ONE_FELL : SCREEN_OVER_WALL_FALLS;
}

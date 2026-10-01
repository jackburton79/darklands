// The banks' tasks against a robber knight, his tower and the reward

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


// The tower, or the fort his men have built
int
CityVisit::_TowerScreen() const
{
    return _EventHere(3, 0x27) ? SCREEN_FORT : SCREEN_TOWER;
}


// The garrison's losses (0E76:3742(3, 2, place)): +0x1E of the robber
// knight's event of subject 2 here, 0 if none
int
CityVisit::_Losses() const
{
    const int e = _FindEvent(0x1C, 2, fCity, -1, 3, -1);
    return e >= 0 ? (*fEvents)[size_t(e)].unknown1E : 0;
}


// 0E76:2E74(-2, place, 0x1C, 2, count, 3, add): the event made for 999
// hours if there is none
void
CityVisit::_AddLosses(int count)
{
    int e = _FindEvent(0x1C, 2, fCity, -1, 3, -1);
    if (e < 0)
        e = _AddEvent(-2, int16(fCity), 0, 0x1C, 2, 0, 0, 999, 0, 3);
    if (e >= 0)
        (*fEvents)[size_t(e)].unknown1E += int16(count);
}


// Laying siege (file 0xFFD5A): random(24) hours, card 1; then, s the
// losses, the knight attacks if random(3) + 2 <= s (card 7), else his
// band returns if random(11) <= s + 5 (card 8), else the garrison
// sallies (card 9)
int
CityVisit::_LaySiege()
{
    const int s = _Losses();
    if (fClock != NULL)
        fClock->AddHours(fRandom() % 24);
    if (int(fRandom() % 3) + 2 <= s)
        fAfterCard = SCREEN_SIEGE_ATTACK;
    else if (int(fRandom() % 11) <= s + 5)
        fAfterCard = SCREEN_SIEGE_RETURN;
    else
        fAfterCard = SCREEN_SIEGE_SALLY;
    return SCREEN_SIEGE;
}


// Asking to come inside (file 0x10019E): s the losses, one less with a
// fame over 200, one more over 400; r = random(6). Welcome if s <= 0 or
// r > s + 4 (card 11, an hour, the audience), attacked if r <= s (card
// 12, an hour), else turned away (card 10, three hours)
int
CityVisit::_AskInside()
{
    const int fame = fParty != NULL ? fParty->fame : 0;
    int s = _Losses();
    if (fame > 200)
        s--;
    if (fame > 400)
        s--;
    const int r = int(fRandom() % 6);
    int next = SCREEN_TOWER_REFUSED;
    if (s <= 0 || r > s + 4)
        next = SCREEN_TOWER_WELCOME;
    else if (r <= s)
        next = SCREEN_TOWER_ATTACK;
    if (fClock != NULL)
        fClock->AddHours(next == SCREEN_TOWER_REFUSED ? 3 : 1);
    fAfterCard = _TowerScreen();
    return next;
}


// The knight accepts a duel with a chance of s · 4 - fame / 10 + 50,
// within 1..99
int
CityVisit::_DuelChance() const
{
    const int fame = fParty != NULL ? fParty->fame : 0;
    return std::max(1, std::min(99, _Losses() * 4 - fame / 10 + 50));
}


// Single combat (file 0x100316): accepted if random(100) <= the chance
// (card 13, two hours), else his men if random(4) <= s (card 14, an
// hour), else turned away (card 10, three hours)
int
CityVisit::_Duel()
{
    int next = SCREEN_TOWER_REFUSED;
    if (int(fRandom() % 100) <= _DuelChance())
        next = SCREEN_DUEL;
    else if (int(fRandom() % 4) <= _Losses())
        next = SCREEN_DUEL_MEN;
    if (fClock != NULL)
        fClock->AddHours(next == SCREEN_DUEL ? 2
            : next == SCREEN_DUEL_MEN ? 1 : 3);
    fAfterCard = _TowerScreen();
    return next;
}


// (average Agility + average Stealth) / 2, +30 where the knight's men are
// away (0E76:360C(3, 4, place)), within 0..99
int
CityVisit::_TowerSneakChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    int agility = 0;
    int stealth = 0;
    for (const character& member : fParty->members) {
        agility += member.attributes[ATTRIBUTE_AGILITY];
        stealth += member.skills[kSkillStealth];
    }
    const int count = int(fParty->members.size());
    int chance = (agility / count + stealth / count) / 2;
    if (_EventHere(3, 4))
        chance += 30;
    return std::max(0, std::min(99, chance));
}


// Sneaking in after dark (file 0x100486): two hours, by day until 19h;
// in (card 16, a lesson in Stealth of mode 1, inside) or heard (card 17,
// a lesson of mode 0, his men)
int
CityVisit::_SneakIntoTower()
{
    if (fClock != NULL) {
        fClock->AddHours(2);
        if (!fClock->IsNight())
            fClock->AddHours(19 - fClock->Hour());
    }
    const bool in = int(fRandom() % 100) <= _TowerSneakChance();
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (fParty != NULL)
        TrainParty(*fParty, kSkillStealth, in ? 1 : 0, 10, random);
    return in ? SCREEN_SNEAK_IN : SCREEN_SNEAK_HEARD;
}


// Storming the tower with allies (file 0x100722): an event of subject 6
// for 3 hours (0E76:2C4E(-2, place, 0, 0x1C, 6, 0, 0, 3, 0, 3)), inside
int
CityVisit::_StormTower()
{
    _AddEvent(-2, int16(fCity), 0, 0x1C, 6, 0, 0, 3, 0, 3);
    return _TowerInside(0x94);
}


// The knight himself (file 0xFF9B8): enemy 35 at variant 4
void
CityVisit::_FightKnight()
{
    fFoes.clear();
    fFoes.push_back(foes{ 35, 4, 1 });
    fBattleKind = BATTLE_WITH_KNIGHT;
    fPendingBattle = true;
}


// His men (file 0xFFBF6): random(5) + 3 of enemy 3 at variant 2
void
CityVisit::_FightKnightsMen()
{
    fFoes.clear();
    fFoes.push_back(foes{ 3, 2, int(fRandom() % 5) + 3 });
    fBattleKind = BATTLE_WITH_KNIGHTS_MEN;
    fPendingBattle = true;
}


// Won, the knight slain (card 20, _KnightSlain()); fled, card 21, two
// hours, the tower; lost, card 22, a day, a beating, the search, and his
// losses one less
int
CityVisit::_ResolveKnightBattle(int outcome)
{
    if (outcome == BATTLE_WON) {
        _KnightSlain();
        return SCREEN_KNIGHT_SLAIN;
    }
    if (outcome != BATTLE_LOST) {
        if (fClock != NULL)
            fClock->AddHours(2);
        fAfterCard = _TowerScreen();
        return SCREEN_DRIVEN_OFF;
    }
    if (fClock != NULL)
        fClock->AddHours(24);
    _Beating();
    _Search();
    _AddLosses(-1);
    return SCREEN_LEFT_FOR_DEAD;
}


// Won, card 23, an hour, his losses one more, the tower; fled, card 21,
// three hours, the map; lost, card 22, three hours, the map
int
CityVisit::_ResolveMenBattle(int outcome)
{
    if (outcome == BATTLE_WON) {
        if (fClock != NULL)
            fClock->AddHours(1);
        _AddLosses(1);
        fAfterCard = _TowerScreen();
        return SCREEN_MEN_BEATEN;
    }
    if (fClock != NULL)
        fClock->AddHours(3);
    fAfterCard = -1;
    return outcome == BATTLE_LOST ? SCREEN_LEFT_FOR_DEAD : SCREEN_DRIVEN_OFF;
}


// The knight slain (file 0xFFA7F): an event of subject 1 for 2400 hours
// (0E76:2C4E(-2, place, 0x5B, 0x1C, 1, 0, 0, 2400, 0, 3)), the patrons'
// rewards (0E76:3E06(3, place)), mark 0x27 for 800 hours; the castle is
// taken (flag 4 of +0x14) and becomes a ruin (state 0x157, not
// implemented)
void
CityVisit::_KnightSlain()
{
    _AddEvent(-2, int16(fCity), 0x5B, 0x1C, 1, 0, 0, 2400, 0, 3);
    _ClaimRewards(3, fCity);
    _Mark(0x27, 800);
    if (fLocationFlags != NULL && fCity >= 0
            && fCity < int(fLocationFlags->size()))
        (*fLocationFlags)[size_t(fCity)] |= 4;
    if (fEnterStates != NULL && fCity >= 0
            && fCity < int(fEnterStates->size()))
        (*fEnterStates)[size_t(fCity)] = 0x157;
}


// The fame for a task of a level (1462:1E06): the table, then by the
// difficulty (DS:906A) 2/3 of it at 0 and 3/2 at 2, rounded down
static int
FameFor(int level, int difficulty)
{
    static const int kFame[5] = { 0, 3, 10, 25, 64 };
    const int fame = level >= 0 && level < 5 ? kFame[level] : 200;
    if (difficulty == DIFFICULTY_BASIC)
        return fame * 2 / 3;
    if (difficulty == DIFFICULTY_EXPERT)
        return fame * 3 / 2;
    return fame;
}


// The tasks done (0E76:3E06(kind, place)): each patron's event of the
// kind at the place (category 8) becomes a reward due for a year
// (category 36) at the patron's city (+0x1E), its +0x1A the patron,
// subject too, +0x1E the old subject, +0x2E kept, +0x2C the place; the
// patron's city likes the party better by +0x2A, the fame grows by the
// task's level (+0x2C). If there was any, the events of the kind at the
// place (categories 8 and 28) are gone (0E76:3D92).
void
CityVisit::_ClaimRewards(int kind, int place)
{
    if (fEvents == NULL)
        return;
    bool any = false;
    const size_t count = fEvents->size();
    for (size_t i = 0; i < count; i++) {
        const world_event e = (*fEvents)[i];
        if (e.category != 8 || e.kind != kind || e.location != place)
            continue;
        any = true;
        const int reward = _AddEvent(e.unknown1A, e.unknown1E, e.unknown20,
            0x24, e.unknown1A, e.subject, 0, 8760, 0, int16(kind));
        if (reward >= 0) {
            (*fEvents)[size_t(reward)].unknown2E = e.unknown2E;
            (*fEvents)[size_t(reward)].unknown2C = int16(place);
        }
        if (fReputations != NULL && e.unknown1E >= 0
                && e.unknown1E < int(fReputations->size())) {
            int16& reputation = (*fReputations)[size_t(e.unknown1E)];
            reputation = int16(reputation + e.unknown2A);
        }
        if (fParty != NULL)
            fParty->fame = uint16(fParty->fame + FameFor(e.unknown2C,
                fSettings != NULL ? fSettings->difficulty : DIFFICULTY_STANDARD));
    }
    if (!any)
        return;
    std::vector<world_event> kept;
    for (const world_event& e : *fEvents) {
        if ((e.category == 8 || e.category == 0x1C) && e.kind == kind
                && e.location == place)
            continue;
        kept.push_back(e);
    }
    *fEvents = kept;
}


// The audience (state 0x95, $RAUBI05) and inside the tower (0x94,
// $RAUBI04) are not implemented: back to the tower
int
CityVisit::_TowerInside(int state)
{
    (void)state;
    fPreviousScreen = _TowerScreen();
    return SCREEN_NOT_IMPLEMENTED;
}


// A task of a patron's (+0x1A) from this city (+0x1E) running (0E76:353E)
bool
CityVisit::_PatronQuest(int kind, int patron) const
{
    for (size_t i = 0; fEvents != NULL && i < fEvents->size(); i++) {
        const world_event& e = (*fEvents)[i];
        if (e.category == 8 && e.kind == kind && e.unknown1A == patron
                && e.unknown1E == fCity && _EventRunning(e))
            return true;
    }
    return false;
}


// A bank's reward for a robber knight (file 0xC4940, the Fuggers;
// 0xC692E, the Medici): twice the city's size in florins (1367:0130),
// $Money1, card 7, no tasks for 72 hours (an event of category 7,
// 0E76:2C4E(-2, location, 0x5F, 7, patron, 0, 0, 72, 0, 0)), then the
// patron's thanks (state 0x91). The bank's standing (DS:4BB6, 4BB8: +5)
// is not kept; a reward in goods (+0x2E) is not paid by the game.
int
CityVisit::_BankReward(int patron)
{
    fThanksReturn = patron == 6 ? SCREEN_MEDICI : SCREEN_FUGGER;
    fQuestPatron = patron;
    const uint32 florins = uint32(_City().size) * 2;
    if (fParty != NULL) {
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash)
            + florins * 240);
    }
    fVariables["Money1"] = MoneyText(florins * 240);
    _AddEvent(-2, int16(fCity), 0x5F, 7, int16(patron), 0, 0, 72, 0, 0);
    return patron == 6 ? SCREEN_MEDICI_REWARD : SCREEN_FUGGER_REWARD;
}


// The patron's thanks (state 0x91, file 0xFEB8C): the reward (0E76:3B62(
// 36, -1, location, patron, 3, -1)) names the patron ($NamedOneName,
// 1367:0DB4(its +0x1E)) and the knight ($NamedTwoName, 1367:0DB4(its
// castle + 1100)), and is gone (0E76:30FE). The card goes by the
// patron's place (DS:E7D8): at a bank card 5 one time in four, else 8.
// Then back to the patron.
int
CityVisit::_PatronThanks()
{
    const int reward = _FindEvent(0x24, -1, fCity, fQuestPatron, 3, -1);
    if (reward >= 0) {
        const world_event e = (*fEvents)[size_t(reward)];
        fVariables["NamedOneName"] = _PersonName(uint16(e.unknown1E));
        fVariables["NamedTwoName"] = _PersonName(uint16(e.unknown2C + 1100));
        fEvents->erase(fEvents->begin() + reward);
    }
    if (fQuestPatron == 10) {
        // the city's lord, at the square (DS:E7D8 0x12, 0x19): card 0, the
        // reputation 40..50 better (0E76:19D0)
        _ChangeReputation(40, 50);
        fThanksReturn = SCREEN_SQUARE;
        return SCREEN_ROBBER_LORD_THANKS;
    }
    return fRandom() % 4 == 0 ? SCREEN_ROBBER_AVENGED : SCREEN_ROBBER_REASON;
}


// A patron's reward due here (0E76:3404: category 36)
bool
CityVisit::_RewardDue(int kind, int patron) const
{
    for (size_t i = 0; fEvents != NULL && i < fEvents->size(); i++) {
        const world_event& e = (*fEvents)[i];
        if (e.category == 0x24 && e.kind == kind && e.unknown1A == patron
                && e.location == fCity && _EventRunning(e))
            return true;
    }
    return false;
}


// The banks' tasks are not offered (file 0xC42C3) with a task of theirs
// running from here (kinds 10 and 3), after a refusal lately (an event of
// category 7 and subject the patron here, 0E76:392C), with a reputation
// under 0 (the bank's standing, DS:4BB6/4BB8, not kept: 0), or with a
// reward due (the reward's option then)
bool
CityVisit::_PatronBusy(int patron) const
{
    if (_Reputation() < 0 || _PatronQuest(10, patron)
            || _PatronQuest(3, patron) || _RewardDue(10, patron)
            || _RewardDue(3, patron))
        return true;
    for (size_t i = 0; fEvents != NULL && i < fEvents->size(); i++) {
        const world_event& e = (*fEvents)[i];
        if (e.category == 7 && e.subject == patron && e.location == fCity
                && _EventRunning(e))
            return true;
    }
    return false;
}


// The castle (location type 2) nearest to the city, not taken (+0x14 bit
// 4 of its record), not on the city itself (1462:2842)
int
CityVisit::_NearestCastle() const
{
    const LocationFile& locations = fData.Locations();
    if (fCity < 0 || uint32(fCity) >= locations.CountLocations())
        return fCity;
    const location& here = locations.LocationAt(uint32(fCity));
    int nearest = -1;
    int best = 9999;
    for (uint32 i = 0; i < locations.CountLocations(); i++) {
        const location& c = locations.LocationAt(i);
        if (c.type != 2 || (c.x == here.x && c.y == here.y))
            continue;
        if (fLocationFlags != NULL && i < fLocationFlags->size()
                && ((*fLocationFlags)[i] & 0x04) != 0)
            continue;
        const int d = MapDistance(here.x, here.y, c.x, c.y);
        if (d < best) {
            best = d;
            nearest = int(i);
        }
    }
    return nearest >= 0 ? nearest : fCity;
}


// The chance of a task (file 0xC48EE, 0xC68F4): the reputation + the
// fame (0E76:1326(4)) + the Fuggers' location property 0x0C (the
// location's byte +0x15, 25 in every city) or the Medici's standing
// (DS:4BB8, not kept: 0), within 10..50
int
CityVisit::_BankTaskChance(int patron) const
{
    const int fame = fParty != NULL ? fParty->fame : 0;
    const int chance = _Reputation() + fame + (patron == 8 ? 25 : 0);
    return std::max(10, std::min(50, chance));
}


// Asking the banks for a task (file 0xC473E, 0xC677E): if random(100) is
// at most the chance, one time in two the robber knight (the reward,
// $Money1, twice the city's size in florins; card 1; the castle nearest
// the city, 1462:2842(x, y, 2); the knight's events, 1462:10B6; an hour;
// his offer, state 0x90), else another task (the city's size in florins,
// card 1, an hour, state 0x151: not implemented); else card 10 and a
// refusal remembered for 72 hours (an event of category 7, subject the
// patron, +0x20 0x5F)
int
CityVisit::_BankTasks(int patron)
{
    const int size = _City().size;
    fQuestPatron = patron;
    // the master banker (the city's number + 8, the Medici's + 6)
    fVariables["NamedOneName"] = _PersonName(uint16(
        _City().peopleSeed + patron));
    if (int(fRandom() % 100) > _BankTaskChance(patron)) {
        _AddEvent(-2, int16(fCity), 0x5F, 7, int16(patron), 0, 0, 72, 0, 0);
        return patron == 6 ? SCREEN_MEDICI_BUSY : SCREEN_FUGGER_BUSY;
    }
    fQuestRobber = fRandom() % 100 < 50;
    fVariables["Money1"] = MoneyText(uint32(fQuestRobber ? 2 * size : size) * 240);
    return patron == 6 ? SCREEN_MEDICI_TASK : SCREEN_FUGGER_TASK;
}


// After the purse: the robber knight's offer, or the other task
int
CityVisit::_OfferQuest()
{
    if (fClock != NULL)
        fClock->AddHours(1);
    if (!fQuestRobber) {
        fPreviousScreen = fQuestPatron == 6 ? SCREEN_MEDICI : SCREEN_FUGGER;
        return SCREEN_NOT_IMPLEMENTED;
    }
    const int castle = _NearestCastle();
    const uint16 number = _City().peopleSeed;
    const int patronSeed = number + (fQuestPatron == 6 ? 6 : 8);
    _HireAgainstRobber(fQuestPatron, castle, patronSeed, 12, 2, 0);
    fQuestPlace = castle;
    // the offer (file 0xFE84A): the patron and the knight, named after
    // the castle (1367:0DB4(castle + 1100, 0, 1)), where the castle is
    fVariables["NamedOneName"] = _PersonName(uint16(patronSeed));
    fVariables["NamedTwoName"] = _PersonName(uint16(castle + 1100));
    _SetPlaceVariables(castle, fCity);
    return fQuestPatron == 6 ? SCREEN_ROBBER_MEDICI : SCREEN_ROBBER_FUGGER;
}


// Hiring the party against the robber knight of a castle (1462:10B6):
// the patron's task, an event of category 8, kind 3 at the castle
// (+0x1A the patron, subject its seed, +0x1E this city, +0x20 0x5F, for
// 9998 hours); if the castle has no knight yet, the task's event is his
// (+0x2A the reward's level, +0x2C his strength, at most 4, +0x2E), with
// his three men (category 28, kind 3, subjects 2, 3, 4) and the castle
// taken (category 43, +0x1E 1, forever); else the knight's event keeps
// the higher level and strength
void
CityVisit::_HireAgainstRobber(int patron, int castle, int patronSeed,
    int reward, int strength, int extra)
{
    strength = std::min(strength, 4);
    const int knight = _FindEvent(8, -1, castle, -1, 3, -1);
    const int task = _AddEvent(int16(patron), int16(castle), 0x5F, 8,
        int16(patronSeed), int16(fCity), 0, 9998, 0, 3);
    if (knight >= 0) {
        world_event& e = (*fEvents)[size_t(knight)];
        e.unknown2A = int16(std::max(int(e.unknown2A), reward));
        e.unknown2C = int16(std::max(int(e.unknown2C), strength));
        return;
    }
    if (task < 0)
        return;
    world_event& e = (*fEvents)[size_t(task)];
    e.unknown2A = int16(reward);
    e.unknown2C = int16(strength);
    e.unknown2E = int16(extra);
    for (int16 man = 2; man <= 4; man++)
        _AddEvent(-2, int16(castle), 0, 0x1C, man, 0, 0, 9998, 0, 3);
    _AddEvent(-2, int16(castle), 0, 0x2B, 0, 1, 0, 9999, 0, 0);
}

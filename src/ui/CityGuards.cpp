// The watch and the guards: challenges, fights, chases, the hazards of the
// night

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


// Attacking the watch (file 0xBF914): they flee (card 7) if random(100)
// is at most the chance: |reputation| / 10 + the party's average
// Charisma / 10 (0E76:1800) + the leader's best weapon skill / 2
// (0E76:01C0) + the fame / 20 (0E76:1326), 0 under 75, at most 90. Else
// (file 0xBF3A2) the local reputation falls by 15..24 with a chance of
// 100 - |reputation| % (0E76:19D0), and the battle begins.
int
CityVisit::_FightWatch()
{
    if (fParty == NULL || fParty->members.empty())
        return fWatchReturn;
    int charisma = 0;
    for (const character& member : fParty->members)
        charisma += member.attributes[ATTRIBUTE_CHARISMA];
    charisma /= int(fParty->members.size());
    const character& leader = fParty->members[size_t(fParty->leader)];
    int weaponSkill = 0;
    for (int s = 0; s < kWeaponSkillCount; s++)
        weaponSkill = std::max(weaponSkill, int(leader.skills[s]));
    int chance = std::abs(_Reputation()) / 10 + charisma / 10
        + weaponSkill / 2 + fParty->fame / 20;
    if (chance < 75)
        chance = 0;
    chance = std::min(chance, 90);
    if (int(fRandom() % 100) <= chance)
        return SCREEN_WATCH_SCARED;

    if (fReputations != NULL && fCity >= 0
            && fCity < int(fReputations->size())) {
        int16& reputation = (*fReputations)[fCity];
        if (int(fRandom() % 100) <= 100 - std::abs(int(reputation))) {
            reputation = int16(std::max(-99,
                reputation - 15 - int(fRandom() % 10)));
        }
    }
    fBattleKind = BATTLE_WITH_WATCH;
    fFoes.clear();
    fFoes.push_back(foes{ 3, 1, int(fRandom() % 5) + 4 });
    fFoes.push_back(foes{ 0, 2, 1 });
    fPendingBattle = true;
    return -1;
}


// A battle with fFoes: enemies of DARKLAND.ENM at a variant of their
// group (e.g. the watch, file 0xBF3A2: die(5) + 3 of enemy 3, the
// "Guard" types, at variant 1 and one of enemy 0, "Sergeant", at
// variant 2, as TAC.TXT prints them). On a city map: which one the game
// picks (from the battlefield type) and where everybody starts are not
// decoded, so the map is one of ICITY.000..003 and the foes start near
// the party.
void
CityVisit::_RunBattle(GameWindow& window)
{
    fPendingBattle = false;
    BattleView view(fData);
    view.SetMenuBar(fMenu);
    {
        std::unique_ptr<Catalog> maps(fData.OpenCatalog("IMAPS.CAT"));
        const std::string name = "ICITY.00" + std::to_string(fRandom() % 4);
        std::unique_ptr<Stream> stream(maps->GetStream(name));
        if (!stream)
            throw std::runtime_error("CityVisit: no map " + name);
        view.SetMap(std::unique_ptr<BattleMap>(new BattleMap(stream.get())),
            name);
    }
    for (size_t i = 0; i < fParty->members.size(); i++) {
        int x = 12;
        int y = 20;
        if (view.FindFreeCell(x, y)) {
            view.AddPartyMember(int(i), fParty->members[i], fParty->images[i],
                i < fParty->colors.size() ? fParty->colors[i]
                    : std::vector<uint8>(), x, y, 2);
        }
    }
    const EnemyFile& enemies = fData.Enemies();
    for (const foes& group : fFoes) {
        const uint32 first = enemies.EnemyAt(uint32(group.enemy)).type;
        const int variants = std::max(1, int(enemies.TypeAt(first).variants));
        const uint32 type = first + uint32(std::min(group.variant, variants - 1));
        for (int i = 0; i < group.count; i++) {
            int x = 20;
            int y = 20;
            if (view.FindFreeCell(x, y))
                view.AddEnemy(type, x, y, 6);
        }
    }
    view.Scroll(0, 12);
    const battle_outcome outcome = view.Run(window);

    // the wounded keep their wounds, the dead leave the party; with
    // nobody left the game is over (the game's end sequence,
    // 09C0:18F1(0x12), is not decoded)
    std::vector<fighter> fighters;
    for (size_t i = 0; i < fParty->members.size(); i++)
        fighters.push_back(view.FigureFighter(int(i)));
    AfterBattle(*fParty, fighters);
    if (fParty->members.empty()) {
        fPartyLost = true;
        return;
    }
    // the winners loot the bodies ("Loot Bodies" in the battle's menu)
    if (outcome == BATTLE_WON) {
        fLoot = view.Loot();
        fTrade.SetPlace(fCity, _Reputation());
        fTrade.SetLoot(&fLoot, money{ 0, 0, 0 });
        fTrade.Run(window);
        fLoot.clear();
    }
    ResolveBattle(outcome);
}


// file 0xBF43C: won, card 8 then on as before; lost, card 11 (the
// dungeon); fled, card 10
void
CityVisit::ResolveBattle(int outcome)
{
    fPendingBattle = false;
    if (fBattleKind == BATTLE_WITH_GATE_GUARDS) {
        _Show(_ResolveGuardBattle(outcome));
        return;
    }
    if (fBattleKind == BATTLE_WITH_JAIL_GUARDS) {
        _Show(_ResolveJailBattle(outcome));
        return;
    }
    if (fBattleKind == BATTLE_AT_EXECUTION) {
        _Show(_ResolveExecutionBattle(outcome));
        return;
    }
    if (fBattleKind == BATTLE_WITH_PURSUERS) {
        _Show(_ResolveChaseBattle(outcome));
        return;
    }
    if (fBattleKind == BATTLE_AT_GATE) {
        _Show(_ResolveGateBattle(outcome));
        return;
    }
    if (fBattleKind == BATTLE_WITH_KNIGHT) {
        _Show(_ResolveKnightBattle(outcome));
        return;
    }
    if (fBattleKind == BATTLE_WITH_KNIGHTS_MEN) {
        _Show(_ResolveMenBattle(outcome));
        return;
    }
    if (fBattleKind == BATTLE_WITH_THIEVES) {
        _Show(_ResolveThievesBattle(outcome));
        return;
    }
    switch (outcome) {
        case BATTLE_WON:
            _Show(SCREEN_WATCH_BEATEN);
            break;
        case BATTLE_LOST:
            _Show(SCREEN_WATCH_PRISON);
            break;
        default:
            _Show(SCREEN_WATCH_RETREAT);
            break;
    }
}


// The guards recognize the party (state 1, file 0x914FE): from where it
// stands (DS:A88D, the previous state), an hour passes; the reputation
// then decides how long the party stays wanted after a fight
int
CityVisit::_Challenge(int from)
{
    if (from < 0)
        from = fScreen;
    fChallengeReturn = from == SCREEN_DAY_GATE_GUARDED ? SCREEN_DAY_GATE
        : from;
    fChallengeReputation = _Reputation();
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_CHALLENGE;
}


// The guards' price (file 0x9156C): city size / 2 florins, + |reputation
// / 20| for a negative reputation, at least one
uint32
CityVisit::_ChallengeBribe() const
{
    const int size = _City().size;
    int florins = size / 2;
    if (_Reputation() < 0)
        florins += std::abs(_Reputation() / 20);
    return uint32(std::max(1, florins)) * 240;
}


// Talking's chance (file 0x91978): the leader's Speak Common + Charisma
// + the reputation within 0..100 (1367:0028); 25 less while mark 0x13
// (a fight at the gate lately) is on
int
CityVisit::_ChallengeTalkChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[size_t(fParty->leader)];
    int chance = leader.skills[kSkillSpeakCommon]
        + leader.attributes[ATTRIBUTE_CHARISMA] + _Reputation();
    if (_Marked(kMarkGateFought))
        chance -= 25;
    return std::max(0, std::min(100, chance));
}


// The bribe's chance (file 0x91A6C): the reputation + the leader's
// Charisma + the bribe in groschen, within 0..100
int
CityVisit::_ChallengeBribeChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[size_t(fParty->leader)];
    const int chance = _Reputation() + leader.attributes[ATTRIBUTE_CHARISMA]
        + int(_ChallengeBribe() / 12);
    return std::max(0, std::min(100, chance));
}


// Talking (file 0x91838): if random(100) is at most the chance, a lesson
// in Speak Common for the leader (1462:0132 mode 1), card 5, 6 or 7 at
// random, an hour, back where the party was; card 7 names the leader's
// weapon (09C0:1E9B, 0E76:1FB8: not decoded, taken as the weapon in hand;
// without one card 5 or 6). Else card 8 and the fight (the leader's
// lesson of mode 0 is not reproduced).
int
CityVisit::_TalkToGuards()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (fParty == NULL || fParty->members.empty()
            || random(100) > _ChallengeTalkChance())
        return SCREEN_CHALLENGE_TALK_FAILED;
    character& leader = fParty->members[size_t(fParty->leader)];
    TrainSkill(leader, kSkillSpeakCommon, 10, random);
    if (fClock != NULL)
        fClock->AddHours(1);
    const int pick = random(3);
    if (pick == 0)
        return SCREEN_CHALLENGE_DECOYED;
    if (pick == 1)
        return SCREEN_CHALLENGE_BLUFFED;
    const int weapon = leader.equipment[EQUIPMENT_WEAPON];
    const std::vector<item_definition>& items = fData.Lists().Items();
    for (size_t code = 0; weapon != kNoEquipment && code < items.size();
            code++) {
        if (!items[code].name.empty() && items[code].type == weapon) {
            fVariables["NamedOneName"] = items[code].name;
            return SCREEN_CHALLENGE_COWED;
        }
    }
    return random(2) == 0 ? SCREEN_CHALLENGE_BLUFFED : SCREEN_CHALLENGE_DECOYED;
}


// Bribing (file 0x919E0): as the game has it, the guards take the offer
// (card 9, an hour, back where the party was) only if random(100) is at
// least the chance, and the purse stays as it was; else card 10 and the
// fight
int
CityVisit::_BribeChallenge()
{
    if (int(fRandom() % 100) < _ChallengeBribeChance())
        return SCREEN_CHALLENGE_REFUSED;
    if (fClock != NULL)
        fClock->AddHours(1);
    return SCREEN_CHALLENGE_BRIBED;
}


// Fighting the guards (file 0x92370): clamp(the party's size (09C0:2161,
// 1462:1DE6), 7, random(7) + 2) of enemy 3 at variant 1 and the
// sergeant (enemy 0) at variant 2; the reputation falls by 15..24 with a
// chance of 100 - |reputation| % (0E76:19D0(location, -15, -25))
void
CityVisit::_FightGuards()
{
    const int size = fParty != NULL ? int(fParty->members.size()) : 1;
    const int guards = std::max(size, std::min(7, int(fRandom() % 7) + 2));
    _ChangeReputation(-24, -15);
    fBattleKind = BATTLE_WITH_GATE_GUARDS;
    fFoes.clear();
    fFoes.push_back(foes{ 3, 1, guards });
    fFoes.push_back(foes{ 0, 2, 1 });
    fPendingBattle = true;
}


// The guards' battle's result (file 0x9241E): won, card 1, the guards
// nervous (mark 0x12) for 2000 / city size hours; retreated, card 3
// and an hour (0E76:23E2 not decoded); lost, card 4 and three hours,
// then the dungeon (state 0xD). The party's result 1, card 2 ("you cut
// your way through", an hour, the side streets), has no BattleView
// outcome. Then the party is wanted (mark 0x11) for 120 hours, 240 with
// a reputation of -75 or less when the guards came.
int
CityVisit::_ResolveGuardBattle(int outcome)
{
    const int size = _City().size;
    int next = SCREEN_CHALLENGE_FLED;
    int hours = 1;
    if (outcome == BATTLE_WON) {
        _Mark(kMarkAlert, uint32(2000 / std::max(1, size)));
        next = SCREEN_CHALLENGE_WON;
        hours = 0;
    } else if (outcome == BATTLE_LOST) {
        next = SCREEN_CHALLENGE_ARRESTED;
        hours = 3;
    }
    if (fClock != NULL)
        fClock->AddHours(uint32(hours));
    _Mark(kMarkWanted, fChallengeReputation <= -75 ? 240 : 120);
    return next;
}


// The chase (state 0x7A, file 0xF2112): no time on entry; the
// reputation then decides how long the party stays wanted after a fight
int
CityVisit::_EnterChase()
{
    fChallengeReputation = _Reputation();
    return SCREEN_CHASE;
}


// Running's value (file 0xF238A): three times the slowest's speed
// (0E76:0656; the speed is the agility, the load is not kept)
int
CityVisit::_ChaseRunChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    return 3 * fParty->members[size_t(_Slowest())].attributes[ATTRIBUTE_AGILITY];
}


// The ambush's value (file 0xF249C): the leader's Streetwise + Stealth
// within 0..100
int
CityVisit::_AmbushChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[size_t(fParty->leader)];
    return std::min(100, leader.skills[kSkillStreetwise]
        + leader.skills[kSkillStealth]);
}


// Hiding's value (file 0xF25C4): the lowest Stealth (from 99), + 20
// outside the game's day, + 3 · the city's size, within 0..100
int
CityVisit::_HideChance() const
{
    int stealth = 99;
    for (int i = 0; fParty != NULL && i < int(fParty->members.size()); i++)
        stealth = std::min(stealth, int(fParty->members[size_t(i)].skills[kSkillStealth]));
    if (fClock != NULL && !IsGameDay(*fClock))
        stealth += 20;
    const int size = _City().size;
    return std::max(0, std::min(100, stealth + 3 * size));
}


// Running (file 0xF22D0). As the game has it, the party gets away only
// if random(100) is at least the value: random(4) hours, card 15, a
// lesson in Streetwise for all (1462:0132(-2, 16, 1, 5)), the side
// streets; else card 14, an hour and the fight.
int
CityVisit::_OutrunGuards()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (random(100) < _ChaseRunChance()) {
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_OVERTAKEN;
    }
    if (fClock != NULL)
        fClock->AddHours(uint32(random(4)));
    if (fParty != NULL)
        TrainParty(*fParty, kSkillStreetwise, 1, 5, random);
    return SCREEN_OUTRUN;
}


// The ambush (file 0xF23F0), the same way round: at least the value,
// lessons in Stealth and Streetwise for the leader (mode 1, 7) and card
// 12; else card 11 (the leader sneezes: the hiding's card, as the game
// has it) and lessons of mode 0 (not reproduced). The fight either way.
int
CityVisit::_Ambush()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_CHASE;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (random(100) < _AmbushChance()) {
        _SetChosen(fParty->leader);
        return SCREEN_HIDING_FOUND;
    }
    character& leader = fParty->members[size_t(fParty->leader)];
    TrainSkill(leader, kSkillStealth, 7, random);
    TrainSkill(leader, kSkillStreetwise, 7, random);
    return SCREEN_AMBUSH;
}


// Hiding (file 0xF24E4), the same way round: at least the value, a
// lesson in Stealth for all (mode 1, 10), and by day a wait until 19
// o'clock (card 9), at night until 5 (card 10), then the side streets;
// else card 11 (the leader sneezes), a lesson of mode 0 and the fight
int
CityVisit::_Hide()
{
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (random(100) < _HideChance()) {
        if (fParty != NULL && !fParty->members.empty())
            _SetChosen(fParty->leader);
        return SCREEN_HIDING_FOUND;
    }
    if (fParty != NULL)
        TrainParty(*fParty, kSkillStealth, 1, 10, random);
    const bool day = fClock == NULL || IsGameDay(*fClock);
    if (fClock != NULL) {
        const int until = day ? 19 : 5;
        const int hour = fClock->Hour();
        fClock->AddHours(uint32(hour > until ? until - hour + 24
            : until - hour));
    }
    return day ? SCREEN_HIDDEN_TILL_NIGHT : SCREEN_HIDDEN_TILL_DAWN;
}


// The fight (file 0xF2A40): random(5) + 3 of enemy 3 at variant 2 and
// random(4) + 3 of enemy 0 ("Sergeant") at variant 2; the reputation
// -1..-5 (0E76:19D0)
void
CityVisit::_FightPursuers()
{
    _ChangeReputation(-5, -1);
    fBattleKind = BATTLE_WITH_PURSUERS;
    fFoes.clear();
    fFoes.push_back(foes{ 3, 2, int(fRandom() % 5) + 3 });
    fFoes.push_back(foes{ 0, 2, int(fRandom() % 4) + 3 });
    fPendingBattle = true;
}


// Its result (file 0xF2AAB): won, card 1 and the guards nervous for
// 2000 / size hours (0E76:2D5C(-2, location, 0x1E, 0x12, ...),
// inferred); fled, card 3 and an hour; both then the side streets;
// lost, card 4, three hours and the dungeon. Result 1 (card 2, "you cut
// your way through") has no BattleView outcome. Then the party is
// wanted (mark 0x11) for 120 hours, 240 at -75 or less.
int
CityVisit::_ResolveChaseBattle(int outcome)
{
    const int size = _City().size;
    int next = SCREEN_CHASE_FLED;
    int hours = 1;
    if (outcome == BATTLE_WON) {
        _Mark(kMarkAlert, uint32(2000 / std::max(1, size)));
        next = SCREEN_CHASE_WON;
        hours = 0;
    } else if (outcome == BATTLE_LOST) {
        next = SCREEN_CHASE_CAUGHT;
        hours = 3;
    }
    if (fClock != NULL)
        fClock->AddHours(uint32(hours));
    _Mark(kMarkWanted, fChallengeReputation <= -75 ? 240 : 120);
    return next;
}


// Walking at night unseen (file 0xA4596): the party's average Stealth
// (0E76:1600) + the best Streetwise - the hazard 1462:0000(15, 1, 5),
// within 1..99
int
CityVisit::_NightWalkChance()
{
    if (fParty == NULL || fParty->members.empty())
        return 1;
    int stealth = 0;
    for (const character& member : fParty->members)
        stealth += member.skills[kSkillStealth];
    stealth /= int(fParty->members.size());
    return std::max(1, std::min(stealth + _BestSkill(kSkillStreetwise)
        - _Hazard(15, 5), 99));
}


// The walks from the square at night (file 0xA3C4A, a handler and a chance
// for each option; the chances at 0xA3E48...): the chance starts at 100,
// each member in turn brings it down to his Stealth if lower, then adds
// a number by the place (25 the notices, 28 the town hall, 30 the barracks,
// 50 the university, 45 the main street, 65 the side street), and it is 20
// less (35 for the barracks) while the party is watched (mark 0x14, or
// 09C0:2107 = 1: mark 0x13 without 0x12), within 0..99. If random(100)
// is at most the chance, the place (no time for the hall and the barracks,
// an hour for the others; the notices, state 0x6D, come back here); else
// mark 0x14 for 32 hours (0E76:2D5C) and the watch, which leads back
// here. `target`: SCREEN_NEWS for the notices
int
CityVisit::_SquareSneak(int target)
{
    int add = 25;
    int penalty = 20;
    int hours = 0;
    switch (target) {
        case SCREEN_TOWN_HALL:		add = 28; break;
        case SCREEN_BARRACKS:		add = 30; penalty = 35; break;
        case SCREEN_UNIVERSITY:		add = 50; hours = 1; break;
        case SCREEN_MAIN_STREET:	add = 45; hours = 1; break;
        case SCREEN_SIDE_STREET:	add = 65; hours = 1; break;
        default:					break;
    }
    int chance = 100;
    if (fParty != NULL) {
        for (const character& member : fParty->members) {
            chance = std::min(chance, int(member.skills[kSkillStealth]));
            chance += add;
        }
    }
    const bool nervous = _Marked(kMarkGateFought) && !_Marked(kMarkAlert);
    if (nervous || _Marked(kMarkSquareWatched))
        chance -= penalty;
    chance = std::max(0, std::min(chance, 99));
    if (int(fRandom() % 100) > chance) {
        _Mark(kMarkSquareWatched, 32);
        fWatchReturn = SCREEN_SQUARE;
        return SCREEN_NIGHT_WATCH;
    }
    if (fClock != NULL && hours > 0)
        fClock->AddHours(uint32(hours));
    if (target == SCREEN_NEWS) {
        fNoticesFromSquare = true;
        return _Notices();
    }
    return target;
}


// A hazard (1462:0000 of overlay 0x27, file 0x809A0; its second argument
// is not used): `high` raised by the city's state (1: 5 / 4, 2: 6 / 4)
// and by each of marks 0x13 and 0x12 (6 / 5), then random(high - low)
// within low..high
int
CityVisit::_Hazard(int high, int low)
{
    const uint8 state = _CityState();
    if (state == 1)
        high = high * 5 / 4;
    else if (state == 2)
        high = high * 6 / 4;
    if (_Marked(kMarkGateFought))
        high = high * 6 / 5;
    if (_Marked(kMarkAlert))
        high = high * 6 / 5;
    const int value = high > low ? int(fRandom() % uint32(high - low)) : 0;
    return std::max(low, std::min(value, high));
}


// A party the guards are after (1462:00BA of overlay 0x27, 09C0:20F3):
// wanted (mark 0x11), a reputation of -75 or less, or of -10 or less
// after a fight at the gate (mark 0x13)
bool
CityVisit::_Wanted() const
{
    const int reputation = _Reputation();
    return _Marked(kMarkWanted) || reputation <= -75
        || (_Marked(kMarkGateFought) && reputation <= -10);
}


// The way to the grove from the streets (file 0x956CC and 0x96BCF by
// day, 0x96296 and 0x974B6 at night): with h a hazard drawn for the
// street, the way fails if random(100) >= 100 - h. By day only a wanted
// party runs a risk (main street h(35, 20), side streets h(15, 2)): the
// guards' challenge. At night (main street h(14, 4); side streets an
// hour first, then h(8, 1)) the watch stops the party, card 1, and
// paying its fine leads to the grove. The crafts' way has no risk.
int
CityVisit::_ToGrove()
{
    const bool mainStreet = fScreen == SCREEN_MAIN_STREET;
    int hazard = 0;
    if (!fNight) {
        if (_Wanted())
            hazard = mainStreet ? _Hazard(35, 20) : _Hazard(15, 2);
    } else {
        if (!mainStreet && fClock != NULL)
            fClock->AddHours(1);
        hazard = mainStreet ? _Hazard(14, 4) : _Hazard(8, 1);
    }
    if (hazard == 0 || int(fRandom() % 100) < 100 - hazard)
        return SCREEN_GROVE;
    if (!fNight)
        return _Challenge();
    fWatchReturn = SCREEN_GROVE;
    return SCREEN_NIGHT_WATCH_MARKET;
}

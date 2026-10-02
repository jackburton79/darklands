// The dungeon, the court and the execution, and the priest

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


// Entering the dungeon (file 0x988C6): the guards search the party
// (18E7:0854(-2)), the best cell, no tunnel yet
int
CityVisit::_EnterPrison()
{
    _Search();
    fCell = 0;
    fTunnel = 0;
    return SCREEN_CELL;
}


// The search (18E7:0854(-2), file 0x66484): the purse is emptied (not when
// only one member is searched, *inferred*); each
// item goes if its quantity · weight is over 2, else if random(100) is
// under h / 2 (2) or h (less), h = (Agility + Stealth) / 2 of its
// owner. As the game has it, the nimbler lose more. What is gone is no
// longer in use.
void
CityVisit::_Search(int who)
{
    if (fParty == NULL)
        return;
    if (who < 0)
        fParty->cash = money{ 0, 0, 0 };
    for (size_t index = 0; index < fParty->members.size(); index++) {
        if (who >= 0 && int(index) != who)
            continue;
        character& member = fParty->members[index];
        const int h = (member.attributes[ATTRIBUTE_AGILITY]
            + member.skills[kSkillStealth]) / 2;
        std::vector<item> kept;
        for (const item& carried : member.items) {
            const int bulk = carried.quantity * carried.weight;
            const int roll = int(fRandom() % 100);
            const bool taken = bulk > 2 || (bulk == 2 ? roll < h / 2
                : roll < h);
            if (!taken)
                kept.push_back(carried);
        }
        member.items = kept;
        for (uint8& slot : member.equipment) {
            bool left = false;
            for (const item& carried : member.items)
                left = left || carried.type == slot;
            if (!left)
                slot = kNoEquipment;
        }
    }
}


int
CityVisit::_CellScreen() const
{
    return SCREEN_CELL + std::max(0, std::min(fCell, 3));
}


// Caught trying to escape: from the best cell to the dark one, else to
// the oubliette; the tunnel is lost
void
CityVisit::_WorseCell()
{
    fCell = fCell == 0 ? 1 : 2;
    fTunnel = 0;
}


// A sound beating (1462:026A(-2, 0, 1, 20)): a fall of amount 20 for all
void
CityVisit::_Beating()
{
    if (fParty == NULL)
        return;
    for (int i = 0; i < int(fParty->members.size()); i++)
        _Fall(i, 20);
}


// A flogging (file 0xFB8F8): every member loses random(18) Endurance and
// random(12) Strength (0E76:0A72(-2, ...))
void
CityVisit::_Flogging()
{
    if (fParty == NULL)
        return;
    for (character& member : fParty->members) {
        AddToAttribute(member, ATTRIBUTE_ENDURANCE, -int(fRandom() % 18));
        AddToAttribute(member, ATTRIBUTE_STRENGTH, -int(fRandom() % 12));
    }
}


// An item of DARKLAND.LST for every member (18E7:0128(-2, code)), at its
// default quality; a weapon is taken in hand when the hand is empty
// (inferred)
void
CityVisit::_GiveEach(int code)
{
    if (fParty == NULL)
        return;
    for (character& member : fParty->members)
        _GiveTo(member, code);
}


// One item for a member (18E7:0128(member, code)); a weapon is taken in
// hand when the hand is empty (inferred)
void
CityVisit::_GiveTo(character& member, int code)
{
    const item_definition& definition = fData.Lists().Items()[size_t(code)];
    member.items.push_back(item{ uint16(code), uint8(definition.type),
        definition.quality, 1, definition.weight });
    if ((definition.flags & (ITEM_EDGED | ITEM_IMPACT | ITEM_POLEARM
            | ITEM_FLAIL)) != 0
            && member.equipment[EQUIPMENT_WEAPON] == kNoEquipment)
        member.equipment[EQUIPMENT_WEAPON] = uint8(definition.type);
}


// The member best at Artifice (0E76:14A4(14): it gives the skill and
// leaves the member's number in DS:991D), the first of equals
int
CityVisit::_Picker() const
{
    int best = 0;
    for (size_t i = 1; fParty != NULL && i < fParty->members.size(); i++) {
        if (fParty->members[i].skills[kSkillArtifice]
                > fParty->members[size_t(best)].skills[kSkillArtifice])
            best = int(i);
    }
    return best;
}


// Picking the lock's chance (file 0x98D36): the picker's Artifice, + 50
// in the dark cell, - 50 in Saint Lucy's light, none in the oubliette,
// within 0..99 (1367:000A)
int
CityVisit::_PickChance() const
{
    if (fParty == NULL || fParty->members.empty() || fCell == 2)
        return 0;
    int chance = fParty->members[size_t(_Picker())].skills[kSkillArtifice];
    if (fCell == 1)
        chance += 50;
    else if (fCell == 3)
        chance -= 50;
    return std::max(0, std::min(99, chance));
}


// The climber (0E76:179C(2, 1)): as the game has it, a member whose
// Agility + Strength is over the best score so far becomes the climber,
// and the score becomes twice his Agility
int
CityVisit::_Climber(int* score) const
{
    int best = 0;
    int climber = 0;
    for (int i = 0; fParty != NULL && i < int(fParty->members.size()); i++) {
        const character& member = fParty->members[size_t(i)];
        if (member.attributes[ATTRIBUTE_AGILITY]
                + member.attributes[ATTRIBUTE_STRENGTH] > best) {
            best = 2 * member.attributes[ATTRIBUTE_AGILITY];
            climber = i;
        }
    }
    if (score != NULL)
        *score = best;
    return climber;
}


// The woman with the best Charisma (0E76:174A(5)), or -1
int
CityVisit::_Seductress() const
{
    int best = -1;
    for (int i = 0; fParty != NULL && i < int(fParty->members.size()); i++) {
        const character& member = fParty->members[size_t(i)];
        if (member.female && (best < 0 || member.attributes[ATTRIBUTE_CHARISMA]
                > fParty->members[size_t(best)].attributes[ATTRIBUTE_CHARISMA]))
            best = i;
    }
    return best;
}


// Picking the lock (file 0x98C1E): if random(100) is at most the chance,
// a lesson in Artifice for the picker (mode 1), an hour, a dagger for
// every member (18E7:0128(-2, 7)), card 6 and the guardroom; else card
// 5, a beating, three hours, the lockpicks taken (18E7:0668(-2, 64)), a
// lesson of mode 0 (not reproduced) and a worse cell
int
CityVisit::_PickLock()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_CELL;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    const int picker = _Picker();
    _SetChosen(picker);
    if (random(100) <= _PickChance()) {
        TrainSkill(fParty->members[size_t(picker)], kSkillArtifice, 10,
            random);
        if (fClock != NULL)
            fClock->AddHours(1);
        _GiveEach(kDaggerCode);
        return SCREEN_LOCK_PICKED;
    }
    _Beating();
    if (fClock != NULL)
        fClock->AddHours(3);
    for (character& member : fParty->members) {
        std::vector<item> kept;
        for (const item& carried : member.items) {
            if ((carried.code & 0x0FFF) != kLockpickCode)
                kept.push_back(carried);
        }
        member.items = kept;
    }
    _WorseCell();
    return SCREEN_PICK_CAUGHT;
}


// Climbing to the window (file 0x98DAA; the best cell only): an hour;
// if random(100) is at most the climber's score / 3 (file 0x98E86), card
// 10, a lesson in Stealth for the climber (mode 1), a club for every
// member and the chase (state 0x7A); else card 11 and a worse cell
int
CityVisit::_ClimbWindow()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_CELL;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (fClock != NULL)
        fClock->AddHours(1);
    int score = 0;
    const int climber = _Climber(&score);
    _SetChosen(climber);
    if (random(100) <= std::min(100, score / 3)) {
        TrainSkill(fParty->members[size_t(climber)], kSkillStealth, 10,
            random);
        _GiveEach(kClubCode);
        return SCREEN_WINDOW_ESCAPED;
    }
    _WorseCell();
    return SCREEN_WINDOW_CAUGHT;
}


// Digging (file 0x98EBE): 12 hours; after 12 o'clock the magistrate
// sends for the party one time in nine (card 16). Else the tunnel grows
// by 12..17 % (50 in Saint Lucy's light): over 95, card 12, the
// reputation down by 1..6 (0E76:19D0) and the side streets; else found if
// random(100) is under a quarter of it (card 13, a beating, the tunnel
// lost, a worse cell: the light's cell is the oubliette's); else card 22
int
CityVisit::_Dig()
{
    if (fClock != NULL) {
        fClock->AddHours(12);
        if (fClock->Hour() > 12 && fRandom() % 9 == 1)
            return SCREEN_TO_MAGISTRATE;
    }
    fTunnel += fCell == 3 ? 50 : int(fRandom() % 6) + 12;
    if (fTunnel > 95) {
        fTunnel = 0;
        _ChangeReputation(-6, -1);
        return SCREEN_TUNNEL_DONE;
    }
    if (int(fRandom() % 100) < fTunnel / 4) {
        _Beating();
        fTunnel = 0;
        fCell = fCell == 3 ? 2 : fCell + 1;
        return SCREEN_TUNNEL_FOUND;
    }
    fVariables["Number1"] = std::to_string(fTunnel);
    return SCREEN_TUNNEL_PROGRESS;
}


// Seducing the turnkey (file 0x9909A; a woman in the best cell): if
// random(100) is at most her Charisma, two days, card 14, a lesson in
// Speak Common for her (mode 1), her Virtue down by 2, or 2..5 with a
// chance of 100 - Virtue % (0E76:18A8(virtue, 2, 6)), and the side
// streets; else two hours and card 15
int
CityVisit::_Seduce()
{
    const int woman = _Seductress();
    if (woman < 0)
        return SCREEN_CELL;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    character& her = fParty->members[size_t(woman)];
    _SetChosen(woman);
    if (random(100) > her.attributes[ATTRIBUTE_CHARISMA]) {
        if (fClock != NULL)
            fClock->AddHours(2);
        return SCREEN_SCOFFED;
    }
    if (fClock != NULL)
        fClock->AddHours(48);
    TrainSkill(her, kSkillSpeakCommon, 10, random);
    const int virtue = her.skills[kSkillVirtue];
    const int loss = random(100) <= std::abs(100 - virtue) ? 2 + random(4) : 2;
    her.skills[kSkillVirtue] = uint8(std::max(0, virtue - loss));
    return SCREEN_SEDUCED;
}


// Praying (file 0x99220): every member's divine favor + 2..11 (12..21 in
// Saint Lucy's light), card 17, 12 hours; then after 12 o'clock the
// magistrate one time in ten
int
CityVisit::_Pray()
{
    if (fParty != NULL) {
        for (character& member : fParty->members) {
            AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR,
                int(fRandom() % 10) + (fCell == 3 ? 12 : 2));
        }
    }
    fMagistrateComing = false;
    if (fClock != NULL) {
        fClock->AddHours(12);
        fMagistrateComing = fClock->Hour() > 12 && fRandom() % 10 == 1;
    }
    return SCREEN_PRAYED;
}


// Waiting (file 0x995A2): until 13 o'clock (1367:0716), then the
// magistrate if random(100) is at most 11
int
CityVisit::_WaitForMagistrate()
{
    if (fClock != NULL) {
        const int hour = fClock->Hour();
        fClock->AddHours(uint32(hour > 13 ? 13 - hour + 24 : 13 - hour));
    }
    return fRandom() % 100 <= 11 ? SCREEN_TO_MAGISTRATE : SCREEN_CELL;
}


// The guardroom (file 0x999C2): random(5) + 4 of enemy 3 at variant
// random(3) + |s| / 4 + 1 and the sergeant at variant random(3) + 1; s
// (09C0:1C1B, 1462:0470) is a measure of the party's strength, not
// reproduced (0)
void
CityVisit::_FightJailGuards()
{
    fBattleKind = BATTLE_WITH_JAIL_GUARDS;
    fFoes.clear();
    const int variant = int(fRandom() % 3) + 1;
    fFoes.push_back(foes{ 3, variant, int(fRandom() % 5) + 4 });
    fFoes.push_back(foes{ 0, int(fRandom() % 3) + 1, 1 });
    fPendingBattle = true;
}


// The guardroom's result (file 0x99A1B): won or fled, two hours, card 8
// and the chase; lost, two hours,
// card 9, the search again, a beating, the same cell
int
CityVisit::_ResolveJailBattle(int outcome)
{
    if (fClock != NULL)
        fClock->AddHours(2);
    if (outcome != BATTLE_LOST)
        return SCREEN_GUARDROOM_WON;
    _Search();
    _Beating();
    return SCREEN_RECAPTURED;
}


// Before the magistrate (file 0xFB4A0): no torture yet
int
CityVisit::_EnterCourt()
{
    fTortures = 0;
    return SCREEN_MAGISTRATE;
}


// Saying nothing (file 0xFB61C): three times the torture (card 1, six
// hours, every member loses random(18) Endurance and random(12)
// Strength); the fourth time card 4, an hour, free in the square
int
CityVisit::_KeepSilent()
{
    if (fTortures >= 3) {
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_UNPLEADED;
    }
    if (fClock != NULL)
        fClock->AddHours(6);
    _Flogging();
    fTortures++;
    return SCREEN_MAGISTRATE_AGAIN;
}


// Pleading (file 0xFB74C) or confessing (file 0xFBB02): the wanted mark
// is lifted (0E76:3CDE(0x11, ...), inferred); s = random(5) - 3 (random(3)
// - 2 confessing) + the reputation / 40 (+ 1462's 0xFBEA4, always 0):
// under 0 death (card 5, three hours, the execution), 0 a flogging (card
// 6, three hours), 1 a fine; pleading, s under -6 or over 1 acquits (card
// 9, an hour); then the square
int
CityVisit::_Plead(bool guilty)
{
    fMarks.erase(std::make_pair(kMarkWanted, fCity));
    const int s = guilty ? int(fRandom() % 3) - 2 + _Reputation() / 40
        : int(fRandom() % 5) - 3 + _Reputation() / 40;
    if (!guilty && (s < -6 || s > 1)) {
        if (fClock != NULL)
            fClock->AddHours(1);
        return SCREEN_ACQUITTED;
    }
    if (guilty && (s < -5 || s > 5))
        return SCREEN_MAGISTRATE;		// cannot happen: nothing
    if (s < 0) {
        if (fClock != NULL)
            fClock->AddHours(3);
        return SCREEN_SENTENCED;
    }
    if (s == 0) {
        if (fClock != NULL)
            fClock->AddHours(3);
        _Flogging();
        return SCREEN_FLOGGED;
    }
    return _CourtFine();
}


// The fine (file 0xFB85A): random(3) + city size / 3 florins, at least
// one ($Money1); with that many florins in the purse, a day and card 7,
// else three hours, card 8 and a flogging. As the game has it, the fine
// is not taken.
int
CityVisit::_CourtFine()
{
    const int size = _City().size;
    const int florins = std::max(1, int(fRandom() % 3) + size / 3);
    fVariables["Money1"] = MoneyText(uint32(florins) * 240);
    if (fParty != NULL && florins <= fParty->cash.florins) {
        if (fClock != NULL)
            fClock->AddHours(24);
        return SCREEN_FINED;
    }
    if (fClock != NULL)
        fClock->AddHours(3);
    _Flogging();
    return SCREEN_FINED_FLOGGED;
}


// The rescues (1838:0CF6(saint), file 0xFC196; -1 without a saint),
// each tried in turn with random(n), n = 50 for St. Jude (0x50), else
// 100: the ruler's pardon if at most the reputation / 10 (card 7, the
// reputation set to -9, the square); the abbot if at most the best
// Virtue + Religion + Charisma + Speak Latin / 10 (card 8, the church;
// with an event of kind 2 here, 0E76:360C, the game starts from an
// uninitialized word: no events are kept here); the bankers if
// at most the florins in the purse (card 9, five more hours, the
// reputation -9, the square); the mob if at most |reputation / 5| (card
// 10, a weapon each by the best weapon skill, the fight). Two more, on
// DS:9082 (the city ruler's quest, state 0x84), are not reproduced. If
// none comes, a saint's answer decides (St. Alcuin: the pardon or the
// abbot; St. John Nepomuk: the pardon; St. Jude: any of the six, the
// quest not implemented); without one a member is beheaded (card 1)
// and the execution goes on. An hour passes first.
int
CityVisit::_Rescue(int saint)
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_EXECUTION;
    const int reputation = _Reputation();
    int best = 0;
    for (const character& member : fParty->members) {
        best = std::max(best, member.skills[kSkillVirtue]
            + member.skills[kSkillReligion]
            + member.attributes[ATTRIBUTE_CHARISMA]
            + member.skills[kSkillSpeakLatin]);
    }
    const uint32 n = saint == 80 ? 50 : 100;
    int rescue = 6;
    if (int(fRandom() % n) <= reputation / 10)
        rescue = 0;
    else if (int(fRandom() % n) <= best / 10)
        rescue = 1;
    else if (int(fRandom() % n) <= fParty->cash.florins)
        rescue = 2;
    else if (int(fRandom() % n) <= std::abs(reputation / 5))
        rescue = 3;
    else if (saint == 5)
        rescue = int(fRandom() % 2);
    else if (saint == 78)
        rescue = 0;
    else if (saint == 80)
        rescue = int(fRandom() % 6);
    if (fClock != NULL)
        fClock->AddHours(1);
    switch (rescue) {
        case 0:
            if (fReputations != NULL && fCity < int(fReputations->size()))
                (*fReputations)[fCity] = -9;
            return SCREEN_PARDONED;
        case 1:
            return SCREEN_CLAIMED_BY_ABBOT;
        case 2:
            if (fClock != NULL)
                fClock->AddHours(5);
            if (fReputations != NULL && fCity < int(fReputations->size()))
                (*fReputations)[fCity] = -9;
            return SCREEN_BOUGHT_OFF;
        case 3: {
            // the weapon of the best weapon skill (0E76:01C0, file
            // 0xFC46A): a falchion (edged, bows, missiles), a mace
            // (impact, flails), a short spear (polearms, thrown)
            static const int kWeapons[kWeaponSkillCount]
                = { 4, 14, 14, 21, 21, 4, 4 };
            for (character& member : fParty->members) {
                int skill = 0;
                for (int s = 1; s < kWeaponSkillCount; s++) {
                    if (member.skills[s] > member.skills[skill])
                        skill = s;
                }
                _GiveTo(member, kWeapons[skill]);
            }
            return SCREEN_MOB;
        }
        case 4:
        case 5:
            // the city ruler's quest (state 0x84): not implemented
            fPreviousScreen = SCREEN_SQUARE;
            return SCREEN_NOT_IMPLEMENTED;
        default:
            break;
    }
    // file 0xFC4CE: a member at random (the next one standing)
    const int victim = int(fRandom() % fParty->members.size());
    _SetChosen(victim);
    RemoveMember(*fParty, size_t(victim));
    return SCREEN_BEHEADED;
}


// Breaking the ropes (file 0xFC5BC): if random(100) is at most the
// strongest's Strength (0E76:16FE(1)), card 6, a dagger each and the
// fight; else card 11 with a member at random, then the rescues
int
CityVisit::_BreakRopes()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_EXECUTION;
    const int strongest = _Strongest();
    if (int(fRandom() % 100)
            <= fParty->members[size_t(strongest)].attributes[ATTRIBUTE_STRENGTH]) {
        _SetChosen(strongest);
        _GiveEach(kDaggerCode);
        return SCREEN_ROPES_BROKEN;
    }
    _SetChosen(int(fRandom() % fParty->members.size()));
    return SCREEN_ROPES_HOLD;
}


// The fight at the execution (file 0xFC004): random(4) + s / 3 + 1 of
// enemy 3 at variant random(2) + |s| / 4 + 1 and enemy 23, the
// "Executioner", at variant s % 3 + 1 (s: see _FightJailGuards(), 0
// here); the reputation falls by 5..19 (0E76:1DFE)
void
CityVisit::_FightAtExecution()
{
    if (fReputations != NULL && fCity >= 0
            && fCity < int(fReputations->size())) {
        int16& reputation = (*fReputations)[fCity];
        reputation = int16(std::max(-99, reputation - 5
            - int(fRandom() % 15)));
    }
    fBattleKind = BATTLE_AT_EXECUTION;
    fFoes.clear();
    const int variant = int(fRandom() % 2) + 1;
    fFoes.push_back(foes{ 3, variant, int(fRandom() % 4) + 1 });
    fFoes.push_back(foes{ 23, 1, 1 });
    fPendingBattle = true;
}


// Its result (file 0xFC0A4): won or fled, card 12, an hour and the
// chase; lost, card 13, three hours and
// the block again. Then the party is wanted (mark 0x11) for 240 hours,
// 480 with a reputation of -75 or less.
int
CityVisit::_ResolveExecutionBattle(int outcome)
{
    const int next = outcome == BATTLE_LOST ? SCREEN_EXECUTION_RECAPTURED
        : SCREEN_EXECUTION_ESCAPED;
    if (fClock != NULL)
        fClock->AddHours(outcome == BATTLE_LOST ? 3 : 1);
    _Mark(kMarkWanted, _Reputation() <= -75 ? 480 : 240);
    return next;
}


// Asking the priest's help (file 0xF6A86): the leader's Virtue +
// Religion + Charisma + Speak Latin, / 10, + 20 while bit 0x40 of the
// location's byte +0x14 is set (location property 0x20; not kept here);
// 0 if not over 0, else within 1..99
int
CityVisit::_PriestChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[size_t(fParty->leader)];
    const int chance = (leader.skills[kSkillVirtue] + leader.skills[kSkillReligion]
        + leader.attributes[ATTRIBUTE_CHARISMA]
        + leader.skills[kSkillSpeakLatin]) / 10;
    return chance <= 0 ? 0 : std::max(1, std::min(99, chance));
}


// The confession (file 0xF68B4): a lesson in Virtue for the leader (mode
// 1), his divine favor + random(10) + the reputation / 20, three hours,
// card 1
int
CityVisit::_ConfessToPriest()
{
    if (fParty == NULL || fParty->members.empty())
        return _BackFromPriest();
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    character& leader = fParty->members[size_t(fParty->leader)];
    TrainSkill(leader, kSkillVirtue, 10, random);
    AddToAttribute(leader, ATTRIBUTE_DIVINE_FAVOR,
        random(10) + _Reputation() / 20);
    if (fClock != NULL)
        fClock->AddHours(3);
    return SCREEN_PRIEST_CONFESSION;
}


// Help to escape (file 0xF6970): if random(100) is at most the chance, a
// day passes; over 25 the Church claims the party (card 2, the church),
// else the leader gets lockpicks or, one time in two, an Eater Water
// ($NamedOneName; card 3), back to the cell. Else an hour, card 4 and
// the cell the party returns to is the oubliette from the best cell,
// the dark one from the others (as the game has it).
int
CityVisit::_AskPriestForHelp()
{
    if (fParty == NULL || fParty->members.empty())
        return _BackFromPriest();
    const int chance = _PriestChance();
    if (int(fRandom() % 100) > chance) {
        if (fClock != NULL)
            fClock->AddHours(1);
        fCell = fCell < 1 ? 2 : 1;
        return SCREEN_PRIEST_OUTRAGED;
    }
    if (fClock != NULL)
        fClock->AddHours(24);
    if (chance > 25)
        return SCREEN_PRIEST_RELEASED;
    static const int kEaterWaterCode = 99;
    const int code = fRandom() % 2 == 1 ? kLockpickCode : kEaterWaterCode;
    _GiveTo(fParty->members[size_t(fParty->leader)], code);
    fVariables["NamedOneName"] = fData.Lists().Items()[size_t(code)].name;
    return SCREEN_PRIEST_SMUGGLED;
}


// A good word with the magistrate (file 0xF6B2C): card 5, 12 hours, then
// after 12 o'clock the magistrate one time in ten (card 6). The game
// works out a score (Religion, Speak Latin, Virtue, the reputation) and
// never uses it.
int
CityVisit::_AskGoodWord()
{
    fMagistrateComing = false;
    if (fClock != NULL) {
        fClock->AddHours(12);
        fMagistrateComing = fClock->Hour() > 12 && fRandom() % 10 == 1;
    }
    return SCREEN_PRIEST_GOOD_WORD;
}


// Back in the dungeon (state 0xD) from state 0x83: the search again
// (the handler's entry, file 0x98953), the cell and the tunnel kept
int
CityVisit::_BackFromPriest()
{
    _Search();
    return SCREEN_CELL;
}

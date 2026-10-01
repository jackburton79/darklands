// The meetings of the map that are not fights from the start: pilgrims,
// a hermit, the bishop's tithe and the nobleman's toll, a friar. The
// states are 0x106, 0x118, 0x108, 0x163 and 0x7F of DARKLAND.EXE (see
// docs/exe.md, "Meetings on the map").

#include "CityVisitInternal.h"

#include "BattleView.h"
#include "Character.h"
#include "GameData.h"
#include "GameTime.h"
#include "ListFile.h"
#include "LocationFile.h"

#include <algorithm>


// 0E76:1326(5): every member has a mount (an item of the horse kind)
bool
CityVisit::_AllMounted() const
{
    if (fParty == NULL || fParty->members.empty())
        return false;
    for (const character& member : fParty->members) {
        if (_BestHorse(member) == 0)
            return false;
    }
    return true;
}


// The best quality of the member's horses, 0 without (0E76:0F54(member,
// 0x2000))
int
CityVisit::_BestHorse(const character& member) const
{
    const std::vector<item_definition>& items = fData.Lists().Items();
    int best = 0;
    for (const item& carried : member.items) {
        const size_t code = carried.code & 0x0FFF;
        if (code < items.size() && (items[code].flags & ITEM_HORSE) != 0)
            best = std::max(best, std::max(1, int(carried.quality)));
    }
    return best;
}


// 1367:023E(purse, $Money1)
void
CityVisit::_PayMeetingMoney()
{
    if (fParty == NULL)
        return;
    const uint32 purse = TotalPfennigs(fParty->cash);
    fParty->cash = MoneyFromPfennigs(purse > fMeetMoney ? purse - fMeetMoney : 0);
}


// A member's skill or attribute changed by the meetings (0E76:02EA,
// 0E76:0BB6)
static void
AddToSkill(character& member, int skill, int amount)
{
    member.skills[skill] = uint8(std::max(0, std::min(99,
        int(member.skills[skill]) + amount)));
}


static void
AddToMaximum(character& member, int attribute, int amount)
{
    member.maxAttributes[attribute] = uint8(std::max(1, std::min(99,
        int(member.maxAttributes[attribute]) + amount)));
}


// The pilgrims (state 0x106, file 0x13BCD8): a gift of random(50) + 3
// groschen is $Money1; it is not offered to a purse that cannot pay it,
// the mounts to a party without them all
int
CityVisit::_MeetPilgrims()
{
    fMeetMoney = uint32(fRandom() % 50 + 3) * 12;
    fVariables["Money1"] = MoneyText(fMeetMoney);
    return SCREEN_PILGRIMS;
}


// Money (file 0x13BF8C): the gift is paid, card 2, a lesson in Virtue for
// all (09C0:1F63(-2, 9, 1, 5)) and divine favor + Religion / 4 + 4 each
int
CityVisit::_PilgrimsGive()
{
    if (fParty == NULL)
        return SCREEN_PILGRIMS_PAID;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    _PayMeetingMoney();
    TrainParty(*fParty, kSkillVirtue, 1, 5, random);
    for (character& member : fParty->members) {
        AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR,
            member.skills[kSkillReligion] / 4 + 4);
    }
    return SCREEN_PILGRIMS_PAID;
}


// The mounts (file 0x13C03C): three hours, card 3, a lesson in Virtue (15),
// the same favor, and the horses are left with them
int
CityVisit::_PilgrimsMounts()
{
    if (fParty == NULL)
        return SCREEN_PILGRIMS_MOUNTS;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (fClock != NULL)
        fClock->AddHours(3);
    TrainParty(*fParty, kSkillVirtue, 1, 15, random);
    for (character& member : fParty->members) {
        AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR,
            member.skills[kSkillReligion] / 4 + 4);
    }
    _LeaveHorses();
    return SCREEN_PILGRIMS_MOUNTS;
}


// The escort (file 0x13C0F6): card 4, random(3) + 1 days ($Number1) on
// the road, card 5, a lesson in Virtue (10) and favor + Religion / 5 + the
// days for all
int
CityVisit::_PilgrimsArrive()
{
    fMeetDays = int(fRandom() % 3) + 1;
    fVariables["Number1"] = std::to_string(fMeetDays);
    if (fClock != NULL)
        fClock->AddHours(uint32(fMeetDays * 24));
    if (fParty != NULL) {
        const std::function<int(int)> random
            = [this](int n) { return int(fRandom() % uint32(n)); };
        TrainParty(*fParty, kSkillVirtue, 1, 10, random);
        for (character& member : fParty->members) {
            AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR,
                member.skills[kSkillReligion] / 5 + fMeetDays);
        }
    }
    return SCREEN_PILGRIMS_ARRIVED;
}


// The hermit (state 0x118, file 0x147EE4): $NamedOneName is a name made
// at random
int
CityVisit::_MeetHermit()
{
    fVariables["NamedOneName"] = _PersonName(uint16(fRandom()));
    return SCREEN_HERMIT;
}


// Meeting him (file 0x148186): if random(100) is at most the leader's
// Speak Common / 3 + Religion / 4 + Virtue / 3 + Perception / 2 (within
// 0..99) card 2, else card 1
int
CityVisit::_HermitMeet()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_HERMIT_COLD;
    const character& leader = fParty->members[size_t(fParty->leader)];
    const int chance = std::max(0, std::min(99,
        leader.skills[kSkillSpeakCommon] / 3 + leader.skills[kSkillReligion] / 4
        + leader.skills[kSkillVirtue] / 3
        + leader.attributes[ATTRIBUTE_PERCEPTION] / 2));
    if (int(fRandom() % 100) > chance)
        return SCREEN_HERMIT_COLD;
    return SCREEN_HERMIT_VISIT;
}


// Training (file 0x148316): card 3, a lesson in Religion for all (mode 1,
// 15), until noon
int
CityVisit::_HermitTrain()
{
    if (fParty != NULL) {
        const std::function<int(int)> random
            = [this](int n) { return int(fRandom() % uint32(n)); };
        TrainParty(*fParty, kSkillReligion, 1, 15, random);
    }
    if (fClock != NULL)
        fClock->AddHours(uint32(_HoursUntil(*fClock, 12)));
    return SCREEN_HERMIT_TRAINED;
}


// Praying (file 0x1483BA): card 4, divine favor + 25 for all, until noon
// of the next day
int
CityVisit::_HermitPray()
{
    if (fParty != NULL) {
        for (character& member : fParty->members)
            AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR, 25);
    }
    if (fClock != NULL)
        fClock->AddHours(uint32(_HoursUntil(*fClock, 12) + 24));
    return SCREEN_HERMIT_PRAYED;
}


// A saint for a member (file 0x14844A, 0x14853E..): one of the 136 at
// random; if he knows it card 6, else he learns it, card 5; until noon of
// the next day, modulo 25
int
CityVisit::_HermitTeach(int member)
{
    if (fParty == NULL || size_t(member) >= fParty->members.size())
        return SCREEN_HERMIT_KNEW;
    character& who = fParty->members[size_t(member)];
    const int saint = int(fRandom() % 136);
    const uint8 bit = uint8(0x80 >> (saint % 8));
    int screen = SCREEN_HERMIT_KNEW;
    if ((who.saints[saint / 8] & bit) == 0) {
        who.saints[saint / 8] |= bit;
        screen = SCREEN_HERMIT_TAUGHT;
    }
    if (fClock != NULL) {
        fClock->AddHours(uint32((_HoursUntil(*fClock, 12) + 24) % 25));
    }
    return screen;
}


// The tithe of a bishop (state 0x108, file 0x13D546) and the toll of a
// nobleman (0x163, file 0x1773B9). The bishop asks purse / 50 + the letter
// of credit / 100 pfennigs (1..9999, at most half the purse; with 10
// pfennigs or less he lets them go, card 16); the nobleman 6 pfennigs of
// a purse under 30, two groschen of one under 300, else a florin.
int
CityVisit::_MeetTithe()
{
    fPleaFailed = false;
    fPrayerFailed = false;
    const uint32 purse = fParty != NULL ? TotalPfennigs(fParty->cash) : 0;
    if (fToll) {
        fMeetMoney = purse < 30 ? 6 : purse < 300 ? 24 : 240;
    } else {
        if (purse <= 10)
            return SCREEN_TITHE_POOR;
        const uint32 bank = fParty != NULL ? uint32(fParty->bankNotes) * 240 : 0;
        fMeetMoney = std::max(uint32(1), std::min(uint32(9999),
            purse / 50 + bank / 100));
        fMeetMoney = std::min(fMeetMoney, purse / 2);
    }
    fVariables["Money1"] = MoneyText(fMeetMoney);
    if (fParty != NULL && !fParty->members.empty())
        _SetChosen(fParty->leader);
    return SCREEN_TITHE;
}


// Escaping (file 0x13DE3C): with every member mounted, the speed of the
// slowest (agility + his best horse's quality) + his Riding / 2, within
// 0..99; else none
int
CityVisit::_TitheEscapeChance() const
{
    if (!_AllMounted())
        return 0;
    int speed = 1000;
    size_t slowest = 0;
    for (size_t i = 0; i < fParty->members.size(); i++) {
        const character& member = fParty->members[i];
        const int value = member.attributes[ATTRIBUTE_AGILITY]
            + _BestHorse(member);
        if (value < speed) {
            speed = value;
            slowest = i;
        }
    }
    return std::max(0, std::min(99,
        speed + fParty->members[slowest].skills[17] / 2));
}


// Pleading (file 0x13DC58): the bishop's, the party's average Virtue + the
// leader's Speak Common / 2, within 0..100; the toll's (0x17790E), the
// party's fame + the leader's Speak Common, within 1..99
int
CityVisit::_TithePleaChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[size_t(fParty->leader)];
    if (fToll) {
        return std::max(1, std::min(99, int(fParty->fame)
            + leader.skills[kSkillSpeakCommon]));
    }
    int virtue = 0;
    for (const character& member : fParty->members)
        virtue += member.skills[kSkillVirtue];
    virtue /= int(fParty->members.size());
    return std::max(0, std::min(100,
        virtue + leader.skills[kSkillSpeakCommon] / 2));
}


// If random(100) is at most the chance card 13 and a lesson in Speak
// Common for the leader (mode 7, 10), else card 14 and no more pleas
int
CityVisit::_TithePlead()
{
    if (int(fRandom() % 100) <= _TithePleaChance()) {
        if (fParty != NULL && !fParty->members.empty()) {
            const std::function<int(int)> random
                = [this](int n) { return int(fRandom() % uint32(n)); };
            TrainSkill(fParty->members[size_t(fParty->leader)],
                kSkillSpeakCommon, 10, random);
        }
        return SCREEN_TITHE_PLEADED;
    }
    fPleaFailed = true;
    return SCREEN_TITHE_PLEA_FAILED;
}


// Refusing: card 4, submit or fight (the options 0..5 are not there)
int
CityVisit::_TitheRefuse()
{
    return SCREEN_TITHE_REFUSED;
}


// Escaping (file 0x13DD2E): with no chance card 7; if random(100) is at
// most it card 5 and a lesson in Riding for all (mode 1, 10), else card 6
// and a lesson of mode 0; the last two ask to submit or fight
int
CityVisit::_TitheEscape()
{
    const int chance = _TitheEscapeChance();
    if (chance == 0)
        return SCREEN_TITHE_CORNERED;
    const std::function<int(int)> random
        = [this](int n) { return int(fRandom() % uint32(n)); };
    if (int(fRandom() % 100) <= chance) {
        if (fParty != NULL)
            TrainParty(*fParty, 17, 1, 10, random);
        return SCREEN_TITHE_ESCAPED;
    }
    if (fParty != NULL)
        TrainParty(*fParty, 17, 0, 10, random);
    return SCREEN_TITHE_CAUGHT;
}


// Submitting (file 0x13DF7C): the search (09C0:1EB9(-2)), two hours, card
// 8; the toll's men leave clubs
int
CityVisit::_TitheSubmit()
{
    if (fToll)
        _Robbed();
    else
        _Search();
    if (fClock != NULL)
        fClock->AddHours(2);
    return SCREEN_TITHE_ROBBED;
}


// The fight (files 0x13D85C and 0x1776A0, field 0x2E), s the party's
// strength. The bishop's: enemy 3 (guards) at variant random(3) + s / 4
// + 2, random(2) + 4 of them, and enemy 0x21 (knights) at variant s % 3
// + 2, 3 if s is over 6, else 2. The nobleman's: enemy 3 at variant
// random(3) + s / 4 + 1, random(3) + 3 of them, and three of enemy 0x21 at
// variant 2 if s is over 6, else 1.
void
CityVisit::_FightTithe()
{
    fFoes.clear();
    const int s = fParty != NULL ? PartyStrength(*fParty) : 1;
    if (fToll) {
        fFoes.push_back(foes{ 3, int(fRandom() % 3) + s / 4 + 1,
            int(fRandom() % 3) + 3 });
        fFoes.push_back(foes{ 0x21, s > 6 ? 2 : 1, 3 });
    } else {
        fFoes.push_back(foes{ 3, int(fRandom() % 3) + s / 4 + 2,
            int(fRandom() % 2) + 4 });
        fFoes.push_back(foes{ 0x21, s % 3 + 2, s > 6 ? 3 : 2 });
    }
    fBattleKind = BATTLE_WITH_TITHE_GUARDS;
    fPendingBattle = true;
}


// The fight's end: won (file 0x13D8E0), an hour, the reputation of the
// nearest city down by 10..20 (the toll's, 20..30), card 11; retreated,
// those left behind come back, card 15; lost, random(3) + 3 hours, the
// search, card 12; the bishop's fight also costs each member 2 points of
// Virtue and 25 of divine favor, whatever the end
int
CityVisit::_ResolveTitheBattle(int outcome)
{
    int screen = SCREEN_TITHE_FLED;
    if (outcome == BATTLE_WON) {
        if (fClock != NULL)
            fClock->AddHours(1);
        if (fToll)
            _ChangeReputation(-30, -20);
        else
            _ChangeReputation(-20, -10);
        screen = SCREEN_TITHE_WON;
    } else if (outcome == BATTLE_LOST) {
        if (fClock != NULL)
            fClock->AddHours(uint32(fRandom() % 3 + 3));
        if (fToll)
            _Robbed();
        else
            _Search();
        screen = SCREEN_TITHE_BEATEN;
    }
    if (!fToll && fParty != NULL) {
        for (character& member : fParty->members) {
            AddToSkill(member, kSkillVirtue, -2);
            AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR, -25);
        }
    }
    return screen;
}


// The friar (state 0x7F, file 0xF4C08): his pardon costs purse / 60 + 3
// pfennigs for each member (the whole purse if it is a groschen or less;
// nothing is offered to an empty one)
int
CityVisit::_MeetFriar()
{
    fPrayerFailed = false;
    const uint32 purse = fParty != NULL ? TotalPfennigs(fParty->cash) : 0;
    if (purse <= 12)
        fMeetMoney = purse;
    else
        fMeetMoney = purse / 60 + 3 * uint32(fParty->members.size());
    fVariables["Money1"] = MoneyText(fMeetMoney);
    if (fParty != NULL && !fParty->members.empty())
        _SetChosen(fParty->leader);
    return SCREEN_FRIAR;
}


// His curse (file 0xF4EB4): each member loses x = (100 - Virtue) / 14 + 1
// points of Strength and Endurance, 4x of divine favor, and x / 2 off the
// maximum of the first two
void
CityVisit::_FriarCurse()
{
    if (fParty == NULL)
        return;
    for (character& member : fParty->members) {
        const int x = (100 - member.skills[kSkillVirtue]) / 14 + 1;
        AddToAttribute(member, ATTRIBUTE_STRENGTH, -x);
        AddToAttribute(member, ATTRIBUTE_ENDURANCE, -x);
        AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR, -4 * x);
        AddToMaximum(member, ATTRIBUTE_STRENGTH, -(x / 2));
        AddToMaximum(member, ATTRIBUTE_ENDURANCE, -(x / 2));
    }
}


// Whatever the fight's end, each member loses 2 points of Virtue and 30 of
// divine favor (file 0xF5002)
void
CityVisit::_FriarPenalties()
{
    if (fParty == NULL)
        return;
    for (character& member : fParty->members) {
        AddToSkill(member, kSkillVirtue, -2);
        AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR, -30);
    }
}


// The fight (file 0xF4F3E, field 0x2E): seven of enemy 13 (followers) and
// the friar, enemy 32, both at variant 1
void
CityVisit::_FightFriar()
{
    fFoes.clear();
    fFoes.push_back(foes{ 13, 1, 7 });
    fFoes.push_back(foes{ 32, 1, 1 });
    fBattleKind = BATTLE_WITH_FRIAR;
    fPendingBattle = true;
}


// Won (file 0xF4F74): an hour, card 9; retreated, the curse and an hour,
// card 8; lost, an hour and the search, card 11 (or 10 for the other
// of the game's two defeats, which BattleView does not tell apart)
int
CityVisit::_ResolveFriarBattle(int outcome)
{
    int screen = SCREEN_FRIAR_WON;
    if (fClock != NULL)
        fClock->AddHours(1);
    if (outcome == BATTLE_LOST) {
        _Search();
        screen = SCREEN_FRIAR_ROBBED;
    } else if (outcome != BATTLE_WON) {
        _FriarCurse();
        screen = SCREEN_FRIAR_CURSED;
    }
    _FriarPenalties();
    return screen;
}


// A saint answers the friar: Godfrey and John Nepomuk make him confess
// (card 5, a lesson in Virtue for all, favor + 8), Dominic (card 4, + 9);
// against the curse the plague saints protect (card 6, + 6). The first
// two are the game's first slots; the others are *inferred* as the
// last ones.
int
CityVisit::_FriarSaint(int saint)
{
    const bool cursing = fScreen == SCREEN_FRIAR_CURSING;
    int screen = SCREEN_FRIAR_HONEST;
    int favor = 8;
    if (saint == 36) {
        screen = SCREEN_FRIAR_REPENTS;
        favor = 9;
    } else if (cursing && saint != 61 && saint != 78) {
        screen = SCREEN_FRIAR_PROTECTED;
        favor = 6;
    }
    if (fParty != NULL) {
        const std::function<int(int)> random
            = [this](int n) { return int(fRandom() % uint32(n)); };
        TrainParty(*fParty, kSkillVirtue, 7, 15, random);
        for (character& member : fParty->members)
            AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR, favor);
    }
    return screen;
}

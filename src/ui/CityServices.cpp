// The inn, the banks, the physician and the alchemist

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


// The price of a meal and a night for the party (DARKLAND.EXE,
// 1462:1D2C), in pfennigs: the city size + 1, a third more for a
// suspect party (reputation -10 or less), 30% less for a respected one
// (10 or more), half for a local hero (50 or more); times the party's
// size. (The game raises it by 8/3 or 5/3 with some location states,
// which are not kept here.)
uint32
CityVisit::InnPrice() const
{
    const int reputation = _Reputation();
    int price = _City().size + 1;
    if (reputation <= -10)
        price = price * 4 / 3;
    else if (reputation >= 50)
        price = price / 2;
    else if (reputation >= 10)
        price = price * 7 / 10;
    price = std::max(price, 1);
    if (fParty != NULL)
        price *= int(fParty->members.size());
    return uint32(std::max(1, std::min(price, 1000)));
}


// A meal and a night (file 0xA6F70): every member gets back all the
// endurance it lacks, and a point of strength; the time passes when the
// card is left
int
CityVisit::_Sleep()
{
    if (fParty == NULL)
        return SCREEN_SLEEP;
    const uint32 price = InnPrice();
    fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) - price);
    for (character& member : fParty->members) {
        member.attributes[ATTRIBUTE_ENDURANCE]
            = member.maxAttributes[ATTRIBUTE_ENDURANCE];
        if (member.attributes[ATTRIBUTE_STRENGTH]
                < member.maxAttributes[ATTRIBUTE_STRENGTH])
            AddToAttribute(member, ATTRIBUTE_STRENGTH, 1);
    }
    return SCREEN_SLEEP;
}


// The stables (file 0xA7120): by day the stablemaster shows his horses
// and mules, asking about the party's mounts unless every member has one
// (0E76:1326); at night the stableboy only says to come back
int
CityVisit::_Stables()
{
    if (fNight || fParty == NULL)
        return SCREEN_STABLES;
    const std::vector<item_definition>& items = fData.Lists().Items();
    for (const character& member : fParty->members) {
        bool mounted = false;
        for (const item& carried : member.items) {
            if (carried.code < items.size()
                    && (items[carried.code].flags & ITEM_HORSE) != 0)
                mounted = true;
        }
        if (!mounted)
            return SCREEN_STABLES_SALE;
    }
    return SCREEN_STABLES;
}


// Redeeming the letter of credit: its florins go to the purse, less
// 6 pfennigs a florin
int
CityVisit::_Redeem(int result)
{
    if (fParty == NULL)
        return result;
    const uint32 notes = fParty->bankNotes;
    const uint32 fee = notes * 6;
    fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash)
        + notes * 240 - fee);
    fParty->bankNotes = 0;
    fVariables["Money1"] = MoneyText(notes * 240);
    fVariables["Money2"] = MoneyText(fee);
    return result;
}


// Buying a letter of credit: the florins typed in, at most 500 and what
// the purse holds in florins (not counting the smaller coins), without a
// fee
void
CityVisit::_Deposit()
{
    if (fParty == NULL)
        return;
    const std::string& typed = fView.PromptText();
    uint32 amount = 0;
    for (char c : typed)
        amount = std::min(amount * 10 + uint32(c - '0'), 500u);
    amount = std::min(amount, uint32(fParty->cash.florins));
    amount = std::min(amount, uint32(0xFFFF - fParty->bankNotes));
    fParty->cash.florins -= uint16(amount);
    fParty->bankNotes += uint16(amount);
}


CityVisit::~CityVisit()
{
}


// The physician's skill, made when the party first meets him (file
// 0xA2F1A): (the city's property 0x21 % 10) · (city size + random(4)
// - 3), within 1..99
int
CityVisit::_PhysicianSkill()
{
    std::map<int, int>::const_iterator found = fPhysicianSkill.find(fCity);
    if (found != fPhysicianSkill.end())
        return found->second;
    const city& c = _City();
    const int skill = (_PeopleSeed() % 10)
        * (c.size + int(fRandom() % 4) - 3);
    return fPhysicianSkill[fCity] = std::max(1, std::min(skill, 99));
}


// The members whose strength is under its maximum
int
CityVisit::_Wounded() const
{
    int wounded = 0;
    if (fParty != NULL) {
        for (const character& member : fParty->members) {
            if (member.attributes[ATTRIBUTE_STRENGTH]
                    < member.maxAttributes[ATTRIBUTE_STRENGTH])
                wounded++;
        }
    }
    return wounded;
}


// skill / 10 + 12 pfennigs for each wounded member (file 0xA33A6)
uint32
CityVisit::_TreatmentPrice()
{
    return uint32((_PhysicianSkill() / 10 + 12) * _Wounded());
}


// The member best at healing, who speaks with the physician (0E76:14A4)
int
CityVisit::_BestHealer() const
{
    int best = 0;
    for (size_t i = 1; fParty != NULL && i < fParty->members.size(); i++) {
        if (fParty->members[i].skills[kSkillHealing]
                > fParty->members[best].skills[kSkillHealing])
            best = int(i);
    }
    return best;
}


// Discussing treatments (file 0xA31D6): an hour; the best healer
// judges the physician if random(100) is at most his intelligence, half
// his charisma and the physician's skill (file 0xA333C)
int
CityVisit::_DiscussTreatments()
{
    const int skill = _PhysicianSkill();
    if (fClock != NULL)
        fClock->AddHours(1);
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_PHYSICIAN_UNSURE;
    const character& healer = fParty->members[_BestHealer()];
    const int chance = healer.attributes[ATTRIBUTE_INTELLIGENCE]
        + healer.attributes[ATTRIBUTE_CHARISMA] / 2 + skill;
    if (int(fRandom() % 100) > chance)
        return SCREEN_PHYSICIAN_UNSURE;
    if (skill <= 1)
        return SCREEN_PHYSICIAN_IDIOT;
    static const char* kWords[] = { "Poor", "Modest", "Good", "Very Good",
        "Excellent" };
    fVariables["Text1"] = kWords[std::min(skill / 20, 4)];
    return SCREEN_PHYSICIAN_SKILL;
}


// Asking his aid (file 0xA3388): an hour, then his price, and the
// treatment is offered
int
CityVisit::_AskAid()
{
    if (fClock != NULL)
        fClock->AddHours(1);
    fVariables["Number1"] = std::to_string(_Wounded());
    fVariables["Money1"] = MoneyText(_TreatmentPrice());
    fTreatmentOffered = true;
    return SCREEN_PHYSICIAN_PRICE;
}


// Alchemical components (file 0xA35E8): he trades if random(100) is at
// most the leader's Speak Common, charisma, (the city's number + month)
// % 30 and the local reputation, within 0..75 (file 0xA365C); an hour
int
CityVisit::_Components()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_PHYSICIAN_NO_TRADE;
    const character& leader = fParty->members[fParty->leader];
    const int month = fClock != NULL ? fClock->Month() : 0;
    const int chance = std::max(0, std::min(75,
        leader.skills[kSkillSpeakCommon] + leader.attributes[ATTRIBUTE_CHARISMA]
            + int(uint16(_PeopleSeed() + month) % 30) + _Reputation()));
    if (int(fRandom() % 100) > chance)
        return SCREEN_PHYSICIAN_NO_TRADE;
    if (fClock != NULL)
        fClock->AddHours(1);
    fPendingTrade = MERCHANT_PHYSICIAN;
    return SCREEN_PHYSICIAN;
}


// The treatment (file 0xA36C0): paid, an hour, and every wounded member
// gains skill / 30 strength (at least 1; an idiot's treatment takes 1 or
// 2); then he treats no one for 20 hours (0E76:2930)
int
CityVisit::_Treatment()
{
    if (fParty == NULL)
        return SCREEN_PHYSICIAN;
    const uint32 price = _TreatmentPrice();
    const uint32 purse = TotalPfennigs(fParty->cash);
    if (purse < price)
        return SCREEN_PHYSICIAN_POOR;
    fParty->cash = MoneyFromPfennigs(purse - price);
    const int skill = _PhysicianSkill();
    if (fClock != NULL)
        fClock->AddHours(1);
    for (character& member : fParty->members) {
        if (member.attributes[ATTRIBUTE_STRENGTH]
                >= member.maxAttributes[ATTRIBUTE_STRENGTH])
            continue;
        const int amount = skill > 1 ? std::max(1, std::min(skill / 30, 99))
            : int(fRandom() % 2) - 2;
        AddToAttribute(member, ATTRIBUTE_STRENGTH, amount);
    }
    if (fClock != NULL)
        fTreatedUntil[fCity] = fClock->HourStamp() + 20;
    return SCREEN_PHYSICIAN_TREATED;
}


// Asking to be his students (file 0xA349C): nothing to learn from him if
// the best healer is better (and he is no idiot); no again for 30 hours
// after a no; he takes students for skill / 5 + 10 pfennigs a day where
// (the city's property 0x21 + year) % 3 is not 0 (a teacher of healing
// up to 60, for 168 hours); else he has 1..4 apprentices already
int
CityVisit::_Students()
{
    const int skill = _PhysicianSkill();
    int best = 0;
    if (fParty != NULL && !fParty->members.empty())
        best = fParty->members[_BestHealer()].skills[kSkillHealing];
    if (best > skill && skill > 1)
        return SCREEN_PHYSICIAN_NOTHING;
    const uint32 now = fClock != NULL ? fClock->HourStamp() : 0;
    const std::map<int, uint32>::const_iterator refused
        = fNoStudentsUntil.find(fCity);
    if (refused != fNoStudentsUntil.end() && now < refused->second)
        return SCREEN_PHYSICIAN_NO_STUDENTS;
    const int year = fClock != NULL ? fClock->Year() : 1400;
    if (int16(_PeopleSeed() + year) % 3 != 0 && skill > 1) {
        const uint32 fee = uint32(skill / 5 + 10);
        fVariables["Money1"] = MoneyText(fee);
        // the teacher the game makes charges 60 pfennigs a day, not the
        // fee the card shows, with a level of 50 (0E76:2C4E's arguments)
        _AddTutor(city_tutor{ kSkillHealing, 50, 60, now + 168 });
        return SCREEN_PHYSICIAN_TUTOR;
    }
    fVariables["Number1"] = std::to_string(fRandom() % 4 + 1);
    fNoStudentsUntil[fCity] = now + 30;
    return SCREEN_PHYSICIAN_APPRENTICES;
}


// Leaving the physician (file 0xA3902, 0xA39B6): at night, apologizing
// with two groschen for the trouble (to the district), or else, half the
// time
// (random(100) <= 50), he curses the party and the local reputation
// falls by 1..4 (card 7)
int
CityVisit::_LeavePhysician(bool apologize)
{
    if (apologize) {
        if (fParty != NULL) {
            const uint32 purse = TotalPfennigs(fParty->cash);
            fParty->cash = MoneyFromPfennigs(purse - std::min(purse, 24u));
        }
        return SCREEN_DISTRICT;
    }
    if (int(fRandom() % 100) > 50)
        return SCREEN_CRAFTS;
    if (fReputations != NULL && fCity >= 0
            && fCity < int(fReputations->size())) {
        int16& reputation = (*fReputations)[fCity];
        reputation = int16(std::max(-99, reputation - int(fRandom() % 4) - 1));
    }
    return SCREEN_PHYSICIAN_CURSES;
}


// The alchemist's skill (file 0xD9BBB): city size · 3 + (property 0x21
// + 4) % 41, and (property 0x21 + 9) % 11 + 10 more in a city with the
// flag 0x100 (0E76:1A7E(0x1B)); the shop (file 0xDA0CA) leaves this out
int
CityVisit::_AlchemistSkill(bool withBonus) const
{
    const city& c = _City();
    int skill = c.size * 3 + (_PeopleSeed() + 4) % 41;
    if (withBonus && (c.flags & 0x100) != 0)
        skill += (_PeopleSeed() + 9) % 11 + 10;
    return skill;
}


// The stone he can make (file 0xDA00C): (property 0x21 + 4) % 25 + 1
int
CityVisit::_StoneQuality() const
{
    return (_PeopleSeed() + 4) % 25 + 1;
}


// Its price, in groschen (file 0xD9C62): (skill / 2 + 1) · (quality + 5)
// / 12, within 1..50 + (the seed global + location) % 20
uint32
CityVisit::_StonePrice() const
{
    const int limit = 50 + (fSeed + fCity) % 20;
    const int groschen = (_AlchemistSkill(true) / 2 + 1)
        * (_StoneQuality() + 5) / 12;
    return uint32(std::max(1, std::min(groschen, limit))) * 12;
}


// Whether he deigns to deal (file 0xD9F6C): random(100) at most the
// leader's charisma + reputation / 10 + property 0x21 % 31 + the
// leader's Speak Common / 3 + the best Alchemy / 2 - 15, within 0..99
int
CityVisit::_AlchemistChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[fParty->leader];
    const int chance = leader.attributes[ATTRIBUTE_CHARISMA]
        + _Reputation() / 10 + _PeopleSeed() % 31
        + leader.skills[kSkillSpeakCommon] / 3
        + _BestSkill(kSkillAlchemy) / 2 - 15;
    return std::max(0, std::min(chance, 99));
}


// A better philosopher's stone (file 0xD9FFE): offended if the chance
// fails (card 6, and no dealings for 60 hours); else the stone becomes
// his quality, paid, if it is better (card 7), or card 5
int
CityVisit::_Stone()
{
    fStoneOffered = true;
    if (int(fRandom() % 100) > _AlchemistChance()) {
        if (fClock != NULL)
            fAlchemistAngryUntil[fCity] = fClock->HourStamp() + 60;
        return SCREEN_ALCHEMIST_ANGRY;
    }
    const int quality = _StoneQuality();
    if (fParty == NULL || fParty->philosopherStone >= quality)
        return SCREEN_STONE_BEYOND;
    const uint32 price = _StonePrice();
    fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) - price);
    fParty->philosopherStone = uint16(quality);
    fVariables["Number1"] = std::to_string(quality);
    fVariables["Money1"] = MoneyText(price);
    return SCREEN_STONE_IMPROVED;
}


// Purchasing (file 0xDA0CA): offended if the chance fails; else the trade
// screen, with potions if his skill (without the bonus) is over 24, else
// alchemical components
int
CityVisit::_AlchemistShop()
{
    if (int(fRandom() % 100) > _AlchemistChance()) {
        if (fClock != NULL)
            fAlchemistAngryUntil[fCity] = fClock->HourStamp() + 60;
        return SCREEN_ALCHEMIST_ANGRY;
    }
    fPendingTrade = _AlchemistSkill(false) > 24 ? MERCHANT_ALCHEMIST
        : MERCHANT_ALCHEMIST_COMPONENTS;
    return SCREEN_ALCHEMIST_AGAIN;
}

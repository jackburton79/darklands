// The cathedral: Mass, a prelate, gifts and relics

#include "CityVisitInternal.h"

#include "Character.h"
#include "CityFile.h"
#include "GameData.h"
#include "GameTime.h"
#include "ListFile.h"

#include <algorithm>


// A relic among the party's items (the item flag 0x1000, 09C0:203F)
bool
CityVisit::_FindRelic(size_t* member, size_t* index) const
{
    if (fParty == NULL)
        return false;
    const std::vector<item_definition>& items = fData.Lists().Items();
    for (size_t m = 0; m < fParty->members.size(); m++) {
        const std::vector<item>& carried = fParty->members[m].items;
        for (size_t i = 0; i < carried.size(); i++) {
            const size_t code = carried[i].code & 0x0FFF;
            if (code < items.size() && (items[code].flags & ITEM_RELIC) != 0) {
                if (member != NULL)
                    *member = m;
                if (index != NULL)
                    *index = i;
                return true;
            }
        }
    }
    return false;
}


// Mass (file 0xB6D74 by day, 0xB7BA2 at night): by day at bell 2 always,
// at 3 in cities of size 6 or more, 4: 7, 5: 8, 6: 5, 7: 7, never at 1 and
// 8; at night only at bell 7 in cities of size 7 or more. Every member
// gains divine favor Religion / 60 + Speak Latin / 40 + 1 and the party
// waits until the start of the bell after the next one (card 2). Else
// card 1 names the next Mass: Vespers (hour 18) by day at bells 3..5 in
// cities over size 3, Matins at night at bell 1 in cities of size 8 or
// more, else Prime. No time passes.
int
CityVisit::_CathedralMass()
{
    const int none = fNight ? SCREEN_CATHEDRAL_NIGHT_NO_MASS
        : SCREEN_CATHEDRAL_NO_MASS;
    if (fClock == NULL || fParty == NULL)
        return none;
    static const int kMassSize[9] = { 99, 99, 0, 6, 7, 8, 5, 7, 99 };
    const int size = _City().size;
    const int bell = fClock->Hour() / 3 + 1;
    const bool mass = fNight ? bell == 7 && size >= 7 : size >= kMassSize[bell];
    if (!mass) {
        int next = 6;
        if (fNight)
            next = bell == 1 && size >= 8 ? 1 : 6;
        else if (bell >= 3 && bell <= 5 && size > 3)
            next = 18;
        fVariables["NamedOneName"] = GameTime(1400, 0, 1, uint16(next)).BellName();
        return none;
    }
    for (character& member : fParty->members) {
        AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR,
            member.skills[kSkillReligion] / 60
            + member.skills[kSkillSpeakLatin] / 40 + 1);
    }
    const int end = (bell + 1) * 3;
    fClock->AddHours(uint32((end - fClock->Hour() + 24) % 24));
    return fNight ? SCREEN_CATHEDRAL_NIGHT_MASS : SCREEN_CATHEDRAL_MASS;
}


// Talking to a priest (file 0xB6F34, 0xB7D30): an hour; by day with unrest
// and rebels here the priests do not take the party to their superior (the
// chance the game passes is -1, so it never does: card 21); else what the
// prelate says of the cathedral's saints and relics (card 3: the data of
// every city has neither, DARKLAND.LOC's words +0x1A and +0x1C being 0)
int
CityVisit::_CathedralPrelate()
{
    if (fClock != NULL)
        fClock->AddHours(1);
    if (!fNight && _EventHere(2) && _EventHere(2, 0))
        return SCREEN_CATHEDRAL_TURNED_AWAY;
    return fNight ? SCREEN_CATHEDRAL_NIGHT_PRELATE : SCREEN_CATHEDRAL_PRELATE;
}


// A gift (file 0xB71DC): a third of the purse ($Money1, offered from a
// purse of 7200 pfennigs); the leader's divine favor + that / 200 within
// 0..30; a lesson in Virtue for him (09C0:1F63(-1, 9, 7, 10, 0)); an hour;
// card 9 under 240 pfennigs, 10 under 1440, 11 under 3600, else 12
int
CityVisit::_CathedralGift()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_CATHEDRAL_GIFT;
    const uint32 purse = TotalPfennigs(fParty->cash);
    const uint32 gift = purse / 3;
    fParty->cash = MoneyFromPfennigs(purse - gift);
    character& leader = fParty->members[size_t(fParty->leader)];
    AddToAttribute(leader, ATTRIBUTE_DIVINE_FAVOR,
        std::max(0, std::min(30, int(gift / 200))));
    TrainSkill(leader, kSkillVirtue, 10,
        [this](int n) { return int(fRandom() % uint32(n)); });
    if (fClock != NULL)
        fClock->AddHours(1);
    if (gift < 240)
        return SCREEN_CATHEDRAL_GIFT;
    if (gift < 1440)
        return SCREEN_CATHEDRAL_GIFT_MORE;
    return gift < 3600 ? SCREEN_CATHEDRAL_GIFT_BIG : SCREEN_CATHEDRAL_GIFT_GRAND;
}


// Giving a relic (file 0xB768E, 0xB7F20): the reputation + 30
// (0E76:19D0(location, 30, 30)), every member's divine favor up (+ 99),
// the relic left ($NamedOneName; the leader is $ChosenOneName), three hours
int
CityVisit::_CathedralRelic()
{
    size_t member = 0;
    size_t index = 0;
    if (fParty == NULL || !_FindRelic(&member, &index))
        return fNight ? SCREEN_CATHEDRAL_NIGHT_RELIC : SCREEN_CATHEDRAL_RELIC;
    _ChangeReputation(30, 30);
    for (character& who : fParty->members)
        AddToAttribute(who, ATTRIBUTE_DIVINE_FAVOR, 99);
    _SetChosen(fParty->leader);
    std::vector<item>& carried = fParty->members[member].items;
    const size_t code = carried[index].code & 0x0FFF;
    const std::vector<item_definition>& items = fData.Lists().Items();
    if (code < items.size())
        fVariables["NamedOneName"] = items[code].name;
    const uint8 type = carried[index].type;
    carried.erase(carried.begin() + long(index));
    for (uint8& slot : fParty->members[member].equipment) {
        bool left = false;
        for (const item& other : carried)
            left = left || other.type == type;
        if (slot == type && !left)
            slot = kNoEquipment;
    }
    if (fClock != NULL)
        fClock->AddHours(3);
    return fNight ? SCREEN_CATHEDRAL_NIGHT_RELIC : SCREEN_CATHEDRAL_RELIC;
}

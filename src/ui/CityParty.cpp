// The party's composition at the inn and the barracks: members who retire,
// and people who join

#include "CityVisitInternal.h"

#include "Character.h"
#include "PartySelectView.h"

#include <algorithm>


// Looking for people (file 0x10DA7A): random(3) + 2 hours ($Number1, card
// 6), then the game shows the people it found (0E76:2246, the character
// selection screen of overlay 0x26: not decoded). Here they are the
// members who retired in this city.
int
CityVisit::_PartyLooking()
{
    const int hours = 2 + int(fRandom() % 3);
    fVariables["Number1"] = std::to_string(hours);
    if (fClock != NULL)
        fClock->AddHours(uint32(hours));
    return SCREEN_PARTY_LOOKING;
}


// The people found (0E76:2246: the party selection screen, the overlay at
// file 0x7C5F0, with the characters of the world): those who retired in
// this city, or anywhere, can be added to the party. Whether the game
// lets more be done in a city is not known: only adding and examining
// are on here
void
CityVisit::_SelectParty(GameWindow& window)
{
    if (fParty == NULL || fRetired == NULL)
        return;
    PartySelectView view(fData);
    view.SetInfoView(fInfo);
    view.SetRoster(PartySelectView::MakeRoster(*fParty, *fRetired, fCity), true);
    if (view.Run(window) == PartySelectView::RESULT_QUIT)
        return;
    PartySelectView::ApplyRoster(view.Roster(), *fParty, *fRetired, fCity);
    SetParty(fParty);
}


// A member retires (file 0x10DB9C and the like, then 0x10D98C): card 7,
// an hour, and he takes a fifth of the party's whole wealth, the purse and
// the letters of credit alike (1367:00F2 and 0180 on DS:906B and DS:9072);
// he and his possessions leave the party (09C0:1EC3) and wait here. The
// game does not say what becomes of his share when he comes back
int
CityVisit::_Retire(int slot)
{
    if (fParty == NULL || slot < 0 || slot >= int(fParty->members.size())
            || fParty->members.size() < 2)
        return SCREEN_PARTY;
    const character& who = fParty->members[size_t(slot)];
    _SetChosen(slot);
    fVariables["ChosenOneName"] = who.shortName;
    if (fClock != NULL)
        fClock->AddHours(1);
    const uint32 purse = TotalPfennigs(fParty->cash);
    fParty->cash = MoneyFromPfennigs(purse - purse / 5);
    fParty->bankNotes = uint16(fParty->bankNotes - fParty->bankNotes / 5);
    if (fRetired != NULL) {
        retired_member gone;
        gone.member = who;
        gone.image = size_t(slot) < fParty->images.size()
            ? fParty->images[size_t(slot)] : std::string();
        if (size_t(slot) < fParty->colors.size())
            gone.colors = fParty->colors[size_t(slot)];
        gone.city = fCity;
        fRetired->push_back(gone);
    }
    RemoveMember(*fParty, size_t(slot));
    SetParty(fParty);
    return SCREEN_PARTY_RETIRED;
}

// What the city knows of the world: the news, the events, places and
// directions

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


// News and rumors at the inn (file 0xA6E40): every member gains an
// eighth of his maximum Endurance; a party of -40 or less is found by
// the guards (card 4, then the guards' challenge) if random(100) is over
// -10 - the reputation (file 0xA6F26); else two hours and the news, back
// to the inn after (DS:E7D8)
int
CityVisit::_InnNews()
{
    if (fParty != NULL) {
        for (character& member : fParty->members) {
            const int most = member.maxAttributes[ATTRIBUTE_ENDURANCE];
            member.attributes[ATTRIBUTE_ENDURANCE] = uint8(std::min(most,
                member.attributes[ATTRIBUTE_ENDURANCE] + most / 8));
        }
    }
    const int reputation = _Reputation();
    if (reputation <= -40 && int(fRandom() % 100) > -10 - reputation)
        return SCREEN_INN_RAID;
    if (fClock != NULL)
        fClock->AddHours(2);
    fNewsReturn = SCREEN_INN;
    return SCREEN_NEWS;
}


// The notices (state 0x6D, file 0xE97C2), shown one after the other:
// with nobody reading better than 10 (0E76:14A4(12)), by day a citizen
// reads them (card 4), at night nothing (card 5, and no more); then
// card 1 or 2 when the location's state (+0x14) is 1 or 2 (prices, a
// siege), card 3 with unrest here (0E76:3470(2, location)), and the
// curfew, card 6 when the city's property 0x21 is even, else card 0;
// no time passes
int
CityVisit::_Notices()
{
    fNewsQueue.clear();
    if (_BestSkill(kSkillReadWrite) <= 10 && fParty != NULL
            && !fParty->members.empty()) {
        if (fClock != NULL && !IsGameDay(*fClock))
            return SCREEN_NOTICES_TOO_DARK;
        fNewsQueue.push_back(std::make_pair(SCREEN_NOTICES_EXPLAINED, -1));
    }
    if (_CityState() == 1)
        fNewsQueue.push_back(std::make_pair(SCREEN_NOTICE_PRICES, -1));
    else if (_CityState() == 2)
        fNewsQueue.push_back(std::make_pair(SCREEN_NOTICE_SIEGE, -1));
    if (_EventHere(2))
        fNewsQueue.push_back(std::make_pair(SCREEN_NOTICE_ASSEMBLY, -1));
    fNewsQueue.push_back(std::make_pair(int16(_PeopleSeed()) % 2 == 0
        ? SCREEN_NOTICE_CURFEW_LORD : SCREEN_NOTICE_CURFEW, -1));
    return _NextNews();
}


// News from elsewhere (state 0x6E, file 0xE9FE4), one card after the
// other: a dragon (an event of kind 4, card 5: $Direction from here to
// its place, 0E76:3C28(28, -1, -1, 4, -1)), the mines (kind 12, card 17:
// $NearestCity, the city nearest its place, 1462:271A); then about the
// city: its ruler overthrown (the location's state has bit 0x80, card 1)
// or a rebellion put down (mark 0x42, card 2); else unrest (kind 2)
// elsewhere, card 4 ($LocName, $Direction), or here, card 3; else card 3
// unless the dragon or the mines were told of; no time passes
int
CityVisit::_Affairs()
{
    fNewsQueue.clear();
    if (_AnyEvent(4)) {
        fNewsQueue.push_back(std::make_pair(SCREEN_AFFAIRS_DRAGON,
            _EventLocation(0x1C, 4)));
    }
    if (_AnyEvent(12)) {
        fNewsQueue.push_back(std::make_pair(SCREEN_AFFAIRS_MINES,
            _EventLocation(0x1C, 12)));
    }
    if ((_CityState() & 0x80) != 0)
        fNewsQueue.push_back(std::make_pair(SCREEN_AFFAIRS_OVERTHROWN, -1));
    else if (_Marked(0x42))
        fNewsQueue.push_back(std::make_pair(SCREEN_AFFAIRS_CRUSHED, -1));
    else if (_AnyEvent(2)) {
        const int place = _EventLocation(0x1C, 2);
        if (place != fCity)
            fNewsQueue.push_back(std::make_pair(SCREEN_AFFAIRS_UNREST, place));
        else
            fNewsQueue.push_back(std::make_pair(SCREEN_AFFAIRS_NONE, -1));
    } else if (!_AnyEvent(4) && !_AnyEvent(12))
        fNewsQueue.push_back(std::make_pair(SCREEN_AFFAIRS_NONE, -1));
    return _NextNews();
}


// The gossip (state 0xAE, file 0x10F96E): the location's state first
// (bit 1 prices, card 1; else bit 2 rats, card 2), then card 9 (bit
// 0x80, the new rulers), 8 (mark 0x42, the traitors), 3 (unrest here),
// else by the city's property 0x21 % 20 (a signed remainder): over 14
// card 5, over 9 card 6, over 4 card 7, else card 0 (card 4, for 20,
// never comes); an hour
int
CityVisit::_Gossip()
{
    if (fClock != NULL)
        fClock->AddHours(1);
    fNewsQueue.clear();
    const uint8 state = _CityState();
    if ((state & 0x01) != 0)
        fNewsQueue.push_back(std::make_pair(SCREEN_GOSSIP_PRICES, -1));
    else if ((state & 0x02) != 0)
        fNewsQueue.push_back(std::make_pair(SCREEN_GOSSIP_RATS, -1));
    const int r = int16(_PeopleSeed()) % 20;
    if ((state & 0x80) != 0)
        fNewsQueue.push_back(std::make_pair(SCREEN_GOSSIP_NEW_RULERS, -1));
    else if (_Marked(0x42))
        fNewsQueue.push_back(std::make_pair(SCREEN_GOSSIP_TRAITORS, -1));
    else if (_EventHere(2))
        fNewsQueue.push_back(std::make_pair(SCREEN_GOSSIP_POLITICS, -1));
    else if (r > 14)
        fNewsQueue.push_back(std::make_pair(SCREEN_GOSSIP_JOKES, -1));
    else if (r > 9)
        fNewsQueue.push_back(std::make_pair(SCREEN_GOSSIP_DULL, -1));
    else if (r > 4)
        fNewsQueue.push_back(std::make_pair(SCREEN_GOSSIP_NOTHING_EVER, -1));
    else
        fNewsQueue.push_back(std::make_pair(SCREEN_GOSSIP_NOTHING, -1));
    return _NextNews();
}


// The news' next card, with the variables of its place, or back to the
// news (DS:E896)
int
CityVisit::_NextNews()
{
    if (fNewsQueue.empty())
        return SCREEN_NEWS;
    const std::pair<int, int> next = fNewsQueue.front();
    fNewsQueue.erase(fNewsQueue.begin());
    if (next.second >= 0)
        _SetPlaceVariables(next.second, fCity);
    return next.first;
}


// An event counts from its start (0E76:3180) until its end; the game
// takes ended events away as time passes (inferred)
bool
CityVisit::_EventRunning(const world_event& e) const
{
    return fClock == NULL
        || (EventStarted(e, *fClock) && !EventEnded(e, *fClock));
}


// An event of the world (category 8) of a kind (0E76:32FE)
bool
CityVisit::_AnyEvent(int kind) const
{
    if (fEvents == NULL)
        return false;
    for (const world_event& e : *fEvents) {
        if (e.category == 8 && e.kind == kind && _EventRunning(e))
            return true;
    }
    return false;
}


// One here (0E76:3470)
bool
CityVisit::_EventHere(int kind) const
{
    if (fEvents == NULL)
        return false;
    for (const world_event& e : *fEvents) {
        if (e.category == 8 && e.kind == kind && e.location == fCity
                && _EventRunning(e))
            return true;
    }
    return false;
}


// One of category 28 (its people) or 8 here, of a subject (0E76:360C)
bool
CityVisit::_EventHere(int kind, int subject) const
{
    if (fEvents == NULL)
        return false;
    for (const world_event& e : *fEvents) {
        if ((e.category == 0x1C || e.category == 8) && e.kind == kind
                && e.subject == subject && e.location == fCity
                && _EventRunning(e))
            return true;
    }
    return false;
}


// The place of the first event of a category (28 takes 8 too) and kind
// (0E76:3C28, which does not look at the dates), or -1
int
CityVisit::_EventLocation(int category, int kind) const
{
    if (fEvents == NULL)
        return -1;
    for (const world_event& e : *fEvents) {
        if ((e.category == category || (category == 0x1C && e.category == 8))
                && e.kind == kind)
            return e.location;
    }
    return -1;
}


// The city's state: its location record's byte +0x14 (property 0x20)
uint8
CityVisit::_CityState() const
{
    if (fLocationFlags == NULL || fCity < 0
            || fCity >= int(fLocationFlags->size()))
        return 0;
    return (*fLocationFlags)[size_t(fCity)];
}


// Octile distance on the map (1462:271A: rows count a third)
int
MapDistance(int x1, int y1, int x2, int y2)
{
    const int dx = std::abs(x2 - x1);
    const int dy = std::abs(y2 - y1) / 3;
    return dy <= dx ? dx + dy / 2 : dy + dx / 2;
}


// The direction from one location to another (1462:29FA): West or East
// when |dx| / 2 >= |dy| / 3, North or South when the reverse, else the
// diagonal; "North"... as the game's words (290E:20E3)
std::string
CityVisit::_DirectionTo(int from, int place) const
{
    const LocationFile& locations = fData.Locations();
    if (place < 0 || uint32(place) >= locations.CountLocations()
            || from < 0 || uint32(from) >= locations.CountLocations())
        return "";
    const location& there = locations.LocationAt(uint32(place));
    const location& here = locations.LocationAt(uint32(from));
    static const char* const kDirections[8] = { "North", "Northeast", "East",
        "Southeast", "South", "Southwest", "West", "Northwest" };
    const int dx = std::abs(int(there.x) - int(here.x));
    const int dy = std::abs(int(there.y) - int(here.y)) / 3;
    int direction;
    if (dx / 2 >= dy)
        direction = there.x > here.x ? 2 : 6;
    else if (dy / 2 >= dx)
        direction = there.y > here.y ? 4 : 0;
    else if (there.x > here.x)
        direction = there.y > here.y ? 3 : 1;
    else
        direction = there.y > here.y ? 5 : 7;
    return kDirections[direction];
}


// The city nearest to a place (1462:271A): of the first 92 locations,
// not a city on the place itself, by max + min / 2 of dx, dy (dy a
// third); -1 if none
int
CityVisit::_NearestCity(int place) const
{
    const LocationFile& locations = fData.Locations();
    if (place < 0 || uint32(place) >= locations.CountLocations())
        return -1;
    const location& there = locations.LocationAt(uint32(place));
    int nearest = -1;
    int best = 9999;
    const uint32 cities = std::min(locations.CountLocations(),
        fData.Cities().CountCities());
    for (uint32 i = 0; i < cities; i++) {
        const location& c = locations.LocationAt(i);
        if (c.x == there.x && c.y == there.y && c.type == 0)
            continue;
        const int d = MapDistance(there.x, there.y, c.x, c.y);
        if (d < best) {
            best = d;
            nearest = int(i);
        }
    }
    return nearest;
}


// $LocName (a place), $Direction (from `from` to it), $NearestCity (the
// city nearest to it) and $Direction2 (from that city to it)
void
CityVisit::_SetPlaceVariables(int place, int from)
{
    const LocationFile& locations = fData.Locations();
    if (place < 0 || uint32(place) >= locations.CountLocations())
        return;
    fVariables["LocName"] = locations.LocationAt(uint32(place)).name;
    fVariables["Direction"] = _DirectionTo(from, place);
    const int nearest = _NearestCity(place);
    if (nearest >= 0) {
        fVariables["NearestCity"] = locations.LocationAt(uint32(nearest)).name;
        fVariables["Direction2"] = _DirectionTo(nearest, place);
    }
}


// A new event (0E76:2C4E): created and started now, over after `hours`
// (1367:09EA), or never (9999: 31 December 1499, 23h); +0x2A..+0x2E 0.
// Returns its index.
int
CityVisit::_AddEvent(int16 unknown1A, int16 location, int16 unknown20,
    int16 category, int16 subject, int16 unknown1E, int16 unknown26,
    int hours, int16 unknown24, int16 kind)
{
    if (fEvents == NULL)
        return -1;
    world_event e = world_event();
    const GameTime now = fClock != NULL ? GameTime(fClock->Year(),
        fClock->Month(), fClock->Day(), fClock->Hour()) : GameTime();
    e.created = event_date{ int16(now.Hour()), int16(now.Day()),
        int16(now.Month()), int16(now.Year()) };
    e.start = e.created;
    if (hours == 9999)
        e.end = event_date{ 23, 31, 12, 1499 };
    else {
        GameTime end = now;
        end.AddHours(uint32(hours));
        e.end = event_date{ int16(end.Hour()), int16(end.Day()),
            int16(end.Month()), int16(end.Year()) };
    }
    e.unknown1A = unknown1A;
    e.location = location;
    e.unknown20 = unknown20;
    e.category = category;
    e.subject = subject;
    e.unknown1E = unknown1E;
    e.unknown26 = unknown26;
    e.unknown24 = unknown24;
    e.kind = kind;
    fEvents->push_back(e);
    return int(fEvents->size()) - 1;
}


// The first event matching (0E76:3B62; -1: any; category 28 takes 8
// too, 0E76:324C), or -1
int
CityVisit::_FindEvent(int category, int subject, int location,
    int unknown1A, int kind, int unknown2A) const
{
    for (size_t i = 0; fEvents != NULL && i < fEvents->size(); i++) {
        const world_event& e = (*fEvents)[i];
        if ((e.category == category || (category == 0x1C && e.category == 8))
                && (subject == -1 || e.subject == subject)
                && (location == -1 || e.location == location)
                && (unknown1A == -1 || e.unknown1A == unknown1A)
                && (kind == -1 || e.kind == kind)
                && (unknown2A == -1 || e.unknown2A == unknown2A))
            return int(i);
    }
    return -1;
}

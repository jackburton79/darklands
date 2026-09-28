/*
 * EventFile.h
 * The game's events (EVENTS.TMP, and the same records in the saved
 * games): what happens in the world, by location and date (unrest, a
 * dragon, robber knights, quests...). Only the fields the game's
 * queries use are named. See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

class GameTime;

struct event_date {
    int16 hour;
    int16 day;
    int16 month;				// compared with the game's (0-based) month
    int16 year;
};

struct world_event {
    int16 subject;				// +0x00: compared by some queries
    event_date created;			// +0x02
    event_date start;			// +0x0A: from then on (0E76:3180)
    event_date end;				// +0x12: over from then on (0E76:31E6)
    int16 unknown1A;			// +0x1A: compared by 0E76:3C28
    int16 location;				// +0x1C: index into DARKLAND.LOC, -1: none
    int16 unknown1E;
    int16 unknown20;
    int16 category;				// +0x22: 8 for the world's events
    int16 unknown24;
    int16 unknown26;
    int16 kind;					// +0x28: within the category
    int16 unknown2A;			// compared by 0E76:3C28
    int16 unknown2C;
    int16 unknown2E;
};

static const size_t kEventRecordSize = 48;

// A record of the game's 48 bytes
world_event ReadEvent(const uint8* record);
// Whether an event has started (its start is not after `now`), and
// whether it is over, as the game compares the dates (year, month, day,
// hour).
bool EventStarted(const world_event& e, const GameTime& now);
bool EventEnded(const world_event& e, const GameTime& now);

class EventFile {
public:
    explicit		EventFile(const std::string& fileName);	// throws on error

    const std::vector<world_event>& Events() const	{ return fEvents; }

private:
    std::vector<world_event> fEvents;
};

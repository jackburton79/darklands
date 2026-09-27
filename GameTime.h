/*
 * GameTime.h
 * The game's date and time. The calendar is the Julian one without leap
 * years; the hours are named after the eight monastic hours, three modern
 * hours each (manual p. 21, "Medieval Timekeeping"). The saved games
 * store year, month (0-based), day and hour.
 */
#pragma once

#include "SupportDefs.h"

#include <functional>
#include <string>

class GameTime {
public:
    static const uint16 kNightStart	= 21;	// Compline
    static const uint16 kNightEnd	= 6;	// Prime

                    GameTime();		// 1 January 1400, midnight
                    GameTime(uint16 year, uint16 month, uint16 day,
                        uint16 hour);

    uint16			Year() const	{ return fYear; }
    uint16			Month() const	{ return fMonth; }	// 0 = January
    uint16			Day() const		{ return fDay; }	// 1-based
    uint16			Hour() const	{ return fHour; }
    uint16			Minute() const	{ return fMinute; }

    // Called after time passes (at least a minute), with whether a day
    // began: the game changes the party then (see PassTime()).
    typedef std::function<void(bool newDay)> listener;
    void			SetListener(const listener& handler)	{ fListener = handler; }

    void			AddMinutes(uint32 minutes);
    void			AddHours(uint32 hours)	{ AddMinutes(hours * 60); }

    // Between Compline (9 PM) and Prime (6 AM): the curfew (inferred:
    // "a late-night curfew, after which it is illegal to be on the
    // streets until dawn", manual p. 66).
    bool			IsNight() const;

    const char*		MonthName() const;	// "January"
    const char*		BellName() const;	// "Matins", "Latins"... "Compline"
    // "Terce, 28 May": the game hides the year ("sometime in the 15th
    // Century", manual p. 20)
    std::string		Describe() const;

private:
    uint16			fYear;
    uint16			fMonth;
    uint16			fDay;
    uint16			fHour;
    uint16			fMinute;
    listener		fListener;
};

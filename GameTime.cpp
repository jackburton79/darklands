#include "GameTime.h"

#include <sstream>

const uint16 GameTime::kNightStart;
const uint16 GameTime::kNightEnd;

static const uint16 kDaysInMonth[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static const char* kMonthNames[12] = {
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December"
};

// Each starts at hour 3 * index (manual p. 21; "Latins" there is an OCR
// error of the scan, the office is Lauds)
static const char* kBellNames[8] = {
    "Matins", "Lauds", "Prime", "Terce", "Sext", "Nones", "Vespers",
    "Compline"
};



GameTime::GameTime()
    :
    fYear(1400),
    fMonth(0),
    fDay(1),
    fHour(0),
    fMinute(0)
{
}


GameTime::GameTime(uint16 year, uint16 month, uint16 day, uint16 hour)
    :
    fYear(year),
    fMonth(month % 12),
    fDay(day),
    fHour(hour % 24),
    fMinute(0)
{
    if (fDay < 1)
        fDay = 1;
    if (fDay > kDaysInMonth[fMonth])
        fDay = kDaysInMonth[fMonth];
}


void
GameTime::AddMinutes(uint32 minutes)
{
    uint32 total = fMinute + minutes;
    fMinute = uint16(total % 60);
    total = fHour + total / 60;
    fHour = uint16(total % 24);
    uint32 days = total / 24;
    while (days > 0) {
        days--;
        if (++fDay > kDaysInMonth[fMonth]) {
            fDay = 1;
            if (++fMonth == 12) {
                fMonth = 0;
                fYear++;
            }
        }
    }
}


bool
GameTime::IsNight() const
{
    return fHour >= kNightStart || fHour < kNightEnd;
}


const char*
GameTime::MonthName() const
{
    return kMonthNames[fMonth];
}


const char*
GameTime::BellName() const
{
    return kBellNames[fHour / 3];
}


std::string
GameTime::Describe() const
{
    std::ostringstream text;
    text << BellName() << ", " << fDay << " " << MonthName();
    return text.str();
}

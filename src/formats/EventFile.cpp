#include "EventFile.h"

#include "FileStream.h"
#include "GameTime.h"
#include "Stream.h"

#include <memory>
#include <stdexcept>


static int16
Int16At(const uint8* data, size_t offset)
{
    return int16(data[offset] | (data[offset + 1] << 8));
}


static event_date
DateAt(const uint8* data, size_t offset)
{
    return event_date{ Int16At(data, offset), Int16At(data, offset + 2),
        Int16At(data, offset + 4), Int16At(data, offset + 6) };
}


world_event
ReadEvent(const uint8* r)
{
    world_event e;
    e.subject = Int16At(r, 0x00);
    e.created = DateAt(r, 0x02);
    e.start = DateAt(r, 0x0A);
    e.end = DateAt(r, 0x12);
    e.unknown1A = Int16At(r, 0x1A);
    e.location = Int16At(r, 0x1C);
    e.unknown1E = Int16At(r, 0x1E);
    e.unknown20 = Int16At(r, 0x20);
    e.category = Int16At(r, 0x22);
    e.unknown24 = Int16At(r, 0x24);
    e.unknown26 = Int16At(r, 0x26);
    e.kind = Int16At(r, 0x28);
    e.unknown2A = Int16At(r, 0x2A);
    e.unknown2C = Int16At(r, 0x2C);
    e.unknown2E = Int16At(r, 0x2E);
    return e;
}


// -1, 0, 1 as `date` is before, at or after `now` (0E76:3180: year,
// month, day, hour in turn)
static int
Compare(const event_date& date, const GameTime& now)
{
    const int fields[4][2] = {
        { date.year, now.Year() }, { date.month, now.Month() },
        { date.day, now.Day() }, { date.hour, now.Hour() } };
    for (const auto& field : fields) {
        if (field[0] != field[1])
            return field[0] < field[1] ? -1 : 1;
    }
    return 0;
}


bool
EventStarted(const world_event& e, const GameTime& now)
{
    return Compare(e.start, now) <= 0;
}


bool
EventEnded(const world_event& e, const GameTime& now)
{
    return Compare(e.end, now) <= 0;
}


// verified: a word count, then the records (2 + 28 · 48 = 1346); the
// game writes the count of its used slots, then each record (file
// 0x6141E)
EventFile::EventFile(const std::string& fileName)
{
    std::unique_ptr<Stream> stream(
        new FileStream(fileName.c_str(), FileStream::READ_ONLY));
    const size_t size = stream->Size();
    std::vector<uint8> data(size);
    if (size < 2 || stream->ReadAt(0, data.data(), size) != (ssize_t)size)
        throw std::runtime_error("EventFile: file too small");
    const size_t count = data[0] | (data[1] << 8);
    if (size != 2 + count * kEventRecordSize)
        throw std::runtime_error("EventFile: unexpected file size");
    for (size_t i = 0; i < count; i++)
        fEvents.push_back(ReadEvent(&data[2 + i * kEventRecordSize]));
}

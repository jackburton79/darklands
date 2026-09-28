#include "SaveFile.h"

#include "FileStream.h"
#include "LocationFile.h"
#include "Stream.h"

#include <cstring>
#include <memory>
#include <stdexcept>

// Header layout (see docs/formats.md)
static const size_t kLocationNameOffset	= 0x00;
static const size_t kLocationNameLength	= 12;
static const size_t kLabelOffset		= 0x15;
static const size_t kLabelLength		= 23;
static const size_t kSeedOffset			= 0x64;	// DS:9C4A (verified: the
                                                // game's save code)
static const size_t kDateOffset			= 0x68;	// year, month, day, hour
static const size_t kMoneyOffset		= 0x70;	// florins, groschen, pfennigs
static const size_t kFameOffset			= 0x7A;
static const size_t kBankNotesOffset	= 0x8C;
static const size_t kStoneOffset		= 0x92;
static const size_t kLocationOffset		= 0x7C;
static const size_t kCoordinatesOffset	= 0x7E;
static const size_t kLeaderOffset		= 0xA1;
static const size_t kCharacterCountOffset = 0xF1;
static const size_t kIndicesOffset		= 0xF3;
static const size_t kImagesOffset		= 0xFD;
static const size_t kColorsOffset		= 0x111;	// 24 bytes per slot
static const size_t kCharactersOffset	= 0x189;
// After the characters: a word count of 48-byte event records, then a
// word count of 58-byte location records, as in DARKLAND.LOC (verified)
static const size_t kLocationSize		= 58;
static const size_t kReputationOffset	= 0x12;	// in a location record
static const size_t kLocationFlagsOffset = 0x14;
static const size_t kEnterStateOffset	= 0x0C;


static uint16
WordAt(const std::vector<uint8>& data, size_t offset)
{
    return uint16(data[offset] | (data[offset + 1] << 8));
}


static std::string
StringAt(const std::vector<uint8>& data, size_t offset, size_t length)
{
    const char* string = (const char*)&data[offset];
    return LocationFile::DecodeName(string, strnlen(string, length));
}


SaveFile::SaveFile(const std::string& fileName)
{
    std::unique_ptr<Stream> stream(
        new FileStream(fileName.c_str(), FileStream::READ_ONLY));
    const size_t size = stream->Size();
    std::vector<uint8> data(size);
    if (size < kCharactersOffset
            || stream->ReadAt(0, data.data(), size) != (ssize_t)size)
        throw std::runtime_error("SaveFile: file too small");

    fLocationName = StringAt(data, kLocationNameOffset, kLocationNameLength);
    fLabel = StringAt(data, kLabelOffset, kLabelLength);
    fSeed = WordAt(data, kSeedOffset);
    fDate = GameTime(WordAt(data, kDateOffset), WordAt(data, kDateOffset + 2),
        WordAt(data, kDateOffset + 4), WordAt(data, kDateOffset + 6));
    const uint16 location = WordAt(data, kLocationOffset);
    fLocation = location == 0xFFFF ? -1 : int(location);
    fX = WordAt(data, kCoordinatesOffset);
    fY = WordAt(data, kCoordinatesOffset + 2);

    const size_t count = WordAt(data, kCharacterCountOffset);
    if (size < kCharactersOffset + count * kCharacterRecordSize)
        throw std::runtime_error("SaveFile: truncated characters");
    for (size_t i = 0; i < count; i++) {
        fCharacters.push_back(ReadCharacter(
            &data[kCharactersOffset + i * kCharacterRecordSize]));
    }
    size_t offset = kCharactersOffset + count * kCharacterRecordSize;
    if (offset + 2 <= size) {
        const size_t events = WordAt(data, offset);
        offset += 2;
        if (offset + events * kEventRecordSize > size)
            throw std::runtime_error("SaveFile: truncated events");
        for (size_t i = 0; i < events; i++)
            fEvents.push_back(ReadEvent(&data[offset + i * kEventRecordSize]));
        offset += events * kEventRecordSize;
        if (offset + 2 <= size) {
            const size_t locations = WordAt(data, offset);
            offset += 2;
            if (offset + locations * kLocationSize <= size) {
                for (size_t i = 0; i < locations; i++) {
                    const size_t record = offset + i * kLocationSize;
                    fReputations.push_back(int16(WordAt(data,
                        record + kReputationOffset)));
                    fLocationFlags.push_back(data[record + kLocationFlagsOffset]);
                    fEnterStates.push_back(WordAt(data,
                        record + kEnterStateOffset));
                }
            }
        }
    }

    fParty = MakeParty(fCharacters, &data[kIndicesOffset],
        &data[kImagesOffset], data[kLeaderOffset], &data[kColorsOffset]);
    fParty.cash = money{ WordAt(data, kMoneyOffset),
        WordAt(data, kMoneyOffset + 2), WordAt(data, kMoneyOffset + 4) };
    fParty.fame = WordAt(data, kFameOffset);
    fParty.bankNotes = WordAt(data, kBankNotesOffset);
    fParty.philosopherStone = WordAt(data, kStoneOffset);
}

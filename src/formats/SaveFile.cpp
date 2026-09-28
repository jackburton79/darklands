#include "SaveFile.h"

#include "FileStream.h"
#include "LocationFile.h"
#include "Stream.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

// Header layout (see docs/formats.md)
static const size_t kLocationNameOffset	= 0x00;
static const size_t kLocationNameLength	= 12;
static const size_t kLocationNameField	= 21;	// written by the game
static const size_t kLabelOffset		= 0x15;
static const size_t kLabelLength		= 23;
static const size_t kLabelField			= 79;
static const size_t kSeedOffset			= 0x64;	// DS:9C4A (verified: the
                                                // game's save code)
static const size_t kDateOffset			= 0x68;	// year, month, day, hour
static const size_t kMoneyOffset		= 0x70;	// florins, groschen, pfennigs
static const size_t kFameOffset			= 0x7A;
static const size_t kBankNotesOffset	= 0x8C;
static const size_t kStoneOffset		= 0x92;
static const size_t kLocationOffset		= 0x7C;
static const size_t kCoordinatesOffset	= 0x7E;
static const size_t kStateOffset		= 0x82;	// DS:A772
static const size_t kDifficultyOffset	= 0x96;	// DS:906A, a byte
static const size_t kMapOffset			= 0xA4;	// DS:A891: 3 on the map
static const size_t kLeaderOffset		= 0xA1;
static const size_t kMembersOffset		= 0xEF;	// DS:A67E
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
    fState = WordAt(data, kStateOffset);
    fDifficulty = data[kDifficultyOffset];
    fBytes = data;
}


static void
PutWord(std::vector<uint8>& data, size_t offset, uint16 value)
{
    data[offset] = uint8(value & 0xFF);
    data[offset + 1] = uint8(value >> 8);
}


static void
PutString(std::vector<uint8>& data, size_t offset, size_t length,
    const std::string& utf8)
{
    const std::string text = LocationFile::EncodeName(utf8);
    std::fill(data.begin() + offset, data.begin() + offset + length, 0);
    std::copy(text.begin(), text.begin() + std::min(text.size(), length - 1),
        data.begin() + offset);
}


// The header as the game's save code writes it (file 0x751A8...), then
// the party's characters, the events, the locations and what followed
// them in this file (the inns' caches: kept)
void
SaveFile::Write(const std::string& fileName, const saved_game& game,
    const LocationFile& locations) const
{
    const party& members = *game.members;
    if (members.members.empty() || members.members.size() > kMaxPartySize)
        throw std::runtime_error("SaveFile: invalid party");
    std::vector<uint8> data(fBytes.begin(),
        fBytes.begin() + kCharactersOffset);
    const bool placed = game.location >= 0
        && uint32(game.location) < locations.CountLocations();
    PutString(data, kLocationNameOffset, kLocationNameField,
        placed ? locations.LocationAt(uint32(game.location)).name
            : "Wilderness");
    PutString(data, kLabelOffset, kLabelField, game.label);
    PutWord(data, kSeedOffset, game.seed);
    PutWord(data, kDateOffset, game.date.Year());
    PutWord(data, kDateOffset + 2, game.date.Month());
    PutWord(data, kDateOffset + 4, game.date.Day());
    PutWord(data, kDateOffset + 6, game.date.Hour());
    PutWord(data, kMoneyOffset, members.cash.florins);
    PutWord(data, kMoneyOffset + 2, members.cash.groschen);
    PutWord(data, kMoneyOffset + 4, members.cash.pfennigs);
    PutWord(data, kFameOffset, members.fame);
    PutWord(data, kLocationOffset, placed ? uint16(game.location) : 0xFFFF);
    PutWord(data, kCoordinatesOffset, game.x);
    PutWord(data, kCoordinatesOffset + 2, game.y);
    PutWord(data, kStateOffset, game.state);
    PutWord(data, kBankNotesOffset, members.bankNotes);
    PutWord(data, kStoneOffset, members.philosopherStone);
    PutWord(data, kMapOffset, placed ? 0 : 3);
    const size_t count = members.members.size();
    data[kLeaderOffset] = uint8(members.leader);
    PutWord(data, kMembersOffset, uint16(count));
    PutWord(data, kCharacterCountOffset, uint16(count));
    for (size_t slot = 0; slot < kMaxPartySize; slot++) {
        PutWord(data, kIndicesOffset + slot * 2,
            slot < count ? uint16(slot) : 0xFFFF);
        if (slot < members.images.size()) {
            std::string image = members.images[slot];
            image.resize(4, '\0');
            std::copy(image.begin(), image.begin() + 4,
                data.begin() + kImagesOffset + slot * 4);
        }
        if (slot < members.colors.size() && members.colors[slot].size() == 24) {
            std::copy(members.colors[slot].begin(), members.colors[slot].end(),
                data.begin() + kColorsOffset + slot * 24);
        }
    }

    // the characters, the events
    data.resize(kCharactersOffset + count * kCharacterRecordSize);
    for (size_t i = 0; i < count; i++) {
        WriteCharacter(members.members[i],
            &data[kCharactersOffset + i * kCharacterRecordSize]);
    }
    const size_t events = game.events != NULL ? game.events->size() : 0;
    size_t offset = data.size();
    data.resize(offset + 2 + events * kEventRecordSize);
    PutWord(data, offset, uint16(events));
    for (size_t i = 0; i < events; i++)
        WriteEvent((*game.events)[i], &data[offset + 2 + i * kEventRecordSize]);

    // the locations: this file's records, else DARKLAND.LOC's, with the
    // state of the game
    size_t old = kCharactersOffset + WordAt(fBytes, kCharacterCountOffset)
        * kCharacterRecordSize;
    old += 2 + WordAt(fBytes, old) * kEventRecordSize;
    const size_t oldLocations = WordAt(fBytes, old);
    old += 2;
    const size_t locationCount = std::max(oldLocations,
        size_t(locations.CountLocations()));
    offset = data.size();
    data.resize(offset + 2 + locationCount * kLocationSize);
    PutWord(data, offset, uint16(locationCount));
    for (size_t i = 0; i < locationCount; i++) {
        const size_t record = offset + 2 + i * kLocationSize;
        const uint8* source = i < oldLocations ? &fBytes[old + i * kLocationSize]
            : locations.RecordAt(uint32(i));
        std::copy(source, source + kLocationSize, data.begin() + record);
        if (game.reputations != NULL && i < game.reputations->size())
            PutWord(data, record + kReputationOffset,
                uint16((*game.reputations)[i]));
        if (game.locationFlags != NULL && i < game.locationFlags->size())
            data[record + kLocationFlagsOffset] = (*game.locationFlags)[i];
        if (game.enterStates != NULL && i < game.enterStates->size())
            PutWord(data, record + kEnterStateOffset, (*game.enterStates)[i]);
    }
    data.insert(data.end(), fBytes.begin() + old + oldLocations * kLocationSize,
        fBytes.end());

    FILE* file = fopen(fileName.c_str(), "wb");
    if (file == NULL)
        throw std::runtime_error("SaveFile: cannot create " + fileName);
    const bool written = fwrite(data.data(), 1, data.size(), file) == data.size();
    if (fclose(file) != 0 || !written)
        throw std::runtime_error("SaveFile: cannot write " + fileName);
}

#include "ExeNames.h"

#include "LocationFile.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

// Data segments are at segment · 16 + this in the file (docs/exe.md)
static const uint32 kDataBase		= 0x15EC20;
static const uint32 kNamesSegment	= 0x290E;
// The far pointer tables in segment 290E and their lengths (1367:0DB4)
static const uint32 kMaleTable		= 0x235F;
static const size_t kMaleCount		= 108;
static const uint32 kFemaleTable	= 0x2517;
static const size_t kFemaleCount	= 88;
static const uint32 kSurnameTable	= 0x267F;
static const size_t kSurnameCount	= 146;
static const size_t kMaxNameLength	= 30;


static uint16
WordAt(const std::vector<uint8>& data, size_t offset)
{
    return uint16(data[offset] | (data[offset + 1] << 8));
}


static std::vector<std::string>
ReadNames(const std::vector<uint8>& data, uint32 table, size_t count)
{
    const size_t start = kNamesSegment * 16 + kDataBase + table;
    if (start + count * 4 > data.size())
        throw std::runtime_error("ExeNames: name table past the end");
    std::vector<std::string> names;
    for (size_t i = 0; i < count; i++) {
        const uint16 offset = WordAt(data, start + i * 4);
        const uint16 segment = WordAt(data, start + i * 4 + 2);
        const size_t address = size_t(segment) * 16 + kDataBase + offset;
        if (address >= data.size())
            throw std::runtime_error("ExeNames: name past the end");
        const char* text = reinterpret_cast<const char*>(&data[address]);
        const size_t length = strnlen(text,
            std::min(kMaxNameLength, data.size() - address));
        if (length == 0 || length == kMaxNameLength)
            throw std::runtime_error("ExeNames: invalid name");
        names.push_back(LocationFile::DecodeName(text, length));
    }
    return names;
}


ExeNames::ExeNames(const std::string& exePath)
{
    std::ifstream file(exePath.c_str(), std::ios::binary);
    if (!file)
        throw std::runtime_error("ExeNames: cannot open " + exePath);
    const std::vector<uint8> data((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    fMale = ReadNames(data, kMaleTable, kMaleCount);
    fFemale = ReadNames(data, kFemaleTable, kFemaleCount);
    fSurnames = ReadNames(data, kSurnameTable, kSurnameCount);
}


// 1367:0DB4, kind 0: srand(seed), a draw thrown away, a first name, a
// space, a surname
std::string
ExeNames::MaleName(uint16 seed) const
{
    MscRandom random(seed);
    random.Below(1000);
    const std::string& first = fMale[random.Below(int(fMale.size()))];
    return first + " " + fSurnames[random.Below(int(fSurnames.size()))];
}


int
MscRandom::Next()
{
    fSeed = fSeed * 0x343FD + 0x269EC3;
    return int((fSeed >> 16) & 0x7FFF);
}


// 0410:0008
int
MscRandom::Below(int n)
{
    return int((int32(Next()) * 2 * n) >> 16);
}

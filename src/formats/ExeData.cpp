#include "ExeData.h"

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
// The jobs: 31 records of 18 bytes in DGROUP (321A); their names are the
// far pointers at 290E:219B, by the record's first byte
static const uint32 kDataGroup		= 0x321A;
// The weapon table: 63 entries per array (verified: the codes and the
// item names agree, docs/exe.md)
static const uint32 kWeaponSegment	= 0x20A5;
static const size_t kWeaponCount	= 63;
static const uint32 kWeaponCategories = 0x75AC;
static const uint32 kWeaponCodes	= 0x75EB;	// 2 bytes each
static const uint32 kWeaponSpeeds	= 0x7669;
static const uint32 kWeaponHands	= 0x76A8;	// hands << 4 | penetration
static const uint32 kWeaponDamages	= 0x76E7;
static const uint32 kWeaponSkills	= 0x7726;
static const uint32 kWeaponMinimum	= 0x7765;
static const uint32 kWeaponMaximum	= 0x77A4;
static const uint32 kWeaponRanges	= 0x77E3;
static const uint32 kArmorStrengths	= 0x781E;	// by item type
static const size_t kArmorCount		= 100;
static const uint32 kJobTable		= 0x3ACE;
static const size_t kJobCount		= 31;
static const size_t kJobSize		= 18;
static const uint32 kJobNameTable	= 0x219B;
static const size_t kJobNameCount	= 32;


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
        throw std::runtime_error("ExeData: name table past the end");
    std::vector<std::string> names;
    for (size_t i = 0; i < count; i++) {
        const uint16 offset = WordAt(data, start + i * 4);
        const uint16 segment = WordAt(data, start + i * 4 + 2);
        const size_t address = size_t(segment) * 16 + kDataBase + offset;
        if (address >= data.size())
            throw std::runtime_error("ExeData: name past the end");
        const char* text = reinterpret_cast<const char*>(&data[address]);
        const size_t length = strnlen(text,
            std::min(kMaxNameLength, data.size() - address));
        if (length == 0 || length == kMaxNameLength)
            throw std::runtime_error("ExeData: invalid name");
        names.push_back(LocationFile::DecodeName(text, length));
    }
    return names;
}


ExeData::ExeData(const std::string& exePath)
{
    std::ifstream file(exePath.c_str(), std::ios::binary);
    if (!file)
        throw std::runtime_error("ExeData: cannot open " + exePath);
    const std::vector<uint8> data((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    fMale = ReadNames(data, kMaleTable, kMaleCount);
    fFemale = ReadNames(data, kFemaleTable, kFemaleCount);
    fSurnames = ReadNames(data, kSurnameTable, kSurnameCount);

    const std::vector<std::string> jobNames = ReadNames(data, kJobNameTable,
        kJobNameCount);
    const size_t jobs = kDataGroup * 16 + kDataBase + kJobTable;
    if (jobs + kJobCount * kJobSize > data.size())
        throw std::runtime_error("ExeData: job table past the end");
    for (size_t i = 0; i < kJobCount; i++) {
        const uint8* record = &data[jobs + i * kJobSize];
        if (record[0] >= jobNames.size())
            throw std::runtime_error("ExeData: invalid job name");
        exe_job job;
        job.name = jobNames[record[0]];
        job.cityFlags = uint32(record[2] | record[3] << 8 | record[4] << 16
            | record[5] << 24);
        job.excluded = uint32(record[6] | record[7] << 8 | record[8] << 16
            | record[9] << 24);
        job.locationFlags = record[10];
        job.attribute = record[11];
        job.attributeBase = record[12];
        job.skills[0] = record[13];
        job.skillBases[0] = record[14];
        job.skills[1] = record[15];
        job.skillBases[1] = record[16];
        job.multiplier = record[17];
        if (job.attribute >= 7 || job.skills[0] >= 19 || job.skills[1] >= 19)
            throw std::runtime_error("ExeData: invalid job");
        fJobs.push_back(job);
    }

    const size_t weapons = kWeaponSegment * 16 + kDataBase;
    if (weapons + kWeaponRanges + kWeaponCount > data.size())
        throw std::runtime_error("ExeData: weapon table past the end");
    const uint8* w = &data[weapons];
    for (size_t i = 0; i < kWeaponCount; i++) {
        exe_weapon weapon;
        weapon.category = w[kWeaponCategories + i];
        weapon.code = std::string((const char*)&w[kWeaponCodes + 2 * i], 2);
        weapon.speed = w[kWeaponSpeeds + i];
        weapon.hands = w[kWeaponHands + i] >> 4;
        weapon.penetration = w[kWeaponHands + i] & 0x0F;
        weapon.damage = w[kWeaponDamages + i];
        weapon.skill = w[kWeaponSkills + i];
        weapon.minStrength = w[kWeaponMinimum + i];
        weapon.maxStrength = w[kWeaponMaximum + i];
        weapon.range = w[kWeaponRanges + i];
        fWeapons.push_back(weapon);
    }
    if (weapons + kArmorStrengths + kArmorCount > data.size())
        throw std::runtime_error("ExeData: armor table past the end");
    fArmor.assign(w + kArmorStrengths, w + kArmorStrengths + kArmorCount);
}


int
ExeData::ArmorStrength(int type) const
{
    return type >= 0 && size_t(type) < fArmor.size() ? fArmor[size_t(type)] : 0;
}


// 1367:0DB4, kind 0: srand(seed), a draw thrown away, a first name, a
// space, a surname
std::string
ExeData::MaleName(uint16 seed) const
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

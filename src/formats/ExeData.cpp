#include "ExeData.h"

#include "LocationFile.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
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
static const uint32 kWeaponAmmo		= 0x7822;	// verified (file 0x448A2)
static const uint32 kArmorStrengths	= 0x781E;	// by item type
static const size_t kArmorCount		= 100;
// The saints' functions: far pointers at 290E:2937 to RTLink thunks
// (09C0:xxxx, 10 bytes: E8 rel16, EA off seg, overlay) into segment 165C
// of overlay 0x27, at file 0x82940
static const uint32 kSaintTable		= 0x2937;
static const size_t kSaintCount		= 136;
static const uint32 kThunkSegment	= 0x09C0;
static const uint32 kSaintSegment	= 0x165C;
static const uint32 kSaintCode		= 0x82940;
static const uint32 kJobTable		= 0x3ACE;
static const size_t kJobCount		= 31;
static const size_t kJobSize		= 18;
static const uint32 kJobNameTable	= 0x219B;
static const size_t kJobNameCount	= 32;
// The life of a character: the far pointers to the game's texts are at
// 290E:1D43 (index 218..223 the families, 105..141 the occupations),
// the records after the textual data (docs/exe.md)
static const uint32 kTextTable		= 0x1D43;
static const size_t kFamilyNameIndex = 218;
static const size_t kOccupationNameIndex = 105;
static const uint32 kFamilyTable	= 0x2B65;
static const uint32 kOccupationTable = 0x2CC1;
static const uint32 kAgingTable		= 0x3521;
static const size_t kLifeRecordSize	= 58;


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


static inline uint16
CheckedWordAt(const std::vector<uint8>& data, size_t offset)
{
    if (offset + 2 > data.size())
        throw std::runtime_error("ExeData: saint code past the end");
    return uint16(data[offset] | (data[offset + 1] << 8));
}


// A saint's function begins with `enter`, `push si` (and `push di`),
// `mov si, [bp+0xA]` (the mode), then fills a table of words on the
// stack, `mov word [bp-x], imm` or `mov ax, imm` + `mov [bp-x], ax`, and
// for modes 0..6 returns `[bp+si-first]` (`8B C6`: mov ax, si).
static exe_saint
ReadSaint(const std::vector<uint8>& data, size_t function)
{
    if (function + 4 > data.size() || data[function] != 0xC8)
        throw std::runtime_error("ExeData: not a saint's function");
    size_t p = function + 4;
    while (p < data.size() && (data[p] == 0x56 || data[p] == 0x57))
        p++;
    if (p + 3 > data.size() || data[p] != 0x8B || data[p + 1] != 0x76
            || data[p + 2] != 0x0A)
        throw std::runtime_error("ExeData: unexpected saint's function");
    p += 3;
    std::map<int, uint16> slots;
    uint16 ax = 0;
    for (;;) {
        if (p + 5 <= data.size() && data[p] == 0xC7 && data[p + 1] == 0x46) {
            slots[int8(data[p + 2])] = CheckedWordAt(data, p + 3);
            p += 5;
        } else if (p + 3 <= data.size() && data[p] == 0xB8) {
            ax = CheckedWordAt(data, p + 1);
            p += 3;
        } else if (p + 3 <= data.size() && data[p] == 0x89
                && data[p + 1] == 0x46) {
            slots[int8(data[p + 2])] = ax;
            p += 3;
        } else
            break;
    }
    if (slots.size() != 7 || CheckedWordAt(data, p) != 0xC68B)
        throw std::runtime_error("ExeData: unexpected saint's table");
    uint16 values[7];
    int i = 0;
    int previous = slots.begin()->first - 2;
    for (const auto& slot : slots) {
        if (slot.first != previous + 2)
            throw std::runtime_error("ExeData: saint's table not contiguous");
        previous = slot.first;
        values[i++] = slot.second;
    }
    return exe_saint{ values[0], values[1], values[2], values[3], values[4],
        values[5], values[6] };
}


static std::vector<exe_life_stage>
ReadLifeStages(const std::vector<uint8>& data, uint32 table, size_t count,
    size_t firstName)
{
    const std::vector<std::string> names = ReadNames(data,
        kTextTable + uint32(firstName) * 4, count);
    const size_t start = kNamesSegment * 16 + kDataBase + table;
    if (start + count * kLifeRecordSize > data.size())
        throw std::runtime_error("ExeData: life table past the end");
    std::vector<exe_life_stage> stages;
    for (size_t i = 0; i < count; i++) {
        const uint8* record = &data[start + i * kLifeRecordSize];
        exe_life_stage stage;
        stage.name = names[i];
        stage.points = WordAt(data, start + i * kLifeRecordSize);
        memcpy(stage.attributes, record + 2, 6);
        memcpy(stage.skills, record + 8, kLifeSkillCount);
        memcpy(stage.limits, record + 27, kLifeSkillCount);
        stages.push_back(stage);
    }
    return stages;
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
    fFamilies = ReadLifeStages(data, kFamilyTable, kFamilyCount,
        kFamilyNameIndex);
    fOccupations = ReadLifeStages(data, kOccupationTable, kOccupationCount,
        kOccupationNameIndex);
    const size_t aging = kNamesSegment * 16 + kDataBase + kAgingTable;
    if (aging + kAgingCount * 6 > data.size())
        throw std::runtime_error("ExeData: aging table past the end");
    memcpy(fAging, &data[aging], sizeof(fAging));

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
    if (weapons + kWeaponAmmo + kWeaponCount > data.size())
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
        weapon.ammo = w[kWeaponAmmo + i];
        fWeapons.push_back(weapon);
    }
    if (weapons + kArmorStrengths + kArmorCount > data.size())
        throw std::runtime_error("ExeData: armor table past the end");
    fArmor.assign(w + kArmorStrengths, w + kArmorStrengths + kArmorCount);

    const size_t saints = kNamesSegment * 16 + kDataBase + kSaintTable;
    if (saints + 4 * kSaintCount > data.size())
        throw std::runtime_error("ExeData: saint table past the end");
    for (size_t i = 0; i < kSaintCount; i++) {
        const uint16 offset = CheckedWordAt(data, saints + 4 * i);
        if (CheckedWordAt(data, saints + 4 * i + 2) != kThunkSegment)
            throw std::runtime_error("ExeData: saint's function not a thunk");
        const size_t thunk = 0x1800 + kThunkSegment * 16 + offset;
        if (thunk + 10 > data.size() || data[thunk] != 0xE8
                || data[thunk + 3] != 0xEA
                || CheckedWordAt(data, thunk + 6) != kSaintSegment)
            throw std::runtime_error("ExeData: unexpected saint's thunk");
        fSaints.push_back(ReadSaint(data, kSaintCode + CheckedWordAt(data, thunk + 4)));
    }
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


// 1367:0DB4, kind 1: the same with a woman's first name
std::string
ExeData::FemaleName(uint16 seed) const
{
    MscRandom random(seed);
    random.Below(1000);
    const std::string& first = fFemale[random.Below(int(fFemale.size()))];
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

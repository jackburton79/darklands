// What a city teaches: the university (state 0x31), the library of the
// saints (0x39, the university's and the monastery's), the formulae for
// sale (0x53, the university's and the alchemist's) and the alchemist's
// other dealings (state 0x59). See docs/exe.md, "The university".

#include "CityVisitInternal.h"

#include "Character.h"
#include "Equipment.h"
#include "GameData.h"
#include "GameTime.h"
#include "ListFile.h"
#include "MsgFile.h"

#include <algorithm>
#include <cstdlib>


static const int kFormulaVersionCount = 66;		// 22 formulae, 3 versions
static const int kSaintCount = 135;


static int
Clamp(int value, int low, int high)
{
    return std::max(low, std::min(high, value));
}


// A card text with a heading and a line for each choice
static void
AddChoiceLine(std::string& text, const std::string& line)
{
    text += char(MSG_CODE_OPTION);
    text += "...";
    text += char(MSG_CODE_OPTION_TEXT);
    text += line;
    text += "\n";
}


// Entering the university (file 0xB59EC): $Number1 is the stone it can
// make, (property 0x21 + 7) & 15 + 14; $Money1 its price, the city's size
// · $Number1 · 4 groschen (not offered to a purse under it). The options
// tried this visit are forgotten when the party comes back from elsewhere
void
CityVisit::_EnterUniversity(int previous)
{
    const bool inside = previous == SCREEN_UNIVERSITY
        || previous == SCREEN_UNIVERSITY_LOST
        || previous == SCREEN_UNIVERSITY_NO_PROFESSORS
        || previous == SCREEN_UNIVERSITY_STONE
        || previous == SCREEN_UNIVERSITY_NO_STONE
        || previous == SCREEN_UNIVERSITY_SHOP
        || previous == SCREEN_UNIVERSITY_NO_SHOP
        || previous == SCREEN_UNIVERSITY_TEACHERS
        || previous == SCREEN_UNIVERSITY_NO_TEACHERS;
    if (!inside) {
        for (bool& off : fUniversityOff)
            off = false;
    }
    fMeetBack = SCREEN_UNIVERSITY;
    const int number = ((_PeopleSeed() + 7) & 0xF) + 14;
    fMeetMoney = uint32(_City().size * number * 4 * 12);
    fVariables["Number1"] = std::to_string(number);
    fVariables["Money1"] = MoneyText(fMeetMoney);
    if (fParty != NULL && !fParty->members.empty())
        _SetChosen(fParty->leader);
}


// How likely each option is (files 0xB5DEC, 0xB5F4E, 0xB60C8, 0xB6236,
// 0xB643A): (the fame / 200 + 20 + ...) / 4, 0..99. The saints take the
// best Religion, Intelligence, Perception and Speak Latin of the party;
// the others the leader's Perception, Intelligence and Speak Latin and
// the best Alchemy
int
CityVisit::_UniversityChance(int option) const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[fParty->leader];
    int sum = fParty->fame / 20 / 10 + 20;
    if (option == 0) {
        int religion = 0, intelligence = 0, perception = 0, latin = 0;
        for (const character& member : fParty->members) {
            religion = std::max(religion, int(member.skills[kSkillReligion]));
            intelligence = std::max(intelligence,
                int(member.attributes[ATTRIBUTE_INTELLIGENCE]));
            perception = std::max(perception,
                int(member.attributes[ATTRIBUTE_PERCEPTION]));
            latin = std::max(latin, int(member.skills[kSkillSpeakLatin]));
        }
        sum += religion + intelligence + perception + latin;
    } else {
        sum += leader.skills[kSkillSpeakLatin] + _BestSkill(kSkillAlchemy)
            + leader.attributes[ATTRIBUTE_PERCEPTION]
            + leader.attributes[ATTRIBUTE_INTELLIGENCE];
    }
    return Clamp(sum / 4, 0, 99);
}


// An option of the university: if random(100) is at most the chance an
// hour passes and the option works (below); else two hours pass, the
// option is closed for a week (the formulae's failure closes the saints'
// by a slip of the original), the reputation may fall by 1, and a card
// tells it. 0: card 3, the library; 1: card 4, the formulae for sale; 2:
// the stone (card 6 when it is better than the party's, which is paid,
// else card 7); 3: card 8, the rare components; 4: card 10, five
// teachers (Alchemy, Religion, Speak Latin, Read & Write, Healing: fees 90,
// 70, 50, 50, 110 pfennigs a day, level (property 0x21 % 15) + 45) for a
// week
int
CityVisit::_University(int option)
{
    static const int kLost[5] = { SCREEN_UNIVERSITY_LOST,
        SCREEN_UNIVERSITY_NO_PROFESSORS, SCREEN_UNIVERSITY_NO_STONE,
        SCREEN_UNIVERSITY_NO_SHOP, SCREEN_UNIVERSITY_NO_TEACHERS };
    fMeetBack = SCREEN_UNIVERSITY;
    if (int(fRandom() % 100) > _UniversityChance(option)) {
        if (fClock != NULL)
            fClock->AddHours(2);
        _Mark(kMarkUniversity | ((option == 1 ? 0 : option) << 8), 168);
        _ChangeReputation(-1, -1);
        fUniversityOff[option] = true;
        return kLost[option];
    }
    if (fClock != NULL)
        fClock->AddHours(1);
    switch (option) {
        case 0:
            fLibraryReturn = SCREEN_UNIVERSITY;
            return SCREEN_UNIVERSITY_LIBRARY;
        case 1:
            fFormulaCaller = fFormulaLeave = SCREEN_UNIVERSITY;
            return SCREEN_UNIVERSITY_PROFESSORS;
        case 2: {
            fUniversityOff[2] = true;
            const int quality = std::atoi(fVariables["Number1"].c_str());
            if (fParty == NULL || fParty->philosopherStone >= quality)
                return SCREEN_UNIVERSITY_NO_STONE;
            fParty->philosopherStone = uint16(quality);
            const uint32 purse = TotalPfennigs(fParty->cash);
            fParty->cash = MoneyFromPfennigs(purse - std::min(purse, fMeetMoney));
            return SCREEN_UNIVERSITY_STONE;
        }
        case 3:
            return SCREEN_UNIVERSITY_SHOP;
        default: {
            const uint32 now = fClock != NULL ? fClock->HourStamp() : 0;
            const int level = _PeopleSeed() % 15 + 45;
            static const int kTaught[5][2] = { { kSkillAlchemy, 90 },
                { kSkillReligion, 70 }, { kSkillSpeakLatin, 50 },
                { kSkillReadWrite, 50 }, { 13, 110 } };
            for (const auto& taught : kTaught) {
                _AddTutor(city_tutor{ taught[0], level, uint32(taught[1]),
                    now + 168 });
            }
            return SCREEN_UNIVERSITY_TEACHERS;
        }
    }
}


// The library (state 0x39, file 0xBC3C8): $Money1 is the gift, (the purse
// / 30 + 12, at most 240) / 12 groschen; the four saints to study come
// from random(135) of the C library's generator seeded with the seed
// global + the location, so they are the same each time
void
CityVisit::_EnterLibrary()
{
    const uint32 purse = fParty != NULL ? TotalPfennigs(fParty->cash) : 0;
    fMeetMoney = uint32(Clamp(int(purse / 30) + 12, 0, 240) / 12) * 12;
    fVariables["Money1"] = MoneyText(fMeetMoney);
    uint32 seed = uint16(fSeed + fCity);
    for (int& saint : fLibrarySaints) {
        seed = seed * 214013u + 2531011u;
        saint = int((seed >> 16) & 0x7FFF) % kSaintCount;
    }
}


// The list of the library's saints or of the formulae, then of the members
// who may use the one chosen; one more line to give up
void
CityVisit::_ShowLearnList()
{
    fSaintChoices.clear();
    const bool library = fChoosingLearn == 1;
    const std::vector<std::string>& saintNames = fData.Lists().Saints();
    const std::vector<std::string>& formulaNames
        = fData.Lists().FormulaShortNames();
    std::string text;
    if (fLearnItem < 0) {
        text = library ? "Which saint will be studied?\n"
            : "Which formula will be bought?\n";
    } else if (library) {
        text = "Who will study "
            + saintNames[size_t(fLibrarySaints[fLearnItem])] + "?\n";
    } else {
        text = "Who will learn "
            + formulaNames[size_t(fFormulaIds[fLearnItem])] + "?\n";
    }
    text += char(MSG_CODE_PARAGRAPH);
    text += char(MSG_CODE_PARAGRAPH);
    if (fLearnItem < 0) {
        for (int k = 0; k < 4; k++) {
            fSaintChoices.push_back(std::make_pair(0, k));
            AddChoiceLine(text, library
                ? saintNames[size_t(fLibrarySaints[k])] + "."
                : formulaNames[size_t(fFormulaIds[k])] + " for "
                    + MoneyText(_FormulaPrice(k)) + ".");
        }
    } else {
        for (int m = 0; fParty != NULL
                && m < int(fParty->members.size()); m++) {
            const character& member = fParty->members[size_t(m)];
            if (library && KnowsSaint(member,
                    fLibrarySaints[fLearnItem]))
                continue;
            fSaintChoices.push_back(std::make_pair(m, fLearnItem));
            AddChoiceLine(text, member.fullName + ".");
        }
    }
    text += char(MSG_CODE_OPTION);
    text += "...";
    text += char(MSG_CODE_OPTION_TEXT);
    text += library ? "study none of them.\n" : "buy none of them.\n";
    msg_card card = fNotImplementedCard;
    card.text = text;
    fView.SetCard(card, fVariables);
}


// The saint studied (file 0xBC572): the gift is paid, the day goes on until
// six in the evening, the member knows the saint; back where he came from
int
CityVisit::_LibraryStudy(int member, int index)
{
    if (fParty == NULL || member >= int(fParty->members.size()))
        return fLibraryReturn;
    const uint32 purse = TotalPfennigs(fParty->cash);
    fParty->cash = MoneyFromPfennigs(purse - std::min(purse, fMeetMoney));
    if (fClock != NULL)
        fClock->AddHours(uint32(_HoursUntil(*fClock, 18)));
    const int saint = fLibrarySaints[index];
    fParty->members[size_t(member)].saints[saint >> 3]
        |= uint8(0x80 >> (saint & 7));
    return fLibraryReturn;
}


// Leaving without studying: an hour
int
CityVisit::_LibraryLeave()
{
    if (fClock != NULL)
        fClock->AddHours(1);
    return fLibraryReturn;
}


// The formulae for sale (state 0x53, file 0xD3298): four, the number
// (property 0x21 + 0x64 + 3 · slot) % 22 of the version names; $Money is
// 10 · (that % 3 + 1) / the city's size florins
void
CityVisit::_EnterFormulas()
{
    const std::vector<std::string>& names = fData.Lists().Formulae();
    for (int k = 0; k < 4; k++) {
        fFormulaIds[k] = (_PeopleSeed() + 0x64 + 3 * k) % 22;
        const std::string number = std::to_string(k + 1);
        fVariables["Text" + number] = names[size_t(fFormulaIds[k])];
        fVariables["Money" + number] = MoneyText(_FormulaPrice(k));
    }
    fMeetBack = SCREEN_FORMULAS;
}


uint32
CityVisit::_FormulaPrice(int slot) const
{
    const int size = std::max(1, int(_City().size));
    return uint32(10 * (fFormulaIds[slot] % 3 + 1) / size) * 240;
}


// Buying (file 0xD373C): without the money card 1, a version he knows
// card 2; else it is paid and he knows it, and the party is where it
// came from
int
CityVisit::_FormulaBuy(int member, int slot)
{
    if (fParty == NULL || member >= int(fParty->members.size()))
        return SCREEN_FORMULAS;
    const int formula = fFormulaIds[slot];
    const uint32 price = _FormulaPrice(slot);
    const uint32 purse = TotalPfennigs(fParty->cash);
    fMeetBack = SCREEN_FORMULAS;
    if (purse < price)
        return SCREEN_FORMULAS_POOR;
    character& buyer = fParty->members[size_t(member)];
    const int bit = 1 << (formula % 3);
    if ((FormulaVersions(buyer, formula / 3) & bit) != 0)
        return SCREEN_FORMULAS_KNOWN;
    LearnFormula(buyer, formula / 3, bit);
    fParty->cash = MoneyFromPfennigs(purse - price);
    return fFormulaCaller;
}


// "Talk about other things" and leaving: an hour; the first to where the
// sale came from (DS:A88D), the second to DS:E7D8 (the crafts for the
// alchemist)
int
CityVisit::_FormulasBack()
{
    if (fClock != NULL)
        fClock->AddHours(1);
    return fFormulaCaller;
}


int
CityVisit::_FormulasLeave()
{
    if (fClock != NULL)
        fClock->AddHours(1);
    return fFormulaLeave;
}


// The alchemist is offended (card 6): no dealings for 60 hours
int
CityVisit::_AlchemistOffended()
{
    if (fClock != NULL)
        fAlchemistAngryUntil[fCity] = fClock->HourStamp() + 60;
    return SCREEN_ALCHEMIST_ANGRY;
}


// Purchasing formulae (file 0xDA166): offended if the chance fails; else
// the formulae for sale, and the crafts when they are done
int
CityVisit::_AlchemistFormulas()
{
    if (int(fRandom() % 100) > _AlchemistChance())
        return _AlchemistOffended();
    fFormulaCaller = SCREEN_ALCHEMIST_AGAIN;
    fFormulaLeave = SCREEN_CRAFTS;
    return SCREEN_FORMULAS;
}


// Trading formulae (file 0xDA1BC): not again for 60 hours; offended if the
// chance fails. He offers one where the city's property 0x21 is a
// multiple of 5, or (property % 50 + 30) is under the best Alchemy (card
// 11, else card 12): a version of a formula the leader does not know at
// all, from a random start (random(330) % 66), which he learns without
// giving one up
int
CityVisit::_AlchemistTrade()
{
    _Mark(kMarkFormulaTrade, 60);
    fMeetBack = SCREEN_ALCHEMIST_AGAIN;
    if (int(fRandom() % 100) > _AlchemistChance())
        return _AlchemistOffended();
    const int seed = _PeopleSeed();
    if (seed % 5 != 0 && seed % 50 + 30 >= _BestSkill(kSkillAlchemy))
        return SCREEN_ALCHEMIST_NO_TRADE;
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_ALCHEMIST_NO_TRADE;
    character& leader = fParty->members[fParty->leader];
    const int start = int(fRandom() % 330) % kFormulaVersionCount;
    int version = start;
    for (;;) {
        if (FormulaVersions(leader, version / 3) == 0)
            break;
        version = (version + 1) % kFormulaVersionCount;
        if (version == start)
            break;
    }
    LearnFormula(leader, version / 3, 1 << (version % 3));
    fVariables["Text2"] = fData.Lists().Formulae()[size_t(version)];
    fVariables["ChosenThreeName"] = leader.shortName;
    return SCREEN_ALCHEMIST_TRADE;
}


// Instruction (file 0xDA32C): offended if the chance fails; else a teacher
// of Alchemy for a week, of his skill, for skill / 4 + 12 pfennigs a day
// ($Money2), card 8
int
CityVisit::_AlchemistTeach()
{
    if (int(fRandom() % 100) > _AlchemistChance())
        return _AlchemistOffended();
    const int skill = _AlchemistSkill(true);
    const uint32 fee = uint32(std::abs(skill) / 4 + 12);
    const uint32 now = fClock != NULL ? fClock->HourStamp() : 0;
    _AddTutor(city_tutor{ kSkillAlchemy, skill, fee, now + 168 });
    fVariables["Money2"] = MoneyText(fee);
    fAlchemistTaught = true;
    fMeetBack = SCREEN_ALCHEMIST_AGAIN;
    return SCREEN_ALCHEMIST_TEACH;
}

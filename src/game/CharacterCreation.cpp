/*
 * CharacterCreation.cpp
 */

#include "CharacterCreation.h"

#include "ListFile.h"

#include <algorithm>
#include <cstring>
#include <initializer_list>

// The items an occupation gives (1462:4664 overlay, the switch at
// S2:3748 and its handlers): armor for the vitals and the limbs, by item
// code of DARKLAND.LST
enum {
    ITEM_V_LEATHER		= 0x26,
    ITEM_V_STUDDED		= 0x27,
    ITEM_V_CUIRBOULLI	= 0x28,
    ITEM_V_CHAINMAIL	= 0x2A,
    ITEM_V_BRIGANDINE	= 0x2B,
    ITEM_L_LEATHER		= 0x2F,
    ITEM_L_STUDDED		= 0x30,
    ITEM_L_CUIRBOUILLI	= 0x31,
    ITEM_L_CHAINMAIL	= 0x33,
    ITEM_FIRST_POTION	= 0x5F			// one per alchemical formula
};
static const int kSaintCount	= 136;
static const int kFormulaCount	= 22;

// What an occupation gives besides skills (S2:3748)
enum reward {
    REWARD_NONE,
    REWARD_ARMOR_A,			// V:Cuirboulli, L:Leather
    REWARD_ARMOR_B,			// V:Chainmail, L:Studded
    REWARD_ARMOR_C,			// V:Brigandine, L:Cuirbouilli
    REWARD_ARMOR_D,			// V:Brigandine, L:Studded
    REWARD_ARMOR_E,			// V:Studded, L:Leather
    REWARD_ARMOR_F,			// V:Brigandine, L:Chainmail
    REWARD_ARMOR_G,			// V:Leather
    REWARD_ARMOR_H,			// V:Chainmail, L:Leather
    REWARD_ARMOR_I,			// V:Cuirboulli, L:Studded
    REWARD_ARMOR_J,			// V:Leather, L:Leather
    REWARD_SAINT,			// a saint, at random
    REWARD_SAINTS,			// two
    REWARD_FORMULA,			// an alchemical formula, at random
    REWARD_FORMULAE			// two
};
static const uint8 kRewards[kOccupationCount] = {
    REWARD_ARMOR_A, REWARD_ARMOR_B, REWARD_ARMOR_C, REWARD_ARMOR_D,
    REWARD_ARMOR_E, REWARD_ARMOR_F, REWARD_ARMOR_G, REWARD_ARMOR_H,
    REWARD_SAINT, REWARD_SAINTS, REWARD_SAINT, REWARD_SAINT,
    REWARD_FORMULA, REWARD_NONE, REWARD_SAINT, REWARD_SAINTS,
    REWARD_ARMOR_G, REWARD_ARMOR_G, REWARD_ARMOR_H, REWARD_NONE,
    REWARD_ARMOR_I, REWARD_NONE, REWARD_NONE, REWARD_NONE,
    REWARD_FORMULA, REWARD_FORMULA, REWARD_FORMULAE, REWARD_ARMOR_G,
    REWARD_ARMOR_G, REWARD_ARMOR_G, REWARD_NONE, REWARD_NONE,
    REWARD_NONE, REWARD_ARMOR_J, REWARD_NONE, REWARD_ARMOR_E,
    REWARD_ARMOR_E
};

// The weapon a character starts with, by his best weapon skill
// (1462:5286): a short sword, club, military flail, short spear,
// throwing knife, short bow, crossbow
static const int kWeaponItems[kWeaponSkillCount] = {
    0x05, 0x0F, 0x11, 0x15, 0x01, 0x1C, 0x1E
};


// The state the occupations' rules look at (1462:691E)
namespace {

struct life_state {
    int age;
    int family;
    int last;
    int previous;
    bool female;
    const int* attributes;
    const int* skills;
    const bool* held;

    bool Held(int occupation) const { return held[occupation]; }
    bool HeldAny(std::initializer_list<int> list) const
    {
        for (int occupation : list) {
            if (held[occupation])
                return true;
        }
        return false;
    }
    bool LastIn(std::initializer_list<int> list) const
    {
        return std::find(list.begin(), list.end(), last) != list.end();
    }
    bool PreviousIn(std::initializer_list<int> list) const
    {
        return std::find(list.begin(), list.end(), previous) != list.end();
    }
    bool Attributes(int minimum) const
    {
        // INT, PER and CHR
        return attributes[ATTRIBUTE_INTELLIGENCE] >= minimum
            && attributes[ATTRIBUTE_PERCEPTION] >= minimum
            && attributes[ATTRIBUTE_CHARISMA] >= minimum;
    }
    int Intelligence() const { return attributes[ATTRIBUTE_INTELLIGENCE]; }
    int Skill(int skill) const { return skills[skill]; }
    bool Young() const { return age < 20; }
    bool FamilyIs(int a) const { return family == a; }
};

}	// namespace

// The game's own tests, transcribed from the 37 blocks of 1462:691E in
// the order they appear (docs/exe.md): the occupations of the "young"
// first choice (age 15) depend on the family too. A few tests are as
// the code has them: always true or never true.
static bool
Offered(int occupation, const life_state& s)
{
    const int kSpeakCommon = kSkillSpeakCommon;
    switch (occupation) {
        case 0:		// Recruit (the code's second alternative never holds)
            return !s.Held(0) && s.age < 30;
        case 1:		// Soldier
            return s.HeldAny({ 0, 1, 2, 3, 5, 20, 36 });
        case 2:		// Veteran
            return s.HeldAny({ 1, 2, 3, 5 });
        case 3:		// Captain
            return s.Attributes(20) && s.HeldAny({ 2, 3, 5, 6, 7, 9, 20 });
        case 4:		// Noble Heir
            return s.HeldAny({ 4, 6 }) || s.family == 0;
        case 5: {	// Knight
            const bool others = s.HeldAny({ 5, 7 })
                || s.LastIn({ 3, 4, 6, 9, 15 });
            return s.Skill(kSkillVirtue) > 15
                && (others || (s.Young() && s.family == 0));
        }
        case 6: {	// Courtier
            const bool others = s.HeldAny({ 6, 7 })
                || s.LastIn({ 3, 5, 8, 9, 15, 20 });
            return others || (s.Young() && s.family <= 1);
        }
        case 7:		// Manorial Lord
            return s.last == 7 || (s.PreviousIn({ 4, 6, 9, 15 })
                && s.LastIn({ 4, 6, 9, 15 }));
        case 8:		// Priest
            return !s.female && s.Attributes(20)
                && (s.HeldAny({ 7, 8, 9, 15 })
                    || s.LastIn({ 4, 6, 14, 20, 22, 24 })
                    || (s.PreviousIn({ 10, 12, 13, 21 })
                        && s.LastIn({ 10, 12, 13, 21 })));
        case 9:		// Bishop
            return !s.female && s.Attributes(25)
                && (s.LastIn({ 9, 15 }) || (s.PreviousIn({ 6, 7, 8 })
                    && s.LastIn({ 6, 7, 8 })));
        case 10:	// Friar
            return !s.female && s.LastIn({ 8, 10, 11, 13, 14, 15 });
        case 11:	// Hermit
            return s.Skill(kSkillVirtue) >= 15;
        case 12: {	// Oblate
            const bool free = !s.HeldAny({ 8, 9, 10, 13, 14, 15, 22, 23, 24, 25 })
                && s.Intelligence() >= 12;
            return free || (s.Young() && s.family != 4 && s.family != 5);
        }
        case 13:	// Novice Monk/Nun
            return s.Young() || !s.HeldAny({ 4, 6, 7, 8, 9, 10, 13, 14, 15,
                22, 23, 24, 25 });
        case 14:	// Monk/Nun
            return s.Attributes(15) && s.Skill(kSkillReligion) >= 5
                && s.HeldAny({ 4, 6, 7, 8, 9, 10, 13, 14, 15, 21, 22, 23, 24,
                    25 });
        case 15:	// Abbot/Abbess
            return s.Attributes(20) && s.Skill(kSkillReligion) >= 15
                && s.Intelligence() >= 12
                && (s.LastIn({ 4, 6, 7, 8, 9, 15 })
                    || (s.PreviousIn({ 14, 24 }) && s.LastIn({ 14, 24 })));
        case 16: {	// Peddler
            const bool free = !s.HeldAny({ 7, 9, 15, 19, 20, 26, 29 })
                && !s.LastIn({ 3, 4, 5, 6, 8, 18, 24, 25, 28 });
            return free || (s.Young() && (s.family == 4 || s.family == 5));
        }
        case 17: {	// Local Trader
            const bool able = s.Skill(kSpeakCommon) >= 5
                && s.Intelligence() >= 12
                && s.HeldAny({ 3, 4, 6, 8, 9, 14, 15, 16, 17, 18, 22, 23, 24,
                    25, 28, 29, 32 });
            return able || (s.Young() && s.family != 4 && s.family != 5);
        }
        case 18: {	// Travelling Merchant
            const bool able = s.Skill(kSpeakCommon) >= 10
                && s.Intelligence() >= 15
                && s.HeldAny({ 4, 7, 17, 18, 19, 24, 26, 29 });
            return able || (s.Young() && s.family <= 1);
        }
        case 19:	// Merchant-Proprietor
            return s.Skill(kSpeakCommon) >= 10 && s.Intelligence() >= 20
                && s.LastIn({ 7, 9, 18, 19 });
        case 20:	// Schulz
            return s.Held(34) && s.HeldAny({ 2, 3, 4, 5, 7, 8, 9, 15, 19, 20,
                24, 26, 29 });
        case 21:	// Student
            return s.Young() || (s.Skill(kSkillReadWrite) >= 6
                && s.Intelligence() >= 12
                && s.LastIn({ 0, 1, 2, 4, 10, 11, 12, 13, 14, 16, 17, 18, 21,
                    27, 28, 32, 33 }));
        case 22: {	// Clerk
            const bool able = s.Skill(kSkillReadWrite) >= 15
                && s.Intelligence() >= 12
                && (s.HeldAny({ 3, 4, 5, 6, 8, 9, 15, 19, 20, 21, 22, 24, 25,
                        26 })
                    || (s.PreviousIn({ 12, 14, 18 }) && s.LastIn({ 12, 14, 18 })));
            return able || (s.Young() && s.family == 1);
        }
        case 23:	// Physician
            return s.Skill(kSkillHealing) >= 15
                && s.HeldAny({ 21, 22, 23, 24, 25, 26 });
        case 24:	// Professor
            return s.Skill(kSkillReadWrite) >= 20
                && s.HeldAny({ 9, 15, 22, 23, 24, 25, 26 });
        case 25:	// Alchemist
            return s.Intelligence() >= 30
                && (s.HeldAny({ 8, 9, 15, 21, 22, 24, 25, 26 })
                    || (s.PreviousIn({ 10, 12, 14, 23 })
                        && s.LastIn({ 10, 12, 14, 23 })));
        case 26:	// Master Alchemist
            return s.Intelligence() >= 35 && s.Held(25);
        case 27:	// Apprentice Craftsman
            if (s.Young())
                return s.family != 0;
            return !s.HeldAny({ 7, 9, 15, 19, 26, 27, 28, 29 })
                && !s.LastIn({ 3, 4, 5, 6, 8, 18, 20, 22, 23, 24, 25 });
        case 28: {	// Journeyman Craftsman
            const bool able = s.HeldAny({ 23, 25, 26, 27, 28, 29 });
            return able || (s.Young() && (s.family == 2 || s.family == 3));
        }
        case 29:	// Master Craftsman
            return s.Intelligence() >= 12 && s.HeldAny({ 19, 26, 28 });
        case 30:	// Laborer
            if (s.Young())
                return s.family != 0;
            return !s.HeldAny({ 7, 9, 15, 19, 26, 29 })
                && !s.LastIn({ 3, 4, 5, 6, 8, 18, 20, 24, 25 });
        case 31:	// Vagabond
            if (s.Young())
                return s.family != 0;
            return !s.HeldAny({ 7, 9, 15, 19, 26, 28, 29 })
                && !s.LastIn({ 3, 4, 5, 6, 8, 18, 20, 24, 25 });
        case 32: {	// Swindler
            const bool able = s.Intelligence() >= 25
                && s.Skill(kSkillStreetwise) >= 15
                && s.HeldAny({ 1, 2, 3, 4, 8, 10, 14, 16, 17, 18, 21, 22, 24,
                    25, 28, 30, 31, 33 });
            return able || (s.Young() && s.family <= 2);
        }
        case 33:	// Thief (always, while young: the code's tests cannot fail)
            return s.Young() || (s.Skill(kSkillStreetwise) >= 10
                && s.HeldAny({ 1, 2, 8, 10, 16, 17, 18, 21, 22, 24, 25, 28,
                    30, 31, 33, 35, 36 }));
        case 34:	// Peasant (likewise)
            return s.Young() || (!s.HeldAny({ 7, 9, 15, 19, 26, 29 })
                && !s.LastIn({ 3, 4, 5, 6, 8, 18, 24, 25, 28 }));
        case 35: {	// Hunter
            const bool able = s.Skill(18) >= 15
                && s.HeldAny({ 0, 1, 2, 3, 5, 10, 11, 16, 18, 20, 34, 35, 36 });
            return able || (s.Young() && (s.family == 0 || s.family == 3
                || s.family == 5));
        }
        case 36:	// Bandit
            return s.HeldAny({ 0, 1, 2, 3, 5, 33, 35, 36 }) || s.Young()
                || s.LastIn({ 10, 11, 16, 20, 30, 31, 32, 34 });
        default:
            return false;
    }
}


static int
Clamp(int low, int high, int value)
{
    return std::max(low, std::min(high, value));
}


CharacterCreation::CharacterCreation(const ExeData& exe, const ListFile& lists,
    const std::function<int(int)>& random)
    :
    fExe(exe),
    fLists(lists),
    fRandom(random)
{
    Restart();
}


// 1462:675A: a new man, with the attributes of his sex and nothing else
void
CharacterCreation::Restart()
{
    fStage = STAGE_NAME;
    fFemale = false;
    fNameTyped = false;
    fAge = 0;
    fFamily = fLast = fPrevious = -1;
    memset(fSkills, 0, sizeof(fSkills));
    memset(fHeld, 0, sizeof(fHeld));
    memset(fSaints, 0, sizeof(fSaints));
    memset(fFormulae, 0, sizeof(fFormulae));
    fItems.clear();
    fPoints = fTotalPoints = 0;
    _ClearSpent();
    fChoices.clear();
    for (int i = 0; i < kFamilyCount; i++)
        fChoices.push_back(i);
    // the starting attributes of a man (END 12, STR 15, CHR 11 ...)
    fAttributes[ATTRIBUTE_ENDURANCE] = 12;
    fAttributes[ATTRIBUTE_STRENGTH] = 15;
    fAttributes[ATTRIBUTE_AGILITY] = 12;
    fAttributes[ATTRIBUTE_PERCEPTION] = 12;
    fAttributes[ATTRIBUTE_INTELLIGENCE] = 12;
    fAttributes[ATTRIBUTE_CHARISMA] = 11;
    fAttributes[ATTRIBUTE_DIVINE_FAVOR] = 99;
    NewName();
}


void
CharacterCreation::NewName()
{
    const uint16 seed = uint16(fRandom(65536));
    fName = fFemale ? fExe.FemaleName(seed) : fExe.MaleName(seed);
    // the first word, at most nine letters
    fNickname = fName.substr(0, fName.find(' '));
    if (fNickname.size() > size_t(kMaxNicknameLength))
        fNickname.resize(size_t(kMaxNicknameLength));
    fNameTyped = false;
}


bool
CharacterCreation::SetName(const std::string& name)
{
    if (name.empty())
        return false;
    fName = name.substr(0, size_t(kMaxNameLength));
    fNameTyped = true;
    return true;
}


bool
CharacterCreation::SetNickname(const std::string& nickname)
{
    if (nickname.empty())
        return false;
    fNickname = nickname.substr(0, size_t(kMaxNicknameLength));
    return true;
}


void
CharacterCreation::ToggleSex()
{
    if (fStage != STAGE_NAME)
        return;
    fFemale = !fFemale;
    // 1462:50AA: a woman has more endurance and less strength
    fAttributes[ATTRIBUTE_ENDURANCE] = fFemale ? 14 : 12;
    fAttributes[ATTRIBUTE_STRENGTH] = fFemale ? 12 : 15;
    fAttributes[ATTRIBUTE_AGILITY] = 12;
    fAttributes[ATTRIBUTE_PERCEPTION] = 12;
    fAttributes[ATTRIBUTE_INTELLIGENCE] = 12;
    fAttributes[ATTRIBUTE_CHARISMA] = fFemale ? 12 : 11;
    if (!fNameTyped)
        NewName();
}


void
CharacterCreation::BeginChildhood()
{
    if (fStage == STAGE_NAME)
        fStage = STAGE_FAMILY;
}


void
CharacterCreation::BackToName()
{
    if (fStage == STAGE_FAMILY)
        fStage = STAGE_NAME;
}


bool
CharacterCreation::KnowsSaint(int saint) const
{
    return saint >= 0 && saint < kSaintCount
        && (fSaints[saint >> 3] & (0x80 >> (saint & 7))) != 0;
}


const std::string&
CharacterCreation::ChoiceName(size_t row) const
{
    static const std::string kNone;
    if (row >= fChoices.size())
        return kNone;
    const int choice = fChoices[row];
    return fStage == STAGE_FAMILY ? fExe.Families()[size_t(choice)].name
        : fExe.Occupations()[size_t(choice)].name;
}


void
CharacterCreation::_ClearSpent()
{
    memset(fAttributeSpent, 0, sizeof(fAttributeSpent));
    memset(fSkillSpent, 0, sizeof(fSkillSpent));
}


bool
CharacterCreation::IsOccupationOffered(int occupation) const
{
    if (occupation < 0 || occupation >= kOccupationCount)
        return false;
    life_state state;
    state.age = fAge;
    state.family = fFamily;
    state.last = fLast;
    state.previous = fPrevious;
    state.female = fFemale;
    state.attributes = fAttributes;
    state.skills = fSkills;
    state.held = fHeld;
    return Offered(occupation, state);
}


// 1462:691E
void
CharacterCreation::_BuildChoices()
{
    fChoices.clear();
    for (int i = 0; i < kOccupationCount; i++) {
        if (IsOccupationOffered(i))
            fChoices.push_back(i);
    }
}


// 1462:7BCC: what a stage does to a character
void
CharacterCreation::_Apply(const exe_life_stage& what, bool family, int index)
{
    for (int i = 0; i < 6; i++) {
        // past 40 the first two attributes (END, STR) are left alone
        if (family || fAge <= 40 || i >= 2)
            fAttributes[i] += what.attributes[i];
        fAttributes[i] = Clamp(1, 99, fAttributes[i]);
    }
    for (int i = 0; i < kSkillCount; i++) {
        // the first occupation, at 15, gives two more points
        const int more = !family && fAge < 19 ? 2 : 0;
        fSkills[i] = Clamp(0, 99, fSkills[i] + what.skills[i] + more);
    }
    if (!family) {
        fPrevious = fLast;
        fLast = index;
        fHeld[index] = true;
        _GiveOccupationRewards(index);
    }
    int points = what.points;
    if (!family && fAge == 15)
        points += 20;
    else if (!family && fAge == 20)
        points += 5;
    fTotalPoints = fPoints = points;
    fAge += family ? kChildhoodYears : kOccupationYears;
    if (fAge >= 30) {
        // every five years take their toll
        const int8* aging = fExe.Aging(Clamp(0, kAgingCount - 1,
            (fAge - 30) / 5));
        for (int i = 0; i < 6; i++)
            fAttributes[i] += aging[i];
    }
    for (int i = 0; i < 6; i++)
        fAttributes[i] = Clamp(1, 99, fAttributes[i]);
}


// 1462:7D8A: the equipment is the last occupation's alone; saints and
// formulae add up
void
CharacterCreation::_GiveOccupationRewards(int occupation)
{
    fItems.clear();
    switch (kRewards[occupation]) {
        case REWARD_ARMOR_A:
            _GiveItem(ITEM_V_CUIRBOULLI);
            _GiveItem(ITEM_L_LEATHER);
            break;
        case REWARD_ARMOR_B:
            _GiveItem(ITEM_V_CHAINMAIL);
            _GiveItem(ITEM_L_STUDDED);
            break;
        case REWARD_ARMOR_C:
            _GiveItem(ITEM_V_BRIGANDINE);
            _GiveItem(ITEM_L_CUIRBOUILLI);
            break;
        case REWARD_ARMOR_D:
            _GiveItem(ITEM_V_BRIGANDINE);
            _GiveItem(ITEM_L_STUDDED);
            break;
        case REWARD_ARMOR_E:
            _GiveItem(ITEM_V_STUDDED);
            _GiveItem(ITEM_L_LEATHER);
            break;
        case REWARD_ARMOR_F:
            _GiveItem(ITEM_V_BRIGANDINE);
            _GiveItem(ITEM_L_CHAINMAIL);
            break;
        case REWARD_ARMOR_G:
            _GiveItem(ITEM_V_LEATHER);
            break;
        case REWARD_ARMOR_H:
            _GiveItem(ITEM_V_CHAINMAIL);
            _GiveItem(ITEM_L_LEATHER);
            break;
        case REWARD_ARMOR_I:
            _GiveItem(ITEM_V_CUIRBOULLI);
            _GiveItem(ITEM_L_STUDDED);
            break;
        case REWARD_ARMOR_J:
            _GiveItem(ITEM_V_LEATHER);
            _GiveItem(ITEM_L_LEATHER);
            break;
        case REWARD_SAINT:
        case REWARD_SAINTS:
            for (int i = kRewards[occupation] == REWARD_SAINTS ? 2 : 1; i > 0; i--) {
                const int saint = fRandom(kSaintCount);
                fSaints[saint >> 3] |= uint8(0x80 >> (saint & 7));
            }
            break;
        case REWARD_FORMULA:
        case REWARD_FORMULAE:
            for (int i = kRewards[occupation] == REWARD_FORMULAE ? 2 : 1; i > 0; i--)
                fFormulae[fRandom(kFormulaCount)]++;
            break;
        default:
            break;
    }
}


// 1462:80FE: the item, or one more of it when he has it already
void
CharacterCreation::_GiveItem(int code)
{
    const item_definition& definition = fLists.Items()[size_t(code)];
    for (item& have : fItems) {
        if (have.code == code && have.type == definition.type
                && have.quality == definition.quality) {
            have.quantity++;
            return;
        }
    }
    if (fItems.size() < 64) {
        fItems.push_back(item{ uint16(code), uint8(definition.type),
            definition.quality, 1, definition.weight });
    }
}


bool
CharacterCreation::Choose(size_t row)
{
    if (row >= fChoices.size())
        return false;
    const int choice = fChoices[row];
    if (fStage == STAGE_FAMILY) {
        fFamily = choice;
        fStage = STAGE_ATTRIBUTES;
        _ClearSpent();
        _Apply(fExe.Families()[size_t(choice)], true, choice);
        return true;
    }
    if (fStage == STAGE_OCCUPATION) {
        fStage = STAGE_SKILLS;
        _ClearSpent();
        _Apply(fExe.Occupations()[size_t(choice)], false, choice);
        return true;
    }
    return false;
}


int
CharacterCreation::IncreaseCost(int index) const
{
    if (fStage == STAGE_ATTRIBUTES) {
        const int value = fAttributes[index];
        return value >= 39 ? 3 : value >= 29 ? 2 : 1;
    }
    const int value = fSkills[index];
    return value >= 79 ? 3 : value >= 49 ? 2 : 1;
}


int
CharacterCreation::DecreaseRefund(int index) const
{
    if (fStage == STAGE_ATTRIBUTES) {
        const int value = fAttributes[index];
        return value >= 40 ? 3 : value >= 30 ? 2 : 1;
    }
    const int value = fSkills[index];
    return value >= 80 ? 3 : value >= 50 ? 2 : 1;
}


// 1462:4DE2..4F0D
bool
CharacterCreation::Increase(int index)
{
    const int cost = IncreaseCost(index);
    if (fStage == STAGE_ATTRIBUTES) {
        if (index < 0 || index >= 6 || fPoints - cost < 0
                || fAttributeSpent[index] >= kMaxAttribute
                || fAttributes[index] >= kMaxAttribute)
            return false;
        fAttributeSpent[index]++;
        fAttributes[index]++;
        fPoints -= cost;
        return true;
    }
    if (fStage == STAGE_SKILLS) {
        if (index < 0 || index >= kSkillCount || fPoints - cost < 0
                || fSkillSpent[index]
                    >= fExe.Occupations()[size_t(fLast)].limits[index]
                || fSkills[index] >= kMaxSkill)
            return false;
        fSkillSpent[index]++;
        fSkills[index]++;
        fPoints -= cost;
        return true;
    }
    return false;
}


// 1462:4F10..4FF0
bool
CharacterCreation::Decrease(int index)
{
    if (fTotalPoints <= fPoints)
        return false;
    const int refund = DecreaseRefund(index);
    if (fStage == STAGE_ATTRIBUTES) {
        if (index < 0 || index >= 6 || fAttributeSpent[index] == 0)
            return false;
        fAttributeSpent[index]--;
        fAttributes[index]--;
        fPoints += refund;
        return true;
    }
    if (fStage == STAGE_SKILLS) {
        if (index < 0 || index >= kSkillCount || fSkillSpent[index] == 0)
            return false;
        fSkillSpent[index]--;
        fSkills[index]--;
        fPoints += refund;
        return true;
    }
    return false;
}


void
CharacterCreation::DoneWithAttributes()
{
    if (fStage != STAGE_ATTRIBUTES)
        return;
    fStage = STAGE_OCCUPATION;
    _BuildChoices();
}


bool
CharacterCreation::CanContinue() const
{
    return fStage == STAGE_SKILLS && fAge <= kLastAgeToContinue;
}


bool
CharacterCreation::NextOccupation()
{
    if (!CanContinue())
        return false;
    fStage = STAGE_OCCUPATION;
    _ClearSpent();
    _BuildChoices();
    return true;
}


bool
CharacterCreation::CanFinish() const
{
    return fStage == STAGE_SKILLS;
}


CharacterCreation::preview
CharacterCreation::PreviewChoice(int choice) const
{
    preview shown;
    memset(&shown, 0, sizeof(shown));
    const bool family = fStage == STAGE_FAMILY;
    const exe_life_stage& what = family ? fExe.Families()[size_t(choice)]
        : fExe.Occupations()[size_t(choice)];
    shown.points = what.points;
    if (!family)
        shown.points += fAge >= 25 ? 0 : fAge >= 20 ? 5 : 20;
    const int8* aging = fExe.Aging(Clamp(0, kAgingCount - 1,
        (fAge - 25) / 5));
    for (int i = 0; i < 6; i++) {
        int value = fAttributes[i];
        if (family)
            value += what.attributes[i];
        else if (fAge > 40 && i < 2)
            value += aging[i];
        else
            value += aging[i] + what.attributes[i];
        shown.attributes[i] = value;
    }
    for (int i = 0; i < kSkillCount; i++) {
        // the family's points, or the occupation's (two more before 25)
        if (family) {
            shown.skillGain[i] = what.skills[i];
        } else {
            shown.skillGain[i] = what.skills[i] + (fAge < 25 ? 2 : 0);
            shown.skillRoom[i] = what.limits[i] - fSkillSpent[i];
        }
    }
    return shown;
}


CharacterCreation::preview
CharacterCreation::PreviewCurrent() const
{
    preview shown;
    memset(&shown, 0, sizeof(shown));
    shown.points = fPoints;
    for (int i = 0; i < 6; i++)
        shown.attributes[i] = fAttributes[i];
    if (fStage == STAGE_ATTRIBUTES) {
        const exe_life_stage& family = fExe.Families()[size_t(fFamily)];
        for (int i = 0; i < kSkillCount; i++)
            shown.skillGain[i] = family.skills[i];
    } else if (fStage == STAGE_SKILLS) {
        const exe_life_stage& what = fExe.Occupations()[size_t(fLast)];
        for (int i = 0; i < kSkillCount; i++) {
            shown.skillGain[i] = what.skills[i] + (fAge < 25 ? 2 : 0);
            shown.skillRoom[i] = what.limits[i] - fSkillSpent[i];
        }
    }
    return shown;
}


// 1462:5248
character
CharacterCreation::Finish()
{
    // his best weapon skill, the first on a tie
    int best = 0;
    for (int i = 1; i < kWeaponSkillCount; i++) {
        if (fSkills[best] < fSkills[i])
            best = i;
    }
    _GiveItem(kWeaponItems[best]);
    for (int i = 0; i < kFormulaCount; i++) {
        if (fFormulae[i] != 0)
            _GiveItem(ITEM_FIRST_POTION + i);
    }

    character made;
    made.fullName = fName;
    made.shortName = fNickname;
    made.age = uint16(fAge);
    made.female = fFemale;
    made.heraldry = 'A';
    for (int i = 0; i < ATTRIBUTE_COUNT; i++)
        made.attributes[i] = made.maxAttributes[i] = uint8(fAttributes[i]);
    for (int i = 0; i < kSkillCount; i++)
        made.skills[i] = uint8(fSkills[i]);
    for (int i = 0; i < EQUIPMENT_COUNT; i++)
        made.equipment[i] = kNoEquipment;
    memcpy(made.saints, fSaints, sizeof(made.saints));
    made.items = fItems;
    std::vector<uint8> record(kCharacterRecordSize, 0);
    WriteCharacter(made, record.data());
    // the fields the game sets that nothing here knows the meaning of
    record[0x04] = 0x10;
    record[0x05] = 0x01;
    record[0x06] = 0x50;
    record[0x18] = kNoEquipment;
    record[0x49] = 10;
    record[0x4A] = kNoEquipment;
    for (int i = 0; i < kFormulaCount; i++)
        record[0x94 + size_t(i)] = fFormulae[i];
    return ReadCharacter(record.data());
}

// The church, the monastery and the saints

#include "CityVisitInternal.h"

#include "BattleMap.h"
#include "BattleView.h"
#include "Catalog.h"
#include "Character.h"
#include "CityFile.h"
#include "DescriptionFile.h"
#include "GameData.h"
#include "GameTime.h"
#include "InfoView.h"
#include "ExeData.h"
#include "ListFile.h"
#include "LocationFile.h"
#include "ScreenSupport.h"
#include "EnemyFile.h"
#include "Stream.h"
#include "TextSupport.h"

#include <stdexcept>


bool
CityVisit::_InMonastery(int screen) const
{
    return screen == SCREEN_MONASTERY || (screen >= SCREEN_MONASTERY_AGAIN
        && screen <= SCREEN_MONKS_NIGHT_NO_SANCTUARY);
}


// The best Virtue (0E76:14A4(9))
int
CityVisit::_BestVirtue() const
{
    return _BestSkill(kSkillVirtue);
}


// Arriving (file 0xB9825 by day, 0xBBB13 at night): with a reputation of
// -40 or less, or the best Virtue + the fame / 20 at most 15, card 1 and
// the churches; else card 5 if the party came lately (mark 0x32), or
// card 0 and the mark for 12 hours. At night the monks send away the
// same bad reputation, and also a best Virtue + fame / 20 of 20 or more
// (as the game has it: "come back in the morning", card 1).
int
CityVisit::_EnterMonastery()
{
    const int fame = fParty != NULL ? fParty->fame : 0;
    const int standing = _BestVirtue() + fame / 20;
    if (fNight) {
        fMonastery = _Reputation() <= -40 || standing >= 20
            ? SCREEN_MONASTERY_NIGHT_REFUSED : SCREEN_MONASTERY;
        return fMonastery;
    }
    if (_Reputation() <= -40 || standing <= 15)
        return SCREEN_MONASTERY_REFUSED;
    if (_Marked(kMarkMonastery))
        fMonastery = SCREEN_MONASTERY_AGAIN;
    else {
        _Mark(kMarkMonastery, 12);
        fMonastery = SCREEN_MONASTERY;
    }
    return fMonastery;
}


// $Money1: 5 groschen a member (DS:A67E · 60 pfennigs)
uint32
CityVisit::_MonksPrice() const
{
    return fParty != NULL ? uint32(fParty->members.size()) * 60 : 0;
}


// Prayers by day (file 0xB9B3A): card 2, random(2) + 1 hours, the price,
// each member's divine favor up by Religion / 9 + 1 (0E76:0A72), card 3,
// no more prayers for 168 hours (mark 0x30)
int
CityVisit::_MonksPrayers()
{
    if (fClock != NULL)
        fClock->AddHours(fRandom() % 2 + 1);
    if (fParty != NULL) {
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash)
            - std::min(TotalPfennigs(fParty->cash), _MonksPrice()));
        for (character& member : fParty->members) {
            AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR,
                member.skills[kSkillReligion] / 9 + 1);
        }
    }
    _Mark(kMarkMonksPrayed, 168);
    return SCREEN_MONKS_MASS;
}


// At night (file 0xBBED4): the reputation / 2 + the leader's Virtue / 4
// + his Charisma / 2, within 0..99
int
CityVisit::_NightPrayersChance() const
{
    if (fParty == NULL || fParty->members.empty())
        return 0;
    const character& leader = fParty->members[size_t(fParty->leader)];
    const int chance = _Reputation() / 2 + leader.skills[kSkillVirtue] / 4
        + leader.attributes[ATTRIBUTE_CHARISMA] / 2;
    return std::max(0, std::min(99, chance));
}


// Prayers at night (file 0xBBD8E): if random(100) is at most the chance,
// the price, card 2 (the leader), divine favor up by Religion / 10 + 1,
// mark 0x30 for 168 hours; else card 3, and the reputation down by 1 if
// the monks were bothered lately (mark 0x35), else the mark for 8 hours.
// Then the churches.
int
CityVisit::_NightPrayers()
{
    if (int(fRandom() % 100) > _NightPrayersChance()) {
        if (_Marked(kMarkMonksBothered)) {
            if (fReputations != NULL && fCity >= 0
                    && fCity < int(fReputations->size()))
                (*fReputations)[size_t(fCity)] = int16(std::max(-99,
                    (*fReputations)[size_t(fCity)] - 1));
        } else
            _Mark(kMarkMonksBothered, 8);
        return SCREEN_MONKS_NIGHT_UNMOVED;
    }
    if (fParty != NULL) {
        fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash)
            - std::min(TotalPfennigs(fParty->cash), _MonksPrice()));
        for (character& member : fParty->members) {
            AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR,
                member.skills[kSkillReligion] / 10 + 1);
        }
        _SetChosen(fParty->leader);
    }
    _Mark(kMarkMonksPrayed, 168);
    return SCREEN_MONKS_NIGHT_PRAYED;
}


// Someone's Strength at most `percent` % of its maximum (wounds)
bool
CityVisit::_Wounded(int percent) const
{
    for (size_t i = 0; fParty != NULL && i < fParty->members.size(); i++) {
        const character& member = fParty->members[i];
        if (member.maxAttributes[ATTRIBUTE_STRENGTH] * percent / 100
                >= member.attributes[ATTRIBUTE_STRENGTH])
            return true;
    }
    return false;
}


// The abbess (file 0xBA1C6 by day, 0xBC0A6 at night): the average Virtue
// + a bonus if someone is wounded (Strength at most 90 %, 80 % at night
// of its maximum), else 0
int
CityVisit::_AbbessChance(int bonus, int percent) const
{
    if (fParty == NULL || fParty->members.empty() || !_Wounded(percent))
        return 0;
    int virtue = 0;
    for (const character& member : fParty->members)
        virtue += member.skills[kSkillVirtue];
    return std::max(0, std::min(99,
        virtue / int(fParty->members.size()) + bonus));
}


// Healing by day (file 0xBA074): card 4, an hour; if random(100) is at
// most the chance, the abbess (state 0xB4, not implemented); else card 10
// (nobody hurt enough) or 11, then the churches
int
CityVisit::_AskAbbess()
{
    if (fClock != NULL)
        fClock->AddHours(1);
    if (int(fRandom() % 100) <= _AbbessChance(20, 90))
        fMonkAnswer = SCREEN_NOT_IMPLEMENTED;
    else
        fMonkAnswer = _Wounded(90) ? SCREEN_ABBESS_PHYSICIAN
            : SCREEN_ABBESS_REFUSED;
    return SCREEN_MONKS_INQUIRE;
}


// "Please help us, we perish!" (file 0xBBFDA): an hour; if random(100)
// is at most the chance, card 5 and the abbess; else mark 0x35 for 8
// hours, card 6. (The game shows its card 4 first, a monk slamming the
// door on "scoundrels": not reproduced.)
int
CityVisit::_NightHelp()
{
    if (fClock != NULL)
        fClock->AddHours(1);
    if (int(fRandom() % 100) <= _AbbessChance(10, 80))
        return SCREEN_MONKS_NIGHT_ABBESS;
    _Mark(kMarkMonksBothered, 8);
    return SCREEN_MONKS_NIGHT_NO_HELP;
}


// The library (file 0xBA00C): 0E76:179C(11, 12) walks the members for
// the highest maximum Intelligence + Charisma (the fields 0x0B, 0x0C of
// its table: not Latin and reading, as it seems meant), comparing with
// twice the Intelligence kept so far, and gives that; + the reputation
// / 2 when under 0, + 25 after prayers (mark 0x30), within 0..99, 100
// from 75 up. The member found is $ChosenOneName.
int
CityVisit::_LibraryChance()
{
    int best = 0;
    int chosen = fParty != NULL ? fParty->leader : 0;
    for (size_t i = 0; fParty != NULL && i < fParty->members.size(); i++) {
        const character& member = fParty->members[i];
        const int sum = member.maxAttributes[ATTRIBUTE_INTELLIGENCE]
            + member.maxAttributes[ATTRIBUTE_CHARISMA];
        if (sum > best) {
            best = (member.maxAttributes[ATTRIBUTE_INTELLIGENCE] * 2) & 0xFF;
            chosen = int(i);
        }
    }
    _SetChosen(chosen);
    int chance = best;
    if (_Reputation() < 0)
        chance += _Reputation() / 2;
    if (_Marked(kMarkMonksPrayed))
        chance += 25;
    chance = std::max(0, std::min(99, chance));
    return chance >= 75 ? 100 : chance;
}


// The library (file 0xB9EA6): if random(100) is at most the chance, card
// 4, an hour, no more asking for 1440 hours (mark 0x34), the library
// (state 0x39, not implemented); else an hour, mark 0x34 for 720 hours,
// and card 4 and 7 ("too busy": no prayers were paid for, mark 0x30) or
// card 16
int
CityVisit::_AskLibrary()
{
    const int chance = _LibraryChance();
    if (fClock != NULL)
        fClock->AddHours(1);
    if (int(fRandom() % 100) <= chance) {
        _Mark(kMarkLibrary, 1440);
        fLibraryReturn = SCREEN_CHURCHES;
        fMonkAnswer = SCREEN_LIBRARY;
        return SCREEN_MONKS_INQUIRE;
    }
    _Mark(kMarkLibrary, 720);
    if (_Marked(kMarkMonksPrayed))
        return SCREEN_LIBRARY_CLOSED;
    fMonkAnswer = SCREEN_MONKS_BUSY;
    return SCREEN_MONKS_INQUIRE;
}


bool
CityVisit::_HasTutors() const
{
    const std::map<int, std::vector<city_tutor> >::const_iterator tutors
        = fTutors.find(fCity);
    const uint32 now = fClock != NULL ? fClock->HourStamp() : 0;
    if (tutors == fTutors.end())
        return false;
    for (const city_tutor& tutor : tutors->second) {
        if (now < tutor.until)
            return true;
    }
    return false;
}


void
CityVisit::_AddTutor(const city_tutor& tutor)
{
    std::vector<city_tutor>& tutors = fTutors[fCity];
    const uint32 now = fClock != NULL ? fClock->HourStamp() : 0;
    for (city_tutor& other : tutors) {
        if (other.skill == tutor.skill) {
            if (other.until <= now)
                other = tutor;
            return;
        }
    }
    tutors.push_back(tutor);
}


// The monks' tutoring (file 0xB9E22): the average Virtue + the
// reputation + the fame / 50, or the best Virtue + the best Charisma if
// higher (then $ChosenOneName, the most charismatic), within 1..99
int
CityVisit::_TutoringChance()
{
    if (fParty == NULL || fParty->members.empty())
        return 1;
    int virtue = 0;
    int charming = 0;
    for (size_t i = 0; i < fParty->members.size(); i++) {
        virtue += fParty->members[i].skills[kSkillVirtue];
        if (fParty->members[i].attributes[ATTRIBUTE_CHARISMA]
                > fParty->members[size_t(charming)].attributes[ATTRIBUTE_CHARISMA])
            charming = int(i);
    }
    const int party = virtue / int(fParty->members.size()) + _Reputation()
        + fParty->fame / 50;
    const int best = _BestVirtue()
        + fParty->members[size_t(charming)].attributes[ATTRIBUTE_CHARISMA];
    if (best > party)
        _SetChosen(charming);
    return std::max(1, std::min(99, std::max(party, best)));
}


// Tutoring (file 0xB9C86): card 4, two hours; if random(100) is at most
// the chance, three teachers for 168 hours (0E76:2C4E: Religion for 50
// pfennigs a day, Speak Latin and Read & Write for 25, all at level 50)
// and card 8, whose $Money1 (city size + the purse / 150, within
// 12..180, / 12 groschen a day) is not what they charge; else no asking
// for 55 hours (mark 0x33), card 6
int
CityVisit::_AskTutoring()
{
    const int chance = _TutoringChance();
    if (fClock != NULL)
        fClock->AddHours(2);
    const uint32 now = fClock != NULL ? fClock->HourStamp() : 0;
    if (int(fRandom() % 100) <= chance) {
        const uint32 purse = fParty != NULL ? TotalPfennigs(fParty->cash) : 0;
        const int groschen = std::max(12, std::min(180,
            int(_City().size) + int(purse / 150))) / 12;
        fVariables["Money1"] = MoneyText(uint32(groschen) * 12);
        _AddTutor(city_tutor{ kSkillReligion, 50, 50, now + 168 });
        _AddTutor(city_tutor{ kSkillSpeakLatin, 50, 25, now + 168 });
        _AddTutor(city_tutor{ kSkillReadWrite, 50, 25, now + 168 });
        fMonkAnswer = SCREEN_MONKS_TUTORS;
    } else {
        _Mark(kMarkMonksNoTutors, 55);
        fMonkAnswer = SCREEN_MONKS_NO_TUTORS;
    }
    return SCREEN_MONKS_INQUIRE;
}


// Mass: said at some hours only, more of them in bigger cities; every
// member gains divine favor, Religion / 8 + Speak Latin / 35 + 1 by day
// (1838:0214), Religion / 60 + Speak Latin / 40 + 1 at night (file
// 0xB93AA), and it lasts until the start of the bell after the next
// one. Otherwise the priest, or the altar boy, tells when the next Mass
// is.
int
CityVisit::_Mass()
{
    const bool night = fNight;		// the church's night card
    if (fClock == NULL || fParty == NULL)
        return night ? SCREEN_NIGHT_NO_MASS : SCREEN_NO_MASS;
    // the smallest city size with a Mass, by bell (1 Matins .. 8
    // Compline); 99: none
    static const int kMassSize[9] = { 99, 99, 0, 5, 6, 7, 4, 6, 99 };
    static const int kNightMassSize[9] = { 99, 7, 0, 5, 99, 99, 4, 6, 99 };
    const int size = _City().size;
    const int bell = fClock->Hour() / 3 + 1;
    if (size < (night ? kNightMassSize : kMassSize)[bell]) {
        int next = 6;
        if (night)
            next = bell == 3 && size > 3 ? 18 : 6;
        else
            next = bell >= 3 && bell <= 5 && size > 3 ? 18 : 6;
        fVariables["NamedOneName"] = GameTime(1400, 0, 1, uint16(next)).BellName();
        return night ? SCREEN_NIGHT_NO_MASS : SCREEN_NO_MASS;
    }
    for (character& member : fParty->members) {
        const int religion = member.skills[kSkillReligion];
        const int latin = member.skills[kSkillSpeakLatin];
        AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR, night
            ? religion / 60 + latin / 40 + 1 : religion / 8 + latin / 35 + 1);
    }
    const int end = (bell + 1) * 3;
    fClock->AddHours(uint32((end - fClock->Hour() + 24) % 24));
    return night ? SCREEN_NIGHT_MASS : SCREEN_MASS;
}


// The altar boy at night (file 0xB9572) names the next Mass, by bell and
// city size. (At Terce, Sexts and Compline the game reads an unset
// variable; the church shows its night card only at other bells.)
int
CityVisit::_AltarBoy()
{
    int next = 6;
    if (fClock != NULL) {
        const int size = _City().size;
        switch (fClock->Hour() / 3 + 1) {
            case 1:
                next = size >= 7 ? 0 : 6;
                break;
            case 3:
                next = size >= 5 ? 9 : size == 4 ? 18 : 6;
                break;
            case 6:
                next = size >= 4 ? 18 : 6;
                break;
            case 7:
                next = size >= 6 ? 21 : 6;
                break;
            default:
                break;
        }
    }
    fVariables["NamedOneName"] = GameTime(1400, 0, 1, uint16(next)).BellName();
    return SCREEN_ALTAR_BOY;
}


// Confession: the leader gains random(5) + Religion / 10 + 2 divine
// favor; it takes 11 hours, less with a good local reputation; if
// random(100) <= Religion and random(100) <= 25, a chance of a point of
// Virtue (1462:0132, see TrainSkill())
int
CityVisit::_Confession()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_CONFESSION;
    const int reputation = _Reputation();
    const int hours = reputation >= 0 ? 11 - reputation / 10
        : 12 + reputation / 20;
    if (fClock != NULL)
        fClock->AddHours(uint32(std::max(hours, 0)));
    character& leader = fParty->members[fParty->leader];
    // 0x9C0:1F63(-1, Virtue, 1, 10, -1): a lesson in Virtue
    if (int(fRandom() % 100) <= leader.skills[kSkillReligion]
            && int(fRandom() % 100) <= 25) {
        TrainSkill(leader, kSkillVirtue, 10,
            [this](int n) { return int(fRandom() % uint32(n)); });
    }
    AddToAttribute(leader, ATTRIBUTE_DIVINE_FAVOR,
        int(fRandom() % 5) + leader.skills[kSkillReligion] / 10 + 2);
    return SCREEN_CONFESSION;
}


// Donation: a tenth of the purse, a pfennig of which buys a sixth of a
// divine favor point per member. The most religious member gets back all
// the favor it lacks, then the others in turn while points remain. Over
// 600 pfennigs, every member gains Religion / 30 Virtue. An hour passes.
int
CityVisit::_Donation()
{
    if (fParty == NULL || fParty->members.empty())
        return SCREEN_SMALL_DONATION;
    const uint32 purse = TotalPfennigs(fParty->cash);
    const int amount = int(purse / 10);
    fParty->cash = MoneyFromPfennigs(purse - uint32(amount));

    std::vector<character>& members = fParty->members;
    const int count = int(members.size());
    int points = amount / (count * 6);
    // the game means the most religious member here, but uses its Religion
    // as a member index (a bug of the original)
    int first = 0;
    for (int i = 1; i < count; i++) {
        if (members[i].skills[kSkillReligion] > members[first].skills[kSkillReligion])
            first = i;
    }
    for (int k = 0; k < count && (k == 0 || points > 0); k++) {
        character& member = members[(first + k) % count];
        const int lacking = std::max(0, std::min(99,
            member.maxAttributes[ATTRIBUTE_DIVINE_FAVOR]
                - member.attributes[ATTRIBUTE_DIVINE_FAVOR]));
        AddToAttribute(member, ATTRIBUTE_DIVINE_FAVOR, lacking);
        points -= lacking;
    }
    if (amount > 600) {
        for (character& member : members) {
            member.skills[kSkillVirtue] = uint8(std::min(99,
                member.skills[kSkillVirtue] + member.skills[kSkillReligion] / 30));
        }
    }
    if (fClock != NULL)
        fClock->AddHours(1);
    if (amount < 120)
        return SCREEN_SMALL_DONATION;
    return amount < 600 ? SCREEN_DONATION : SCREEN_LARGE_DONATION;
}


// A card's saints (DS:EE4B, four words, set by each state's handler)
std::vector<int>
CityVisit::_SaintsFor(int screen) const
{
    std::vector<int> saints;
    if (screen >= SCREEN_CELL && screen <= SCREEN_LIT_CELL) {
        // file 0x9896E: Bathildis, Dismas, Peter, and by the cell
        // Reinold, Lucy (the dark cell, its light) or Jude (0x98A50)
        static const int kCellSaints[4] = { 114, 87, 80, 87 };
        saints = { 14, 35, 108, kCellSaints[screen - SCREEN_CELL] };
    }
    // the guards (file 0x9163D): Christina, Genevieve, Godfrey, Reinold
    if (screen == SCREEN_CHALLENGE)
        saints = { 21, 54, 61, 114 };
    // the gate by day (file 0x9273D): Lutgardis; at night (0x9357D) and
    // the wall by day (0x99CAB) Lutgardis, Milburga; the wall at night
    // (0x9A902) Christina too
    if (screen == SCREEN_DAY_GATE || screen == SCREEN_DAY_GATE_GUARDED)
        saints = { 89 };
    if (screen == SCREEN_NIGHT_GATE || screen == SCREEN_NIGHT_GATE_ALERTED
            || screen == SCREEN_DAY_WALL)
        saints = { 89, 97 };
    if (screen == SCREEN_NIGHT_WALL)
        saints = { 21, 89, 97 };
    // the night watch (file 0xBF1D6): Raphael, Finbar, Lucy, Odilia
    if (screen >= SCREEN_NIGHT_WATCH && screen <= SCREEN_NIGHT_WATCH_CAUGHT)
        saints = { 111, 49, 87, 100 };
    // the magistrate (file 0xFB518): Devota, Lawrence; the execution
    // (0xFBF17): Alcuin, Gregory Thaumaturgus, John Nepomuk, Jude
    if (screen == SCREEN_MAGISTRATE || screen == SCREEN_MAGISTRATE_AGAIN)
        saints = { 34, 84 };
    if (screen == SCREEN_EXECUTION)
        saints = { 5, 63, 78, 80 };
    // leaving: the gate (file 0xBC94B) and the wall (0xBDACB):
    // Christina, Lutgardis, Milburga
    if (screen == SCREEN_GATE || screen == SCREEN_INNER_WALL)
        saints = { 21, 89, 97 };
    // the robber knight's tower (file 0xFF828): Edward the Confessor,
    // Eric, Hedwig, Reinold
    if (screen == SCREEN_TOWER || screen == SCREEN_FORT)
        saints = { 41, 46, 64, 114 };
    // the city lord's audience (file 0xA4B0E, 0xB1A98): Alcuin, Raymond
    // Penafort, Wolfgang, Wenceslaus
    if (screen == SCREEN_FORTRESS || screen == SCREEN_TOWN_HALL)
        saints = { 5, 112, 134, 129 };
    // the bandits (file 0x137C8D): the list's first five, and Saint
    // Hubert in the forests (tile types 12..17)
    if (screen >= SCREEN_BANDITS_WARNING && screen <= SCREEN_BANDITS_CHARGE) {
        saints = { 101, 102, 54, 61, 131 };
        if (fBanditsTerrain >= 12 && fBanditsTerrain <= 17)
            saints.push_back(69);
    }
    // the bishop's tithe (file 0x13D686): Godfrey, John Nepomuk, Odo, Olaf;
    // the nobleman's toll (0x1774D4): Alcuin instead of Godfrey
    if (screen == SCREEN_TITHE)
        saints = { fToll ? 5 : 61, 78, 101, 102 };
    // the friar: Godfrey, John Nepomuk, Dominic, Odo, Olaf (file 0xF4CEC);
    // against his curse the plague saints, Roch and Sebastian (0xF51D3)
    if (screen == SCREEN_FRIAR)
        saints = { 61, 78, 36, 101, 102 };
    if (screen == SCREEN_FRIAR_CURSING)
        saints = { 61, 78, 115, 117, 101, 102 };
    // a caravan (file 0x13A16A); refugees: the plague saints, Roch,
    // Sebastian and one more (0x13C2E4), and against their ambush the ones
    // that the warning (0x13C700) and the ambush (0x13C773) bring
    if (screen == SCREEN_CARAVAN)
        saints = { 17, 37, 67, 93 };
    if (screen == SCREEN_REFUGEES)
        saints = { 115, 117, 85 };
    if (screen == SCREEN_REFUGEES_WARNED)
        saints = { 93, 37, 17, 67 };
    if (screen == SCREEN_REFUGEES_AMBUSH)
        saints = { 54, 61, 131 };
    // the camp's soldiers (file 0x17B06D): Genevieve, Godfrey, Hubert; the
    // bandits (0x17B931): Genevieve, Godfrey, Dismas
    if (screen == SCREEN_CAMPJ || screen == SCREEN_CAMPJ_SURPRISED
            || screen == SCREEN_CAMPJ_HUNTSMAN)
        saints = { 54, 61, 69 };
    if (screen == SCREEN_CAMPB || screen == SCREEN_CAMPB_UNANSWERED)
        saints = { 54, 61, 35 };
    // a blizzard (file 0x1470EA): Christopher, Drogo, Godehard, Wilfrid; a
    // peat bog (0x14649C): Cecilia, Finnian, Florian, Godehard; a flood
    // (0x149F5A): Engelbert, Finnian, Florian, Pantaleon
    // wolves (file 0x1083A0) and wild boars (0x10B390): Aidan, Hubert,
    // Perpetua, Tarachus
    if (screen == SCREEN_WOLVES || screen == SCREEN_BOARS)
        saints = { 3, 69, 107, 121 };
    if (screen == SCREEN_BLIZZARD)
        saints = { 22, 38, 60, 130 };
    // the tatzelwurms (file 0x141E0E) and the spiders (0x142FD6): Genevieve,
    // Godfrey, Willehad and Aidan (the spiders: saint 69 instead of Aidan
    // on the tile types 4..14) when warned; Januarius, Pantaleon, Perpetua
    // and Tarachus once ambushed (the tatzelwurms: not at the start)
    const bool tatzel = screen >= SCREEN_TATZEL && screen <= SCREEN_TATZEL_HIDDEN;
    const bool spiders = screen >= SCREEN_SPIDERS && screen <= SCREEN_SPIDERS_HIDDEN;
    // the schrats (file 0x1440DA): Genevieve, Godfrey, Willehad, 69, always
    if (screen >= SCREEN_SCHRATS && screen <= SCREEN_SCHRATS_HIDDEN)
        saints = { 54, 61, 131, 69 };
    if (tatzel || spiders) {
        const bool ambushed = screen == SCREEN_TATZEL_AMBUSH
            || screen == SCREEN_TATZEL_HIDDEN || screen == SCREEN_SPIDERS_AMBUSH
            || screen == SCREEN_SPIDERS_HIDDEN;
        if (ambushed && !fMonsterSaintsFirst)
            saints = { 74, 103, 107, 121 };
        else if (spiders)
            saints = { 54, 61, 131, fMeetTerrain >= 4 && fMeetTerrain <= 14 ? 69 : 3 };
        else
            saints = { 54, 61, 131, 3 };
    }
    if (screen == SCREEN_BOG || screen == SCREEN_BOG_AGAIN
            || screen == SCREEN_BOG_ALONE)
        saints = { 18, 50, 51, 60 };
    if (screen == SCREEN_FLOOD || screen == SCREEN_FLOOD_AGAIN)
        saints = { 44, 50, 51, 103 };
    // the thieves (file 0xAC27F): Apollinarius, Genevieve, Godfrey
    if (screen == SCREEN_THIEVES)
        saints = { 12, 54, 61 };
    return saints;
}


// 150B:168C: a member standing who knows one of the card's saints
bool
CityVisit::_SaintKnown(int screen) const
{
    if (fParty == NULL)
        return false;
    for (const character& member : fParty->members) {
        for (int saint : _SaintsFor(screen)) {
            if (KnowsSaint(member, saint))
                return true;
        }
    }
    return false;
}


// The saint list (file 0x8E252, the card's text replaced): a line for
// each member and each of the card's saints he knows; one more to give
// up (inferred)
void
CityVisit::_ShowSaints()
{
    fSaintChoices.clear();
    const std::vector<int> saints = _SaintsFor(fScreen);
    const std::vector<std::string>& names = fData.Lists().Saints();
    std::string text = "Which saint do you call upon?\n";
    text += char(MSG_CODE_PARAGRAPH);
    text += char(MSG_CODE_PARAGRAPH);
    for (int m = 0; fParty != NULL && m < int(fParty->members.size()); m++) {
        const character& member = fParty->members[size_t(m)];
        for (size_t i = 0; i < saints.size(); i++) {
            if (!KnowsSaint(member, saints[i]))
                continue;
            fSaintChoices.push_back(std::make_pair(m, int(i)));
            text += char(MSG_CODE_OPTION);
            text += "...";
            text += char(MSG_CODE_OPTION_TEXT);
            text += Font::ToGameCharset(member.shortName + " calls upon "
                + names[size_t(saints[i])] + ".") + "\n";
        }
    }
    text += char(MSG_CODE_OPTION);
    text += "...";
    text += char(MSG_CODE_OPTION_TEXT);
    text += "call upon none of them.\n";
    msg_card card = fNotImplementedCard;
    card.text = text;
    fView.SetCard(card, fVariables);
    fChoosingSaint = true;
}


// An invocation (0E76:2180, 1462:0000 of overlay 0x22, file 0x6B7D0):
// the chance is the saint's base + (Virtue - its Virtue) / 2 (165C:0000,
// file 0x82940), 0 under its Virtue or while the divine favor is under
// its cost. If random(100) is at most the chance, the saint answers
// (its own effect, mode 8, is not reproduced) and the divine favor
// falls by the cost; else by the cost, and by half of it more if
// random(99) is under the chance - 66. The game first shows the saint
// (0x6B9FC), where one can give up: not reproduced. Returns 1 or 0.
int
CityVisit::_Invoke(int member, int saint)
{
    if (fExe == NULL)
        fExe.reset(new ExeData(fData.PathFor("DARKLAND.EXE")));
    character& c = fParty->members[size_t(member)];
    const exe_saint& rule = fExe->Saints()[size_t(saint)];
    const int favor = c.attributes[ATTRIBUTE_DIVINE_FAVOR];
    const int virtue = c.skills[kSkillVirtue];
    const int cost = favor < int(rule.cost) ? 0 : int(rule.cost);
    const int chance = virtue < int(rule.minVirtue) || cost < int(rule.cost)
        ? 0 : int(rule.base) + (virtue - int(rule.minVirtue)) / 2;
    _SetChosen(member);
    fVariables["NamedOneName"] = fData.Lists().Saints()[size_t(saint)];
    if (int(fRandom() % 100) <= chance) {
        AddToAttribute(c, ATTRIBUTE_DIVINE_FAVOR, -cost);
        return 1;
    }
    if (int(fRandom() % 99) < chance - 66)
        AddToAttribute(c, ATTRIBUTE_DIVINE_FAVOR, -(cost / 2));
    AddToAttribute(c, ATTRIBUTE_DIVINE_FAVOR, -cost);
    return 0;
}


// A saint answered: the card's own outcome, by the saint's place in its
// list
int
CityVisit::_SaintAnswered(int screen, int index)
{
    if (screen >= SCREEN_CELL && screen <= SCREEN_LIT_CELL) {
        // the dungeon (file 0x9932A): three hours; Bathildis (card 7)
        // pays with half the purse, Dismas and Peter (card 19) with the
        // reputation (-1..-8), both to the square; Reinold (card 20) to
        // the side streets, Lucy (card 3) lights the dark cell, Jude
        // (card 21) to the square, the reputation +2..+7
        if (fClock != NULL)
            fClock->AddHours(3);
        if (index == 0) {
            if (fParty != NULL)
                fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash) / 2);
            return SCREEN_BATHILDIS;
        }
        if (index <= 2) {
            _ChangeReputation(-8, -1);
            return SCREEN_WALL_CRACKED;
        }
        if (fCell == 0)
            return SCREEN_REINOLD_CLIMB;
        if (fCell == 1) {
            fCell = 3;
            return SCREEN_LIT_CELL;
        }
        if (fCell == 2) {
            _ChangeReputation(2, 7);
            return SCREEN_EARTHQUAKE;
        }
        return SCREEN_CELL;				// Lucy again: nothing
    }
    if (screen == SCREEN_FORTRESS || screen == SCREEN_TOWN_HALL)
        return _LordSaint(index);
    // with a good reputation (over -10) the answer raises it, else it
    // lowers it (0E76:19D0 with the signs reversed)
    const bool liked = _Reputation() > -10;
    switch (screen) {
        case SCREEN_CHALLENGE:
            // file 0x91CC2: an hour; Christina (card 16, +-3..9) takes the
            // party away from the city; Genevieve, Godfrey (card 17,
            // +2..6) stop the guards; Reinold (card 18, +-2..6) walks up
            // a wall to the side streets
            if (fClock != NULL)
                fClock->AddHours(1);
            if (index == 0) {
                _ChangeReputation(liked ? 3 : -9, liked ? 9 : -3);
                return SCREEN_CHRISTINA_LIFTS;
            }
            if (index <= 2) {
                _ChangeReputation(2, 6);
                return SCREEN_GUARDS_AT_PEACE;
            }
            _ChangeReputation(liked ? 2 : -6, liked ? 6 : -2);
            return SCREEN_REINOLD_WALKS;
        case SCREEN_DAY_GATE:
        case SCREEN_DAY_GATE_GUARDED:
            // file 0x92D62: card 10, +4..12 (-3..9), the main street
            _ChangeReputation(liked ? 4 : -9, liked ? 12 : -3);
            return SCREEN_GATE_LIFTED;
        case SCREEN_NIGHT_GATE:
        case SCREEN_NIGHT_GATE_ALERTED:
            // file 0x93B46: card 10, +-2..6, the main street
            _ChangeReputation(liked ? 2 : -6, liked ? 6 : -2);
            return SCREEN_NIGHT_GATE_LIFTED;
        case SCREEN_DAY_WALL:
            // file 0x9A3C0: card 9, +1..4, the side streets
            _ChangeReputation(1, 4);
            return SCREEN_DAY_WALL_LIFTED;
        case SCREEN_NIGHT_WALL:
            // file 0x9B0ED: card 8 (Christina) or 9, an hour, the side
            // streets
            if (fClock != NULL)
                fClock->AddHours(1);
            return index == 0 ? SCREEN_NIGHT_WALL_CHRISTINA
                : SCREEN_NIGHT_WALL_LIFTED;
        case SCREEN_NIGHT_WATCH:
        case SCREEN_NIGHT_WATCH_MARKET:
        case SCREEN_NIGHT_WATCH_AGAIN:
        case SCREEN_NIGHT_WATCH_CAUGHT:
            // file 0xBF844: an hour, +5..10, card 6, on as after the fine
            if (fClock != NULL)
                fClock->AddHours(1);
            _ChangeReputation(5, 10);
            return SCREEN_WATCH_SUNLIGHT;
        case SCREEN_MAGISTRATE:
        case SCREEN_MAGISTRATE_AGAIN:
            // file 0xFB9A1: card 2, the torture (every member loses
            // random(20) Endurance, the leader keeps 1), 12 hours, card 4
            // (free, the square)
            if (fParty != NULL) {
                for (character& member : fParty->members) {
                    AddToAttribute(member, ATTRIBUTE_ENDURANCE,
                        -int(fRandom() % 20));
                }
            }
            if (fClock != NULL)
                fClock->AddHours(12);
            return SCREEN_COURT_SAINT;
        case SCREEN_EXECUTION:
            // file 0xFC6DB: Gregory (card 3) brings a storm, three hours,
            // the side streets; the others (card 2) help the rescues
            if (index == 1) {
                if (fClock != NULL)
                    fClock->AddHours(3);
                return SCREEN_STORM;
            }
            fRescueSaint = _SaintsFor(screen)[size_t(index)];
            return SCREEN_EXECUTION_SAINT;
        case SCREEN_GATE:
            // file 0xBD184: card 7, an hour, +-2..8, out of the city
            if (fClock != NULL)
                fClock->AddHours(1);
            _ChangeReputation(liked ? 2 : -8, liked ? 8 : -2);
            return SCREEN_GATE_SAINT;
        case SCREEN_INNER_WALL:
            // file 0xBE84F: card 10, an hour, out of the city
            if (fClock != NULL)
                fClock->AddHours(1);
            return SCREEN_INNER_SAINT;
        case SCREEN_THIEVES:
            return SCREEN_THIEVES_SAINT;			// card 8 (file 0xACA1A)
        case SCREEN_TITHE:
            return SCREEN_TITHE_BLESSED;			// card 2 (file 0x13DAF6)
        case SCREEN_CAMPJ:
        case SCREEN_CAMPJ_SURPRISED:
        case SCREEN_CAMPJ_HUNTSMAN:
            _Mark(kMarkCampSafe, 168);
            return SCREEN_CAMPJ_BLESSED;			// card 6 (file 0x17B598)
        case SCREEN_CAMPB:
        case SCREEN_CAMPB_UNANSWERED:
            _Mark(kMarkCampSafe, 168);
            return SCREEN_CAMPB_PRAYED;				// card 2 (file 0x17BCBB)
        case SCREEN_TATZEL:
        case SCREEN_TATZEL_AMBUSH:
        case SCREEN_TATZEL_HIDDEN:
            return SCREEN_TATZEL_CALMED;			// card 4, on (file 0x141F9B)
        case SCREEN_SPIDERS:
        case SCREEN_SPIDERS_AMBUSH:
        case SCREEN_SPIDERS_HIDDEN:
            return SCREEN_SPIDERS_CALMED;			// card 4, on
        case SCREEN_SCHRATS:
        case SCREEN_SCHRATS_AMBUSH:
        case SCREEN_SCHRATS_HIDDEN:
        case SCREEN_SCHRATS_WINDED:
        case SCREEN_SCHRATS_DEMAND:
            return SCREEN_SCHRATS_CALMED;			// card 4, on
        case SCREEN_WOLVES:
            // file 0x108912: an hour, card 7
            if (fClock != NULL)
                fClock->AddHours(1);
            return SCREEN_WOLVES_CALMED;
        case SCREEN_BOARS:
            // file 0x10B810: an hour, card 4
            if (fClock != NULL)
                fClock->AddHours(1);
            return SCREEN_BOARS_CALMED;
        case SCREEN_BLIZZARD:
            return SCREEN_BLIZZARD_PRAYED;			// card 2 (file 0x147525)
        case SCREEN_BOG:
        case SCREEN_BOG_AGAIN:
        case SCREEN_BOG_ALONE:
            return _BogSaint(index);
        case SCREEN_FLOOD:
        case SCREEN_FLOOD_AGAIN:
            return SCREEN_FLOOD_PRAYED;				// card 6 (file 0x14A377)
        case SCREEN_CARAVAN:
            // the devil-man's band seen (card 6), else an honest caravan
            // (card 4)
            fMeetBack = SCREEN_CARAVAN;
            return fCaravanTrap ? SCREEN_CARAVAN_DEVIL : SCREEN_CARAVAN_CALM;
        case SCREEN_REFUGEES:
            return _RefugeesSaint(true);
        case SCREEN_REFUGEES_WARNED:
            return SCREEN_REFUGEES_REVEALED;
        case SCREEN_REFUGEES_AMBUSH:
            return SCREEN_REFUGEES_PEACE;
        case SCREEN_FRIAR:
        case SCREEN_FRIAR_CURSING:
            return _FriarSaint(_SaintsFor(screen)[size_t(index)]);
        case SCREEN_BANDITS_WARNING:
        case SCREEN_BANDITS_AMBUSH:
        case SCREEN_BANDITS_SCOUTED:
            // Saint Hubert, card 9; the others, a peace or a light at
            // random (cards 7, 8; *inferred*), an hour
            if (fClock != NULL)
                fClock->AddHours(1);
            if (_SaintsFor(screen)[size_t(index)] == 69)
                return SCREEN_BANDITS_HUBERT;
            return fRandom() % 2 == 0 ? SCREEN_BANDITS_PEACE
                : SCREEN_BANDITS_LIGHT;
        case SCREEN_TOWER:
        case SCREEN_FORT:
            // file 0x1005F3: an hour; the first three, card 15 and the
            // audience; St. Reinold, card 18, a lesson in Stealth for all
            // and inside
            _Mark(kMarkTowerAsked, 6480);
            if (fClock != NULL)
                fClock->AddHours(1);
            if (index == 3) {
                const std::function<int(int)> random
                    = [this](int n) { return int(fRandom() % uint32(n)); };
                if (fParty != NULL)
                    TrainParty(*fParty, kSkillStealth, 1, 10, random);
                return SCREEN_REINOLD_WINDOW;
            }
            return SCREEN_TOWER_SAINT;
        default:
            return screen;
    }
}


// No answer: the dungeon's card 23, six hours in all
int
CityVisit::_SaintIgnored(int screen)
{
    if (screen >= SCREEN_CELL && screen <= SCREEN_LIT_CELL) {
        if (fClock != NULL)
            fClock->AddHours(6);
        return SCREEN_NO_ANSWER;
    }
    if (screen == SCREEN_FORTRESS || screen == SCREEN_TOWN_HALL)
        return _LordCard(SCREEN_LORD_SAINT_VAIN, screen);	// card 11
    switch (screen) {
        case SCREEN_CHALLENGE:
            return SCREEN_CHALLENGE_UNANSWERED;		// card 15, the fight
        case SCREEN_DAY_GATE:
        case SCREEN_DAY_GATE_GUARDED:
            return SCREEN_GATE_UNANSWERED;			// card 12
        case SCREEN_NIGHT_GATE:
        case SCREEN_NIGHT_GATE_ALERTED:
            if (fClock != NULL)
                fClock->AddHours(1);
            return SCREEN_NIGHT_GATE_UNANSWERED;	// card 11, an hour
        case SCREEN_DAY_WALL:
            return SCREEN_DAY_WALL_UNANSWERED;		// card 10
        case SCREEN_NIGHT_WALL:
            if (fClock != NULL)
                fClock->AddHours(1);
            return SCREEN_NIGHT_WALL_UNANSWERED;	// card 10, an hour
        case SCREEN_NIGHT_WATCH:
        case SCREEN_NIGHT_WATCH_MARKET:
        case SCREEN_NIGHT_WATCH_AGAIN:
        case SCREEN_NIGHT_WATCH_CAUGHT:
            return SCREEN_WATCH_UNANSWERED;			// card 13
        case SCREEN_MAGISTRATE:
        case SCREEN_MAGISTRATE_AGAIN:
            return SCREEN_COURT_UNANSWERED;			// card 3
        case SCREEN_EXECUTION:
            return SCREEN_EXECUTION_UNANSWERED;		// card 4, the rescues
        case SCREEN_GATE:
            if (fClock != NULL)
                fClock->AddHours(1);
            return SCREEN_GATE_SAINT_UNANSWERED;	// card 8, an hour
        case SCREEN_INNER_WALL:
            if (fClock != NULL)
                fClock->AddHours(1);
            return SCREEN_INNER_SAINT_UNANSWERED;	// card 11, an hour
        case SCREEN_THIEVES:
            return SCREEN_THIEVES_UNANSWERED;		// card 9, the fight
        case SCREEN_TATZEL:
            fPrayerFailed = true;
            fMeetBack = SCREEN_TATZEL;
            return SCREEN_TATZEL_UNHEARD_BACK;		// card 5, then the options
        case SCREEN_TATZEL_AMBUSH:
        case SCREEN_TATZEL_HIDDEN:
            return SCREEN_TATZEL_UNHEARD;			// card 5, the fight
        case SCREEN_SPIDERS:
            fPrayerFailed = true;
            fMeetBack = SCREEN_SPIDERS;
            return SCREEN_SPIDERS_UNHEARD_BACK;
        case SCREEN_SPIDERS_AMBUSH:
        case SCREEN_SPIDERS_HIDDEN:
            return SCREEN_SPIDERS_UNHEARD;
        case SCREEN_SCHRATS:
            fPrayerFailed = true;
            fMeetBack = SCREEN_SCHRATS;
            return SCREEN_SCHRATS_UNHEARD_BACK;
        case SCREEN_SCHRATS_AMBUSH:
        case SCREEN_SCHRATS_HIDDEN:
        case SCREEN_SCHRATS_WINDED:
        case SCREEN_SCHRATS_DEMAND:
            return SCREEN_SCHRATS_UNHEARD;
        case SCREEN_WOLVES:
            fMeetBack = SCREEN_WOLVES;
            return SCREEN_WOLVES_UNHEARD;			// card 8
        case SCREEN_BOARS:
            fMeetBack = SCREEN_BOARS;
            return SCREEN_BOARS_UNHEARD;			// card 5
        case SCREEN_BLIZZARD:
            fMeetBack = SCREEN_BLIZZARD;
            return SCREEN_BLIZZARD_UNHEARD;			// card 3 (file 0x14755A)
        case SCREEN_BOG:
        case SCREEN_BOG_AGAIN:
        case SCREEN_BOG_ALONE:
            return _BogUnheard(screen);
        case SCREEN_FLOOD:
        case SCREEN_FLOOD_AGAIN:
            return SCREEN_FLOOD_AGAIN;				// card 5 again
        case SCREEN_CARAVAN:
            fPrayerFailed = true;
            fMeetBack = SCREEN_CARAVAN;
            return SCREEN_CARAVAN_UNHEARD;			// card 3
        case SCREEN_REFUGEES:
            return _RefugeesSaint(false);
        case SCREEN_REFUGEES_WARNED:
            fPrayerFailed = true;
            fMeetBack = SCREEN_REFUGEES_WARNED;
            return SCREEN_REFUGEES_NOANSWER;
        case SCREEN_REFUGEES_AMBUSH:
            return SCREEN_REFUGEES_UNANSWERED;
        case SCREEN_CAMPJ:
        case SCREEN_CAMPJ_SURPRISED:
        case SCREEN_CAMPJ_HUNTSMAN:
            fMeetBack = SCREEN_CAMPJ_HUNTSMAN;		// card 7, then card 2
            return SCREEN_CAMPJ_UNANSWERED;
        case SCREEN_CAMPB:
        case SCREEN_CAMPB_UNANSWERED:
            return SCREEN_CAMPB_UNANSWERED;			// card 6, the menu again
        case SCREEN_TITHE:
            fPrayerFailed = true;
            return SCREEN_TITHE_PRAYED;				// card 3, no more prayers
        case SCREEN_FRIAR:
            fPrayerFailed = true;
            return SCREEN_FRIAR_PRAYED;				// card 3
        case SCREEN_FRIAR_CURSING:
            _FriarCurse();							// card 12 and the curse
            return SCREEN_FRIAR_CURSE;
        case SCREEN_BANDITS_WARNING:
        case SCREEN_BANDITS_AMBUSH:
        case SCREEN_BANDITS_SCOUTED:
            fBanditsReturn = screen;
            return SCREEN_BANDITS_UNANSWERED;		// card 6, as before
        case SCREEN_TOWER:
        case SCREEN_FORT:
            _Mark(kMarkTowerAsked, 6480);
            if (fClock != NULL)
                fClock->AddHours(1);
            fAfterCard = _TowerScreen();
            return SCREEN_TOWER_UNANSWERED;			// card 19, an hour
        default:
            return screen;
    }
}

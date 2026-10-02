#include "CityVisitInternal.h"

#include "BattleMap.h"
#include "BattleView.h"
#include "Catalog.h"
#include "Character.h"
#include "Equipment.h"
#include "CityFile.h"
#include "DescriptionFile.h"
#include "GameData.h"
#include "GameTime.h"
#include "InfoView.h"
#include "MenuBar.h"
#include "ExeData.h"
#include "ListFile.h"
#include "LocationFile.h"
#include "ScreenSupport.h"
#include "EnemyFile.h"
#include "Stream.h"
#include "TextSupport.h"

#include <stdexcept>

// The game minutes of a rule: its own, or until night or morning.
static uint32
MinutesFor(const option_rule& rule, const GameTime& clock)
{
    if (rule.minutes >= 0)
        return uint32(rule.minutes);
    if (rule.minutes == kAnHourMoreAtNight)
        return IsGameDay(clock) ? 60 : 120;
    return 0;
}


static const screen_rules&
RulesFor(int screen, bool night)
{
    if (night) {
        const screen_rules* rules = NightRules(screen);
        if (rules != NULL)
            return *rules;
    }
    return DayRules(screen);
}


static const option_rule&
RuleFor(const screen_rules& rules, int option)
{
    static const option_rule kNotImplemented = { ACTION_NOT_IMPLEMENTED, 0,
        kAlways, 0 };
    if (option < 0 || option >= kMaxOptions
            || rules.options[option].action == ACTION_UNLISTED)
        return kNotImplemented;
    return rules.options[option];
}


// #pragma mark - CityVisit


CityVisit::CityVisit(GameData& data)
    :
    fData(data),
    fView(data),
    fTrade(data),
    fPendingTrade(-1),
    fParty(NULL),
    fClock(NULL),
    fInfo(NULL),
    fMenu(NULL),
    fSettings(NULL),
    fReputations(NULL),
    fNight(false),
    fCity(-1),
    fScreen(SCREEN_START),
    fPreviousScreen(SCREEN_START),
    fRandom(std::random_device()()),
    fSeed(0),
    fResidence(data),
    fPendingResidence(false),
    fPendingCache(false),
    fTreatmentOffered(false),
    fStoneOffered(false),
    fWatchReturn(SCREEN_NOT_IMPLEMENTED),
    fPendingBattle(false),
    fBattleKind(BATTLE_WITH_WATCH),
    fCell(0),
    fTunnel(0),
    fTortures(0),
    fMagistrateComing(false),
    fChoosingSaint(false),
    fRescueSaint(-1),
    fGateReturn(SCREEN_MAIN_STREET),
    fGateShoutFight(false),
    fAfterDark(-1),
    fNoticesFromSquare(false),
    fNewsReturn(SCREEN_INN),
    fEvents(NULL),
    fLocationFlags(NULL),
    fEnterStates(NULL),
    fRetired(NULL),
    fPartyReturn(SCREEN_INN),
    fPendingSelection(false),
    fLordHall(false),
    fQuestReturn(-1),
    fQuestPatron(-1),
    fQuestPlace(-1),
    fQuestRobber(false),
    fAfterCard(-1),
    fSlumCamp(false),
    fThievesReturn(SCREEN_SLUM),
    fOnMap(false),
    fToll(false),
    fMeetMoney(0),
    fMeetDays(0),
    fMeetReturn(0),
    fPleaFailed(false),
    fPrayerFailed(false),
    fCaravanTrap(false),
    fMeetBack(0),
    fCampGuard(-1),
    fCampIgnored(false),
    fCampSoldiers(false),
    fBanditsSoldiers(false),
    fBanditsTerrain(0),
    fBanditsReturn(SCREEN_BANDITS_WARNING),
    fWeatherMember(0),
    fBogFailed(),
    fFloodRope(false),
    fBattleLeaves(false),
    fShellReturn(SCREEN_SQUARE),
    fShellWon(false),
    fGroveHours(0),
    fMonastery(SCREEN_MONASTERY),
    fMonkAnswer(SCREEN_MONASTERY),
    fThanksReturn(SCREEN_FUGGER),
    fChallengeReturn(SCREEN_OUTSIDE),
    fChallengeReputation(0),
    fPartyLost(false),
    fWallFailed(false)
{
    // every screen has a row and a day card, and the decks load: missing
    // files show up right away
    CheckScreenTables();
    for (int i = 0; i < SCREEN_COUNT; i++) {
        const screen_rules* rules[2] = { &DayRules(i), NightRules(i) };
        for (const screen_rules* r : rules) {
            if (r != NULL && r->deck != NULL)
                fData.Messages(r->deck).CardAt(uint32(r->card));
        }
    }

    // in the game's character set, with the cards' control codes
    fNotImplementedCard.textTop = 10;
    fNotImplementedCard.textLeft = 10;
    fNotImplementedCard.unknown1 = 0;
    fNotImplementedCard.textRight = 240;
    fNotImplementedCard.unknown2 = 0;
    fNotImplementedCard.text = std::string("This is not implemented yet.\n")
        + char(MSG_CODE_PARAGRAPH) + char(MSG_CODE_PARAGRAPH)
        + char(MSG_CODE_OPTION) + "..." + char(MSG_CODE_OPTION_TEXT)
        + "go back.\n";
}


void
CityVisit::SetParty(party* members)
{
    fParty = members;
    fView.SetParty(members);
    fTrade.SetParty(members);
    fResidence.SetParty(members);
}


void
CityVisit::SetInfoView(InfoView* info)
{
    fInfo = info;
    fResidence.SetInfoView(info);
    fTrade.SetInfoView(info);
    fView.SetInfoView(info);
}


void
CityVisit::SetMenuBar(MenuBar* menu)
{
    fMenu = menu;
    fView.SetMenuBar(menu);
    fTrade.SetMenuBar(menu);
    fResidence.SetMenuBar(menu);
}


CityVisit::result
CityVisit::Run(GameWindow& window, int cityIndex, int screen)
{
    Enter(cityIndex, screen);
    if (fPendingBattle) {
        // a meeting that starts with the fight
        _RunBattle(window);
        if (fPartyLost) {
            fPartyLost = false;
            return PARTY_LOST;
        }
        if (fBattleLeaves) {
            fBattleLeaves = false;
            return LEAVE_CITY;
        }
    }
    for (;;) {
        const int option = fView.Run(window);
        if (option == CardView::kSaveRequested) {
            if (fSaveHandler)
                fSaveHandler(window);
            continue;
        }
        if (option == CardView::kOrderRequested) {
            if (fOrderHandler)
                fOrderHandler(window);
            _Show(fScreen, false);
            continue;
        }
        if (option == CardView::kLoadRequested) {
            if (fLoadHandler && fLoadHandler(window))
                return LOAD_GAME;
            continue;
        }
        if (option < 0)
            return QUIT;
        if (!Choose(option)) {
            // the last member drowned in the bog: the game is over
            return fParty != NULL && fParty->members.empty() ? PARTY_LOST
                : LEAVE_CITY;
        }
        if (fParty != NULL && fParty->members.empty())
            return PARTY_LOST;			// all executed
        if (fPendingSelection) {
            _SelectParty(window);
            fPendingSelection = false;
            _Show(SCREEN_PARTY, false);
        }
        if (fPendingTrade >= 0) {
            const int reputation = fReputations != NULL
                && fCity < int(fReputations->size()) ? (*fReputations)[fCity] : 0;
            fTrade.SetPlace(fCity, reputation);
            // the same stock all day long
            const uint32 day = fClock != NULL ? uint32(fClock->Year()) * 400
                + fClock->Month() * 32 + fClock->Day() : 0;
            fTrade.SetMerchant(merchant_kind(fPendingTrade),
                uint32(fCity) * 7919 + uint32(fPendingTrade) * 104729 + day);
            fTrade.Run(window);
            fPendingTrade = -1;
    fPendingBattle = false;
            _Show(fScreen, false);
        }
        if (fPendingCache) {
            fTrade.SetPlace(fCity, _Reputation());
            fTrade.SetCache(&fCaches[fCity]);
            fTrade.Run(window);
            fPendingCache = false;
            _Show(fScreen, false);
        }
        if (fPendingBattle) {
            _RunBattle(window);
            if (fPartyLost) {
                fPartyLost = false;
                return PARTY_LOST;
            }
            if (fBattleLeaves) {
                fBattleLeaves = false;
                return LEAVE_CITY;
            }
        }
        if (fPendingResidence && fSlumCamp) {
            // file 0xAB4EE: the camp of the slum (0E76:222A(3)), a
            // pfennig a day (file 0x70926), the thieves before each day
            fResidence.SetClock(fClock);
            fResidence.SetPlace(fCity, _Reputation(), 1, fTutors[fCity]);
            fResidence.SetAmbush(std::max(25, std::min(95,
                _BestSkill(kSkillStreetwise) + 25)));
            fResidence.Run(window);
            fPendingResidence = false;
            _Show(_AfterSlumCamp(), false);
        } else if (fPendingResidence) {
            // DARKLAND.EXE, file 0xA709A: the residence, then the inn;
            // a day costs the inn's price
            fResidence.SetClock(fClock);
            fResidence.SetPlace(fCity, _Reputation(), InnPrice(),
                fTutors[fCity]);
            fResidence.Run(window);
            fPendingResidence = false;
            _Show(SCREEN_INN, false);
        }
    }
}


void
CityVisit::Enter(int cityIndex, int screen)
{
    if (screen < 0 || screen >= SCREEN_COUNT)
        throw std::out_of_range("CityVisit::Enter(): invalid screen");
    fCity = cityIndex;
    if (fInfo != NULL) {
        if (_InCity()) {
            const city& c = fData.Cities().CityAt(uint32(cityIndex));
            fInfo->SetPosition(map_position{ c.x, c.y });
        } else {
            const location& l = fData.Locations().LocationAt(uint32(cityIndex));
            fInfo->SetPosition(map_position{ l.x, l.y });
        }
    }
    fVariables.clear();
    if (_InCity())
        AddCityVariables(fData, cityIndex, fVariables);
    else {
        fVariables["PlaceName"]
            = fData.Locations().LocationAt(uint32(cityIndex)).name;
    }
    if (fOnMap && uint32(cityIndex) < fData.Locations().CountLocations()) {
        fVariables["NearestCity"]
            = fData.Locations().LocationAt(uint32(cityIndex)).name;
    }
    if (fParty != NULL)
        AddPartyVariables(*fParty, fVariables);
    fPreviousScreen = screen;
    if (!_InCity() && screen == SCREEN_OUTSIDE)
        screen = _EnterPlace(cityIndex);
    _Show(screen);
}


bool
CityVisit::Choose(int option)
{
    fPendingTrade = -1;
    fPendingResidence = false;
    fPendingCache = false;
    // $ChosenOneName and its pronouns are the leader's unless an option
    // names another member
    if (fParty != NULL)
        AddPartyVariables(*fParty, fVariables);
    if (fScreen == SCREEN_NOT_IMPLEMENTED) {
        if (fPreviousScreen < 0)
            return false;				// a place not implemented: away
        _Show(fPreviousScreen, false);
        return true;
    }
    if (fChoosingSaint) {
        // the saint list: a member and a saint, or none (the last line)
        fChoosingSaint = false;
        if (option < 0 || option >= int(fSaintChoices.size())) {
            _Show(fScreen, false);
            return true;
        }
        const std::pair<int, int> choice = fSaintChoices[size_t(option)];
        const int saint = _SaintsFor(fScreen)[size_t(choice.second)];
        const int screen = fScreen;
        fLordHall = screen == SCREEN_TOWN_HALL;
        fVariables["ChosenTwoName"] = fParty->members[size_t(choice.first)].shortName;
        _Show(_Invoke(choice.first, saint) > 0
            ? _SaintAnswered(screen, choice.second) : _SaintIgnored(screen));
        return true;
    }
    const option_rule& rule = RuleFor(RulesFor(fScreen, fNight), option);
    switch (rule.action) {
        case ACTION_GO:
            if (fClock != NULL)
                fClock->AddMinutes(MinutesFor(rule, *fClock));
            _Show(rule.target);
            return true;
        case ACTION_LEAVE:
            return false;
        case ACTION_TRADE:
            if (fClock != NULL)
                fClock->AddMinutes(MinutesFor(rule, *fClock));
            fPendingTrade = rule.target;
            if (rule.then != 0)
                fScreen = rule.then;	// shown after the trade
            return true;
        case ACTION_SLEEP:
            _Show(_Sleep());
            return true;
        case ACTION_STABLES:
            _Show(_Stables());
            return true;
        case ACTION_RESIDENCE:
            fPendingResidence = true;
            return true;
        case ACTION_CACHE:
            if (fClock != NULL)
                fClock->AddMinutes(MinutesFor(rule, *fClock));
            fCaches[fCity];		// the location has a cache now
            fPendingCache = true;
            fScreen = rule.then;
            return true;
        case ACTION_REDEEM:
            _Show(_Redeem(rule.target));
            return true;
        case ACTION_DEPOSIT:
            _Deposit();
            _Show(rule.target);
            return true;
        case ACTION_DISCUSS_TREATMENTS:
            _Show(_DiscussTreatments());
            return true;
        case ACTION_ASK_AID:
            _Show(_AskAid());
            return true;
        case ACTION_COMPONENTS:
            _Show(_Components());
            return true;
        case ACTION_TREATMENT:
            _Show(_Treatment());
            return true;
        case ACTION_STUDENTS:
            _Show(_Students());
            return true;
        case ACTION_STONE:
            _Show(_Stone());
            return true;
        case ACTION_ALCHEMIST_SHOP:
            _Show(_AlchemistShop());
            return true;
        case ACTION_NIGHT_WALK: {
            // DARKLAND.EXE, e.g. file 0xA47A2 and 0xA4596: at night the
            // tinkers' and clothmakers' streets cost an hour more first;
            // then the watch stops the party if random(100) is over the
            // chance, and paying its fine leads back here
            if (fClock != NULL && !IsGameDay(*fClock)) {
                if (rule.minutes == kAnHourMoreAtNight)
                    fClock->AddHours(1);
                if (int(fRandom() % 100) > _NightWalkChance()) {
                    fWatchReturn = fScreen;
                    _Show(SCREEN_NIGHT_WATCH);
                    return true;
                }
            }
            if (fClock != NULL)
                fClock->AddHours(1);
            _Show(rule.target);
            return true;
        }
        case ACTION_TO_GROVE:
            _Show(_ToGrove());
            return true;
        case ACTION_SQUARE_SNEAK:
            _Show(_SquareSneak(rule.target));
            return true;
        case ACTION_SNEAK:
            _Show(_Sneak());
            return true;
        case ACTION_BRIBE:
            _Show(_BribeGuards());
            return true;
        case ACTION_PAY_FINE:
            _Show(_PayFine());
            return true;
        case ACTION_RUN:
            _Show(_RunFromWatch());
            return true;
        case ACTION_FIGHT: {
            const int next = _FightWatch();
            if (next >= 0)
                _Show(next);
            return true;
        }
        case ACTION_WATCH_RETURN:
            _Show(fWatchReturn);
            return true;
        case ACTION_GUARDS_FIGHT:
            _FightGuards();
            return true;
        case ACTION_GUARDS_TALK:
            _Show(_TalkToGuards());
            return true;
        case ACTION_GUARDS_BRIBE:
            _Show(_BribeChallenge());
            return true;
        case ACTION_CHALLENGE_RETURN:
            _Show(fChallengeReturn);
            return true;
        case ACTION_TO_PRISON:
            _Show(_EnterPrison());
            return true;
        case ACTION_BACK_TO_CELL:
            _Show(SCREEN_CELL);
            return true;
        case ACTION_PICK_LOCK:
            _Show(_PickLock());
            return true;
        case ACTION_CLIMB_WINDOW:
            _Show(_ClimbWindow());
            return true;
        case ACTION_DIG:
            _Show(_Dig());
            return true;
        case ACTION_SEDUCE:
            _Show(_Seduce());
            return true;
        case ACTION_PRAY:
            _Show(_Pray());
            return true;
        case ACTION_AFTER_PRAYER:
            _Show(fMagistrateComing ? SCREEN_TO_MAGISTRATE : SCREEN_CELL);
            return true;
        case ACTION_WAIT_MAGISTRATE:
            _Show(_WaitForMagistrate());
            return true;
        case ACTION_JAIL_FIGHT:
            _FightJailGuards();
            return true;
        case ACTION_TO_COURT:
            _Show(_EnterCourt());
            return true;
        case ACTION_KEEP_SILENT:
            _Show(_KeepSilent());
            return true;
        case ACTION_PLEAD_INNOCENT:
            _Show(_Plead(false));
            return true;
        case ACTION_CONFESS_GUILT:
            _Show(_Plead(true));
            return true;
        case ACTION_TO_EXECUTION:
            _Show(SCREEN_EXECUTION);
            return true;
        case ACTION_SUBMIT:
        case ACTION_RESCUE:
            _Show(_Rescue());
            return true;
        case ACTION_BREAK_ROPES:
            _Show(_BreakRopes());
            return true;
        case ACTION_EXECUTION_FIGHT:
        case ACTION_MOB_FIGHT:
            _FightAtExecution();
            return true;
        case ACTION_CHALLENGE_RUN:
            // file 0x917EE: an hour, then the chase
            if (fClock != NULL)
                fClock->AddHours(1);
            _Show(_EnterChase());
            return true;
        case ACTION_TO_CHASE:
            _Show(_EnterChase());
            return true;
        case ACTION_CHASE_RUN:
            _Show(_OutrunGuards());
            return true;
        case ACTION_CHASE_FIGHT:
            _FightPursuers();
            return true;
        case ACTION_CHASE_AMBUSH:
            _Show(_Ambush());
            return true;
        case ACTION_CHASE_HIDE:
            _Show(_Hide());
            return true;
        case ACTION_SAINT:
            _ShowSaints();
            return true;
        case ACTION_SAINT_RESCUE:
            _Show(_Rescue(fRescueSaint));
            return true;
        case ACTION_EXIT_WALK: {
            const int next = _ExitWalk();
            if (next < 0)
                return false;
            _Show(next);
            return true;
        }
        case ACTION_EXIT_HIDE:
            _Show(_ExitHide());
            return true;
        case ACTION_EXIT_FIGHT:
            _FightAtGate(false);
            return true;
        case ACTION_AFTER_SHOUT:
            if (fGateShoutFight)
                _FightAtGate(true);
            else
                _Show(SCREEN_GATE);
            return true;
        case ACTION_GATE_RETURN:
            // file 0xBD53C: back to the previous state
            _Show(fGateReturn);
            return true;
        case ACTION_INNER_SEWER:
            _Show(_Sewer(rule.target == 1));
            return true;
        case ACTION_INNER_BRIBE:
            _Show(_BribeSally());
            return true;
        case ACTION_INNER_ROPE:
            _Show(_RopeDown(rule.target == 1));
            return true;
        case ACTION_INNER_CLIMB:
            _Show(_ClimbOver(rule.target == 1));
            return true;
        case ACTION_INNER_AFTER_DARK: {
            const int option = fAfterDark;
            fAfterDark = -1;
            _Show(option == 4 ? _RopeDown(false) : _ClimbOver(false));
            return true;
        }
        case ACTION_INN_NEWS:
            _Show(_InnNews());
            return true;
        case ACTION_NEWS:
            // the square (file 0x9DB66): an hour; the slum (0xAB15A): city
            // size / 2 hours
            if (fClock != NULL) {
                fClock->AddHours(uint32(rule.minutes == kHalfSize
                    ? _City().size / 2
                    : rule.minutes / 60));
            }
            fNewsReturn = rule.target;
            fNoticesFromSquare = false;
            _Show(SCREEN_NEWS);
            return true;
        case ACTION_NEWS_RETURN:
            // file 0xE3EF6: back where the news came from (DS:E7D8)
            _Show(fNewsReturn);
            return true;
        case ACTION_INN_RAID:
            _Show(_Challenge(SCREEN_INN));
            return true;
        case ACTION_NOTICES:
            _Show(_Notices());
            return true;
        case ACTION_AFFAIRS:
            _Show(_Affairs());
            return true;
        case ACTION_NEWS_NEXT:
            _Show(_NextNews());
            return true;
        case ACTION_BANK_TASKS:
            _Show(_BankTasks(rule.target));
            return true;
        case ACTION_QUEST_OFFER:
            _Show(_OfferQuest());
            return true;
        case ACTION_TOWER_SIEGE:
            _Mark(kMarkTowerAsked, 6480);
            _Show(_LaySiege());
            return true;
        case ACTION_TOWER_ASK:
            _Mark(kMarkTowerAsked, 6480);
            _Show(_AskInside());
            return true;
        case ACTION_TOWER_DUEL:
            _Mark(kMarkTowerAsked, 6480);
            _Show(_Duel());
            return true;
        case ACTION_TOWER_SNEAK:
            _Mark(kMarkTowerAsked, 6480);
            _Show(_SneakIntoTower());
            return true;
        case ACTION_TOWER_STORM:
            _Mark(kMarkTowerAsked, 6480);
            _Show(_StormTower());
            return true;
        case ACTION_TOWER_FIGHT_KNIGHT:
            _FightKnight();
            return true;
        case ACTION_TOWER_FIGHT_MEN:
            _FightKnightsMen();
            return true;
        case ACTION_TOWER_INSIDE:
            _Show(_TowerInside(rule.target));
            return true;
        case ACTION_SLUM_LODGING:
            _Show(_SlumLodging());
            return true;
        case ACTION_SLUM_CAMP:
            fSlumCamp = true;
            fPendingResidence = true;
            return true;
        case ACTION_MEET_THIEVES:
            _Show(_MeetThieves());
            return true;
        case ACTION_THIEVES_GROVEL:
            _Robbed();
            _Show(SCREEN_THIEVES_ROBBED);
            return true;
        case ACTION_THIEVES_TALK:
            _Show(_TalkToThieves());
            return true;
        case ACTION_THIEVES_SCARE:
            _Show(_ScareThieves());
            return true;
        case ACTION_THIEVES_RUN:
            _Show(_RunFromThieves());
            return true;
        case ACTION_THIEVES_FIGHT:
            _FightThieves();
            return true;
        case ACTION_THIEVES_RETURN:
            if (fThievesReturn < 0)
                return false;			// the map
            _Show(fThievesReturn);
            return true;
        case ACTION_PILGRIMS_GO:
            _Show(SCREEN_PILGRIMS_WISHED);
            return true;
        case ACTION_PILGRIMS_GIVE:
            _Show(_PilgrimsGive());
            return true;
        case ACTION_PILGRIMS_MOUNTS:
            _Show(_PilgrimsMounts());
            return true;
        case ACTION_PILGRIMS_ESCORT:
            _Show(SCREEN_PILGRIMS_ESCORT);
            return true;
        case ACTION_PILGRIMS_ARRIVE:
            _Show(_PilgrimsArrive());
            return true;
        case ACTION_BLIZZARD_ONWARD:
            _Show(_BlizzardOnward());
            return true;
        case ACTION_BLIZZARD_CAMP:
            _Show(_BlizzardCamp());
            return true;
        case ACTION_BOG_PULL:
            _Show(_BogPull(rule.target));
            return true;
        case ACTION_BOG_ABANDON:
            _Show(_BogAbandon());
            return true;
        case ACTION_FLOOD_SEARCH:
            _Show(_FloodSearch());
            return true;
        case ACTION_FLOOD_RAFT:
            _Show(_FloodRaft());
            return true;
        case ACTION_WOLVES_LORE:
            _Show(_WolvesLore());
            return true;
        case ACTION_WOLVES_RIDE:
            _Show(_WolvesRide());
            return true;
        case ACTION_WOLVES_BATTLE:
            _Show(_WolvesBattle());
            return true;
        case ACTION_BOARS_DODGE:
            _Show(_BoarsDodge());
            return true;
        case ACTION_BOARS_RIDE:
            _Show(_BoarsRide());
            return true;
        case ACTION_BOARS_BATTLE:
            _Show(_BoarsBattle());
            return true;
        case ACTION_HERMIT_MEET:
            _Show(_HermitMeet());
            return true;
        case ACTION_HERMIT_TRAIN:
            _Show(_HermitTrain());
            return true;
        case ACTION_HERMIT_PRAY:
            _Show(_HermitPray());
            return true;
        case ACTION_HERMIT_TEACH:
            _Show(_HermitTeach(rule.target));
            return true;
        case ACTION_TITHE_PAY:
            _PayMeetingMoney();
            _Show(SCREEN_TITHE_PAID);
            return true;
        case ACTION_TITHE_PLEAD:
            fMeetReturn = fScreen;
            _Show(_TithePlead());
            return true;
        case ACTION_TITHE_REFUSE:
            _Show(_TitheRefuse());
            return true;
        case ACTION_TITHE_ESCAPE:
            _Show(_TitheEscape());
            return true;
        case ACTION_TITHE_SUBMIT:
            _Show(_TitheSubmit());
            return true;
        case ACTION_TITHE_FIGHT:
            _FightTithe();
            return true;
        case ACTION_TITHE_RETURN:
            _Show(fMeetReturn);
            return true;
        case ACTION_CARAVAN_NEWS:
            _Show(_CaravanNews());
            return true;
        case ACTION_CARAVAN_TRAVEL:
            _Show(_CaravanTravel());
            return true;
        case ACTION_CARAVAN_ACCEPT:
            _Show(_CaravanAccept());
            return true;
        case ACTION_CARAVAN_DECLINE:
            _Show(SCREEN_CARAVAN_DECLINED);
            return true;
        case ACTION_CARAVAN_AVOID:
            _Show(_CaravanAvoid());
            return true;
        case ACTION_CARAVAN_ATTACK:
            _FightCaravan(false);
            return true;
        case ACTION_CARAVAN_RUN:
            _Show(_CaravanRun());
            return true;
        case ACTION_CARAVAN_FIGHT:
            _FightCaravan(true);
            return true;
        case ACTION_MEET_BACK:
            _Show(fMeetBack);
            return true;
        case ACTION_MEET_TALK:
            _Show(SCREEN_CARAVAN_TALK);
            return true;
        case ACTION_REFUGEES_GIVE:
            _Show(_RefugeesGive());
            return true;
        case ACTION_REFUGEES_AVOID:
            // file 0x13CDB0: an hour, and on
            if (fClock != NULL)
                fClock->AddHours(1);
            return false;
        case ACTION_REFUGEES_IGNORE:
            _Show(SCREEN_REFUGEES_AMBUSH);
            return true;
        case ACTION_REFUGEES_BARGAIN:
            _Show(_RefugeesBargain());
            return true;
        case ACTION_REFUGEES_FIGHT:
            _FightRefugees();
            return true;
        case ACTION_REFUGEES_SURRENDER:
            _Show(_RefugeesSurrender());
            return true;
        case ACTION_CAMPJ_IGNORE:
            _Show(_CampSoldiersIgnore());
            return true;
        case ACTION_CAMPJ_TALK:
            fMeetBack = fScreen;
            _Show(_CampSoldiersTalk());
            return true;
        case ACTION_CAMPJ_PAY:
            _PayMeetingMoney();
            _Mark(kMarkCampSafe, 168);
            _Show(SCREEN_CAMPJ_PAID);
            return true;
        case ACTION_CAMPJ_FIGHT:
            _FightAtCamp(true);
            return true;
        case ACTION_CAMPB_IGNORE:
            _Show(SCREEN_CAMPB_RAID);
            return true;
        case ACTION_CAMPB_AMBUSH:
            _Show(_CampBanditsAmbush());
            return true;
        case ACTION_CAMPB_FIGHT:
            _FightAtCamp(false);
            return true;
        case ACTION_FRIAR_PAY:
            _PayMeetingMoney();
            _Show(SCREEN_FRIAR_PAID);
            return true;
        case ACTION_FRIAR_FIGHT:
            _FightFriar();
            return true;
        case ACTION_FRIAR_LEAVE:
            _FriarCurse();
            _Show(SCREEN_FRIAR_CURSED);
            return true;
        case ACTION_BANDITS_IGNORE:
            _Show(SCREEN_BANDITS_AMBUSH);
            return true;
        case ACTION_BANDITS_TALK:
            _Show(_BanditsTalk());
            return true;
        case ACTION_BANDITS_SURRENDER:
            _Robbed();
            if (fClock != NULL)
                fClock->AddHours(1);
            _Show(SCREEN_BANDITS_SURRENDERED);
            return true;
        case ACTION_BANDITS_SNEAK:
            _Show(_BanditsSneak());
            return true;
        case ACTION_BANDITS_SCOUT:
            if (fClock != NULL)
                fClock->AddHours(1);
            _Show(SCREEN_BANDITS_SCOUTED);
            return true;
        case ACTION_BANDITS_CHARGE:
            _Show(SCREEN_BANDITS_CHARGE);
            return true;
        case ACTION_BANDITS_FIGHT:
            _FightBandits();
            return true;
        case ACTION_BANDITS_RETURN:
            _Show(fBanditsReturn);
            return true;
        case ACTION_SHELL_PAY:
            // file 0x110DCA: a groschen, and the pea seems to be under one
            // of the shells at random (cards 4..6)
            if (fParty != NULL)
                fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash)
                    - 12);
            _Show(SCREEN_SHELL_RIGHT + int(fRandom() % 3));
            return true;
        case ACTION_SHELL_PICK:
            _Show(_PlayShells(rule.target));
            return true;
        case ACTION_MONKS_PRAY:
            _Show(_MonksPrayers());
            return true;
        case ACTION_MONKS_TUTORING:
            _Show(_AskTutoring());
            return true;
        case ACTION_MONKS_LIBRARY:
            _Show(_AskLibrary());
            return true;
        case ACTION_MONKS_HEALING:
            _Show(_AskAbbess());
            return true;
        case ACTION_MONASTERY_ANSWER:
            if (fMonkAnswer == SCREEN_NOT_IMPLEMENTED)
                fPreviousScreen = SCREEN_CHURCHES;	// DS:E7D8 = 0x13
            _Show(fMonkAnswer);
            return true;
        case ACTION_MONASTERY_BACK:
            _Show(fMonastery);
            return true;
        case ACTION_MONKS_NIGHT_PRAY:
            _Show(_NightPrayers());
            return true;
        case ACTION_MONKS_NIGHT_HELP:
            _Show(_NightHelp());
            return true;
        case ACTION_MONKS_NIGHT_SANCTUARY:
            // file 0xBC12C: mark 0x35 for 8 hours, card 8
            _Mark(kMarkMonksBothered, 8);
            _Show(SCREEN_MONKS_NIGHT_NO_SANCTUARY);
            return true;
        case ACTION_BANK_REWARD:
            _Show(_BankReward(rule.target));
            return true;
        case ACTION_PATRON_THANKS:
            _Show(_PatronThanks());
            return true;
        case ACTION_THANKS_RETURN:
            _Show(fThanksReturn);
            return true;
        case ACTION_ABBESS:
            fPreviousScreen = SCREEN_CHURCHES;	// DS:E7D8 = 0x13
            _Show(SCREEN_NOT_IMPLEMENTED);
            return true;
        case ACTION_GROVE:
            _Show(_Grove(rule.target));
            return true;
        case ACTION_SHELL_LEAVE:
            _Show(fShellReturn);			// file 0x110E48: no time passes
            return true;
        case ACTION_CATHEDRAL_MASS:
            _Show(_CathedralMass());
            return true;
        case ACTION_CATHEDRAL_PRELATE:
            _Show(_CathedralPrelate());
            return true;
        case ACTION_CATHEDRAL_GIFT:
            _Show(_CathedralGift());
            return true;
        case ACTION_CATHEDRAL_RELIC:
            _Show(_CathedralRelic());
            return true;
        case ACTION_SANCTUARY:
            _Show(SCREEN_SANCTUARY);
            return true;
        case ACTION_SANCTUARY_REST:
            _Show(_SanctuaryRest(rule.target));
            return true;
        case ACTION_SANCTUARY_SURRENDER:
            _Show(_SanctuarySurrender());
            return true;
        case ACTION_SANCTUARY_WORD:
            _Show(_SanctuaryWord());
            return true;
        case ACTION_SANCTUARY_SNEAK:
            _Show(_SanctuarySneak());
            return true;
        case ACTION_SANCTUARY_BACK:
            _Show(SCREEN_SANCTUARY);
            return true;
        case ACTION_PARTY:
            fPartyReturn = fScreen;
            _Show(SCREEN_PARTY);
            return true;
        case ACTION_PARTY_FIND:
            _Show(_PartyLooking());
            return true;
        case ACTION_PARTY_RECRUITS:
            fPendingSelection = true;
            return true;
        case ACTION_PARTY_RETIRE:
            _Show(_Retire(rule.target));
            return true;
        case ACTION_PARTY_AGAIN:
            _Show(SCREEN_PARTY);
            return true;
        case ACTION_PARTY_DONE:
            _Show(fPartyReturn);
            return true;
        case ACTION_LORD_AUDIENCE:
            fLordHall = fScreen == SCREEN_TOWN_HALL;
            _Show(_LordRequest(false));
            return true;
        case ACTION_LORD_CLERK:
            fLordHall = fScreen == SCREEN_TOWN_HALL;
            _Show(_LordRequest(true));
            return true;
        case ACTION_LORD_NEXT: {
            const int next = fLordQueue.empty() ? fScreen : fLordQueue.front();
            if (!fLordQueue.empty())
                fLordQueue.erase(fLordQueue.begin());
            _Show(next);
            return true;
        }
        case ACTION_SWIM:
            _Show(_Swim());
            return true;
        case ACTION_SWIM_NEXT:
            return _SwimNext();
        case ACTION_AFTER_CARD:
            if (fAfterCard < 0)
                return false;			// back to the map (state 0xC)
            _Show(fAfterCard);
            return true;
        case ACTION_QUEST_RETURN:
            // back to the patron (DS:E7D8)
            if (fQuestPatron == 10 && fQuestReturn >= 0)
                _Show(fQuestReturn);
            else
                _Show(fQuestPatron == 6 ? SCREEN_MEDICI : SCREEN_FUGGER);
            return true;
        case ACTION_GOSSIP:
            _Show(_Gossip());
            return true;
        case ACTION_SALLY_CHALLENGE:
            _Show(_Challenge(SCREEN_INNER_WALL));
            return true;
        case ACTION_CALL_PRIEST:
            // file 0x991CA: three hours; the cell and the tunnel are kept
            if (fClock != NULL)
                fClock->AddHours(3);
            _Show(SCREEN_PRIEST);
            return true;
        case ACTION_PRIEST_CONFESSION:
            _Show(_ConfessToPriest());
            return true;
        case ACTION_PRIEST_HELP:
            _Show(_AskPriestForHelp());
            return true;
        case ACTION_PRIEST_GOOD_WORD:
            _Show(_AskGoodWord());
            return true;
        case ACTION_AFTER_GOOD_WORD:
            _Show(fMagistrateComing ? SCREEN_PRIEST_MAGISTRATE
                : _BackFromPriest());
            return true;
        case ACTION_FROM_PRIEST:
            _Show(_BackFromPriest());
            return true;
        case ACTION_GATE_DAY:
            _Show(_GoToGate(true));
            return true;
        case ACTION_GATE_NIGHT:
            _Show(_GoToGate(false));
            return true;
        case ACTION_PAY_TOLL:
            _Show(_PayToll());
            return true;
        case ACTION_CHARM_GUARDS:
            _Show(_CharmGuards());
            return true;
        case ACTION_SLIP_IN:
            _Show(_SlipIn());
            return true;
        case ACTION_BACK_TO_GATE:
            // states 2 or 3 by 1367:072A
            _Show(fClock == NULL || IsGameDay(*fClock) ? SCREEN_DAY_GATE
                : SCREEN_NIGHT_GATE);
            return true;
        case ACTION_HAIL_WATCH:
            _Show(_HailWatch());
            return true;
        case ACTION_TALK_TO_WATCH:
            _Show(_TalkToWatch());
            return true;
        case ACTION_BRIBE_WATCH:
            _Show(_BribeWatch());
            return true;
        case ACTION_WALL_DAY:
            _Show(_GoToWall(true));
            return true;
        case ACTION_WALL_NIGHT:
            _Show(_GoToWall(false));
            return true;
        case ACTION_BRIBE_WALL:
            _Show(_BribeWall());
            return true;
        case ACTION_ROPE:
            _Show(_ClimbWithRope(fScreen == SCREEN_DAY_WALL));
            return true;
        case ACTION_CLIMB:
            _Show(_ClimbAlone(fScreen == SCREEN_DAY_WALL));
            return true;
        case ACTION_GRATE:
            _Show(_ForceGrate());
            return true;
        case ACTION_BACK_TO_WALL:
            // states 0xE or 0xF by 1367:072A
            _Show(fClock == NULL || IsGameDay(*fClock) ? SCREEN_DAY_WALL
                : SCREEN_NIGHT_WALL);
            return true;
        case ACTION_FALL_BACK:
            // file 0x93BF2: card 12, an hour, before the walls
            if (fClock != NULL)
                fClock->AddHours(1);
            _Show(SCREEN_NIGHT_GATE_RETIRED);
            return true;
        case ACTION_LEAVE_PHYSICIAN:
            _Show(_LeavePhysician(false));
            return true;
        case ACTION_APOLOGIZE:
            _Show(_LeavePhysician(true));
            return true;
        case ACTION_MASS:
            _Show(_Mass());
            return true;
        case ACTION_CONFESSION:
            _Show(_Confession());
            return true;
        case ACTION_DONATION:
            _Show(_Donation());
            return true;
        case ACTION_ALTAR_BOY:
            _Show(_AltarBoy());
            return true;
        default:
            fPreviousScreen = fScreen;
            _Show(SCREEN_NOT_IMPLEMENTED);
            return true;
    }
}


/* static */
void
CityVisit::AddCityVariables(GameData& data, int cityIndex,
    card_variables& variables)
{
    // place variables by DARKLAND.CTY slot: inferred from the names (the
    // list of variable names is in DARKLAND.EXE) and the texts
    static const struct {
        const char* name;
        int place;
    } kPlaceVariables[] = {
        { "CityLordName", CITY_RULER },		// DARKLAND.EXE, file 0x8DCD5
        { "citySquare", CITY_SQUARE }, { "councilHall", CITY_TOWN_HALL },
        { "fortress", CITY_CASTLE }, { "cathedral", CITY_CATHEDRAL },
        { "cityChurch", CITY_CHURCH }, { "marketplace", CITY_MARKET },
        { "imperialMint", CITY_MINT_SQUARE }, { "slum", CITY_SLUMS },
        { "cityBarracks", CITY_ARMORY }, { "pawnshop", CITY_PAWNSHOP },
        { "monastery", CITY_MONASTERY }, { "Inn", CITY_INN },
        { "inn", CITY_INN }, { "university", CITY_UNIVERSITY }
    };
    const city& c = data.Cities().CityAt(uint32(cityIndex));
    variables["PlaceName"] = c.shortName;
    const DescriptionFile& descriptions = data.CityDescriptions();
    if (uint32(cityIndex) < descriptions.CountDescriptions())
        variables["PlaceDesc"] = descriptions.DescriptionAt(uint32(cityIndex));
    for (const auto& variable : kPlaceVariables) {
        if (!c.places[variable.place].empty())
            variables[variable.name] = c.places[variable.place];
    }
    variables["CityLordTitle"] = CityLordTitle(c);
}


// $CityLordTitle (DARKLAND.EXE, file 0x8DC82): the ruler in its seat; in
// a city ruled for him, one of six officers, in a free city one of nine
// (tables at 290E:2323 and 233B), by the city's number (+0x56). The
// location's flag 0x80, which would switch to the city record's +0x5A,
// is not kept here.
/* static */
std::string
CityVisit::CityLordTitle(const city& c)
{
    static const char* kOfficers[6] = {
        "Vogt", "Erbvogt", "Obervogt", "Burggraf", "Richter", "Landhofmeister"
    };
    static const char* kCouncils[9] = {
        "alte Herr", "\xC3\x84ltere Herren", "Frager", "Losunger",
        "alte Losunger", "Oberste Hauptm\xC3\xA4nn", "Schultheiss",
        "Sch\xC3\xB6" "ff", "B\xC3\xBCrgermeister"
    };
    switch (c.rule) {
        case CITY_CAPITAL:
            return c.places[CITY_RULER];
        case CITY_RULED:
            return kOfficers[c.peopleSeed % 6];
        default:
            return kCouncils[c.peopleSeed % 9];
    }
}


/* static */
void
CityVisit::AddPartyVariables(const party& members, card_variables& variables)
{
    if (members.members.empty())
        return;
    static const char* kOrdinals[kMaxPartySize] = {
        "One", "Two", "Three", "Four", "Five"
    };
    // who the game "chooses" for a scene is unknown: the leader first
    std::vector<const character*> chosen;
    chosen.push_back(&members.members[members.leader]);
    for (size_t i = 0; i < members.members.size(); i++) {
        if (int(i) != members.leader)
            chosen.push_back(&members.members[i]);
    }
    for (size_t i = 0; i < chosen.size() && i < size_t(kMaxPartySize); i++)
        variables[std::string("Chosen") + kOrdinals[i] + "Name"] = chosen[i]->shortName;
    variables["LeaderName"] = chosen[0]->shortName;

    const bool female = chosen[0]->female;
    variables["he"] = female ? "she" : "he";
    variables["He"] = female ? "She" : "He";
    variables["his"] = female ? "her" : "his";
    variables["His"] = female ? "Her" : "His";
    variables["him"] = female ? "her" : "him";
    variables["himself"] = female ? "herself" : "himself";
}


void
CityVisit::_Show(int screen, bool withScene)
{
    const int previous = fScreen;
    fScreen = screen;
    fChoosingSaint = false;
    fNight = fClock != NULL && fClock->IsNight();
    if (fClock != NULL) {
        fVariables["CurrentBell"] = fClock->BellName();
        fVariables["MonthName"] = fClock->MonthName();
    }
    if (screen == SCREEN_BANDITS_MEET)
        screen = _MeetBandits();
    if (screen == SCREEN_PILGRIMS_MEET)
        screen = _MeetPilgrims();
    else if (screen == SCREEN_HERMIT_MEET)
        screen = _MeetHermit();
    else if (screen == SCREEN_TITHE_MEET)
        screen = _MeetTithe();
    else if (screen == SCREEN_FRIAR_MEET)
        screen = _MeetFriar();
    else if (screen == SCREEN_CARAVAN_MEET)
        screen = _MeetCaravan();
    else if (screen == SCREEN_REFUGEES_MEET)
        screen = _MeetRefugees();
    else if (screen == SCREEN_CAMPJ_MEET)
        screen = _MeetCampSoldiers();
    else if (screen == SCREEN_CAMPB_MEET)
        screen = _MeetCampBandits();
    else if (screen == SCREEN_BLIZZARD_MEET)
        screen = _MeetBlizzard();
    else if (screen == SCREEN_BOG_MEET)
        screen = _MeetBog();
    else if (screen == SCREEN_FLOOD_MEET)
        screen = _MeetFlood();
    else if (screen == SCREEN_WOLVES_MEET)
        screen = _MeetWolves();
    else if (screen == SCREEN_BOARS_MEET)
        screen = _MeetBoars();
    if (screen == SCREEN_THIEVES_MAP_MEET) {
        fThievesReturn = -1;
        screen = _MeetThieves();
        fScreen = screen;
    }
    // arriving at the monastery
    if (screen == SCREEN_MONASTERY && !_InMonastery(previous))
        screen = _EnterMonastery();
    if (screen == SCREEN_MONASTERY || screen == SCREEN_MONASTERY_AGAIN)
        fVariables["Money1"] = MoneyText(_MonksPrice());
    // the shell game man, by day, around the city's feast, once a day
    // (the square, file 0x9D6EC; the market, 0x9F676)
    if ((screen == SCREEN_SQUARE || screen == SCREEN_MARKET)
            && previous != screen && !fNight && _FeastNear()
            && !_Marked(kMarkShellGame)) {
        _Mark(kMarkShellGame, 24);
        fShellReturn = screen;
        screen = SCREEN_SHELL_GAME;
    }
    // the docks at night: the river freezes in the cold months (file 0xA9893:
    // November to May the swimming options are not offered, card 1)
    if (screen == SCREEN_DOCKS && fNight && _ColdWater())
        screen = SCREEN_DOCKS_ICE;
    // the cathedral: $Money1 is a third of the purse
    if (screen == SCREEN_CATHEDRAL && fParty != NULL)
        fVariables["Money1"] = MoneyText(TotalPfennigs(fParty->cash) / 3);
    // the sanctuary: the captain of the guard (1367:0DB4, the city's seed +
    // 0x156)
    if (screen == SCREEN_SANCTUARY || (screen >= SCREEN_SANCTUARY_FREE
            && screen <= SCREEN_SANCTUARY_HARSH))
        fVariables["NamedOneName"] = _PersonName(uint16(_PeopleSeed() + 0x156));
    // the party's composition: $ChosenOneName..$ChosenFiveName are the
    // members in their order
    if (screen == SCREEN_PARTY && fParty != NULL) {
        static const char* kNames[5] = { "ChosenOneName", "ChosenTwoName",
            "ChosenThreeName", "ChosenFourName", "ChosenFiveName" };
        for (size_t i = 0; i < 5 && i < fParty->members.size(); i++)
            fVariables[kNames[i]] = fParty->members[i].shortName;
    }
    // swimming away: the names on the cards
    if (screen >= SCREEN_SWIM_ALL && screen <= SCREEN_SWIM_FOLLOW
            && !fSwimSteps.empty()) {
        const swim_step& step = fSwimSteps.front();
        fVariables["ChosenOneName"] = step.first;
        fVariables["ChosenTwoName"] = step.second;
        fVariables["ChosenThreeName"] = step.lost;
        fVariables["he"] = step.female ? "she" : "he";
        fVariables["He"] = step.female ? "She" : "He";
        fVariables["his"] = step.female ? "her" : "his";
        fVariables["His"] = step.female ? "Her" : "His";
        fVariables["him"] = step.female ? "her" : "him";
        fVariables["himself"] = step.female ? "herself" : "himself";
    }
    // a wanted party is not welcome at the inn (DARKLAND.EXE: a
    // reputation of -40 or less)
    if (screen == SCREEN_INN && _Reputation() <= -40)
        screen = SCREEN_UNWELCOME;
    // the physician shuts his door to a wanted party
    if (screen == SCREEN_PHYSICIAN && _Reputation() <= -40)
        screen = SCREEN_PHYSICIAN_SHUT;
    else if (screen == SCREEN_PHYSICIAN && fClock != NULL
            && !IsGameDay(*fClock))
        screen = SCREEN_PHYSICIAN_NIGHT;
    if (screen == SCREEN_PHYSICIAN || screen == SCREEN_PHYSICIAN_SHUT
            || screen == SCREEN_PHYSICIAN_NIGHT) {
        // the offer is off at each entry to the physician's code
        // (DARKLAND.EXE 1838:0084 clears DS:EE7E)
        if (!_InPhysician(previous))	// a new visit
            fTreatmentOffered = false;
        _PhysicianSkill();
        // his name: the city's property 0x21 + 800
        fVariables["NamedOneName"] = _PersonName(uint16(_PeopleSeed() + 0x320));
        if (fParty != NULL && !fParty->members.empty())
            _SetChosen(_BestHealer());
    }
    // the master banker and the League's master: the city's number + 8
    // (the Fuggers, file 0xC4280), + 6 (the Medici, file 0xC62E9), + 7
    // (the Hanse, file 0xC7BBF)
    if ((screen >= SCREEN_FUGGER && screen <= SCREEN_MEDICI_DEPOSIT)
            || screen == SCREEN_FUGGER_REWARD || screen == SCREEN_MEDICI_REWARD) {
        const bool fugger = screen == SCREEN_FUGGER
            || screen == SCREEN_FUGGER_COLD || screen == SCREEN_FUGGER_REDEEMED
            || screen == SCREEN_FUGGER_DEPOSIT || screen == SCREEN_FUGGER_REWARD;
        const uint16 number = _City().peopleSeed;
        fVariables["NamedOneName"] = _PersonName(uint16(number
            + (fugger ? 8 : screen == SCREEN_HANSE ? 7 : 6)));
    }
    // the alchemist (file 0xD9E30): angry for a while, not found by a
    // party that knows too little alchemy, closed at night; card 1 after
    // the first question
    if (screen == SCREEN_ALCHEMIST || screen == SCREEN_ALCHEMIST_AGAIN) {
        const uint32 now = fClock != NULL ? fClock->HourStamp() : 0;
        const std::map<int, uint32>::const_iterator angry
            = fAlchemistAngryUntil.find(fCity);
        // "found" if (the seed global + the location) % 30 is under the
        // best Alchemy, at least 10
        const bool found = int((fSeed + fCity) % 30)
            < std::max(10, std::min(_BestSkill(kSkillAlchemy), 99));
        if (screen == SCREEN_ALCHEMIST)
            fStoneOffered = false;
        if (angry != fAlchemistAngryUntil.end() && now < angry->second)
            screen = SCREEN_ALCHEMIST_ANGRY;
        else if (!found)
            screen = SCREEN_ALCHEMIST_UNKNOWN;
        else if (fClock != NULL && !IsGameDay(*fClock))
            screen = SCREEN_ALCHEMIST_NIGHT;
        fVariables["NamedOneName"] = _PersonName(uint16(_PeopleSeed() + 0x49));
        fVariables["Money1"] = MoneyText(_StonePrice());
        fVariables["Text4"] = _AlchemistSkill(true) >= 25 ? "Potions"
            : "Alchemical Components";
        if (fParty != NULL && !fParty->members.empty()) {
            int best = 0;
            for (size_t i = 1; i < fParty->members.size(); i++) {
                if (fParty->members[i].skills[kSkillAlchemy]
                        > fParty->members[best].skills[kSkillAlchemy])
                    best = int(i);
            }
            _SetChosen(best);
        }
    }
    // the market at night: watched for a while after trouble; the bribe
    if (screen == SCREEN_MARKET && fClock != NULL && fClock->IsNight()
            && _Marked(kMarkGuarded))
        screen = SCREEN_MARKET_GUARDED;
    if (screen == SCREEN_MARKET || screen == SCREEN_MARKET_GUARDED)
        fVariables["Money1"] = MoneyText(_Bribe());
    // the night watch: "Not you again" within 7 hours (0E76:2930(0x40, 7))
    if (screen == SCREEN_NIGHT_WATCH || screen == SCREEN_NIGHT_WATCH_MARKET) {
        if (screen == SCREEN_NIGHT_WATCH && _Marked(kMarkWatchMet))
            screen = SCREEN_NIGHT_WATCH_AGAIN;
        _Mark(kMarkWatchMet, 7);
    }
    if (screen == SCREEN_NIGHT_WATCH || screen == SCREEN_NIGHT_WATCH_MARKET
            || screen == SCREEN_NIGHT_WATCH_AGAIN
            || screen == SCREEN_NIGHT_WATCH_CAUGHT
            || screen == SCREEN_WATCH_UNANSWERED)
        fVariables["Money1"] = MoneyText(_Fine());
    // before the walls: the card of the city's rule (file 0x942E0), and
    // what the party expects there
    if (screen == SCREEN_OUTSIDE) {
        const int rule = _City().rule;
        if (rule == CITY_CAPITAL)
            screen = SCREEN_OUTSIDE_CAPITAL;
        else if (rule != CITY_RULED)
            screen = SCREEN_OUTSIDE_FREE;
    }
    if (screen == SCREEN_OUTSIDE || screen == SCREEN_OUTSIDE_CAPITAL
            || screen == SCREEN_OUTSIDE_FREE) {
        fVariables["PlaceAttitude"] = ReputationWord(_Reputation());
    }
    // the gate by day: nervous guards (mark 0x12: card 18), the toll
    if (screen == SCREEN_DAY_GATE && _Marked(kMarkAlert))
        screen = SCREEN_DAY_GATE_GUARDED;
    if (screen == SCREEN_DAY_GATE || screen == SCREEN_DAY_GATE_GUARDED)
        fVariables["Money1"] = MoneyText(_Toll());
    if (screen == SCREEN_NIGHT_GATE || screen == SCREEN_NIGHT_GATE_ALERTED
            || screen == SCREEN_NIGHT_GATE_BRIBED)
        fVariables["Money1"] = MoneyText(_NightBribe());
    // the wall: its bribe; a failed climb holds for this stay only (the
    // handler's disabled options), a new one begins before the walls or
    // when the day turns to night and back
    if (screen == SCREEN_DAY_WALL || screen == SCREEN_DAY_WALL_BRIBED)
        fVariables["Money1"] = MoneyText(_WallBribe());
    // the dungeon's cell, the magistrate after the torture
    if (screen == SCREEN_CELL)
        screen = _CellScreen();
    if (screen == SCREEN_MAGISTRATE && fTortures > 0)
        screen = SCREEN_MAGISTRATE_AGAIN;
    // the gate from inside: where "not leave just yet" goes back to (the
    // previous state, DS:A88D); the wall's bribe
    if (screen == SCREEN_GATE && previous != SCREEN_GATE
            && previous != SCREEN_NOT_IMPLEMENTED
            && (previous < SCREEN_GATE_SHOUT || previous > SCREEN_STUMBLING))
        fGateReturn = previous;
    if (screen == SCREEN_INNER_WALL || screen == SCREEN_SALLY_BRIBED)
        fVariables["Money1"] = MoneyText(_InnerWallBribe());
    if (screen == SCREEN_CHALLENGE || screen == SCREEN_CHALLENGE_BRIBED
            || screen == SCREEN_CHALLENGE_REFUSED)
        fVariables["Money1"] = MoneyText(_ChallengeBribe());
    if (screen == SCREEN_OUTSIDE || screen == SCREEN_OUTSIDE_CAPITAL
            || screen == SCREEN_OUTSIDE_FREE
            || (screen == SCREEN_DAY_WALL && previous != SCREEN_DAY_WALL_FALL
                && previous != SCREEN_DAY_WALL_HELP
                && previous != SCREEN_DAY_WALL_SLIP_ALONE
                && previous != SCREEN_DAY_WALL_UNANSWERED)
            || (screen == SCREEN_NIGHT_WALL
                && previous != SCREEN_NIGHT_WALL_FALL
                && previous != SCREEN_NIGHT_WALL_HELP
                && previous != SCREEN_NIGHT_WALL_SLIP_ALONE
                && previous != SCREEN_SEWER_STUCK
                && previous != SCREEN_NIGHT_WALL_UNANSWERED))
        fWallFailed = false;
    // the banks are cold to a party with a bad local reputation
    if (screen == SCREEN_FUGGER && _Reputation() < 0)
        screen = SCREEN_FUGGER_COLD;
    else if (screen == SCREEN_MEDICI && _Reputation() < 0)
        screen = SCREEN_MEDICI_COLD;
    fScreen = screen;
    if (screen == SCREEN_INN)
        fVariables["Money1"] = MoneyText(InnPrice());
    else if (fParty != NULL && screen == SCREEN_CHURCH)	// the donation
        fVariables["Money1"] = MoneyText(TotalPfennigs(fParty->cash) / 10);
    else if (fParty != NULL && (screen == SCREEN_FUGGER
            || screen == SCREEN_MEDICI || screen == SCREEN_FUGGER_COLD
            || screen == SCREEN_MEDICI_COLD))	// the letter of credit
        fVariables["Money1"] = MoneyText(uint32(fParty->bankNotes) * 240);
    const screen_rules& rules = RulesFor(screen, fNight);
    if (rules.deck == NULL) {
        fView.SetCard(fNotImplementedCard, fVariables);
        fView.SetScene("");
        return;
    }
    // the city lord's cards are the same in the town hall's deck
    const char* deck = rules.deck;
    if (fLordHall && screen >= SCREEN_LORD_NOT_TODAY
            && screen <= SCREEN_LORD_SAINT_VAIN)
        deck = "COUNC00";
    if (fBanditsSoldiers && screen >= SCREEN_BANDITS_WARNING
            && screen <= SCREEN_BANDITS_CHARGE)
        deck = "MEETB02";
    if (fToll && screen >= SCREEN_TITHE_MEET && screen <= SCREEN_TITHE_POOR)
        deck = "MEETH02";
    fView.SetCard(fData.Messages(deck).CardAt(uint32(rules.card)),
        fVariables, _HiddenOptions(screen));
    if ((screen == SCREEN_FUGGER_DEPOSIT || screen == SCREEN_MEDICI_DEPOSIT)
            && fParty != NULL) {
        // DARKLAND.EXE: the purse's florins to start with, 10 digits
        fView.SetPrompt("Deposit how many Florins?",
            std::to_string(fParty->cash.florins), 10);
    }
    fView.SetScene(rules.scene != NULL ? rules.scene : "", withScene);
}


std::vector<int>
CityVisit::_HiddenOptions(int screen) const
{
    const city& c = _City();
    std::vector<int> hidden;
    const screen_rules& rules = RulesFor(screen, fNight);
    for (int i = 0; i < kMaxOptions; i++) {
        const option_rule& rule = RuleFor(rules, i);
        bool hide = rule.action == ACTION_HIDE;
        if (rule.needs == kNeedsDonation)
            hide = fParty == NULL || TotalPfennigs(fParty->cash) / 10 < 10;
        else if (rule.needs == kNeedsBadReputation)
            hide = _Reputation() > -10 && !_Wanted();
        else if (rule.needs == kNeedsByDay)
            hide = fClock == NULL || !IsGameDay(*fClock);
        else if (rule.needs == kNeedsByNight)
            hide = fClock != NULL && IsGameDay(*fClock);
        else if (rule.needs == kNeedsLordAudience || rule.needs == kNeedsLordClerk
                || rule.needs == kNeedsLordSaint) {
            // the fortress: not while the city's flag 2 is set (dim in the
            // game); the town hall: not to a wanted party (and for the
            // clerk, not with an appointment made)
            const bool hall = screen == SCREEN_TOWN_HALL;
            hide = hall ? _Wanted() : (c.flags & 2) != 0;
            if (hall && rule.needs == kNeedsLordClerk && _Marked(kMarkAppointment))
                hide = true;
            if (rule.needs == kNeedsLordSaint && !_SaintKnown(screen))
                hide = true;
        } else if (rule.needs == kNeedsCathedralGift) {
            hide = fParty == NULL || TotalPfennigs(fParty->cash) < 7200;
        } else if (rule.needs == kNeedsRelic) {
            hide = !_FindRelic(NULL, NULL);
        } else if (rule.needs == kNeedsPartyRoom) {
            hide = fParty == NULL || fParty->members.size() >= 4;
        } else if (rule.needs == kNeedsPartyRetire) {
            hide = fParty == NULL || fParty->members.size() < 2
                || size_t(rule.target) >= fParty->members.size();
        } else if (rule.needs == kNeedsSwimMounted)
            hide = !_HasHorses();
        else if (rule.needs == kNeedsSwimOnFoot)
            hide = _HasHorses();
        else if (rule.needs == kNeedsInnPrice)
            hide = fParty == NULL || TotalPfennigs(fParty->cash) < InnPrice();
        else if (rule.needs == kNeedsCache)
            hide = fCaches.find(fCity) == fCaches.end();
        else if (rule.needs == kNeedsBankNotes)
            hide = fParty == NULL || fParty->bankNotes == 0;
        else if (rule.needs == kNeedsFlorins)
            hide = fParty == NULL || fParty->cash.florins < 2;
        else if (rule.needs == kNeedsPhysician) {
            // DARKLAND.EXE, file 0xA433D: in towns of size 3 or less, by
            // the city's number and the year
            // (a signed 16-bit remainder)
            hide = c.size <= 3 && int16(_PeopleSeed() + (fClock != NULL
                ? fClock->Year() : 1400)) % 3 == 0;
        } else if (rule.needs == kNeedsAlchemist) {
            // file 0xA4368: in towns of size 3 or less where the city's
            // property 0x21 is even, of size 4 where it divides by 3
            const int number = int16(_PeopleSeed());
            hide = (c.size <= 3 && number % 2 == 0)
                || (c.size == 4 && number % 3 == 0);
        } else if (rule.needs == kNeedsSneak) {
            hide = _Marked(kMarkSneakFailed);
        } else if (rule.needs == kNeedsBribe) {
            hide = _Marked(kMarkBribeRefused) || fParty == NULL
                || TotalPfennigs(fParty->cash) < _Bribe();
        } else if (rule.needs == kNeedsFine) {
            hide = fParty == NULL || TotalPfennigs(fParty->cash) < _Fine();
        } else if (rule.needs == kNeedsToll) {
            hide = fParty == NULL || TotalPfennigs(fParty->cash) < _Toll();
        } else if (rule.needs == kNeedsCharm) {
            hide = _Marked(kMarkCharmFailed) || _Marked(kMarkWanted);
        } else if (rule.needs == kNeedsSlip) {
            hide = _Marked(kMarkSlipFailed);
        } else if (rule.needs == kNeedsHail) {
            hide = _Marked(kMarkHailFailed);
        } else if (rule.needs == kNeedsTalk) {
            hide = _Marked(kMarkTalkFailed);
        } else if (rule.needs == kNeedsWallBribe) {
            hide = fParty == NULL
                || TotalPfennigs(fParty->cash) < _WallBribe();
        } else if (rule.needs == kNeedsRope || rule.needs == kNeedsNoRope
                || rule.needs == kNeedsClimb) {
            // 0E76:0C76(-2, 0x3B): a rope (item 59) in the party
            bool rope = false;
            if (fParty != NULL) {
                for (const character& member : fParty->members) {
                    for (const item& carried : member.items)
                        rope = rope || (carried.code & 0x0FFF) == kRopeCode;
                }
            }
            const bool day = fScreen == SCREEN_DAY_WALL;
            hide = fWallFailed || (day && _Marked(kMarkWallAlert));
            if (rule.needs == kNeedsRope)
                hide = hide || !rope;
            else if (rule.needs == kNeedsNoRope)
                hide = hide || rope;
        } else if (rule.needs == kNeedsGrate) {
            hide = _Marked(kMarkGrateFailed);
        } else if (rule.needs == kNeedsMeetMoney) {
            hide = fParty == NULL || TotalPfennigs(fParty->cash) < fMeetMoney
                || fMeetMoney == 0;
        } else if (rule.needs == kNeedsMounts) {
            hide = !_AllMounted();
        } else if (rule.needs == kNeedsPlea) {
            hide = fPleaFailed;
        } else if (rule.needs == kNeedsFreshSaint) {
            hide = fPrayerFailed || !_SaintKnown(screen);
        } else if (rule.needs == kNeedsCampIgnore) {
            hide = fCampIgnored;
        } else if (rule.needs == kNeedsBogOption) {
            hide = fBogFailed[rule.target];
        } else if (rule.needs == kNeedsRaft) {
            hide = !fFloodRope;
        } else if (rule.needs == kNeedsMemberHere) {
            hide = fParty == NULL || size_t(rule.target) >= fParty->members.size();
        } else if (rule.needs == kNeedsSaint) {
            hide = !_SaintKnown(screen);
        } else if (rule.needs == kNeedsTowerWelcome) {
            hide = _Marked(kMarkTowerAsked);
        } else if (rule.needs == kNeedsMonksPrayers) {
            hide = _Marked(kMarkMonksPrayed) || fParty == NULL
                || TotalPfennigs(fParty->cash) < _MonksPrice();
        } else if (rule.needs == kNeedsTutoring) {
            hide = _HasTutors() || _Marked(kMarkMonksNoTutors);
        } else if (rule.needs == kNeedsLibrary) {
            hide = _Marked(kMarkLibrary);
        } else if (rule.needs == kNeedsAbbess) {
            hide = _Marked(kMarkAbbess);
        } else if (rule.needs == kNeedsFuggerReward
                || rule.needs == kNeedsMediciReward) {
            hide = !_RewardDue(3, rule.needs == kNeedsFuggerReward ? 8 : 6);
        } else if (rule.needs == kNeedsGroschen) {
            hide = fParty == NULL || TotalPfennigs(fParty->cash) <= 12;
        } else if (rule.needs == kNeedsTowerAllies) {
            hide = !_EventHere(3, 5) && !_EventHere(3, 0x27);
        } else if (rule.needs == kNeedsFuggerTasks
                || rule.needs == kNeedsMediciTasks) {
            hide = _PatronBusy(rule.needs == kNeedsFuggerTasks ? 8 : 6);
        } else if (rule.needs == kNeedsUnrestHere) {
            hide = !_EventHere(2);
        } else if (rule.needs == kNeedsRebelsHere) {
            hide = !_EventHere(2, 0);
        } else if (rule.needs == kNeedsJobRumor) {
            hide = int16(_PeopleSeed()) % 20 != 0 || _Marked(0x65);
        } else if (rule.needs == kNeedsInnerWall) {
            // file 0xBD992: the grate tried (mark 0x10), the sally port's
            // alarm (0x22), the walls guarded (0x0F), a rope (item 59),
            // horses: the options without them or those abandoning them
            // (0E76:0DD8(-2, 0x2000)); the purse short of the bribe takes
            // away the second sewer option, as the game has it
            bool rope = false;
            if (fParty != NULL) {
                for (const character& member : fParty->members) {
                    for (const item& carried : member.items)
                        rope = rope || (carried.code & 0x0FFF) == kRopeCode;
                }
            }
            const bool horses = _HasHorses();
            const bool guarded = _Marked(kMarkWallAlert);
            switch (i) {
                case 0:
                    hide = horses || _Marked(kMarkGrateFailed);
                    break;
                case 1:
                    hide = !horses || _Marked(kMarkGrateFailed) || fParty == NULL
                        || TotalPfennigs(fParty->cash) < _InnerWallBribe();
                    break;
                case 2:
                    hide = _Marked(kMarkSallyAlarm) || guarded;
                    break;
                case 3:
                    hide = horses || !rope || guarded;
                    break;
                case 4:
                    hide = !horses || !rope || guarded;
                    break;
                case 5:
                    hide = horses || guarded;
                    break;
                default:
                    hide = !horses;
                    break;
            }
        } else if (rule.needs == kNeedsLockpicks) {
            bool lockpicks = false;
            if (fParty != NULL) {
                for (const character& member : fParty->members) {
                    for (const item& carried : member.items) {
                        lockpicks = lockpicks
                            || (carried.code & 0x0FFF) == kLockpickCode;
                    }
                }
            }
            hide = !lockpicks;
        } else if (rule.needs == kNeedsWoman) {
            hide = _Seductress() < 0;
        } else if (rule.needs == kNeedsGuardsBribe) {
            hide = fParty == NULL
                || TotalPfennigs(fParty->cash) < _ChallengeBribe()
                || fChallengeReturn != SCREEN_SIDE_STREET;
        } else if (rule.needs == kNeedsNightBribe) {
            hide = fParty == NULL
                || TotalPfennigs(fParty->cash) < _NightBribe();
        } else if (rule.needs == kNeedsStone) {
            hide = fStoneOffered || fParty == NULL
                || TotalPfennigs(fParty->cash) < _StonePrice();
        } else if (rule.needs == kNeedsMaster)
            hide = _AlchemistSkill(true) < 25;
        else if (rule.needs == kNeedsWounded)
            hide = _Wounded() == 0;
        else if (rule.needs == kNeedsTreatment) {
            const std::map<int, uint32>::const_iterator treated
                = fTreatedUntil.find(fCity);
            hide = !fTreatmentOffered || (fClock != NULL
                && treated != fTreatedUntil.end()
                && fClock->HourStamp() < treated->second);
        } else if (rule.needs == kNeedsPawnshop)
            hide = (c.flags & CITY_HAS_PAWNSHOP) == 0;
        else if (rule.needs >= kNeedsShop)
            hide = c.shopQuality[rule.needs - kNeedsShop] == 0;
        else if (rule.needs == kNeedsHarbor)
            hide = c.harbor == CITY_HARBOR_NONE;
        else if (rule.needs != kAlways)
            hide = c.places[rule.needs].empty();
        if (hide)
            hidden.push_back(i);
    }
    return hidden;
}


// Arriving at a place (file 0x5E6C3): its location record's state
// (+0x0C). The castles' is the robber knight's tower (0x93): named after
// the castle (1367:0DB4(place + 1100, 0, 0)), his fort (card 24) where
// his men (0E76:360C(3, 0x27, place)) have built one. Other places are
// not implemented: back to the map.
int
CityVisit::_EnterPlace(int place)
{
    uint16 state = fData.Locations().LocationAt(uint32(place)).enterState;
    if (fEnterStates != NULL && uint32(place) < fEnterStates->size())
        state = (*fEnterStates)[size_t(place)];
    if (state == 0x93) {
        fVariables["NamedOneName"] = _PersonName(uint16(place + 1100));
        return _TowerScreen();
    }
    fPreviousScreen = -1;
    return SCREEN_NOT_IMPLEMENTED;
}


bool
CityVisit::_InCity() const
{
    return fCity >= 0 && uint32(fCity) < fData.Cities().CountCities();
}


const city&
CityVisit::_City() const
{
    static city sNoCity;
    if (_InCity())
        return fData.Cities().CityAt(uint32(fCity));
    sNoCity = city();
    sNoCity.size = 1;
    return sNoCity;
}


// 1367:0DB4 adds the seed global to `seed`
std::string
CityVisit::_PersonName(uint16 seed)
{
    if (fNames == NULL)
        fNames.reset(new ExeData(fData.PathFor("DARKLAND.EXE")));
    return fNames->MaleName(uint16(fSeed + seed));
}


uint16
CityVisit::_PeopleSeed() const
{
    return uint16(_City().peopleSeed + fSeed);
}


int
CityVisit::_Reputation() const
{
    if (fReputations == NULL || fCity < 0 || fCity >= int(fReputations->size()))
        return 0;
    return (*fReputations)[fCity];
}


int
CityVisit::_BestSkill(int skill) const
{
    int best = 0;
    if (fParty != NULL) {
        for (const character& member : fParty->members)
            best = std::max(best, int(member.skills[skill]));
    }
    return best;
}


bool
CityVisit::_Marked(int kind) const
{
    const std::map<std::pair<int, int>, uint32>::const_iterator mark
        = fMarks.find(std::make_pair(kind, fCity));
    return mark != fMarks.end() && fClock != NULL
        && fClock->HourStamp() < mark->second;
}


// 0E76:2930 makes a mark for `hours`; 0E76:2A32 (extend) adds them to a
// mark still running
void
CityVisit::_Mark(int kind, uint32 hours, bool extend)
{
    if (fClock == NULL)
        return;
    uint32& until = fMarks[std::make_pair(kind, fCity)];
    const uint32 now = fClock->HourStamp();
    until = (extend && until > now ? until : now) + hours;
}


// The member who falls behind (0E76:0656): the lowest speed, the Agility
// lowered by the load carried (the first of them)
int
CityVisit::_Slowest() const
{
    int slowest = 0;
    int lowest = 0x7FFF;
    for (size_t i = 0; fParty != NULL && i < fParty->members.size(); i++) {
        const int speed = MemberSpeed(fParty->members[i]);
        if (speed < lowest) {
            lowest = speed;
            slowest = int(i);
        }
    }
    return slowest;
}


// A change of the local reputation by low..high (0E76:19D0), taken with
// a chance of 100 - |reputation| %
void
CityVisit::_ChangeReputation(int low, int high)
{
    if (fReputations == NULL || fCity < 0 || fCity >= int(fReputations->size()))
        return;
    int16& reputation = (*fReputations)[fCity];
    if (int(fRandom() % 100) > 100 - std::abs(int(reputation)))
        return;
    const int change = low + (high > low ? int(fRandom() % uint32(high - low + 1))
        : 0);
    reputation = int16(std::max(-99, std::min(99, reputation + change)));
}


// $ChosenOneName and the pronouns that go with it ($he, $his...)
void
CityVisit::_SetChosen(int member)
{
    if (fParty == NULL || member < 0 || member >= int(fParty->members.size()))
        return;
    const character& chosen = fParty->members[member];
    fVariables["ChosenOneName"] = chosen.shortName;
    const bool female = chosen.female;
    fVariables["he"] = female ? "she" : "he";
    fVariables["He"] = female ? "She" : "He";
    fVariables["his"] = female ? "her" : "his";
    fVariables["His"] = female ? "Her" : "His";
    fVariables["him"] = female ? "her" : "him";
    fVariables["himself"] = female ? "herself" : "himself";
}


bool
CityVisit::CampSafe() const
{
    return _Marked(kMarkCampSafe);
}

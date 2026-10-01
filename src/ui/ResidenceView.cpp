#include "ResidenceView.h"

#include "Bitmap.h"
#include "Character.h"
#include "CityFile.h"
#include "ExeData.h"
#include "FileStream.h"
#include "GameData.h"
#include "GameTime.h"
#include "InfoView.h"
#include "MenuBar.h"
#include "PICImage.h"
#include "Palette.h"
#include "PartySidebar.h"
#include "ScreenSupport.h"
#include "TextSupport.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>

static const uint32 kFontIndex		= 2;	// FONTS.FNT, as the cards

// Layout and colors, from DARKLAND.EXE (file 0x6F97C...): EGA colors on
// the picture's own palette
static const int kMessageLeft		= 80;	// the day's price
static const int kMessageTop		= 18;
static const int kMessageWidth		= 220;
static const int kMembersTop		= 48;	// "Gretch..." and what he does
static const int kMemberHeight		= 18;
static const int kNameLeft			= 73;
static const int kActivityLeft		= 120;
static const int kActivityWidth		= 80;
static const int kMenuLeft			= 208;	// the activities
static const int kMenuTop			= 48;
static const int kMenuHeight		= 9;
static const int kDecideLeft		= 75;	// "You decide to..."
static const int kDecideTop			= 164;
static const int kOptionsLeft		= 95;
static const int kSpendTop			= 172;
static const int kLeaveTop			= 181;
static const int kLineHeight		= 8;
static const int kTutorsLeft		= 246;	// the teachers (file 0x6F27A)
static const int kTutorsWidth		= 72;

static const uint8 kMessageColor	= 15;	// white
static const uint8 kMemberColor		= 14;	// yellow
static const uint8 kSelectedColor	= 11;	// light cyan
static const uint8 kAvailableColor	= 10;	// light green (in a city)
static const uint8 kUnavailableColor = 2;	// green
static const uint8 kCrimsonColor	= 4;	// the letter to type

// The menu (DS:3D69...) and its letters
static const char* kMenu[ResidenceView::ACTIVITY_COUNT] = {
    "Just Relax", "Regain Strength", "Pray for divine favor", "Alchemy work",
    "Earn a little money", "Guard the camp", "Train or study"
};
// The skills' names (290E:2137)
static const char* kSkillNames[kSkillCount] = {
    "Edged Wpns", "Impact Wpns", "Flail Wpns", "Polearm Wpns",
    "Thrown Wpns", "Bow Weapons", "Missile Device", "Alchemy",
    "Religious Trng", "Virtue", "Speak Common", "Speak Latin",
    "Read & Write", "Healing", "Artifice", "Stealth", "Streetwise",
    "Riding", "Woodwise"
};
static const char kMenuKeys[ResidenceView::ACTIVITY_COUNT] = {
    'j', 'r', 'p', 'a', 'e', 'g', 't'
};


// DARKLAND.EXE 0x47C:11AB (an integer square root) as the pay uses it
// (file 0x70BDC): 1 under 4, else the root within 1..9
static int
SkillPay(int value)
{
    if (value < 4)
        return 1;
    int root = 0;
    while ((root + 1) * (root + 1) <= value)
        root++;
    return std::max(1, std::min(root, 9));
}


ResidenceView::ResidenceView(GameData& data)
    :
    fData(data),
    fBuffer(NULL),
    fRandom(std::random_device{}()),
    fParty(NULL),
    fClock(NULL),
    fInfo(NULL),
    fMenu(NULL),
    fCity(-1),
    fReputation(0),
    fInnPrice(0),
    fTutor(0),
    fTutorList(false),
    fMember(0),
    fDays(0),
    fAmbushSafe(-1),
    fCamp(false),
    fCampBase(0),
    fDanger(0),
    fGuard(-1),
    fInterrupted(false),
    fMouse(0, 0),
    fCursorVisible(false)
{
    fFont.reset(new Font(fData.Fonts(), kFontIndex));
    fSidebar.reset(new PartySidebar(fData));
    fExe.reset(new ExeData(fData.PathFor("DARKLAND.EXE")));

    fPalette = PICImage::EGAPalette();
    FileStream stream(fData.PathFor("CAMPCITY.PIC").c_str(),
        FileStream::READ_ONLY);
    PICImage image(&stream);
    image.ApplyPalette(fPalette);
    fBackground.width = image.Width();
    fBackground.height = image.Height();
    fBackground.pixels = image.RawBytes();

    FileStream wild(fData.PathFor("CAMPWILD.PIC").c_str(),
        FileStream::READ_ONLY);
    PICImage wildImage(&wild);
    GFX::Palette wildPalette = PICImage::EGAPalette();
    wildImage.ApplyPalette(wildPalette);
    fWildPalette = wildPalette;
    fWildBackground.width = wildImage.Width();
    fWildBackground.height = wildImage.Height();
    fWildBackground.pixels = wildImage.RawBytes();

    fBuffer = new Bitmap(320, 200, 8);
}


ResidenceView::~ResidenceView()
{
    if (fBuffer != NULL)
        fBuffer->Release();
}


void
ResidenceView::SetParty(party* members)
{
    fParty = members;
    fSidebar->SetParty(members);
}


void
ResidenceView::SetPlace(int cityIndex, int reputation, uint32 innPrice,
    const std::vector<city_tutor>& tutors)
{
    fCity = cityIndex;
    fReputation = reputation;
    fInnPrice = innPrice;
    fTutors = tutors;
    fTutor = 0;
    fTutorList = false;
    const size_t count = fParty != NULL ? fParty->members.size() : 0;
    fActivities.assign(count, ACTIVITY_RELAX);
    fTutorOf.assign(count, 0);
    fValues.assign(count, 0);
    fJobs.assign(count, std::string());
    fMember = 0;
    fDays = 0;
    fMessage.clear();
    fAmbushSafe = -1;
    fCamp = false;
    fDanger = 0;
    fGuard = -1;
    fInterrupted = false;
    _UpdateValues();
}


void
ResidenceView::SetCamp(int base)
{
    SetPlace(-1, 0, 0);
    fCamp = true;
    fCampBase = base;
}


// File 0x70C10: the camp's base danger a day, less for each member
// guarding: 3, 2 or 1 by Woodwise (70, 30), 2 or 1 by Stealth (70, 30),
// and 1 if a weapon skill but Impact is 30 or more; kept within 1..15
int
ResidenceView::GuardedDanger() const
{
    int danger = fCampBase;
    if (fParty != NULL) {
        for (size_t i = 0; i < fParty->members.size(); i++) {
            if (fActivities[i] != ACTIVITY_GUARD)
                continue;
            const character& member = fParty->members[i];
            const int woods = member.skills[18];
            const int stealth = member.skills[kSkillStealth];
            danger -= woods >= 70 ? 3 : woods >= 30 ? 2 : 1;
            danger -= stealth >= 70 ? 2 : stealth >= 30 ? 1 : 0;
            for (int skill = 0; skill < kWeaponSkillCount; skill++) {
                if (skill != 1 && member.skills[skill] >= 30) {
                    danger--;
                    break;
                }
            }
        }
    }
    return std::max(1, std::min(15, danger));
}


void
ResidenceView::Run(GameWindow& window)
{
    const MenuLimits limits(fMenu, { MENU_SAVE_GAME, MENU_LOAD_GAME,
        MENU_MARCHING_ORDER });
    bool dirty = true;
    for (;;) {
        if (dirty) {
            window.Show(Draw());
            dirty = false;
        }
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 100) == 0)
            continue;
        menu_command command;
        if (fMenu != NULL && fMenu->Handle(window, event,
                [this]() { return Draw(); }, true, command)) {
            if (command == MENU_QUIT) {
                MenuBar::PostQuit();	// for the screen below, which quits
                return;
            }
            if (command == MENU_PARTY_INFO && fInfo != NULL)
                fInfo->Run(window, InfoView::kPartyPage);
            else if (command == MENU_PAUSE)
                MenuBar::Pause(window);
            dirty = true;
            continue;
        }
        switch (event.type) {
            case SDL_QUIT:
                SDL_PushEvent(&event);	// for the screen below, which quits
                return;
            case SDL_KEYDOWN: {
                const SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE || key == SDLK_l)
                    return;
                fMessage.clear();
                const bool ctrl = (event.key.keysym.mod & KMOD_CTRL) != 0;
                if (ctrl && key >= SDLK_F1 && key <= SDLK_F5)
                    SetLeader(int(key - SDLK_F1));
                else if (key >= SDLK_F1 && key <= SDLK_F5 && fInfo != NULL)
                    fInfo->Run(window, int(key - SDLK_F1));
                else if (key == SDLK_F6 && fInfo != NULL)
                    fInfo->Run(window, InfoView::kPartyPage);
                if (key >= SDLK_1 && key <= SDLK_5)
                    SelectMember(int(key - SDLK_1));
                else if (key == SDLK_s) {
                    if (!SpendDay() && fInterrupted) {
                        if (fCamp)
                            _WaitForKey(window);
                        return;
                    }
                }
                else {
                    for (int i = 0; i < ACTIVITY_COUNT; i++) {
                        if (key == SDL_Keycode(kMenuKeys[i]))
                            Choose(activity(i));
                    }
                }
                dirty = true;
                break;
            }
            case SDL_MOUSEMOTION:
                MouseMoved(GameWindow::ToScreen(event.motion.x, event.motion.y));
                dirty = true;
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    const GFX::point point = GameWindow::ToScreen(
                        event.button.x, event.button.y);
                    const int member = fSidebar->MemberAt(point);
                    if (member >= 0 && fInfo != NULL)
                        fInfo->Run(window, member);
                    else if (!Clicked(point)) {
                        if (fCamp && fInterrupted)
                            _WaitForKey(window);
                        return;
                    }
                    dirty = true;
                }
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_LEAVE)
                    fCursorVisible = false;
                dirty = true;
                break;
            default:
                break;
        }
    }
}


// The camp was found: the message stays until a key or a click
void
ResidenceView::_WaitForKey(GameWindow& window)
{
    window.Show(Draw());
    for (;;) {
        SDL_Event event;
        if (SDL_WaitEvent(&event) == 0)
            return;
        if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONUP) {
            return;
        } else if (event.type == SDL_QUIT) {
            SDL_PushEvent(&event);
            return;
        } else if (event.type == SDL_WINDOWEVENT) {
            window.Show(Draw());
        }
    }
}


void
ResidenceView::SelectMember(int member)
{
    if (fParty != NULL && member >= 0 && member < int(fParty->members.size()))
        fMember = member;
}


void
ResidenceView::SetLeader(int member)
{
    if (fParty != NULL && member >= 0 && member < int(fParty->members.size()))
        fParty->leader = member;
}


// What the game lets a member take up (file 0x6FB30...): regaining
// strength when it is under its maximum, praying when divine favor is,
// earning money in a city, training when the city has a teacher; the
// alchemy work (potions) is not implemented, guarding is for camps
bool
ResidenceView::Available(activity what) const
{
    if (fParty == NULL || fMember >= int(fParty->members.size()))
        return false;
    const character& member = fParty->members[fMember];
    switch (what) {
        case ACTIVITY_RELAX:
            return true;
        case ACTIVITY_REGAIN_STRENGTH:
            return member.attributes[ATTRIBUTE_STRENGTH]
                < member.maxAttributes[ATTRIBUTE_STRENGTH];
        case ACTIVITY_PRAY:
            return member.attributes[ATTRIBUTE_DIVINE_FAVOR]
                < member.maxAttributes[ATTRIBUTE_DIVINE_FAVOR];
        case ACTIVITY_EARN:
            return fCity >= 0;
        case ACTIVITY_GUARD:
            return fCamp;
        case ACTIVITY_TRAIN:
            for (size_t i = 0; i < fTutors.size(); i++) {
                if (_TutorAvailable(int(i)))
                    return true;
            }
            return false;
        default:
            return false;
    }
}


bool
ResidenceView::Choose(activity what)
{
    if (!Available(what))
        return false;
    fActivities[fMember] = what;
    if (what == ACTIVITY_EARN)
        _FindJob(fMember);
    if (what == ACTIVITY_TRAIN) {
        // the teacher chosen, else the first one still here
        int tutor = fTutor;
        for (size_t i = 0; !_TutorAvailable(tutor) && i < fTutors.size(); i++)
            tutor = int(i);
        fTutorOf[fMember] = tutor;
    }
    _UpdateValues();
    return true;
}


ResidenceView::activity
ResidenceView::ActivityOf(int member) const
{
    return member >= 0 && member < int(fActivities.size())
        ? fActivities[member] : ACTIVITY_RELAX;
}


int
ResidenceView::ValueOf(int member) const
{
    return member >= 0 && member < int(fValues.size()) ? fValues[member] : 0;
}


const std::string&
ResidenceView::JobOf(int member) const
{
    static const std::string kNone;
    return member >= 0 && member < int(fJobs.size()) ? fJobs[member] : kNone;
}


// File 0x708E8: the inn's price, less what the workers earn, plus the
// teacher's fee for each student
int32
ResidenceView::NetCost() const
{
    int32 cost = int32(fInnPrice);
    for (size_t i = 0; i < fActivities.size(); i++) {
        if (fActivities[i] == ACTIVITY_EARN)
            cost -= fValues[i];
        else if (fActivities[i] == ACTIVITY_TRAIN
                && _TutorAvailable(fTutorOf[i]))
            cost += int32(fTutors[size_t(fTutorOf[i])].fee);
    }
    return cost;
}


// File 0x7050A: each member's day, then the inn is paid (if the purse
// can), then the time until 5 in the morning passes (at least 9 hours).
// In the slum the thieves may come first (file 0x6FDBE).
bool
ResidenceView::SpendDay()
{
    if (fParty == NULL || fInterrupted)
        return false;
    if (fAmbushSafe >= 0 && _Random(100) >= fAmbushSafe) {
        fInterrupted = true;
        return false;
    }
    // file 0x6FDFC: in the wilderness the camp may be found first, by
    // chance against the danger so far; the guard is the first member
    // on guard
    if (fCamp && _Random(50) + _Random(50) <= fDanger) {
        fGuard = -1;
        for (size_t i = 0; i < fActivities.size() && fGuard < 0; i++) {
            if (fActivities[i] == ACTIVITY_GUARD)
                fGuard = int(i);
        }
        fInterrupted = true;
        // the encounter is the soldiers' or the bandits' (file 0x5E1E5,
        // random(2)); the texts are the first cards of $CampJ00 and
        // $CampB00 (the encounters themselves are not reproduced)
        const bool soldiers = _Random(2) == 0;
        if (fGuard >= 0) {
            fMessage = fParty->members[size_t(fGuard)].shortName
                + (soldiers ? " spots a party of soldiers headed toward the "
                    "camp. Alas, someone has discovered your presence here."
                : " spots a party of bandits headed your way. Alas, someone "
                    "has discovered your presence here.");
        } else {
            fMessage = soldiers ? "You are surprised by a party of soldiers, "
                "led by a stern woodsman. \"Leave or pay rent,\" he says."
                : "Suddenly wild screams erupt around you! Dirty bandits "
                "leap forward from all directions. No one guarded the camp!";
        }
        return false;
    }
    const int32 cost = NetCost();
    if (cost > 0 && int64(TotalPfennigs(fParty->cash)) < cost) {
        fMessage = "Not Enough Money";
        return false;
    }
    const std::function<int(int)> random = [this](int n) { return _Random(n); };
    const int bestHealing = _BestHealing();
    for (size_t i = 0; i < fParty->members.size(); i++) {
        character& member = fParty->members[i];
        uint8* current = member.attributes;
        const uint8* maximum = member.maxAttributes;
        switch (fActivities[i]) {
            case ACTIVITY_REGAIN_STRENGTH:
                current[ATTRIBUTE_STRENGTH] = uint8(std::min(
                    current[ATTRIBUTE_STRENGTH]
                        + std::max(1, std::min(bestHealing / 15, 99)),
                    int(maximum[ATTRIBUTE_STRENGTH])));
                if (current[ATTRIBUTE_STRENGTH] >= maximum[ATTRIBUTE_STRENGTH])
                    fActivities[i] = ACTIVITY_RELAX;
                current[ATTRIBUTE_ENDURANCE] = maximum[ATTRIBUTE_ENDURANCE];
                break;
            case ACTIVITY_RELAX:
                current[ATTRIBUTE_ENDURANCE] = maximum[ATTRIBUTE_ENDURANCE];
                break;
            case ACTIVITY_PRAY:
                current[ATTRIBUTE_DIVINE_FAVOR] = uint8(std::min(
                    current[ATTRIBUTE_DIVINE_FAVOR] + fValues[i],
                    int(maximum[ATTRIBUTE_DIVINE_FAVOR])));
                if (current[ATTRIBUTE_DIVINE_FAVOR]
                        >= maximum[ATTRIBUTE_DIVINE_FAVOR])
                    fActivities[i] = ACTIVITY_RELAX;
                break;
            case ACTIVITY_EARN:
                fParty->cash = MoneyFromPfennigs(TotalPfennigs(fParty->cash)
                    + uint32(fValues[i]));
                break;
            case ACTIVITY_TRAIN:
                // the game trains even when it cannot pay the fee
                if (_TutorAvailable(fTutorOf[i])) {
                    const city_tutor& tutor = fTutors[size_t(fTutorOf[i])];
                    const uint32 purse = TotalPfennigs(fParty->cash);
                    if (purse >= tutor.fee)
                        fParty->cash = MoneyFromPfennigs(purse - tutor.fee);
                    TrainSkill(member, tutor.skill, tutor.level * 20 / 100,
                        random);
                }
                break;
            case ACTIVITY_GUARD:
                break;
            default:
                fActivities[i] = ACTIVITY_RELAX;
                break;
        }
    }
    const uint32 purse = TotalPfennigs(fParty->cash);
    if (purse >= fInnPrice)
        fParty->cash = MoneyFromPfennigs(purse - fInnPrice);
    if (fClock != NULL) {
        // 1367:086A(5): the hours until 5, through midnight
        int hours = 5 - fClock->Hour();
        if (hours < 0)
            hours += 24;
        if (hours < 9)
            hours += 24;
        fClock->AddHours(uint32(hours));
        for (size_t i = 0; i < fActivities.size(); i++) {
            if (fActivities[i] == ACTIVITY_TRAIN
                    && !_TutorAvailable(fTutorOf[i]))
                fActivities[i] = ACTIVITY_RELAX;
        }
    }
    if (fCamp)
        fDanger += GuardedDanger();
    fDays++;
    _UpdateValues();
    return true;
}


void
ResidenceView::MouseMoved(const GFX::point& point)
{
    fMouse = point;
    fCursorVisible = point.x >= 0 && point.y >= 0 && point.x < 320
        && point.y < 200;
    // the teachers' list opens on "Train or study" and stays while the
    // mouse is on it
    const int menu = _MenuAt(point);
    if (menu == ACTIVITY_TRAIN && !fTutors.empty())
        fTutorList = true;
    else if (_TutorAt(point) < 0 && menu >= 0)
        fTutorList = false;
}


void
ResidenceView::SelectTutor(int tutor)
{
    if (tutor >= 0 && tutor < int(fTutors.size()))
        fTutor = tutor;
}


int
ResidenceView::TutorOf(int member) const
{
    return member >= 0 && member < int(fTutorOf.size()) ? fTutorOf[member] : 0;
}


// A line of the teachers' list (file 0x6F27A: x 245..317, from y 48, 9
// high), or -1
int
ResidenceView::_TutorAt(const GFX::point& point) const
{
    if (!fTutorList || point.x < kTutorsLeft - 1
            || point.x >= kTutorsLeft + kTutorsWidth || point.y < kMenuTop)
        return -1;
    const int row = (point.y - kMenuTop) / kMenuHeight;
    return row < int(fTutors.size()) ? row : -1;
}


// Still teaching (its offer lasts 168 hours)
bool
ResidenceView::_TutorAvailable(int tutor) const
{
    return tutor >= 0 && tutor < int(fTutors.size())
        && (fClock == NULL || fClock->HourStamp() < fTutors[size_t(tutor)].until);
}


bool
ResidenceView::Clicked(const GFX::point& point)
{
    MouseMoved(point);
    fMessage.clear();
    const int boxMember = fSidebar->MemberAt(point);
    if (boxMember >= 0) {
        SelectMember(boxMember);
        return true;
    }
    if (point.x >= kNameLeft && point.x < kActivityLeft + kActivityWidth
            && point.y >= kMembersTop) {
        const int member = (point.y - kMembersTop) / kMemberHeight;
        if (fParty != NULL && member < int(fParty->members.size())) {
            SelectMember(member);
            return true;
        }
    }
    const int tutor = _TutorAt(point);
    if (tutor >= 0) {
        // file 0x6F365: the teacher chosen, the list closed
        SelectTutor(tutor);
        fTutorList = false;
        return true;
    }
    const int menu = _MenuAt(point);
    if (menu >= 0) {
        Choose(activity(menu));
        return true;
    }
    if (point.x >= kOptionsLeft && point.y >= kSpendTop - 1
            && point.y < kSpendTop + kLineHeight) {
        SpendDay();
        return !fInterrupted;
    }
    return !(point.x >= kOptionsLeft && point.y >= kLeaveTop - 1
        && point.y < kLeaveTop + kLineHeight);
}


Bitmap*
ResidenceView::Draw()
{
    const raw_picture& background = fCamp ? fWildBackground : fBackground;
    fBuffer->SetColors((fCamp ? fWildPalette : fPalette).colors, 0, 256);
    for (int y = 0; y < background.height; y++) {
        for (int x = 0; x < background.width; x++)
            fBuffer->PutPixel(x, y, background.pixels[size_t(y) * background.width + x]);
    }
    fSidebar->Draw(fBuffer, false);
    if (fParty == NULL || fParty->members.empty())
        return fBuffer;

    // the day's price and the purse after it (file 0x70160)
    char text[200];
    const int32 cost = NetCost();
    const uint32 purse = TotalPfennigs(fParty->cash);
    const int groschen = int(fInnPrice / 12);
    const int pfennigs = int(fInnPrice % 12);
    if (fCamp) {
        // file 0x70364
        snprintf(text, sizeof(text), "You live off the land, but the lord "
            "may be upset if you are caught. After %d days the risk is %d",
            fDays, fDanger / 5);
    } else if (cost > 0 && int64(purse) < cost) {
        snprintf(text, sizeof(text), "You consider staying here today, at "
            "%dgr, %dpf.  But, you do not have enough money.   For the day "
            "you will:", groschen, pfennigs);
    } else {
        const money left = MoneyFromPfennigs(uint32(int64(purse) - cost));
        snprintf(text, sizeof(text), "You consider staying here today, at "
            "%dgr, %dpf.  %s leave you with %dfl, %dgr, %dpf.   For the day "
            "you will:", groschen, pfennigs,
            cost > 0 ? "That will" : "With work that will", left.florins,
            left.groschen, left.pfennigs);
    }
    _DrawWrapped(fMessage.empty() ? text : fMessage, kMessageLeft,
        kMessageTop, kMessageWidth, kMessageColor);

    // "%s..." and what each does (file 0x703F8, the texts at DS:3A2C)
    for (size_t i = 0; i < fParty->members.size(); i++) {
        const int y = kMembersTop + int(i) * kMemberHeight;
        _DrawText(fParty->members[i].shortName + "...", kNameLeft, y,
            int(i) == fMember ? kSelectedColor : kMemberColor);
        switch (fActivities[i]) {
            case ACTIVITY_REGAIN_STRENGTH:
                snprintf(text, sizeof(text), "Regain strength (gain %1d)",
                    fValues[i]);
                break;
            case ACTIVITY_PRAY:
                snprintf(text, sizeof(text), "Pray for divine favor (gain %1d)",
                    fValues[i]);
                break;
            case ACTIVITY_EARN:
                snprintf(text, sizeof(text), "Earn %d pf/day as %s",
                    fValues[i], fJobs[i].c_str());
                break;
            case ACTIVITY_TRAIN:
                // the teacher's fee (file 0x6FC2F); the skill is ours
                if (_TutorAvailable(fTutorOf[i])) {
                    const city_tutor& tutor = fTutors[size_t(fTutorOf[i])];
                    snprintf(text, sizeof(text), "%s (%upfs)",
                        kSkillNames[tutor.skill], tutor.fee);
                } else
                    snprintf(text, sizeof(text), "Train or Study");
                break;
            case ACTIVITY_GUARD:
                snprintf(text, sizeof(text), "Maintain camp, food");
                break;
            default:
                snprintf(text, sizeof(text), "Relaxes");
                break;
        }
        _DrawWrapped(text, kActivityLeft, y, kActivityWidth, kMemberColor);
    }

    // the menu (file 0x7001C), in light green what makes sense
    for (int i = 0; i < ACTIVITY_COUNT; i++) {
        const int y = kMenuTop + i * kMenuHeight;
        const uint8 color = Available(activity(i)) ? kAvailableColor
            : kUnavailableColor;
        const std::string name = kMenu[i];
        _DrawText(name.substr(0, 1), kMenuLeft, y, Available(activity(i))
            ? kCrimsonColor : color);
        _DrawText(name.substr(1), kMenuLeft
            + fFont->StringWidth(name.substr(0, 1)) + 1, y, color);
    }

    // the teachers (file 0x70DB4), over the menu while its last line is
    // pointed at (file 0x6F520); the one chosen highlighted
    if (fTutorList) {
        for (size_t i = 0; i < fTutors.size(); i++) {
            const int y = kMenuTop + int(i) * kMenuHeight;
            fBuffer->FillRect(GFX::rect(kTutorsLeft, y - 1, kTutorsWidth,
                kMenuHeight), 0);
            _DrawText(kSkillNames[fTutors[i].skill], kTutorsLeft + 2, y,
                int(i) == fTutor ? kSelectedColor : _TutorAvailable(int(i))
                    ? kAvailableColor : kUnavailableColor);
        }
    }

    _DrawText("You decide to...", kDecideLeft, kDecideTop, kMessageColor);
    _DrawText("S", kOptionsLeft, kSpendTop, kCrimsonColor);
    _DrawText("pend a day, doing the above", kOptionsLeft
        + fFont->StringWidth("S") + 1, kSpendTop, kMemberColor);
    _DrawText("L", kOptionsLeft, kLeaveTop, kCrimsonColor);
    _DrawText("eave", kOptionsLeft + fFont->StringWidth("L") + 1, kLeaveTop,
        kMemberColor);

    if (fCursorVisible) {
        DrawMouseCursor(fBuffer, fMouse, NearestColor(fPalette, 0, 0, 0),
            NearestColor(fPalette, 255, 255, 255));
    }
    return fBuffer;
}


int
ResidenceView::_Random(int n)
{
    return n > 0 ? int(fRandom() % uint32(n)) : 0;
}


// 0E76:14A4(13): the party's best Healing
int
ResidenceView::_BestHealing() const
{
    int best = 0;
    for (const character& member : fParty->members)
        best = std::max(best, int(member.skills[kSkillHealing]));
    return best;
}


// The gains shown (file 0x6FB60, 0x6FCAC, 0x707D1): regaining strength
// gives the party's best Healing / 15 (1..99), praying (Religion +
// Virtue) / 12 + 1
void
ResidenceView::_UpdateValues()
{
    if (fParty == NULL)
        return;
    for (size_t i = 0; i < fParty->members.size(); i++) {
        const character& member = fParty->members[i];
        switch (fActivities[i]) {
            case ACTIVITY_REGAIN_STRENGTH:
                fValues[i] = std::max(1, std::min(_BestHealing() / 15, 99));
                break;
            case ACTIVITY_PRAY:
                fValues[i] = (member.skills[kSkillReligion]
                    + member.skills[kSkillVirtue]) / 12 + 1;
                break;
            case ACTIVITY_EARN:
                break;
            default:
                fValues[i] = member.skills[kSkillHealing] / 15;
                break;
        }
    }
}


// The best paid job (file 0x70A0E), 2 pf a day as a Day Laborer if none
// pays more: ((the city's number % 4) + min(0, reputation / 20) +
// (attribute - threshold) / 3 + f(skill - threshold) for its two skills)
// · 9 · multiplier / 70, f being a square root within 1..9. A job may
// need a bit of the city's flags, or of the location's (+0x14, not kept
// here); those with an exclusion mask are never offered, as in the game.
void
ResidenceView::_FindJob(int index)
{
    const character& member = fParty->members[index];
    const city* place = fCity >= 0
        ? &fData.Cities().CityAt(uint32(fCity)) : NULL;
    int best = 0;
    std::string name;
    for (const exe_job& job : fExe->Jobs()) {
        if (job.cityFlags != 0
                && (place == NULL || (place->flags & job.cityFlags) == 0))
            continue;
        if (job.excluded != 0 || job.locationFlags != 0)
            continue;
        const int attribute = (member.attributes[job.attribute]
            - job.attributeBase) / 3;
        const int first = SkillPay(member.skills[job.skills[0]]
            - job.skillBases[0]);
        const int second = SkillPay(member.skills[job.skills[1]]
            - job.skillBases[1]);
        // the game takes the larger of the city size and 9: always 9
        const int size = std::max(place != NULL ? int(place->size) : 0, 9);
        const int reputation = std::min(0, fReputation / 20);
        const int seed = place != NULL ? place->peopleSeed % 4 : 0;
        const int pay = (seed + reputation + attribute + second + first)
            * size * job.multiplier / 70;
        if (pay > best) {
            best = pay;
            name = job.name;
        }
    }
    if (best < 2) {
        best = 2;
        name = "Day Laborer";
    }
    fValues[index] = best;
    fJobs[index] = name;
}


int
ResidenceView::_MenuAt(const GFX::point& point) const
{
    if (point.x < kMenuLeft || point.y < kMenuTop - 1)
        return -1;
    const int row = (point.y - kMenuTop + 1) / kMenuHeight;
    return row < ACTIVITY_COUNT ? row : -1;
}


void
ResidenceView::_DrawText(const std::string& utf8, int x, int y, uint8 color,
    int maxWidth)
{
    fFont->RenderString(Font::ToGameCharset(utf8), fBuffer, GFX::point(x, y),
        color, maxWidth);
}


// Word-wrapped text; returns the y under it
int
ResidenceView::_DrawWrapped(const std::string& utf8, int x, int y, int width,
    uint8 color)
{
    std::string rest = Font::ToGameCharset(utf8);
    while (!rest.empty()) {
        const std::string line = fFont->TruncateString(rest, uint16(width));
        fFont->RenderString(line, fBuffer, GFX::point(x, y), color);
        y += kLineHeight;
        if (line.empty())
            break;
    }
    return y;
}

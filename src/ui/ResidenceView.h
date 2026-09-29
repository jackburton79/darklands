/*
 * ResidenceView.h
 * Living at the inn ("take up residence to study, work, pray..."): the
 * screen on CAMPCITY.PIC where each party member picks what to do with
 * the day (relax, regain strength, pray, earn money, train with a
 * teacher), then the party spends days so, paying the inn.
 *
 * The rules are DARKLAND.EXE's (see docs/exe.md, "The residence"). The
 * input handlers and Draw() work without a window, for testing.
 */
#pragma once

#include "GraphicsDefs.h"
#include "SupportDefs.h"

#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

class Bitmap;
class ExeData;
class Font;
class GameData;
class GameTime;
class GameWindow;
class InfoView;
class PartySidebar;
struct character;
struct party;

// A teacher found in a city: the physician who takes students gives
// lessons while the party lives at the inn (DARKLAND.EXE: a person of
// kind 0x28, 0E76:2C4E)
struct city_tutor {
    int		skill;			// e.g. kSkillHealing
    int		level;			// the chance of a lesson: level · 20 / 100
    uint32	fee;			// pfennigs a day
    uint32	until;			// the offer, in hours (GameTime::HourStamp())
};

class ResidenceView {
public:
    enum activity {
        ACTIVITY_RELAX = 0,
        ACTIVITY_REGAIN_STRENGTH,
        ACTIVITY_PRAY,
        ACTIVITY_ALCHEMY,		// not implemented
        ACTIVITY_EARN,
        ACTIVITY_GUARD,			// in the wilderness only
        ACTIVITY_TRAIN,
        ACTIVITY_COUNT
    };

    explicit		ResidenceView(GameData& data);	// throws if data is missing
                    ~ResidenceView();

    // Not owned. Spending days changes the party's money and members and
    // advances the clock.
    void			SetParty(party* members);
    void			SetClock(GameTime* clock)	{ fClock = clock; }
    // The information screens that F1..F6 and the character boxes open
    // (not owned; NULL: none), as in the game (file 0x6FF0A).
    void			SetInfoView(InfoView* info)	{ fInfo = info; }
    // The inn: its city, the party's local reputation there, the price
    // of a day (the inn's meal and night) and the city's teachers (the
    // physician's, the monastery's). Every member starts relaxing.
    void			SetPlace(int cityIndex, int reputation, uint32 innPrice,
                        const std::vector<city_tutor>& tutors
                            = std::vector<city_tutor>());
    // The teacher that "Train or study" gives the selected member (file
    // 0x6F29B: the list shown by the menu's line, DS:89FA)
    void			SelectTutor(int tutor);
    int				Tutor() const			{ return fTutor; }
    int				TutorOf(int member) const;
    // Living in the slum (DARKLAND.EXE, file 0x6FDBE): before each day,
    // if random(100) is at least `safe`, thieves come and the party
    // leaves (Interrupted()); -1, as SetPlace() leaves it: never.
    void			SetAmbush(int safe)		{ fAmbushSafe = safe; }
    bool			Interrupted() const		{ return fInterrupted; }

    // Runs until the party leaves.
    void			Run(GameWindow& window);

    // Actions, as the keys and options of the screen
    void			SelectMember(int member);
    int				Member() const			{ return fMember; }
    // Ctrl+F1..F5: another member leads (file 0x6FF70)
    void			SetLeader(int member);
    // Whether the selected member can take up an activity now; Choose()
    // returns false if not.
    bool			Available(activity what) const;
    bool			Choose(activity what);
    // One day, until 5 in the morning (at least 9 hours): false, and
    // nothing happens, if the purse cannot pay for it or thieves come
    // (see SetAmbush()).
    bool			SpendDay();

    activity		ActivityOf(int member) const;
    int				ValueOf(int member) const;		// the gain, the pay
    const std::string& JobOf(int member) const;
    // What a day costs, in pfennigs: the inn, less the pay, plus the
    // lessons (negative: the party earns)
    int32			NetCost() const;
    int				Days() const			{ return fDays; }
    const std::string& Message() const		{ return fMessage; }

    // Input, in screen (320x200) coordinates; Clicked() returns false
    // when the click chose "Leave".
    void			MouseMoved(const GFX::point& point);
    bool			Clicked(const GFX::point& point);

    Bitmap*			Draw();

private:
    struct raw_picture {
        uint16 width;
        uint16 height;
        std::vector<uint8> pixels;
    };

    int				_Random(int n);
    int				_BestHealing() const;
    void			_UpdateValues();
    void			_FindJob(int member);
    int				_MenuAt(const GFX::point& point) const;
    int				_TutorAt(const GFX::point& point) const;
    bool			_TutorAvailable(int tutor) const;
    void			_DrawText(const std::string& utf8, int x, int y,
                        uint8 color, int maxWidth = 320);
    int				_DrawWrapped(const std::string& utf8, int x, int y,
                        int width, uint8 color);

    GameData&		fData;
    Bitmap*			fBuffer;
    std::unique_ptr<Font>	fFont;
    std::unique_ptr<PartySidebar> fSidebar;
    std::unique_ptr<ExeData> fExe;
    raw_picture		fBackground;
    GFX::Palette	fPalette;
    std::mt19937	fRandom;

    party*			fParty;
    GameTime*		fClock;
    InfoView*		fInfo;
    int				fCity;
    int				fReputation;
    uint32			fInnPrice;
    std::vector<city_tutor> fTutors;
    int				fTutor;			// the one chosen in the list
    std::vector<int> fTutorOf;		// by member
    bool			fTutorList;		// shown (DS:8A32)
    int				fMember;
    std::vector<activity> fActivities;	// by member
    std::vector<int> fValues;
    std::vector<std::string> fJobs;
    int				fDays;
    std::string		fMessage;
    int				fAmbushSafe;
    bool			fInterrupted;
    GFX::point		fMouse;
    bool			fCursorVisible;
};

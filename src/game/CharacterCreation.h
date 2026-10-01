/*
 * CharacterCreation.h
 * The creation of a character: the simulation of his life (manual p. 13).
 * He is born in one of six family backgrounds; fifteen years of
 * childhood give attributes and skills, then he goes through occupations
 * of five years each, as many as the player likes. Every stage gives
 * experience points (EPs) to spend on attributes (after childhood) or on
 * skills (after each occupation); which occupations a stage offers
 * depends on his age, attributes, skills and the occupations he held.
 *
 * All of it is DARKLAND.EXE's overlay at file 0x76890 (the code of
 * 1462:4664.., see docs/exe.md, "Character creation"); the tables are
 * the ones of ExeData. No SDL here: CreationView shows it.
 */
#pragma once

#include "Character.h"
#include "ExeData.h"
#include "SupportDefs.h"

#include <functional>
#include <string>
#include <vector>

class ListFile;

class CharacterCreation {
public:
    enum stage {
        STAGE_NAME = 0,			// name, nickname and sex
        STAGE_FAMILY,			// choosing the family background
        STAGE_ATTRIBUTES,		// EPs on the attributes
        STAGE_OCCUPATION,		// choosing the next occupation
        STAGE_SKILLS			// EPs on the skills
    };
    static const int kMaxNameLength		= 23;	// 1462:43E6 asks for 23
    static const int kMaxNicknameLength	= 9;
    static const int kChildhoodYears	= 15;
    static const int kOccupationYears	= 5;
    static const int kLastAgeToContinue	= 65;	// "next occupation" past it is refused
    static const int kMaxAttribute		= 40;	// EPs cannot take one past it
    static const int kMaxSkill			= 89;
    static const int kMaxChoices		= 20;	// the game's list holds 20

    // `random(n)` is 0..n-1
    CharacterCreation(const ExeData& exe, const ListFile& lists,
        const std::function<int(int)>& random);

    stage			Stage() const			{ return fStage; }

    // Stage STAGE_NAME: a new person, a man at first (1462:675A)
    void			Restart();
    bool			Female() const			{ return fFemale; }
    const std::string& Name() const			{ return fName; }
    const std::string& Nickname() const		{ return fNickname; }
    // A new random name for the sex, its first word the nickname
    void			NewName();
    // Typed names (the nickname is not touched by a new name; an empty
    // one is refused)
    bool			SetName(const std::string& name);
    bool			SetNickname(const std::string& nickname);
    // "Make him a woman" / "Make her a man": the starting attributes
    // of the other sex; a random name is made again unless the name
    // was typed
    void			ToggleSex();
    // "Begin childhood", and back to the name before a family is chosen
    void			BeginChildhood();
    void			BackToName();

    // The choices of STAGE_FAMILY and STAGE_OCCUPATION: the families
    // 0..5, or the occupations that are offered now (indexes into
    // ExeData::Occupations())
    const std::vector<int>&	Choices() const	{ return fChoices; }
    const std::string&	ChoiceName(size_t row) const;
    // Takes a choice: a family (childhood goes by, STAGE_ATTRIBUTES) or
    // an occupation (five years, STAGE_SKILLS)
    bool			Choose(size_t row);

    // Experience points
    int				Age() const				{ return fAge; }
    int				Points() const			{ return fPoints; }
    int				TotalPoints() const		{ return fTotalPoints; }
    // STAGE_ATTRIBUTES: +1 / -1 on attribute 0..5; STAGE_SKILLS: on skill
    // 0..18. What a point costs grows with the value; only the points
    // spent in this stage can be taken back.
    bool			Increase(int index);
    bool			Decrease(int index);
    int				IncreaseCost(int index) const;
    int				DecreaseRefund(int index) const;
    // How many EPs went on each skill in this stage
    int				SkillPointsSpent(int skill) const	{ return fSkillSpent[skill]; }

    // What he learned in his occupations
    bool			KnowsSaint(int saint) const;
    int				FormulaCount(int formula) const	{ return fFormulae[formula]; }

    // The attributes (maximum, which the game builds) and skills so far
    int				Attribute(int index) const		{ return fAttributes[index]; }
    int				Skill(int index) const			{ return fSkills[index]; }
    int				Held(int occupation) const		{ return fHeld[occupation]; }
    int				Last() const			{ return fLast; }
    int				Previous() const		{ return fPrevious; }
    int				Family() const			{ return fFamily; }

    // What the screen shows for a family or an occupation before it is
    // taken (1462:5D98): the EPs, the attributes it leads to, and for
    // each skill the points it gives and the EPs it leaves to spend (the
    // game shows two more points than it gives for the second
    // occupation, at age 20)
    struct preview {
        int points;
        int attributes[6];
        int skillGain[kLifeSkillCount];
        int skillRoom[kLifeSkillCount];
    };
    // `choice` is a family (STAGE_FAMILY) or an occupation
    preview			PreviewChoice(int choice) const;
    // What the boards show in the current stage: the choice made last
    preview			PreviewCurrent() const;

    // STAGE_SKILLS: "Go to next occupation" (not past 65), "Begin
    // Adventuring", "Kill character"
    bool			CanContinue() const;
    bool			NextOccupation();
    // STAGE_ATTRIBUTES: the EPs that are left are lost
    void			DoneWithAttributes();
    // The character: attributes at their maximum, the equipment of the
    // last occupation, a weapon for his best weapon skill and a potion
    // for each formula he learned (1462:5248); STAGE_SKILLS only
    bool			CanFinish() const;
    character		Finish();

    // The rules that decide which occupations are offered, for tests
    bool			IsOccupationOffered(int occupation) const;

private:
    void			_Apply(const exe_life_stage& what, bool family, int index);
    void			_GiveOccupationRewards(int occupation);
    void			_GiveItem(int code);
    void			_BuildChoices();
    void			_ClearSpent();

    const ExeData&	fExe;
    const ListFile&	fLists;
    std::function<int(int)> fRandom;

    stage			fStage;
    bool			fFemale;
    bool			fNameTyped;
    std::string		fName;
    std::string		fNickname;
    int				fAge;
    int				fAttributes[ATTRIBUTE_COUNT];	// maximum values
    int				fSkills[kSkillCount];
    int				fFamily;
    int				fLast;			// the occupation held now
    int				fPrevious;		// the one before
    bool			fHeld[kOccupationCount];
    int				fPoints;		// EPs left
    int				fTotalPoints;
    int				fAttributeSpent[6];
    int				fSkillSpent[kSkillCount];
    std::vector<int> fChoices;
    std::vector<item> fItems;		// of the last occupation
    uint8			fSaints[20];
    uint8			fFormulae[22];
};

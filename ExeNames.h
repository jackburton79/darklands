/*
 * ExeNames.h
 * The names DARKLAND.EXE gives the people the party meets: its lists of
 * first names and surnames, read from the executable (they are not in
 * any data file), and its name generator (1367:0DB4). See docs/exe.md.
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

class ExeNames {
public:
    explicit		ExeNames(const std::string& exePath);	// throws on error

    // A man's name, "Albrecht Behaim", as the game makes it for `seed`
    // (the game's seed global plus a number for the person)
    std::string		MaleName(uint16 seed) const;

    const std::vector<std::string>& MaleNames() const	{ return fMale; }
    const std::vector<std::string>& FemaleNames() const	{ return fFemale; }
    const std::vector<std::string>& Surnames() const	{ return fSurnames; }

private:
    std::vector<std::string>	fMale;		// UTF-8
    std::vector<std::string>	fFemale;
    std::vector<std::string>	fSurnames;
};

// The Microsoft C runtime's rand(), as DARKLAND.EXE uses it
class MscRandom {
public:
    explicit		MscRandom(uint32 seed) : fSeed(seed) {}

    int				Next();				// 0..32767
    int				Below(int n);		// 0..n-1, the game's random(n)

private:
    uint32			fSeed;
};

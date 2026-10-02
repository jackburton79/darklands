/*
 * GameSettings.h
 * What the "Game" menu of the menu bar sets (manual p. 18): the difficulty,
 * and whether the temporary changes are shown, the music plays and the
 * sound effects play. DARKLAND.EXE keeps the difficulty at DS:906A, and
 * a saved game holds it (docs/formats.md); the others are not saved so far.
 */
#pragma once

enum game_difficulty {
    DIFFICULTY_BASIC = 0,
    DIFFICULTY_STANDARD = 1,
    DIFFICULTY_EXPERT = 2
};

struct game_settings {
    game_settings()
        :
        difficulty(DIFFICULTY_STANDARD),
        showChanges(true),
        music(true),
        soundEffects(true),
        extras(false)
    {
    }

    int		difficulty;			// game_difficulty
    bool	showChanges;		// messages about temporary changes
    bool	music;
    bool	soundEffects;
    // The aids the original game does not have (docs/extras.md): off, the
    // game is as DARKLAND.EXE has it. The "Extras" item of the Game menu
    bool	extras;
};

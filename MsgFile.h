/*
 * MsgFile.h
 * Reader for the .MSG menu/card files, stored in the MSGFILES catalog
 * (e.g. $PARTY02.MSG). A file is a deck of cards: each card is a text
 * box with menu options. See docs/formats.md.
 */
#pragma once

#include "SupportDefs.h"

#include <string>
#include <vector>

// Control codes in the card text. The other bytes are characters in the
// game's character set (0x1F is 'ä', not a code).
enum msg_code {
    MSG_CODE_BATTLE_OPTION	= 0x06,	// option that starts a battle at once
    MSG_CODE_NEWLINE		= 0x0A,
    MSG_CODE_POTION_OPTION	= 0x10,	// option that opens the potion list
    MSG_CODE_PARAGRAPH		= 0x14,
    MSG_CODE_OPTION			= 0x15,	// normal option
    MSG_CODE_SAINT_OPTION	= 0x16,	// option that opens the saint list
    MSG_CODE_OPTION_TEXT	= 0x1D	// ends an option's "..." prefix
};

struct msg_card {
    uint8 textTop;			// header +0: top of the text
    uint8 textLeft;			// header +1: left of the text
    uint8 unknown1;			// header +2: 0 in every real card
    uint8 textRight;		// header +3: right limit of the text
    uint8 unknown2;			// header +4: 0 in every real card
    std::string text;		// game character set, with msg_code codes
};

class Stream;

class MsgFile {
public:
    // Reads the whole stream, which stays owned by the caller.
    explicit		MsgFile(Stream* stream);		// throws on error
    explicit		MsgFile(const std::string& fileName);

    uint32			CountCards() const;
    const msg_card&	CardAt(uint32 index) const;

private:
    void			_Load(Stream* stream);

    std::vector<msg_card>	fCards;
};

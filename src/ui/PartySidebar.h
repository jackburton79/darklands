/*
 * PartySidebar.h
 * The character boxes on the left of the screen (manual p. 17), one per
 * party member: nickname (the leader in another color), the character's
 * picture and three bars with their values: current endurance, strength
 * and divine favor. They use the interface palette range 128..159 and
 * the EGA colors, the same in every screen.
 */
#pragma once

#include "GraphicsDefs.h"
#include "SupportDefs.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class Bitmap;
class Font;
class GameData;
struct party;

class PartySidebar {
public:
    static const int kWidth = 60;

    explicit		PartySidebar(GameData& data);	// throws if data is missing
                    ~PartySidebar();

    // Not owned; NULL: none. Throws if a character's picture is missing.
    void			SetParty(const party* members);

    // Draws the boxes; `background`: also their blue background
    // (SIDEBAR.PIC); the info screens have their own.
    void			Draw(Bitmap* bitmap, bool background) const;

    // The member whose box is under `point`, or -1.
    int				MemberAt(const GFX::point& point) const;

private:
    struct raw_picture {
        uint16 width;
        uint16 height;
        std::vector<uint8> pixels;
    };

    raw_picture		_LoadPicture(const std::string& name) const;
    static void		_DrawPicture(Bitmap* bitmap, const raw_picture& picture,
                        int x, int y, int transparent);

    GameData&		fData;
    std::unique_ptr<Font>	fNameFont;
    std::unique_ptr<Font>	fNumberFont;
    raw_picture		fBackground;
    const party*	fParty;
    std::map<std::string, raw_picture> fPictures;	// by image code
};

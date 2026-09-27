#include "PartySidebar.h"

#include "Bitmap.h"
#include "Character.h"
#include "FileStream.h"
#include "GameData.h"
#include "PICImage.h"
#include "TextSupport.h"

#include <algorithm>

// Fonts of FONTS.FNT: the nicknames (as the card text), the numbers
static const uint32 kNameFontIndex		= 2;
static const uint32 kNumberFontIndex	= 0;

static const int kBackgroundLeft	= 3;	// SIDEBAR.PIC, 54 x 198
static const int kBackgroundTop		= 1;

// Boxes 40 pixels high (verified: the box borders of PTYSTATS.PIC), the
// rest measured on the manual's screenshots, pp. 17 and 20, +-1 pixel
static const int kBoxHeight			= 40;
static const int kNameLeft			= 5;
static const int kNameTop			= 1;
static const int kPictureLeft		= 5;	// <image>STAT.PIC, 10 x 19
static const int kPictureTop		= 9;
static const int kBarsTop			= 10;
static const int kBarHeight			= 16;
static const int kBarWidth			= 2;
static const int kNumbersTop		= 28;
static const int kBarCenters[3]		= { 23, 40, 53 };

// Colors (inferred from the screenshots, which are almost black and
// white): white text and divine favor bar, darker endurance and strength
// bars, the leader "in special colored text"
static const uint8 kBackgroundColor	= 159;
static const uint8 kNameColor		= 15;	// EGA white
static const uint8 kLeaderColor		= 14;	// EGA yellow
static const uint8 kNumberColor		= 15;
static const uint8 kBarTrackColor	= 8;	// EGA dark gray
static const uint8 kBarColors[3]	= { 12, 10, 15 };	// light red, light
                                                        // green, white


PartySidebar::PartySidebar(GameData& data)
    :
    fData(data),
    fParty(NULL)
{
    fNameFont.reset(new Font(fData.Fonts(), kNameFontIndex));
    fNumberFont.reset(new Font(fData.Fonts(), kNumberFontIndex));
    fBackground = _LoadPicture("SIDEBAR.PIC");
}


PartySidebar::~PartySidebar()
{
}


void
PartySidebar::SetParty(const party* members)
{
    fParty = members;
    fPictures.clear();
    if (fParty == NULL)
        return;
    for (const std::string& image : fParty->images) {
        if (fPictures.find(image) == fPictures.end())
            fPictures[image] = _LoadPicture(image + "STAT.PIC");
    }
}


void
PartySidebar::Draw(Bitmap* bitmap, bool background) const
{
    if (background) {
        bitmap->FillRect(GFX::rect(0, 0, kWidth, 200), kBackgroundColor);
        _DrawPicture(bitmap, fBackground, kBackgroundLeft, kBackgroundTop, -1);
    }
    if (fParty == NULL)
        return;

    static const int kBarAttributes[3] = { ATTRIBUTE_ENDURANCE,
        ATTRIBUTE_STRENGTH, ATTRIBUTE_DIVINE_FAVOR };
    for (size_t i = 0; i < fParty->members.size() && i < size_t(kMaxPartySize);
            i++) {
        const character& member = fParty->members[i];
        const int top = int(i) * kBoxHeight;
        fNameFont->RenderString(Font::ToGameCharset(member.shortName), bitmap,
            GFX::point(kNameLeft, top + kNameTop), int(i) == fParty->leader
                ? kLeaderColor : kNameColor, kWidth - kNameLeft);

        const std::map<std::string, raw_picture>::const_iterator picture
            = fPictures.find(fParty->images[i]);
        if (picture != fPictures.end()) {
            _DrawPicture(bitmap, picture->second, kPictureLeft,
                top + kPictureTop, 0);
        }

        // bars: the current value as a share of the maximum
        for (int bar = 0; bar < 3; bar++) {
            const int attribute = kBarAttributes[bar];
            const int value = member.attributes[attribute];
            const int maximum = std::max<int>(member.maxAttributes[attribute], 1);
            const int height = std::min(kBarHeight, value * kBarHeight / maximum);
            const int x = kBarCenters[bar] - kBarWidth / 2;
            const int bottom = top + kBarsTop + kBarHeight;
            bitmap->FillRect(GFX::rect(x, top + kBarsTop, kBarWidth,
                kBarHeight), kBarTrackColor);
            if (height > 0) {
                bitmap->FillRect(GFX::rect(x, bottom - height, kBarWidth,
                    height), kBarColors[bar]);
            }
            const std::string number = std::to_string(value);
            const int width = fNumberFont->StringWidth(number);
            fNumberFont->RenderString(number, bitmap,
                GFX::point(kBarCenters[bar] - width / 2, top + kNumbersTop),
                kNumberColor);
        }
    }
}


int
PartySidebar::MemberAt(const GFX::point& point) const
{
    if (fParty == NULL || point.x < 0 || point.x >= kWidth || point.y < 0)
        return -1;
    const int member = point.y / kBoxHeight;
    return member < int(fParty->members.size()) ? member : -1;
}


PartySidebar::raw_picture
PartySidebar::_LoadPicture(const std::string& name) const
{
    FileStream stream(fData.PathFor(name).c_str(), FileStream::READ_ONLY);
    PICImage image(&stream);
    raw_picture picture;
    picture.width = image.Width();
    picture.height = image.Height();
    picture.pixels = image.RawBytes();
    return picture;
}


/* static */
void
PartySidebar::_DrawPicture(Bitmap* bitmap, const raw_picture& picture,
    int x, int y, int transparent)
{
    for (int row = 0; row < picture.height; row++) {
        for (int column = 0; column < picture.width; column++) {
            const uint8 pixel = picture.pixels[size_t(row) * picture.width
                + column];
            if (pixel != transparent)
                bitmap->PutPixel(x + column, y + row, pixel);
        }
    }
}

/*
 * CreationView.cpp
 */

#include "CreationView.h"

#include "Bitmap.h"
#include "FileStream.h"
#include "GameData.h"
#include "ListFile.h"
#include "PICImage.h"
#include "Palette.h"
#include "ScreenSupport.h"
#include "TextSupport.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

static const uint32 kFontIndex		= 2;

// Where DARKLAND.EXE puts things (1462:573A, 5D98 and 4664), on CHARGEN.PIC
static const int kNicknameLeft		= 71;
static const int kNameLeft			= 131;
static const int kTitleTop			= 4;
static const int kAgeLeft			= 268;
static const int kAgeLabelLeft		= 286;
static const int kHelpLeft			= 71;
static const int kHelpTop			= 18;
static const int kHelpPitch			= 8;
static const int kButtonLeft		= 64;
static const int kButtonTop			= 45;
static const int kButtonPitch		= 11;
static const int kButtonWidth		= 109;
static const int kButtonHeight		= 10;
static const int kButtonTextLeft	= 68;
static const int kButtonTextTop		= 2;
static const int kButtonRows		= 14;	// the list ends at y 199
static const int kRowPitch			= 8;
static const int kBoardTop			= 20;
static const int kAttributeValueLeft = 201;
static const int kAttributeNameLeft	= 216;
static const int kAttributeLeft		= 197;
static const int kAttributeWidth	= 38;
static const int kGainLeft			= 246;
static const int kSkillValueLeft	= 278;
static const int kSkillNameLeft		= 290;
static const int kSkillLeft			= 247;
static const int kSkillWidth		= 67;
static const int kLastGroupShift	= 2;	// the last board's text sits lower
static const int kPointsLabelLeft	= 216;
static const int kPointsValueLeft	= 198;
static const int kPointsTop			= 87;
static const int kGaugeLeft			= 208;
static const int kGaugeTop			= 105;
static const int kGaugeWidth		= 12;
static const int kGaugeHeight		= 50;
static const int kTubeLeft			= 196;
static const int kTubeTop			= 96;
static const int kTubeWidth			= 39;
static const int kTubeHeight		= 69;
static const int kListButtonLeft	= 175;
static const int kListButtonWidth	= 59;
static const int kFormulaeTop		= 169;
static const int kSaintsTop			= 185;
static const int kListButtonHeight	= 11;
static const int kInputLeft			= 110;
static const int kInputTop			= 80;
static const uint32 kFrameMilliseconds = 120;

static const char* kAttributeNames[ATTRIBUTE_COUNT] = {
    "End", "Str", "Agl", "Per", "Int", "Chr", "DF "
};
static const char* kSkillNames[kSkillCount] = {
    "wEdg", "wImp", "wFll", "wPol", "wThr", "wBow", "wMsD",
    "Alch", "Relg", "Virt", "SpkC", "SpkL", "R&W",
    "Heal", "Artf", "Stlh", "StrW", "Ride", "WdWs"
};

// The three lines of help on the second plaque, by stage
static const char* kHelp[5][3] = {
    { "Select character name", "and gender, then start", "childhood." },
    { "Select family background", "", "" },
    { "Use EPs to improve attr", "Select attribute and", "use + or - to change" },
    { "Select an occupation", "", "" },
    { "Use EPs to improve skills", "Select skill and use + or", "- to change" }
};

// The buttons' texts of the name stage, and their keys, as the game has
// them (290E:1D43 strings 45..51)
static const char* kNameButtons[6] = {
    "Begin childhood", "Make him a woman", "Select new name",
    "Enter a new name", "Create a nickname", "Return to game options"
};
static const char kNameKeys[6] = { 'B', 'M', 'S', 'E', 'C', 'R' };
static const char* kSkillButtons[3] = {
    "Go to next occupation", "Begin Adventuring", "Kill character"
};
static const char kSkillKeys[3] = { 'G', 'B', 'K' };


CreationView::CreationView(GameData& data,
    const std::function<int(int)>& random)
    :
    fData(data),
    fBuffer(NULL),
    fDone(false),
    fCancelled(false),
    fHighlight(-1),
    fRow(0),
    fFrame(0),
    fFrameTime(0),
    fTyping(TYPING_NONE),
    fList(LIST_NONE),
    fMouse(0, 0)
{
    fCreation.reset(new CharacterCreation(data.Exe(), data.Lists(), random));
    fFont.reset(new Font(data.Fonts(), kFontIndex));
    fPalette = PICImage::EGAPalette();
    fBackground = _LoadPicture("CHARGEN.PIC", &fPalette);
    fPlate = _LoadPicture("BUTTONA.PIC");
    for (int i = 0; i < 5; i++)
        fBubbles[i] = _LoadPicture(std::string("BUBBLE0") + char('1' + i) + ".PIC");
    fTube = _LoadPicture("BUBLBACK.PIC");
    fTextBack = _LoadPicture("TEXTBACK.PIC");
    fBuffer = new Bitmap(kScreenWidth, kScreenHeight, 8);
    fBuffer->SetColors(fPalette.colors, 0, 256);
    fBlack = 0;
    fCream = 191;				// the plaques' light color
    fHover = 9;					// EGA light blue
    fSelected = 10;				// EGA light green
    fWhite = 15;
    fCrimson = NearestColor(fPalette, 200, 16, 40);
}


CreationView::~CreationView()
{
    if (fBuffer != NULL)
        fBuffer->Release();
}


CreationView::raw_picture
CreationView::_LoadPicture(const std::string& name,
    GFX::Palette* palette) const
{
    FileStream stream(fData.PathFor(name).c_str(), FileStream::READ_ONLY);
    PICImage image(&stream);
    if (palette != NULL)
        image.ApplyPalette(*palette);
    raw_picture picture;
    picture.width = image.Width();
    picture.height = image.Height();
    picture.pixels = image.RawBytes();
    return picture;
}


void
CreationView::_DrawPicture(const raw_picture& picture, int x, int y)
{
    _DrawPartOf(picture, x, y, picture.height);
}


// The first `rows` rows of a picture
void
CreationView::_DrawPartOf(const raw_picture& picture, int x, int y, int rows)
{
    for (int row = 0; row < rows && row < picture.height; row++) {
        for (int column = 0; column < picture.width; column++) {
            const int px = x + column;
            const int py = y + row;
            if (px < 0 || py < 0 || px >= kScreenWidth || py >= kScreenHeight)
                continue;
            fBuffer->PutPixel(px, py,
                picture.pixels[size_t(row) * picture.width + column]);
        }
    }
}


void
CreationView::_Text(const std::string& text, int x, int y, uint8 color)
{
    fFont->RenderString(Font::ToGameCharset(text), fBuffer, GFX::point(x, y),
        color);
}


// On the plaques: dark text over a light shadow, one pixel lower
void
CreationView::_ShadowText(const std::string& text, int x, int y)
{
    _Text(text, x, y + 1, fCream);
    _Text(text, x, y, fBlack);
}


/* static */
GFX::rect
CreationView::ButtonRect(int row)
{
    return GFX::rect(kButtonLeft, kButtonTop + row * kButtonPitch,
        kButtonWidth, kButtonHeight);
}


/* static */
GFX::rect
CreationView::AttributeRect(int row)
{
    return GFX::rect(kAttributeLeft, kBoardTop + row * kRowPitch,
        kAttributeWidth, kRowPitch);
}


/* static */
int
CreationView::SkillOfRow(int row)
{
    if (row < 0 || row >= kSkillRows || row == 7 || row == 14)
        return -1;
    return row < 7 ? row : row < 14 ? row - 1 : row - 2;
}


/* static */
int
CreationView::RowOfSkill(int skill)
{
    return skill < 7 ? skill : skill < 13 ? skill + 1 : skill + 2;
}


/* static */
GFX::rect
CreationView::SkillRect(int row)
{
    return GFX::rect(kSkillLeft, kBoardTop + row * kRowPitch, kSkillWidth,
        kRowPitch);
}


/* static */
GFX::rect
CreationView::GaugeRect()
{
    return GFX::rect(kTubeLeft, kTubeTop, kTubeWidth, kTubeHeight);
}


std::vector<std::string>
CreationView::_ButtonLabels() const
{
    std::vector<std::string> labels;
    switch (fCreation->Stage()) {
        case CharacterCreation::STAGE_NAME:
            for (int i = 0; i < 6; i++) {
                labels.push_back(i == 1 && fCreation->Female()
                    ? "Make her a man" : kNameButtons[i]);
            }
            break;
        case CharacterCreation::STAGE_ATTRIBUTES:
            labels.push_back("Done changing attr");
            break;
        case CharacterCreation::STAGE_SKILLS:
            for (int i = 0; i < 3; i++)
                labels.push_back(kSkillButtons[i]);
            break;
        default:
            for (size_t i = 0; i < fCreation->Choices().size()
                    && int(i) < kButtonRows; i++)
                labels.push_back(fCreation->ChoiceName(i));
            break;
    }
    return labels;
}


// The key of a button is shown in crimson
void
CreationView::_Button(int row, const std::string& label, bool hot)
{
    const GFX::rect box = ButtonRect(row);
    _DrawPicture(fPlate, box.x, box.y);
    const int x = kButtonTextLeft;
    const int y = box.y + kButtonTextTop;
    const CharacterCreation::stage stage = fCreation->Stage();
    const bool keyed = stage == CharacterCreation::STAGE_NAME
        || stage == CharacterCreation::STAGE_SKILLS;
    const bool dim = stage == CharacterCreation::STAGE_SKILLS && row == 0
        && !fCreation->CanContinue();
    const uint8 color = dim ? 8 : hot ? fHover : fBlack;
    if (keyed) {
        _Text(label.substr(0, 1), x, y, dim ? uint8(8) : fCrimson);
        _Text(label.substr(1), x + fFont->StringWidth(
            Font::ToGameCharset(label.substr(0, 1))), y, color);
    } else {
        _Text(label, x, y, color);
    }
}


int
CreationView::_ButtonAt(const GFX::point& point) const
{
    const int rows = int(_ButtonLabels().size());
    for (int row = 0; row < rows; row++) {
        const GFX::rect box = ButtonRect(row);
        if (point.x >= box.x && point.x < box.x + box.w + 1
                && point.y >= box.y && point.y < box.y + kButtonPitch)
            return row;
    }
    return -1;
}


Bitmap*
CreationView::Draw()
{
    _DrawPicture(fBackground, 0, 0);
    const CharacterCreation::stage stage = fCreation->Stage();

    // the names and the age on the title plaque
    _ShadowText(fCreation->Nickname(), kNicknameLeft, kTitleTop);
    _ShadowText(fCreation->Name(), kNameLeft, kTitleTop);
    char text[16];
    snprintf(text, sizeof(text), "%02d", fCreation->Age());
    _ShadowText(text, kAgeLeft, kTitleTop);
    _ShadowText("Age", kAgeLabelLeft, kTitleTop);

    // what to do, on the second one
    for (int line = 0; line < 3; line++) {
        if (kHelp[stage][line][0] != 0)
            _ShadowText(kHelp[stage][line], kHelpLeft,
                kHelpTop + line * kHelpPitch);
    }

    // the buttons, or the choices
    const std::vector<std::string> labels = _ButtonLabels();
    const bool choosing = stage == CharacterCreation::STAGE_FAMILY
        || stage == CharacterCreation::STAGE_OCCUPATION;
    const int hotButton = IsTyping() || fList != LIST_NONE ? -1
        : _ButtonAt(fMouse);
    for (size_t i = 0; i < labels.size(); i++)
        _Button(int(i), labels[i], int(i) == (choosing ? fHighlight : hotButton));

    _DrawBoards();
    _DrawGauge();
    if (fList != LIST_NONE)
        _DrawList();
    if (IsTyping())
        _DrawInput();
    return fBuffer;
}


void
CreationView::_DrawBoards()
{
    const CharacterCreation::stage stage = fCreation->Stage();
    const bool choosing = stage == CharacterCreation::STAGE_FAMILY
        || stage == CharacterCreation::STAGE_OCCUPATION;
    const bool hovering = choosing && fHighlight >= 0
        && fHighlight < int(fCreation->Choices().size());
    CharacterCreation::preview shown;
    if (hovering)
        shown = fCreation->PreviewChoice(fCreation->Choices()[size_t(fHighlight)]);
    else
        shown = fCreation->PreviewCurrent();
    char text[32];

    // the points to spend
    if (stage != CharacterCreation::STAGE_NAME) {
        _Text("EP", kPointsLabelLeft, kPointsTop, fCream);
        if (hovering || stage == CharacterCreation::STAGE_ATTRIBUTES
                || stage == CharacterCreation::STAGE_SKILLS) {
            snprintf(text, sizeof(text), "%02d", shown.points);
            _Text(text, kPointsValueLeft, kPointsTop, fCream);
        }
    }

    // the attributes
    const bool editingAttributes = stage == CharacterCreation::STAGE_ATTRIBUTES;
    for (int i = 0; i < ATTRIBUTE_COUNT; i++) {
        const int y = kBoardTop + i * kRowPitch;
        const uint8 color = editingAttributes && i == fRow ? fSelected : fCream;
        const int value = i < 6 ? (hovering ? shown.attributes[i]
            : fCreation->Attribute(i)) : fCreation->Attribute(i);
        snprintf(text, sizeof(text), "%02d", value);
        _Text(text, kAttributeValueLeft, y, color);
        _Text(kAttributeNames[i], kAttributeNameLeft, y, color);
    }

    // the skills: the points the choice gives and the EPs it leaves
    const bool editingSkills = stage == CharacterCreation::STAGE_SKILLS;
    const bool occupation = stage == CharacterCreation::STAGE_OCCUPATION
        || stage == CharacterCreation::STAGE_SKILLS;
    for (int row = 0; row < kSkillRows; row++) {
        const int skill = SkillOfRow(row);
        if (skill < 0)
            continue;
        const int y = kBoardTop + row * kRowPitch
            + (row > 14 ? kLastGroupShift : 0);
        const uint8 color = editingSkills && row == fRow ? fSelected : fCream;
        if (stage == CharacterCreation::STAGE_NAME
                || (stage == CharacterCreation::STAGE_OCCUPATION && !hovering)) {
            snprintf(text, sizeof(text), "0:00");
        } else if (occupation) {
            snprintf(text, sizeof(text), "%2d:%02d", shown.skillGain[skill],
                shown.skillRoom[skill]);
        } else {
            snprintf(text, sizeof(text), "%2d:00", shown.skillGain[skill]);
        }
        _Text(text, kGainLeft, y, color);
        snprintf(text, sizeof(text), "%02d", fCreation->Skill(skill));
        _Text(text, kSkillValueLeft, y, color);
        _Text(kSkillNames[skill], kSkillNameLeft, y, color);
    }
}


// The potion in the tube: it empties as the points are spent (the level
// is the share of them that went, 1462:46C4)
void
CreationView::_DrawGauge()
{
    const CharacterCreation::stage stage = fCreation->Stage();
    int level = 0;
    if ((stage == CharacterCreation::STAGE_ATTRIBUTES
            || stage == CharacterCreation::STAGE_SKILLS)
            && fCreation->TotalPoints() > 0) {
        level = (100 - 100 * fCreation->Points() / fCreation->TotalPoints()
            + 1) / 2;
    }
    level = std::max(0, std::min(kGaugeHeight, level));
    _DrawPicture(fBubbles[fFrame % 5], kGaugeLeft, kGaugeTop);
    _DrawPartOf(fTube, kGaugeLeft, kGaugeTop, level);
}


// 1462:43E6: a plaque with what is asked and what was typed so far
void
CreationView::_DrawInput()
{
    _DrawPicture(fTextBack, kInputLeft, kInputTop);
    _Text(fTyping == TYPING_NAME ? "Character Name:" : "Character Nickname:",
        125, 90, fBlack);
    _Text(fTypedText + "_", 135, 100, fBlack);
}


// The formulae and the saints he knows (1462:7F06 and 802A)
void
CreationView::_DrawList()
{
    const bool formulae = fList == LIST_FORMULAE;
    fBuffer->FillRect(GFX::rect(70, 40, 158, 150), fBlack);
    fBuffer->FillRect(GFX::rect(72, 42, 154, 146), fCream);
    _Text(formulae ? "Formulae" : "Saints", 76, 44, fBlack);
    const ListFile& lists = fData.Lists();
    int line = 0;
    if (formulae) {
        // a byte per potion: a bit for each of the three authors' versions
        for (int k = 0; k < 22 && line < 16; k++) {
            for (int bit = 0; bit < 3 && line < 16; bit++) {
                const size_t name = size_t(3 * k + bit);
                if ((fCreation->FormulaCount(k) & (1 << bit)) == 0
                        || name >= lists.Formulae().size())
                    continue;
                _Text(lists.Formulae()[name], 76, 56 + line * 8, fBlack);
                line++;
            }
        }
    } else {
        for (size_t i = 0; i < lists.Saints().size() && line < 16; i++) {
            if (!fCreation->KnowsSaint(int(i)))
                continue;
            _Text(lists.Saints()[i], 76, 56 + line * 8, fBlack);
            line++;
        }
    }
    if (line == 0)
        _Text("None.", 76, 56, fBlack);
}


void
CreationView::MouseMoved(const GFX::point& point)
{
    fMouse = point;
    const CharacterCreation::stage stage = fCreation->Stage();
    if (stage == CharacterCreation::STAGE_FAMILY
            || stage == CharacterCreation::STAGE_OCCUPATION) {
        const int row = _ButtonAt(point);
        fHighlight = row < int(fCreation->Choices().size()) ? row : -1;
    }
}


void
CreationView::_Select(int row)
{
    fRow = row;
}


void
CreationView::_MoveRow(int step)
{
    const CharacterCreation::stage stage = fCreation->Stage();
    if (stage == CharacterCreation::STAGE_ATTRIBUTES) {
        fRow = (fRow + step + 6) % 6;
    } else if (stage == CharacterCreation::STAGE_SKILLS) {
        do {
            fRow = (fRow + step + kSkillRows) % kSkillRows;
        } while (SkillOfRow(fRow) < 0);
    } else if (stage == CharacterCreation::STAGE_FAMILY
            || stage == CharacterCreation::STAGE_OCCUPATION) {
        const int count = int(std::min(fCreation->Choices().size(),
            size_t(kButtonRows)));
        if (count > 0)
            fHighlight = fHighlight < 0 ? (step > 0 ? 0 : count - 1)
                : (fHighlight + step + count) % count;
    }
}


void
CreationView::_Plus()
{
    const CharacterCreation::stage stage = fCreation->Stage();
    if (stage == CharacterCreation::STAGE_ATTRIBUTES)
        fCreation->Increase(fRow);
    else if (stage == CharacterCreation::STAGE_SKILLS)
        fCreation->Increase(SkillOfRow(fRow));
}


void
CreationView::_Minus()
{
    const CharacterCreation::stage stage = fCreation->Stage();
    if (stage == CharacterCreation::STAGE_ATTRIBUTES)
        fCreation->Decrease(fRow);
    else if (stage == CharacterCreation::STAGE_SKILLS)
        fCreation->Decrease(SkillOfRow(fRow));
}


void
CreationView::_StartTyping(typing what)
{
    fTyping = what;
    fTypedText.clear();
}


void
CreationView::_StopTyping(bool accept)
{
    if (accept && !fTypedText.empty()) {
        if (fTyping == TYPING_NAME)
            fCreation->SetName(fTypedText);
        else if (fTyping == TYPING_NICKNAME)
            fCreation->SetNickname(fTypedText);
    }
    fTyping = TYPING_NONE;
    fTypedText.clear();
}


void
CreationView::TypeText(const std::string& text)
{
    if (!IsTyping())
        return;
    const size_t limit = size_t(fTyping == TYPING_NAME
        ? CharacterCreation::kMaxNameLength
        : CharacterCreation::kMaxNicknameLength);
    for (char c : text) {
        // the game takes the printable ASCII characters
        if (c >= 0x20 && c <= 0x7E && fTypedText.size() < limit)
            fTypedText += c;
    }
}


void
CreationView::_Finish()
{
    if (!fCreation->CanFinish())
        return;
    fCreated = fCreation->Finish();
    fDone = true;
}


// A button of the middle column, or a choice of the list
void
CreationView::_PressButton(int row)
{
    switch (fCreation->Stage()) {
        case CharacterCreation::STAGE_NAME:
            switch (row) {
                case 0:
                    fCreation->BeginChildhood();
                    fHighlight = -1;
                    break;
                case 1:
                    fCreation->ToggleSex();
                    break;
                case 2:
                    fCreation->NewName();
                    break;
                case 3:
                    _StartTyping(TYPING_NAME);
                    break;
                case 4:
                    _StartTyping(TYPING_NICKNAME);
                    break;
                case 5:
                    fCancelled = true;
                    break;
            }
            break;
        case CharacterCreation::STAGE_FAMILY:
        case CharacterCreation::STAGE_OCCUPATION:
            if (fCreation->Choose(size_t(row))) {
                fRow = 0;
                fHighlight = -1;
            }
            break;
        case CharacterCreation::STAGE_ATTRIBUTES:
            if (row == 0) {
                fCreation->DoneWithAttributes();
                fHighlight = -1;
            }
            break;
        case CharacterCreation::STAGE_SKILLS:
            if (row == 0) {
                if (fCreation->NextOccupation())
                    fHighlight = -1;
            } else if (row == 1) {
                _Finish();
            } else if (row == 2) {
                fCreation->Restart();
                fRow = 0;
            }
            break;
    }
}


void
CreationView::Clicked(const GFX::point& point)
{
    if (fList != LIST_NONE) {
        fList = LIST_NONE;
        return;
    }
    if (IsTyping())
        return;
    const CharacterCreation::stage stage = fCreation->Stage();
    const int row = _ButtonAt(point);
    if (row >= 0) {
        _PressButton(row);
        return;
    }
    if (stage == CharacterCreation::STAGE_ATTRIBUTES) {
        // the attributes: a click on the one selected adds a point
        for (int i = 0; i < 6; i++) {
            const GFX::rect box = AttributeRect(i);
            if (point.x >= box.x && point.x < box.x + box.w
                    && point.y >= box.y && point.y < box.y + box.h) {
                if (i == fRow)
                    _Plus();
                else
                    _Select(i);
                return;
            }
        }
    } else if (stage == CharacterCreation::STAGE_SKILLS) {
        for (int i = 0; i < kSkillRows; i++) {
            const GFX::rect box = SkillRect(i);
            if (SkillOfRow(i) >= 0 && point.x >= box.x
                    && point.x < box.x + box.w && point.y >= box.y
                    && point.y < box.y + box.h) {
                if (i == fRow)
                    _Plus();
                else
                    _Select(i);
                return;
            }
        }
    }
    const GFX::rect tube = GaugeRect();
    if ((stage == CharacterCreation::STAGE_ATTRIBUTES
            || stage == CharacterCreation::STAGE_SKILLS)
            && point.x >= tube.x && point.x < tube.x + tube.w
            && point.y >= tube.y && point.y < tube.y + tube.h) {
        // the tube takes a point back
        _Minus();
        return;
    }
    if ((stage == CharacterCreation::STAGE_OCCUPATION
            || stage == CharacterCreation::STAGE_SKILLS)
            && point.x >= kListButtonLeft
            && point.x < kListButtonLeft + kListButtonWidth) {
        if (point.y >= kFormulaeTop && point.y < kFormulaeTop + kListButtonHeight)
            fList = LIST_FORMULAE;
        else if (point.y >= kSaintsTop
                && point.y < kSaintsTop + kListButtonHeight)
            fList = LIST_SAINTS;
    }
}


void
CreationView::KeyPressed(int key)
{
    if (fList != LIST_NONE) {
        fList = LIST_NONE;
        return;
    }
    if (IsTyping()) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
            _StopTyping(true);
        else if (key == SDLK_ESCAPE)
            _StopTyping(false);
        else if (key == SDLK_BACKSPACE && !fTypedText.empty())
            fTypedText.erase(fTypedText.size() - 1);
        return;
    }
    const CharacterCreation::stage stage = fCreation->Stage();
    const bool enter = key == SDLK_RETURN || key == SDLK_KP_ENTER;
    if (key == SDLK_UP) {
        _MoveRow(-1);
    } else if (key == SDLK_DOWN) {
        _MoveRow(1);
    } else if (key == SDLK_PLUS || key == SDLK_KP_PLUS || key == SDLK_EQUALS) {
        _Plus();
    } else if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
        _Minus();
    } else if (key == SDLK_ESCAPE) {
        if (stage == CharacterCreation::STAGE_NAME)
            fCancelled = true;
        else if (stage == CharacterCreation::STAGE_FAMILY)
            fCreation->BackToName();
    } else if (stage == CharacterCreation::STAGE_NAME) {
        if (enter)
            _PressButton(0);
        for (int i = 0; i < 6; i++) {
            if (key == SDLK_a + (kNameKeys[i] - 'A'))
                _PressButton(i);
        }
    } else if (stage == CharacterCreation::STAGE_FAMILY
            || stage == CharacterCreation::STAGE_OCCUPATION) {
        if (enter && fHighlight >= 0)
            _PressButton(fHighlight);
        else if (key >= SDLK_1 && key <= SDLK_9
                && key - SDLK_1 < int(fCreation->Choices().size()))
            _PressButton(key - SDLK_1);
    } else if (stage == CharacterCreation::STAGE_ATTRIBUTES) {
        if (enter || key == SDLK_d)
            _PressButton(0);
    } else if (stage == CharacterCreation::STAGE_SKILLS) {
        if (enter || key == SDLK_g)
            _PressButton(0);
        else if (key == SDLK_b)
            _PressButton(1);
        else if (key == SDLK_k)
            _PressButton(2);
        else if (key == SDLK_n)
            _PressButton(0);
    }
}


void
CreationView::Tick(uint32 milliseconds)
{
    if (milliseconds - fFrameTime >= kFrameMilliseconds) {
        fFrameTime = milliseconds;
        fFrame = (fFrame + 1) % 5;
    }
}


CreationView::result
CreationView::Run(GameWindow& window)
{
    SDL_StartTextInput();
    bool dirty = true;
    for (;;) {
        if (dirty) {
            Draw();
            DrawMouseCursor(fBuffer, fMouse, fBlack, fWhite);
            window.Show(fBuffer);
            dirty = false;
        }
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 60) == 0) {
            const int before = fFrame;
            Tick(SDL_GetTicks());
            dirty = before != fFrame;
            continue;
        }
        switch (event.type) {
            case SDL_QUIT:
                SDL_PushEvent(&event);		// the screens below quit too
                SDL_StopTextInput();
                return RESULT_QUIT;
            case SDL_KEYDOWN:
                KeyPressed(event.key.keysym.sym);
                dirty = true;
                break;
            case SDL_TEXTINPUT:
                TypeText(event.text.text);
                dirty = true;
                break;
            case SDL_MOUSEMOTION:
                MouseMoved(GameWindow::ToScreen(event.motion.x, event.motion.y));
                dirty = true;
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    fMouse = GameWindow::ToScreen(event.button.x, event.button.y);
                    Clicked(fMouse);
                    dirty = true;
                }
                break;
            case SDL_WINDOWEVENT:
                dirty = true;
                break;
            default:
                break;
        }
        if (fDone) {
            SDL_StopTextInput();
            return RESULT_CREATED;
        }
        if (fCancelled) {
            SDL_StopTextInput();
            return RESULT_CANCELLED;
        }
    }
}

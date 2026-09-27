#include "ScreenSupport.h"

#include "Bitmap.h"
#include "GraphicsEngine.h"

#include <stdexcept>

static const uint16 kScreenWidth	= 320;
static const uint16 kScreenHeight	= 200;

// '#' = outline, 'o' = fill; hot spot top left.
static const char* kCursor[] = {
    "#",
    "##",
    "#o#",
    "#oo#",
    "#ooo#",
    "#oooo#",
    "#ooooo#",
    "#oooooo#",
    "#ooooo###",
    "#oo#oo#",
    "##  #oo#",
    "#   #oo#",
    "     ##"
};


GameWindow::GameWindow(const char* caption)
    :
    fConverted(NULL)
{
    if (!GraphicsEngine::Initialize())
        throw std::runtime_error("cannot initialize the graphics engine");
    GraphicsEngine* engine = GraphicsEngine::Get();
    engine->SetVideoMode(kScreenWidth * kWindowScale,
        kScreenHeight * kWindowScale, 32, GraphicsEngine::VIDEOMODE_WINDOWED);
    engine->SetWindowCaption(caption);
    fConverted = new Bitmap(kScreenWidth, kScreenHeight, 32);
}


GameWindow::~GameWindow()
{
    fConverted->Release();
    GraphicsEngine::Destroy();
}


void
GameWindow::Show(const Bitmap* buffer)
{
    // 8-bit buffer -> 32-bit copy (palette conversion) -> scaled to the
    // screen (SDL_SoftStretch needs matching formats; it keeps pixels sharp)
    GraphicsEngine* engine = GraphicsEngine::Get();
    GFX::rect source = buffer->Frame();
    GraphicsEngine::BlitBitmap(buffer, &source, fConverted, &source);
    GFX::rect screen = engine->ScreenFrame();
    GraphicsEngine::BlitBitmapScaled(fConverted, &source,
        engine->ScreenBitmap(), &screen);
    engine->Update();
}


/* static */
GFX::point
GameWindow::ToScreen(int x, int y)
{
    // the screen is logical size, so SDL already maps window coordinates
    // to the scaled screen
    return GFX::point(x / kWindowScale, y / kWindowScale);
}


void
DrawMouseCursor(Bitmap* bitmap, const GFX::point& point,
    uint32 outlineColor, uint32 fillColor)
{
    for (size_t row = 0; row < sizeof(kCursor) / sizeof(kCursor[0]); row++) {
        for (int column = 0; kCursor[row][column] != '\0'; column++) {
            const char c = kCursor[row][column];
            if (c == '#' || c == 'o') {
                bitmap->PutPixel(point.x + column, point.y + int(row),
                    c == '#' ? outlineColor : fillColor);
            }
        }
    }
}

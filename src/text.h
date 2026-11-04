// TrueType text via SDL_ttf. Fonts are embedded in the executable.
// HUD/menu text is queued in logical (640x360) coordinates and drawn after the game canvas
// at the real output resolution, so it stays crisp when the pixel art is scaled up.
#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include "canvas.h"

enum FontId { FONT_DISPLAY, FONT_UI, FONT_COUNT };
enum Align { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

struct TextStyle {
    FontId font = FONT_UI;
    double size = 10;                    // logical pixels
    Color color = rgb(255, 255, 255);
    Color color2 = 0;                    // gradient bottom colour (0 = solid)
    Color outline = rgb(10, 10, 20);
    double outlineW = 1.0;               // logical pixels, 0 = none
    bool shadow = true;
    double alpha = 1.0;
};

// set on threads that draw a strip of the HUD in parallel: their text.draw() calls are ignored, so the text
// is queued exactly once (by the main thread)
extern thread_local bool textMuted;

class TextLayer {
public:
    bool init();
    void shutdown();
    void beginFrame(SDL_Renderer* r, int frame);
    void draw(const std::string& s, double x, double y, const TextStyle& st, Align a = ALIGN_LEFT);
    double width(const std::string& s, const TextStyle& st);
    void flush(SDL_Renderer* r);
    // render text straight into a bitmap (used to bake signs and banners into sprites)
    Bitmap bitmap(const std::string& s, FontId f, int pixelSize, Color c, Color c2 = 0, Color outline = 0, int outlinePx = 0);

private:
    struct Entry { SDL_Texture* tex = nullptr; int w = 0, h = 0, pad = 0; int last = 0; };
    struct Cmd { std::string s; double x, y; TextStyle st; };
    SDL_Surface* renderSurface(const std::string& s, FontId f, int px, Color c, Color c2, Color outline, int opx, int* pad);
    TTF_Font* font(FontId f, int px, int outline);
    Entry* entry(SDL_Renderer* r, const std::string& s, const TextStyle& st);

    std::map<long long, TTF_Font*> fonts_;
    std::unordered_map<std::string, Entry> cache_;
    std::vector<Cmd> queue_;
    double scale_ = 2.0;
    int frame_ = 0;
    bool ok_ = false;
};

extern TextLayer text;

// convenience styles
TextStyle styleUI(double size, Color c = rgb(255, 255, 255));
TextStyle styleDisplay(double size, Color top, Color bottom = 0);

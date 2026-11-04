#include "text.h"
#include <SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include "font_data.h"   // generated at configure time from fonts/*.ttf

TextLayer text;

TextStyle styleUI(double size, Color c) {
    TextStyle s; s.font = FONT_UI; s.size = size; s.color = c;
    s.outlineW = size >= 9 ? 1.0 : 0.75;
    return s;
}
TextStyle styleDisplay(double size, Color top, Color bottom) {
    TextStyle s; s.font = FONT_DISPLAY; s.size = size; s.color = top; s.color2 = bottom;
    s.outlineW = std::max(1.0, size / 12);
    return s;
}

bool TextLayer::init() {
    if (TTF_Init() != 0) { SDL_Log("TTF_Init: %s", TTF_GetError()); return false; }
    ok_ = true;
    return true;
}

void TextLayer::shutdown() {
    for (auto& kv : cache_) if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    cache_.clear();
    for (auto& kv : fonts_) TTF_CloseFont(kv.second);
    fonts_.clear();
    if (ok_) TTF_Quit();
    ok_ = false;
}

TTF_Font* TextLayer::font(FontId f, int px, int outline) {
    long long key = ((long long)f << 40) | ((long long)px << 20) | outline;
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second;
    const unsigned char* data = f == FONT_DISPLAY ? embed_RussoOne_Regular : embed_ChakraPetch_BoldItalic;
    int size = f == FONT_DISPLAY ? int(sizeof(embed_RussoOne_Regular)) : int(sizeof(embed_ChakraPetch_BoldItalic));
    TTF_Font* fnt = TTF_OpenFontRW(SDL_RWFromConstMem(data, size), 1, px);
    if (!fnt) { SDL_Log("TTF_OpenFont: %s", TTF_GetError()); return nullptr; }
    TTF_SetFontHinting(fnt, TTF_HINTING_LIGHT);
    if (outline > 0) TTF_SetFontOutline(fnt, outline);
    fonts_[key] = fnt;
    return fnt;
}

SDL_Surface* TextLayer::renderSurface(const std::string& s, FontId f, int px, Color c, Color c2, Color outline, int opx, int* pad) {
    *pad = 0;
    TTF_Font* fn = font(f, px, 0);
    if (!fn || s.empty()) return nullptr;
    SDL_Surface* raw = TTF_RenderUTF8_Blended(fn, s.c_str(), SDL_Color{255, 255, 255, 255});
    if (!raw) return nullptr;
    SDL_Surface* fill = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(raw);
    // colourise with a vertical gradient
    SDL_LockSurface(fill);
    for (int y = 0; y < fill->h; ++y) {
        double t = fill->h > 1 ? double(y) / (fill->h - 1) : 0;
        // concentrate the gradient on the glyph body (skip ascender/descender space)
        t = std::clamp((t - 0.2) / 0.6, 0.0, 1.0);
        Color col = c2 ? lerpColor(c, c2, t) : c;
        Uint32* row = reinterpret_cast<Uint32*>(static_cast<Uint8*>(fill->pixels) + y * fill->pitch);
        for (int x = 0; x < fill->w; ++x) {
            unsigned a = row[x] >> 24;
            row[x] = (Uint32((a * (col >> 24)) / 255) << 24) | (col & 0x00FFFFFFu);
        }
    }
    SDL_UnlockSurface(fill);
    if (opx <= 0 || (outline >> 24) == 0) return fill;

    TTF_Font* fo = font(f, px, opx);
    SDL_Surface* rawO = fo ? TTF_RenderUTF8_Blended(fo, s.c_str(), SDL_Color{Uint8(outline >> 16), Uint8(outline >> 8), Uint8(outline), Uint8(outline >> 24)}) : nullptr;
    if (!rawO) return fill;
    SDL_Surface* out = SDL_ConvertSurfaceFormat(rawO, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(rawO);
    SDL_SetSurfaceBlendMode(fill, SDL_BLENDMODE_BLEND);
    SDL_Rect dst{opx, opx, fill->w, fill->h};
    SDL_BlitSurface(fill, nullptr, out, &dst);
    SDL_FreeSurface(fill);
    *pad = opx;
    return out;
}

void TextLayer::beginFrame(SDL_Renderer* r, int frame) {
    frame_ = frame;
    queue_.clear();
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(r, &ow, &oh);
    double sc = std::max(1.0, std::min(ow / double(SCREEN_W), oh / double(SCREEN_H)));
    if (std::fabs(sc - scale_) > 0.01) {             // window resized: re-rasterise everything
        for (auto& kv : cache_) if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
        cache_.clear();
        scale_ = sc;
    }
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (frame - it->second.last > 90) { if (it->second.tex) SDL_DestroyTexture(it->second.tex); it = cache_.erase(it); }
        else ++it;
    }
}

TextLayer::Entry* TextLayer::entry(SDL_Renderer* r, const std::string& s, const TextStyle& st) {
    char key[96];
    std::snprintf(key, sizeof key, "%d|%.2f|%08x|%08x|%08x|%.2f|", int(st.font), st.size, st.color, st.color2, st.outline, st.outlineW);
    std::string k = std::string(key) + s;
    auto it = cache_.find(k);
    if (it != cache_.end()) { it->second.last = frame_; return &it->second; }
    int px = std::max(4, int(std::lround(st.size * scale_)));
    int opx = st.outlineW > 0 ? std::max(1, int(std::lround(st.outlineW * scale_))) : 0;
    Entry e;
    e.last = frame_;
    SDL_Surface* surf = renderSurface(s, st.font, px, st.color, st.color2, st.outline, opx, &e.pad);
    if (surf) {
        e.tex = r ? SDL_CreateTextureFromSurface(r, surf) : nullptr;
        e.w = surf->w; e.h = surf->h;
        if (e.tex) {
            SDL_SetTextureBlendMode(e.tex, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(e.tex, SDL_ScaleModeLinear);
        }
        SDL_FreeSurface(surf);
    }
    return &(cache_[k] = e);
}

double TextLayer::width(const std::string& s, const TextStyle& st) {
    TTF_Font* fn = font(st.font, std::max(4, int(std::lround(st.size * scale_))), 0);
    int w = 0, h = 0;
    if (fn) TTF_SizeUTF8(fn, s.c_str(), &w, &h);
    return w / scale_;
}

thread_local bool textMuted = false;

void TextLayer::draw(const std::string& s, double x, double y, const TextStyle& st, Align a) {
    if (s.empty() || !ok_ || textMuted) return;
    if (a != ALIGN_LEFT) {
        double w = width(s, st);
        x -= a == ALIGN_CENTER ? w / 2 : w;
    }
    queue_.push_back({s, x, y, st});
}

void TextLayer::flush(SDL_Renderer* r) {
    for (Cmd& c : queue_) {
        Entry* e = entry(r, c.s, c.st);
        if (!e->tex) continue;
        double pad = e->pad / scale_;
        SDL_FRect dst{float(c.x - pad), float(c.y - pad), float(e->w / scale_), float(e->h / scale_)};
        if (c.st.shadow) {
            double off = std::max(1.0, c.st.size * 0.09);
            SDL_FRect sh{dst.x + float(off), dst.y + float(off), dst.w, dst.h};
            SDL_SetTextureColorMod(e->tex, 0, 0, 0);
            SDL_SetTextureAlphaMod(e->tex, Uint8(150 * c.st.alpha));
            SDL_RenderCopyF(r, e->tex, nullptr, &sh);
        }
        SDL_SetTextureColorMod(e->tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(e->tex, Uint8(255 * std::clamp(c.st.alpha, 0.0, 1.0)));
        SDL_RenderCopyF(r, e->tex, nullptr, &dst);
    }
    queue_.clear();
}

Bitmap TextLayer::bitmap(const std::string& s, FontId f, int pixelSize, Color c, Color c2, Color outline, int outlinePx) {
    int pad = 0;
    SDL_Surface* surf = renderSurface(s, f, pixelSize, c, c2, outline, outlinePx, &pad);
    if (!surf) return Bitmap(1, 1);
    Bitmap b(surf->w, surf->h);
    SDL_LockSurface(surf);
    for (int y = 0; y < surf->h; ++y) {
        const Uint32* row = reinterpret_cast<const Uint32*>(static_cast<Uint8*>(surf->pixels) + y * surf->pitch);
        for (int x = 0; x < surf->w; ++x) b.px[size_t(y) * b.w + x] = row[x];
    }
    SDL_UnlockSurface(surf);
    SDL_FreeSurface(surf);
    return b;
}

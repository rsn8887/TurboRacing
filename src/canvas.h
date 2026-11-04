// 32-bit software canvas: ARGB bitmaps with real alpha, mipmapped scaled blits, anti-aliased spans
#pragma once
#include "config.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>

struct Rect { int x, y, w, h; };

inline int alphaOf(Color c) { return int(c >> 24); }
inline Color withAlpha(Color c, int a) { return (c & 0x00FFFFFFu) | (uint32_t(a < 0 ? 0 : a > 255 ? 255 : a) << 24); }
inline Color lerpColor(Color a, Color b, double t) {
    if (t <= 0) return a;
    if (t >= 1) return b;
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255, aa = a >> 24;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255, ba = b >> 24;
    return (uint32_t(aa + (ba - aa) * t) << 24) | (uint32_t(ar + (br - ar) * t) << 16) |
           (uint32_t(ag + (bg - ag) * t) << 8) | uint32_t(ab + (bb - ab) * t);
}
inline Color shade(Color c, double f) {           // f<1 darker, f>1 brighter (keeps alpha)
    auto ch = [f](int v) { int r = int(v * f); return r < 0 ? 0 : (r > 255 ? 255 : r); };
    return (c & 0xFF000000u) | (uint32_t(ch((c >> 16) & 255)) << 16) | (uint32_t(ch((c >> 8) & 255)) << 8) | uint32_t(ch(c & 255));
}
// alpha-composite src over dst (dst assumed opaque)
inline Color blendOver(Color dst, Color src) {
    unsigned a = src >> 24;
    if (a == 255) return src;
    if (a == 0) return dst;
    unsigned ia = 255 - a;
    unsigned r = (((src >> 16) & 255) * a + ((dst >> 16) & 255) * ia) / 255;
    unsigned g = (((src >> 8) & 255) * a + ((dst >> 8) & 255) * ia) / 255;
    unsigned b = ((src & 255) * a + (dst & 255) * ia) / 255;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

class Bitmap {
public:
    int w = 0, h = 0;
    std::vector<Color> px;
    std::vector<Bitmap> mips;     // successively halved copies for smooth minification
    // bounding box of the non-transparent pixels [bx0,bx1) x [by0,by1); scaled blits skip the rest.
    // Defaults to the whole bitmap; tightened by computeBounds() (called from buildMips()).
    int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
    std::vector<int> rowL, rowR;  // per row: visible columns [rowL, rowR) (empty = whole width); set by computeBounds()

    Bitmap() = default;
    Bitmap(int w_, int h_, Color c = TRANSPARENT) : w(w_), h(h_), px(size_t(w_) * h_, c), bx1(w_), by1(h_) {}

    Color get(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? TRANSPARENT : px[size_t(y) * w + x]; }
    void set(int x, int y, Color c) { if (x >= 0 && y >= 0 && x < w && y < h) px[size_t(y) * w + x] = c; }
    void blend(int x, int y, Color c) { if (x >= 0 && y >= 0 && x < w && y < h) { Color& d = px[size_t(y) * w + x]; d = alphaOf(d) ? blendOver(d, c) : c; } }

    void fill(Color c);
    void fillRect(int x, int y, int rw, int rh, Color c);
    Bitmap flipped() const;
    Bitmap downsampled(int factor) const;   // box filter with premultiplied alpha
    void buildMips();             // also computes the opaque bounds of this bitmap and every mip
    void computeBounds();
    void bleed();                 // give fully transparent pixels next to visible ones their neighbours' colour
                                  // (alpha stays 0) so filtered scaling doesn't pull in black at sprite edges
    void draw(const Bitmap& src, int dx, int dy);   // unscaled alpha blit
};

// 8-bit map (e.g. the paint mask of a car frame) with box-filtered mips matching a Bitmap's mip chain
struct ByteMap {
    int w = 0, h = 0;
    std::vector<unsigned char> px;
    std::vector<ByteMap> mips;
    void buildMips();
};

class Canvas : public Bitmap {
public:
    Rect clip{0, 0, CANVAS_W, CANVAS_H};
    Canvas() : Bitmap(CANVAS_W, CANVAS_H, rgb(0, 0, 0)) {}

    // band: the part of the canvas this canvas may draw into at all (used to render horizontal strips of
    // one frame on several threads); every clip rectangle is limited to it
    Rect band{0, 0, CANVAS_W, CANVAS_H};
    void setClip(const Rect& r) {
        int x0 = std::max(r.x, band.x), y0 = std::max(r.y, band.y);
        int x1 = std::min(r.x + r.w, band.x + band.w), y1 = std::min(r.y + r.h, band.y + band.h);
        clip = {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
    }
    void resetClip() { setClip({0, 0, w, h}); }

    void rect(int x, int y, int rw, int rh, Color c);                 // clipped fill
    void rectBlend(int x, int y, int rw, int rh, Color c, double a);  // translucent fill
    void vgradRect(int x, int y, int rw, int rh, Color top, Color bottom, double a = 1.0);
    void span(int x0, int x1, int y, Color c);                        // [x0,x1) clipped
    void spanAA(double x0, double x1, int y, Color c);                // sub-pixel anti-aliased ends
    void vspanAA(int x, double y0, double y1, Color c, double clipBottom = 1e9);   // vertical, AA ends
    // scaled blit with alpha (picks the best mip level); only rows < clipBottom are drawn
    // mask + palette: recolour while drawing - where mask > 0 the grey value of the pixel is looked up in the
    // 256-entry palette and blended in by the mask (used for car paint)
    void blitScaled(const Bitmap& src, double dx, double dy, double dw, double dh,
                    bool flipX = false, double clipBottom = 1e9, double fog = 0, Color fogColor = 0, double alpha = 1.0,
                    const ByteMap* mask = nullptr, const Color* palette = nullptr);
    void blit(const Bitmap& src, double dx, double dy) { blitScaled(src, dx, dy, src.w, src.h); }
};

// Menus and the HUD are laid out in 640x360 layout units; this draws them on the full-resolution canvas.
struct UiCanvas {
    Canvas& cv;
    static int S(double v) { return int(std::lround(v * RENDER_SCALE)); }
    // edges are scaled separately so neighbouring boxes still meet exactly
    void rect(double x, double y, double w, double h, Color c) { cv.rect(S(x), S(y), S(x + w) - S(x), S(y + h) - S(y), c); }
    void rectBlend(double x, double y, double w, double h, Color c, double a) {
        cv.rectBlend(S(x), S(y), S(x + w) - S(x), S(y + h) - S(y), c, a);
    }
    void vgradRect(double x, double y, double w, double h, Color top, Color bottom, double a = 1.0) {
        cv.vgradRect(S(x), S(y), S(x + w) - S(x), S(y + h) - S(y), top, bottom, a);
    }
    void blitScaled(const Bitmap& src, double dx, double dy, double dw, double dh, bool flipX = false, double alpha = 1.0) {
        const double k = RENDER_SCALE;
        cv.blitScaled(src, dx * k, dy * k, dw * k, dh * k, flipX, 1e9, 0, 0, alpha);
    }
};

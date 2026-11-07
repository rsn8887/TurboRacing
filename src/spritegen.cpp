// Procedural scenery, effects and background generator (asset tool only)
#include "sprites.h"
#include "cars.h"
#include "painter.h"
#include "text.h"
#include <cmath>
#include <random>
#include <algorithm>
#include <array>

namespace {
constexpr int SS = 4;
std::mt19937 rng(4242);
double rnd(double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); }
const Color WHITE = rgb(255, 255, 255), BLACK = rgb(0, 0, 0);
Color alpha(Color c, double a) { return withAlpha(c, int(a * 255)); }

// TTF text rendered at supersampled resolution, for baking into sprites
Bitmap ttf(const std::string& s, FontId f, double size, Color c, Color c2 = 0, Color outline = 0, double outlinePx = 0) {
    return text.bitmap(s, f, int(size * SS), c, c2, outline, int(outlinePx * SS));
}

// soft radial glow (alpha falloff) added on top of whatever is there
void glow(Painter& p, double cx, double cy, double rx, double ry, Color c, double strength) {
    p.ellipse(cx, cy, rx, ry, Paint([=](double x, double y) {
        double dx = (x - cx) / rx, dy = (y - cy) / ry;
        double d = std::max(0.0, 1 - std::sqrt(dx * dx + dy * dy));
        return alpha(c, std::min(1.0, d * d * strength));
    }));
}

// ------------------------------------------------------------------ cars ----
// -------------------------------------------------------------- scenery -----
Bitmap makePalm() {
    Painter p(130, 172, SS, RENDER_SCALE, 2, 2);   // wide enough for the longest fronds, plus margins for the outline
    // trunk: curved, cylindrically shaded, with ring notches
    auto cx = [](double t) { return 61 + 16 * t * t; };
    auto hw = [](double t) { return 6.0 - 2.6 * t; };
    std::vector<double> poly;
    for (int i = 0; i <= 30; ++i) { double t = i / 30.0; poly.push_back(cx(t) - hw(t)); poly.push_back(172 - t * 130); }
    for (int i = 30; i >= 0; --i) { double t = i / 30.0; poly.push_back(cx(t) + hw(t)); poly.push_back(172 - t * 130); }
    p.poly(poly, Paint([&](double x, double y) {
        double t = (172 - y) / 130;
        double nx = std::clamp((x - cx(t)) / hw(t), -0.98, 0.98);
        Color c = lit(rgb(156, 108, 62), nx, -0.1, std::sqrt(1 - nx * nx), 0.4, 0.2, 10);
        double ring = std::fmod(y + 2 * nx + 200, 7.0);
        if (ring < 1.6) c = shade(c, 0.62);
        else if (ring < 2.6) c = shade(c, 1.12);
        return c;
    }));
    double tx = cx(1), ty = 42;
    // fronds: spine + leaflets, back ones darker
    struct Fr { double ang, len; bool back; };
    std::vector<Fr> fr = {{-150, 36, true}, {-115, 30, true}, {-60, 32, true}, {-25, 38, true},
                          {-175, 44, false}, {-140, 40, false}, {-95, 26, false}, {-45, 40, false}, {-5, 46, false},
                          {160, 34, false}, {25, 34, false}};
    for (const Fr& f : fr) {
        double rad = f.ang * M_PI / 180;
        auto pt = [&](double t, double& x, double& y) {
            x = tx + std::cos(rad) * f.len * t;
            y = ty + std::sin(rad) * f.len * t * 0.55 + t * t * 30;
        };
        Color top = f.back ? rgb(40, 120, 40) : rgb(80, 185, 60), bot = f.back ? rgb(20, 80, 30) : rgb(30, 120, 40);
        for (double t = 0.08; t <= 1.0; t += 0.035) {
            double x, y, x2, y2;
            pt(t, x, y); pt(t + 0.03, x2, y2);
            double dx = x2 - x, dy = y2 - y, l = std::sqrt(dx * dx + dy * dy) + 1e-9;
            double nx = -dy / l, ny = dx / l;
            double len = 3 + 10 * std::sin(t * M_PI) * (0.8 + 0.2 * hashNoise(t * 50, f.ang));
            for (int s = -1; s <= 1; s += 2) {
                double ex = x + (nx * s * 0.7 + dx / l * 0.5) * len, ey = y + (ny * s * 0.7 + 0.55) * len;
                p.line(x, y, ex, ey, 1.5, s < 0 ? top : bot);
            }
        }
        for (double t = 0; t < 1.0; t += 0.05) {
            double x, y, x2, y2;
            pt(t, x, y); pt(t + 0.05, x2, y2);
            p.line(x, y, x2, y2, 1.8 - t, rgb(110, 150, 50));
        }
    }
    for (auto c : {std::pair<double, double>{tx - 3, ty + 3}, {tx + 3, ty + 4}, {tx, ty + 6}})
        p.ellipse(c.first, c.second, 3.2, 3.2, sphere(c.first, c.second, 3.2, 3.2, rgb(120, 76, 30), 0.4, 0.5, 16));
    return p.finish(rgb(18, 40, 18), 1.0);   // outline; the canvas is wide enough that it is never cut off
}

Bitmap makePine(bool snow) {
    Painter p(80, 160, SS, RENDER_SCALE, 3, 2);   // margins: the lowest branch tips and the outline reach past 0..80
    p.rect(35.5, 124, 9, 36, cylX(35.5, 44.5, rgb(110, 72, 40)));
    for (int i = 0; i < 5; ++i) {
        double top = 2 + i * 22, bottom = top + 44, half = 11 + i * 7.5;
        std::vector<double> pts = {40, top, 40 + half, bottom - 3};
        int teeth = 4 + i;
        for (int k = 0; k <= teeth * 2; ++k) {
            double x = 40 + half - (2 * half) * k / (teeth * 2.0);
            pts.push_back(x); pts.push_back(k % 2 ? bottom + 2 : bottom - 3);
        }
        pts.push_back(40 - half); pts.push_back(bottom - 3);
        p.poly(pts, Paint([=](double x, double y) {
            double wAt = half * std::clamp((y - top) / (bottom - top), 0.05, 1.0);
            double nx = std::clamp((x - 40) / wAt, -0.98, 0.98);
            Color c = lit(rgb(34, 120, 52), nx, -0.25, std::sqrt(1 - nx * nx), 0.38, 0.12, 8);
            double n = hashNoise(x * 1.7, y * 2.3);
            if (n > 0.85) c = shade(c, 1.25); else if (n < 0.12) c = shade(c, 0.75);
            return c;
        }));
        if (snow) {
            std::vector<double> cap = {40, top};
            double capB = top + (bottom - top) * 0.45;
            for (int k = 0; k <= 8; ++k) {
                double t = k / 8.0;
                double x = 40 + half * 0.55 * (1 - 2 * t);
                cap.push_back(x); cap.push_back(capB + std::sin(t * 9 + i) * 2.5 - std::fabs(1 - 2 * t) * 3);
            }
            p.poly(cap, Paint([=](double x, double y) {
                double wAt = half * 0.55 * std::clamp((y - top) / (capB - top), 0.05, 1.0);
                double nx = std::clamp((x - 40) / wAt, -0.98, 0.98);
                return lit(rgb(246, 250, 255), nx, -0.6, std::sqrt(1 - nx * nx), 0.62, 0.25, 12);
            }));
            p.rect(40 - half + 3, bottom - 2, 2 * half - 6, 1.6, rgb(230, 238, 252));
        }
    }
    return p.finish(rgb(10, 36, 22), 1.0);
}

Bitmap makeBush() {
    Painter p(96, 48, SS, RENDER_SCALE, 2, 2);   // margins keep the outline from being cut off
    std::vector<std::array<double, 3>> balls;
    for (int i = 0; i < 16; ++i) balls.push_back({rnd(16, 80), rnd(20, 36), rnd(9, 16)});
    std::sort(balls.begin(), balls.end(), [](auto& a, auto& b) { return a[1] < b[1]; });
    for (auto& b : balls) {
        Color base = rgb(40 + int(rnd(0, 30)), 130 + int(rnd(0, 40)), 50);
        p.ellipse(b[0], b[1], b[2], b[2] * 0.85, Paint([=](double x, double y) {
            Color c = sphere(b[0], b[1], b[2], b[2] * 0.85, base, 0.38, 0.15, 8)(x, y);
            double n = hashNoise(x * 2, y * 2);
            return n > 0.8 ? shade(c, 1.2) : n < 0.15 ? shade(c, 0.8) : c;
        }));
    }
    p.rect(0, 46, 96, 2, TRANSPARENT);
    for (int i = 0; i < 8; ++i) {   // a few flowers
        double x = rnd(18, 78), y = rnd(14, 34);
        p.ellipse(x, y, 1.3, 1.3, rnd(0, 1) < 0.5 ? rgb(255, 120, 160) : rgb(255, 240, 120));
    }
    Bitmap b = p.finish(rgb(12, 50, 18), 1.0);
    // (+2: the painter's top margin)
    for (int y = (46 + 2) * RENDER_SCALE; y < b.h; ++y) for (int x = 0; x < b.w; ++x) b.px[size_t(y) * b.w + x] = TRANSPARENT;
    b.buildMips();
    return b;
}

Bitmap makeCactus() {
    Painter p(64, 120, SS, RENDER_SCALE, 2, 3);   // margins: room for the flower and the outline on top
    Color g = rgb(56, 156, 76);
    auto column = [&](double x, double y, double w, double h) {
        Paint ribbed([=](double px_, double py) {
            double nx = std::clamp(2 * (px_ - x) / w - 1, -0.98, 0.98);
            Color c = lit(g, nx, 0, std::sqrt(1 - nx * nx), 0.4, 0.35, 16);
            double rib = std::fmod((nx + 1) * 2.5, 1.0);
            if (rib < 0.12) c = shade(c, 0.72);
            if (hashNoise(px_ * 3, py * 1.5) > 0.96) c = rgb(250, 245, 210);   // spines
            return c;
        });
        p.rect(x, y + w / 2, w, h - w / 2, ribbed);
        p.ellipse(x + w / 2, y + w / 2, w / 2, w / 2, Paint([=](double px_, double py) {
            return sphere(x + w / 2, y + w / 2, w / 2, w / 2, g, 0.4, 0.35, 16)(px_, py);
        }));
    };
    column(25, 0, 15, 120);
    column(6, 26, 11, 38);
    p.rect(11, 57, 16, 9, cylY(57, 66, g));
    column(47, 40, 11, 32);
    p.rect(37, 64, 14, 9, cylY(64, 73, g));
    p.ellipse(32.5, 3, 3, 2, rgb(255, 120, 170));      // a flower on top
    return p.finish(rgb(16, 56, 26), 1.0);
}

Bitmap makeRock() {
    Painter p(96, 56, SS, RENDER_SCALE, 2, 2);   // margins keep the outline from being cut off
    std::vector<double> pts = {4, 56, 9, 30, 22, 13, 44, 4, 66, 6, 84, 18, 93, 38, 92, 56};
    p.poly(pts, Paint([](double x, double y) {
        double nx = (x - 48) / 46, ny = (y - 34) / 32;
        Color c = lit(rgb(150, 138, 124), nx, ny, std::sqrt(std::max(0.05, 1 - nx * nx - ny * ny)), 0.4, 0.1, 6);
        double n = hashNoise(x * 3.1, y * 2.7);
        c = shade(c, 0.88 + 0.24 * n);
        return c;
    }));
    // facets / cracks
    p.line(44, 4, 50, 24, 1.0, alpha(rgb(60, 50, 44), 0.7));
    p.line(50, 24, 84, 18, 1.0, alpha(rgb(60, 50, 44), 0.6));
    p.line(50, 24, 40, 56, 1.0, alpha(rgb(60, 50, 44), 0.5));
    p.line(22, 13, 50, 24, 0.8, alpha(rgb(220, 210, 190), 0.6));
    return p.finish(rgb(40, 32, 28), 1.0);
}

Bitmap makeLamp() {
    Painter p(64, 200, SS, RENDER_SCALE, 6, 2);   // margins keep the outline from being cut off
    Color metal = rgb(150, 156, 170);
    p.rect(8, 22, 5, 168, cylX(8, 13, metal, 0.35, 0.7, 30));
    p.roundRect(4.5, 184, 12, 16, 2, cylX(4.5, 16.5, shade(metal, 0.7), 0.35, 0.6, 24));
    for (double t = 0; t < 1; t += 0.05) {                         // curved arm
        double a0 = M_PI * (1 - t * 0.5), a1 = M_PI * (1 - (t + 0.05) * 0.5);
        p.line(30 + 20 * std::cos(a0), 30 - 18 * std::sin(a0), 30 + 20 * std::cos(a1), 30 - 18 * std::sin(a1), 3.2, metal);
    }
    p.line(30, 12, 50, 12, 3.2, metal);
    p.roundRect(40, 9, 22, 7, 3, cylY(9, 16, rgb(70, 72, 84), 0.35, 0.7, 30));
    p.roundRect(42, 15, 18, 3, 1.5, rgb(255, 250, 200));
    glow(p, 51, 20, 16, 10, rgb(255, 240, 160), 1.2);
    return p.finish(rgb(18, 18, 24), 1.0);
}

Bitmap makeBillboard(int kind) {
    Painter p(160, 106, SS, RENDER_SCALE, 5, 2);   // margins keep the outline from being cut off
    Color metal = rgb(150, 150, 162);
    p.rect(25, 60, 9, 46, cylX(25, 34, metal, 0.35, 0.6, 24));
    p.rect(126, 60, 9, 46, cylX(126, 135, metal, 0.35, 0.6, 24));
    p.roundRect(0, 0, 160, 68, 3, vgrad(0, 68, rgb(245, 245, 250), rgb(150, 150, 165)));
    Color bg0 = kind == 0 ? rgb(230, 30, 50) : kind == 1 ? rgb(40, 100, 230) : rgb(255, 220, 40);
    Color bg1 = kind == 0 ? rgb(120, 0, 20) : kind == 1 ? rgb(10, 30, 110) : rgb(230, 130, 0);
    p.rect(4, 4, 152, 60, vgrad(4, 64, bg0, bg1));
    if (kind == 2)      // diagonal shine stripes, kept inside the board face
        for (int i = 0; i < 6; ++i)
            p.poly({-10.0 + i * 30, 64, 6.0 + i * 30, 64, 26.0 + i * 30, 4, 10.0 + i * 30, 4},
                   Paint([](double x, double) { return x >= 4 && x <= 156 ? alpha(WHITE, 0.12) : TRANSPARENT; }));
    Color ol = rgb(10, 10, 20);
    if (kind == 0) {
        p.imageCentered(ttf("TURBO", FONT_DISPLAY, 21, rgb(255, 255, 255), rgb(200, 210, 255), ol, 1.2), 80, 20);
        p.imageCentered(ttf("RACING", FONT_DISPLAY, 21, rgb(255, 240, 110), rgb(255, 150, 20), ol, 1.2), 80, 44);
    } else if (kind == 1) {
        p.imageCentered(ttf("SDL2", FONT_DISPLAY, 26, WHITE, rgb(150, 210, 255), ol, 1.2), 80, 22);
        p.imageCentered(ttf("POWERED", FONT_UI, 17, WHITE, rgb(190, 220, 255), ol, 1.0), 80, 47);
    } else {
        p.imageCentered(ttf("TURBO", FONT_DISPLAY, 30, rgb(40, 40, 50), rgb(0, 0, 0), WHITE, 1.2), 80, 28);
        p.imageCentered(ttf("> > > > >", FONT_UI, 14, rgb(220, 20, 20), 0, 0, 0), 80, 52);
    }
    p.rect(4, 4, 152, 10, alpha(WHITE, 0.16));
    return p.finish(rgb(16, 16, 22), 1.0);
}

Bitmap makeBuilding(int kind) {
    int w = kind == 0 ? 92 : 124, h = kind == 0 ? 224 : 172;
    Painter p(w, h, SS, RENDER_SCALE, 2, 10);   // margins keep the outline from being cut off
    Color wall0 = kind == 0 ? rgb(70, 70, 120) : rgb(100, 56, 100), wall1 = kind == 0 ? rgb(30, 30, 60) : rgb(50, 26, 54);
    double side = w * 0.18;
    p.rect(0, 12, w - side, h - 12, vgrad(12, h, wall0, wall1));                       // front face
    p.poly({double(w) - side, 12, double(w), 4, double(w), double(h), double(w) - side, double(h)},
           vgrad(4, h, shade(wall0, 0.6), shade(wall1, 0.6)));                           // side face
    p.poly({0, 12, side, 4, double(w), 4, double(w) - side, 12}, shade(wall0, 1.3));      // roof
    p.rect(w * 0.3, 0, 2, 8, rgb(150, 150, 170));                                        // antenna
    p.ellipse(w * 0.3 + 1, 0.5, 1.5, 1.5, rgb(255, 40, 40));
    for (double y = 22; y < h - 14; y += 11)
        for (double x = 7; x < w - side - 8; x += 11) {
            double r = hashNoise(x * 7 + kind, y * 3);
            if (r < 0.45) {
                Color c = r < 0.38 ? rgb(255, 214, 110) : rgb(140, 220, 255);
                p.rect(x, y, 6, 7, vgrad(y, y + 7, c, shade(c, 0.75)));
                glow(p, x + 3, y + 3.5, 6, 6, c, 0.35);
            } else p.rect(x, y, 6, 7, rgb(24, 22, 40));
        }
    if (kind == 1) {
        p.roundRect(10, 26, w - side - 20, 18, 3, vgrad(26, 44, rgb(255, 80, 200), rgb(180, 0, 120)));
        glow(p, (w - side) / 2, 35, (w - side) / 2 + 6, 16, rgb(255, 80, 200), 0.6);
        p.imageCentered(ttf("NEON", FONT_DISPLAY, 13, WHITE, rgb(255, 210, 255), rgb(120, 0, 80), 0.8), (w - side) / 2, 35);
    } else {
        p.imageCentered(ttf("HOTEL", FONT_DISPLAY, 11, rgb(140, 255, 255), rgb(40, 180, 255), rgb(0, 30, 60), 0.8), (w - side) / 2, 17);
    }
    return p.finish(rgb(8, 8, 20), 1.0);
}

Bitmap makeSnowman() {
    Painter p(60, 86, SS, RENDER_SCALE, 2, 2);   // margins keep the outline from being cut off
    Color s = rgb(248, 250, 255);
    p.ellipse(30, 64, 21, 20, sphere(30, 64, 21, 20, s, 0.62, 0.2, 10));
    p.ellipse(30, 35, 15, 14, sphere(30, 35, 15, 14, s, 0.62, 0.2, 10));
    p.ellipse(30, 14, 11, 10, sphere(30, 14, 11, 10, s, 0.62, 0.2, 10));
    p.rect(21, 0, 18, 7, cylX(21, 39, rgb(40, 40, 46), 0.4, 0.5, 20));
    p.rect(16, 6, 28, 2.5, rgb(30, 30, 36));
    p.ellipse(26, 12, 1.4, 1.4, BLACK); p.ellipse(34, 12, 1.4, 1.4, BLACK);
    p.poly({30, 14, 41, 16.5, 30, 17.5}, vgrad(14, 17.5, rgb(255, 160, 40), rgb(220, 90, 0)));
    p.roundRect(18, 22, 24, 5, 2, cylY(22, 27, rgb(220, 30, 40)));
    p.rect(34, 26, 5, 11, cylX(34, 39, rgb(220, 30, 40)));
    for (double y : {33.0, 40.0, 47.0}) p.ellipse(30, y, 1.6, 1.6, rgb(30, 30, 30));
    return p.finish(rgb(110, 120, 160), 1.0);
}

Bitmap makeTurnSign() {   // chevrons pointing right
    Painter p(96, 82, SS, RENDER_SCALE, 2, 2);   // margins keep the outline from being cut off
    Color metal = rgb(150, 150, 162);
    p.rect(19, 48, 6, 34, cylX(19, 25, metal, 0.35, 0.6, 24));
    p.rect(71, 48, 6, 34, cylX(71, 77, metal, 0.35, 0.6, 24));
    p.roundRect(0, 0, 96, 53, 3, rgb(20, 20, 24));
    p.rect(3, 3, 90, 47, vgrad(3, 50, rgb(255, 230, 60), rgb(240, 160, 0)));
    for (int i = 0; i < 3; ++i) {
        double x = 11 + i * 26;
        p.poly({x, 8, x + 12, 8, x + 26, 26.5, x + 12, 45, x, 45, x + 14, 26.5}, vgrad(8, 45, rgb(40, 40, 48), rgb(5, 5, 8)));
        p.poly({x, 8, x + 12, 8, x + 13, 9.5, x + 1.5, 9.5}, alpha(WHITE, 0.3));
    }
    p.rect(3, 3, 90, 8, alpha(WHITE, 0.2));
    return p.finish(rgb(10, 10, 12), 1.0);
}

// warning signs: 0 = jump, 1 = road narrows, 2 = tunnel
Bitmap makeWarningSign(int kind) {
    Painter p(96, 86, SS, RENDER_SCALE, 2, 2);   // margins keep the outline from being cut off
    Color metal = rgb(150, 150, 162);
    p.rect(19, 52, 6, 34, cylX(19, 25, metal, 0.35, 0.6, 24));
    p.rect(71, 52, 6, 34, cylX(71, 77, metal, 0.35, 0.6, 24));
    Color bg0 = kind == 2 ? rgb(60, 140, 255) : kind == 3 ? rgb(255, 190, 70) : rgb(255, 230, 60);
    Color bg1 = kind == 2 ? rgb(10, 50, 160) : kind == 3 ? rgb(200, 100, 20) : rgb(240, 150, 0);
    Color ink = kind == 2 ? WHITE : rgb(20, 20, 26);
    p.roundRect(0, 0, 96, 57, 4, rgb(20, 20, 24));
    p.roundRect(3, 3, 90, 51, 3, vgrad(3, 54, bg0, bg1));
    const char* word = kind == 0 ? "JUMP!" : kind == 1 ? "NARROW" : kind == 2 ? "TUNNEL" : "DIRT";
    p.imageCentered(ttf(word, FONT_DISPLAY, kind == 0 ? 17 : 14, ink, ink, 0, 0), 48, 14);
    if (kind == 0) {            // ramp with a flying car
        p.poly({14, 50, 50, 50, 50, 38}, ink);
        p.line(54, 36, 80, 30, 1.2, ink);
        p.roundRect(58, 24, 18, 7, 2, ink);
        p.ellipse(62, 32, 2.4, 2.4, ink); p.ellipse(72, 31, 2.4, 2.4, ink);
    } else if (kind == 1) {     // converging road edges
        p.line(22, 51, 40, 30, 3, ink); p.line(74, 51, 56, 30, 3, ink);
        p.line(40, 30, 40, 26, 3, ink); p.line(56, 30, 56, 26, 3, ink);
    } else if (kind == 3) {     // bumpy road with flying stones
        for (double x = 14; x < 82; x += 2)
            p.line(x, 46 + 3 * std::sin(x * 0.35), x + 2, 46 + 3 * std::sin((x + 2) * 0.35), 2.6, ink);
        for (auto q : {std::pair<double, double>{30, 36}, {44, 31}, {58, 35}, {70, 30}}) p.ellipse(q.first, q.second, 2.2, 1.8, ink);
    } else {                    // tunnel arch
        p.ellipse(48, 46, 22, 18, ink);
        p.rect(26, 46, 44, 6, ink);
        p.ellipse(48, 46, 15, 12, bg1);
        p.rect(33, 46, 30, 6, bg1);
        p.poly({44, 52, 52, 52, 49.5, 38, 46.5, 38}, ink);
    }
    p.rect(3, 3, 90, 8, alpha(WHITE, 0.2));
    return p.finish(rgb(10, 10, 12), 1.0);
}

Bitmap makeGantry(bool start) {
    Painter p(320, 130, SS, RENDER_SCALE, 2, 2);   // margins keep the outline from being cut off
    Color metal = rgb(140, 146, 162);
    for (double px_ : {0.0, 304.0}) {                               // truss towers
        p.rect(px_, 0, 3.5, 130, cylX(px_, px_ + 3.5, metal, 0.35, 0.6, 24));
        p.rect(px_ + 12.5, 0, 3.5, 130, cylX(px_ + 12.5, px_ + 16, metal, 0.35, 0.6, 24));
        for (double y = 40; y < 130; y += 12) {
            p.line(px_ + 2, y, px_ + 14, y + 12, 1.4, shade(metal, 0.8));
            p.line(px_ + 14, y, px_ + 2, y + 12, 1.4, shade(metal, 0.65));
        }
    }
    if (start) {
        p.rect(14, 3, 292, 36, rgb(20, 20, 24));
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 36; ++x) {
                bool w = (x + y) & 1;
                double xx = 16 + x * 8, yy = 5 + y * 8;
                p.rect(xx, yy, 8, 8, vgrad(yy, yy + 8, w ? rgb(255, 255, 255) : rgb(40, 40, 46), w ? rgb(190, 190, 200) : rgb(10, 10, 14)));
            }
        p.roundRect(104, 1, 112, 40, 4, vgrad(1, 41, rgb(240, 40, 50), rgb(130, 0, 20)));
        p.imageCentered(ttf("START", FONT_DISPLAY, 26, WHITE, rgb(255, 220, 140), rgb(40, 0, 10), 1.3), 160, 21);
    } else {
        p.rect(14, 3, 292, 34, vgrad(3, 37, rgb(255, 235, 80), rgb(235, 150, 0)));
        p.rect(14, 3, 292, 3, rgb(20, 20, 24));
        p.rect(14, 34, 292, 3, rgb(20, 20, 24));
        p.imageCentered(ttf("CHECKPOINT", FONT_DISPLAY, 22, rgb(30, 30, 40), BLACK, rgb(255, 250, 220), 1.0), 160, 20);
    }
    p.rect(14, 3, 292, 7, alpha(WHITE, 0.18));
    return p.finish(rgb(10, 10, 16), 1.0);
}

Bitmap makeLight(Color c, bool on) {
    Painter p(32, 32, SS, RENDER_SCALE, 2, 2);   // margins keep the outline from being cut off
    p.ellipse(16, 16, 15.5, 15.5, sphere(16, 16, 15.5, 15.5, rgb(40, 40, 46), 0.4, 0.4, 20));
    Color lc = on ? c : shade(c, 0.25);
    p.ellipse(16, 16, 11.5, 11.5, sphere(16, 16, 11.5, 11.5, lc, on ? 0.75 : 0.4, 0.8, 30));
    if (on) glow(p, 14, 13, 9, 8, WHITE, 0.6);
    return p.finish(rgb(0, 0, 0), 0.6);
}

Bitmap makePuff(Color c, double strength) {
    Bitmap b(24, 24);
    for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x) {
            double dx = (x + 0.5 - 12) / 12, dy = (y + 0.5 - 12) / 12;
            double d = dx * dx + dy * dy;
            if (d >= 1) continue;
            double a = std::pow(1 - d, 1.5) * strength * (0.75 + 0.25 * hashNoise(x, y));
            double lift = 1.0 + 0.25 * (-dx - dy);
            b.px[size_t(y) * 24 + x] = withAlpha(shade(c, lift), int(a * 255));
        }
    b.buildMips();
    return b;
}

// ----------------------------------------------------------- backgrounds ----
struct Ridge {
    double a[6], ph[6]; int f[6];
    explicit Ridge(int baseFreq) { for (int i = 0; i < 6; ++i) { f[i] = baseFreq * (i + 1) + int(rnd(0, 2)); a[i] = 1.0 / (i + 1.3); ph[i] = rnd(0, 6.283); } }
    double operator()(double x, double w) const {
        double s = 0, n = 0;
        for (int i = 0; i < 6; ++i) { s += a[i] * std::sin(6.283 * f[i] * x / w + ph[i]); n += a[i]; }
        return 0.5 + 0.5 * s / n;
    }
};

// silhouette with slope lighting, anti-aliased top edge and optional snow caps
BgLayer ridgeLayer(int h, double minH, double maxH, int freq, Color top, Color bottom, double parallax, int yOff,
                   Color snow = 0, bool jagged = false) {
    // drawn at full resolution: X, Y are bitmap pixels, x, y layout units (RENDER_SCALE pixels each)
    const int R = RENDER_SCALE, BW = 1024 * R, BH = h * R;
    BgLayer L{Bitmap(BW, BH), parallax, yOff};
    Ridge r(freq);
    auto height = [&](double x) {
        double v = r(x, 1024);
        if (jagged) v = 1 - std::fabs(2 * v - 1);
        return minH + (maxH - minH) * v;
    };
    for (int X = 0; X < BW; ++X) {
        double x = (X + 0.5) / R;
        double hh = height(x);
        double topY = h - hh;
        for (int Y = std::max(0, int(std::floor(topY * R))); Y < BH; ++Y) {
            double y = double(Y) / R;
            // facet lighting: compare heights left/right over a window that widens with depth,
            // so the faces of each peak are lit (left) or shaded (right) smoothly
            double depthPx = y - topY;
            double k = 2 + std::max(0.0, depthPx) * 0.9;
            double slope = (height(x + k) - height(x - k)) / (2 * k);
            double light = 1.0 + 0.24 * std::tanh(slope * 2.5);
            Color c = shade(lerpColor(top, bottom, y / h), light);
            if (snow) {
                // snow cap thickness grows with the peak height, with a wavy lower edge
                double cap = std::clamp((hh - (minH + (maxH - minH) * 0.3)) / ((maxH - minH) * 0.7), 0.0, 1.0);
                double edge = cap * 0.5 * (0.8 + 0.2 * std::sin(x * 0.21) + 0.12 * std::sin(x * 0.77 + 1.3)) * hh;
                double dy = depthPx - edge;
                if (dy < 1.5) {
                    Color sc = shade(snow, std::clamp(0.86 + (light - 1) * 0.9, 0.7, 1.05));
                    c = lerpColor(sc, c, std::clamp(dy / 1.5, 0.0, 1.0));
                }
            }
            double cov = std::clamp(Y + 1 - topY * R, 0.0, 1.0);
            L.bmp.px[size_t(Y) * BW + X] = withAlpha(c, int(cov * 255));
        }
    }
    return L;
}

// layers drawn in layout pixels (sharp-edged windows, single-pixel stars): blown up to full resolution as
// exact RENDER_SCALE x RENDER_SCALE blocks
BgLayer upscaled(BgLayer L) {
    const int R = RENDER_SCALE;
    if (R == 1) return L;
    Bitmap b(L.bmp.w * R, L.bmp.h * R);
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) b.px[size_t(y) * b.w + x] = L.bmp.px[size_t(y / R) * L.bmp.w + x / R];
    L.bmp = std::move(b);
    return L;
}

BgLayer cloudLayer(int count, Color c, double parallax, int yOff) {
    Painter p(1024, 90, 2);
    for (int i = 0; i < count; ++i) {
        double cx = rnd(70, 950), cy = rnd(40, 62), w = rnd(40, 90);
        std::vector<std::array<double, 3>> puffs;
        for (int k = 0; k < 9; ++k) puffs.push_back({cx + rnd(-w, w) * 0.65, cy + rnd(-10, 4), rnd(12, 26)});
        std::sort(puffs.begin(), puffs.end(), [](auto& a, auto& b) { return a[2] < b[2]; });
        for (auto& q : puffs)
            p.ellipse(q[0], q[1], q[2], q[2] * 0.62, sphere(q[0], q[1], q[2], q[2] * 0.62, c, 0.72, 0.1, 6));
        p.rect(cx - w * 1.2, cy + 6, w * 2.4, 30, TRANSPARENT);
    }
    Bitmap b = p.finish(0, 0, false);
    // flatten cloud bottoms (rect above writes nothing; cut here)
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x)
            if (y > 66 * RENDER_SCALE) b.px[size_t(y) * b.w + x] = TRANSPARENT;
    return BgLayer{b, parallax, yOff};
}

BgLayer skylineLayer(int h, Color wall, bool lit_, double parallax, int yOff) {
    BgLayer L{Bitmap(1024, h), parallax, yOff};
    int x = 0;
    while (x < 1024) {
        int bw = int(rnd(24, 62)), bh = int(rnd(h * 0.3, h * 0.95));
        if (x + bw > 1024) bw = 1024 - x;
        for (int y = h - bh; y < h; ++y)
            for (int xx = x; xx < x + bw; ++xx) {
                double t = double(y - (h - bh)) / bh;
                Color c = lerpColor(shade(wall, 1.25), wall, t);
                if (xx == x) c = shade(c, 1.35);
                if (xx >= x + bw - 2) c = shade(c, 0.75);
                L.bmp.px[size_t(y) * 1024 + xx] = c;
            }
        if (lit_)
            for (int y = h - bh + 6; y < h - 4; y += 7)
                for (int wx = x + 4; wx < x + bw - 4; wx += 6)
                    if (rnd(0, 1) < 0.35) {
                        Color wc = rnd(0, 1) < 0.8 ? rgb(255, 210, 110) : rgb(120, 220, 255);
                        L.bmp.fillRect(wx, y, 3, 3, wc);
                        for (int gx = wx - 1; gx <= wx + 3; ++gx) L.bmp.blend(gx, y - 1, withAlpha(wc, 60));
                    }
        x += bw + int(rnd(0, 6));
    }
    return L;
}

BgLayer sunLayer(Color c1, Color c2, int radius, bool stripes, double parallax, int yOff) {
    const int R = RENDER_SCALE;
    int hgt = radius * 3;
    BgLayer L{Bitmap(1024 * R, hgt * R), parallax, yOff};
    double cx = 512, cy = hgt / 2.0;
    for (int Y = 0; Y < hgt * R; ++Y)
        for (int X = int((cx - radius * 1.6) * R); X < int((cx + radius * 1.6) * R); ++X) {
            double dx = (X + 0.5) / R - cx, dy = (Y + 0.5) / R - cy;
            double d = std::sqrt(dx * dx + dy * dy) / radius;
            Color out = TRANSPARENT;
            if (d < 1) {
                double t = (dy / radius + 1) / 2;
                bool gap = stripes && t > 0.52 && std::fmod(dy * 0.42 + t * t * 9, 3.0) < (t - 0.45) * 2.6;
                if (!gap) out = withAlpha(lerpColor(c1, c2, t), int(std::clamp((1 - d) * radius * R, 0.0, 1.0) * 255));
            }
            double g = std::max(0.0, 1 - (d - 0.9) / 0.7);          // halo
            if (d >= 0.9 && g > 0) {
                Color halo = withAlpha(c1, int(g * g * 90));
                out = (out >> 24) ? blendOver(halo | 0xFF000000u, out) : halo;
            }
            if (out >> 24) L.bmp.px[size_t(Y) * L.bmp.w + X] = out;
        }
    return L;
}

BgLayer starLayer(double parallax) {
    BgLayer L{Bitmap(1024, 170), parallax, -60};
    for (int i = 0; i < 260; ++i) {
        int x = int(rnd(1, 1023)), y = int(rnd(1, 169));
        Color c = rnd(0, 1) < 0.2 ? rgb(255, 250, 210) : rgb(190, 190, 240);
        L.bmp.set(x, y, c);
        if (rnd(0, 1) < 0.15) {
            L.bmp.set(x - 1, y, withAlpha(c, 110)); L.bmp.set(x + 1, y, withAlpha(c, 110));
            L.bmp.set(x, y - 1, withAlpha(c, 110)); L.bmp.set(x, y + 1, withAlpha(c, 110));
        }
    }
    return L;
}

BgLayer treeLineLayer(Color c, double parallax, int yOff) {
    Painter p(1024, 64, 2);
    for (double x = -4; x < 1028; x += rnd(5, 8)) {
        double h = rnd(26, 60), w = rnd(8, 13);
        p.poly({x, 64, x + w / 2, 64 - h, x + w, 64}, hgrad(x, x + w, shade(c, 1.2), shade(c, 0.8)));
    }
    p.rect(0, 54, 1024, 10, c);
    return BgLayer{p.finish(0, 0, false), parallax, yOff};
}

BgLayer mesaLayer() {
    Painter p(1024, 72, 2);
    double x = 0;
    while (x < 1024) {
        double w = rnd(40, 140), h = rnd(25, 66);
        if (rnd(0, 1) < 0.6) {
            Color base = rgb(196, 96, 54);
            p.poly({x, 72, x + 10, 72 - h, x + w - 10, 72 - h, x + w, 72},
                   Paint([=](double px_, double py) {
                       Color c = lerpColor(base, shade(base, 0.7), (py - (72 - h)) / h);
                       if (px_ > x + w * 0.62) c = shade(c, 0.78);             // shadowed side
                       if (std::fmod(py, 9.0) < 1.2) c = shade(c, 0.88);        // strata
                       return c;
                   }));
            p.rect(x + 10, 72 - h, w - 20, 3, rgb(232, 140, 90));
        }
        x += w + rnd(10, 60);
    }
    return BgLayer{p.finish(0, 0, false), 0.2, 0};
}

// =========================================================== stages 7-11 ====
// (generated after everything above, so the original stages' graphics stay exactly the same)

// ------------------------------------------------------------- future -----
Bitmap makeHoloPylon() {   // street light of the future: a slim white mast with a glowing emitter reaching over the road
    Painter p(64, 220, SS, RENDER_SCALE, 6, 2);
    Color white = rgb(236, 238, 240), teal = rgb(30, 190, 190);
    p.roundRect(4, 202, 13, 18, 2, cylX(4, 17, rgb(200, 204, 210), 0.45, 0.6, 24));
    p.rect(8, 26, 5, 178, cylX(8, 13, white, 0.45, 0.8, 30));
    for (double y = 60; y < 200; y += 34) p.rect(7, y, 7, 2.2, teal);              // teal rings
    p.poly({10, 30, 54, 18, 56, 22, 12, 36}, vgrad(18, 36, rgb(250, 250, 252), rgb(200, 204, 212)));   // arm
    p.roundRect(36, 18, 26, 6, 3, cylY(18, 24, rgb(214, 218, 224), 0.45, 0.7, 30));
    p.roundRect(38, 23, 22, 3, 1.5, rgb(210, 255, 250));
    glow(p, 49, 26, 14, 8, rgb(120, 240, 230), 0.8);
    return p.finish(rgb(60, 80, 90), 1.0);
}

Bitmap makeScifiTower() {   // white tower with teal glass, a saucer deck and a spire
    Painter p(110, 260, SS, RENDER_SCALE, 3, 14);
    Color hull0 = rgb(248, 248, 246), hull1 = rgb(206, 210, 214), glass = rgb(40, 170, 180), gold = rgb(240, 196, 90);
    p.poly({18, 260, 32, 60, 78, 60, 92, 260}, Paint([=](double x, double y) {         // tapering main body
        double t = (y - 60) / 200;
        double half = 23 + t * 14, cx = 55;
        double nx = std::clamp((x - cx) / half, -0.98, 0.98);
        Color c = lit(lerpColor(hull0, hull1, t), nx, -0.2, std::sqrt(1 - nx * nx), 0.55, 0.5, 20);
        if (std::fmod(y - 60, 18.0) < 1.0) c = shade(c, 0.86);                      // floor lines
        return c;
    }));
    for (double x : {40.0, 55.0, 70.0})                                              // tall glass strips
        p.poly({x - 3, 70, x + 3, 70, x + 3.6, 252, x - 3.6, 252}, Paint([=](double px_, double py) {
            Color c = lerpColor(shade(glass, 1.35), shade(glass, 0.8), (py - 70) / 182);
            return std::fabs(px_ - x) < 0.6 ? shade(c, 1.4) : c;
        }));
    p.ellipse(55, 58, 48, 11, sphere(55, 58, 48, 11, rgb(236, 238, 240), 0.55, 0.8, 30));   // saucer deck
    p.rect(9, 61, 92, 1.6, alpha(rgb(60, 70, 80), 0.6));
    for (int i = 0; i < 9; ++i) p.ellipse(15 + i * 10, 63, 2.0, 1.2, gold);
    p.ellipse(55, 44, 18, 14, sphere(55, 44, 18, 14, glass, 0.5, 0.9, 40));          // glass dome
    p.rect(54, 2, 2, 32, cylX(54, 56, rgb(230, 232, 236)));                          // spire
    p.ellipse(55, 2, 2.5, 2.5, rgb(255, 60, 60));
    return p.finish(rgb(70, 80, 90), 1.0);
}

Bitmap makeUfo() {   // a flying saucer hovering over the verge, with a tractor beam
    Painter p(120, 150, SS, RENDER_SCALE, 2, 2);
    // the beam: a soft translucent cone down to the ground
    p.poly({48, 34, 72, 34, 104, 150, 16, 150}, Paint([](double x, double y) {
        double t = (y - 34) / 116, half = 12 + t * 44;
        double e = 1 - std::fabs(x - 60) / half;
        return alpha(rgb(150, 255, 200), std::clamp(e, 0.0, 1.0) * (0.28 - 0.12 * t));
    }));
    p.ellipse(60, 148, 44, 4, alpha(rgb(150, 255, 200), 0.35));
    p.ellipse(60, 30, 56, 13, sphere(60, 30, 56, 13, rgb(170, 176, 196), 0.45, 0.9, 40));   // hull
    p.ellipse(60, 33, 40, 7, alpha(rgb(30, 32, 50), 0.6));
    p.ellipse(60, 20, 22, 15, sphere(60, 20, 22, 15, rgb(110, 255, 170), 0.5, 1.0, 50));   // canopy
    for (int i = 0; i < 7; ++i) {
        double x = 18 + i * 14;
        Color c = (i & 1) ? rgb(255, 230, 80) : rgb(255, 60, 120);
        p.ellipse(x, 33, 2.4, 1.8, c);
        glow(p, x, 33, 6, 5, c, 0.8);
    }
    return p.finish(rgb(10, 12, 24), 1.0);
}

Bitmap makeHoloSign() {   // a sleek billboard of the future
    Painter p(160, 106, SS, RENDER_SCALE, 5, 2);
    Color white = rgb(236, 238, 240);
    p.rect(25, 60, 7, 46, cylX(25, 32, white, 0.45, 0.7, 24));
    p.rect(128, 60, 7, 46, cylX(128, 135, white, 0.45, 0.7, 24));
    p.roundRect(0, 0, 160, 66, 6, vgrad(0, 66, rgb(250, 250, 248), rgb(214, 216, 220)));
    p.roundRect(4, 4, 152, 58, 4, Paint([](double, double y) {
        Color c = lerpColor(rgb(40, 190, 190), rgb(20, 110, 140), y / 66);
        return std::fmod(y, 3.0) < 0.8 ? shade(c, 0.92) : c;                         // faint scan lines
    }));
    Color ol = rgb(0, 50, 60);
    p.imageCentered(ttf("HYPER", FONT_DISPLAY, 24, WHITE, rgb(210, 255, 250), ol, 1.2), 80, 22);
    p.imageCentered(ttf("DRIVE 2099", FONT_UI, 15, rgb(255, 216, 110), rgb(250, 180, 60), ol, 1.0), 80, 47);
    p.rect(4, 4, 152, 10, alpha(WHITE, 0.16));
    return p.finish(rgb(50, 60, 70), 1.0);
}

// -------------------------------------------------------------- Brazil -----
Bitmap makeTropical() {   // a bush of big leaves and bright hibiscus flowers
    Painter p(100, 60, SS, RENDER_SCALE, 2, 2);
    for (int i = 0; i < 14; ++i) {                                        // broad leaves
        double a = rnd(-2.9, -0.25), len = rnd(22, 34);
        double bx = 50 + rnd(-14, 14), by = 58;
        double tx = bx + std::cos(a) * len, ty = by + std::sin(a) * len * 0.9;
        double nx = -(ty - by) / len * 0.32, ny = (tx - bx) / len * 0.32;
        Color g = rgb(20 + int(rnd(0, 30)), 140 + int(rnd(0, 60)), 60);
        p.poly({bx, by, (bx + tx) / 2 + nx * len * 0.5, (by + ty) / 2 + ny * len * 0.5, tx, ty,
                (bx + tx) / 2 - nx * len * 0.5, (by + ty) / 2 - ny * len * 0.5},
               hgrad(std::min(bx, tx), std::max(bx, tx) + 1, shade(g, 1.2), shade(g, 0.8)));
        p.line(bx, by, tx, ty, 0.6, shade(g, 0.6));
    }
    Color petals[] = {rgb(255, 40, 90), rgb(255, 140, 0), rgb(255, 220, 30), rgb(240, 40, 220), rgb(255, 80, 40)};
    for (int i = 0; i < 9; ++i) {                                         // hibiscus flowers
        double x = rnd(16, 84), y = rnd(14, 44), r = rnd(5, 8);
        Color c = petals[i % 5];
        for (int k = 0; k < 5; ++k) {
            double a = k * 1.2566 + rnd(0, 0.4);
            double px_ = x + std::cos(a) * r * 0.6, py = y + std::sin(a) * r * 0.6;
            p.ellipse(px_, py, r * 0.62, r * 0.5, sphere(px_, py, r * 0.62, r * 0.5, c, 0.55, 0.3, 10));
        }
        p.ellipse(x, y, r * 0.25, r * 0.25, rgb(255, 250, 200));
    }
    return p.finish(rgb(20, 40, 20), 1.0);
}

Bitmap makeCarnival() {   // a carnival standard: a pole with a giant fan of feathers and ribbons
    Painter p(70, 180, SS, RENDER_SCALE, 3, 2);
    p.rect(33, 50, 4, 130, cylX(33, 37, rgb(240, 200, 60), 0.4, 0.8, 30));
    Color feathers[] = {rgb(255, 30, 90), rgb(255, 150, 0), rgb(255, 235, 0), rgb(40, 220, 80), rgb(0, 170, 255), rgb(170, 60, 255)};
    for (int i = 0; i < 13; ++i) {                                        // fan of feathers
        double a = -M_PI + 0.12 + i * (M_PI - 0.24) / 12;
        double len = 30 + 4 * std::sin(i * 1.3);
        double tx = 35 + std::cos(a) * len, ty = 52 + std::sin(a) * len;
        Color c = feathers[i % 6];
        double nx = -std::sin(a) * 4.5, ny = std::cos(a) * 4.5;
        p.poly({35, 52, (35 + tx) / 2 + nx, (52 + ty) / 2 + ny, tx, ty, (35 + tx) / 2 - nx, (52 + ty) / 2 - ny},
               Paint([=](double x, double y) {
                   double d = std::hypot(x - 35, y - 52) / len;
                   return lerpColor(shade(c, 0.75), shade(c, 1.25), d);
               }));
        p.line(35, 52, tx, ty, 0.5, alpha(WHITE, 0.6));
    }
    p.ellipse(35, 52, 7, 7, sphere(35, 52, 7, 7, rgb(255, 215, 40), 0.5, 0.9, 30));   // golden boss
    for (int i = 0; i < 4; ++i) {                                          // ribbons
        Color c = feathers[(i * 2 + 1) % 6];
        double x0 = 35 + (i - 1.5) * 3;
        for (double y = 60; y < 120; y += 2) {
            double xx = x0 + std::sin(y * 0.15 + i) * (3 + (y - 60) * 0.12);
            double xn = x0 + std::sin((y + 2) * 0.15 + i) * (3 + (y + 2 - 60) * 0.12);
            p.line(xx, y, xn, y + 2, 2.0, c);
        }
    }
    return p.finish(rgb(30, 10, 30), 1.0);
}

Bitmap makeFavela() {   // stacked, brightly painted little houses on the hillside
    Painter p(140, 170, SS, RENDER_SCALE, 2, 8);
    Color walls[] = {rgb(255, 90, 80), rgb(255, 200, 40), rgb(60, 200, 120), rgb(70, 150, 255), rgb(255, 140, 200),
                     rgb(255, 150, 40), rgb(150, 230, 90), rgb(120, 220, 230), rgb(200, 110, 255)};
    int k = 0;
    double y = 170;
    for (int row = 0; row < 4; ++row) {
        double hgt = rnd(32, 44);
        double x = row * rnd(4, 9);
        double right = 140 - row * rnd(6, 14);
        while (x < right - 12) {
            double w = std::min(right - x, rnd(24, 40));
            Color c = walls[(k++ * 4 + row) % 9];
            p.rect(x, y - hgt, w, hgt, vgrad(y - hgt, y, shade(c, 1.08), shade(c, 0.86)));
            p.rect(x, y - hgt, w, 2.5, shade(c, 0.7));                                  // flat roof edge
            for (double wx = x + 4; wx < x + w - 7; wx += 10) {
                p.rect(wx, y - hgt + 9, 5, 7, rgb(40, 34, 50));
                p.rect(wx, y - hgt + 9, 5, 1.5, rgb(240, 240, 240));
            }
            if (hashNoise(x, row) < 0.4) {                                              // blue water tank
                p.rect(x + w * 0.55, y - hgt - 7, 8, 7, cylX(x + w * 0.55, x + w * 0.55 + 8, rgb(40, 120, 220)));
                p.ellipse(x + w * 0.55 + 4, y - hgt - 7, 4, 1.4, rgb(90, 170, 255));
            }
            p.rect(x + w - 1, y - hgt, 1, hgt, alpha(BLACK, 0.35));
            x += w;
        }
        y -= hgt;
    }
    return p.finish(rgb(30, 20, 30), 1.0);
}

// --------------------------------------------------------------- Paris -----
Bitmap makeParisLamp() {   // cast-iron street lamp with a lantern on a curled arm
    Painter p(64, 200, SS, RENDER_SCALE, 6, 2);
    Color iron = rgb(34, 64, 52);
    p.roundRect(3, 180, 15, 20, 3, cylX(3, 18, iron, 0.38, 0.6, 24));         // fluted base
    p.rect(6, 172, 9, 10, cylX(6, 15, iron, 0.38, 0.6, 24));
    p.rect(8.5, 30, 4, 146, cylX(8.5, 12.5, iron, 0.38, 0.7, 26));
    for (double y : {60.0, 120.0}) p.ellipse(10.5, y, 4, 2, cylX(6.5, 14.5, iron, 0.38, 0.6, 24));
    for (double t = 0; t < 1; t += 0.04) {                                     // curled arm
        double a0 = M_PI * (1 - t * 0.55), a1 = M_PI * (1 - (t + 0.04) * 0.55);
        p.line(30 + 20 * std::cos(a0), 34 - 16 * std::sin(a0), 30 + 20 * std::cos(a1), 34 - 16 * std::sin(a1), 2.6, iron);
    }
    p.line(30, 18, 48, 18, 2.6, iron);
    p.line(48, 18, 48, 22, 1.6, iron);
    p.poly({40, 22, 56, 22, 53, 42, 43, 42}, vgrad(22, 42, rgb(255, 236, 170), rgb(255, 196, 90)));   // lantern glass
    p.poly({38, 22, 58, 22, 52, 16, 44, 16}, iron);                                                   // lantern cap
    p.rect(43, 42, 10, 2.5, iron);
    for (double x : {45.0, 48.0, 51.0}) p.line(x, 22, x - (x - 48) * 0.15, 42, 0.6, alpha(iron, 0.8));
    glow(p, 48, 32, 18, 16, rgb(255, 220, 140), 1.0);
    return p.finish(rgb(10, 20, 16), 1.0);
}

Bitmap makeHaussmann() {   // cream limestone apartment block with a zinc mansard roof
    const int w = 130, h = 230;
    Painter p(w, h, SS, RENDER_SCALE, 2, 10);
    const double side = w * 0.16, roofY = 52;
    Color stone = rgb(232, 218, 190), zinc = rgb(110, 124, 146);
    p.rect(0, roofY, w - side, h - roofY, Paint([=](double, double y) {       // front face
        Color c = lerpColor(stone, shade(stone, 0.86), (y - roofY) / (h - roofY));
        if (std::fmod(y - roofY, 6.0) < 0.5) c = shade(c, 0.92);                // stone courses
        return c;
    }));
    p.poly({w - side, roofY, double(w), roofY - 6, double(w), double(h), w - side, double(h)},
           vgrad(roofY, h, shade(stone, 0.7), shade(stone, 0.6)));              // side face
    // mansard roof with dormers and chimneys
    p.poly({-1, roofY, 8, 20, w - side - 8, 20, w - side + 1, roofY}, vgrad(20, roofY, shade(zinc, 1.15), shade(zinc, 0.8)));
    p.poly({w - side + 1, roofY, w - side - 8, 20, double(w) - 6, 14, double(w), roofY - 6}, shade(zinc, 0.62));
    for (double x = 14; x < w - side - 16; x += 22) {
        p.rect(x, 30, 11, 16, rgb(232, 226, 210));
        p.poly({x - 1, 31, x + 5.5, 25, x + 12, 31}, shade(zinc, 0.9));
        p.rect(x + 2.5, 34, 6, 10, rgb(50, 60, 80));
    }
    for (double x : {20.0, 64.0, 98.0}) {
        p.rect(x, 6, 9, 16, cylX(x, x + 9, rgb(190, 120, 90), 0.45, 0.2, 8));
        for (double cx : {x + 2.0, x + 6.0}) p.rect(cx, 3, 2, 4, rgb(150, 90, 70));
    }
    p.rect(-1, roofY - 2, w - side + 2, 4, rgb(236, 228, 210));                 // cornice
    // windows: tall French windows, balconies on the 2nd and 5th floors
    for (int f = 0; f < 5; ++f) {
        double y = roofY + 10 + f * 33;
        bool balcony = f == 0 || f == 3;
        for (double x = 8; x < w - side - 12; x += 19) {
            p.rect(x - 1.5, y - 2, 13, 2, shade(stone, 0.84));
            p.rect(x, y, 10, 22, vgrad(y, y + 22, rgb(70, 86, 110), rgb(40, 46, 62)));
            p.rect(x + 4.5, y, 1, 22, rgb(220, 214, 200));
            p.rect(x, y + 8, 10, 1, rgb(220, 214, 200));
            if (!balcony) {
                p.rect(x - 1, y + 16, 12, 6, alpha(rgb(20, 20, 24), 0.85));         // small railing
                for (double bx = x; bx < x + 10; bx += 2) p.rect(bx, y + 16, 0.6, 6, rgb(60, 60, 70));
            }
        }
        if (balcony) {                                                           // continuous iron balcony
            p.rect(2, y + 21, w - side - 6, 2, rgb(30, 30, 36));
            p.rect(2, y + 13, w - side - 6, 1.2, rgb(30, 30, 36));
            for (double bx = 3; bx < w - side - 4; bx += 2.2) p.rect(bx, y + 13, 0.6, 8, rgb(40, 40, 48));
        }
    }
    // shop front on the ground floor
    p.rect(0, h - 22, w - side, 22, rgb(40, 50, 70));
    for (double x = 6; x < w - side - 20; x += 26) p.rect(x, h - 18, 20, 18, vgrad(h - 18, h, rgb(255, 220, 150), rgb(200, 150, 90)));
    return p.finish(rgb(40, 36, 32), 1.0);
}

Bitmap makeCafe() {   // a pavement cafe: striped awning, little round tables
    Painter p(150, 90, SS, RENDER_SCALE, 3, 4);
    p.rect(0, 20, 150, 70, vgrad(20, 90, rgb(70, 40, 40), rgb(40, 22, 22)));              // shop front
    p.rect(8, 34, 134, 40, vgrad(34, 74, rgb(255, 214, 140), rgb(200, 140, 80)));        // lit window
    for (double x = 8; x < 142; x += 26.8) p.rect(x, 34, 2, 40, rgb(60, 34, 30));
    // striped awning
    p.poly({-3, 18, 153, 18, 160, 36, -10, 36}, Paint([](double x, double y) {
        double u = (x + 10 + (36 - y) * 0.4) / 10;
        return (int(std::floor(u)) & 1) ? rgb(240, 240, 236) : rgb(200, 30, 40);
    }));
    for (double x = -10; x < 160; x += 10) p.ellipse(x + 5, 36, 5, 2.5, (int((x + 10) / 10) & 1) ? rgb(240, 240, 236) : rgb(200, 30, 40));
    p.rect(30, 2, 90, 15, rgb(30, 40, 60));
    p.imageCentered(ttf("CAFE", FONT_DISPLAY, 13, rgb(255, 230, 160), rgb(255, 200, 110), 0, 0), 75, 9.5);
    // tables and chairs on the pavement
    for (double x : {22.0, 75.0, 128.0}) {
        p.rect(x - 1, 74, 2, 14, rgb(40, 40, 46));
        p.ellipse(x, 74, 9, 2.4, cylY(71.6, 76.4, rgb(220, 220, 214)));
        p.rect(x - 7, 87, 14, 2, rgb(40, 40, 46));
        for (double cx : {x - 15, x + 12}) {
            p.rect(cx, 70, 3, 18, rgb(150, 90, 50));
            p.rect(cx - 1, 78, 5, 2, rgb(170, 110, 60));
        }
    }
    return p.finish(rgb(26, 16, 14), 1.0);
}

Bitmap makeTreeRound() {   // a broad-leaved tree (plane tree / oak)
    Painter p(100, 150, SS, RENDER_SCALE, 2, 2);
    p.poly({44, 150, 46, 80, 54, 80, 56, 150}, Paint([](double x, double y) {
        double nx = std::clamp((x - 50) / 6, -0.98, 0.98);
        Color c = lit(rgb(120, 96, 70), nx, 0, std::sqrt(1 - nx * nx), 0.4, 0.1, 6);
        return hashNoise(x * 2, y * 0.7) > 0.8 ? shade(c, 0.8) : c;
    }));
    p.line(50, 100, 34, 76, 3, rgb(110, 88, 64));
    p.line(50, 96, 68, 74, 3, rgb(110, 88, 64));
    std::vector<std::array<double, 3>> balls;
    for (int i = 0; i < 22; ++i) {
        double a = rnd(0, 6.283), r = rnd(0, 1);
        balls.push_back({50 + std::cos(a) * r * 32, 50 + std::sin(a) * r * 34, rnd(13, 20)});
    }
    std::sort(balls.begin(), balls.end(), [](auto& a, auto& b) { return a[1] < b[1]; });
    for (auto& b : balls) {
        Color base = rgb(50 + int(rnd(0, 30)), 130 + int(rnd(0, 40)), 50 + int(rnd(0, 20)));
        p.ellipse(b[0], b[1], b[2], b[2] * 0.9, Paint([=](double x, double y) {
            Color c = sphere(b[0], b[1], b[2], b[2] * 0.9, base, 0.38, 0.12, 8)(x, y);
            double n = hashNoise(x * 2.2, y * 2.2);
            return n > 0.82 ? shade(c, 1.2) : n < 0.15 ? shade(c, 0.78) : c;
        }));
    }
    return p.finish(rgb(14, 40, 18), 1.0);
}

// ------------------------------------------------------------- Germany -----
Bitmap makeFachwerk() {   // half-timbered house with a steep tiled roof and flower boxes
    const int w = 140, h = 200;
    Painter p(w, h, SS, RENDER_SCALE, 3, 4);
    Color plaster = rgb(244, 236, 220), beam = rgb(86, 52, 32), tile = rgb(170, 60, 40);
    const double wallTop = 92, side = 22;
    p.rect(0, wallTop, w - side, h - wallTop, vgrad(wallTop, h, plaster, shade(plaster, 0.9)));
    p.poly({w - side, wallTop, double(w), wallTop - 8, double(w), double(h), w - side, double(h)}, shade(plaster, 0.72));
    // timber frame: posts, rails and diagonal braces
    for (double x = 0; x <= w - side + 0.1; x += (w - side) / 4) p.rect(std::min(x, w - side - 4.0), wallTop, 4, h - wallTop, beam);
    for (double y : {wallTop, wallTop + 52.0, double(h) - 4}) p.rect(0, y, w - side, 4, beam);
    for (int i = 0; i < 4; ++i) {
        double x0 = i * (w - side) / 4 + 4, x1 = (i + 1) * (w - side) / 4;
        if (i % 2 == 0) { p.line(x0, wallTop + 4, x1, wallTop + 52, 2.6, beam); p.line(x0, h - 4, x1, wallTop + 56, 2.6, beam); }
        else            { p.line(x0, wallTop + 52, x1, wallTop + 4, 2.6, beam); p.line(x0, wallTop + 56, x1, h - 4, 2.6, beam); }
    }
    // windows with shutters and flower boxes
    for (int i = 0; i < 4; ++i) {
        double cx = (i + 0.5) * (w - side) / 4;
        for (double y : {wallTop + 16, wallTop + 70}) {
            if (y > wallTop + 60 && i == 1) {                                         // the front door
                p.roundRect(cx - 8, wallTop + 64, 16, h - wallTop - 68, 7, vgrad(wallTop + 64, h, rgb(110, 60, 36), rgb(70, 36, 20)));
                continue;
            }
            p.rect(cx - 7, y, 14, 18, vgrad(y, y + 18, rgb(150, 190, 220), rgb(60, 80, 110)));
            p.rect(cx - 0.6, y, 1.2, 18, plaster);
            p.rect(cx - 7, y + 8, 14, 1.2, plaster);
            p.rect(cx - 12, y, 5, 18, rgb(40, 110, 60));
            p.rect(cx + 7, y, 5, 18, rgb(40, 110, 60));
            p.rect(cx - 9, y + 18, 18, 4, rgb(120, 70, 40));
            for (double fx = cx - 8; fx < cx + 8; fx += 3) p.ellipse(fx + 1, y + 17, 1.6, 1.6, hashNoise(fx, y) < 0.5 ? rgb(230, 30, 50) : rgb(255, 120, 160));
        }
    }
    // steep roof (gable end facing us) with a little dormer
    p.poly({-6, wallTop + 2, (w - side) / 2.0, 6, w - side + 6, wallTop + 2}, Paint([=](double x, double y) {
        Color c = lerpColor(shade(tile, 1.15), shade(tile, 0.8), (y - 6) / (wallTop - 4));
        if (std::fmod(y, 5.0) < 0.8) c = shade(c, 0.7);                               // rows of tiles
        if (std::fmod(x + std::floor(y / 5) * 3, 6.0) < 0.5) c = shade(c, 0.85);
        return c;
    }));
    p.poly({(w - side) / 2.0, 6, double(w), wallTop - 10, double(w), wallTop - 6, w - side + 6, wallTop + 2}, shade(tile, 0.62));
    p.poly({(w - side) / 2.0 - 30, wallTop - 30, (w - side) / 2.0 + 30, wallTop - 30, (w - side) / 2.0 + 18, wallTop - 52,
            (w - side) / 2.0 - 18, wallTop - 52}, Paint([=](double x, double) {             // half-timbered gable
        double rel = std::fmod(x - (w - side) / 2.0 + 60, 12.0);
        return rel < 2 ? beam : plaster;
    }));
    p.rect((w - side) / 2.0 - 30, wallTop - 32, 60, 3, beam);
    p.rect((w - side) / 2.0 - 5, wallTop - 48, 10, 12, vgrad(wallTop - 48, wallTop - 36, rgb(150, 190, 220), rgb(60, 80, 110)));
    p.rect(w - side - 30, 22, 9, 24, cylX(w - side - 30, w - side - 21, rgb(150, 70, 50), 0.4, 0.2, 8));   // chimney
    return p.finish(rgb(40, 24, 16), 1.0);
}

Bitmap makeAutobahnSign() {   // blue motorway direction sign
    Painter p(150, 110, SS, RENDER_SCALE, 3, 2);
    Color metal = rgb(150, 150, 162);
    p.rect(22, 60, 6, 50, cylX(22, 28, metal, 0.35, 0.6, 24));
    p.rect(122, 60, 6, 50, cylX(122, 128, metal, 0.35, 0.6, 24));
    p.roundRect(0, 0, 150, 66, 4, rgb(240, 240, 244));
    p.roundRect(3, 3, 144, 60, 3, vgrad(3, 63, rgb(30, 90, 200), rgb(16, 56, 150)));
    Color ink = WHITE;
    p.imageCentered(ttf("BERLIN", FONT_UI, 15, ink, ink, 0, 0), 56, 17);
    p.imageCentered(ttf("240", FONT_UI, 15, ink, ink, 0, 0), 122, 17);
    p.imageCentered(ttf("HAMBURG", FONT_UI, 15, ink, ink, 0, 0), 56, 40);
    p.imageCentered(ttf("95", FONT_UI, 15, ink, ink, 0, 0), 122, 40);
    p.rect(10, 52, 130, 1.4, ink);
    p.poly({120, 55, 134, 55, 134, 52, 140, 57.5, 134, 63, 134, 60, 120, 60}, ink);   // exit arrow
    p.rect(3, 3, 144, 8, alpha(WHITE, 0.15));
    return p.finish(rgb(10, 14, 30), 1.0);
}

Bitmap makeWindTurbine() {   // a tall, slender wind turbine
    Painter p(120, 300, SS, RENDER_SCALE, 3, 4);         // wide enough for the blades in every direction
    Color white = rgb(236, 240, 244);
    const double hx = 60, hy = 61;
    p.poly({hx - 4, 300, hx - 2, 64, hx + 2, 64, hx + 4, 300}, cylX(hx - 4, hx + 4, white, 0.5, 0.4, 16));   // tower
    p.roundRect(hx - 3, 56, 16, 10, 4, cylY(56, 66, white, 0.5, 0.5, 20));                                  // nacelle
    p.ellipse(hx, hy, 4, 4, sphere(hx, hy, 4, 4, white, 0.55, 0.6, 20));                                    // hub
    for (int i = 0; i < 3; ++i) {                                                                           // blades
        double a = -M_PI / 2 + 0.35 + i * 2 * M_PI / 3;
        double tx = hx + std::cos(a) * 54, ty = hy + std::sin(a) * 54;
        double nx = -std::sin(a), ny = std::cos(a);
        p.poly({hx + nx * 2.4, hy + ny * 2.4, tx + nx * 0.6, ty + ny * 0.6, tx - nx * 0.6, ty - ny * 0.6, hx - nx * 1.2, hy - ny * 1.2},
               hgrad(std::min(hx, tx) - 3, std::max(hx, tx) + 3, white, shade(white, 0.8)));
        if (i == 0) p.line(tx - std::cos(a) * 6, ty - std::sin(a) * 6, tx, ty, 1.0, rgb(220, 40, 40));   // red tip
    }
    return p.finish(rgb(60, 66, 80), 0.8);
}

// ------------------------------------------------------------ Tuscany -----
Bitmap makeCypress() {   // tall, narrow, flame-shaped Italian cypress
    Painter p(40, 180, SS, RENDER_SCALE, 2, 2);
    p.rect(18, 166, 4, 14, cylX(18, 22, rgb(100, 76, 54)));
    auto half = [](double y) {                                 // outline: pointed top, widest at 2/3
        double t = y / 170;
        return 15 * std::pow(std::sin(std::min(1.0, t * 1.25) * M_PI / 2), 1.6) * (t > 0.8 ? 1 - (t - 0.8) * 1.6 : 1.0);
    };
    std::vector<double> pts;
    for (int i = 0; i <= 40; ++i) { double y = i * 170.0 / 40; pts.push_back(20 + half(y) + std::sin(y * 0.9) * 0.8); pts.push_back(y); }
    for (int i = 40; i >= 0; --i) { double y = i * 170.0 / 40; pts.push_back(20 - half(y) + std::sin(y * 1.1 + 1) * 0.8); pts.push_back(y); }
    p.poly(pts, Paint([=](double x, double y) {
        double hw = std::max(1.0, half(y));
        double nx = std::clamp((x - 20) / hw, -0.98, 0.98);
        Color c = lit(rgb(34, 84, 44), nx, -0.2, std::sqrt(1 - nx * nx), 0.4, 0.1, 6);
        double n = hashNoise(x * 2.5, y * 1.3);
        return n > 0.82 ? shade(c, 1.22) : n < 0.18 ? shade(c, 0.75) : c;
    }));
    return p.finish(rgb(10, 30, 16), 1.0);
}

Bitmap makeVilla() {   // Tuscan farmhouse: ochre walls, terracotta roof, green shutters and a little tower
    const int w = 150, h = 140;
    Painter p(w, h, SS, RENDER_SCALE, 4, 4);
    Color wall = rgb(226, 178, 110), roof = rgb(196, 90, 52), shutter = rgb(70, 110, 70);
    const double side = 22, top = 66;
    // tower
    p.rect(14, 12, 34, top - 12, vgrad(12, top, shade(wall, 1.05), shade(wall, 0.9)));
    p.poly({10, 14, 31, 0, 52, 14}, vgrad(0, 14, shade(roof, 1.1), shade(roof, 0.85)));
    p.roundRect(25, 24, 12, 18, 6, rgb(50, 40, 34));
    // main house
    p.rect(0, top, w - side, h - top, Paint([=](double x, double y) {
        Color c = lerpColor(wall, shade(wall, 0.88), (y - top) / (h - top));
        double n = hashNoise(x * 0.7, y * 0.7);
        return shade(c, 0.94 + 0.12 * n);                                            // weathered plaster
    }));
    p.poly({w - side, top, double(w), top - 6, double(w), double(h), w - side, double(h)}, shade(wall, 0.7));
    p.poly({-6, top + 2, 12, top - 18, w - side - 4, top - 18, w - side + 6, top + 2}, Paint([=](double x, double y) {
        Color c = lerpColor(shade(roof, 1.1), shade(roof, 0.85), (y - top + 18) / 20);
        if (std::fmod(x, 4.0) < 0.8) c = shade(c, 0.8);                              // rows of curved tiles
        return c;
    }));
    p.poly({w - side - 4, top - 18, double(w) - 2, top - 22, double(w) + 2, top - 6, w - side + 6, top + 2}, shade(roof, 0.66));
    for (double x = 14; x < w - side - 14; x += 28) {
        for (double y : {top + 12, top + 44}) {
            if (y > top + 30 && std::fabs(x - 70) < 4) {                               // arched doorway
                p.roundRect(x - 2, top + 38, 18, h - top - 38, 9, rgb(80, 50, 34));
                continue;
            }
            p.rect(x, y, 12, 16, rgb(50, 44, 40));
            p.rect(x - 6, y, 6, 16, shutter);
            p.rect(x + 12, y, 6, 16, shutter);
            for (double sy = y + 2; sy < y + 16; sy += 3) { p.rect(x - 6, sy, 6, 0.6, shade(shutter, 0.7)); p.rect(x + 12, sy, 6, 0.6, shade(shutter, 0.7)); }
        }
    }
    p.rect(0, h - 3, w, 3, rgb(150, 130, 100));
    return p.finish(rgb(50, 30, 20), 1.0);
}

Bitmap makeOlive() {   // gnarled olive tree with a silvery crown
    Painter p(90, 70, SS, RENDER_SCALE, 2, 2);
    Color bark = rgb(110, 100, 86);
    p.poly({38, 70, 40, 46, 34, 34, 40, 34, 45, 44, 50, 32, 55, 34, 49, 48, 52, 70}, Paint([=](double x, double y) {
        Color c = shade(bark, 0.85 + 0.3 * hashNoise(x * 1.5, y * 3));
        return c;
    }));
    std::vector<std::array<double, 3>> balls;
    for (int i = 0; i < 18; ++i) balls.push_back({rnd(12, 78), rnd(12, 34), rnd(8, 13)});
    std::sort(balls.begin(), balls.end(), [](auto& a, auto& b) { return a[1] < b[1]; });
    for (auto& b : balls) {
        Color base = rgb(120 + int(rnd(0, 20)), 146 + int(rnd(0, 20)), 96);
        p.ellipse(b[0], b[1], b[2], b[2] * 0.8, Paint([=](double x, double y) {
            Color c = sphere(b[0], b[1], b[2], b[2] * 0.8, base, 0.45, 0.1, 6)(x, y);
            double n = hashNoise(x * 3, y * 3);
            if (n > 0.86) return shade(rgb(200, 210, 190), 1.0);                     // silver undersides
            return n < 0.2 ? shade(c, 0.8) : c;
        }));
    }
    return p.finish(rgb(30, 40, 24), 1.0);
}

// --------------------------------------------------- new backgrounds ----
BgLayer planetLayer() {   // a big ringed planet and a small moon, pale in the daytime sky (future)
    Painter p(1024, 170, 2);
    const double cx = 300, cy = 80, r = 52;
    auto ring = [&](bool front) {
        p.ellipse(cx, cy, r * 2.1, r * 0.42, Paint([=](double x, double y) {
            double dx = (x - cx) / (r * 2.1), dy = (y - cy) / (r * 0.42), d = std::sqrt(dx * dx + dy * dy);
            if (d < 0.62 || (front ? y < cy : y >= cy)) return TRANSPARENT;
            double band = std::sin(d * 40) * 0.5 + 0.5;
            return alpha(lerpColor(rgb(255, 250, 240), rgb(230, 236, 246), band), 0.55 * (1 - std::pow((d - 0.62) / 0.38, 3)));
        }));
    };
    ring(false);
    p.ellipse(cx, cy, r, r, Paint([=](double x, double y) {
        Color c = sphere(cx, cy, r, r, rgb(236, 206, 196), 0.55, 0.2, 10)(x, y);      // pale, washed out by daylight
        double band = std::sin((y - cy) * 0.22 + std::sin(x * 0.03) * 1.5);
        return alpha(shade(c, 0.95 + 0.06 * band), 0.85);
    }));
    ring(true);
    p.ellipse(760, 40, 13, 13, sphere(760, 40, 13, 13, rgb(240, 240, 246), 0.6, 0.2, 10));
    return BgLayer{p.finish(0, 0, false), 0.02, -40};
}

BgLayer futureSkylineLayer(int h, Color body, double parallax, int yOff, Color accent) {
    Painter p(1024, h, 2);
    double x = 0;
    while (x < 1024) {
        double bw = rnd(18, 46), bh = rnd(h * 0.3, h * 0.95);
        double kind = rnd(0, 1);
        Color c = shade(body, rnd(0.94, 1.06));
        if (kind < 0.35) {                                            // needle tower
            p.poly({x, double(h), x + bw * 0.4, h - bh, x + bw * 0.6, h - bh, x + bw, double(h)}, hgrad(x, x + bw, shade(c, 1.06), shade(c, 0.86)));
            p.rect(x + bw * 0.5 - 0.5, h - bh - 14, 1, 14, shade(c, 0.9));
        } else if (kind < 0.6) {                                      // dome
            p.rect(x, h - bh * 0.6, bw, bh * 0.6, hgrad(x, x + bw, shade(c, 1.05), shade(c, 0.86)));
            p.ellipse(x + bw / 2, h - bh * 0.6, bw / 2, bw * 0.35, sphere(x + bw / 2, h - bh * 0.6, bw / 2, bw * 0.35, accent, 0.55, 0.6, 20));
        } else {                                                      // block with a deck
            p.rect(x + bw * 0.15, h - bh, bw * 0.7, bh, hgrad(x + bw * 0.15, x + bw * 0.85, shade(c, 1.06), shade(c, 0.86)));
            p.rect(x, h - bh * 0.7, bw, 3, shade(c, 1.1));
            p.rect(x + bw * 0.45, h - bh + 4, bw * 0.1, bh - 8, accent);
        }
        x += bw + rnd(0, 8);
    }
    return BgLayer{p.finish(0, 0, false), parallax, yOff};
}

BgLayer rioMountainsLayer() {   // Sugarloaf by the bay and the statue on the peak
    Painter p(1024, 130, 2);
    Color rock = rgb(40, 110, 90), rock2 = rgb(30, 90, 80);
    auto dome = [&](double cx, double w, double hgt, Color c) {
        p.ellipse(cx, 130, w, hgt, Paint([=](double x, double y) {
            double nx = std::clamp((x - cx) / w, -0.98, 0.98);
            return lit(lerpColor(c, shade(c, 0.7), (y - (130 - hgt)) / hgt), nx, -0.3, std::sqrt(1 - nx * nx), 0.5, 0.15, 6);
        }));
    };
    dome(170, 60, 96, rock);                        // Sugarloaf
    dome(230, 40, 60, rock2);
    dome(420, 120, 70, rock2);
    // the peak with the statue
    p.poly({560, 130, 640, 34, 664, 30, 690, 40, 800, 130}, vgrad(30, 130, rgb(50, 120, 80), rgb(30, 90, 70)));
    const double sx = 664, sy = 30;
    Color st = rgb(236, 236, 240);
    p.rect(sx - 2.5, sy - 6, 5, 6, st);                                    // pedestal
    p.poly({sx - 2.2, sy - 6, sx - 1.5, sy - 22, sx + 1.5, sy - 22, sx + 2.2, sy - 6}, st);   // robe
    p.rect(sx - 13, sy - 21, 26, 2.4, st);                                 // outstretched arms
    p.ellipse(sx, sy - 24, 1.8, 2, st);                                    // head
    dome(900, 100, 50, rock);
    for (int i = 0; i < 6; ++i) glow(p, 664, 10, 60, 30, rgb(255, 250, 220), 0.05);
    return BgLayer{p.finish(0, 0, false), 0.08, 0};
}

BgLayer favelaHillLayer() {   // colourful little houses scattered over a green hill
    Painter p(1024, 70, 2);
    Ridge r(3);
    std::vector<double> pts = {0, 70};
    for (int x = 0; x <= 1024; x += 8) { pts.push_back(x); pts.push_back(70 - 20 - 40 * r(x, 1024)); }
    pts.push_back(1024); pts.push_back(70);
    p.poly(pts, vgrad(10, 70, rgb(40, 160, 70), rgb(20, 110, 50)));
    Color cols[] = {rgb(255, 90, 80), rgb(255, 210, 40), rgb(80, 200, 255), rgb(255, 140, 210), rgb(140, 240, 90), rgb(255, 150, 40)};
    for (int i = 0; i < 420; ++i) {
        double x = rnd(0, 1024);
        double top = 70 - 20 - 40 * r(x, 1024);
        double y = rnd(top + 3, 68);
        p.rect(x, y, rnd(2.5, 5), rnd(2, 3.5), cols[i % 6]);
    }
    return BgLayer{p.finish(0, 0, false), 0.18, 0};
}

BgLayer eiffelLayer() {   // the tower on the skyline, with a dome on a hill far away
    Painter p(1024, 190, 2);
    Color iron = rgb(110, 90, 100);
    // dome on a hill
    p.ellipse(860, 190, 110, 30, rgb(200, 170, 180));
    p.rect(842, 140, 36, 22, rgb(236, 226, 226));
    p.ellipse(860, 140, 18, 18, sphere(860, 140, 18, 18, rgb(246, 240, 238), 0.6, 0.2, 8));
    p.rect(858.5, 115, 3, 9, rgb(236, 226, 226));
    // the tower: two curved legs, platforms, a tapering shaft
    const double cx = 300, base = 190, top = 6;
    auto halfW = [=](double y) { double t = (base - y) / (base - top); return 4 + 64 * std::pow(1 - t, 3.2); };
    std::vector<double> pts;
    for (int i = 0; i <= 50; ++i) { double y = base - i * (base - top) / 50; pts.push_back(cx - halfW(y)); pts.push_back(y); }
    for (int i = 50; i >= 0; --i) { double y = base - i * (base - top) / 50; pts.push_back(cx + halfW(y)); pts.push_back(y); }
    p.poly(pts, Paint([=](double x, double y) {
        double hw = halfW(y);
        double u = (x - cx) / hw;
        // lattice: criss-cross lines; the arch between the legs is open
        if (y > base - 40 && std::fabs(u) < 0.62 * std::sqrt(std::max(0.0, 1 - std::pow((base - y) / 40, 2)))) return TRANSPARENT;
        double a = std::fmod(std::fabs(x - cx) + y, 7.0), b = std::fmod(std::fabs(x - cx) - y + 700, 7.0);
        bool lattice = a < 1.3 || b < 1.3 || std::fabs(u) > 0.8;
        return lattice ? iron : alpha(iron, 0.25);
    }));
    for (double y : {base - 52, base - 100}) p.rect(cx - halfW(y) - 3, y, 2 * halfW(y) + 6, 3, iron);   // platforms
    p.rect(cx - 1, top - 6, 2, 8, iron);
    return BgLayer{p.finish(0, 0, false), 0.06, 0};
}

BgLayer rooftopLayer(Color c, double parallax, int yOff) {   // mansard roofs and chimney pots
    Painter p(1024, 60, 2);
    double x = -10;
    while (x < 1034) {
        double w = rnd(30, 70), hgt = rnd(24, 46);
        Color wall = shade(c, rnd(0.9, 1.1));
        p.rect(x, 60 - hgt, w, hgt, wall);
        p.poly({x - 1, 60 - hgt, x + 4, 60 - hgt - 9, x + w - 4, 60 - hgt - 9, x + w + 1, 60 - hgt}, shade(wall, 0.75));
        for (double cxp = x + 6; cxp < x + w - 8; cxp += rnd(10, 18)) {
            p.rect(cxp, 60 - hgt - 15, 4, 7, shade(wall, 0.85));
            p.rect(cxp + 0.6, 60 - hgt - 17, 1.2, 2, shade(wall, 0.7));
            p.rect(cxp + 2.4, 60 - hgt - 17, 1.2, 2, shade(wall, 0.7));
        }
        for (double wx = x + 4; wx < x + w - 4; wx += 7)
            for (double wy = 60 - hgt + 5; wy < 58; wy += 8) p.rect(wx, wy, 3, 5, shade(wall, 0.8));
        x += w;
    }
    return BgLayer{p.finish(0, 0, false), parallax, yOff};
}

BgLayer castleLayer() {   // a fairy-tale castle on a wooded crag
    Painter p(1024, 150, 2);
    const double cx = 620;
    p.poly({500, 150, 560, 96, 600, 88, 660, 92, 700, 104, 760, 150}, vgrad(88, 150, rgb(70, 110, 70), rgb(50, 90, 56)));
    Color stone = rgb(236, 232, 226), roof = rgb(70, 90, 130);
    auto tower = [&](double x, double y, double w, double hgt) {
        p.rect(x - w / 2, y - hgt, w, hgt, cylX(x - w / 2, x + w / 2, stone, 0.55, 0.2, 8));
        p.poly({x - w / 2 - 1.5, y - hgt, x, y - hgt - w * 1.9, x + w / 2 + 1.5, y - hgt}, hgrad(x - w / 2, x + w / 2, shade(roof, 1.2), shade(roof, 0.75)));
        for (double wy = y - hgt + 6; wy < y - 6; wy += 9) p.rect(x - 1, wy, 2, 4, rgb(60, 60, 80));
    };
    p.rect(cx - 48, 60, 80, 34, cylX(cx - 48, cx + 32, stone, 0.6, 0.15, 6));                  // palace
    p.poly({cx - 50, 60, cx - 40, 46, cx + 24, 46, cx + 34, 60}, shade(roof, 0.9));
    for (double wx = cx - 42; wx < cx + 28; wx += 8) p.rect(wx, 68, 3, 6, rgb(60, 60, 80));
    tower(cx - 56, 94, 11, 46);
    tower(cx + 40, 94, 9, 60);
    tower(cx - 8, 60, 8, 34);
    tower(cx + 18, 60, 6, 20);
    tower(cx - 30, 60, 6, 16);
    for (int i = 0; i < 60; ++i) {                                                          // trees on the crag
        double x = rnd(510, 750), y = rnd(100, 148), hh = rnd(8, 16);
        p.poly({x, y, x + 3.5, y - hh, x + 7, y}, shade(rgb(40, 100, 50), rnd(0.8, 1.15)));
    }
    return BgLayer{p.finish(0, 0, false), 0.1, 0};
}

BgLayer florenceLayer() {   // the cathedral dome, the bell tower and the town hall tower over red roofs
    Painter p(1024, 150, 2);
    Color roofs = rgb(196, 106, 70), walls = rgb(226, 196, 150);
    // town of red roofs
    double x = 300;
    while (x < 760) {
        double w = rnd(10, 24), h = rnd(10, 22);
        p.rect(x, 150 - h, w, h, shade(walls, rnd(0.85, 1.05)));
        p.poly({x - 1, 150 - h, x + w / 2, 150 - h - 5, x + w + 1, 150 - h}, shade(roofs, rnd(0.85, 1.1)));
        x += w;
    }
    // the dome (octagonal drum, red brick shell with white ribs, lantern)
    const double dx = 520, dy = 104;
    p.rect(dx - 40, dy, 80, 26, cylX(dx - 40, dx + 40, rgb(236, 226, 206), 0.6, 0.15, 6));
    for (double wx = dx - 34; wx < dx + 34; wx += 12) p.ellipse(wx + 3, dy + 12, 2.6, 2.6, rgb(90, 90, 80));
    p.ellipse(dx, dy, 38, 54, Paint([=](double px_, double py) {
        if (py > dy) return TRANSPARENT;
        double nx = std::clamp((px_ - dx) / 38, -0.98, 0.98);
        Color c = lit(rgb(196, 92, 60), nx, -0.4, std::sqrt(1 - nx * nx), 0.5, 0.25, 10);
        for (double rib : {-0.62, 0.0, 0.62}) if (std::fabs(nx - rib) < 0.05) c = rgb(246, 240, 230);
        return c;
    }));
    p.rect(dx - 5, dy - 66, 10, 14, cylX(dx - 5, dx + 5, rgb(246, 240, 230), 0.55, 0.2, 8));
    p.poly({dx - 6, dy - 66, dx, dy - 78, dx + 6, dy - 66}, rgb(150, 150, 140));
    p.ellipse(dx, dy - 80, 2, 2, rgb(220, 190, 80));
    // the bell tower (tall, square, striped marble)
    const double tx = 470;
    p.rect(tx - 9, 30, 18, 120, Paint([=](double px_, double py) {
        Color c = std::fmod(py, 20.0) < 2 ? rgb(70, 120, 90) : rgb(240, 232, 220);
        if (std::fabs(px_ - tx) > 7) c = shade(c, 0.8);
        if (std::fmod(py, 20.0) > 8 && std::fmod(py, 20.0) < 15 && std::fabs(px_ - tx) < 3) c = rgb(80, 70, 70);
        return c;
    }));
    p.rect(tx - 10, 27, 20, 3, rgb(230, 220, 205));
    // the town hall tower (crenellated)
    const double hx = 610;
    p.rect(hx - 7, 50, 14, 100, cylX(hx - 7, hx + 7, rgb(200, 160, 110), 0.5, 0.1, 6));
    p.rect(hx - 10, 42, 20, 10, rgb(200, 160, 110));
    for (double cxp = hx - 10; cxp < hx + 10; cxp += 4) p.rect(cxp, 38, 2, 4, rgb(190, 150, 104));
    p.rect(hx - 4, 22, 8, 20, rgb(206, 166, 116));
    p.poly({hx - 5, 22, hx, 12, hx + 5, 22}, rgb(150, 90, 60));
    return BgLayer{p.finish(0, 0, false), 0.08, 0};
}

BgLayer tuscanHillsLayer() {   // soft rolling hills with rows of cypresses
    Painter p(1024, 60, 2);
    Ridge r(2);
    std::vector<double> pts = {0, 60};
    for (int x = 0; x <= 1024; x += 8) { pts.push_back(x); pts.push_back(60 - 12 - 34 * r(x, 1024)); }
    pts.push_back(1024); pts.push_back(60);
    p.poly(pts, Paint([](double x, double y) {
        Color c = lerpColor(rgb(170, 170, 80), rgb(130, 150, 60), y / 60);
        if (std::fmod(x * 0.5 + y * 2, 9.0) < 1.0) c = shade(c, 0.9);              // furrows
        return c;
    }));
    for (int i = 0; i < 70; ++i) {
        double x = rnd(0, 1024), top = 60 - 12 - 34 * r(x, 1024), hh = rnd(8, 14);
        p.ellipse(x, top - hh * 0.4, 1.6, hh * 0.6, rgb(36, 76, 44));
    }
    return BgLayer{p.finish(0, 0, false), 0.2, 0};
}
} // namespace

void Graphics::generate() {
    sprite[SPR_PALM] = makePalm();
    sprite[SPR_PINE] = makePine(false);
    sprite[SPR_PINE_SNOW] = makePine(true);
    sprite[SPR_BUSH] = makeBush();
    sprite[SPR_CACTUS] = makeCactus();
    sprite[SPR_ROCK] = makeRock();
    sprite[SPR_LAMP] = makeLamp();
    sprite[SPR_BILLBOARD1] = makeBillboard(0);
    sprite[SPR_BILLBOARD2] = makeBillboard(1);
    sprite[SPR_BILLBOARD3] = makeBillboard(2);
    sprite[SPR_BUILDING1] = makeBuilding(0);
    sprite[SPR_BUILDING2] = makeBuilding(1);
    sprite[SPR_SNOWMAN] = makeSnowman();
    sprite[SPR_TURN_RIGHT] = makeTurnSign();
    sprite[SPR_TURN_LEFT] = sprite[SPR_TURN_RIGHT].flipped();
    sprite[SPR_GANTRY_START] = makeGantry(true);
    sprite[SPR_GANTRY_CHECK] = makeGantry(false);
    sprite[SPR_SIGN_JUMP] = makeWarningSign(0);
    sprite[SPR_SIGN_NARROW] = makeWarningSign(1);
    sprite[SPR_SIGN_TUNNEL] = makeWarningSign(2);
    sprite[SPR_SIGN_DIRT] = makeWarningSign(3);
    {   // brighter core for tunnel lamps
        Bitmap g(32, 32);
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                double dx = (x + 0.5 - 16) / 16, dy = (y + 0.5 - 16) / 16, d = std::sqrt(dx * dx + dy * dy);
                if (d >= 1) continue;
                double core = d < 0.28 ? 1.0 : std::pow(1 - (d - 0.28) / 0.72, 2.2);
                g.px[size_t(y) * 32 + x] = withAlpha(lerpColor(rgb(255, 230, 160), rgb(255, 255, 250), d < 0.28 ? 1 : 0.3), int(core * 255));
            }
        g.buildMips();
        lampGlow = g;
    }

    dust = makePuff(rgb(190, 165, 125), 1.0);
    smoke = makePuff(rgb(225, 225, 232), 0.7);
    spark = makePuff(rgb(255, 230, 140), 1.0);
    lightRed = makeLight(rgb(255, 40, 30), true);
    lightGreen = makeLight(rgb(60, 255, 80), true);
    lightOff = makeLight(rgb(255, 40, 30), false);

    // 0 coast
    bg[0].push_back(cloudLayer(9, rgb(255, 255, 255), 0.05, -70));
    bg[0].push_back(ridgeLayer(80, 20, 70, 2, rgb(120, 150, 210), rgb(160, 190, 228), 0.1, 0));
    bg[0].push_back(ridgeLayer(50, 8, 40, 3, rgb(40, 160, 64), rgb(14, 110, 44), 0.25, 0));
    // 1 desert
    bg[1].push_back(sunLayer(rgb(255, 252, 230), rgb(255, 225, 150), 20, false, 0.03, -100));
    bg[1].push_back(ridgeLayer(90, 20, 80, 2, rgb(160, 116, 170), rgb(200, 146, 156), 0.08, 0, 0, true));
    bg[1].push_back(mesaLayer());
    // 2 night city
    bg[2].push_back(upscaled(starLayer(0.02)));
    bg[2].push_back(upscaled(skylineLayer(110, rgb(50, 28, 78), false, 0.06, 0)));
    bg[2].push_back(upscaled(skylineLayer(80, rgb(28, 22, 50), true, 0.15, 0)));
    // 3 snow
    bg[3].push_back(cloudLayer(7, rgb(240, 244, 252), 0.04, -80));
    bg[3].push_back(ridgeLayer(130, 40, 125, 2, rgb(116, 132, 170), rgb(156, 172, 202), 0.08, 0, rgb(252, 253, 255), true));
    bg[3].push_back(ridgeLayer(60, 10, 50, 3, rgb(240, 244, 252), rgb(212, 222, 242), 0.2, 0));
    // 4 forest
    bg[4].push_back(ridgeLayer(80, 20, 70, 2, rgb(132, 152, 144), rgb(142, 160, 152), 0.08, 0));
    bg[4].push_back(treeLineLayer(rgb(98, 124, 108), 0.15, 0));
    bg[4].push_back(treeLineLayer(rgb(58, 88, 68), 0.25, 4));
    // 5 sunset
    bg[5].push_back(sunLayer(rgb(255, 240, 120), rgb(255, 50, 120), 70, true, 0.03, 75));
    bg[5].push_back(ridgeLayer(70, 10, 60, 2, rgb(100, 44, 120), rgb(126, 52, 116), 0.08, 0));
    bg[5].push_back(ridgeLayer(36, 4, 24, 4, rgb(64, 26, 74), rgb(74, 30, 74), 0.2, 0));

    // ---- stages 7-11: generated after the original graphics; each stage has its own random numbers, so changing
    // one stage's graphics never changes another's
    rng.seed(7001);
    sprite[SPR_HOLO_PYLON] = makeHoloPylon();
    sprite[SPR_SCIFI_TOWER] = makeScifiTower();
    sprite[SPR_UFO] = makeUfo();
    sprite[SPR_HOLO_SIGN] = makeHoloSign();
    bg[6].push_back(cloudLayer(5, rgb(255, 255, 255), 0.04, -80));
    bg[6].push_back(planetLayer());
    bg[6].push_back(futureSkylineLayer(120, rgb(196, 214, 222), 0.06, 0, rgb(120, 200, 200)));
    bg[6].push_back(futureSkylineLayer(80, rgb(240, 240, 236), 0.15, 0, rgb(40, 170, 180)));
    rng.seed(7002);
    sprite[SPR_TROPICAL] = makeTropical();
    sprite[SPR_CARNIVAL] = makeCarnival();
    sprite[SPR_FAVELA] = makeFavela();
    bg[7].push_back(sunLayer(rgb(255, 255, 200), rgb(255, 150, 40), 34, true, 0.03, -40));
    bg[7].push_back(rioMountainsLayer());
    bg[7].push_back(favelaHillLayer());
    rng.seed(7003);
    sprite[SPR_PARIS_LAMP] = makeParisLamp();
    sprite[SPR_HAUSSMANN] = makeHaussmann();
    sprite[SPR_CAFE] = makeCafe();
    sprite[SPR_TREE_ROUND] = makeTreeRound();
    bg[8].push_back(cloudLayer(6, rgb(255, 226, 226), 0.04, -80));
    bg[8].push_back(eiffelLayer());
    bg[8].push_back(rooftopLayer(rgb(206, 196, 186), 0.16, 0));
    rng.seed(7004);
    sprite[SPR_FACHWERK] = makeFachwerk();
    sprite[SPR_AUTOBAHN_SIGN] = makeAutobahnSign();
    sprite[SPR_WIND_TURBINE] = makeWindTurbine();
    bg[9].push_back(cloudLayer(8, rgb(255, 255, 255), 0.05, -70));
    bg[9].push_back(ridgeLayer(70, 20, 60, 2, rgb(110, 150, 120), rgb(130, 168, 130), 0.07, 0));
    bg[9].push_back(castleLayer());
    bg[9].push_back(treeLineLayer(rgb(40, 96, 56), 0.25, 4));
    rng.seed(7005);
    sprite[SPR_CYPRESS] = makeCypress();
    sprite[SPR_VILLA] = makeVilla();
    sprite[SPR_OLIVE] = makeOlive();
    // 10 Florence
    bg[10].push_back(ridgeLayer(70, 14, 50, 2, rgb(150, 160, 150), rgb(176, 176, 150), 0.05, 0));
    bg[10].push_back(florenceLayer());
    bg[10].push_back(tuscanHillsLayer());
}

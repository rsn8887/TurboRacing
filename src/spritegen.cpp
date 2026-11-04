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
}

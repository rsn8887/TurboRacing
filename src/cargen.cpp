// Car sprite generator (used by the asset tool, not by the game). Car sprites are pre-rendered from small 3D
// models (like many 90s racers did), so every steering and hill angle shows one solid body. Each model is
// lofted from cross-sections (body), plus a cabin, wheels, wings, mirrors etc. Fine detail (tail lights,
// plates, grilles, rear windows) is painted into textures ("decals") that are mapped onto the model's faces.
// The model is rendered with flat shaded facets + specular highlights at 2x resolution, outlined, then
// box-filtered down: retro pixels with anti-aliased edges. Body paint is rendered in neutral grey with a
// paint mask; the game recolours it per car while drawing.
#include "cargen.h"
#include "painter.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <thread>
#include <vector>

namespace {
constexpr int SS = RENDER_SCALE >= 2 ? 2 : 3;   // supersampling of the car renderer
const Color BODY = rgb(170, 170, 170);     // neutral paint, recoloured later
const Color WHITE = rgb(255, 255, 255), BLACK = rgb(0, 0, 0), CHROME = rgb(196, 198, 210);
Color alpha(Color c, double a) { return withAlpha(c, int(a * 255)); }

// ------------------------------------------------------------- 3D basics ----
struct V3 { double x, y, z; };
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 norm(V3 a) { double l = std::sqrt(dot(a, a)); return l > 1e-12 ? a * (1 / l) : V3{0, 0, 1}; }

enum Mat { M_BODY, M_TRIM, M_GLASS, M_BLACK, M_CHROME, M_TYRE, M_TYRE2, M_RIM, M_RIM2, M_UNDER, M_LIGHT, M_DECAL };
struct MatDef { Color c; double amb, spec, shin; bool paint; };
const MatDef MATS[] = {
    {BODY,              0.40, 0.55, 28, true},    // body paint
    {rgb(110, 110, 110), 0.40, 0.35, 20, true},   // darker painted trim
    {rgb(34, 46, 72),   0.55, 0.85, 60, false},   // glass
    {rgb(26, 26, 30),   0.55, 0.25, 16, false},   // black plastic
    {CHROME,            0.45, 0.95, 40, false},
    {rgb(38, 38, 42),   0.55, 0.10, 8,  false},   // tyre
    {rgb(26, 26, 30),   0.55, 0.10, 8,  false},   // tyre tread groove
    {rgb(160, 162, 172), 0.45, 0.80, 30, false},  // rim
    {rgb(70, 72, 80),   0.45, 0.50, 20, false},   // rim (dark spokes)
    {rgb(14, 14, 18),   0.70, 0.00, 1,  false},   // underside
    {rgb(230, 40, 36),  0.80, 0.60, 30, false},   // light lens (unused by default)
    {WHITE,             0.40, 0.55, 28, false},   // decal (per texel)
};

struct Tex { Bitmap bmp; std::vector<unsigned char> mask; };
struct Tri { V3 p[3]; double u[3] = {0, 0, 0}, v[3] = {0, 0, 0}; int mat = M_BODY; const Tex* tex = nullptr; };

// the rear tyres' contact points with the road (x, z) of the model being built: tyre smoke starts there
thread_local std::vector<std::pair<double, double>> g_tyres;

struct Mesh {
    std::vector<Tri> tris;
    void tri(V3 a, V3 b, V3 c, int mat, const Tex* tex = nullptr, std::array<double, 6> uv = {0, 0, 0, 0, 0, 0}) {
        Tri t; t.p[0] = a; t.p[1] = b; t.p[2] = c; t.mat = mat; t.tex = tex;
        t.u[0] = uv[0]; t.v[0] = uv[1]; t.u[1] = uv[2]; t.v[1] = uv[3]; t.u[2] = uv[4]; t.v[2] = uv[5];
        tris.push_back(t);
    }
    // quad a-b-c-d; with a texture the corners get uv (0,0) (1,0) (1,1) (0,1)
    void quad(V3 a, V3 b, V3 c, V3 d, int mat, const Tex* tex = nullptr) {
        tri(a, b, c, tex ? M_DECAL : mat, tex, {0, 0, 1, 0, 1, 1});
        tri(a, c, d, tex ? M_DECAL : mat, tex, {0, 0, 1, 1, 0, 1});
    }
    // 8-corner block: rear face (z small) bl, br, tr, tl and front face likewise. Faces: rear, top,
    // left, right, front, bottom. Optional decals on the rear and top faces.
    void hexa(V3 rbl, V3 rbr, V3 rtr, V3 rtl, V3 fbl, V3 fbr, V3 ftr, V3 ftl,
              int rear, int top, int side, int front, const Tex* rearTex = nullptr, const Tex* topTex = nullptr) {
        quad(rtl, rtr, rbr, rbl, rear, rearTex);
        quad(ftl, ftr, rtr, rtl, top, topTex);
        quad(rtl, ftl, fbl, rbl, side);
        quad(ftr, rtr, rbr, fbr, side);
        quad(ftr, ftl, fbl, fbr, front);
        quad(rbl, rbr, fbr, fbl, M_UNDER);
    }
    void box(double x0, double x1, double y0, double y1, double z0, double z1, int mat, int rearMat = -1, const Tex* rearTex = nullptr) {
        hexa({x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0}, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
             rearMat < 0 ? mat : rearMat, mat, mat, mat, rearTex);
    }
    // cylinder along the x axis (wheels) or z axis (spare wheel); 'out' = which cap shows the rim
    void wheel(V3 c, double r, double w, bool alongZ, int outSign) {
        const int n = 18;
        if (!alongZ && c.z < 0) g_tyres.push_back({c.x, c.z});   // a rear road wheel (not the spare)
        auto P = [&](double a, double off, double rr) -> V3 {
            double s = std::sin(a) * rr, co = std::cos(a) * rr;
            return alongZ ? V3{c.x + co, c.y + s, c.z + off} : V3{c.x + off, c.y + s, c.z + co};
        };
        for (int i = 0; i < n; ++i) {
            double a0 = i * 2 * M_PI / n, a1 = (i + 1) * 2 * M_PI / n;
            quad(P(a0, -w / 2, r), P(a1, -w / 2, r), P(a1, w / 2, r), P(a0, w / 2, r), (i & 1) ? M_TYRE : M_TYRE2);
            for (int s : {-1, 1}) {
                double off = s * w / 2;
                bool outer = s == outSign;
                // sidewall ring + rim with spokes on the outer side, plain black inside
                quad(P(a0, off, r), P(a1, off, r), P(a1, off, r * 0.66), P(a0, off, r * 0.66), outer ? M_TYRE : M_BLACK);
                V3 hub = alongZ ? V3{c.x, c.y, c.z + off} : V3{c.x + off, c.y, c.z};
                tri(hub, P(a0, off, r * 0.66), P(a1, off, r * 0.66), outer ? ((i % 3 == 0) ? M_RIM2 : M_RIM) : M_BLACK);
            }
        }
    }
};

// rounded body cross-section (half width w, bottom y0, top y1, top/bottom bevels)
struct Sec { double z, w, y0, y1, bt, bb; };
std::vector<std::pair<double, double>> secPoints(const Sec& s) {
    double w = s.w, y0 = s.y0, y1 = s.y1, bt = s.bt, bb = s.bb;
    return {{-w + bb, y0}, {w - bb, y0}, {w, y0 + bb}, {w, y1 - bt}, {w - bt * 0.3, y1 - bt * 0.45},
            {w - bt * 0.75, y1 - bt * 0.1}, {w - bt * 1.4, y1}, {-w + bt * 1.4, y1}, {-w + bt * 0.75, y1 - bt * 0.1},
            {-w + bt * 0.3, y1 - bt * 0.45}, {-w, y1 - bt}, {-w, y0 + bb}};
}

// body lofted through the sections; the rear cap carries the decal (uv from the cap's bounding box)
thread_local std::vector<Sec> lastLoft;     // the body of the model being built (cabin() sits on it)

// height of the top of the last lofted body at z (linear between its sections)
double bodyTopAt(double z) {
    const auto& S = lastLoft;
    if (S.empty()) return -1e9;
    if (z <= S.front().z) return S.front().y1;
    for (size_t i = 0; i + 1 < S.size(); ++i)
        if (z <= S[i + 1].z) {
            double t = (z - S[i].z) / (S[i + 1].z - S[i].z);
            return S[i].y1 + (S[i + 1].y1 - S[i].y1) * t;
        }
    return S.back().y1;
}

// door mirrors: sitting on the body top beside the windscreen base, so they don't float in the air
void mirrors(Mesh& m, double xIn, double xOut, double z0, double z1, int mat = M_BODY) {
    double y0 = bodyTopAt((z0 + z1) / 2) - 1;
    for (int s : {-1, 1}) m.box(s < 0 ? -xOut : xIn, s < 0 ? -xIn : xOut, y0, y0 + 6.5, z0, z1, mat);
}

// big door mirrors (SUV, pickup): the head stands on the door line beside the windscreen, with an arm reaching
// into the cab side (xCab), so it is attached to the car from every angle
void armMirrors(Mesh& m, double xCab, double xIn, double xOut, double yBase, double h, double z0, double z1, int mat) {
    for (int s : {-1, 1}) {
        auto X = [s](double a, double b) { return s < 0 ? std::pair<double, double>{-b, -a} : std::pair<double, double>{a, b}; };
        auto [hx0, hx1] = X(xIn, xOut);
        m.box(hx0, hx1, yBase, yBase + h, z0, z1, mat);                                   // head
        auto [ax0, ax1] = X(xCab - 2, xIn + 1);
        m.box(ax0, ax1, yBase + 2, yBase + 7, z0 + 1, z1 - 1, mat);                       // arm
    }
}

// exhaust pipes as they are painted onto the rear decal (x, height in cm) and the z of the tail they sit on:
// collected while a model is built, so the generator knows where the nitrous flames come out
thread_local std::vector<std::pair<double, double>> g_pipes;
thread_local double g_tailZ = 0;

void loft(Mesh& m, const std::vector<Sec>& secs, const Tex* rearTex, int mat = M_BODY) {
    if (rearTex) g_tailZ = secs[0].z;
    lastLoft = secs;
    for (size_t i = 0; i + 1 < secs.size(); ++i) {
        auto a = secPoints(secs[i]), b = secPoints(secs[i + 1]);
        for (size_t k = 0; k < a.size(); ++k) {
            size_t k1 = (k + 1) % a.size();
            V3 p0{a[k].first, a[k].second, secs[i].z}, p1{a[k1].first, a[k1].second, secs[i].z};
            V3 q0{b[k].first, b[k].second, secs[i + 1].z}, q1{b[k1].first, b[k1].second, secs[i + 1].z};
            m.quad(p0, p1, q1, q0, k == 0 ? M_UNDER : mat);
        }
    }
    const Sec& r = secs.front();
    auto a = secPoints(r);
    V3 c{0, (r.y0 + r.y1) / 2, r.z};
    auto uv = [&](V3 p) { return std::pair<double, double>{(p.x + r.w) / (2 * r.w), (r.y1 - p.y) / (r.y1 - r.y0)}; };
    for (size_t k = 0; k < a.size(); ++k) {
        size_t k1 = (k + 1) % a.size();
        V3 p0{a[k].first, a[k].second, r.z}, p1{a[k1].first, a[k1].second, r.z};
        auto u0 = uv(p0), u1 = uv(p1), uc = uv(c);
        if (rearTex) m.tri(c, p0, p1, M_DECAL, rearTex, {uc.first, uc.second, u0.first, u0.second, u1.first, u1.second});
        else m.tri(c, p0, p1, mat);
    }
    const Sec& f = secs.back();
    auto b = secPoints(f);
    V3 cf{0, (f.y0 + f.y1) / 2, f.z};
    for (size_t k = 0; k < b.size(); ++k) {
        size_t k1 = (k + 1) % b.size();
        m.tri(cf, {b[k].first, b[k].second, f.z}, {b[k1].first, b[k1].second, f.z}, mat);
    }
}

// cabin / greenhouse: rear window (decal), roof, side windows
void cabin(Mesh& m, double zrb, double yb, double wb, double zrt, double yt, double wt, double zft, double zfb,
           const Tex* rearTex, int sideMat = M_GLASS) {
    // The windscreen must meet the bonnet: where the bonnet top is lower than the cabin's base, the front
    // edge of the base goes down to just inside the bonnet, so no gap shows when the car is seen from an
    // angle (the part inside the body is hidden by the z-buffer).
    double ybf = std::min(yb, bodyTopAt(zfb) - 3);
    double topR = bodyTopAt(zrb), ybr = topR < yb ? topR - 1 : yb;   // rear: only if there is a gap
    m.hexa({-wb, ybr, zrb}, {wb, ybr, zrb}, {wt, yt, zrt}, {-wt, yt, zrt},
           {-wb, ybf, zfb}, {wb, ybf, zfb}, {wt, yt, zft}, {-wt, yt, zft},
           M_GLASS, M_BODY, sideMat, M_GLASS, rearTex);
}

// ----------------------------------------------------------- decal painting --
// Paints a texture for a face that spans x in [-w, w] (cm) and heights [y0, y1] (cm above ground).
struct Decal {
    Painter p;
    double w, y0, y1, k;
    Decal(double w_, double y0_, double y1_, double pxPerCm = 1.6 * RENDER_SCALE)
        : p(int(2 * w_ * pxPerCm), int((y1_ - y0_) * pxPerCm), 2, 1), w(w_), y0(y0_), y1(y1_), k(pxPerCm) {}
    double X(double x) const { return (x + w) * k; }
    double Y(double h) const { return (y1 - h) * k; }
    void rect(double x0, double h0, double x1, double h1, const Paint& pt, double r = 0) {
        if (r > 0) p.roundRect(X(x0), Y(h1), (x1 - x0) * k, (h1 - h0) * k, r * k, pt);
        else p.rect(X(x0), Y(h1), (x1 - x0) * k, (h1 - h0) * k, pt);
    }
    void ellipse(double x, double h, double rx, double ry, const Paint& pt) { p.ellipse(X(x), Y(h), rx * k, ry * k, pt); }
    Paint vg(double h0, double h1, Color bottom, Color top) const { return vgrad(Y(h1), Y(h0), top, bottom); }
    void paintAll() { p.material = true; p.rect(0, 0, p.W, p.H, BODY); p.material = false; }
    Tex finish() { Tex t; t.bmp = p.finishMask(t.mask, 0, 0); return t; }
};

Color lensTop(bool b) { return b ? rgb(255, 150, 140) : rgb(235, 46, 42); }
Color lensBot(bool b) { return b ? rgb(235, 40, 30) : rgb(120, 0, 10); }

void plate(Decal& d, double cx, double h0, double hw = 17, double hh = 10) {
    d.rect(cx - hw - 1, h0 - 1, cx + hw + 1, h0 + hh + 1, rgb(16, 16, 20), 1);
    d.rect(cx - hw, h0, cx + hw, h0 + hh, d.vg(h0, h0 + hh, rgb(205, 205, 195), rgb(250, 250, 242)), 1);
    for (int i = 0; i < 5; ++i) d.rect(cx - hw + 5 + i * (2 * hw - 10) / 5.0, h0 + 2.5, cx - hw + 5 + i * (2 * hw - 10) / 5.0 + 3.6, h0 + hh - 2.5, rgb(30, 30, 40));
}
void pipe(Decal& d, double x, double h, double r) {
    g_pipes.push_back({x, h});
    d.ellipse(x, h, r * 1.25, r, sphere(d.X(x), d.Y(h), r * 1.25 * d.k, r * d.k, rgb(160, 162, 172), 0.4, 0.8, 26));
    d.ellipse(x, h - 0.2, r * 0.85, r * 0.68, rgb(8, 8, 10));
}
void diffuser(Decal& d, double hw, double h0, double h1, double finStep) {
    d.rect(-hw, h0, hw, h1, d.vg(h0, h1, rgb(8, 8, 12), rgb(38, 38, 46)), 2);
    for (double x = -hw + finStep; x < hw - 2; x += finStep) d.rect(x, h0, x + 1.4, h1 - 1, rgb(62, 62, 72));
}
void glowAt(Decal& d, double x, double h, double rx, double ry) {
    double cx = d.X(x), cy = d.Y(h), RX = rx * d.k, RY = ry * d.k;
    d.p.ellipse(cx, cy, RX, RY, Paint([=](double px, double py) {
        double dx = (px - cx) / RX, dy = (py - cy) / RY;
        double v = std::max(0.0, 1 - std::sqrt(dx * dx + dy * dy));
        return alpha(rgb(255, 80, 60), std::min(1.0, v * v * 1.4));
    }));
}

// rear window texture: frame in body paint, tinted glass with headrests and a reflection
// reflections on glass: off for the base frames (they get mirrored for left turns), on for the glare overlays
thread_local bool g_glare = true;

Tex windowDecal(double frame, bool headrests, bool brake, bool brakeLight = true) {
    Decal d(50, 0, 30, 4.0);    // normalized: 100 x 30 "units"
    d.paintAll();
    double f = frame;
    d.rect(-50 + f, f * 0.8, 50 - f, 30 - f * 0.8, d.vg(f, 30 - f, rgb(10, 12, 26), rgb(66, 86, 126)), 2);
    if (headrests)
        for (double hx : {-20.0, 20.0}) d.rect(hx - 9, f * 0.8, hx + 9, 17, d.vg(0, 17, rgb(14, 12, 16), rgb(52, 46, 52)), 4);
    // reflection streaks
    if (g_glare) d.p.poly({d.X(-30), d.Y(30 - f), d.X(-18), d.Y(30 - f), d.X(-34), d.Y(f), d.X(-46), d.Y(f)}, alpha(rgb(200, 220, 255), 0.30));
    if (g_glare) d.p.poly({d.X(-12), d.Y(30 - f), d.X(-8), d.Y(30 - f), d.X(-24), d.Y(f), d.X(-28), d.Y(f)}, alpha(rgb(200, 220, 255), 0.22));
    if (brakeLight) d.rect(-12, 30 - f * 0.8 - 2.2, 12, 30 - f * 0.8 - 0.4, brake ? rgb(255, 110, 90) : rgb(130, 18, 18));
    return d.finish();
}
// simple painted top texture with black slats (engine cover louvres / bed)
Tex slatsDecal(int n, bool bed) {
    Decal d(50, 0, 30, 4.0);
    if (bed) {
        d.rect(-50, 0, 50, 30, d.vg(0, 30, rgb(26, 26, 30), rgb(40, 40, 46)));
        (void)n;
    } else {
        d.paintAll();
        for (int i = 0; i < n; ++i) d.rect(-44, 3 + i * 26.0 / n, 44, 3 + i * 26.0 / n + 26.0 / n * 0.55, rgb(14, 14, 18), 1);
    }
    return d.finish();
}

// ------------------------------------------------------------------- models --
struct Model {
    Mesh mesh;
    double rearAxleZ = -130;
    std::vector<V3> exhausts;                // the exhaust pipe openings (see g_pipes)
    std::vector<V3> tyres;                   // where the rear tyres touch the road (see g_tyres)
    std::vector<std::unique_ptr<Tex>> texs;
    const Tex* add(Tex t) { texs.push_back(std::make_unique<Tex>(std::move(t))); return texs.back().get(); }
};

void sport(Model& M, bool brake) {
    Mesh& m = M.mesh;
    // rear decal on the tail (w 86, h 14..72)
    Decal d(86, 14, 72);
    d.paintAll();
    diffuser(d, 74, 14, 28, 9);
    for (double x : {-40.0, -27.0, 27.0, 40.0}) pipe(d, x, 21.5, 4.6);
    d.rect(-6, 23, 6, 27, brake ? rgb(255, 90, 80) : rgb(120, 10, 14), 1);
    d.rect(-80, 43, 80, 59, d.vg(43, 59, rgb(12, 12, 16), rgb(40, 40, 46)), 3);
    for (int s : {-1, 1}) {
        double x0 = s < 0 ? -76 : 24, x1 = s < 0 ? -24 : 76;
        d.rect(x0, 45, x1, 57, CHROME, 2);
        d.rect(x0 + 1, 46, x1 - 1, 56, d.vg(46, 56, lensBot(brake), lensTop(brake)), 2);
        d.rect(x0 + 4, 50, x1 - 14, 52.6, brake ? rgb(255, 235, 225) : rgb(250, 90, 80));
        double rx = s < 0 ? x1 - 10 : x0 + 2;
        d.rect(rx, 47.5, rx + 8, 54.5, d.vg(47.5, 54.5, rgb(170, 170, 180), rgb(250, 250, 255)), 1);
        d.rect(s < 0 ? -22 : 14, 46, s < 0 ? -14 : 22, 56, d.vg(46, 56, rgb(200, 100, 0), rgb(255, 190, 60)), 1.5);
        d.rect(x0 + 2, 55, x1 - 2, 56, alpha(WHITE, 0.4));
        if (brake) glowAt(d, (x0 + x1) / 2, 51, 34, 12);
    }
    d.ellipse(0, 51, 4.4, 3.4, sphere(d.X(0), d.Y(51), 4.4 * d.k, 3.4 * d.k, rgb(220, 200, 120), 0.5, 0.9, 30));
    plate(d, 0, 30);
    for (int s : {-1, 1}) for (int i = 0; i < 3; ++i) d.rect(s * 84 - (s < 0 ? 0 : 8), 32 + i * 3.4, s * 84 + (s < 0 ? 8 : 0), 34 + i * 3.4, rgb(14, 14, 18), 0.8);
    d.rect(-84, 63.5, 84, 64.6, alpha(WHITE, 0.25));
    const Tex* rear = M.add(d.finish());
    loft(m, {{-215, 86, 14, 72, 14, 6}, {-200, 93, 12, 80, 16, 6}, {-60, 95, 12, 84, 18, 6},
             {100, 93, 12, 75, 18, 6}, {215, 82, 14, 52, 14, 6}}, rear);
    cabin(m, -45, 82, 70, 25, 116, 54, 80, 150, M.add(windowDecal(4, true, brake)));
    mirrors(m, 68, 80, 132, 142);
    // side intakes
    for (int s : {-1, 1}) m.box(s < 0 ? -96 : 93, s < 0 ? -93 : 96, 40, 58, -120, -70, M_BLACK);
    // big rear wing on struts with end plates
    m.box(-92, 92, 100, 106, -212, -176, M_BODY);
    m.box(-92, 92, 99, 100.5, -176, -172, M_BLACK);
    for (int s : {-1, 1}) {
        m.box(s * 40 - 3, s * 40 + 3, 78, 100, -200, -188, M_BLACK);
        m.box(s < 0 ? -96 : 91, s < 0 ? -91 : 96, 88, 112, -216, -168, M_TRIM);
    }
    for (int s : {-1, 1}) { m.wheel({s * 78.0, 34, -130}, 34, 30, false, s); m.wheel({s * 78.0, 34, 135}, 33, 28, false, s); }
    M.rearAxleZ = -130;
}

void coupe(Model& M, bool brake) {     // rear-engined coupe: rounded tail, long sloping rear glass, light bar
    Mesh& m = M.mesh;
    Decal d(74, 16, 64);
    d.paintAll();
    d.rect(-70, 16, 70, 27, d.vg(16, 27, rgb(12, 12, 16), rgb(44, 44, 52)), 3);
    pipe(d, -10, 21, 5.0); pipe(d, 10, 21, 5.0);
    d.rect(-72, 44, 72, 49.5, rgb(30, 8, 10), 2);
    d.rect(-71, 44.6, 71, 48.9, d.vg(44.6, 48.9, lensBot(brake), lensTop(brake)), 2);
    d.rect(-68, 46.2, 68, 47.4, brake ? rgb(255, 235, 225) : rgb(255, 100, 90));
    for (int s : {-1, 1}) d.rect(s < 0 ? -66 : 58, 44.8, s < 0 ? -58 : 66, 48.7, d.vg(44.8, 48.7, rgb(200, 100, 0), rgb(255, 190, 60)), 1);
    if (brake) glowAt(d, 0, 47, 74, 9);
    d.rect(-22, 51, 22, 61, rgb(14, 14, 18), 2);                                   // engine lid grille
    for (int i = 0; i < 5; ++i) d.rect(-21, 52 + i * 1.8, 21, 52.8 + i * 1.8, rgb(60, 60, 68));
    plate(d, 0, 30);
    const Tex* rear = M.add(d.finish());
    loft(m, {{-210, 74, 16, 64, 22, 8}, {-195, 84, 14, 78, 26, 8}, {-110, 88, 14, 86, 24, 8},
             {60, 88, 14, 76, 22, 8}, {210, 78, 16, 56, 18, 8}}, rear);
    // ducktail lip
    m.hexa({-60, 76, -205}, {60, 76, -205}, {62, 84, -212}, {-62, 84, -212}, {-60, 76, -170}, {60, 76, -170}, {62, 82, -170}, {-62, 82, -170},
           M_BODY, M_BODY, M_BODY, M_BODY);
    cabin(m, -175, 82, 62, 5, 124, 50, 60, 130, M.add(windowDecal(5, true, brake)));
    mirrors(m, 60, 72, 112, 122);
    for (int s : {-1, 1}) { m.wheel({s * 74.0, 32, -125}, 32, 30, false, s); m.wheel({s * 72.0, 31, 130}, 31, 24, false, s); }
    M.rearAxleZ = -125;
}

void supercar(Model& M, bool brake) {  // wide mid-engined wedge: round lamps, mesh grille, louvred engine cover
    Mesh& m = M.mesh;
    Decal d(92, 12, 62);
    d.paintAll();
    diffuser(d, 82, 12, 26, 11);
    d.rect(-38, 27, 38, 53, rgb(12, 12, 16), 3);
    for (double x = -37; x < 37; x += 3) d.rect(x, 28, x + 0.9, 52, rgb(48, 48, 56));
    for (double h = 28.5; h < 52; h += 3) d.rect(-37, h, 37, h + 0.8, rgb(48, 48, 56));
    plate(d, 0, 33, 15, 9);
    pipe(d, -14, 19.5, 4.4); pipe(d, 14, 19.5, 4.4);
    for (double lx : {-68.0, -48.0, 48.0, 68.0}) {
        d.ellipse(lx, 45, 9.5, 8, CHROME);
        d.ellipse(lx, 45, 8.2, 6.8, rgb(70, 0, 6));
        d.ellipse(lx, 45, 6.8, 5.6, sphere(d.X(lx), d.Y(45), 6.8 * d.k, 5.6 * d.k, brake ? rgb(255, 160, 150) : rgb(240, 50, 50), 0.6, 0.8, 24));
        if (brake) glowAt(d, lx, 45, 15, 12);
    }
    const Tex* rear = M.add(d.finish());
    loft(m, {{-225, 92, 12, 62, 10, 5}, {-205, 98, 10, 72, 12, 5}, {-40, 99, 10, 77, 14, 5},
             {120, 95, 10, 64, 14, 5}, {225, 84, 12, 44, 12, 5}}, rear);
    // louvred engine cover sitting on the deck
    m.hexa({-58, 76.5, -195}, {58, 76.5, -195}, {58, 77.5, -195}, {-58, 77.5, -195},
           {-58, 77.5, -70}, {58, 77.5, -70}, {58, 78.5, -70}, {-58, 78.5, -70}, M_BODY, M_DECAL, M_BODY, M_BODY, nullptr, M.add(slatsDecal(7, false)));
    // lip spoiler
    m.hexa({-80, 70, -222}, {80, 70, -222}, {82, 76, -228}, {-82, 76, -228}, {-80, 70, -200}, {80, 70, -200}, {82, 73, -200}, {-82, 73, -200},
           M_BODY, M_BODY, M_BODY, M_BODY);
    cabin(m, -60, 77, 62, -10, 110, 48, 50, 140, M.add(windowDecal(4, true, brake)));
    for (int s : {-1, 1}) m.box(s < 0 ? -101 : 97, s < 0 ? -97 : 101, 30, 52, -130, -60, M_BLACK);    // side intakes
    mirrors(m, 60, 72, 110, 120);
    for (int s : {-1, 1}) { m.wheel({s * 82.0, 34, -135}, 34, 34, false, s); m.wheel({s * 80.0, 32, 140}, 32, 28, false, s); }
    M.rearAxleZ = -135;
}

void suv(Model& M, bool brake) {
    Mesh& m = M.mesh;
    Decal d(92, 32, 112);
    d.paintAll();
    d.rect(-92, 32, 92, 46, d.vg(32, 46, rgb(26, 26, 30), rgb(70, 70, 78)), 3);    // bumper
    d.rect(-20, 42, 20, 45, rgb(96, 96, 106));
    pipe(d, -62, 37, 4.6);
    plate(d, 0, 54);
    for (int s : {-1, 1}) {
        double x0 = s < 0 ? -91 : 79, x1 = s < 0 ? -79 : 91;
        d.rect(x0, 62, x1, 106, CHROME, 2);
        d.rect(x0 + 1, 63, x1 - 1, 105, d.vg(63, 105, lensBot(brake), lensTop(brake)), 2);
        d.rect(x0 + 1, 72, x1 - 1, 78, d.vg(72, 78, rgb(200, 100, 0), rgb(255, 190, 60)));
        d.rect(x0 + 1, 64, x1 - 1, 70, rgb(235, 235, 242));
        if (brake) glowAt(d, (x0 + x1) / 2, 90, 12, 20);
    }
    d.rect(-14, 98, 14, 101, rgb(26, 26, 30), 1);                                 // handle
    d.rect(-80, 108.5, 80, 109.5, alpha(BLACK, 0.3));
    const Tex* rear = M.add(d.finish());
    loft(m, {{-232, 92, 32, 112, 10, 8}, {-218, 96, 30, 116, 14, 8}, {150, 96, 30, 118, 14, 8}, {232, 90, 32, 104, 20, 8}}, rear);
    cabin(m, -224, 112, 92, -212, 174, 86, 120, 170, M.add(windowDecal(9, true, brake)));
    // spare wheel on the tailgate, roof rails, roof spoiler, mirrors, tow ball
    m.wheel({0, 80, -244}, 30, 18, true, -1);
    m.box(-4, 4, 74, 86, -236, -232, M_BLACK);
    for (int s : {-1, 1}) m.box(s * 72 - 3, s * 72 + 3, 174, 181, -190, 100, M_CHROME);
    m.box(-80, 80, 172, 177, -216, -200, M_BLACK);
    armMirrors(m, 88, 97, 110, 117, 15, 140, 147, M_BLACK);
    m.box(-3, 3, 22, 30, -246, -232, M_CHROME);
    for (int s : {-1, 1}) { m.wheel({s * 80.0, 40, -140}, 40, 32, false, s); m.wheel({s * 80.0, 40, 150}, 40, 32, false, s); }
    M.rearAxleZ = -140;
}

void pickup(Model& M, bool brake) {
    Mesh& m = M.mesh;
    Decal d(94, 34, 112);
    d.paintAll();
    d.rect(-94, 34, 94, 48, cylY(d.Y(48), d.Y(34), CHROME, 0.45, 0.9, 30), 3);      // chrome bumper
    d.rect(-24, 43, 24, 46, rgb(40, 40, 48));
    pipe(d, -66, 38, 4.6);
    d.rect(-84, 68, 84, 69, alpha(BLACK, 0.3));
    d.rect(-84, 100, 84, 101, alpha(BLACK, 0.3));
    d.rect(-12, 102, 12, 107, rgb(20, 20, 24), 1.5);                              // tailgate handle
    d.rect(-10, 103.4, 10, 105.6, CHROME, 1);
    plate(d, 0, 54);
    for (int s : {-1, 1}) {
        double x0 = s < 0 ? -93 : 82, x1 = s < 0 ? -82 : 93;
        d.rect(x0, 62, x1, 104, CHROME, 2);
        d.rect(x0 + 1, 63, x1 - 1, 103, d.vg(63, 103, lensBot(brake), lensTop(brake)), 2);
        d.rect(x0 + 1, 64, x1 - 1, 72, rgb(235, 235, 242));
        if (brake) glowAt(d, (x0 + x1) / 2, 88, 12, 20);
    }
    const Tex* rear = M.add(d.finish());
    loft(m, {{-250, 94, 34, 112, 6, 6}, {-236, 96, 32, 114, 8, 6}, {180, 96, 32, 112, 8, 6}, {250, 90, 34, 98, 12, 6}}, rear);
    // open bed (dark floor between the side rails), cab with its own rear window
    m.hexa({-84, 113, -238}, {84, 113, -238}, {84, 114.5, -238}, {-84, 114.5, -238},
           {-84, 113, 22}, {84, 113, 22}, {84, 114.5, 22}, {-84, 114.5, 22}, M_BLACK, M_DECAL, M_BLACK, M_BLACK, nullptr, M.add(slatsDecal(6, true)));
    for (int s : {-1, 1}) m.box(s < 0 ? -96 : 84, s < 0 ? -84 : 96, 112, 118, -248, 24, M_BODY);
    m.box(-96, 96, 112, 118, -250, -238, M_BODY);
    Tex cabRear = windowDecal(14, true, brake);
    m.hexa({-92, 112, 26}, {92, 112, 26}, {86, 178, 34}, {-86, 178, 34}, {-92, 112, 170}, {92, 112, 170}, {86, 178, 120}, {-86, 178, 120},
           M_BODY, M_BODY, M_GLASS, M_GLASS, M.add(std::move(cabRear)));
    armMirrors(m, 88, 97, 112, 114, 18, 150, 157, M_BLACK);
    m.box(-4, 4, 26, 34, -262, -248, M_CHROME);
    for (int s : {-1, 1}) { m.wheel({s * 80.0, 40, -165}, 40, 32, false, s); m.wheel({s * 80.0, 40, 170}, 40, 32, false, s); }
    M.rearAxleZ = -165;
}

void sedan(Model& M, bool brake) {
    Mesh& m = M.mesh;
    Decal d(86, 22, 82);
    d.paintAll();
    d.rect(-86, 22, 86, 36, d.vg(22, 36, rgb(14, 14, 18), rgb(50, 50, 56)), 3);   // rubber bumper
    d.rect(-84, 34, 84, 35.6, CHROME);
    pipe(d, -58, 27, 4.0);
    plate(d, 0, 38);
    for (int s : {-1, 1}) {
        double x0 = s < 0 ? -82 : 24, x1 = s < 0 ? -24 : 82;
        d.rect(x0, 50, x1, 66, CHROME, 2);
        d.rect(x0 + 1, 51, x1 - 1, 65, d.vg(51, 65, lensBot(brake), lensTop(brake)), 1.5);
        double am = s < 0 ? x1 - 22 : x0 + 12, wh = s < 0 ? x1 - 11 : x0 + 1;
        d.rect(am, 51, am + 10, 65, d.vg(51, 65, rgb(200, 100, 0), rgb(255, 190, 60)));
        d.rect(wh, 51, wh + 10, 65, d.vg(51, 65, rgb(180, 180, 190), rgb(250, 250, 255)));
        for (double x = x0 + 3; x < x1 - 2; x += 4) d.rect(x, 51.5, x + 0.8, 64.5, alpha(BLACK, 0.18));
        if (brake) glowAt(d, (x0 + x1) / 2, 58, 34, 12);
    }
    d.rect(-76, 76, 76, 77, alpha(BLACK, 0.35));                                  // trunk shut line
    d.ellipse(0, 71, 1.6, 2, CHROME);
    const Tex* rear = M.add(d.finish());
    loft(m, {{-235, 86, 22, 82, 8, 6}, {-222, 90, 20, 88, 10, 6}, {170, 90, 20, 84, 10, 6}, {235, 86, 22, 70, 14, 6}}, rear);
    cabin(m, -140, 88, 80, -72, 138, 70, 60, 130, M.add(windowDecal(4, true, brake)));
    m.box(58, 60, 88, 140, -182, -178, M_BLACK);                                   // antenna
    mirrors(m, 78, 94, 112, 122, M_BLACK);
    for (int s : {-1, 1}) { m.wheel({s * 76.0, 32, -140}, 32, 26, false, s); m.wheel({s * 76.0, 32, 150}, 32, 26, false, s); }
    M.rearAxleZ = -140;
}

using BuildFn = void (*)(Model&, bool);
const BuildFn BUILD[CAR_TYPES] = {sport, coupe, supercar, suv, pickup, sedan};

// ----------------------------------------------------------------- renderer --
// A rendered frame at SS x resolution before the outline and downsampling.
struct HiFrame {
    int W = 0, H = 0;
    std::vector<Color> px;
    std::vector<unsigned char> paint;
    int x0 = 0, y0 = 0, x1 = -1, y1 = -1;   // for variants: the hi-res box where they differ from model 0 (inclusive)
};

// Render one or more models that share the same geometry (they differ only in their decal textures, e.g.
// brake lights off / on). Deferred: pass 1 rasterises the triangles into a z-buffer that keeps the
// triangle and its barycentrics per pixel, pass 2 shades each visible pixel once for every model.
// yaw: steering (radians, + = nose right), elev: how far we look down onto the car
struct Variant { const Model* model; bool glare; };   // models with the same geometry; glare = streaks on the glass
void renderModels(const std::vector<Variant>& variants, std::vector<HiFrame>& out, double yaw, double elev, int ss = SS) {
    std::vector<const Model*> models;
    std::vector<char> glare;                       // per variant (kept apart: avoids a false -Warray-bounds at -O3)
    for (auto& v : variants) { models.push_back(v.model); glare.push_back(v.glare); }
    const Model& M = *models[0];
    const int W = CAR_BMP_W * ss, H = CAR_BMP_H * ss;
    const size_t N = size_t(W) * H;
    std::vector<float> zb(N, 1e30f), bw0(N), bw1(N);
    std::vector<int> tid(N, -1);
    const double D = CAR_CAM_DIST;                 // camera distance (mild perspective)
    const double f = CAR_PX_PER_CM * ss * (D - 200);   // the tail end is drawn at CAR_PX_PER_CM
    const double cy = std::cos(yaw), sy = std::sin(yaw), ce = std::cos(elev), se = std::sin(elev);
    auto T = [&](V3 p) {
        V3 a{p.x * cy + p.z * sy, p.y, -p.x * sy + p.z * cy};             // yaw
        return V3{a.x, a.y * ce + a.z * se, a.z * ce - a.y * se};         // look down by elev
    };
    // anchor: horizontally the car's centre, vertically the ground under the rear axle
    V3 ancX = T({0, 0, 0}), ancY = T({0, 0, M.rearAxleZ});
    double ax = W / 2.0 - f * ancX.x / (D + ancX.z), ay = CAR_ANCHOR_Y * ss + f * ancY.y / (D + ancY.z);
    auto S = [&](V3 v) { return std::pair<double, double>{ax + f * v.x / (D + v.z), ay - f * v.y / (D + v.z)}; };

    const V3 L = norm({-0.42, 0.80, -0.45});        // light from the upper left, a little from behind the camera
    const size_t NT = M.mesh.tris.size();
    // per triangle: brightness factor k and specular term for its own material, body paint and decals
    struct TL { float k[3], sp[3]; };
    std::vector<TL> tl(NT);
    for (size_t ti = 0; ti < NT; ++ti) {
        const Tri& t = M.mesh.tris[ti];
        V3 v[3] = {T(t.p[0]), T(t.p[1]), T(t.p[2])};
        V3 n = norm(cross(v[1] - v[0], v[2] - v[0]));
        V3 toCam = norm(V3{0, 0, -D} - v[0]);
        if (dot(n, toCam) < 0) n = n * -1;          // shade the side that faces the camera
        double diff = std::max(0.0, dot(n, L)), nh = std::max(0.0, dot(n, norm(L + toCam)));
        const MatDef* mds[3] = {&MATS[t.mat], &MATS[M_BODY], &MATS[M_DECAL]};
        for (int q = 0; q < 3; ++q) {
            tl[ti].k[q] = float(mds[q]->amb + (1 - mds[q]->amb) * 1.1 * diff);
            tl[ti].sp[q] = float(255 * mds[q]->spec * std::pow(nh, mds[q]->shin));
        }
        auto s0 = S(v[0]), s1 = S(v[1]), s2 = S(v[2]);
        double area = (s1.first - s0.first) * (s2.second - s0.second) - (s2.first - s0.first) * (s1.second - s0.second);
        if (std::fabs(area) < 1e-9) continue;
        int x0 = std::max(0, int(std::floor(std::min({s0.first, s1.first, s2.first}))));
        int x1 = std::min(W - 1, int(std::ceil(std::max({s0.first, s1.first, s2.first}))));
        int y0 = std::max(0, int(std::floor(std::min({s0.second, s1.second, s2.second}))));
        int y1 = std::min(H - 1, int(std::ceil(std::max({s0.second, s1.second, s2.second}))));
        if (x0 > x1 || y0 > y1) continue;
        // barycentrics are affine in screen space: w = A x + B y + C
        const double inv = 1.0 / area;
        const double A0 = -(s2.second - s1.second) * inv, B0 = (s2.first - s1.first) * inv;
        const double C0 = (s1.first * s2.second - s2.first * s1.second) * inv;
        const double A1 = -(s0.second - s2.second) * inv, B1 = (s0.first - s2.first) * inv;
        const double C1 = (s2.first * s0.second - s0.first * s2.second) * inv;
        const double z0 = v[0].z, z1 = v[1].z, z2 = v[2].z;
        const double A2 = -A0 - A1, B2 = -B0 - B1, C2 = 1 - C0 - C1;
        for (int y = y0; y <= y1; ++y) {
            double py = y + 0.5;
            // the pixels of this row inside all three edges: each edge w = A x + B y + C >= 0 bounds x
            double lo = x0 + 0.5, hi = x1 + 0.5;
            bool empty = false;
            for (auto [A, B, C] : {std::array<double, 3>{A0, B0, C0}, {A1, B1, C1}, {A2, B2, C2}}) {
                double r = B * py + C;
                if (std::fabs(A) < 1e-12) { if (r < -1e-6) empty = true; continue; }
                double xb = (-1e-6 - r) / A;
                if (A > 0) lo = std::max(lo, xb); else hi = std::min(hi, xb);
            }
            if (empty) continue;
            int xs = std::max(x0, int(std::ceil(lo - 0.5)) - 1), xe = std::min(x1, int(std::floor(hi - 0.5)) + 1);
            if (xs > xe) continue;
            double w0 = A0 * (xs + 0.5) + B0 * py + C0, w1 = A1 * (xs + 0.5) + B1 * py + C1;
            size_t i = size_t(y) * W + xs;
            for (int x = xs; x <= xe; ++x, ++i, w0 += A0, w1 += A1) {
                double w2 = 1 - w0 - w1;
                if (w0 < -1e-6 || w1 < -1e-6 || w2 < -1e-6) continue;
                float z = float(w0 * z0 + w1 * z1 + w2 * z2);
                if (z >= zb[i]) continue;
                zb[i] = z; tid[i] = int(ti); bw0[i] = float(w0); bw1[i] = float(w1);
            }
        }
    }

    out.assign(models.size(), HiFrame{});
    for (auto& o : out) { o.W = W; o.H = H; o.px.assign(N, 0); o.paint.assign(N, 0); o.x0 = W; o.y0 = H; }
    // variants only need shading where a triangle's texture actually differs from model 0's
    std::vector<std::vector<char>> differs(models.size(), std::vector<char>(NT, 0));
    for (size_t mi = 1; mi < models.size(); ++mi)
        for (size_t ti = 0; ti < NT; ++ti) {
            const Tex* a = M.mesh.tris[ti].tex;
            const Tex* b = models[mi]->mesh.tris[ti].tex;
            differs[mi][ti] = (a == nullptr) != (b == nullptr) || (a && (a->bmp.px != b->bmp.px || a->mask != b->mask)) ||
                              (!a && M.mesh.tris[ti].mat == M_GLASS && glare[mi] != glare[0]);
        }
    for (size_t i = 0; i < N; ++i) {
        int ti = tid[i];
        if (ti < 0) continue;
        double w0 = bw0[i], w1 = bw1[i], w2 = 1 - w0 - w1;
        const TL& L3 = tl[size_t(ti)];
        auto shadeC = [&](Color base, int q) -> Color {
            float k = L3.k[q], sp = L3.sp[q];
            auto ch = [&](int c) { return int(std::clamp(c * k + sp, 0.0f, 255.0f)); };
            return rgb(ch((base >> 16) & 255), ch((base >> 8) & 255), ch(base & 255));
        };
        for (size_t mi = 0; mi < models.size(); ++mi) {
            if (mi > 0 && !differs[mi][size_t(ti)]) { out[mi].px[i] = out[0].px[i]; out[mi].paint[i] = out[0].paint[i]; continue; }
            if (mi > 0) {
                HiFrame& o = out[mi];
                int x = int(i % W), y = int(i / W);
                o.x0 = std::min(o.x0, x); o.x1 = std::max(o.x1, x); o.y0 = std::min(o.y0, y); o.y1 = std::max(o.y1, y);
            }
            const Tri& t = models[mi]->mesh.tris[size_t(ti)];
            const MatDef& md = MATS[t.mat];
            Color c;
            bool paint = md.paint;
            if (t.tex) {
                double u = w0 * t.u[0] + w1 * t.u[1] + w2 * t.u[2], vv = w0 * t.v[0] + w1 * t.v[1] + w2 * t.v[2];
                int tx = std::clamp(int(u * t.tex->bmp.w), 0, t.tex->bmp.w - 1), ty = std::clamp(int(vv * t.tex->bmp.h), 0, t.tex->bmp.h - 1);
                size_t txi = size_t(ty) * t.tex->bmp.w + tx;
                Color tc = t.tex->bmp.px[txi];
                if (!(tc >> 24)) tc = BODY;
                paint = t.tex->mask[txi] > 128;
                c = paint ? shadeC(tc, 1) : lerpColor(tc | 0xFF000000u, shadeC(tc | 0xFF000000u, 2), 0.35);
            } else {
                c = shadeC(md.c, 0);
                if (t.mat == M_GLASS && glare[mi]) {   // diagonal reflection streaks on glass
                    double px = double(i % W) + 0.5, py = double(i / W) + 0.5;
                    const double k = CAR_PX_PER_CM * ss;          // hi-res pixels per cm
                    double band = std::fmod(px * 0.8 + py + 4000 * k, 42.0 * k);
                    if (band < 4.8 * k) c = lerpColor(c, rgb(200, 220, 255), 0.35);
                }
            }
            out[mi].px[i] = c;
            out[mi].paint[i] = paint ? 255 : 0;
        }
    }
}

// dark outline around the silhouette (r hi-res pixels thick), then box-filter down by SS
// ss: downsampling factor, r: outline thickness in hi-res pixels (0 = the sprite default)
void finishFrames(std::vector<HiFrame>& hf, std::vector<Bitmap>& bmps, std::vector<std::vector<unsigned char>>& masks,
                  int ss = SS, int r = 0) {
    const int W = hf[0].W, H = hf[0].H;
    if (r <= 0) r = std::max(1, int(std::lround(ss * RENDER_SCALE * 0.75)));
    const Color outline = rgb(10, 10, 14);
    std::vector<unsigned char> cov(size_t(W) * H), t(size_t(W) * H);
    for (size_t i = 0; i < cov.size(); ++i) cov[i] = hf[0].px[i] >> 24 ? 1 : 0;
    for (int y = 0; y < H; ++y) {                  // separable dilation with a running count
        const unsigned char* row = &cov[size_t(y) * W];
        int cnt = 0;
        for (int x = 0; x < std::min(W, r); ++x) cnt += row[x];
        for (int x = 0; x < W; ++x) {
            if (x + r < W) cnt += row[x + r];
            if (x - r - 1 >= 0) cnt -= row[x - r - 1];
            t[size_t(y) * W + x] = cnt > 0;
        }
    }
    std::vector<unsigned char> dil(size_t(W) * H);
    for (int x = 0; x < W; ++x) {
        int cnt = 0;
        for (int y = 0; y < std::min(H, r); ++y) cnt += t[size_t(y) * W + x];
        for (int y = 0; y < H; ++y) {
            if (y + r < H) cnt += t[size_t(y + r) * W + x];
            if (y - r - 1 >= 0) cnt -= t[size_t(y - r - 1) * W + x];
            dil[size_t(y) * W + x] = cnt > 0;
        }
    }
    const int w = W / ss, h = H / ss;
    bmps.assign(hf.size(), Bitmap());
    masks.assign(hf.size(), {});
    for (size_t k = 0; k < hf.size(); ++k) {
        HiFrame& F = hf[k];
        // model 0: everything; variants: only the (final-pixel) box where they differ, the rest stays empty
        int fx0 = 0, fy0 = 0, fx1 = w, fy1 = h;
        if (k > 0) {
            if (F.x1 < F.x0) { bmps[k] = bmps[0]; masks[k] = masks[0]; continue; }
            fx0 = std::max(0, F.x0 / ss - 1); fy0 = std::max(0, F.y0 / ss - 1);
            fx1 = std::min(w, F.x1 / ss + 2); fy1 = std::min(h, F.y1 / ss + 2);
        }
        for (int y = fy0 * ss; y < fy1 * ss; ++y)
            for (int x = fx0 * ss; x < fx1 * ss; ++x) {
                size_t i = size_t(y) * W + x;
                if (dil[i] && !cov[i]) F.px[i] = outline;
            }
        Bitmap b = k ? bmps[0] : Bitmap(w, h);                       // variants start as a copy of model 0
        std::vector<unsigned char> m = k ? masks[0] : std::vector<unsigned char>(size_t(w) * h);
        for (int y = fy0; y < fy1; ++y)
            for (int x = fx0; x < fx1; ++x) {
                unsigned R = 0, G = 0, B = 0, A = 0, P = 0;
                for (int sy = 0; sy < ss; ++sy) {
                    size_t i = size_t(y * ss + sy) * W + size_t(x * ss);
                    for (int sx = 0; sx < ss; ++sx, ++i) {
                        Color c = F.px[i];
                        unsigned a = c >> 24;
                        R += ((c >> 16) & 255) * a; G += ((c >> 8) & 255) * a; B += (c & 255) * a; A += a;
                        P += F.paint[i];
                    }
                }
                if (!A) continue;
                unsigned oa = A / (ss * ss);
                if (!oa) continue;
                b.px[size_t(y) * w + x] = (Color(oa) << 24) | (Color(R / A) << 16) | (Color(G / A) << 8) | Color(B / A);
                m[size_t(y) * w + x] = (unsigned char)(P / (ss * ss));
            }
        bmps[k] = std::move(b);
        masks[k] = std::move(m);
    }
}

struct Base { Bitmap bmp; std::vector<unsigned char> mask; int ox = 0, oy = 0; };   // cropped to the visible pixels
struct BasePair { Base car; std::vector<Base> brake; };   // brake = small overlays with the lit brake lights

// cut out [x0,x1) x [y0,y1) of a full-size frame (pixels + paint mask)
Base cut(const Bitmap& bmp, const std::vector<unsigned char>& mask, int x0, int y0, int x1, int y1) {
    Base b;
    int w = std::max(1, x1 - x0), h = std::max(1, y1 - y0);
    b.bmp = Bitmap(w, h);
    b.mask.assign(size_t(w) * h, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int sx = x + x0, sy = y + y0;
            if (sx < 0 || sy < 0 || sx >= bmp.w || sy >= bmp.h) continue;   // outside the frame: transparent
            size_t si = size_t(sy) * bmp.w + sx, di = size_t(y) * w + x;
            b.bmp.px[di] = bmp.px[si];
            b.mask[di] = mask[si];
        }
    b.ox = x0; b.oy = y0;
    return b;
}

// the car cropped to its visible pixels, plus the region where the braking version differs. The overlay's
// origin is kept on a 16-pixel grid relative to the car's crop, so their mip levels line up exactly.
BasePair makePair(const Bitmap& car, const std::vector<unsigned char>& carMask, const Bitmap& brk,
                  const std::vector<unsigned char>& brkMask) {
    BasePair bp;
    Bitmap c = car;
    c.computeBounds();
    bp.car = cut(car, carMask, c.bx0, c.by0, c.bx1, c.by1);
    // where the braking version differs
    std::vector<unsigned char> diff(size_t(car.w) * car.h, 0);
    for (size_t i = 0; i < diff.size(); ++i) {
        Color a = car.px[i], b = brk.px[i];
        if (a == b) continue;
        int d = 0;
        for (int sh = 0; sh < 32; sh += 8) d = std::max(d, std::abs(int((a >> sh) & 255) - int((b >> sh) & 255)));
        diff[i] = d >= 6;
    }
    // split into separate boxes (high brake light, left and right lamps...): bands of rows, then runs of columns
    const int GAP = 12, G = 16;
    auto rowHas = [&](int y, int x0, int x1) { for (int x = x0; x < x1; ++x) if (diff[size_t(y) * car.w + x]) return true; return false; };
    auto colHas = [&](int x, int y0, int y1) { for (int y = y0; y < y1; ++y) if (diff[size_t(y) * car.w + x]) return true; return false; };
    for (int y = 0; y < car.h;) {
        if (!rowHas(y, 0, car.w)) { ++y; continue; }
        int y0 = y, last = y;
        for (; y < car.h && y - last <= GAP; ++y) if (rowHas(y, 0, car.w)) last = y;
        int y1 = last + 1;
        for (int x = 0; x < car.w;) {
            if (!colHas(x, y0, y1)) { ++x; continue; }
            int x0 = x, lx = x;
            for (; x < car.w && x - lx <= GAP; ++x) if (colHas(x, y0, y1)) lx = x;
            int x1 = lx + 1;
            // keep the overlay origin on a 16-pixel grid relative to the car crop so the mip levels line up
            int gx = bp.car.ox + int(std::floor(double(x0 - 1 - bp.car.ox) / G)) * G;
            int gy = bp.car.oy + int(std::floor(double(y0 - 1 - bp.car.oy) / G)) * G;
            Base o = cut(brk, brkMask, gx, gy, x1 + 1, y1 + 1);
            o.ox = gx; o.oy = gy;
            bp.brake.push_back(std::move(o));
        }
    }
    return bp;
}

// The car's shadow on the road, seen from the same camera as the car frame, so it turns with the car. Every
// vertex is projected along the sun direction onto the ground (sun high up to the left, fixed relative to
// the camera): the shadow falls to the right of the car. Stored at half resolution (it is blurry anyway).
CarFrame footprint(const Model& M, double yaw, double elev) {
    // rasterised at frame resolution, then halved; with room around the frame (the shadow of a turned car
    // can reach past the frame's edge, it must not be cut off there)
    const int MX = CAR_BMP_W / 2 / 2 * 2, MY = CAR_BMP_H / 4 / 2 * 2;
    const int W = CAR_BMP_W + 2 * MX, H = CAR_BMP_H + MY;
    std::vector<unsigned char> cov(size_t(W) * H, 0);
    const double D = 2000, f = CAR_PX_PER_CM * (D - 200);
    const double cy = std::cos(yaw), sy = std::sin(yaw), ce = std::cos(elev), se = std::sin(elev);
    auto T = [&](V3 p) {
        V3 a{p.x * cy + p.z * sy, p.y, -p.x * sy + p.z * cy};
        return V3{a.x, a.y * ce + a.z * se, a.z * ce - a.y * se};
    };
    V3 ancX = T({0, 0, 0}), ancY = T({0, 0, M.rearAxleZ});
    double ax = W / 2.0 - f * ancX.x / (D + ancX.z), ay = CAR_ANCHOR_Y + f * ancY.y / (D + ancY.z);   // frame centre = W / 2
    auto S = [&](V3 v) { return std::pair<double, double>{ax + f * v.x / (D + v.z), ay - f * v.y / (D + v.z)}; };
    // towards the sun, in camera-aligned coordinates: upper left and a little behind the viewer, so the shadow
    // falls to the right and away from us (it must not reach down past the wheels: nearer road covers that)
    const V3 sun = norm({-0.7, 1.0, -0.25});
    auto castT = [&](V3 p) {
        V3 a{p.x * cy + p.z * sy, p.y, -p.x * sy + p.z * cy};             // yaw (car -> camera aligned)
        double h = std::max(0.0, a.y);
        a = {a.x - sun.x * h / sun.y, 0, a.z - sun.z * h / sun.y};        // along the sun ray to the ground
        return V3{a.x, a.y * ce + a.z * se, a.z * ce - a.y * se};         // look down by elev
    };
    for (const Tri& t : M.mesh.tris) {
        std::pair<double, double> s3[3];
        for (int k = 0; k < 3; ++k) {
            V3 p = t.p[k];
            p.x *= 1.03; p.z *= 1.02;                       // a touch larger than the body
            s3[k] = S(castT(p));
        }
        double area = (s3[1].first - s3[0].first) * (s3[2].second - s3[0].second) - (s3[2].first - s3[0].first) * (s3[1].second - s3[0].second);
        if (std::fabs(area) < 1e-9) continue;
        int x0 = std::max(0, int(std::floor(std::min({s3[0].first, s3[1].first, s3[2].first}))));
        int x1 = std::min(W - 1, int(std::ceil(std::max({s3[0].first, s3[1].first, s3[2].first}))));
        int y0 = std::max(0, int(std::floor(std::min({s3[0].second, s3[1].second, s3[2].second}))));
        int y1 = std::min(H - 1, int(std::ceil(std::max({s3[0].second, s3[1].second, s3[2].second}))));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                double px = x + 0.5, py = y + 0.5;
                double w0 = ((s3[1].first - px) * (s3[2].second - py) - (s3[2].first - px) * (s3[1].second - py)) / area;
                double w1 = ((s3[2].first - px) * (s3[0].second - py) - (s3[0].first - px) * (s3[2].second - py)) / area;
                if (w0 < 0 || w1 < 0 || 1 - w0 - w1 < 0) continue;
                cov[size_t(y) * W + x] = 255;
            }
    }
    // halve, then soften with a few box blur passes
    const int w = W / 2, h = H / 2;
    std::vector<float> a(size_t(w) * h), t(a.size());
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            size_t i = size_t(y * 2) * W + x * 2;
            a[size_t(y) * w + x] = (cov[i] + cov[i + 1] + cov[i + W] + cov[i + W + 1]) / (4 * 255.0f);
        }
    const int r = std::max(1, int(std::lround(3 * RENDER_SCALE / 2.0)));
    for (int pass = 0; pass < 2; ++pass) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                float sum = 0; int n = 0;
                for (int k = std::max(0, x - r); k <= std::min(w - 1, x + r); ++k, ++n) sum += a[size_t(y) * w + k];
                t[size_t(y) * w + x] = sum / n;
            }
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                float sum = 0; int n = 0;
                for (int k = std::max(0, y - r); k <= std::min(h - 1, y + r); ++k, ++n) sum += t[size_t(k) * w + x];
                a[size_t(y) * w + x] = sum / n;
            }
    }
    Bitmap full(w, h);
    for (size_t i = 0; i < a.size(); ++i) {
        float v = std::min(1.0f, a[i] * 1.15f);
        if (v > 0.004f) full.px[i] = withAlpha(rgb(0, 0, 0), int(v * v * (3 - 2 * v) * 255));
    }
    full.computeBounds();
    CarFrame fr;
    int bw = std::max(1, full.bx1 - full.bx0), bh = std::max(1, full.by1 - full.by0);
    fr.bmp = Bitmap(bw, bh);
    for (int y = 0; y < bh; ++y)
        for (int x = 0; x < bw; ++x) fr.bmp.px[size_t(y) * bw + x] = full.px[size_t(y + full.by0) * w + x + full.bx0];
    fr.scale = 2;
    fr.ox = full.bx0 * 2 - MX; fr.oy = full.by0 * 2;   // may lie outside the frame
    return fr;
}

// The pixels where 'var' differs from 'base' (everything else transparent), cut into boxes whose origin lies
// on a 16-pixel grid relative to (gx0, gy0) - the crop origin of the frame it is drawn over.
std::vector<Base> diffOverlays(const Bitmap& base, const Bitmap& var, const std::vector<unsigned char>& varMask, int gx0, int gy0) {
    std::vector<Base> res;
    const int W = base.w, H = base.h, G = 16;
    std::vector<unsigned char> diff(size_t(W) * H, 0);
    for (size_t i = 0; i < diff.size(); ++i) {
        Color a = base.px[i], b = var.px[i];
        int d = 0;
        for (int sh = 0; sh < 32; sh += 8) d = std::max(d, std::abs(int((a >> sh) & 255) - int((b >> sh) & 255)));
        diff[i] = d >= 3;
    }
    // grow by one pixel so anti-aliased streak edges come along
    std::vector<unsigned char> grown = diff;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (diff[size_t(y) * W + x])
                for (int yy = std::max(0, y - 1); yy <= std::min(H - 1, y + 1); ++yy)
                    for (int xx = std::max(0, x - 1); xx <= std::min(W - 1, x + 1); ++xx) grown[size_t(yy) * W + xx] = 1;
    int x0 = W, y0 = H, x1 = 0, y1 = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (grown[size_t(y) * W + x]) { x0 = std::min(x0, x); x1 = std::max(x1, x + 1); y0 = std::min(y0, y); y1 = y + 1; }
    if (x0 >= x1) return res;
    int gx = gx0 + int(std::floor(double(x0 - gx0) / G)) * G, gy = gy0 + int(std::floor(double(y0 - gy0) / G)) * G;
    Base o = cut(var, varMask, gx, gy, x1, y1);
    for (int y = 0; y < o.bmp.h; ++y)
        for (int x = 0; x < o.bmp.w; ++x) {
            int sx = gx + x, sy = gy + y;
            if (sx < 0 || sy < 0 || sx >= W || sy >= H || !grown[size_t(sy) * W + sx]) {
                o.bmp.px[size_t(y) * o.bmp.w + x] = TRANSPARENT;
                o.mask[size_t(y) * o.bmp.w + x] = 0;
            }
        }
    res.push_back(std::move(o));
    return res;
}

// where model points end up in a frame rendered at (yaw, elev), as fractions of the whole frame: the same
// projection and anchor as renderModels
std::vector<std::array<double, 2>> projectPoints(const Model& M, double yaw, double elev, const std::vector<V3>& pts) {
    const int W = CAR_BMP_W * SS;
    const double D = CAR_CAM_DIST, f = CAR_PX_PER_CM * SS * (D - 200);
    const double cy = std::cos(yaw), sy = std::sin(yaw), ce = std::cos(elev), se = std::sin(elev);
    auto T = [&](V3 p) {
        V3 a{p.x * cy + p.z * sy, p.y, -p.x * sy + p.z * cy};
        return V3{a.x, a.y * ce + a.z * se, a.z * ce - a.y * se};
    };
    V3 ancX = T({0, 0, 0}), ancY = T({0, 0, M.rearAxleZ});
    double ax = W / 2.0 - f * ancX.x / (D + ancX.z), ay = CAR_ANCHOR_Y * SS + f * ancY.y / (D + ancY.z);
    std::vector<std::array<double, 2>> out;
    for (const V3& p : pts) {
        V3 v = T(p);
        out.push_back({(ax + f * v.x / (D + v.z)) / SS / CAR_BMP_W, (ay - f * v.y / (D + v.z)) / SS / CAR_BMP_H});
    }
    return out;
}

CarFrame toFrame(Base&& b) {
    CarFrame f;
    f.ox = b.ox; f.oy = b.oy;
    f.bmp = std::move(b.bmp);
    f.mask.w = f.bmp.w; f.mask.h = f.bmp.h;
    f.mask.px = std::move(b.mask);
    return f;
}
} // namespace

void generateCarSprites(CarSprites out[CAR_TYPES]) {
    // per type three versions of the model (same triangles, different decals): plain, brake lights on, and with
    // reflections on the glass. The plain frames get mirrored for left turns; brake lights are symmetric and
    // mirror with them; reflections must not jump sides, so they are separate overlays rendered for every yaw.
    static Model models[CAR_TYPES][3];
    {
        std::vector<std::thread> builders;
        for (int type = 0; type < CAR_TYPES; ++type)
            builders.emplace_back([type] {
                for (auto& m : models[type]) m = Model();
                g_glare = false;
                g_pipes.clear();
                g_tyres.clear();
                BUILD[type](models[type][0], false);
                for (auto& p : g_pipes) models[type][0].exhausts.push_back({p.first, p.second, g_tailZ - 2});
                for (auto& p : g_tyres) models[type][0].tyres.push_back({p.first, 0, p.second});
                BUILD[type](models[type][1], true);
                g_glare = true;
                BUILD[type](models[type][2], false);
            });
        for (auto& b : builders) b.join();
    }
    std::atomic<int> next{0};
    const int tasks = CAR_TYPES * CAR_YAWS;
    auto worker = [&] {
        for (int t; (t = next++) < tasks;) {
            int type = t / CAR_YAWS, s = t % CAR_YAWS;
            const Model* model = models[type];
            // all versions must have the same triangles (they only differ in textures) to share a raster
            if (model[0].mesh.tris.size() != model[1].mesh.tris.size() || model[0].mesh.tris.size() != model[2].mesh.tris.size())
                std::abort();
            const double yaw = s * CAR_YAW_STEP * M_PI / 180;
            for (int pi = 0; pi < CAR_ELEV; ++pi) {
                const double el = CAR_ELEV_DEG[pi] * M_PI / 180;
                std::vector<HiFrame> hf;
                renderModels({{&model[0], false}, {&model[1], false}, {&model[2], true}}, hf, yaw, el);
                std::vector<Bitmap> img;
                std::vector<std::vector<unsigned char>> mask;
                finishFrames(hf, img, mask);
                BasePair bp = makePair(img[0], mask[0], img[1], mask[1]);
                CarSprites& cs = out[type];
                cs.car[s][pi] = toFrame(std::move(bp.car));
                const CarFrame& car = cs.car[s][pi];
                cs.brake[s][pi].clear();
                for (Base& o : bp.brake) cs.brake[s][pi].push_back(toFrame(std::move(o)));
                auto& gr = cs.glare[s + CAR_YAWS - 1][pi];
                gr.clear();
                for (Base& o : diffOverlays(img[0], img[2], mask[2], car.ox, car.oy)) gr.push_back(toFrame(std::move(o)));
                if (s > 0) {
                    // turned left: the car frame is the mirrored right one; render the reflections for real
                    std::vector<HiFrame> hl;
                    renderModels({{&model[0], false}, {&model[2], true}}, hl, -yaw, el);
                    std::vector<Bitmap> il;
                    std::vector<std::vector<unsigned char>> ml;
                    finishFrames(hl, il, ml);
                    int mirroredX = car.frameW - car.ox - car.bmp.w;      // where the mirrored crop starts
                    auto& gl = cs.glare[-s + CAR_YAWS - 1][pi];
                    gl.clear();
                    for (Base& o : diffOverlays(il[0], il[1], ml[1], mirroredX, car.oy)) gl.push_back(toFrame(std::move(o)));
                }
                cs.exhaust[s][pi] = projectPoints(model[0], yaw, el, model[0].exhausts);
                cs.tyres[s][pi] = projectPoints(model[0], yaw, el, model[0].tyres);
                if (pi % 2 == 1)    // shadows for elevations 1, 3, 5, turned right (s) and left (-s)
                    for (int sg : {1, -1}) {
                        if (s == 0 && sg < 0) continue;
                        cs.shadow[sg * s + CAR_YAWS - 1][pi / 2] = footprint(model[0], sg * yaw, el);
                    }
            }
        }
    };
    unsigned n = std::max(2u, std::thread::hardware_concurrency());
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < n; ++i) workers.emplace_back(worker);
    for (auto& w : workers) w.join();
}

// One large picture of a car (for the app icon), painted in 'paint'. It is rendered at ss samples per sprite
// pixel, outlined (outline = thickness in those samples) and box-filtered down by 'down': the result has
// ss / down x the sprite's resolution. Returns the full frame; the rear wheels touch the ground at row
// CAR_ANCHOR_Y * ss / down, and the car is centred horizontally.
Bitmap renderCarImage(int type, Color paint, double yawDeg, double elevDeg, int ss, int down, int outline) {
    Model m;
    g_glare = true;
    BUILD[type](m, false);
    std::vector<HiFrame> hf;
    renderModels({{&m, true}}, hf, yawDeg * M_PI / 180, elevDeg * M_PI / 180, ss);
    std::vector<Bitmap> img;
    std::vector<std::vector<unsigned char>> mask;
    finishFrames(hf, img, mask, down, outline);
    Bitmap& b = img[0];
    paint |= 0xFF000000u;
    for (size_t i = 0; i < b.px.size(); ++i) {           // recolour the grey paint, like the game does
        unsigned p = mask[0][i];
        if (!p || !(b.px[i] >> 24)) continue;
        Color c = b.px[i];
        double v = ((c >> 16) & 255) / 170.0;
        Color col = v <= 1 ? shade(paint, v) : lerpColor(paint, rgb(255, 255, 255), std::min(1.0, (v - 1) / 0.5));
        b.px[i] = (lerpColor(c, col, p / 255.0) & 0x00FFFFFFu) | (c & 0xFF000000u);
    }
    return std::move(b);
}

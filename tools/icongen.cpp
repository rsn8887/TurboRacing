// App icons for Turbo Racing Mayhem, painted by the asset tool:  turbo_assetgen --icons <folder>
//
//   icon.png         1024x1024 master, square, no transparency
//   icon_switch.jpg  256x256 JPEG for the Switch homebrew menu (nx_create_nro ICON)
//   icon_macos.png   1024x1024 with the rounded macOS shape and margin
//   TurboRacingMayhem.icns the macOS app icon (all sizes, built from icon_macos.png)
//   android/mipmap-*/ic_launcher.png             Android launcher icons (the macOS shape, for older launchers)
//   android/mipmap-*/ic_launcher_foreground.png  the foreground of the adaptive icon (Android 8+): the rounded
//                                                icon inside the safe zone; the background colour is in the app
//
// The scene: a sunset over a road, with the red sport car seen from behind, rendered from its 3D model.
#include "icongen.h"
#include "cargen.h"
#include "painter.h"
#include "sprites.h"
#include "stb_image_write.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace {

// area-average resize (for shrinking only), premultiplied alpha
Bitmap resized(const Bitmap& src, int w, int h) {
    Bitmap out(w, h);
    const double fx = double(src.w) / w, fy = double(src.h) / h;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            double x0 = x * fx, x1 = x0 + fx, y0 = y * fy, y1 = y0 + fy;
            double R = 0, G = 0, B = 0, A = 0, W = 0;
            for (int sy = int(y0); sy < int(std::ceil(y1)) && sy < src.h; ++sy) {
                double wy = std::min(y1, sy + 1.0) - std::max(y0, double(sy));
                for (int sx = int(x0); sx < int(std::ceil(x1)) && sx < src.w; ++sx) {
                    double wgt = wy * (std::min(x1, sx + 1.0) - std::max(x0, double(sx)));
                    Color c = src.px[size_t(sy) * src.w + sx];
                    double a = (c >> 24) * wgt;
                    R += ((c >> 16) & 255) * a; G += ((c >> 8) & 255) * a; B += (c & 255) * a; A += a; W += wgt;
                }
            }
            if (A <= 0) continue;
            auto ch = [&](double v) { return Color(std::clamp(int(std::lround(v / A)), 0, 255)); };
            out.px[size_t(y) * w + x] = (Color(std::clamp(int(std::lround(A / W)), 0, 255)) << 24) | (ch(R) << 16) | (ch(G) << 8) | ch(B);
        }
    return out;
}

std::vector<unsigned char> rgbaBytes(const Bitmap& b, bool dropAlpha = false) {
    std::vector<unsigned char> v;
    v.reserve(b.px.size() * (dropAlpha ? 3 : 4));
    for (Color c : b.px) {
        v.push_back((c >> 16) & 255); v.push_back((c >> 8) & 255); v.push_back(c & 255);
        if (!dropAlpha) v.push_back(c >> 24);
    }
    return v;
}

void appendBytes(void* ctx, void* data, int size) {
    auto* v = static_cast<std::vector<unsigned char>*>(ctx);
    v->insert(v->end(), static_cast<unsigned char*>(data), static_cast<unsigned char*>(data) + size);
}
std::vector<unsigned char> pngBytes(const Bitmap& b) {
    std::vector<unsigned char> out;
    auto px = rgbaBytes(b);
    stbi_write_png_to_func(appendBytes, &out, b.w, b.h, 4, px.data(), b.w * 4);
    return out;
}
bool writeFile(const std::string& path, const std::vector<unsigned char>& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    return std::fclose(f) == 0 && ok;
}

// the scene, 1024x1024, full bleed
Bitmap paintScene() {
    const double HZ = 134;                       // horizon (in 256 units)
    Painter p(256, 256, 8, 4);                   // 2048x2048 hi-res, finished at 1024x1024

    // sky: deep violet at the top, through magenta to a warm glow at the horizon
    p.rect(0, 0, 256, 70, vgrad(0, 70, rgb(26, 14, 66), rgb(112, 30, 120)));
    p.rect(0, 70, 256, HZ - 70, vgrad(70, HZ, rgb(112, 30, 120), rgb(255, 120, 84)));
    // stars
    for (int i = 0; i < 40; ++i) {
        double x = hashNoise(i, 3) * 256, y = hashNoise(i, 7) * 60, r = 0.35 + hashNoise(i, 11) * 0.5;
        p.ellipse(x, y, r, r, withAlpha(rgb(255, 240, 255), int(120 + 120 * hashNoise(i, 13))));
    }
    // the sun, with gaps in its lower half
    const double sx = 128, sy = 88, sr = 58;
    p.ellipse(sx, sy, sr + 6, sr + 6, withAlpha(rgb(255, 200, 120), 60));
    p.ellipse(sx, sy, sr, sr, Paint([=](double, double y) -> Color {
        double t = (y - (sy - sr)) / (2 * sr);
        if (y > sy - 10) {                       // gaps that get thicker towards the bottom
            double k = (y - (sy - 10)) / (sr + 10);
            if (std::fmod(y - (sy - 10), 10.0) < 1.0 + 5.5 * k) return TRANSPARENT;
        }
        return lerpColor(rgb(255, 238, 120), rgb(255, 70, 140), std::clamp(t, 0.0, 1.0));
    }));
    // distant hills
    p.poly({0, HZ, 0, 118, 22, 110, 48, 121, 70, 113, 92, 124, 104, HZ}, rgb(70, 26, 96));
    p.poly({150, HZ, 166, 120, 186, 108, 210, 118, 234, 106, 256, 116, 256, HZ}, rgb(70, 26, 96));
    p.poly({0, HZ, 0, 126, 30, 121, 60, 128, 80, HZ}, rgb(48, 18, 74));
    p.poly({176, HZ, 200, 124, 226, 128, 256, 121, 256, HZ}, rgb(48, 18, 74));

    // the ground: grass, kerbs and road in perspective, with the game's light / dark bands
    p.rect(0, HZ, 256, 256 - HZ, Paint([=](double x, double y) -> Color {
        double d = std::max(0.05, y - HZ);
        double hw = 236 * d / (256 - HZ) + 1.5;  // road half width
        double dx = std::fabs(x - 128);
        bool band = int(std::floor(240.0 / d + 0.35)) % 2 == 0;
        double fog = std::clamp(1 - d / 26, 0.0, 1.0) * 0.55;   // haze near the horizon
        Color c;
        if (dx < hw * 0.84) {
            c = band ? rgb(88, 88, 98) : rgb(98, 98, 108);
            if (band && dx < hw * 0.022) c = rgb(240, 240, 230);                          // centre dashes
            if (std::fabs(dx - hw * 0.79) < hw * 0.018) c = rgb(235, 235, 225);           // edge lines
        } else if (dx < hw) {
            c = band ? rgb(222, 28, 36) : rgb(240, 240, 240);                            // kerbs
        } else {
            c = band ? rgb(28, 132, 70) : rgb(36, 150, 80);                              // grass
        }
        return lerpColor(c, rgb(214, 96, 110), fog);
    }));

    // the car: red sport car from behind, turned a little, with its shadow
    const int SSR = 8, DOWN = 2;                 // 4 hi-res pixels per sprite pixel: the car fills ~2/3 of the width
    const double groundY = 222;                  // where the rear wheels touch the road (units)
    // look down onto the car at the angle whose ground plane vanishes at this picture's horizon, so that the
    // car stands flat on the road with all four wheels on it (the renderer's focal length, in hi-res pixels)
    const double focal = CAR_PX_PER_CM * SSR / DOWN * (CAR_CAM_DIST - 200);
    const double elev = std::atan((groundY - HZ) * p.ss / focal) * 180 / M_PI;
    Bitmap car = renderCarImage(0, CAR_COLORS[0], 0, elev, SSR, DOWN, 11);   // straight from behind
    p.ellipse(128, groundY + 4, 84, 11, withAlpha(rgb(10, 4, 20), 120));
    p.image(car, (p.hi.w / 2.0 - car.w / 2.0 - p.mx) / p.ss, groundY - double(CAR_ANCHOR_Y * SSR / DOWN) / p.ss);
    return p.finish(rgb(0, 0, 0), 0, false);
}

// macOS shape: rounded square of 824 px with a 100 px margin and a soft shadow (Apple's icon grid)
Bitmap macShape(const Bitmap& scene) {
    const int S = 1024, M = 100, C = S - 2 * M;
    const double R = 185;
    Bitmap inner = resized(scene, C, C);
    Bitmap out(S, S);
    // coverage of the rounded square at (x, y) relative to its corner, 4x4 samples
    auto cover = [&](double x, double y, double size, double r) {
        int n = 0;
        for (int sy = 0; sy < 4; ++sy)
            for (int sx = 0; sx < 4; ++sx) {
                double px = x + (sx + 0.5) / 4, py = y + (sy + 0.5) / 4;
                double qx = std::max({r - px, px - (size - r), 0.0}), qy = std::max({r - py, py - (size - r), 0.0});
                if (px >= 0 && py >= 0 && px <= size && py <= size && qx * qx + qy * qy <= r * r) ++n;
            }
        return n / 16.0;
    };
    // shadow: the shape, offset down and blurred
    std::vector<float> sh(size_t(S) * S, 0), tmp(size_t(S) * S);
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) sh[size_t(y) * S + x] = float(cover(x - M, y - M - 10, C, R));
    for (int pass = 0; pass < 3; ++pass) {      // three box blurs ~ a gaussian
        const int r = 9;
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x) {
                float s = 0;
                for (int k = -r; k <= r; ++k) s += sh[size_t(y) * S + std::clamp(x + k, 0, S - 1)];
                tmp[size_t(y) * S + x] = s / (2 * r + 1);
            }
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x) {
                float s = 0;
                for (int k = -r; k <= r; ++k) s += tmp[size_t(std::clamp(y + k, 0, S - 1)) * S + x];
                sh[size_t(y) * S + x] = s / (2 * r + 1);
            }
    }
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            Color c = withAlpha(rgb(0, 0, 0), int(sh[size_t(y) * S + x] * 90));
            double a = (x >= M && y >= M && x < M + C && y < M + C) ? cover(x - M, y - M, C, R) : 0;
            if (a > 0) {
                Color img = inner.px[size_t(y - M) * C + (x - M)] | 0xFF000000u;
                c = blendOver(c, withAlpha(img, int(std::lround(a * 255))));
            }
            out.px[size_t(y) * S + x] = c;
        }
    return out;
}

// .icns: a header and one PNG per size (macOS 10.7+ reads PNG data for all of these)
bool writeIcns(const std::string& path, const Bitmap& master) {
    struct E { const char* type; int size; };
    const E entries[] = {{"icp4", 16}, {"icp5", 32}, {"icp6", 64}, {"ic07", 128}, {"ic08", 256}, {"ic09", 512},
                         {"ic10", 1024}, {"ic11", 32}, {"ic12", 64}, {"ic13", 256}, {"ic14", 512}};
    std::vector<unsigned char> body;
    auto be32 = [](std::vector<unsigned char>& v, size_t n) {
        for (int s = 24; s >= 0; s -= 8) v.push_back((unsigned char)(n >> s));
    };
    for (const E& e : entries) {
        auto png = pngBytes(e.size == master.w ? master : resized(master, e.size, e.size));
        body.insert(body.end(), e.type, e.type + 4);
        be32(body, png.size() + 8);
        body.insert(body.end(), png.begin(), png.end());
    }
    std::vector<unsigned char> file = {'i', 'c', 'n', 's'};
    be32(file, body.size() + 8);
    file.insert(file.end(), body.begin(), body.end());
    return writeFile(path, file);
}

} // namespace

bool generateIcons(const std::string& dir, std::string& err) {
    Bitmap scene = paintScene();
    Bitmap mac = macShape(scene);
    Bitmap sw = resized(scene, 256, 256);
    auto fail = [&](const std::string& f) { err = "cannot write " + dir + "/" + f; return false; };
    auto rgba = rgbaBytes(scene);
    if (!stbi_write_png((dir + "/icon.png").c_str(), scene.w, scene.h, 4, rgba.data(), scene.w * 4)) return fail("icon.png");
    auto rgb3 = rgbaBytes(sw, true);
    if (!stbi_write_jpg((dir + "/icon_switch.jpg").c_str(), sw.w, sw.h, 3, rgb3.data(), 95)) return fail("icon_switch.jpg");
    if (!writeFile(dir + "/icon_macos.png", pngBytes(mac))) return fail("icon_macos.png");
    if (!writeIcns(dir + "/TurboRacingMayhem.icns", mac)) return fail("TurboRacingMayhem.icns");
    // Android: five densities; the adaptive icon is 108 dp, of which the launcher shows at least the middle 66 dp
    struct D { const char* name; int legacy, adaptive; };
    const D dens[] = {{"mdpi", 48, 108}, {"hdpi", 72, 162}, {"xhdpi", 96, 216}, {"xxhdpi", 144, 324}, {"xxxhdpi", 192, 432}};
    for (const D& d : dens) {
        std::string sub = "android/mipmap-" + std::string(d.name);
        std::error_code ec;
        std::filesystem::create_directories(dir + "/" + sub, ec);
        if (!writeFile(dir + "/" + sub + "/ic_launcher.png", pngBytes(resized(mac, d.legacy, d.legacy))))
            return fail(sub + "/ic_launcher.png");
        Bitmap fg(d.adaptive, d.adaptive);
        int inner = int(std::lround(d.adaptive * 0.76));            // the rounded square then spans ~61%: inside 66 dp
        Bitmap icon = resized(mac, inner, inner);
        int off = (d.adaptive - inner) / 2;
        for (int y = 0; y < inner; ++y)
            for (int x = 0; x < inner; ++x) fg.px[size_t(y + off) * fg.w + (x + off)] = icon.px[size_t(y) * inner + x];
        if (!writeFile(dir + "/" + sub + "/ic_launcher_foreground.png", pngBytes(fg)))
            return fail(sub + "/ic_launcher_foreground.png");
    }
    return true;
}

#include "painter.h"
#include <algorithm>

static const Light LIGHT = [] {
    Light l; double n = std::sqrt(l.x * l.x + l.y * l.y + l.z * l.z);
    l.x /= n; l.y /= n; l.z /= n; return l;
}();

Color lit(Color base, double nx, double ny, double nz, double amb, double spec, double shin) {
    double n = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (n < 1e-9) { nz = 1; n = 1; }
    nx /= n; ny /= n; nz /= n;
    double d = nx * LIGHT.x + ny * LIGHT.y + nz * LIGHT.z;
    double diff = std::max(0.0, d);
    double rz = 2 * d * nz - LIGHT.z;                       // reflection towards the viewer (0,0,1)
    double s = spec > 0 ? spec * std::pow(std::max(0.0, rz), shin) : 0;
    double k = amb + (1.0 - amb) * 1.15 * diff;
    auto ch = [&](int v) { double r = v * k + 255 * s; return int(std::clamp(r, 0.0, 255.0)); };
    return (base & 0xFF000000u) | (Color(ch((base >> 16) & 255)) << 16) | (Color(ch((base >> 8) & 255)) << 8) | Color(ch(base & 255));
}

Paint sphere(double cx, double cy, double rx, double ry, Color base, double amb, double spec, double shin) {
    return Paint([=](double x, double y) {
        double nx = (x - cx) / rx, ny = (y - cy) / ry;
        double d = nx * nx + ny * ny;
        double nz = std::sqrt(std::max(0.02, 1 - d));
        return lit(base, nx, ny, nz, amb, spec, shin);
    });
}
Paint cylX(double x0, double x1, Color base, double amb, double spec, double shin) {
    return Paint([=](double x, double) {
        double nx = std::clamp(2 * (x - x0) / (x1 - x0) - 1, -0.98, 0.98);
        return lit(base, nx, 0, std::sqrt(1 - nx * nx), amb, spec, shin);
    });
}
Paint cylY(double y0, double y1, Color base, double amb, double spec, double shin) {
    return Paint([=](double, double y) {
        double ny = std::clamp(2 * (y - y0) / (y1 - y0) - 1, -0.98, 0.98);
        return lit(base, 0, ny, std::sqrt(1 - ny * ny), amb, spec, shin);
    });
}
Paint vgrad(double y0, double y1, Color c0, Color c1) {
    return Paint([=](double, double y) { return lerpColor(c0, c1, (y - y0) / (y1 - y0)); });
}
Paint hgrad(double x0, double x1, Color c0, Color c1) {
    return Paint([=](double x, double) { return lerpColor(c0, c1, (x - x0) / (x1 - x0)); });
}
double hashNoise(double x, double y) {
    unsigned h = unsigned(int(std::floor(x))) * 73856093u ^ unsigned(int(std::floor(y))) * 19349663u;
    h ^= h >> 13; h *= 0x5bd1e995; h ^= h >> 15;
    return (h & 0xFFFF) / 65535.0;
}

// ------------------------------------------------------------------ Painter --
void Painter::put(int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= hi.w || y >= hi.h) return;
    unsigned a = c >> 24;
    if (!a) return;
    if (material || !mat_.empty()) {
        if (mat_.empty()) mat_.assign(size_t(hi.w) * hi.h, 0);
        unsigned char& m = mat_[size_t(y) * hi.w + x];
        if (material) m = (unsigned char)(m + (255 - m) * a / 255);
        else m = (unsigned char)(m * (255 - a) / 255);
    }
    Color& d = hi.px[size_t(y) * hi.w + x];
    if (a == 255 || !(d >> 24)) { d = c; return; }
    if ((d >> 24) == 255) { d = blendOver(d, c); return; }
    // both translucent: simple "over" with alpha accumulation
    unsigned da = d >> 24;
    unsigned oa = a + da * (255 - a) / 255;
    Color mix = lerpColor(d | 0xFF000000u, c | 0xFF000000u, double(a) / oa);
    d = (mix & 0x00FFFFFFu) | (Color(oa) << 24);
}

void Painter::scan(int hy, double xa, double xb, const Paint& p) {
    int a = int(std::lround(xa)), b = int(std::lround(xb));
    a = std::max(a, 0); b = std::min(b, hi.w);
    double fy = (hy + 0.5 - my) / ss;
    for (int x = a; x < b; ++x) put(x, hy, p((x + 0.5 - mx) / ss, fy));
}

void Painter::rect(double x, double y, double w, double h, const Paint& p) {
    int y0 = int(std::lround(Y(y))), y1 = int(std::lround(Y(y + h)));
    for (int hy = std::max(0, y0); hy < std::min(hi.h, y1); ++hy) scan(hy, X(x), X(x + w), p);
}

void Painter::poly(const std::vector<double>& pts, const Paint& p) {
    size_t n = pts.size() / 2;
    if (n < 3) return;
    double minY = 1e9, maxY = -1e9;
    for (size_t i = 0; i < n; ++i) { minY = std::min(minY, Y(pts[i * 2 + 1])); maxY = std::max(maxY, Y(pts[i * 2 + 1])); }
    std::vector<double> xs;
    for (int hy = std::max(0, int(std::floor(minY))); hy <= std::min(hi.h - 1, int(std::ceil(maxY))); ++hy) {
        double sy = hy + 0.5;
        xs.clear();
        for (size_t i = 0; i < n; ++i) {
            double x0 = X(pts[i * 2]), y0 = Y(pts[i * 2 + 1]);
            double x1 = X(pts[((i + 1) % n) * 2]), y1 = Y(pts[((i + 1) % n) * 2 + 1]);
            if ((y0 <= sy && y1 > sy) || (y1 <= sy && y0 > sy)) xs.push_back(x0 + (sy - y0) / (y1 - y0) * (x1 - x0));
        }
        std::sort(xs.begin(), xs.end());
        for (size_t i = 0; i + 1 < xs.size(); i += 2) scan(hy, xs[i], xs[i + 1], p);
    }
}

void Painter::ellipse(double cx, double cy, double rx, double ry, const Paint& p) {
    if (rx <= 0 || ry <= 0) return;
    double hcx = X(cx), hcy = Y(cy), hrx = rx * ss, hry = ry * ss;
    for (int hy = std::max(0, int(std::floor(hcy - hry))); hy <= std::min(hi.h - 1, int(std::ceil(hcy + hry))); ++hy) {
        double dy = (hy + 0.5 - hcy) / hry;
        if (dy * dy >= 1) continue;
        double dx = hrx * std::sqrt(1 - dy * dy);
        scan(hy, hcx - dx, hcx + dx, p);
    }
}

void Painter::line(double x0, double y0, double x1, double y1, double t, const Paint& p) {
    double dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6) { ellipse(x0, y0, t / 2, t / 2, p); return; }
    double nx = -dy / len * t / 2, ny = dx / len * t / 2;
    poly({x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny}, p);
    ellipse(x0, y0, t / 2, t / 2, p);
    ellipse(x1, y1, t / 2, t / 2, p);
}

void Painter::roundRect(double x, double y, double w, double h, double r, const Paint& p) {
    r = std::min({r, w / 2, h / 2});
    rect(x + r, y, w - 2 * r, h, p);
    rect(x, y + r, r, h - 2 * r, p);
    rect(x + w - r, y + r, r, h - 2 * r, p);
    ellipse(x + r, y + r, r, r, p);
    ellipse(x + w - r, y + r, r, r, p);
    ellipse(x + r, y + h - r, r, r, p);
    ellipse(x + w - r, y + h - r, r, r, p);
}

void Painter::image(const Bitmap& b, double x, double y) {
    int ox = int(std::lround(X(x))), oy = int(std::lround(Y(y)));
    for (int yy = 0; yy < b.h; ++yy)
        for (int xx = 0; xx < b.w; ++xx) put(ox + xx, oy + yy, b.px[size_t(yy) * b.w + xx]);
}

Bitmap Painter::finishMask(std::vector<unsigned char>& mask, Color outline, double outlinePx) {
    std::vector<unsigned char> m = mat_;
    if (m.empty()) m.assign(size_t(hi.w) * hi.h, 0);
    Bitmap res = finish(outline, outlinePx, false);
    // outline pixels were transparent before finish(): their material is 0 already
    const int f = ss / out;
    mask.assign(size_t(res.w) * res.h, 0);
    for (int y = 0; y < res.h; ++y)
        for (int x = 0; x < res.w; ++x) {
            unsigned sum = 0;
            for (int sy = 0; sy < f; ++sy)
                for (int sx = 0; sx < f; ++sx) sum += m[size_t(y * f + sy) * hi.w + (x * f + sx)];
            mask[size_t(y) * res.w + x] = (unsigned char)(sum / (f * f));
        }
    return res;
}

Bitmap Painter::finish(Color outline, double outlinePx, bool mips) {
    if (outlinePx > 0) {
        int r = std::max(1, int(std::lround(outlinePx * ss)));   // outline width is given in units
        int w = hi.w, h = hi.h;
        std::vector<unsigned char> m(size_t(w) * h), t(size_t(w) * h), d(size_t(w) * h);
        for (size_t i = 0; i < m.size(); ++i) m[i] = (hi.px[i] >> 24) >= 160;
        for (int y = 0; y < h; ++y)                       // separable dilation
            for (int x = 0; x < w; ++x) {
                unsigned char v = 0;
                for (int k = std::max(0, x - r); k <= std::min(w - 1, x + r) && !v; ++k) v = m[size_t(y) * w + k];
                t[size_t(y) * w + x] = v;
            }
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                unsigned char v = 0;
                for (int k = std::max(0, y - r); k <= std::min(h - 1, y + r) && !v; ++k) v = t[size_t(k) * w + x];
                d[size_t(y) * w + x] = v;
            }
        for (size_t i = 0; i < m.size(); ++i)
            if (d[i] && !m[i]) {
                Color& c = hi.px[i];
                c = (c >> 24) ? blendOver(outline, c) : outline;     // keep glows on top of the outline
            }
    }
    Bitmap res = hi.downsampled(ss / out);
    if (mips) res.buildMips();
    return res;
}

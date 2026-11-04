// Supersampling painter for procedural pixel art.
// Shapes are drawn with per-pixel "paint" functions (gradients, lit spheres/cylinders) at ss x the
// final resolution, then outlined and box-filtered down -> anti-aliased, shaded 90s-style sprites.
#pragma once
#include "canvas.h"
#include <functional>
#include <vector>
#include <cmath>

struct Paint {
    std::function<Color(double, double)> f;
    Paint(Color c) : f([c](double, double) { return c; }) {}
    template <class F, class = decltype(std::declval<F>()(0.0, 0.0))>
    Paint(F fn) : f(fn) {}
    Color operator()(double x, double y) const { return f(x, y); }
};

// ---- lighting helpers (light from upper left, slightly in front) ----
struct Light { double x = -0.48, y = -0.68, z = 0.55; };
Color lit(Color base, double nx, double ny, double nz, double amb = 0.42, double spec = 0.35, double shin = 18);
Paint sphere(double cx, double cy, double rx, double ry, Color base, double amb = 0.42, double spec = 0.35, double shin = 18);
Paint cylX(double x0, double x1, Color base, double amb = 0.42, double spec = 0.3, double shin = 14);   // vertical tube
Paint cylY(double y0, double y1, Color base, double amb = 0.42, double spec = 0.3, double shin = 14);   // horizontal tube
Paint vgrad(double y0, double y1, Color c0, Color c1);
Paint hgrad(double x0, double x1, Color c0, Color c1);
double hashNoise(double x, double y);   // 0..1, deterministic

class Painter {
public:
    int W, H, ss;
    int out;      // finished bitmaps have 'out' pixels per unit (RENDER_SCALE for game graphics)
    int mx, my;   // hi-res pixel offset of the grid's origin (margins)
    Bitmap hi;
    // shapes are given in units of a W x H grid; they are drawn at ss samples per unit, and the finished
    // bitmap has out pixels per unit (ss is rounded up to a multiple of out)
    // marginX / marginTop: extra transparent room (in units) left, right and above the W x H grid, so shapes or
    // outlines reaching a little past it are not cut off. Drawing coordinates stay those of the W x H grid.
    Painter(int w, int h, int ss_ = 4, int out_ = RENDER_SCALE, int marginX = 0, int marginTop = 0)
        : W(w), H(h), ss((ss_ + out_ - 1) / out_ * out_), out(out_),
          mx(marginX * ss), my(marginTop * ss), hi((w + 2 * marginX) * ss, (h + marginTop) * ss) {}

    // material tracking: while 'material' is true, painted pixels are marked as car paint so the
    // finished sprite can be recoloured later (see finishMask)
    bool material = false;

    void rect(double x, double y, double w, double h, const Paint& p);
    void poly(const std::vector<double>& pts, const Paint& p);
    void ellipse(double cx, double cy, double rx, double ry, const Paint& p);
    void line(double x0, double y0, double x1, double y1, double t, const Paint& p);
    void roundRect(double x, double y, double w, double h, double r, const Paint& p);
    void image(const Bitmap& hiRes, double x, double y);          // bitmap already at hi-res scale
    void imageCentered(const Bitmap& hiRes, double cx, double cy) { image(hiRes, cx - hiRes.w / 2.0 / ss, cy - hiRes.h / 2.0 / ss); }
    double X(double x) const { return x * ss + mx; }   // grid units -> hi-res pixels
    double Y(double y) const { return y * ss + my; }
    // write one hi-res pixel directly (used by the 3D car renderer), with/without the paint flag
    void plot(int x, int y, Color c, bool paint) { bool m = material; material = paint; put(x, y, c); material = m; }
    // outline (in final pixels), downsample, build mipmaps
    Bitmap finish(Color outline = rgb(12, 12, 18), double outlinePx = 1.0, bool mips = true);
    // like finish(), also returns per-pixel paint coverage (0..255) of the final bitmap
    Bitmap finishMask(std::vector<unsigned char>& mask, Color outline = rgb(12, 12, 18), double outlinePx = 1.0);

private:
    std::vector<unsigned char> mat_;
    void put(int x, int y, Color c);
    void scan(int hy, double xa, double xb, const Paint& p);
};

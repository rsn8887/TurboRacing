// Pseudo-3D view renderer (road scanlines, parallax background, scaled sprites, HUD)
#pragma once
#include "canvas.h"
#include "race.h"

struct ViewOptions {
    bool hud = true;
    bool drawViewCar = true;
    int frame = 0;
};

void renderView(Canvas& cv, const Rect& vp, const Race& race, int carIndex, const ViewOptions& opt);
// the same, split into horizontal strips rendered on several threads (identical result)
void renderViewParallel(Canvas& cv, const Rect& vp, const Race& race, int carIndex, const ViewOptions& opt);
std::string formatTime(double t, bool hundredths = true);

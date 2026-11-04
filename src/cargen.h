// Car sprite generator (asset tool only)
#pragma once
#include "cars.h"

// render every car type at every yaw / elevation (+ brake light overlays and shadows), multi-threaded
void generateCarSprites(CarSprites out[CAR_TYPES]);

// distance of the car renderer's camera from the car (cm): sets the perspective of every car frame
constexpr double CAR_CAM_DIST = 2000;

// one large picture of a car type in a given paint colour, at ss / down x the sprite resolution (for the app icons)
Bitmap renderCarImage(int type, Color paint, double yawDeg, double elevDeg, int ss, int down, int outline);

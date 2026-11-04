// Asset files: every sprite, background and car frame lives in PNG texture atlases in the assets folder,
// each with a small text file that says where the pictures are inside the atlas. The asset tool
// (turbo_assetgen) writes them; the game loads them at startup. The PNGs can be edited by hand.
//
//   assets/scenery/trees.png  + trees.txt     palm, pines, bush, cactus
//   assets/scenery/rocks.png  + rocks.txt     rock, snowman
//   assets/scenery/props.png  + props.txt     street lamp, buildings
//   assets/scenery/signs.png  + signs.txt     billboards, turn signs, warning signs, start / checkpoint gantries
//   assets/effects.png        + effects.txt   dust, smoke, sparks, tunnel lamp glow, start lights
//   assets/backgrounds/stageN.png + .txt      the scrolling background layers of each stage
//   assets/cars/<type>.png    + <type>.txt    every yaw / elevation frame, brake light overlays and shadows
//   assets/cars/<type>_paint.png              paint mask: white = car paint (recoloured per car), black = not
#pragma once
#include "sprites.h"
#include "cars.h"
#include <string>

// asset tool: write the generated graphics (gfx + car sprites) into dir
bool saveAssets(const std::string& dir, const Graphics& g, const CarSprites cars[CAR_TYPES], std::string& error);
// game: load everything into gfx and the car sprites; false + error message if something is missing
bool loadAssets(const std::string& dir, Graphics& g, std::string& error);
// where the assets folder is (next to the executable, the working directory, the Switch romfs...)
std::string findAssetsDir();

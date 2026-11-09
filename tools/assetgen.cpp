// turbo_assetgen: paints all of Turbo Racing Mayhem's graphics procedurally (scenery, effects, backgrounds, and the
// 3D-rendered cars) and writes them as PNG texture atlases + layout text files into an assets folder.
//
//   turbo_assetgen [output folder]        (default: assets)
//   turbo_assetgen --icons <folder>       paints the app icons instead (see icongen.cpp)
//
// Careful: this overwrites the PNGs in that folder, including any hand edits. Generate into another folder
// and copy over only what you want to replace.
#include "assets.h"
#include "cargen.h"
#include "icongen.h"
#include "text.h"
#include <SDL.h>
#include <chrono>
#include <cstdio>
#include <memory>

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "--icons") {
        std::printf("painting the app icons into %s/ ...\n", argv[2]);
        std::string err;
        if (!generateIcons(argv[2], err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
        std::printf("done\n");
        return 0;
    }
    std::string out = argc > 1 ? argv[1] : "assets";
    if (SDL_Init(0) != 0) { std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    if (!text.init()) { std::fprintf(stderr, "font setup failed\n"); return 1; }
    auto t0 = std::chrono::steady_clock::now();
    auto ms = [&] { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); };

    std::printf("painting scenery, effects and backgrounds...\n");
    gfx.generate();
    std::printf("rendering cars (%d types x %d yaws x %d elevations)...\n", CAR_TYPES, CAR_YAWS, CAR_ELEV);
    auto cars = std::make_unique<CarSprites[]>(CAR_TYPES);
    generateCarSprites(cars.get());
    std::printf("writing %s/ ...\n", out.c_str());
    std::string err;
    if (!saveAssets(out, gfx, cars.get(), err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    std::printf("done in %.1f s\n", ms() / 1000);
    text.shutdown();
    SDL_Quit();
    return 0;
}

// App icons (asset tool only)
#pragma once
#include <string>

// paints the icon and writes icon.png, icon_switch.jpg, icon_macos.png and TurboRacingMayhem.icns into dir
bool generateIcons(const std::string& dir, std::string& err);

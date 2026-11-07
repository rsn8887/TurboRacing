cmake_minimum_required(VERSION 3.13)
project(TurboRacing VERSION 1.2.0 LANGUAGES CXX)
add_compile_definitions(GAME_VERSION="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(PLATFORM "DESKTOP" CACHE STRING "DESKTOP or SWITCH")
# 2 = render at 1280x720 (default), 1 = render at 640x360 and scale up (for slow machines)
set(TURBO_RENDER_SCALE 2 CACHE STRING "Render resolution multiplier of 640x360 (1 or 2)")

# the game: loads its graphics from PNG atlases in assets/
set(SOURCES
  src/main.cpp src/canvas.cpp src/sprites.cpp src/track.cpp src/race.cpp src/render.cpp
  src/audio.cpp src/input.cpp src/text.cpp src/cars.cpp src/assets.cpp src/painter.cpp)
# the asset tool: paints / renders all graphics procedurally and writes the PNG atlases
set(TOOL_SOURCES
  tools/assetgen.cpp tools/icongen.cpp src/canvas.cpp src/painter.cpp src/sprites.cpp src/spritegen.cpp src/track.cpp
  src/text.cpp src/cars.cpp src/cargen.cpp src/assets.cpp)

# the TrueType fonts are compiled into the executable (no asset files to ship)
include(cmake/EmbedFiles.cmake)
set(FONT_FILES ${CMAKE_SOURCE_DIR}/fonts/RussoOne-Regular.ttf ${CMAKE_SOURCE_DIR}/fonts/ChakraPetch-BoldItalic.ttf)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${FONT_FILES})
embed_files(${CMAKE_BINARY_DIR}/generated/font_data.h ${FONT_FILES})

if(PLATFORM STREQUAL "SWITCH")
  # Configure with the devkitPro toolchain, e.g.
  #   cmake -B build-switch -DPLATFORM=SWITCH -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET SDL2_ttf sdl2)
  add_executable(turboracing ${SOURCES})
  target_include_directories(turboracing PRIVATE ${CMAKE_BINARY_DIR}/generated ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party/stb)
  target_compile_definitions(turboracing PRIVATE TURBO_RENDER_SCALE=${TURBO_RENDER_SCALE})
  target_compile_options(turboracing PRIVATE -O3 -ffunction-sections)
  target_link_libraries(turboracing PRIVATE PkgConfig::SDL2)
  # the assets folder goes into the .nro's romfs (read as romfs:/assets)
  file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/romfs)
  file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR}/romfs)
  nx_generate_nacp(turboracing.nacp NAME "Turbo Racing" AUTHOR "rsn8887" VERSION ${PROJECT_VERSION})
  nx_create_nro(turboracing NACP turboracing.nacp ICON ${CMAKE_SOURCE_DIR}/icon/icon_switch.jpg ROMFS ${CMAKE_BINARY_DIR}/romfs)
else()
  find_package(SDL2 REQUIRED)
  find_package(Threads REQUIRED)
  if(APPLE)
    # macOS: the game is an app bundle (TurboRacing.app) with its icon and the assets in Contents/Resources
    set(ICNS ${CMAKE_SOURCE_DIR}/icon/TurboRacing.icns)
    set_source_files_properties(${ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_executable(turboracing MACOSX_BUNDLE ${SOURCES} ${ICNS})
    set_target_properties(turboracing PROPERTIES
      OUTPUT_NAME TurboRacing
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/Info.plist.in
      MACOSX_BUNDLE_BUNDLE_NAME "Turbo Racing"
      MACOSX_BUNDLE_GUI_IDENTIFIER com.rsn8887.turboracing
      MACOSX_BUNDLE_ICON_FILE TurboRacing.icns
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_BUNDLE_COPYRIGHT "rsn8887")
    # room in the header so make_dmg.sh can point the library references into the app
    target_link_options(turboracing PRIVATE -Wl,-headerpad_max_install_names)
    set(ASSETS_DEST $<TARGET_BUNDLE_CONTENT_DIR:turboracing>/Resources/assets)
  else()
    add_executable(turboracing ${SOURCES})
    set(ASSETS_DEST $<TARGET_FILE_DIR:turboracing>/assets)
  endif()
  add_executable(turbo_assetgen ${TOOL_SOURCES})
  set(GAME_TARGETS turboracing turbo_assetgen)
  find_package(SDL2_ttf CONFIG QUIET)
  if(NOT TARGET SDL2_ttf::SDL2_ttf AND NOT TARGET SDL2_ttf::SDL2_ttf-static)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
  endif()
  foreach(T ${GAME_TARGETS})
    target_link_libraries(${T} PRIVATE Threads::Threads)
    target_include_directories(${T} PRIVATE ${CMAKE_BINARY_DIR}/generated ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tools ${CMAKE_SOURCE_DIR}/third_party/stb)
    target_compile_definitions(${T} PRIVATE TURBO_RENDER_SCALE=${TURBO_RENDER_SCALE})
    if(TARGET SDL2::SDL2)
      target_link_libraries(${T} PRIVATE $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main> SDL2::SDL2)
    else()
      target_include_directories(${T} PRIVATE ${SDL2_INCLUDE_DIRS})
      target_link_libraries(${T} PRIVATE ${SDL2_LIBRARIES})
    endif()
    if(TARGET SDL2_ttf::SDL2_ttf)
      target_link_libraries(${T} PRIVATE SDL2_ttf::SDL2_ttf)
    elseif(TARGET SDL2_ttf::SDL2_ttf-static)
      target_link_libraries(${T} PRIVATE SDL2_ttf::SDL2_ttf-static)
    else()
      target_link_libraries(${T} PRIVATE PkgConfig::SDL2TTF)
    endif()
    if(MSVC)
      target_compile_options(${T} PRIVATE /W3 /D_USE_MATH_DEFINES)
    else()
      target_compile_options(${T} PRIVATE -Wall -Wextra -O3)
    endif()
  endforeach()
  # the game looks for assets/ next to its executable (macOS: in the app's Resources): copy them there after every build
  # (on every build, so regenerated or edited PNGs are picked up even when the game itself is unchanged)
  add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${ASSETS_DEST}
    COMMENT "Copying assets/ next to the game")
  # cmake --build build --target assets   regenerates assets/ in the source tree (overwrites hand edits!)
  add_custom_target(assets COMMAND turbo_assetgen ${CMAKE_SOURCE_DIR}/assets DEPENDS turbo_assetgen
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} COMMENT "Generating assets/ with turbo_assetgen")
  # cmake --build build --target icons   repaints the app icons in icon/
  add_custom_target(icons COMMAND turbo_assetgen --icons ${CMAKE_SOURCE_DIR}/icon DEPENDS turbo_assetgen
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} COMMENT "Painting the app icons into icon/")
  if(WIN32)
    set_target_properties(turboracing PROPERTIES WIN32_EXECUTABLE ON)
  endif()
endif()

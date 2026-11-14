// Keyboard + game controller input for up to 2 players and the menus
#pragma once
#include <SDL.h>
#include <vector>

struct PlayerInput {
    double steer = 0;       // -1 left .. +1 right
    double throttle = 0;    // 0..1
    double brake = 0;       // 0..1
    bool shiftUp = false, shiftDown = false;   // edges (manual gears)
    bool nitro = false;                        // held: boost with nitrous
};

struct MenuInput {
    bool up = false, down = false, left = false, right = false, confirm = false, back = false, pause = false;
    bool background = false;   // the app is going to the background (Android: home button, screen off)
};

class Input {
public:
    void init();
    void beginFrame();                       // clears edge flags
    void handleEvent(const SDL_Event& e, int numPlayers);
    PlayerInput player(int index, int numPlayers);
    MenuInput menu;                          // edges this frame
    bool quitRequested = false;
    bool toggleFullscreen = false;
    int controllerCount() const { return int(pads_.size()); }
    bool overrideOn = false; PlayerInput overrideIn;   // used by the automated test mode

private:
    std::vector<SDL_GameController*> pads_;
    bool shiftUp_[2] = {}, shiftDown_[2] = {};
    int padPlayer(SDL_JoystickID id, int numPlayers) const;
    void openAll();
};

extern Input input;

#include "input.h"
#include <algorithm>
#include <cmath>

Input input;

void Input::init() { openAll(); }

void Input::openAll() {
    for (auto* p : pads_) SDL_GameControllerClose(p);
    pads_.clear();
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
        if (SDL_IsGameController(i))
            if (SDL_GameController* c = SDL_GameControllerOpen(i)) pads_.push_back(c);
}

void Input::beginFrame() {
    menu = MenuInput{};
    toggleFullscreen = false;
}

int Input::padPlayer(SDL_JoystickID id, int numPlayers) const {
    for (size_t i = 0; i < pads_.size(); ++i)
        if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads_[i])) == id)
            return numPlayers > 1 ? int(std::min<size_t>(i, 1)) : 0;
    return 0;
}

void Input::handleEvent(const SDL_Event& e, int numPlayers) {
    switch (e.type) {
    case SDL_QUIT: quitRequested = true; break;
    case SDL_APP_WILLENTERBACKGROUND: menu.background = true; break;
    case SDL_CONTROLLERDEVICEADDED:
    case SDL_CONTROLLERDEVICEREMOVED: openAll(); break;
    case SDL_KEYDOWN: {
        if (e.key.repeat) {
            // allow auto-repeat for menu navigation only
            SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_UP) menu.up = true;
            if (k == SDLK_DOWN) menu.down = true;
            break;
        }
        SDL_Keycode k = e.key.keysym.sym;
        bool alt = (e.key.keysym.mod & KMOD_ALT) != 0;
        if (k == SDLK_F11 || (k == SDLK_RETURN && alt) || (k == SDLK_f && (e.key.keysym.mod & KMOD_GUI))) { toggleFullscreen = true; break; }
        if (k == SDLK_UP || k == SDLK_w) menu.up = true;
        if (k == SDLK_DOWN || k == SDLK_s) menu.down = true;
        if (k == SDLK_LEFT || k == SDLK_a) menu.left = true;
        if (k == SDLK_RIGHT || k == SDLK_d) menu.right = true;
        if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_KP_ENTER) menu.confirm = true;
        if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_AC_BACK) menu.back = true;   // AC_BACK: Android back
        if (k == SDLK_ESCAPE || k == SDLK_p || k == SDLK_AC_BACK) menu.pause = true;
        // gears
        if (k == SDLK_RSHIFT || k == SDLK_m) shiftUp_[0] = true;
        if (k == SDLK_RCTRL || k == SDLK_n) shiftDown_[0] = true;
        int p2 = numPlayers > 1 ? 1 : 0;
        if (k == SDLK_e) shiftUp_[p2] = true;
        if (k == SDLK_q) shiftDown_[p2] = true;
        if (numPlayers == 1) { if (k == SDLK_x) shiftUp_[0] = true; if (k == SDLK_z) shiftDown_[0] = true; }
        break;
    }
    case SDL_CONTROLLERBUTTONDOWN: {
        int pl = padPlayer(e.cbutton.which, numPlayers);
        switch (e.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: menu.up = true; break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: menu.down = true; break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: menu.left = true; break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: menu.right = true; break;
        case SDL_CONTROLLER_BUTTON_A: menu.confirm = true; break;
        case SDL_CONTROLLER_BUTTON_B: menu.back = true; break;
        case SDL_CONTROLLER_BUTTON_START: menu.pause = true; menu.confirm = true; break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        case SDL_CONTROLLER_BUTTON_Y: shiftUp_[pl] = true; break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        case SDL_CONTROLLER_BUTTON_X: shiftDown_[pl] = true; break;
        default: break;
        }
        break;
    }
    case SDL_CONTROLLERAXISMOTION: {
        // stick flicks for menus
        static int lastDir[8] = {};
        int idx = std::min<int>(e.caxis.which, 7);
        if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int dir = e.caxis.value < -20000 ? -1 : e.caxis.value > 20000 ? 1 : 0;
            if (dir != lastDir[idx]) { if (dir < 0) menu.up = true; if (dir > 0) menu.down = true; lastDir[idx] = dir; }
        }
        break;
    }
    default: break;
    }
}

PlayerInput Input::player(int index, int numPlayers) {
    if (overrideOn) return overrideIn;
    PlayerInput in;
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    double l = 0, r = 0, acc = 0, brk = 0;
    bool arrows = index == 0;
    bool wasd = (index == 1) || numPlayers == 1;
    if (arrows) {
        l += k[SDL_SCANCODE_LEFT]; r += k[SDL_SCANCODE_RIGHT];
        acc += k[SDL_SCANCODE_UP]; brk += k[SDL_SCANCODE_DOWN];
    }
    if (wasd) {
        l += k[SDL_SCANCODE_A]; r += k[SDL_SCANCODE_D];
        acc += k[SDL_SCANCODE_W]; brk += k[SDL_SCANCODE_S];
    }
    for (size_t i = 0; i < pads_.size(); ++i) {
        int pl = numPlayers > 1 ? int(std::min<size_t>(i, 1)) : 0;
        if (pl != index) continue;
        SDL_GameController* c = pads_[i];
        double ax = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0;
        if (std::fabs(ax) > 0.2) { if (ax < 0) l += (-ax - 0.2) / 0.8; else r += (ax - 0.2) / 0.8; }
        l += SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        r += SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        acc += SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A);
        brk += SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B);
        acc += SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0;
        brk += SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0;
    }
    in.steer = std::clamp(r - l, -1.0, 1.0);
    in.throttle = std::clamp(acc, 0.0, 1.0);
    in.brake = std::clamp(brk, 0.0, 1.0);
    if (index < 2) {
        in.shiftUp = shiftUp_[index]; in.shiftDown = shiftDown_[index];
        shiftUp_[index] = shiftDown_[index] = false;
    }
    return in;
}

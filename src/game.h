// "Live" mode: a first-person day in the life of one citizen of Dubrava.
// The sim (src/sim) owns the rules; this layer finds what the player is looking at,
// runs actions over game time, and draws the HUD, prompts and menus.
#pragma once
#include "panelka.h"

#include <functional>
#include <string>

namespace game {

// What the game needs from the front end. main.cpp fills these in once.
struct Host {
    std::function<const Interior&()> interior;                         // the loaded interior (rec -1 if none)
    std::function<void(int rec, int flat, int floor)> loadHome;        // build that interior with the home flat set
    std::function<void(double x, double z, double feet, double yaw, double pitch)> place; // put the walker there (world)
    std::function<bool(double x, double z, double feet)> blocked;      // would the walker collide there?
    std::function<void(int room, bool on)> roomLight;                  // switch an interior room's lamps
    std::function<void(double hour)> setHour;                          // drive the world's time of day
};
extern Host host;

bool active();
// Start a new life in the current (district) scene. Returns false with a reason.
bool start(double seed, std::string& why);
void stop();
// Called by main after it (re)builds an interior, so switch states can be re-applied.
void interiorLoaded();
// One frame. cam / look are the camera position and look-at point (world).
void update(double dt, const V3& cam, const V3& look, double feet);
void draw(int screenW, int screenH);
// While true the walker must not move or turn (an action or a menu is running).
bool freezeWalker();
// While true the mouse belongs to a menu (no drag-to-look).
bool wantsMouse();

// Debug / screenshot hooks (PANELKA_LIVE_*): stand in front of the first home object
// of this kind, and run actions instantly ("Stove:1,Table:2", 1-based).
bool faceObject(sim::Obj kind);
void runScript(const std::string& script);

} // namespace game

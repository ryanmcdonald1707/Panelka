// Everyday-life simulation for one citizen of the Érinska Narodna Poblacht.
// Pure C++: no rendering, no raylib. The game layer asks it what can be done with an
// object, runs the chosen action over game time, and draws whatever state it exposes.
#pragma once
#include "objects.h"

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace sim {

/* ================= TIME ================= */
// Game time is counted in minutes from midnight at the start of day 0 (a Luanek).
constexpr double HOUR = 60, DAY = 24 * HOUR;
int dayOf(double t);
int weekdayOf(double t);        // 0 Luanek .. 6 Domhnica
double hourOf(double t);        // 0..24
const char* weekdayName(int wd);
std::string clockText(double t); // "06:30"
std::string stampText(double t); // "Luanek 06:30, day 1"
std::string money(int pingin);   // "3.80 p."

/* ================= NEEDS ================= */
// 100 is fully satisfied, 0 is desperate.
enum Need { Hunger, Thirst, Energy, Bladder, Hygiene, Mood, NEED_N };
const char* needName(Need n);
// Short status word when a need is low, or "" when it is fine.
const char* needWarning(Need n, double v);

enum class Activity { Awake, Sleep, Work };

/* ================= ITEMS ================= */
enum class Item : int {
    Bread, Milk, Kefir, Buckwheat, Tea, Sugar, Butter, Sausage, Kasha, Lunch, Newspaper,
    CouponSugar, CouponButter, CouponMeat,
    ITEM_N
};
struct ItemDef {
    const char* name;    // "Rye bread 'Dubravski'"
    const char* unit;    // "slice"
    const char* units;   // "slices"
};
const ItemDef& itemDef(Item i);
std::string itemCount(Item i, int n); // "6 slices of rye bread 'Dubravski'" style label

using Inv = std::array<int, (size_t)Item::ITEM_N>;
inline int& at(Inv& v, Item i) { return v[(size_t)i]; }
inline int at(const Inv& v, Item i) { return v[(size_t)i]; }

// What the kiosk sells: one purchase adds `units` of `item`.
struct Good {
    Item item;
    int units;
    int price;           // pingin
    Item coupon;         // Item::ITEM_N when no coupon is needed
    const char* label;   // "Loaf of rye bread"
};
const std::vector<Good>& kioskGoods();

/* ================= PEOPLE ================= */
struct Profile {
    std::string first, last;
    int flat = 0, block = 0;
    std::string street;
    std::string fullName() const { return first + " " + last; }
};

/* ================= ACTIONS ================= */
// Something the player can do with an object. `minutes` of game time pass while it
// runs (0 = instant); `done` applies the result at the end.
struct Action {
    std::string label;
    double minutes = 0;
    Activity activity = Activity::Awake;
    bool enabled = true;
    std::string why;                        // why it is disabled
    std::function<void()> done;
    std::function<void()> onMinute;         // called every game minute while it runs
    std::function<bool()> interrupt;        // checked every minute; true stops early (done still runs)
    bool fade = false;                      // long skip: the game blacks out the screen
    int ui = 0;                             // UI to open when done (see Sim::UI_*)
};

// Where an action happens: which object, and which containers are within reach.
struct Ctx {
    Obj kind = Obj::None;
    std::string key;                        // stable id of the object ("home/fridge", "light/3")
    std::vector<std::string> reach;         // container keys usable from here (same room)
    bool home = false;                      // inside the player's own flat
};

struct Note { double t; std::string text; };

/* ================= THE SIMULATION ================= */
struct Sim {
    static constexpr int UI_NONE = 0, UI_KIOSK = 1, UI_CONTAINER = 2, UI_NEWSPAPER = 3;

    double t = 0;
    std::array<double, NEED_N> need{};
    int cash = 0;                           // pingin
    Inv carried{};
    std::map<std::string, Inv> containers;  // fridge, cupboard, ... by key
    std::map<std::string, bool> switches;   // lights and appliances by key
    Profile me;

    // work
    int wagesOwed = 0;                      // paid out at the end of the Aoinek shift
    int lastWorkDay = -1;
    int lastCouponMonth = -1;

    bool radioOn = false;
    bool radioHeard = false;                // set by the game: the player is within earshot
    bool collapsed = false;                 // energy ran out; the game puts the player to sleep
    std::deque<Note> notes;
    std::mt19937 rng;

    explicit Sim(uint32_t seed = 1);
    // A fresh life: day 0, 06:30, in bed, with a stocked kitchen.
    void newLife(const Profile& p, const std::string& fridgeKey, const std::string& cupboardKey);

    // Advance the world by `minutes` doing `a`. Returns early (with the minutes actually
    // used) when `stop` says so.
    double advance(double minutes, Activity a, const std::function<bool()>& stop = nullptr);

    std::vector<Action> actions(const Ctx& c);
    // Run up to `minutes` of an action (its per-minute hooks included); returns the minutes
    // used, less than asked when the action's interrupt fired. `perform` runs it to the end
    // and applies the result in one go.
    double step(const Action& a, double minutes);
    double perform(const Action& a);
    std::string buy(const Good& g);          // "" on success, else the reason
    std::string radioLine() const;           // what Programme One is broadcasting now
    std::string headline() const;            // today's Fírinne front page
    bool hotWater() const;                   // the district boiler's schedule

    // item pools: carried first, then the containers in reach
    int have(const Ctx& c, Item i) const;
    bool take(const Ctx& c, Item i, int n);

    void note(const std::string& s);
    static Profile randomProfile(std::mt19937& g);
    double rnd(double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); }

    // tuning, per game hour
    static const std::array<double, NEED_N> DECAY_AWAKE, DECAY_SLEEP, DECAY_WORK;
    static constexpr int WAGE = 380;         // pingin per full shift
    static constexpr double SHIFT_START = 8 * HOUR, SHIFT_END = 17 * HOUR, BUS_RIDE = 25;

private:
    void tick(Activity a, double minutes);
    void change(Need n, double d);
    Action eat(const Ctx& c, const std::string& label, double minutes, std::vector<std::pair<Item, int>> use,
               std::array<double, NEED_N> gain);
    Action workShift();
};

} // namespace sim

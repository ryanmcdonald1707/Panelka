// Headless tests for the life sim. Run with `ctest` or ./build/sim_test.
#include "sim/sim.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace sim;

static int failures = 0, checks = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        checks++;                                                                     \
        if (!(cond)) { failures++; std::printf("%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); } \
    } while (0)
#define NEAR(a, b) CHECK(std::fabs((a) - (b)) < 1e-6)

static const char* FRIDGE = "home/fridge";
static const char* CUPBOARD = "home/cupboard";

static Sim fresh() {
    Sim s(7);
    Profile p;
    p.first = "Nuala";
    p.last = "Ó Kovač";
    s.newLife(p, FRIDGE, CUPBOARD);
    return s;
}
static Ctx at(Obj k, bool kitchen = true) {
    Ctx c;
    c.kind = k;
    c.home = true;
    if (kitchen) c.reach = {FRIDGE, CUPBOARD};
    return c;
}
static const Action* find(const std::vector<Action>& v, const std::string& label) {
    for (auto& a : v) if (a.label == label) return &a;
    return nullptr;
}

static void testStart() {
    Sim s = fresh();
    CHECK(weekdayOf(s.t) == 0);
    CHECK(clockText(s.t) == "06:30");
    CHECK(stampText(s.t) == "Luanek 06:30, day 1");
    CHECK(s.cash == 1250);
    CHECK(sim::at(s.carried, Item::CouponMeat) == 1);
    CHECK(s.have(at(Obj::Table), Item::Bread) == 5);
    CHECK(s.have(at(Obj::Table, false), Item::Bread) == 0);
    CHECK(money(380) == "3.80 p.");
}

static void testDecay() {
    Sim s = fresh();
    s.need = {80, 80, 80, 80, 80, 60};
    s.advance(HOUR, Activity::Awake);
    NEAR(s.need[Hunger], 80 + Sim::DECAY_AWAKE[Hunger]);
    NEAR(s.need[Thirst], 80 + Sim::DECAY_AWAKE[Thirst]);
    s.need = {80, 80, 50, 80, 80, 60};
    s.advance(2 * HOUR, Activity::Sleep);
    NEAR(s.need[Energy], 50 + 2 * Sim::DECAY_SLEEP[Energy]);
}

static void testAccident() {
    Sim s = fresh();
    s.need[Bladder] = 3;
    s.need[Hygiene] = 90;
    s.advance(HOUR, Activity::Awake);
    CHECK(s.need[Bladder] > 90);
    CHECK(s.need[Hygiene] < 45);
}

static void testCollapse() {
    Sim s = fresh();
    s.need[Energy] = 1;
    s.advance(30, Activity::Awake);
    CHECK(s.collapsed);
    auto v = s.actions(at(Obj::Bed));
    const Action* a = find(v, "Sleep until rested");
    CHECK(a && a->enabled);
    if (a) s.perform(*a);
    CHECK(!s.collapsed);
}

static void testEating() {
    Sim s = fresh();
    s.need[Hunger] = 40;
    auto v = s.actions(at(Obj::Table));
    const Action* a = find(v, "Eat bread with sausage");
    CHECK(a && a->enabled);
    if (a) s.perform(*a);
    CHECK(s.have(at(Obj::Table), Item::Bread) == 4);
    CHECK(s.have(at(Obj::Table), Item::Sausage) == 5);
    CHECK(s.need[Hunger] > 40 + 16 - 1);
    // cook, then eat the kasha at the table
    auto st = s.actions(at(Obj::Stove));
    const Action* k = find(st, "Cook buckwheat kasha (with butter)");
    CHECK(k && k->enabled);
    if (k) s.perform(*k);
    CHECK(sim::at(s.carried, Item::Kasha) == 1);
    CHECK(s.have(at(Obj::Table), Item::Butter) == 3);
    v = s.actions(at(Obj::Table));
    CHECK(find(v, "Eat a bowl of kasha") != nullptr);
    // nothing in reach away from the kitchen: a single disabled entry
    Sim e = fresh();
    auto w = e.actions(at(Obj::Table, false));
    CHECK(w.size() == 1 && !w[0].enabled);
}

static void testLunch() {
    Sim s = fresh();
    auto v = s.actions(at(Obj::Table));
    const Action* a = find(v, "Pack a lunch for work");
    CHECK(a && a->enabled);
    if (a) s.perform(*a);
    CHECK(sim::at(s.carried, Item::Lunch) == 1);
    CHECK(s.have(at(Obj::Table), Item::Sausage) == 4);
}

static void testWork() {
    Sim s = fresh();
    s.t = 7 * HOUR;
    auto v = s.actions(at(Obj::BusStop, false));
    CHECK(v.size() == 1 && v[0].enabled);
    int cash = s.cash;
    s.perform(v[0]);
    CHECK(clockText(s.t) == "17:25");
    CHECK(s.lastWorkDay == 0);
    CHECK(s.wagesOwed == Sim::WAGE);
    CHECK(s.cash == cash - 35); // canteen lunch: no parcel packed
    // can't go twice in a day
    v = s.actions(at(Obj::BusStop, false));
    CHECK(!v[0].enabled);

    // late on Máirtek: docked
    Sim l = fresh();
    l.t = DAY + 8.5 * HOUR;
    auto w = l.actions(at(Obj::BusStop, false));
    CHECK(w[0].enabled);
    l.perform(w[0]);
    CHECK(l.wagesOwed < Sim::WAGE && l.wagesOwed > Sim::WAGE * 3 / 4);

    // weekend: no shift
    Sim we = fresh();
    we.t = 5 * DAY + 7 * HOUR;
    CHECK(!we.actions(at(Obj::BusStop, false))[0].enabled);
}

static void testPayday() {
    Sim s = fresh();
    for (int d = 0; d < 5; d++) {
        s.t = d * DAY + 7 * HOUR;
        s.need = {80, 80, 80, 80, 80, 60};
        auto v = s.actions(at(Obj::BusStop, false));
        CHECK(v[0].enabled);
        s.perform(v[0]);
    }
    CHECK(s.wagesOwed == 0);
    CHECK(s.cash == 1250 - 5 * 35 + 5 * Sim::WAGE);
}

static void testSleep() {
    Sim s = fresh();
    s.t = 22 * HOUR;
    s.need = {80, 80, 30, 100, 80, 60};
    auto v = s.actions(at(Obj::Bed, false));
    const Action* a = find(v, "Sleep until 06:30");
    CHECK(a && a->enabled);
    NEAR(a->minutes, 8.5 * HOUR);
    if (a) s.perform(*a);
    CHECK(clockText(s.t) == "06:30");
    CHECK(s.need[Energy] > 95);
    // a full bladder wakes you early
    Sim b = fresh();
    b.t = 22 * HOUR;
    b.need = {80, 80, 30, 20, 80, 60};
    auto w = b.actions(at(Obj::Bed, false));
    b.perform(*find(w, "Sleep until 06:30"));
    CHECK(clockText(b.t) != "06:30");
    CHECK(b.need[Bladder] < 6);
}

static void testKiosk() {
    Sim s = fresh();
    s.t = 6 * HOUR;
    CHECK(!s.actions(at(Obj::Kiosk, false))[0].enabled);
    s.t = 9 * HOUR;
    CHECK(s.actions(at(Obj::Kiosk, false))[0].enabled);
    const auto& G = kioskGoods();
    const Good* sausage = nullptr;
    for (auto& g : G) if (g.item == Item::Sausage) sausage = &g;
    CHECK(sausage != nullptr);
    CHECK(s.buy(*sausage) == "");
    CHECK(sim::at(s.carried, Item::Sausage) == 20);
    CHECK(s.buy(*sausage) != "");           // the only meat coupon is spent
    s.cash = 10;
    CHECK(s.buy(G[0]) == "Not enough money.");
}

static void testBath() {
    Sim s = fresh();
    s.t = 7 * HOUR;
    CHECK(find(s.actions(at(Obj::Bath, false)), "Take a hot bath") != nullptr);
    s.t = 13 * HOUR;
    auto v = s.actions(at(Obj::Bath, false));
    CHECK(v.size() == 1 && v[0].label.find("cold bath") != std::string::npos);
}

static void testSwitch() {
    Sim s = fresh();
    Ctx c = at(Obj::LightSwitch, false);
    c.key = "light/1";
    s.perform(s.actions(c)[0]);
    CHECK(s.switches["light/1"]);
    CHECK(s.actions(c)[0].label == "Switch the light off");
}

int main() {
    testStart();
    testDecay();
    testAccident();
    testCollapse();
    testEating();
    testLunch();
    testWork();
    testPayday();
    testSleep();
    testKiosk();
    testBath();
    testSwitch();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

#include "sim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace sim {

/* ================= NAMES ================= */
const char* objName(Obj k) {
    switch (k) {
    case Obj::Bed: return "Bed";
    case Obj::Wardrobe: return "Wardrobe";
    case Obj::Sofa: return "Sofa";
    case Obj::Table: return "Table";
    case Obj::Stove: return "Stove";
    case Obj::Fridge: return "Fridge";
    case Obj::Cupboard: return "Cupboard";
    case Obj::KitchenSink: return "Kitchen sink";
    case Obj::Radio: return "Radio";
    case Obj::TV: return "Television";
    case Obj::Toilet: return "Toilet";
    case Obj::BathSink: return "Washbasin";
    case Obj::Bath: return "Bath";
    case Obj::LightSwitch: return "Light switch";
    case Obj::BusStop: return "Bus stop";
    case Obj::Kiosk: return "Kiosk";
    default: return "";
    }
}

/* ================= TIME ================= */
static const char* WEEKDAYS[7] = {"Luanek", "Máirtek", "Céadnica", "Déardan", "Aoinek", "Sathurna", "Domhnica"};
int dayOf(double t) { return (int)std::floor(t / DAY); }
int weekdayOf(double t) { return ((dayOf(t) % 7) + 7) % 7; }
double hourOf(double t) { return std::fmod(t, DAY) / HOUR; }
const char* weekdayName(int wd) { return WEEKDAYS[((wd % 7) + 7) % 7]; }
std::string clockText(double t) {
    int m = (int)std::floor(std::fmod(t, DAY));
    char b[16];
    snprintf(b, sizeof b, "%02d:%02d", m / 60, m % 60);
    return b;
}
std::string stampText(double t) { return std::string(weekdayName(weekdayOf(t))) + " " + clockText(t) + ", day " + std::to_string(dayOf(t) + 1); }
std::string money(int p) {
    char b[24];
    snprintf(b, sizeof b, "%d.%02d p.", p / 100, std::abs(p % 100));
    return b;
}

/* ================= NEEDS ================= */
const char* needName(Need n) {
    static const char* N[NEED_N] = {"Hunger", "Thirst", "Energy", "Bladder", "Hygiene", "Mood"};
    return N[n];
}
const char* needWarning(Need n, double v) {
    static const char* W[NEED_N][2] = {{"Peckish", "Starving"}, {"Thirsty", "Parched"}, {"Tired", "Exhausted"},
                                       {"Needs the toilet", "Bursting"}, {"Grubby", "Filthy"}, {"Low", "Miserable"}};
    return v < 12 ? W[n][1] : v < 30 ? W[n][0] : "";
}
//                                                    Hunger Thirst Energy Bladder Hygiene Mood
const std::array<double, NEED_N> Sim::DECAY_AWAKE{-4.5, -6.0, -5.5, -7.0, -2.0, 0};
const std::array<double, NEED_N> Sim::DECAY_SLEEP{-1.5, -2.0, 12.5, -2.5, -1.0, 0};
const std::array<double, NEED_N> Sim::DECAY_WORK{-6.0, -3.0, -7.0, 0.0, -3.5, 0};

/* ================= ITEMS ================= */
const ItemDef& itemDef(Item i) {
    static const ItemDef D[(size_t)Item::ITEM_N] = {
        {"Rye bread", "slice", "slices"},
        {"Milk", "glass", "glasses"},
        {"Kefir 'Bó Bhán'", "glass", "glasses"},
        {"Buckwheat", "portion", "portions"},
        {"Tea 'Cnoc Gorm'", "brew", "brews"},
        {"Sugar", "spoon", "spoons"},
        {"Butter", "pat", "pats"},
        {"Doctor's sausage", "slice", "slices"},
        {"Buckwheat kasha", "bowl", "bowls"},
        {"Lunch parcel", "parcel", "parcels"},
        {"Fírinne", "copy", "copies"},
        {"Sugar coupon (1 kg)", "coupon", "coupons"},
        {"Butter coupon (200 g)", "coupon", "coupons"},
        {"Meat coupon (500 g)", "coupon", "coupons"},
    };
    return D[(size_t)i];
}
std::string itemCount(Item i, int n) {
    const ItemDef& d = itemDef(i);
    return std::string(d.name) + ": " + std::to_string(n) + " " + (n == 1 ? d.unit : d.units);
}
const std::vector<Good>& kioskGoods() {
    static const Item NO = Item::ITEM_N;
    static const std::vector<Good> G{
        {Item::Bread, 8, 18, NO, "Loaf of rye bread"},
        {Item::Milk, 4, 28, NO, "Milk, 1 L bottle"},
        {Item::Kefir, 2, 22, NO, "Kefir, 0.5 L"},
        {Item::Buckwheat, 10, 50, NO, "Buckwheat, 1 kg"},
        {Item::Tea, 25, 38, NO, "Tea 'Cnoc Gorm', 50 g"},
        {Item::Sugar, 100, 94, Item::CouponSugar, "Sugar, 1 kg"},
        {Item::Butter, 10, 72, Item::CouponButter, "Butter, 200 g"},
        {Item::Sausage, 20, 110, Item::CouponMeat, "Doctor's sausage, 500 g"},
        {Item::Newspaper, 1, 3, NO, "Fírinne, today's edition"},
    };
    return G;
}

/* ================= PEOPLE ================= */
Profile Sim::randomProfile(std::mt19937& g) {
    static const std::vector<std::string> F{"Bríd", "Nuala", "Dervla", "Aoifa", "Sinéad", "Olena", "Máire", "Darina", "Róisa", "Svetla",
                                            "Séamas", "Tadhg", "Pádraig", "Ciarán", "Bohdan", "Oisín", "Dmitrí", "Colm", "Ruairí", "Stanislav"};
    static const std::vector<std::string> L{"Ó Kovač", "Mac Radek", "Dunajová", "Ó Brodaigh", "Kavanagh", "Novák", "Mac Iván",
                                            "Horvath", "Ní Dhubháin", "Ó Ceallaigh", "Petrovič", "Mac Gréagóir"};
    static const std::vector<std::string> S{"Ulica Naomh Bríd", "Prospekt Saothair", "Ulica na Darach", "Ulica Oisína", "Prospekt Síochána"};
    auto pick = [&](const std::vector<std::string>& v) { return v[std::uniform_int_distribution<size_t>(0, v.size() - 1)(g)]; };
    Profile p;
    p.first = pick(F);
    p.last = pick(L);
    p.street = pick(S);
    return p;
}

/* ================= CORE ================= */
Sim::Sim(uint32_t seed) : rng(seed) { need.fill(70); }

void Sim::note(const std::string& s) {
    notes.push_back({t, s});
    while (notes.size() > 40) notes.pop_front();
}

void Sim::newLife(const Profile& p, const std::string& fridgeKey, const std::string& cupboardKey) {
    me = p;
    t = 6.5 * HOUR;
    need = {60, 55, 85, 40, 70, 60};
    cash = 1250;
    carried.fill(0);
    containers.clear();
    switches.clear();
    Inv& fr = containers[fridgeKey];
    fr.fill(0);
    at(fr, Item::Milk) = 3; at(fr, Item::Butter) = 4; at(fr, Item::Sausage) = 6; at(fr, Item::Kefir) = 2;
    Inv& cb = containers[cupboardKey];
    cb.fill(0);
    at(cb, Item::Bread) = 5; at(cb, Item::Buckwheat) = 4; at(cb, Item::Tea) = 10; at(cb, Item::Sugar) = 20;
    wagesOwed = 0;
    lastWorkDay = -1;
    lastCouponMonth = -1;
    radioOn = radioHeard = collapsed = false;
    notes.clear();
    tick(Activity::Sleep, 0); // hands out this month's coupons
    note("Luanek morning in Dubrava. Your shift at the Tractor Works starts at 08:00; bus 14 takes 25 minutes.");
}

void Sim::change(Need n, double d) { need[n] = std::min(100.0, std::max(0.0, need[n] + d)); }

void Sim::tick(Activity a, double m) {
    const auto& R = a == Activity::Sleep ? DECAY_SLEEP : a == Activity::Work ? DECAY_WORK : DECAY_AWAKE;
    double h = m / HOUR;
    for (int n = 0; n < Mood; n++) change((Need)n, R[n] * h);
    // running on empty wears everything else down
    if (need[Hunger] <= 0 || need[Thirst] <= 0) change(Energy, -4 * h);
    // mood drifts toward what the body feels like (and whether the radio is on)
    double target = 65;
    for (int n = 0; n < Mood; n++) target -= std::max(0.0, 35 - need[n]) * .7;
    if (radioOn && radioHeard && a == Activity::Awake) target += 6;
    target = std::min(100.0, std::max(0.0, target));
    double k = 1 - std::exp(-8 * h / 30); // ~8 points an hour when 30 away
    need[Mood] += (target - need[Mood]) * k;

    if (need[Bladder] <= 0) {
        need[Bladder] = 100;
        change(Hygiene, -50);
        change(Mood, -20);
        note("You couldn't hold it any longer. You'll need a wash, and a change of clothes.");
    }
    if (need[Energy] <= 0 && a == Activity::Awake && !collapsed) {
        collapsed = true;
        note("Your legs give way. You sleep where you fall.");
    }
    int month = dayOf(t) / 30;
    if (month > lastCouponMonth) {
        lastCouponMonth = month;
        at(carried, Item::CouponSugar) += 1;
        at(carried, Item::CouponButter) += 2;
        at(carried, Item::CouponMeat) += 1;
        if (m > 0 || month > 0) note("This month's coupon book: sugar 1 kg, butter 2 x 200 g, meat 500 g.");
    }
}

double Sim::advance(double minutes, Activity a, const std::function<bool()>& stop) {
    double used = 0;
    while (used < minutes - 1e-9) {
        double step = std::min(1.0, minutes - used);
        t += step;
        used += step;
        tick(a, step);
        if (stop && stop()) break;
    }
    return used;
}

double Sim::step(const Action& a, double minutes) {
    return advance(minutes, a.activity, [&] {
        if (a.onMinute) a.onMinute();
        return a.interrupt && a.interrupt();
    });
}
double Sim::perform(const Action& a) {
    if (!a.enabled) return 0;
    double used = step(a, a.minutes);
    if (a.done) a.done();
    return used;
}

bool Sim::hotWater() const {
    double h = hourOf(t);
    return (h >= 6 && h < 9) || (h >= 18 && h < 23);
}

int Sim::have(const Ctx& c, Item i) const {
    int n = at(carried, i);
    for (auto& k : c.reach) {
        auto it = containers.find(k);
        if (it != containers.end()) n += at(it->second, i);
    }
    return n;
}
bool Sim::take(const Ctx& c, Item i, int n) {
    if (have(c, i) < n) return false;
    int& a = at(carried, i);
    int d = std::min(a, n);
    a -= d;
    n -= d;
    for (auto& k : c.reach) {
        if (!n) break;
        auto it = containers.find(k);
        if (it == containers.end()) continue;
        int& b = at(it->second, i);
        d = std::min(b, n);
        b -= d;
        n -= d;
    }
    return true;
}

std::string Sim::buy(const Good& g) {
    if (cash < g.price) return "Not enough money.";
    if (g.coupon != Item::ITEM_N && at(carried, g.coupon) < 1) return std::string("You need a ") + itemDef(g.coupon).name + ".";
    cash -= g.price;
    if (g.coupon != Item::ITEM_N) at(carried, g.coupon) -= 1;
    at(carried, g.item) += g.units;
    return "";
}

/* ================= BROADCASTS ================= */
std::string Sim::radioLine() const {
    double h = hourOf(t);
    int slot = (int)(std::fmod(t, HOUR) / 4); // a new line every 4 game minutes
    auto line = [&](const std::vector<const char*>& v) { return std::string(v[(slot + dayOf(t)) % v.size()]); };
    if (h < 6) return "(a soft hum; Programme One has closed for the night)";
    if (std::fmod(t, HOUR) < 12)
        return "News from Dubrava: " + line({
            "Tractor Works No. 3 has exceeded its quarterly plan by four per cent.",
            "Drizzle over the capital, clearing by Domhnica, the Meteorological Office promises.",
            "The Aireacht Soláthair reminds citizens that coupon books are issued on the first of the month.",
            "A new mikrorayon of fourteen blocks has been handed over to tenants in Naomh Bríd district.",
            "Hot water is supplied from six until nine, and from six until eleven in the evening.",
            "The national hurling team drew with the visitors from Brno, two-eleven to one-fourteen.",
            "Bus 14 will run to its winter timetable from the first of next month.",
        });
    if (h < 8) return "Morning Exercises: " + line({"...and stretch, two, three, four. Breathe in the good Dubrava air.",
                                                     "Arms up! Arms down! Now touch your toes, citizens.",
                                                     "(a piano plays a brisk march, slightly out of tune)"});
    if (h < 12) return "Songs of the Bogs and Birches: " + line({"(an accordion and a bodhrán play a slow air)",
                                                                  "(a choir sings 'The Oak Grove of Dubrava')",
                                                                  "(a fiddle reel, then a balalaika takes up the tune)"});
    if (h < 14) return "The Worker's Lunch Hour: " + line({"A letter from a listener in Mikrorayon 7, who asks for more buckwheat in the kiosks.",
                                                            "Today's recipe: kasha with butter, and a little patience.",
                                                            "(a comic sketch about a man who cannot find his coupon book)"});
    if (h < 18) return "Afternoon Concert: " + line({"(a string quartet; someone coughs in the studio)",
                                                     "(a tenor sings 'The Rain on the Panel Roofs')"});
    if (h < 20) return "Poetry Hour: " + line({"\"The rain on the panel is the rain on us all.\" (S. Ó Brodaigh)",
                                              "\"Grey is the colour of every good morning.\" (D. Novák)",
                                              "\"I queued for bread and found the sun instead.\" (N. Mac Iván)"});
    return "Evening Programme: " + line({"(a slow waltz)", "A talk on the history of the bog-oak carvers of Dubrava.",
                                         "(the national anthem; then a weather report: drizzle)"});
}
std::string Sim::headline() const {
    static const std::vector<const char*> H{
        "PLAN EXCEEDED AT TRACTOR WORKS NO. 3",
        "NEW BLOCKS HANDED OVER IN NAOMH BRÍD",
        "BUCKWHEAT HARVEST 'THE BEST IN A DECADE'",
        "CITIZENS PRAISE NEW BUS 14 TIMETABLE",
        "HURLING: A DRAW WITH BRNO",
        "DRIZZLE TO CONTINUE, SAYS METEOROLOGICAL OFFICE",
        "HOT WATER SCHEDULE: READ AND OBSERVE",
    };
    return H[dayOf(t) % H.size()];
}

/* ================= ACTIONS ================= */
Action Sim::eat(const Ctx& c, const std::string& label, double minutes, std::vector<std::pair<Item, int>> use, std::array<double, NEED_N> gain) {
    Action a;
    a.label = label;
    a.minutes = minutes;
    for (auto& u : use)
        if (have(c, u.first) < u.second) { a.enabled = false; a.why = std::string("No ") + itemDef(u.first).name; }
    a.done = [this, c, use, gain] {
        for (auto& u : use)
            if (!take(c, u.first, u.second)) return;
        for (int n = 0; n < NEED_N; n++) change((Need)n, gain[n]);
    };
    return a;
}

Action Sim::workShift() {
    Action a;
    a.label = "Take bus 14 to the Tractor Works";
    a.fade = true;
    a.activity = Activity::Work;
    double h = hourOf(t) * HOUR, arrive = h + BUS_RIDE;
    a.minutes = SHIFT_END + BUS_RIDE - h;
    int wd = weekdayOf(t);
    if (wd >= 5) { a.enabled = false; a.why = std::string("No shift on ") + weekdayName(wd); }
    else if (lastWorkDay == dayOf(t)) { a.enabled = false; a.why = "You've done today's shift"; }
    else if (h < 6.5 * HOUR) { a.enabled = false; a.why = "Bus 14 starts at 06:30"; }
    else if (arrive > 10 * HOUR) { a.enabled = false; a.why = "Too late for today's shift"; }
    auto lunched = std::make_shared<bool>(false);
    a.onMinute = [this, lunched] {
        if (*lunched || hourOf(t) < 12) return;
        *lunched = true;
        if (at(carried, Item::Lunch) > 0) {
            at(carried, Item::Lunch) -= 1;
            change(Hunger, 35);
            change(Mood, 3);
            note("Lunch: you eat the parcel you packed, sitting on an upturned crate.");
        } else if (cash >= 35) {
            cash -= 35;
            change(Hunger, 28);
            note("Lunch: cabbage soup and black bread in the works canteen (0.35 p.).");
        } else {
            change(Mood, -8);
            note("Lunch: no parcel and no money for the canteen. The afternoon is long.");
        }
    };
    a.done = [this, arrive] {
        int day = dayOf(t);
        lastWorkDay = day;
        double late = std::max(0.0, arrive - SHIFT_START);
        double worked = SHIFT_END - std::max(SHIFT_START, arrive);
        int pay = (int)std::lround(WAGE * std::max(0.0, worked) / (SHIFT_END - SHIFT_START));
        wagesOwed += pay;
        need[Bladder] = std::max(need[Bladder], 70.0);
        if (late > 0) note("You clocked in " + std::to_string((int)late) + " minutes late. The foreman docked your pay.");
        note("Shift done: " + money(pay) + " earned, paid on Aoinek.");
        if (weekdayOf(t) == 4) {
            cash += wagesOwed;
            note("Payday: " + money(wagesOwed) + " in a brown envelope.");
            wagesOwed = 0;
        }
    };
    return a;
}

std::vector<Action> Sim::actions(const Ctx& c) {
    std::vector<Action> v;
    auto simple = [&](const std::string& label, double minutes, std::function<void()> done, bool en = true, const std::string& why = "") {
        Action a;
        a.label = label; a.minutes = minutes; a.done = std::move(done); a.enabled = en; a.why = why;
        v.push_back(a);
    };
    const double h = hourOf(t);
    switch (c.kind) {
    case Obj::Bed: {
        auto sleep = [&](const std::string& label, double minutes, bool en, const std::string& why) {
            Action a;
            a.label = label; a.minutes = minutes; a.activity = Activity::Sleep; a.fade = true; a.enabled = en; a.why = why;
            a.interrupt = [this] {
                if (need[Bladder] < 6) { note("You wake up needing the toilet."); return true; }
                return false;
            };
            a.done = [this] { collapsed = false; };
            v.push_back(a);
        };
        double toMorning = std::fmod(6.5 * HOUR - std::fmod(t, DAY) + DAY, DAY);
        if (toMorning < 30) toMorning += DAY;
        bool night = h >= 18 || h < 6;
        sleep("Sleep until 06:30", toMorning, night && need[Energy] < 95, night ? "You're not tired" : "It's the middle of the day");
        sleep("Sleep until rested", (100 - need[Energy]) / DECAY_SLEEP[Energy] * HOUR + 10, need[Energy] < 80, "You're not tired enough");
        sleep("Nap for an hour", HOUR, need[Energy] < 95, "You're not tired");
        break;
    }
    case Obj::Wardrobe:
        simple("Change into fresh clothes", 4, [this] { change(Hygiene, 8); change(Mood, 2); });
        break;
    case Obj::Sofa:
        simple("Sit and rest a while", 20, [this] { change(Energy, 4); change(Mood, 4); });
        break;
    case Obj::Table: {
        //                                                                         Hun  Thi  Ene Bla Hyg Mood
        v.push_back(eat(c, "Eat a bowl of kasha", 10, {{Item::Kasha, 1}}, {35, 0, 0, 0, 0, 4}));
        v.push_back(eat(c, "Eat bread with sausage", 5, {{Item::Bread, 1}, {Item::Sausage, 1}}, {16, 0, 0, 0, 0, 3}));
        v.push_back(eat(c, "Eat bread and butter", 5, {{Item::Bread, 1}, {Item::Butter, 1}}, {12, 0, 0, 0, 0, 2}));
        v.push_back(eat(c, "Eat a slice of bread", 3, {{Item::Bread, 1}}, {8, 0, 0, 0, 0, 0}));
        v.push_back(eat(c, "Drink a glass of milk", 2, {{Item::Milk, 1}}, {5, 18, 0, -4, 0, 1}));
        v.push_back(eat(c, "Drink a glass of kefir", 2, {{Item::Kefir, 1}}, {6, 15, 0, -4, 0, 2}));
        v.push_back(eat(c, "Eat your lunch parcel", 10, {{Item::Lunch, 1}}, {35, 0, 0, 0, 0, 3}));
        {
            Action a = eat(c, "Pack a lunch for work", 4, {{Item::Bread, 2}, {Item::Sausage, 2}}, {});
            auto base = a.done;
            a.done = [this, base, c] {
                if (have(c, Item::Bread) < 2 || have(c, Item::Sausage) < 2) return;
                base();
                at(carried, Item::Lunch) += 1;
                note("You wrap two sausage sandwiches in yesterday's Fírinne.");
            };
            v.push_back(a);
        }
        if (have(c, Item::Newspaper) > 0) {
            Action a = eat(c, "Read Fírinne", 20, {{Item::Newspaper, 1}}, {0, 0, 0, 0, 0, 6});
            a.ui = UI_NEWSPAPER;
            v.push_back(a);
        }
        // only list what you can actually do, unless nothing at all is possible
        std::vector<Action> ok;
        for (auto& a : v) if (a.enabled) ok.push_back(a);
        if (ok.empty()) { v.clear(); simple("Eat something", 0, nullptr, false, "Nothing to eat or drink within reach"); }
        else v = ok;
        break;
    }
    case Obj::Stove: {
        bool sugar = have(c, Item::Sugar) > 0;
        Action tea = eat(c, sugar ? "Make a cup of tea with sugar" : "Make a cup of tea", 7, {{Item::Tea, 1}}, {0, 25, 2, -6, 0, 5});
        auto base = tea.done;
        tea.done = [this, base, c, sugar] {
            base();
            if (sugar && take(c, Item::Sugar, 1)) change(Mood, 3);
        };
        v.push_back(tea);
        bool butter = have(c, Item::Butter) > 0;
        Action k = eat(c, butter ? "Cook buckwheat kasha (with butter)" : "Cook buckwheat kasha", 20, {{Item::Buckwheat, 1}}, {});
        base = k.done;
        k.done = [this, base, c, butter] {
            if (have(c, Item::Buckwheat) < 1) return;
            base();
            if (butter) take(c, Item::Butter, 1);
            at(carried, Item::Kasha) += 1;
            note(butter ? "A bowl of kasha with a pat of butter melting into it." : "A bowl of plain kasha.");
        };
        v.push_back(k);
        break;
    }
    case Obj::Fridge:
    case Obj::Cupboard: {
        Action a;
        a.label = c.kind == Obj::Fridge ? "Open the fridge" : "Open the cupboard";
        a.ui = UI_CONTAINER;
        v.push_back(a);
        break;
    }
    case Obj::KitchenSink:
        simple("Drink a glass of water", 1, [this] { change(Thirst, 15); change(Bladder, -4); });
        simple("Wash your hands", 1, [this] { change(Hygiene, 4); });
        break;
    case Obj::BathSink:
        simple("Wash your face and hands", 3, [this] { change(Hygiene, 10); });
        simple("Brush your teeth", 3, [this] { change(Hygiene, 5); change(Mood, 2); });
        simple("Drink from the tap", 1, [this] { change(Thirst, 12); change(Bladder, -3); });
        break;
    case Obj::Toilet:
        simple("Use the toilet", 3, [this] { need[Bladder] = 100; change(Hygiene, -1); }, need[Bladder] < 90, "You don't need to go");
        break;
    case Obj::Bath:
        if (hotWater()) simple("Take a hot bath", 25, [this] { need[Hygiene] = 100; change(Mood, 8); change(Energy, 3); });
        else simple(std::string("Take a cold bath (hot water from ") + (h < 6 ? "06:00" : "18:00") + ")", 10,
                    [this] { change(Hygiene, 60); change(Mood, -8); });
        break;
    case Obj::Radio:
        simple(radioOn ? "Switch the radio off" : "Switch the radio on", 0, [this] { radioOn = !radioOn; });
        break;
    case Obj::TV:
        if (h >= 18 && h < 23) simple("Watch the evening programme", 30, [this] { change(Mood, 10); change(Energy, -1); });
        else simple("Watch the test card", 10, [this] { change(Mood, -1); });
        break;
    case Obj::LightSwitch: {
        bool on = switches[c.key];
        simple(on ? "Switch the light off" : "Switch the light on", 0, [this, key = c.key] { switches[key] = !switches[key]; });
        break;
    }
    case Obj::BusStop:
        v.push_back(workShift());
        break;
    case Obj::Kiosk: {
        int wd = weekdayOf(t);
        double close = wd == 6 ? 14 : 20;
        bool open = h >= 7 && h < close;
        Action a;
        a.label = "Queue at the kiosk";
        a.minutes = open ? std::round(rnd(2, 12)) : 0;
        a.ui = UI_KIOSK;
        a.enabled = open;
        a.why = h < 7 ? "Closed. Opens at 07:00" : std::string("Closed. Open 07:00 to ") + (wd == 6 ? "14:00 on Domhnica" : "20:00");
        v.push_back(a);
        break;
    }
    default: break;
    }
    return v;
}

} // namespace sim

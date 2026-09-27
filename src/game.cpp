// "Live" mode: first-person everyday life. See game.h.
#include "game.h"

#include "sim/sim.h"
#include "ui.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>

namespace game {

Host host;

namespace {

using sim::Obj;

constexpr double LOOK_RANGE = 2.1;        // metres: reach for things indoors
constexpr double STREET_RANGE = 3.5;      // bus stops and kiosks
constexpr double WALK_SPEED = 1.0;        // game minutes per real second while just living
constexpr double ACTION_SPEED = 12;       // game minutes per real second while doing something
constexpr double SKIP_SPEED = 240;        // sleep and work shifts

bool live = false;
sim::Sim S;
int homeRec = -1, homeFlat = -1, homeFloor = -1;

// what the crosshair is on
struct Target {
    bool valid = false, interior = true, mine = false;
    int idx = -1;                         // Interior::objs or WOBJ index
    Obj kind = Obj::None;
};
Target tgt;
sim::Ctx ctx;
std::vector<sim::Action> acts;

// the action being carried out
bool running = false;
sim::Action run;
double runLeft = 0;

// menus
int menu = 0;                             // sim::Sim::UI_*
bool notebook = false;
sim::Ctx menuCtx;
std::string menuMsg;

std::map<int, bool> lightApplied;         // room -> state last pushed to the renderer
struct ShownNote { std::string text; double at; };
std::vector<ShownNote> shown;
unsigned notesSeen = 0;
double realT = 0;

std::string objKey(int idx) { return "home/" + std::to_string(idx); }
std::string switchKey(int room) { return "light/" + std::to_string(room); }

bool isHome(const Interior& in, const IObj& o) { return in.rec == homeRec && o.flat == homeFlat && o.floor == homeFloor; }

// Context for a home object: containers in the same room are within reach.
sim::Ctx ctxFor(const Interior& in, int idx) {
    const IObj& o = in.objs[idx];
    sim::Ctx c;
    c.kind = o.kind;
    c.key = o.kind == Obj::LightSwitch ? switchKey(o.room) : objKey(idx);
    c.home = isHome(in, o);
    for (size_t i = 0; i < in.objs.size(); i++) {
        const IObj& p = in.objs[i];
        if ((p.kind == Obj::Fridge || p.kind == Obj::Cupboard) && p.room == o.room && isHome(in, p)) c.reach.push_back(objKey((int)i));
    }
    return c;
}

// Ray / box: distance along the (unit) ray to the box, or -1.
double hitBox(const V3& o, const V3& d, const V3& lo, const V3& hi) {
    double t0 = 0, t1 = 1e9;
    for (int i = 0; i < 3; i++) {
        if (std::fabs(d[i]) < 1e-9) {
            if (o[i] < lo[i] || o[i] > hi[i]) return -1;
            continue;
        }
        double a = (lo[i] - o[i]) / d[i], b = (hi[i] - o[i]) / d[i];
        if (a > b) std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        if (t0 > t1) return -1;
    }
    return t0;
}

void toLocalDir(const Interior& in, const V3& d, V3& out) {
    out = {d[0] * in.cs - d[2] * in.sn, d[1], d[0] * in.sn + d[2] * in.cs};
}
V3 toWorld(const Interior& in, const V3& p) {
    return {in.ox + p[0] * in.cs + p[2] * in.sn, p[1], in.oz - p[0] * in.sn + p[2] * in.cs};
}
V3 toWorldDir(const Interior& in, const V3& d) {
    return {d[0] * in.cs + d[2] * in.sn, d[1], -d[0] * in.sn + d[2] * in.cs};
}

void pickTarget(const V3& cam, const V3& look) {
    tgt = Target();
    V3 d{look[0] - cam[0], look[1] - cam[1], look[2] - cam[2]};
    double l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (l < 1e-9) return;
    d = {d[0] / l, d[1] / l, d[2] / l};
    double best = 1e9;
    const Interior& in = host.interior();
    if (in.rec >= 0) {
        V3 lo{}, ld{};
        in.toLocal(cam[0], cam[2], lo[0], lo[2]);
        lo[1] = cam[1];
        toLocalDir(in, d, ld);
        // nearest wall in the way
        double wall = 1e9;
        for (auto& w : in.walls) {
            double h = hitBox(lo, ld, {w.x0, w.y0, w.z0}, {w.x1, w.y1, w.z1});
            if (h >= 0 && h < wall) wall = h;
        }
        for (size_t i = 0; i < in.objs.size(); i++) {
            const IObj& o = in.objs[i];
            double h = hitBox(lo, ld, o.lo, o.hi);
            if (h < 0 || h > LOOK_RANGE || h > wall + .01 || h >= best) continue;
            best = h;
            tgt.valid = true; tgt.interior = true; tgt.idx = (int)i; tgt.kind = o.kind; tgt.mine = isHome(in, o);
        }
    }
    for (size_t i = 0; i < WOBJ.size(); i++) {
        double h = hitBox(cam, d, WOBJ[i].lo, WOBJ[i].hi);
        if (h < 0 || h > STREET_RANGE || h >= best) continue;
        best = h;
        tgt.valid = true; tgt.interior = false; tgt.idx = (int)i; tgt.kind = WOBJ[i].kind; tgt.mine = true;
    }
}

void syncLights() {
    const Interior& in = host.interior();
    if (in.rec != homeRec) return;
    for (size_t i = 0; i < in.objs.size(); i++) {
        const IObj& o = in.objs[i];
        if (o.kind != Obj::LightSwitch || !isHome(in, o)) continue;
        bool on = S.switches[switchKey(o.room)];
        auto it = lightApplied.find(o.room);
        if (it != lightApplied.end() && it->second == on) continue;
        lightApplied[o.room] = on;
        host.roomLight(o.room, on);
    }
}

void finish() {
    running = false;
    if (run.done) run.done();
    if (run.ui) {
        menu = run.ui;
        menuCtx = ctx;
        menuMsg.clear();
    }
    syncLights();
}

void begin(const sim::Action& a) {
    if (!a.enabled) return;
    run = a;
    runLeft = a.minutes;
    running = true;
    if (a.minutes <= 0) finish();
}

bool radioInEarshot(const V3& cam, double feet) {
    const Interior& in = host.interior();
    if (in.rec != homeRec) return false;
    for (auto& o : in.objs) {
        if (o.kind != Obj::Radio || !isHome(in, o)) continue;
        V3 c = toWorld(in, {(o.lo[0] + o.hi[0]) / 2, 0, (o.lo[2] + o.hi[2]) / 2});
        return std::hypot(c[0] - cam[0], c[2] - cam[2]) < 9 && std::fabs(feet - o.lo[1] + 1.5) < 1.6;
    }
    return false;
}

void collectNotes() {
    size_t fresh = std::min<size_t>(S.noteCount - notesSeen, S.notes.size());
    for (size_t i = S.notes.size() - fresh; i < S.notes.size(); i++) shown.push_back({S.notes[i].text, realT});
    notesSeen = S.noteCount;
    while (shown.size() > 5) shown.erase(shown.begin());
}

/* ================= DRAWING ================= */
Color needColor(double v) {
    if (v < 12) return TH.red;
    if (v < 30) return TH.amber;
    return hexc(0x9fc79a);
}

// A clickable row; returns true when clicked this frame.
bool rowButton(Rectangle r, const std::string& left, const std::string& right, bool enabled, Color fg) {
    Vector2 m = GetMousePosition();
    bool hover = enabled && CheckCollisionPointRec(m, r);
    rr(r, 6, hover ? fade(TH.plate, .25f) : fade(TH.enamelDeep, .9f));
    txt(18, left, r.x + 10, r.y + 4, 20, enabled ? fg : TH.dim);
    if (!right.empty()) txt(18, right, r.x + r.width - 10 - tw(18, right), r.y + 4, 20, enabled ? TH.plate : TH.dim);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void drawHUD(int W, int H) {
    // needs panel, bottom left
    const float pw = 250, rowH = 21, ph = 58 + sim::NEED_N * rowH + 12;
    Rectangle P{12, H - 12 - ph, pw, ph};
    plate(P, 10, 4, 7, TH.enamel);
    txt(20, sim::stampText(S.t), P.x + 14, P.y + 10, 22, TH.ink);
    txt(17, sim::money(S.cash) + (S.wagesOwed ? "  (owed " + sim::money(S.wagesOwed) + ")" : ""), P.x + 14, P.y + 32, 19, TH.dim);
    for (int n = 0; n < sim::NEED_N; n++) {
        float y = P.y + 58 + n * rowH;
        double v = S.need[n];
        txt(17, sim::needName((sim::Need)n), P.x + 14, y, 17, TH.dim);
        Rectangle bar{P.x + 92, y + 4, pw - 106, 9};
        rr(bar, 3, TH.enamelDeep);
        rr({bar.x, bar.y, bar.width * (float)(v / 100), bar.height}, 3, needColor(v));
    }
    // notes, top left under the brand plate
    for (size_t i = 0; i < shown.size(); i++) {
        double age = realT - shown[i].at;
        float a = (float)std::min(1.0, std::max(0.0, (14 - age) / 2));
        if (a <= 0) continue;
        const std::string& s = shown[i].text;
        float w = std::min(tw(18, s) + 20, W * .6f);
        Rectangle r{12, 160 + (float)i * 31, w, 27};
        rr(r, 4, fade({0, 0, 0, 158}, a));
        BeginScissorMode((int)r.x, (int)r.y, (int)r.width - 6, (int)r.height);
        txt(18, s, r.x + 10, r.y + 4, 19, fade(WHITE, a));
        EndScissorMode();
    }
    // the radio, as subtitles
    if (S.radioOn && S.radioHeard) {
        std::string s = "Radio: " + S.radioLine();
        float w = std::min(tw(18, s) + 24, W - pw - 60.f);
        Rectangle r{(W - w) / 2, H - 46.f, w, 28};
        rr(r, 4, {0, 0, 0, 150});
        BeginScissorMode((int)r.x, (int)r.y, (int)r.width - 8, (int)r.height);
        txt(18, s, r.x + 12, r.y + 4, 20, hexc(0xe9e4d6));
        EndScissorMode();
    }
}

void drawPrompt(int W, int H) {
    if (running || menu || notebook) return;
    // crosshair
    DrawRectangle(W / 2 - 1, H / 2 - 1, 3, 3, fade(WHITE, .8f));
    if (!tgt.valid) return;
    std::vector<std::string> lines;
    std::vector<bool> ok;
    std::string title = sim::objName(tgt.kind);
    if (!tgt.mine) title += "  (not your flat)";
    int n = 0;
    if (tgt.mine)
        for (auto& a : acts) {
            n++;
            std::string key = n == 1 ? "[E]" : "[" + std::to_string(n) + "]";
            std::string dur = a.minutes >= 60 ? " (" + std::to_string((int)std::round(a.minutes / 60)) + " h)" : a.minutes >= 1 ? " (" + std::to_string((int)std::round(a.minutes)) + " min)" : "";
            lines.push_back(a.enabled ? key + " " + a.label + dur : key + " " + a.label + ": " + a.why);
            ok.push_back(a.enabled);
            if (n == 9) break;
        }
    float w = tw(20, title) + 28;
    for (auto& l : lines) w = std::max(w, tw(18, l) + 28);
    float h = 34 + lines.size() * 21.f + 8;
    Rectangle r{W / 2.f + 28, H / 2.f - h / 2, std::min(w, W / 2.f - 40), h};
    rr(r, 6, {10, 16, 30, 190});
    txt(20, title, r.x + 14, r.y + 8, 22, hexc(0xf0a33a));
    for (size_t i = 0; i < lines.size(); i++) txt(18, lines[i], r.x + 14, r.y + 34 + i * 21, 20, ok[i] ? WHITE : fade(WHITE, .45f));
}

void drawProgress(int W, int H) {
    if (!running) return;
    double done = run.minutes > 0 ? 1 - runLeft / run.minutes : 1;
    if (run.fade) {
        float a = (float)std::min(1.0, std::min(done, 1 - done) * 12 + .15);
        DrawRectangle(0, 0, W, H, fade(BLACK, std::min(1.f, a * 1.6f)));
        std::string c = sim::clockText(S.t);
        txt(34, c, (W - tw(34, c, 2)) / 2, H / 2.f - 40, 34, fade(WHITE, a), 2);
        std::string what = run.activity == sim::Activity::Work ? "At the Tractor Works..." : "Asleep...";
        txt(20, what, (W - tw(20, what)) / 2, H / 2.f + 4, 22, fade(hexc(0x8f8a7d), a));
        return;
    }
    Rectangle r{(W - 320) / 2.f, H * .62f, 320, 48};
    rr(r, 6, {10, 16, 30, 200});
    txt(18, run.label, r.x + 12, r.y + 6, 20, WHITE);
    Rectangle bar{r.x + 12, r.y + 30, r.width - 24, 8};
    rr(bar, 3, TH.enamelDeep);
    rr({bar.x, bar.y, bar.width * (float)std::min(1.0, done), bar.height}, 3, TH.amber);
}

Rectangle menuPlate(int W, int H, float w, float h, const std::string& title) {
    Rectangle P{(W - w) / 2, (H - h) / 2, w, h};
    plate(P, 14, 5, 10, TH.enamel);
    txt(30, title, P.x + 20, P.y + 14, 30, TH.ink);
    return P;
}
bool closeButton(Rectangle P, const char* label = "Close [Esc]") {
    Rectangle b{P.x + P.width - 20 - tw(18, label) - 20, P.y + P.height - 44, tw(18, label) + 20, 30};
    return rowButton(b, label, "", true, WHITE);
}

void drawKiosk(int W, int H) {
    const auto& G = sim::kioskGoods();
    Rectangle P = menuPlate(W, H, 540, 120 + G.size() * 34.f + 40, "Kiosk");
    txt(17, "Cash " + sim::money(S.cash) + ". Click to buy.", P.x + 20, P.y + 48, 19, TH.dim);
    for (size_t i = 0; i < G.size(); i++) {
        const sim::Good& g = G[i];
        std::string right = sim::money(g.price) + (g.coupon != sim::Item::ITEM_N ? " + coupon" : "");
        Rectangle r{P.x + 20, P.y + 76 + i * 34.f, P.width - 40, 30};
        if (rowButton(r, g.label, right, true, WHITE)) {
            std::string e = S.buy(g);
            menuMsg = e.empty() ? std::string("Bought: ") + g.label + "." : e;
        }
    }
    if (!menuMsg.empty()) txt(18, menuMsg, P.x + 20, P.y + P.height - 40, 20, TH.amber);
    if (closeButton(P)) menu = 0;
}

void drawContainer(int W, int H) {
    auto it = S.containers.find(menuCtx.key);
    if (it == S.containers.end()) { menu = 0; return; }
    sim::Inv& box = it->second;
    Rectangle P = menuPlate(W, H, 640, 440, sim::objName(menuCtx.kind));
    txt(17, "Click an item to move one; Shift-click moves them all.", P.x + 20, P.y + 48, 19, TH.dim);
    bool all = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    auto column = [&](float x, const char* head, sim::Inv& from, sim::Inv& to) {
        txt(20, head, x, P.y + 76, 22, TH.plate);
        float y = P.y + 104;
        for (int i = 0; i < (int)sim::Item::ITEM_N; i++) {
            sim::Item item = (sim::Item)i;
            int n = sim::at(from, item);
            if (!n || item >= sim::Item::CouponSugar) continue;
            Rectangle r{x, y, P.width / 2 - 30, 28};
            const sim::ItemDef& d = itemDef(item);
            if (rowButton(r, d.name, std::to_string(n) + " " + (n == 1 ? d.unit : d.units), true, WHITE)) {
                int k = all ? n : 1;
                sim::at(from, item) -= k;
                sim::at(to, item) += k;
            }
            y += 31;
            if (y > P.y + P.height - 60) break;
        }
    };
    column(P.x + 20, "Carrying", S.carried, box);
    column(P.x + P.width / 2 + 10, sim::objName(menuCtx.kind), box, S.carried);
    if (closeButton(P)) menu = 0;
}

void drawNewspaper(int W, int H) {
    Rectangle P = menuPlate(W, H, 560, 330, "FÍRINNE");
    txt(17, std::string(sim::weekdayName(sim::weekdayOf(S.t))) + ", day " + std::to_string(sim::dayOf(S.t) + 1) + ". Price 3 pingin.", P.x + 20, P.y + 48, 19, TH.dim);
    txt(21, S.headline(), P.x + 20, P.y + 84, 24, WHITE);
    const char* body[] = {"The Aireacht Soláthair reports steady supplies of bread, milk and",
                          "buckwheat in every district of Dubrava. Citizens are reminded that",
                          "coupon goods are sold on presentation of a valid coupon book.",
                          "Weather: drizzle, clearing later. Hot water 06:00-09:00, 18:00-23:00."};
    for (int i = 0; i < 4; i++) txt(18, body[i], P.x + 20, P.y + 124 + i * 24, 22, TH.plate);
    if (closeButton(P)) menu = 0;
}

void drawNotebook(int W, int H) {
    Rectangle P = menuPlate(W, H, 620, 470, "Notebook");
    float x = P.x + 22, y = P.y + 56;
    auto line = [&](const std::string& s, Color c) { txt(18, s, x, y, 20, c); y += 22; };
    line(S.me.fullName() + ", flat " + std::to_string(S.me.flat) + ", block " + std::to_string(S.me.block) + ", " + S.me.street, WHITE);
    line("Lathe operator, Dubrava Tractor Works No. 3. Shift 08:00-17:00, Luanek to Aoinek.", TH.plate);
    line("Bus 14 from the stop at the district's edge, from 06:30. The ride is 25 minutes.", TH.plate);
    y += 6;
    line("Cash " + sim::money(S.cash) + ".  Wages owed " + sim::money(S.wagesOwed) + " (paid Aoinek).", WHITE);
    y += 6;
    line("Carrying:", TH.amber);
    bool any = false;
    for (int i = 0; i < (int)sim::Item::ITEM_N; i++) {
        int n = sim::at(S.carried, (sim::Item)i);
        if (!n) continue;
        any = true;
        line("  " + sim::itemCount((sim::Item)i, n), WHITE);
        if (y > P.y + P.height - 110) break;
    }
    if (!any) line("  nothing", TH.dim);
    y += 6;
    std::string warn;
    for (int n = 0; n < sim::NEED_N; n++) {
        const char* w = sim::needWarning((sim::Need)n, S.need[n]);
        if (*w) warn += (warn.empty() ? "" : ", ") + std::string(w);
    }
    line(warn.empty() ? "You feel all right." : "You feel: " + warn + ".", warn.empty() ? TH.plate : TH.amber);
    if (closeButton(P, "Close [Tab]")) notebook = false;
}

} // namespace

/* ================= API ================= */
bool active() { return live; }
bool freezeWalker() { return live && (running || menu || notebook); }
bool wantsMouse() { return live && (menu || notebook); }

void stop() {
    live = false;
    running = false;
    menu = 0;
    notebook = false;
    homeRec = homeFlat = homeFloor = -1;
    lightApplied.clear();
    shown.clear();
}

bool start(double seed, std::string& why) {
    stop();
    // candidate homes: residential blocks, nearest the first bus stop first
    V3 stop0{0, 0, 0};
    for (auto& w : WOBJ) if (w.kind == Obj::BusStop) { stop0 = {(w.lo[0] + w.hi[0]) / 2, 0, (w.lo[2] + w.hi[2]) / 2}; break; }
    std::vector<std::pair<double, int>> cands;
    for (size_t i = 0; i < RECS.size(); i++) {
        const BuildingRec& R = RECS[i];
        if (R.kind != 0 || R.col < 0 || R.type == "school" || R.type == "univermag") continue;
        cands.push_back({std::hypot(R.ox - stop0[0], R.oz - stop0[2]), (int)i});
    }
    std::sort(cands.begin(), cands.end());
    std::mt19937 g((uint32_t)(seed * 2654435761.0));
    for (auto& c : cands) {
        Interior probe = buildInterior(c.second, seed);
        std::vector<IFlat> ok;
        for (auto& h : probe.homes) if (h.floor >= 1 && h.floor <= 3) ok.push_back(h);
        if (ok.empty()) ok = probe.homes;
        if (ok.empty()) continue;
        const IFlat h = ok[std::uniform_int_distribution<size_t>(0, ok.size() - 1)(g)];
        homeRec = c.second; homeFlat = h.flat; homeFloor = h.floor;
        host.loadHome(homeRec, homeFlat, homeFloor);
        const Interior& in = host.interior();
        std::string fridge, cupboard;
        for (size_t i = 0; i < in.objs.size(); i++) {
            if (!isHome(in, in.objs[i])) continue;
            if (in.objs[i].kind == Obj::Fridge && fridge.empty()) fridge = objKey((int)i);
            if (in.objs[i].kind == Obj::Cupboard && cupboard.empty()) cupboard = objKey((int)i);
        }
        sim::Profile p = sim::Sim::randomProfile(g);
        p.flat = h.number;
        p.block = homeRec + 1;
        S = sim::Sim(g());
        S.newLife(p, fridge.empty() ? "home/fridge" : fridge, cupboard.empty() ? "home/cupboard" : cupboard);
        live = true;
        lightApplied.clear();
        notesSeen = 0;
        syncLights();
        V3 w = toWorld(in, h.bedSide), d = toWorldDir(in, h.bedLook);
        host.place(w[0], w[2], h.bedSide[1], std::atan2(-d[0], -d[2]), -.35);
        host.setHour(sim::hourOf(S.t));
        return true;
    }
    why = "No flat with a kitchen, bedroom and bathroom here. Try another seed.";
    return false;
}

void interiorLoaded() {
    if (!live) return;
    lightApplied.clear();
    syncLights();
}

void update(double dt, const V3& cam, const V3& look, double feet) {
    if (!live) return;
    realT += dt;
    S.radioHeard = radioInEarshot(cam, feet);
    if (running) {
        double speed = run.fade ? SKIP_SPEED : ACTION_SPEED;
        double want = std::min(runLeft, dt * speed);
        double used = S.step(run, want);
        runLeft -= used;
        if (used < want - 1e-9 || runLeft <= 1e-9) finish();
    } else {
        S.advance(dt * WALK_SPEED, sim::Activity::Awake);
        if (S.collapsed) {
            sim::Ctx bed;
            bed.kind = Obj::Bed;
            for (auto& a : S.actions(bed))
                if (a.label == "Sleep until rested") { ctx = bed; begin(a); break; }
        }
    }
    host.setHour(sim::hourOf(S.t));
    collectNotes();

    // keys
    if (IsKeyPressed(KEY_TAB) && !menu && !running) notebook = !notebook;
    if (IsKeyPressed(KEY_ESCAPE)) { menu = 0; notebook = false; }
    if (running || menu || notebook) { tgt = Target(); return; }

    pickTarget(cam, look);
    acts.clear();
    if (tgt.valid && tgt.mine) {
        if (tgt.interior) ctx = ctxFor(host.interior(), tgt.idx);
        else { ctx = sim::Ctx(); ctx.kind = tgt.kind; ctx.key = "street/" + std::to_string(tgt.idx); }
        acts = S.actions(ctx);
    }
    int pick = -1;
    if (IsKeyPressed(KEY_E)) {
        for (size_t i = 0; i < acts.size(); i++) if (acts[i].enabled) { pick = (int)i; break; }
    }
    for (int k = 0; k < 9; k++) if (IsKeyPressed(KEY_ONE + k)) pick = k;
    if (pick >= 0 && pick < (int)acts.size()) {
        if (acts[pick].enabled) begin(acts[pick]);
        else toast(acts[pick].why);
    }
}

void draw(int W, int H) {
    if (!live) return;
    drawHUD(W, H);
    drawPrompt(W, H);
    drawProgress(W, H);
    if (menu == sim::Sim::UI_KIOSK) drawKiosk(W, H);
    else if (menu == sim::Sim::UI_CONTAINER) drawContainer(W, H);
    else if (menu == sim::Sim::UI_NEWSPAPER) drawNewspaper(W, H);
    if (notebook) drawNotebook(W, H);
}

/* ================= DEBUG HOOKS ================= */
bool faceObject(Obj kind) {
    if (!live) return false;
    const Interior& in = host.interior();
    for (size_t i = 0; i < in.objs.size(); i++) {
        const IObj& o = in.objs[i];
        if (o.kind != kind || !isHome(in, o)) continue;
        V3 c{(o.lo[0] + o.hi[0]) / 2, (o.lo[1] + o.hi[1]) / 2, (o.lo[2] + o.hi[2]) / 2};
        double feet = 0;
        for (auto& h : in.homes) if (h.flat == homeFlat && h.floor == homeFloor) feet = h.bedSide[1];
        for (double dist : {1.0, 1.3, .8, 1.6})
            for (int k = 0; k < 16; k++) {
                double a = k * M_PI / 8;
                V3 p{c[0] + std::cos(a) * (dist + std::max(o.hi[0] - o.lo[0], o.hi[2] - o.lo[2]) / 2), 0, c[2] + std::sin(a) * (dist + std::max(o.hi[0] - o.lo[0], o.hi[2] - o.lo[2]) / 2)};
                V3 w = toWorld(in, p);
                if (host.blocked(w[0], w[2], feet)) continue;
                // must see it: no wall between
                V3 eye{p[0], feet + 1.65, p[2]}, d{c[0] - p[0], c[1] - eye[1], c[2] - p[2]};
                double l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                d = {d[0] / l, d[1] / l, d[2] / l};
                double reach = hitBox(eye, d, o.lo, o.hi);
                bool clear = reach >= 0 && reach < LOOK_RANGE - .1;
                for (auto& wb : in.walls) {
                    if (!clear) break;
                    // furniture has collision boxes too: the object's own box doesn't hide it
                    if (c[0] > wb.x0 - .05 && c[0] < wb.x1 + .05 && c[1] > wb.y0 - .05 && c[1] < wb.y1 + .05 && c[2] > wb.z0 - .05 && c[2] < wb.z1 + .05) continue;
                    double h = hitBox(eye, d, {wb.x0, wb.y0, wb.z0}, {wb.x1, wb.y1, wb.z1});
                    if (h >= 0 && h < reach - .01) { clear = false; break; }
                }
                if (!clear) continue;
                V3 wd = toWorldDir(in, d);
                host.place(w[0], w[2], feet, std::atan2(-wd[0], -wd[2]), std::asin(d[1]));
                return true;
            }
    }
    return false;
}

void runScript(const std::string& script) {
    if (!live) return;
    size_t p = 0;
    while (p < script.size()) {
        size_t q = script.find(',', p);
        std::string tok = script.substr(p, q == std::string::npos ? std::string::npos : q - p);
        p = q == std::string::npos ? script.size() : q + 1;
        size_t c = tok.find(':');
        if (c == std::string::npos) continue;
        std::string name = tok.substr(0, c), arg = tok.substr(c + 1);
        if (name == "wait") { S.advance(std::atof(arg.c_str()), sim::Activity::Awake); continue; }
        if (name == "ui") {
            if (arg == "notebook") notebook = true;
            if (arg == "kiosk") menu = sim::Sim::UI_KIOSK;
            continue;
        }
        bool street = false;
        for (size_t i = 0; i < WOBJ.size() && !street; i++) {
            if (name != sim::objName(WOBJ[i].kind)) continue;
            street = true;
            ctx = sim::Ctx();
            ctx.kind = WOBJ[i].kind;
            auto v = S.actions(ctx);
            size_t n = (size_t)std::max(1, std::atoi(arg.c_str()));
            if (n <= v.size() && v[n - 1].enabled) {
                S.perform(v[n - 1]);
                if (v[n - 1].ui) { menu = v[n - 1].ui; menuCtx = ctx; }
            }
        }
        if (street) continue;
        const Interior& in = host.interior();
        for (size_t i = 0; i < in.objs.size(); i++) {
            if (!isHome(in, in.objs[i]) || name != sim::objName(in.objs[i].kind)) continue;
            ctx = ctxFor(in, (int)i);
            auto v = S.actions(ctx);
            size_t n = (size_t)std::max(1, std::atoi(arg.c_str()));
            if (n <= v.size() && v[n - 1].enabled) {
                S.perform(v[n - 1]);
                if (v[n - 1].ui) { menu = v[n - 1].ui; menuCtx = ctx; }
            }
            break;
        }
    }
    syncLights();
    host.setHour(sim::hourOf(S.t));
}

} // namespace game

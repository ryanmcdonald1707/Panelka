// Walk-in interiors (extension to the original page): podyezd stairwells, flats,
// churches and izbas. Built on demand per building from the facade records that
// genBuilding() leaves behind, so the seeded exterior stays byte-identical.
#include "panelka.h"

#include <algorithm>
#include <map>

static double H_hash(int a, int b) { return H(a * 7 + 3, b * 13 + 5, 991); }

namespace {

const double WALL_T = 0.2; // wall lining thickness (facade plane -> inner surface)
const double DOOR_H = 2.05;
const V3 UPV{0, 1, 0};
Interior* I = nullptr;

struct Hole { double s0, s1, y0, y1; };

/* ---- facade tile -> window / door rectangle in tile pixels (x0,y0,x1,y1; y=0 at the bottom) ---- */
bool tileHole(int t, int r[4]) {
    auto set = [&](int a, int b, int c, int d) { r[0] = a; r[1] = b; r[2] = c; r[3] = d; return true; };
    if (t >= 1 && t <= 4) return set(7, 9, 24, 26);
    if (t >= 7 && t <= 10) return set(8, 8, 23, 26);
    if (t >= 13 && t <= 16) return set(10, 6, 21, 25);
    if (t == TL::PANEL_ST || t == TL::BRICK_ST) return set(12, 3, 19, 29);
    if (t == TL::DOOR_P || t == TL::DOOR_B) return set(10, 0, 21, 19);
    if (t == TL::DOOR_S) return set(9, 0, 22, 24);
    if (t == TL::ARCH) return set(8, 0, 23, 24);
    if (t == TL::SHOP) return set(2, 2, 29, 24);
    if (t == TL::PLINTH_W) return set(9, 7, 22, 24);
    if (t == TL::SCHW) return set(2, 7, 29, 27);
    if (t == TL::CHWIN) return set(12, 5, 19, 24);
    if (t == 48 || t == 49) return set(11, 8, 20, 21);
    return false;
}

/* ---- geometry helpers (building-local; B carries the building transform) ---- */
V3 add3(const V3& a, const V3& b, double s) { return {a[0] + b[0] * s, a[1] + b[1] * s, a[2] + b[2] * s}; }
V3 cross3(const V3& a, const V3& b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double dot3(const V3& a, const V3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

// Tiled rectangle: points O + S*s + Vv*v, texture grid aligned to multiples of ts (so patterns line up around holes).
void face(const V3& O, const V3& S, const V3& Vv, double s0, double s1, double v0, double v1, int t, const V3& col, double tsS, double tsV, const Em& e = Em()) {
    if (s1 - s0 < 1e-4 || v1 - v0 < 1e-4) return;
    int i0 = (int)std::floor(s0 / tsS + 1e-9), i1 = (int)std::ceil(s1 / tsS - 1e-9);
    int j0 = (int)std::floor(v0 / tsV + 1e-9), j1 = (int)std::ceil(v1 / tsV - 1e-9);
    for (int i = i0; i < i1; i++) {
        double a = std::max(s0, i * tsS), b = std::min(s1, (i + 1) * tsS);
        if (b - a < 1e-5) continue;
        for (int j = j0; j < j1; j++) {
            double c = std::max(v0, j * tsV), d = std::min(v1, (j + 1) * tsV);
            if (d - c < 1e-5) continue;
            auto P = [&](double s, double v) { return add3(add3(O, S, s), Vv, v); };
            B.quadUV(P(a, c), P(b, c), P(b, d), P(a, d), t, col, e, (a - i * tsS) / tsS, (c - j * tsV) / tsV, (b - i * tsS) / tsS, (d - j * tsV) / tsV);
        }
    }
}
// Vertical rect on plane axis=0: x=c (w runs along z) / axis=1: z=c (w runs along x), facing sign along that axis.
void vrect(int axis, double c, int sign, double w0, double w1, double y0, double y1, int t, const V3& col, double ts = 1.2, const Em& e = Em()) {
    if (axis == 0) face({c, 0, 0}, {0, 0, -(double)sign}, UPV, sign > 0 ? -w1 : w0, sign > 0 ? -w0 : w1, y0, y1, t, col, ts, ts, e);
    else face({0, 0, c}, {(double)sign, 0, 0}, UPV, sign > 0 ? w0 : -w1, sign > 0 ? w1 : -w0, y0, y1, t, col, ts, ts, e);
}
void hrect(double x0, double z0, double x1, double z1, double y, bool up, int t, const V3& col, double ts = 1.2, const Em& e = Em()) {
    if (up) face({0, y, 0}, {1, 0, 0}, {0, 0, -1}, x0, x1, -z1, -z0, t, col, ts, ts, e);
    else face({0, y, 0}, {1, 0, 0}, {0, 0, 1}, x0, x1, z0, z1, t, col, ts, ts, e);
}
// Quad forced to face `want`.
void quadFacing(V3 a, V3 b, V3 c, V3 d, const V3& want, int t, const V3& col) {
    V3 n = cross3({b[0] - a[0], b[1] - a[1], b[2] - a[2]}, {d[0] - a[0], d[1] - a[1], d[2] - a[2]});
    if (dot3(n, want) < 0) B.quad(a, d, c, b, t, col);
    else B.quad(a, b, c, d, t, col);
}
void addBox(double x0, double z0, double x1, double z1, double y0, double y1) {
    I->walls.push_back({std::min(x0, x1), std::min(z0, z1), std::max(x0, x1), std::max(z0, z1), y0, y1});
}
// Solid pieces of [w0,w1]x[y0,y1] minus holes.
std::vector<std::array<double, 4>> solids(double w0, double w1, double y0, double y1, const std::vector<Hole>& holes) {
    std::vector<double> bp{w0, w1};
    for (auto& h : holes) { if (h.s0 > w0 && h.s0 < w1) bp.push_back(h.s0); if (h.s1 > w0 && h.s1 < w1) bp.push_back(h.s1); }
    std::sort(bp.begin(), bp.end());
    std::vector<std::array<double, 4>> out;
    for (size_t i = 0; i + 1 < bp.size(); i++) {
        double p = bp[i], q = bp[i + 1];
        if (q - p < 1e-5) continue;
        double mid = (p + q) / 2;
        std::vector<std::pair<double, double>> ys;
        for (auto& h : holes) if (h.s0 < mid && h.s1 > mid) ys.push_back({std::max(y0, h.y0), std::min(y1, h.y1)});
        std::sort(ys.begin(), ys.end());
        double cur = y0;
        for (auto& yy : ys) {
            if (yy.second <= yy.first) continue;
            if (yy.first > cur + 1e-5) out.push_back({p, q, cur, yy.first});
            cur = std::max(cur, yy.second);
        }
        if (y1 > cur + 1e-5) out.push_back({p, q, cur, y1});
    }
    return out;
}
// Wall panel with holes; optional collision slab of thickness [cLo,cHi] along the axis.
void wallHoles(int axis, double c, int sign, double w0, double w1, double y0, double y1, const std::vector<Hole>& holes, int t, const V3& col,
               bool collide = false, double cLo = 0, double cHi = 0, double ts = 1.2) {
    for (auto& r : solids(w0, w1, y0, y1, holes)) {
        vrect(axis, c, sign, r[0], r[1], r[2], r[3], t, col, ts);
        if (collide) {
            if (axis == 0) addBox(cLo, r[0], cHi, r[1], r[2], r[3]);
            else addBox(r[0], cLo, r[1], cHi, r[2], r[3]);
        }
    }
}
// Jambs / lintel / sill of a hole cut through depth [cLo,cHi].
void reveals(int axis, double cLo, double cHi, const Hole& h, double wallY0, int t, const V3& col) {
    if (axis == 1) { // wall plane z=c, w along x
        vrect(0, h.s0, +1, cLo, cHi, h.y0, h.y1, t, col);
        vrect(0, h.s1, -1, cLo, cHi, h.y0, h.y1, t, col);
        hrect(h.s0, cLo, h.s1, cHi, h.y1, false, t, col);
        if (h.y0 > wallY0 + 1e-3) hrect(h.s0, cLo, h.s1, cHi, h.y0, true, t, col);
    } else { // wall plane x=c, w along z
        vrect(1, h.s0, +1, cLo, cHi, h.y0, h.y1, t, col);
        vrect(1, h.s1, -1, cLo, cHi, h.y0, h.y1, t, col);
        hrect(cLo, h.s0, cHi, h.s1, h.y1, false, t, col);
        if (h.y0 > wallY0 + 1e-3) hrect(cLo, h.s0, cHi, h.s1, h.y0, true, t, col);
    }
}
// Two-sided partition (thickness th) with doors.
void partition(int axis, double c, double w0, double w1, double y0, double y1, const std::vector<Hole>& holes, int tNeg, const V3& cNeg, int tPos, const V3& cPos, double th = .1) {
    wallHoles(axis, c + th / 2, +1, w0, w1, y0, y1, holes, tPos, cPos, true, c - th / 2, c + th / 2);
    wallHoles(axis, c - th / 2, -1, w0, w1, y0, y1, holes, tNeg, cNeg);
    for (auto& h : holes) {
        Hole k{std::max(w0, h.s0), std::min(w1, h.s1), std::max(y0, h.y0), std::min(y1, h.y1)};
        if (k.s1 > k.s0 && k.y1 > k.y0) reveals(axis, c - th / 2, c + th / 2, k, y0, TL::WOOD, {.95, .9, .85});
    }
}
void boxC(double x0, double y0, double z0, double x1, double y1, double z1, int t, const V3& col, bool collide = true, const Opt& o = Opt()) {
    B.box(std::min(x0, x1), y0, std::min(z0, z1), std::max(x0, x1), y1, std::max(z0, z1), t, col, TS(1.2), o);
    if (collide) addBox(x0, z0, x1, z1, y0, y1);
}
// Building-local box a lamp's light is confined to (the room it hangs in).
struct Room { double x0, z0, x1, z1, y0, y1; };
Room room(double xa, double za, double xb, double zb, double y0, double y1) {
    return {std::min(xa, xb), std::min(za, zb), std::max(xa, xb), std::max(za, zb), y0, y1};
}
struct LampRef { int light; size_t v0, v1; };
LampRef lamp(double x, double y, double z, const Em& e, const Room& rm, double r = 5.5, double inten = .9) {
    Opt o;
    o.em = e;
    size_t v0 = B.V.size();
    B.box(x - .16, y - .12, z - .16, x + .16, y, z + .16, TL::LAMP, {1, 1, 1}, 99, o);
    size_t v1 = B.V.size();
    Light L;
    L.p = B.tp({x, y - .2, z});
    L.d = {0, 0, 0};
    // Bright enough to read as a lit room through the glass at night (they only light interiors).
    L.c = e.c(); L.r = r * 1.3; L.i = inten * 2.1; L.thr = e.thr; L.cone = -2; L.fl = e.fl; L.pri = 2;
    // Grow the box a little so the room's own wall / floor / ceiling faces are inside it,
    // but not the faces of the rooms behind 0.1 m partitions or 0.2 m slabs.
    const double g = .08;
    L.room = true;
    L.lo = {1e9, rm.y0 - g, 1e9};
    L.hi = {-1e9, rm.y1 + g, -1e9};
    for (double cx : {rm.x0 - g, rm.x1 + g})
        for (double cz : {rm.z0 - g, rm.z1 + g}) {
            V3 w = B.tp({cx, 0, cz});
            L.lo[0] = std::min(L.lo[0], w[0]); L.hi[0] = std::max(L.hi[0], w[0]);
            L.lo[2] = std::min(L.lo[2], w[2]); L.hi[2] = std::max(L.hi[2], w[2]);
        }
    I->lights.push_back(L);
    return {(int)I->lights.size() - 1, v0, v1};
}
// A window for the transparent pass: glass texels (alpha < 0.9) become real glass.
void glassQuad(const V3& a, const V3& b, const V3& c, const V3& d, int t, const V3& col) {
    std::swap(B.V, I->glassV);
    B.quad(a, b, c, d, t, col);
    std::swap(B.V, I->glassV);
}
void addObj(sim::Obj k, double x0, double y0, double z0, double x1, double y1, double z1, int flat, int floor, int room) {
    I->objs.push_back({k, {std::min(x0, x1), std::min(y0, y1), std::min(z0, z1)}, {std::max(x0, x1), std::max(y0, y1), std::max(z0, z1)}, flat, floor, room});
}
int addRoom(int flat, int floor) {
    I->rooms.push_back({flat, floor, {}, {}});
    return (int)I->rooms.size() - 1;
}
// home lamps outrank everything for a slot in the light grid, so switching one on always shows
void roomLamp(int room, bool home, const LampRef& L) {
    if (home) I->lights[L.light].pri = 4;
    I->rooms[room].lights.push_back(L.light);
    I->rooms[room].lampVerts.push_back({L.v0, L.v1});
}
const V3 HOME_LAMP{1, .86, .64}; // the player's own bulbs; off (threshold 99) until switched on
void floorRect(double x0, double z0, double x1, double z1, double h, int t, const V3& col, bool walk = true) {
    hrect(std::min(x0, x1), std::min(z0, z1), std::max(x0, x1), std::max(z0, z1), h, true, t, col);
    if (walk) I->floors.push_back({std::min(x0, x1), std::min(z0, z1), std::max(x0, x1), std::max(z0, z1), -1, h, h});
}
void ceilRect(double x0, double z0, double x1, double z1, double h, int t, const V3& col) {
    hrect(std::min(x0, x1), std::min(z0, z1), std::max(x0, x1), std::max(z0, z1), h, false, t, col);
}
void cutQuad(const V3& a, const V3& b, const V3& c, const V3& d, const V3& nLocal) {
    V3 p[4] = {B.tp(a), B.tp(b), B.tp(c), B.tp(d)};
    Cutout k;
    k.lo = {1e9, 1e9, 1e9}; k.hi = {-1e9, -1e9, -1e9};
    for (auto& q : p) for (int i = 0; i < 3; i++) { k.lo[i] = std::min(k.lo[i], q[i] - .03); k.hi[i] = std::max(k.hi[i], q[i] + .03); }
    k.n = B.td(nLocal);
    I->cuts.push_back(k);
}

const V3 WHITE3{1, 1, 1};
const std::vector<int> PAPERS{TL::WPAPER1, TL::WPAPER2, TL::WPAPER3};
const std::vector<V3> FABRICS{{.62, .32, .28}, {.35, .45, .6}, {.5, .58, .4}, {.72, .62, .4}, {.55, .42, .52}};

/* ================= APARTMENT BLOCK ================= */
struct Frame {
    double hw, hd;
    int side;
    double X(double a) const { return side == 0 ? -hw + a : hw - a; }
    double Z(double e) const { return side == 0 ? hd - e : -hd + e; }
    int sa() const { return side == 0 ? 1 : -1; }
    int se() const { return side == 0 ? -1 : 1; }
};

void genBlock(const BuildingRec& R, Rng& rng) {
    const double W = R.w, D = R.d, hw = W / 2, hd = D / 2, pl = R.plinth, fh = R.fh, BH = R.H;
    const int F = R.floors, NB = R.bays;
    const double bw = W / NB, Ls = D * .5, lan0 = Ls - 1.6, hl1 = WALL_T + 1.4, top = BH - .2;
    auto Y = [&](int k) { return pl + k * fh; };
    auto ceilAt = [&](int k) { return k < F - 1 ? Y(k + 1) - .2 : top; };
    // Half landing between floor k and k+1. The first one roofs the entrance lobby, so it is
    // raised where needed to keep >= 2.25 m of headroom under its 0.2 m slab.
    auto halfH = [&](int k) { double h = Y(k) + fh / 2; return k == 0 ? std::max(h, 2.45) : h; };
    Frame fr{hw, hd, R.entrSide};
    std::set<int> stairs(R.entr.begin(), R.entr.end());
    bool school = R.type == "school", univer = R.type == "univermag";

    // ---- F-frame (a along the entrance facade, e inward) wrappers ----
    auto fFloor = [&](double a0, double a1, double e0, double e1, double h, int t, const V3& col) { floorRect(fr.X(a0), fr.Z(e0), fr.X(a1), fr.Z(e1), h, t, col); };
    auto fCeil = [&](double a0, double a1, double e0, double e1, double h, int t, const V3& col) { ceilRect(fr.X(a0), fr.Z(e0), fr.X(a1), fr.Z(e1), h, t, col); };
    auto toHolesA = [&](const std::vector<Hole>& hs) { // holes given in e -> local z
        std::vector<Hole> o;
        for (auto h : hs) { double z0 = fr.Z(h.s0), z1 = fr.Z(h.s1); o.push_back({std::min(z0, z1), std::max(z0, z1), h.y0, h.y1}); }
        return o;
    };
    auto toHolesE = [&](const std::vector<Hole>& hs) { // holes given in a -> local x
        std::vector<Hole> o;
        for (auto h : hs) { double x0 = fr.X(h.s0), x1 = fr.X(h.s1); o.push_back({std::min(x0, x1), std::max(x0, x1), h.y0, h.y1}); }
        return o;
    };
    // partition on plane a=c (faces: tNeg toward -a, tPos toward +a)
    auto fPartA = [&](double c, double e0, double e1, double y0, double y1, const std::vector<Hole>& hs, int tNegA, const V3& cNegA, int tPosA, const V3& cPosA) {
        double z0 = fr.Z(e0), z1 = fr.Z(e1);
        if (fr.sa() > 0) partition(0, fr.X(c), std::min(z0, z1), std::max(z0, z1), y0, y1, toHolesA(hs), tNegA, cNegA, tPosA, cPosA);
        else partition(0, fr.X(c), std::min(z0, z1), std::max(z0, z1), y0, y1, toHolesA(hs), tPosA, cPosA, tNegA, cNegA);
    };
    auto fPartE = [&](double c, double a0, double a1, double y0, double y1, const std::vector<Hole>& hs, int tNegE, const V3& cNegE, int tPosE, const V3& cPosE) {
        double x0 = fr.X(a0), x1 = fr.X(a1);
        if (fr.se() > 0) partition(1, fr.Z(c), std::min(x0, x1), std::max(x0, x1), y0, y1, toHolesE(hs), tNegE, cNegE, tPosE, cPosE);
        else partition(1, fr.Z(c), std::min(x0, x1), std::max(x0, x1), y0, y1, toHolesE(hs), tPosE, cPosE, tNegE, cNegE);
    };
    auto fBox = [&](double a0, double a1, double e0, double e1, double y0, double y1, int t, const V3& col, bool collide = true) {
        boxC(fr.X(a0), y0, fr.Z(e0), fr.X(a1), y1, fr.Z(e1), t, col, collide);
    };
    auto fP = [&](double a, double y, double e) { return V3{fr.X(a), y, fr.Z(e)}; };
    auto fRoom = [&](double a0, double a1, double e0, double e1, double y0, double y1) { return room(fr.X(a0), fr.Z(e0), fr.X(a1), fr.Z(e1), y0, y1); };
    // usable objects go to the flat / floor / room being built
    int curFlat = -1, curFloor = -1, curRoom = -1;
    auto fObj = [&](sim::Obj kd, double a0, double a1, double e0, double e1, double y0, double y1) {
        addObj(kd, fr.X(a0), y0, fr.Z(e0), fr.X(a1), y1, fr.Z(e1), curFlat, curFloor, curRoom);
    };
    // a switch plate on a wall (e0..e1 is its few-cm thickness) that works the room's lamps
    auto lightSwitch = [&](double a0, double a1, double e0, double e1, double y) {
        fBox(a0, a1, e0, e1, y + 1.25, y + 1.37, TL::PAINT, {.93, .92, .88}, false);
        fObj(sim::Obj::LightSwitch, a0 - .04, a1 + .04, e0 - .04, e1 + .04, y + 1.2, y + 1.42);
    };
    V3 dirA{(double)fr.sa(), 0, 0};

    // ---- zones: which flat owns each bay's front (e<Ls) and back (e>Ls) part ----
    struct Flat { int b0, b1, stair; };
    std::vector<Flat> flats;
    std::vector<int> zoneF(NB, -1), zoneB(NB, -1);
    for (int b = 0; b < NB;) {
        if (stairs.count(b)) { b++; continue; }
        int r0 = b;
        while (b < NB && !stairs.count(b)) b++;
        int r1 = b - 1;
        bool L = stairs.count(r0 - 1), Rt = stairs.count(r1 + 1);
        if (L && Rt) {
            int n = r1 - r0 + 1, nl = (n + 1) / 2;
            flats.push_back({r0, r0 + nl - 1, r0 - 1});
            if (n - nl > 0) flats.push_back({r0 + nl, r1, r1 + 1});
        } else flats.push_back({r0, r1, L ? r0 - 1 : r1 + 1});
    }
    for (size_t i = 0; i < flats.size(); i++) for (int b = flats[i].b0; b <= flats[i].b1; b++) zoneF[b] = zoneB[b] = (int)i;
    int nFlat = (int)flats.size();
    for (int s : stairs) zoneB[s] = nFlat++; // back flats behind each stairwell
    std::map<std::pair<int, int>, std::pair<int, V3>> paper;
    auto flatPaper = [&](int zone, int k) {
        auto key = std::make_pair(zone, k);
        auto it = paper.find(key);
        if (it != paper.end()) return it->second;
        double h = H_hash(zone, k);
        int t = school || univer ? TL::WHITEW : PAPERS[(int)(h * 3) % 3];
        V3 c = school ? V3{.8, .9, .85} : univer ? V3{.95, .92, .85} : V3{.9 + h * .1, .9 + H_hash(k, zone) * .1, .88};
        return paper[key] = {t, c};
    };
    const V3 POD{1, 1, 1};

    // ---- lining of the four outer walls (window / door holes from the actual facade cells) ----
    struct SideDef { V3 o, u, n; double len; };
    const SideDef SD[4] = {{{-hw, 0, hd}, {1, 0, 0}, {0, 0, 1}, W}, {{hw, 0, hd}, {0, 0, -1}, {1, 0, 0}, D}, {{hw, 0, -hd}, {-1, 0, 0}, {0, 0, -1}, W}, {{-hw, 0, -hd}, {0, 0, 1}, {-1, 0, 0}, D}};
    auto zoneAtLocal = [&](double x, double z, bool& isStair) {
        double a = fr.side == 0 ? x + hw : hw - x, e = fr.side == 0 ? hd - z : z + hd;
        int b = std::min(NB - 1, std::max(0, (int)std::floor(a / bw)));
        isStair = stairs.count(b) && e < Ls;
        return e < Ls ? zoneF[b] : zoneB[b];
    };
    for (int si = 0; si < 4; si++) {
        const SideDef& sd = SD[si];
        int nb = si % 2 == 0 ? NB : R.endBays;
        double bws = sd.len / nb;
        int axis = si % 2 == 0 ? 1 : 0; // lining plane normal axis
        int sign = -(int)(sd.n[0] + sd.n[2]); // inward
        double cIn = axis == 1 ? sd.o[2] - sd.n[2] * WALL_T : sd.o[0] - sd.n[0] * WALL_T;
        double cOut = axis == 1 ? sd.o[2] : sd.o[0];
        double cLo = std::min(cIn, cOut), cHi = std::max(cIn, cOut);
        auto W_ = [&](double a) { return axis == 1 ? sd.o[0] + sd.u[0] * a : sd.o[2] + sd.u[2] * a; };
        for (int j = 0; j < nb; j++) {
            double a0 = j * bws, a1 = a0 + bws;
            double la0 = std::max(a0, WALL_T), la1 = std::min(a1, sd.len - WALL_T);
            if (la1 <= la0) continue;
            double w0 = std::min(W_(la0), W_(la1)), w1 = std::max(W_(la0), W_(la1));
            for (int f = -1; f < F; f++) {
                double y0, y1;
                int tile = -1;
                const FacadeCell* cell = nullptr;
                if (f >= 0) { cell = &R.cells[si][j * F + f]; y0 = cell->y0; y1 = std::min(cell->y1, top); tile = cell->tile; }
                else { y0 = 0; y1 = pl; if (R.cells[si][j * F].y0 < pl - 1e-6) continue; } // plinth band (not under the door cell)
                if (y1 <= y0) continue;
                std::vector<Hole> hs;
                int px[4];
                if (tile >= 0 && tileHole(tile, px)) {
                    double hy0 = y0 + px[1] / 32.0 * (cell->y1 - cell->y0), hy1 = y0 + (px[3] + 1) / 32.0 * (cell->y1 - cell->y0);
                    bool door = tile == TL::DOOR_P || tile == TL::DOOR_B || tile == TL::DOOR_S || tile == TL::ARCH;
                    if (door) hy1 = std::min(hy1, halfH(0) - .2);
                    double ha0 = W_(a0 + px[0] / 32.0 * bws), ha1 = W_(a0 + (px[2] + 1) / 32.0 * bws);
                    Hole h{std::min(ha0, ha1), std::max(ha0, ha1), hy0, std::min(hy1, y1)};
                    hs.push_back(h);
                    if (!door) {
                        if (axis == 1) I->windows.push_back({h.s0, cLo, h.s1, cHi, h.y0, h.y1});
                        else I->windows.push_back({cLo, h.s0, cHi, h.s1, h.y0, h.y1});
                        // swap the facade's painted window for the same cell drawn as real glass
                        V3 P0 = add3(sd.o, sd.u, a0), P1 = add3(sd.o, sd.u, a1);
                        V3 q0{P0[0], cell->y0, P0[2]}, q1{P1[0], cell->y0, P1[2]}, q2{P1[0], cell->y1, P1[2]}, q3{P0[0], cell->y1, P0[2]};
                        cutQuad(q0, q1, q2, q3, sd.n);
                        glassQuad(q0, q1, q2, q3, cell->tile, cell->col);
                    }
                    if (door && si == R.entrSide) { // open the entrance: hide the exterior door quad, re-add the frame around the hole
                        V3 P0 = add3(sd.o, sd.u, a0), P1 = add3(sd.o, sd.u, a1);
                        cutQuad({P0[0], cell->y0, P0[2]}, {P1[0], cell->y0, P1[2]}, {P1[0], cell->y1, P1[2]}, {P0[0], cell->y1, P0[2]}, sd.n);
                        double fa0 = a0 + px[0] / 32.0 * bws, fa1 = a0 + (px[2] + 1) / 32.0 * bws, cy0 = cell->y0, cy1 = cell->y1, ch = cy1 - cy0;
                        auto pP = [&](double a, double y) { V3 q = add3(sd.o, sd.u, a); return V3{q[0], y, q[2]}; };
                        auto piece = [&](double pa0, double pa1, double py0, double py1) {
                            if (pa1 - pa0 < 1e-4 || py1 - py0 < 1e-4) return;
                            B.quadUV(pP(pa0, py0), pP(pa1, py0), pP(pa1, py1), pP(pa0, py1), cell->tile, cell->col, cell->e,
                                     (pa0 - a0) / bws, (py0 - cy0) / ch, (pa1 - a0) / bws, (py1 - cy0) / ch);
                        };
                        piece(a0, fa0, cy0, cy1); piece(fa1, a1, cy0, cy1); piece(fa0, fa1, h.y1, cy1);
                    }
                }
                bool st;
                double cx = axis == 1 ? (w0 + w1) / 2 : cIn, cz = axis == 1 ? cIn : (w0 + w1) / 2;
                int zone = zoneAtLocal(cx, cz, st);
                auto pp = flatPaper(zone, std::max(0, f));
                int t = st ? TL::PODYEZD : pp.first;
                V3 col = st ? POD : pp.second;
                wallHoles(axis, cIn, sign, w0, w1, y0, y1, hs, t, col, true, cLo, cHi);
                for (auto& h : hs) reveals(axis, cLo, cHi, h, y0, TL::WHITEW, {.95, .95, .93});
                for (auto& h : hs)
                    if (h.y0 > y0 + .3) { // window sill board
                        double sx0 = axis == 1 ? h.s0 - .05 : cIn - (sign > 0 ? 0 : .18), sx1 = axis == 1 ? h.s1 + .05 : cIn + (sign > 0 ? .18 : 0);
                        double sz0 = axis == 1 ? cIn - (sign > 0 ? 0 : .18) : h.s0 - .05, sz1 = axis == 1 ? cIn + (sign > 0 ? .18 : 0) : h.s1 + .05;
                        B.box(std::min(sx0, sx1), h.y0 - .04, std::min(sz0, sz1), std::max(sx0, sx1), h.y0, std::max(sz0, sz1), TL::PAINT, {.95, .95, .95}, 99);
                    }
            }
        }
    }

    // ---- stairwells ----
    auto steps = [&](double aL, double aR, double eLo, double hLo, double eHi, double hHi) { // flight ascending from eLo to eHi
        double rise = hHi - hLo;
        int n = std::max(2, (int)jsround(std::fabs(rise) / .165));
        double de = (eHi - eLo) / n, dh = rise / n;
        int dirSign = eHi > eLo ? 1 : -1;
        for (int i = 0; i < n; i++) {
            double ea = eLo + de * i, eb = eLo + de * (i + 1), h0 = hLo + dh * i, h1 = h0 + dh;
            fFloor(aL, aR, std::min(ea, eb), std::max(ea, eb), h1, TL::STEP, {1, 1, 1});
            I->floors.pop_back();
            // riser at ea facing the low end
            double x0 = fr.X(aL), x1 = fr.X(aR), z = fr.Z(ea);
            int s = -dirSign * fr.se();
            vrect(1, z, s, std::min(x0, x1), std::max(x0, x1), h0, h1, TL::CONC, {.75, .75, .72});
        }
        // soffit
        quadFacing(fP(aL, hLo - .22, eLo), fP(aR, hLo - .22, eLo), fP(aR, hHi - .22, eHi), fP(aL, hHi - .22, eHi), {0, -1, 0}, TL::WHITEW, {.9, .9, .88});
        // walkable ramp
        double z0 = fr.Z(eLo), z1 = fr.Z(eHi), x0 = fr.X(aL), x1 = fr.X(aR);
        IFloor fl{std::min(x0, x1), std::min(z0, z1), std::max(x0, x1), std::max(z0, z1), 1, z0 < z1 ? hLo : hHi, z0 < z1 ? hHi : hLo};
        I->floors.push_back(fl);
    };
    auto railing = [&](double a, double eLo, double hLo, double eHi, double hHi) {
        const double rh = .95;
        V3 p0 = fP(a, hLo, eLo), p1 = fP(a, hHi, eHi), p2 = fP(a, hHi + rh, eHi), p3 = fP(a, hLo + rh, eLo);
        B.dquad(p0, p1, p2, p3, TL::RAIL, {.35, .35, .38});
        B.beam(p3, p2, .07, TL::WOOD, {.7, .45, .3});
    };
    for (int j : stairs) {
        double a0 = j * bw, a1 = a0 + bw, am = (a0 + a1) / 2;
        const V3 FL{.72, .64, .56};
        // lobby, landings, half landings
        fFloor(a0, a1, WALL_T, hl1, 0, TL::MARBLE, FL);
        for (int k = 0; k < F; k++) {
            fFloor(a0, a1, lan0, Ls, Y(k), TL::MARBLE, FL);
            if (k > 0) fCeil(a0, a1, lan0, Ls, Y(k) - .2, TL::WHITEW, WHITE3);
            // slab edge facing the flights
            { double x0 = fr.X(a0), x1 = fr.X(a1); vrect(1, fr.Z(lan0), -fr.se(), std::min(x0, x1), std::max(x0, x1), Y(k) - .2, Y(k), TL::CONC, {.8, .8, .78}); }
            if (k < F - 1) {
                double hh = halfH(k);
                fFloor(a0, a1, WALL_T, hl1, hh, TL::MARBLE, FL);
                fCeil(a0, a1, WALL_T, hl1, hh - .2, TL::WHITEW, WHITE3);
                double x0 = fr.X(a0), x1 = fr.X(a1);
                vrect(1, fr.Z(hl1), fr.se(), std::min(x0, x1), std::max(x0, x1), hh - .2, hh, TL::CONC, {.8, .8, .78});
                steps(a0, am - .05, lan0, Y(k), hl1, hh);          // flight A (left lane) up to the half landing
                steps(am + .05, a1, hl1, hh, lan0, Y(k + 1));      // flight B (right lane) up to the next floor
                railing(am - .06, lan0, Y(k), hl1, hh);
                railing(am + .06, hl1, hh, lan0, Y(k + 1));
            }
        }
        fCeil(a0, a1, WALL_T, Ls, top, TL::WHITEW, WHITE3);
        // ground flight from the lobby up to the first landing, solid block under flight A0
        steps(am + .05, a1, hl1, 0, lan0, Y(0));
        railing(am + .06, hl1, 0, lan0, Y(0));
        {
            double hh = halfH(0);
            quadFacing(fP(am, 0, hl1), fP(am, 0, lan0), fP(am, Y(0), lan0), fP(am, hh, hl1), dirA, TL::PODYEZD, POD);
            double x0 = fr.X(a0), x1 = fr.X(am);
            vrect(1, fr.Z(hl1), -fr.se(), std::min(x0, x1), std::max(x0, x1), 0, hh - .2, TL::PODYEZD, POD);
            addBox(fr.X(a0), fr.Z(hl1), fr.X(am), fr.Z(lan0), -1, Y(0) - .02);
        }
        addBox(fr.X(am - .06), fr.Z(hl1), fr.X(am + .06), fr.Z(lan0), -1, BH + 5); // railing collision
        // lamps: lobby from the door cell, landings from the stair windows
        for (int k = 0; k < F; k++) {
            const FacadeCell& c = R.cells[R.entrSide][j * F + k];
            Em e = c.e ? c.e : em({.62, .85, .72}, .3);
            double cy = k < F - 1 ? Y(k + 1) - .2 : top;
            Room shaft = fRoom(a0, a1, WALL_T, Ls, -1, BH + 1);
            lamp(fr.X(am), cy - .02, fr.Z((lan0 + Ls) / 2), e, shaft, 5.5, .85);
            if (k == 0) lamp(fr.X(am), halfH(0) - .22, fr.Z((WALL_T + hl1) / 2), e, shaft, 4.5, .8);
        }
        // back wall (stair | back flat), doors per floor
        for (int k = 0; k < F; k++) {
            double y0 = k == 0 ? 0 : Y(k), y1 = ceilAt(k);
            std::vector<Hole> hs{{am - .45, am + .45, Y(k), Y(k) + DOOR_H}};
            auto bp = flatPaper(zoneB[j], k);
            fPartE(Ls, a0, a1, y0, y1, hs, TL::PODYEZD, POD, bp.first, bp.second);
            // door leaf swung into the back flat
            double ha = am - .45 + .04;
            B.dquad(fP(ha, Y(k), Ls + .06), fP(ha, Y(k), Ls + .96), fP(ha, Y(k) + 2.02, Ls + .96), fP(ha, Y(k) + 2.02, Ls + .06), TL::APTDOOR, {1, 1, 1});
            addBox(fr.X(ha - .03), fr.Z(Ls + .06), fr.X(ha + .03), fr.Z(Ls + .96), Y(k), Y(k) + 2.02);
        }
    }

    // ---- bay boundary walls (stair sides, flat-to-flat walls) ----
    for (int b = 0; b + 1 < NB; b++) {
        double c = (b + 1) * bw;
        bool sL = stairs.count(b), sR = stairs.count(b + 1);
        for (int k = 0; k < F; k++) {
            double y0 = k == 0 ? 0 : Y(k), y1 = ceilAt(k);
            // front part (e in [WALL_T, Ls])
            if (zoneF[b] != zoneF[b + 1] || sL != sR) {
                std::vector<Hole> hs;
                int tL, tR;
                V3 cL, cR;
                if (sL) { tL = TL::PODYEZD; cL = POD; } else { auto p = flatPaper(zoneF[b], k); tL = p.first; cL = p.second; }
                if (sR) { tR = TL::PODYEZD; cR = POD; } else { auto p = flatPaper(zoneF[b + 1], k); tR = p.first; cR = p.second; }
                if (sL != sR) {
                    hs.push_back({Ls - 1.2, Ls - .25, Y(k), Y(k) + DOOR_H});
                    // flat door leaf swung into the flat's hallway
                    double side = sL ? 1 : -1, he = Ls - 1.2 + .04;
                    B.dquad(fP(c + side * .06, Y(k), he), fP(c + side * .96, Y(k), he), fP(c + side * .96, Y(k) + 2.02, he), fP(c + side * .06, Y(k) + 2.02, he), TL::APTDOOR, {1, 1, 1});
                    addBox(fr.X(c + side * .06), fr.Z(he - .03), fr.X(c + side * .96), fr.Z(he + .03), Y(k), Y(k) + 2.02);
                }
                fPartA(c, WALL_T, Ls, y0, y1, hs, tL, cL, tR, cR);
            }
            if (zoneB[b] != zoneB[b + 1]) {
                auto pL = flatPaper(zoneB[b], k), pR = flatPaper(zoneB[b + 1], k);
                fPartA(c, Ls, D - WALL_T, k == 0 ? Y(0) : y0, y1, {}, pL.first, pL.second, pR.first, pR.second);
            }
        }
    }

    // ---- flats: floors, hallway walls, rooms, furniture, lamps ----
    // A room's lamp follows its windows: it takes the setting of whichever window lights up
    // earliest, so the room is lit whenever any of its windows glows.
    auto facadeEm = [&](bool front, double ra0, double ra1, int k) {
        int si = front ? R.entrSide : (R.entrSide + 2) % 4;
        Em best;
        for (int j = 0; j < NB; j++) {
            double c0 = j * bw, c1 = c0 + bw;
            double fa0 = front ? c0 : W - c1, fa1 = front ? c1 : W - c0; // cell range in F-frame a
            if (fa1 > ra0 + .1 && fa0 < ra1 - .1) {
                const FacadeCell& c = R.cells[si][j * F + k];
                if (c.e && (!best || c.e.thr < best.thr)) best = c.e;
            }
        }
        return best;
    };
    bool bedFound = false;
    V3 bedSpot{}, bedLook{};
    auto furnish = [&](double ra0, double ra1, double eF, double eH, int k, int kind) {
        // eF: facade-side inner face, eH: hallway-side wall. sgn: +1 if eH > eF
        double y = Y(k), sg = eH > eF ? 1 : -1, depth = std::fabs(eH - eF);
        auto E = [&](double d) { return eF + sg * d; }; // distance from facade wall
        // Each layout is written against the ra0 side wall; `mir` flips it to the ra1 side.
        bool mir = false;
        auto MA = [&](double a) { return mir ? ra0 + ra1 - a : a; };
        auto box = [&](double aa0, double aa1, double d0, double d1, double h0, double h1, int t, const V3& c, bool col = true) {
            fBox(MA(aa0), MA(aa1), E(d0), E(d1), y + h0, y + h1, t, c, col);
        };
        auto obj = [&](sim::Obj kd, double aa0, double aa1, double d0, double d1, double h0, double h1) {
            fObj(kd, MA(aa0), MA(aa1), E(d0), E(d1), y + h0, y + h1);
        };
        // would this box stand in front of a window?
        auto clear = [&](double aa0, double aa1, double d0, double d1, double h0, double h1) {
            V3 p = fP(MA(aa0), y + h0, E(d0)), q = fP(MA(aa1), y + h1, E(d1));
            V3 lo{std::min(p[0], q[0]), p[1], std::min(p[2], q[2])}, hi{std::max(p[0], q[0]), q[1], std::max(p[2], q[2])};
            for (auto& win : I->windows)
                if (frontOfWindow(lo, hi, win)) return false;
            return true;
        };
        // try a layout as written, then mirrored; leaves `mir` set to the one that fits
        auto place = [&](const std::function<bool()>& fits) {
            for (bool m : {false, true}) { mir = m; if (fits()) return true; }
            mir = false;
            return false;
        };
        V3 fab = rng.pick(FABRICS), wood{.72 + rng() * .15, .52 + rng() * .1, .38};
        double w = ra1 - ra0, mid = (ra0 + ra1) / 2;
        const double sa = fr.sa();
        if (kind == 0) { // bedroom: bed and wall carpet on one side wall, wardrobe on the other or by the hallway
            double bl = std::min(2.0, depth - 1.0), cl = std::min(1.8, bl);
            bool carpet = place([&] { return clear(ra0 + .03, ra0 + .05, .3, .3 + cl, .6, 2.1); });
            box(ra0 + .06, ra0 + 1.0, .25, .25 + bl, 0, .45, TL::FABRIC, fab);
            box(ra0 + .1, ra0 + .96, .3, .7, .45, .6, TL::FABRIC, {.95, .95, .95}, false);
            obj(sim::Obj::Bed, ra0 + .06, ra0 + 1.0, .25, .25 + bl, 0, .6);
            bedFound = true;
            bedSpot = fP(MA(ra0 + 1.45), y, E(.25 + bl * .5));
            bedLook = {-sa * (mir ? -1 : 1), 0, 0};
            if (carpet) {
                double cx = fr.X(MA(ra0 + .03)), z0 = fr.Z(E(.3)), z1 = fr.Z(E(.3 + cl));
                vrect(0, cx, (int)(sa * (mir ? -1 : 1)), std::min(z0, z1), std::max(z0, z1), y + .6, y + 2.1, TL::CARPET, {1, 1, 1}, 2.0);
            }
            if (w > 2.2) {
                double d0 = std::max(.3, depth - 1.5), d1 = depth - .15;
                if (clear(ra1 - .65, ra1 - .05, d0, d1, 0, 2.0)) {
                    box(ra1 - .65, ra1 - .05, d0, d1, 0, 2.0, TL::WOOD, wood);
                    obj(sim::Obj::Wardrobe, ra1 - .65, ra1 - .05, d0, d1, 0, 2.0);
                } else if (w > 3.6 && clear(ra1 - 1.95, ra1 - .75, depth - .65, depth - .05, 0, 2.0)) { // against the hallway wall instead
                    box(ra1 - 1.95, ra1 - .75, depth - .65, depth - .05, 0, 2.0, TL::WOOD, wood);
                    obj(sim::Obj::Wardrobe, ra1 - 1.95, ra1 - .75, depth - .65, depth - .05, 0, 2.0);
                }
            }
        } else if (kind == 1) { // living room: wall unit with the TV on one side, sofa on the other
            double sl = std::min(2.0, depth - 1.4), ul = std::min(2.8, depth - 1.3);
            bool unit = place([&] { return clear(ra0 + .05, ra0 + .62, .3, .3 + ul, 0, 2.1); });
            if (unit) {
                box(ra0 + .05, ra0 + .55, .3, .3 + ul, 0, 2.1, TL::WOOD, {.45, .3, .2});
                box(ra0 + .56, ra0 + .6, .3 + ul * .3, .3 + ul * .3 + .7, .8, 1.3, TL::PAINT, {.08, .08, .1}, false); // TV niche screen
                obj(sim::Obj::TV, ra0 + .3, ra0 + .62, .3 + ul * .3, .3 + ul * .3 + .7, .8, 1.3);
            }
            box(ra1 - .85, ra1 - .05, .6, .6 + sl, 0, .45, TL::FABRIC, fab);
            box(ra1 - .25, ra1 - .05, .6, .6 + sl, .45, .9, TL::FABRIC, fab, false);
            obj(sim::Obj::Sofa, ra1 - .85, ra1 - .05, .6, .6 + sl, 0, .9);
            if (w > 2.4) {
                box(mid - .4, mid + .4, depth * .45 - .5, depth * .45 + .5, 0, .74, TL::WOOD, wood);
                obj(sim::Obj::Table, mid - .4, mid + .4, depth * .45 - .5, depth * .45 + .5, 0, .8);
            }
            double x0 = fr.X(mid - .9), x1 = fr.X(mid + .9), z0 = fr.Z(E(depth * .45 - 1.1)), z1 = fr.Z(E(depth * .45 + 1.1));
            hrect(std::min(x0, x1), std::min(z0, z1), std::max(x0, x1), std::max(z0, z1), y + .02, true, TL::CARPET, {1, 1, 1}, 2.2);
        } else if (kind == 2) { // kitchen: stove, fridge, sink and wall cupboard in one run, table by the window
            // The run stands along a side wall, clear of the window reveal; failing that, along the hallway wall.
            const bool sink = depth > 3.2;
            auto runSide = [&] {
                return clear(ra0 + .05, ra0 + .65, .7, 1.3, 0, .95) && clear(ra0 + .05, ra0 + .7, 1.4, 2.05, 0, 1.7) &&
                       (!sink || clear(ra0 + .05, ra0 + .65, 2.15, 2.75, 0, 2.1));
            };
            // hallway-wall run: positions along a, from the ra0 corner toward the door
            const bool hallFits = mid - .5 > ra0 + (sink ? 2.1 : 1.4);
            auto runHall = [&] {
                return hallFits && clear(ra0 + .05, ra0 + .65, depth - .65, depth - .05, 0, .95) &&
                       clear(ra0 + .75, ra0 + 1.4, depth - .65, depth - .05, 0, 1.7) && (!sink || clear(ra0 + 1.5, ra0 + 2.1, depth - .65, depth - .05, 0, 2.1));
            };
            struct Spot { double a0, a1, d0, d1; };
            Spot st, fg, sk;
            bool ok = true;
            if (place(runSide)) { st = {ra0 + .05, ra0 + .65, .7, 1.3}; fg = {ra0 + .05, ra0 + .7, 1.4, 2.05}; sk = {ra0 + .05, ra0 + .65, 2.15, 2.75}; }
            else if (place(runHall)) { st = {ra0 + .05, ra0 + .65, depth - .65, depth - .05}; fg = {ra0 + .75, ra0 + 1.4, depth - .65, depth - .05}; sk = {ra0 + 1.5, ra0 + 2.1, depth - .65, depth - .05}; }
            else ok = false;
            if (ok) {
                box(st.a0, st.a1, st.d0, st.d1, 0, .85, TL::PAINT, {.95, .95, .93});
                box(st.a0, st.a1, st.d0, st.d1, .85, .88, TL::METAL, {.25, .25, .27}, false);
                obj(sim::Obj::Stove, st.a0, st.a1, st.d0, st.d1, 0, .95);
                box(fg.a0, fg.a1, fg.d0, fg.d1, 0, 1.7, TL::PAINT, {.93, .93, .9});
                obj(sim::Obj::Fridge, fg.a0, fg.a1, fg.d0, fg.d1, 0, 1.7);
                if (sink) { // sink unit with an enamel basin, and a wall cupboard above it
                    box(sk.a0, sk.a1, sk.d0, sk.d1, 0, .85, TL::PAINT, {.95, .95, .93});
                    box(sk.a0 + .05, sk.a1 - .05, sk.d0 + .05, sk.d1 - .05, .85, .88, TL::METAL, {.62, .64, .66}, false);
                    obj(sim::Obj::KitchenSink, sk.a0, sk.a1, sk.d0, sk.d1, 0, 1.0);
                    bool alongSide = st.d0 < 1; // the cupboard hangs on whichever wall the run is against
                    double ca0 = sk.a0, ca1 = alongSide ? sk.a0 + .35 : sk.a1, cd0 = alongSide ? sk.d0 : sk.d1 - .35, cd1 = sk.d1;
                    box(ca0, ca1, cd0, cd1, 1.45, 2.1, TL::PAINT, {.9, .88, .8}, false);
                    obj(sim::Obj::Cupboard, ca0, ca1, cd0, cd1, 1.45, 2.1);
                }
            }
            // the table and stool go on the other side, by the window
            box(ra1 - 1.0, ra1 - .1, .5, 1.3, 0, .74, TL::WOOD, wood);
            obj(sim::Obj::Table, ra1 - 1.0, ra1 - .1, .5, 1.3, 0, .8);
            box(ra1 - 1.3, ra1 - 1.0, .7, 1.0, 0, .45, TL::WOOD, wood);
            // wired radio ("radiopunkt") on the hallway wall, where there is never a window
            if (w > 2.2 && place([&] { return clear(ra1 - .57, ra1 - .23, depth - .2, depth, 1.45, 1.77); })) {
                box(ra1 - .55, ra1 - .25, depth - .08, depth, 1.5, 1.72, TL::PAINT, {.55, .38, .26}, false);
                box(ra1 - .51, ra1 - .29, depth - .09, depth - .08, 1.54, 1.68, TL::PLANK, {.85, .8, .7}, false);
                obj(sim::Obj::Radio, ra1 - .57, ra1 - .23, depth - .2, depth, 1.45, 1.77);
            }
        } else if (kind == 3) { // shop: shelving along one side wall, counter along the other
            bool shelves = place([&] { return clear(ra0 + .05, ra0 + .6, .5, depth - 1.2, 0, 2.0); });
            if (shelves) {
                box(ra0 + .05, ra0 + .5, .5, depth - 1.2, 0, 2.0, TL::WOOD, {.6, .45, .3});
                for (double d = .7; d < depth - 1.4; d += .5)
                    for (double h : {.5, 1.1, 1.6}) box(ra0 + .52, ra0 + .6, d, d + .3, h, h + .25, TL::PAINT, rng.pick(FABRICS), false);
            }
            box(ra1 - 1.0, ra1 - .4, .6, depth - 1.2, 0, 1.0, TL::WOOD, {.55, .42, .3});
            box(ra1 - 1.02, ra1 - .38, .58, depth - 1.18, 1.0, 1.05, TL::METAL, {.5, .5, .52}, false);
        } else if (kind == 4) { // classroom: blackboard on a side wall without windows
            double b1 = std::min(depth - .6, 3.6);
            if (place([&] { return clear(ra0 + .03, ra0 + .05, .6, b1, .9, 2.2); })) {
                double cx = fr.X(MA(ra0 + .03)), z0 = fr.Z(E(.6)), z1 = fr.Z(E(b1));
                vrect(0, cx, (int)(sa * (mir ? -1 : 1)), std::min(z0, z1), std::max(z0, z1), y + .9, y + 2.2, TL::BOARD, {1, 1, 1}, 3.0);
            }
            mir = false;
            for (double aa = ra0 + 1.6; aa + .7 < ra1 - .3; aa += 1.3)
                for (double d = .5; d + 1.2 < depth - .8; d += 1.6) {
                    box(aa, aa + .6, d, d + 1.2, .7, .75, TL::WOOD, {.75, .6, .4}, false);
                    box(aa + .25, aa + .35, d + .1, d + 1.1, 0, .7, TL::METAL, {.3, .3, .32});
                }
        } else { // storage
            for (int i = 0; i < 5; i++) {
                double aa = ra0 + .2 + rng() * std::max(.1, w - 1.2), d = .3 + rng() * std::max(.1, depth - 1.8), s = .5 + rng() * .4;
                box(aa, aa + s, d, d + s, 0, s, TL::WOOD, {.75, .6, .42});
            }
        }
    };
    const int flatsPerFloor = nFlat;
    for (size_t fi = 0; fi < flats.size(); fi++) {
        const Flat& fl = flats[fi];
        double aS = fl.b0 == 0 ? WALL_T : fl.b0 * bw, aE = fl.b1 == NB - 1 ? W - WALL_T : (fl.b1 + 1) * bw;
        // room groups of two bays, counted outward from the stairwell
        std::vector<int> bays;
        if (fl.stair < fl.b0) for (int b = fl.b0; b <= fl.b1; b++) bays.push_back(b);
        else for (int b = fl.b1; b >= fl.b0; b--) bays.push_back(b);
        std::vector<std::pair<double, double>> groups;
        for (size_t i = 0; i < bays.size(); i += 2) {
            int bA = bays[i], bB = i + 1 < bays.size() ? bays[i + 1] : bays[i];
            if (i + 3 == bays.size()) { bB = bays[i + 2]; i++; } // fold a trailing single bay into the last room
            double g0 = std::min(bA, bB) * bw, g1 = (std::max(bA, bB) + 1) * bw;
            groups.push_back({std::max(g0, aS), std::min(g1, aE)});
        }
        // the bathroom takes the far end of the hallway, away from the stairwell
        const double BATH_W = 2.2;
        const int dir = fl.stair < fl.b0 ? 1 : -1;                   // +1: the far end is aE
        const double far = dir > 0 ? aE : aS, cB = far - dir * BATH_W; // cB: plane of the bathroom wall
        const double bLo = std::min(cB, far), bHi = std::max(cB, far);
        const bool farIsEnd = dir > 0 ? fl.b1 == NB - 1 : fl.b0 == 0;  // far wall is the building's end wall
        bool bathOK = aE - aS >= 4.8 && !school && !univer;
        for (auto& g : groups) {
            double m = (g.first + g.second) / 2;
            if (m + .45 > bLo - .3 && m - .45 < bHi + .3) bathOK = false; // a room door would open into it
        }
        // the flat's front door, on the stairwell side
        const double aDoor = dir > 0 ? aS : aE;

        for (int k = 0; k < F; k++) {
            double y = Y(k), yc = ceilAt(k);
            const bool home = (int)fi == I->homeFlat && k == I->homeFloor;
            curFlat = (int)fi; curFloor = k;
            bedFound = false;
            bool kitchen = false;
            fFloor(aS, aE, WALL_T, Ls - 1.4, y, TL::PARQUET, {1, 1, 1});
            if (bathOK) { // lino in the hallway, tiles in the bathroom (no overlay in one plane)
                fFloor(dir > 0 ? aS : cB, dir > 0 ? cB : aE, Ls - 1.4, Ls, y, TL::LINO, {1, 1, 1});
                fFloor(bLo, bHi, Ls - 1.4, Ls, y, TL::FLOORTILE, {1, 1, 1});
            } else fFloor(aS, aE, Ls - 1.4, Ls, y, TL::LINO, {1, 1, 1});
            fFloor(aS, aE, Ls, D - WALL_T, y, TL::PARQUET, {1, 1, 1});
            fCeil(aS, aE, WALL_T, D - WALL_T, yc, TL::WHITEW, WHITE3);
            auto pp = flatPaper((int)fi, k);
            std::vector<Hole> hf, hb;
            for (auto& g : groups) {
                double m = (g.first + g.second) / 2;
                hf.push_back({m - .45, m + .45, y, y + DOOR_H});
                hb.push_back({m - .45, m + .45, y, y + DOOR_H});
            }
            fPartE(Ls - 1.4, aS, aE, y, yc, hf, pp.first, pp.second, pp.first, pp.second);
            fPartE(Ls, aS, aE, y, yc, hb, pp.first, pp.second, pp.first, pp.second);
            for (size_t g = 0; g + 1 < groups.size(); g++) {
                double c = fl.stair < fl.b0 ? groups[g].second : groups[g].first;
                fPartA(c, WALL_T, Ls - 1.4, y, yc, {}, pp.first, pp.second, pp.first, pp.second);
                fPartA(c, Ls, D - WALL_T, y, yc, {}, pp.first, pp.second, pp.first, pp.second);
            }
            // rooms
            for (size_t g = 0; g < groups.size(); g++) {
                double r0 = groups[g].first + .05, r1 = groups[g].second - .05, m = (groups[g].first + groups[g].second) / 2;
                for (int fb = 0; fb < 2; fb++) {
                    bool front = fb == 0;
                    double eF = front ? WALL_T : D - WALL_T, eH = front ? Ls - 1.4 - .05 : Ls + .05;
                    int kind;
                    bool shopCell = false;
                    {
                        int si = front ? R.entrSide : (R.entrSide + 2) % 4;
                        for (int j = 0; j < NB; j++) {
                            double c0 = j * bw, c1 = c0 + bw, fa0 = front ? c0 : W - c1, fa1 = front ? c1 : W - c0;
                            if (fa1 > r0 + .1 && fa0 < r1 - .1 && R.cells[si][j * F + k].tile == TL::SHOP) shopCell = true;
                        }
                    }
                    if (shopCell) kind = 3;
                    else if (school) kind = 4;
                    else if (univer) kind = 5;
                    else if (g == 0 && front) kind = 2;
                    else kind = (int)((g + fb + k) % 2);
                    if (kind == 2) kitchen = true;
                    curRoom = addRoom((int)fi, k);
                    furnish(r0, r1, eF + (front ? .02 : -.02), eH, k, kind);
                    Em le = facadeEm(front, r0, r1, k);
                    if (home && !le) le = em(HOME_LAMP, 99);
                    if (le) roomLamp(curRoom, home, lamp(fr.X((r0 + r1) / 2), yc - .02, fr.Z((eF + eH) / 2), le, fRoom(r0, r1, eF, eH, y, yc), 5.5, .95));
                    // switch on the room side of the door
                    double se = front ? eH - .02 : eH;
                    if (kind <= 2) lightSwitch(m + .55, m + .65, se, se + .02, y);
                }
            }
            // hallway: light over the lino, switch by the front door
            curRoom = addRoom((int)fi, k);
            {
                double h0 = bathOK && dir < 0 ? cB : aS, h1 = bathOK && dir > 0 ? cB : aE;
                Em he = facadeEm(true, aS, aE, k);
                if (home && !he) he = em(HOME_LAMP, 99);
                if (he) roomLamp(curRoom, home, lamp(fr.X((h0 + h1) / 2), yc - .02, fr.Z(Ls - .7), he, fRoom(h0, h1, Ls - 1.4, Ls, y, yc), 4, .7));
                double sa = aDoor + dir * .35;
                lightSwitch(std::min(sa, sa + dir * .1), std::max(sa, sa + dir * .1), Ls - .07, Ls - .05, y);
            }
            // bathroom (combined WC): tiles to 1.6 m, oil paint above, bath across the far end
            if (bathOK) {
                curRoom = addRoom((int)fi, k);
                std::vector<Hole> door{{Ls - 1.2, Ls - .45, y, y + DOOR_H}};
                fPartA(cB, Ls - 1.4, Ls, y, yc, door, pp.first, pp.second, pp.first, pp.second);
                const double off = farIsEnd ? 0 : .05, g = .02; // tile panels stand 2 cm off the walls
                const V3 WHITE_T{1, 1, 1}, OILPAINT{.72, .84, .8};
                auto band = [&](auto&& wall) { wall(y, y + 1.6, TL::BATHTILE, WHITE_T, .8); wall(y + 1.6, yc, TL::PAINT, OILPAINT, 1.2); };
                auto eWall = [&](double e, bool plusE) { // wall plane e, facing +e / -e
                    band([&](double y0, double y1, int t, const V3& c, double ts) {
                        double x0 = fr.X(bLo), x1 = fr.X(bHi);
                        vrect(1, fr.Z(e), plusE ? fr.se() : -fr.se(), std::min(x0, x1), std::max(x0, x1), y0, y1, t, c, ts);
                    });
                };
                auto aWall = [&](double a, int facing, const std::vector<Hole>& hs) { // wall plane a, facing +a (1) / -a (-1)
                    band([&](double y0, double y1, int t, const V3& c, double ts) {
                        double z0 = fr.Z(Ls - 1.4), z1 = fr.Z(Ls);
                        wallHoles(0, fr.X(a), facing * fr.sa(), std::min(z0, z1), std::max(z0, z1), y0, y1, toHolesA(hs), t, c, false, 0, 0, ts);
                    });
                };
                eWall(Ls - 1.35 + g, true);
                eWall(Ls - .05 - g, false);
                aWall(cB + dir * (.05 + g), dir, door);
                aWall(far - dir * (off + g), -dir, {});
                auto A = [&](double d) { return far - dir * d; }; // distance in from the far wall
                const V3 ENAMEL{.96, .96, .94};
                fBox(A(.72), A(.03 + off), Ls - 1.32, Ls - .08, y, y + .55, TL::PAINT, ENAMEL);
                fBox(A(.66), A(.09 + off), Ls - 1.26, Ls - .14, y + .5, y + .56, TL::PAINT, {.62, .78, .86}, false);
                fObj(sim::Obj::Bath, A(.72), A(off), Ls - 1.35, Ls - .05, y, y + .7);
                fBox(A(1.2), A(.82), Ls - .72, Ls - .08, y, y + .42, TL::PAINT, ENAMEL);
                fBox(A(1.18), A(.84), Ls - .25, Ls - .08, y + .42, y + .85, TL::PAINT, ENAMEL, false);
                fObj(sim::Obj::Toilet, A(1.22), A(.8), Ls - .75, Ls - .05, y, y + .9);
                fBox(A(1.28), A(.8), Ls - 1.32, Ls - .9, y + .72, y + .86, TL::PAINT, ENAMEL, false);
                fBox(A(1.1), A(.98), Ls - 1.3, Ls - 1.18, y, y + .72, TL::PAINT, ENAMEL, false);
                fBox(A(1.25), A(.83), Ls - 1.325, Ls - 1.31, y + 1.2, y + 1.7, TL::METAL, {.8, .86, .9}, false); // mirror, in front of the tiles
                fObj(sim::Obj::BathSink, A(1.3), A(.78), Ls - 1.35, Ls - .88, y + .55, y + 1.0);
                Em be = facadeEm(true, aS, aE, k);
                if (home && !be) be = em(HOME_LAMP, 99);
                if (be) roomLamp(curRoom, home, lamp(fr.X(A(1.1)), yc - .02, fr.Z(Ls - .7), be, fRoom(bLo, bHi, Ls - 1.4, Ls, y, yc), 3.5, .6));
                // its switch is outside, in the hallway, as they always are
                double sa = cB - dir * .06;
                lightSwitch(std::min(sa, sa - dir * .02), std::max(sa, sa - dir * .02), Ls - .38, Ls - .28, y);
            }
            if (bathOK && kitchen && bedFound)
                I->homes.push_back({(int)fi, k, k * flatsPerFloor + (int)fi + 1, bedSpot, bedLook});
        }
    }
    // back flats behind stairwells
    for (int j : stairs) {
        double a0 = j * bw + (j == 0 ? WALL_T : 0), a1 = (j + 1) * bw - (j == NB - 1 ? WALL_T : 0);
        for (int k = 0; k < F; k++) {
            double y = Y(k), yc = ceilAt(k);
            curFlat = zoneB[j]; curFloor = k;
            curRoom = addRoom(curFlat, k);
            fFloor(a0, a1, Ls, D - WALL_T, y, TL::PARQUET, {1, 1, 1});
            fCeil(a0, a1, Ls, D - WALL_T, yc, TL::WHITEW, WHITE3);
            furnish(a0 + .05, a1 - .05, D - WALL_T - .02, Ls + 1.0, k, school ? 4 : univer ? 5 : 0);
            Em le = facadeEm(false, a0, a1, k);
            if (le) roomLamp(curRoom, false, lamp(fr.X((a0 + a1) / 2), yc - .02, fr.Z((Ls + D) / 2), le, fRoom(a0, a1, Ls, D - WALL_T, y, yc), 5, .9));
        }
    }
}

/* ================= CHURCH ================= */
void genChurchInt(const BuildingRec& R, Rng& rng) {
    const V3 WC{.96, .95, .92}, FL{.9, .85, .8};
    const Room ROOM{-5.4, -11.2, 5.4, 12.4, 0, 20};
    // vestibule (inside the bell tower) and nave
    floorRect(-2.4, 7.2, 2.4, 12.0, 0, TL::MARBLE, FL);
    ceilRect(-2.4, 7.2, 2.4, 12.0, 4.6, TL::WHITEW, WC);
    floorRect(-4.8, -6.8, 4.8, 6.8, .8, TL::MARBLE, FL);
    ceilRect(-4.8, -6.8, 4.8, 6.8, 8.8, TL::STARS, {1, 1, 1});
    // ramp from the vestibule up into the nave (through the wall)
    floorRect(-1, 6.8, 1, 7.2, .8, TL::MARBLE, FL);
    hrect(-1.2, 7.2, 1.2, 8.8, .4, true, TL::STEP, {1, 1, 1}, 1.6);
    I->floors.push_back({-1.2, 7.2, 1.2, 8.8, 1, .8, 0});
    // nave lining with windows (front middle cell faces the tower: doorway instead)
    const double wy0 = .8 + 5 / 32.0 * 8.2, wy1 = .8 + 25 / 32.0 * 8.2;
    auto winHoles = [&](double from, double len, int n, double skipLo, double skipHi) {
        std::vector<Hole> hs;
        for (int i = 0; i < n; i++) {
            double c = from + len * (i + .5) / n, hw = len / n * 4 / 32.0;
            if (c > skipLo && c < skipHi) continue;
            hs.push_back({c - hw, c + hw, wy0, wy1});
        }
        return hs;
    };
    for (int sx : {1, -1}) {
        auto hs = winHoles(-7, 14, 4, 99, 99);
        double c = sx * 4.8;
        wallHoles(0, c, -sx, -6.8, 6.8, .8, 8.8, hs, TL::WHITEW, WC, true, std::min(c, sx * 5.0), std::max(c, sx * 5.0));
        for (auto& h : hs) {
            reveals(0, std::min(c, sx * 5.0), std::max(c, sx * 5.0), h, .8, TL::WHITEW, WC);
            double x = sx * 4.9; // a pane in the middle of the reveal
            glassQuad({x, h.y0, h.s0}, {x, h.y0, h.s1}, {x, h.y1, h.s1}, {x, h.y1, h.s0}, TL::GLASS, {1, 1, 1});
        }
    }
    {
        auto hs = winHoles(-5, 10, 3, -1, 1);
        for (auto& h : hs) glassQuad({h.s0, h.y0, 6.9}, {h.s1, h.y0, 6.9}, {h.s1, h.y1, 6.9}, {h.s0, h.y1, 6.9}, TL::GLASS, {1, 1, 1});
        hs.push_back({-1, 1, .8, 3.2});
        wallHoles(1, 6.8, -1, -4.8, 4.8, .8, 8.8, hs, TL::WHITEW, WC, true, 6.8, 7.0);
        for (auto& h : hs) reveals(1, 6.8, 7.2, h, .8, TL::WHITEW, WC);
        wallHoles(1, -6.8, 1, -4.8, 4.8, .8, 8.8, {}, TL::WHITEW, WC, true, -7.0, -6.8);
    }
    // vestibule lining + open door on the tower front
    {
        std::vector<Hole> door{{-1.1375, 1.1375, 0, 3.75}};
        wallHoles(1, 12.0, -1, -2.4, 2.4, 0, 4.6, door, TL::WHITEW, WC, true, 12.0, 12.2);
        reveals(1, 12.0, 12.2, door[0], 0, TL::WHITEW, WC);
        wallHoles(1, 7.2, 1, -2.4, 2.4, 0, 4.6, {{-1, 1, .8, 3.2}}, TL::WHITEW, WC, true, 7.0, 7.2);
        for (int sx : {1, -1}) { double c = sx * 2.4; wallHoles(0, c, -sx, 7.2, 12.0, 0, 4.6, {}, TL::WHITEW, WC, true, std::min(c, sx * 2.6), std::max(c, sx * 2.6)); }
        cutQuad({-2.6, 0, 12.2}, {2.6, 0, 12.2}, {2.6, 4.8, 12.2}, {-2.6, 4.8, 12.2}, {0, 0, 1});
        Em de = em({1, .75, .4}, .1);
        auto piece = [&](double x0, double x1, double y0, double y1) {
            B.quadUV({x0, y0, 12.2}, {x1, y0, 12.2}, {x1, y1, 12.2}, {x0, y1, 12.2}, TL::DOOR_S, R.wc, de, (x0 + 2.6) / 5.2, y0 / 4.8, (x1 + 2.6) / 5.2, y1 / 4.8);
        };
        piece(-2.6, -1.1375, 0, 4.8); piece(1.1375, 2.6, 0, 4.8); piece(-1.1375, 1.1375, 3.75, 4.8);
        cutQuad({-5 + 10 / 3.0, .8, 7}, {-5 + 20 / 3.0, .8, 7}, {-5 + 20 / 3.0, 9, 7}, {-5 + 10 / 3.0, 9, 7}, {0, 0, 1});
        addBox(-4, -11.2, 4, -7, -1, 20); // apse shell
    }
    // iconostasis
    vrect(1, -6.7, 1, -4.6, 4.6, .8, 6.2, TL::IKONO, {1, 1, 1}, 1.6);
    B.box(-4.7, .8, -6.8, 4.7, 1.1, -6.55, TL::GOLD, {1, 1, 1}, 99);
    for (int tier = 0; tier < 2; tier++)
        for (int i = -3; i <= 3; i++) {
            if (i == 0 && tier == 0) continue;
            double x = i * 1.25, y0 = tier == 0 ? 1.9 : 4.0;
            vrect(1, -6.68, 1, x - .45, x + .45, y0, y0 + 1.35, TL::ICON, {1, 1, 1}, 1.4);
        }
    vrect(1, -6.68, 1, -.65, .65, .8, 3.3, TL::GOLD, {1, 1, 1}, 1.3); // royal doors
    addBox(-4.8, -6.8, 4.8, -6.4, -1, 10);
    // candle stands, chandelier
    for (int i = 0; i < 4; i++) {
        double x = (i % 2 ? 1 : -1) * (1.8 + rng() * 1.8), z = -5.6 + (i / 2) * 3.2 + rng() * .8;
        boxC(x - .05, .8, z - .05, x + .05, 1.8, z + .05, TL::GOLD, {1, 1, 1}, false);
        B.box(x - .3, 1.8, z - .3, x + .3, 1.86, z + .3, TL::GOLD, {1, 1, 1}, 99);
        Em ce = em({1, .72, .35}, 0);
        ce.fl = .3 + rng();
        lamp(x, 2.0, z, ce, ROOM, 4.5, .7);
        addBox(x - .3, z - .3, x + .3, z + .3, -1, 3);
    }
    B.box(-.04, 6.2, -.04, .04, 8.8, .04, TL::METAL, {.3, .3, .3}, 99);
    for (int i = 0; i < 12; i++) {
        double a = i * M_PI / 6, x = std::cos(a) * 1.6, z = std::sin(a) * 1.6;
        B.box(x - .07, 6.1, z - .07, x + .07, 6.3, z + .07, TL::GOLD, {1, 1, 1}, 99);
    }
    lamp(0, 6.1, 0, em({1, .8, .5}, .15), ROOM, 9, 1.0);
}

/* ================= IZBA ================= */
void genIzbaInt(const BuildingRec& R, Rng& rng) {
    const double hw = R.ihw, hd = R.ihd, fb = R.ifb, IH = R.iH, pz = R.ipz, t = .15, yc = IH - .1;
    const V3 LC{.95, .88, .8};
    const Room ROOM{-hw, -hd, hw, hd, 0, IH + 1};
    floorRect(-hw + t, -hd + t, hw - t, hd - t, fb, TL::WOOD, {.9, .85, .8});
    ceilRect(-hw + t, -hd + t, hw - t, hd - t, yc, TL::PLANK, {.85, .8, .72});
    double w = hw * 2, d = hd * 2, cw = w / 3, cl = d / 3;
    auto hy = [&](double f) { return fb + f * (IH - fb); };
    std::vector<Hole> front;
    for (int i = 0; i < 3; i++) front.push_back({-hw + i * cw + 11 / 32.0 * cw, -hw + i * cw + 21 / 32.0 * cw, hy(8 / 32.0), hy(22 / 32.0)});
    wallHoles(1, hd - t, -1, -hw + t, hw - t, fb, yc, front, TL::LOG, LC, true, hd - t, hd);
    for (auto& h : front) reveals(1, hd - t, hd, h, fb, TL::WOOD, {1, 1, 1});
    wallHoles(1, -hd + t, 1, -hw + t, hw - t, fb, yc, {}, TL::LOG, LC, true, -hd, -hd + t);
    for (int sx : {1, -1}) {
        std::vector<Hole> hs;
        double za = sx > 0 ? hd - cl : -hd + cl;
        if (sx > 0) hs.push_back({za - 21 / 32.0 * cl, za - 11 / 32.0 * cl, hy(8 / 32.0), hy(22 / 32.0)});
        else hs.push_back({za + 11 / 32.0 * cl, za + 21 / 32.0 * cl, hy(8 / 32.0), hy(22 / 32.0)});
        Hole win = hs[0];
        if (sx > 0) hs.push_back({pz - .6, pz + .6, fb, fb + 2});
        double c = sx * (hw - t);
        wallHoles(0, c, -sx, -hd + t, hd - t, fb, yc, hs, TL::LOG, LC, true, std::min(c, sx * hw), std::max(c, sx * hw));
        for (auto& h : hs) reveals(0, std::min(c, sx * hw), std::max(c, sx * hw), h, fb, TL::WOOD, {1, 1, 1});
        (void)win;
    }
    // open the door: hide the exterior door quad and the side-wall thirds behind it, then
    // re-add those thirds with the doorway cut out (as genBlock does for entrance cells)
    cutQuad({hw + .05, fb, pz + .6}, {hw + .05, fb, pz - .6}, {hw + .05, fb + 2, pz - .6}, {hw + .05, fb + 2, pz + .6}, {1, 0, 0});
    {
        const double dz0 = pz - .6, dz1 = pz + .6, dy1 = fb + 2, vh = IH - fb;
        for (int i = 0; i < 3; i++) {
            const double za = hd - i * cl, zb = za - cl; // texture u runs from za to zb
            if (dz1 <= zb || dz0 >= za) continue;
            const int tile = i == 1 ? R.izTile : TL::LOG;
            const Em e = i == 1 ? R.izWin[3] : Em();
            cutQuad({hw, fb, za}, {hw, fb, zb}, {hw, IH, zb}, {hw, IH, za}, {1, 0, 0});
            auto piece = [&](double zA, double zB, double y0, double y1) {
                if (zA - zB < 1e-4 || y1 - y0 < 1e-4) return;
                B.quadUV({hw, y0, zA}, {hw, y0, zB}, {hw, y1, zB}, {hw, y1, zA}, tile, R.wc, e,
                         (za - zA) / cl, (y0 - fb) / vh, (za - zB) / cl, (y1 - fb) / vh);
            };
            piece(za, std::max(zb, dz1), fb, IH);                          // beside the door, +z
            piece(std::min(za, dz0), zb, fb, IH);                          // beside the door, -z
            piece(std::min(za, dz1), std::max(zb, dz0), dy1, IH);          // above the door
        }
    }
    // windows: the painted window thirds of the log walls become real glass
    {
        auto swapWin = [&](const V3& a, const V3& b, const V3& c, const V3& d, const V3& n) {
            cutQuad(a, b, c, d, n);
            glassQuad(a, b, c, d, R.izTile, R.wc);
        };
        for (int i = 0; i < 3; i++)
            swapWin({-hw + i * cw, fb, hd}, {-hw + (i + 1) * cw, fb, hd}, {-hw + (i + 1) * cw, IH, hd}, {-hw + i * cw, IH, hd}, {0, 0, 1});
        for (int sx : {1, -1}) {
            double za = sx > 0 ? hd - cl : -hd + cl, zb = sx > 0 ? za - cl : za + cl;
            if (sx > 0 && pz + .6 > std::min(za, zb) && pz - .6 < std::max(za, zb)) continue; // rebuilt around the door above
            swapWin({sx * hw, fb, za}, {sx * hw, fb, zb}, {sx * hw, IH, zb}, {sx * hw, IH, za}, {(double)sx, 0, 0});
        }
    }
    // porch (walkable) and its posts
    I->floors.push_back({hw, pz - 1.2, hw + 1.6, pz + 1.2, -1, fb, fb});
    addBox(hw + 1.4, pz - 1.1, hw + 1.55, pz - .95, -1, 5);
    addBox(hw + 1.4, pz + .95, hw + 1.55, pz + 1.1, -1, 5);
    // stove in the far corner, red corner diagonally opposite
    double sx0 = -hw + t, sz0 = -hd + t;
    boxC(sx0, fb, sz0, sx0 + 1.8, fb + 1.9, sz0 + 2.0, TL::STOVE, {1, 1, 1});
    boxC(sx0 + .3, fb + 1.9, sz0 + .3, sx0 + .9, yc, sz0 + .9, TL::STOVE, {1, 1, 1});
    vrect(0, sx0 + 1.81, 1, sz0 + .7, sz0 + 1.3, fb + .5, fb + 1.0, TL::PAINT, {.1, .08, .07}, 1.0);
    B.box(sx0, fb + 1.9, sz0 + 2.0, sx0 + 1.8, fb + 1.95, hd - t - .1 > sz0 + 3.6 ? sz0 + 3.6 : hd - t - .1, TL::PLANK, {.8, .72, .6}, 99); // polati
    double rx = hw - t, rz = hd - t;
    vrect(1, rz - .02, -1, rx - 1.2, rx - .4, fb + 1.6, fb + 2.4, TL::ICON, {1, 1, 1}, .8);
    Em lp = em({1, .25, .15}, 0);
    lp.fl = .5 + rng();
    lamp(rx - .8, fb + 1.55, rz - .25, lp, ROOM, 3, .6);
    boxC(rx - 1.9, fb, rz - 1.7, rx - .9, fb + .76, rz - .8, TL::WOOD, {.8, .62, .45});
    boxC(rx - 2.6, fb, rz - .45, rx - .05, fb + .45, rz - .05, TL::WOOD, {.7, .55, .4});
    boxC(rx - .45, fb, rz - 2.8, rx - .05, fb + .45, rz - .5, TL::WOOD, {.7, .55, .4});
    boxC(-hw + t + .1, fb, hd - t - 2.2, -hw + t + 1.0, fb + .5, hd - t - .1, TL::FABRIC, rng.pick(FABRICS));
    for (int i = 0; i < 5; i++)
        if (R.izWin[i]) { lamp(0, yc - .02, 0, R.izWin[i], ROOM, 5, .9); break; }
}

} // namespace

Interior buildInterior(int ri, double seed, int homeFlat, int homeFloor) {
    Interior out;
    const BuildingRec& R = RECS[ri];
    out.rec = ri; out.ox = R.ox; out.oz = R.oz; out.cs = R.cs; out.sn = R.sn;
    out.homeFlat = homeFlat; out.homeFloor = homeFloor;
    I = &out;
    B.reset();
    B.ox = R.ox; B.oz = R.oz; B.cs = R.cs; B.sn = R.sn;
    B.snowK = 0; // no snow on indoor floors
    Rng rng(seed * 7 + ri * 131 + 17);
    if (R.kind == 0) { genBlock(R, rng); out.lx0 = -R.w / 2; out.lx1 = R.w / 2; out.lz0 = -R.d / 2; out.lz1 = R.d / 2; }
    else if (R.kind == 1) { genChurchInt(R, rng); out.lx0 = -5.4; out.lx1 = 5.4; out.lz0 = -11.2; out.lz1 = 12.4; }
    else { genIzbaInt(R, rng); out.lx0 = -R.ihw; out.lx1 = R.ihw; out.lz0 = -R.ihd; out.lz1 = R.ihd; }
    out.V = std::move(B.V);
    B.reset();
    I = nullptr;
    return out;
}

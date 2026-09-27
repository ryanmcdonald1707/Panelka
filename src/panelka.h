// Panelka — PS1 Eastern Bloc generator (raylib C++ port)
#pragma once
#include "sim/objects.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

using V3 = std::array<double, 3>;

/* ================= RNG (bit-exact with the JS version) ================= */
inline uint32_t imul(uint32_t a, uint32_t b) { return a * b; }
inline double jsround(double v) { return std::floor(v + 0.5); }
double H(int32_t x, int32_t y, int32_t s);

struct Rng {
    uint32_t a;
    explicit Rng(double seed);
    double operator()();
    double range(double lo, double hi) { return lo + (*this)() * (hi - lo); }
    int integer(int lo, int hi) { return lo + (int)std::floor((*this)() * (hi - lo + 1)); }
    template <class T> const T& pick(const std::vector<T>& v) { return v[(size_t)std::floor((*this)() * v.size())]; }
    bool chance(double p) { return (*this)() < p; }
};

/* ================= TEXTURE ATLAS ================= */
constexpr int T = 32, AT = 16, AS = 512, GL = 190, LO = 150;
extern uint8_t atlas[AS * AS * 4];
void buildAtlas();

namespace TL {
enum : int {
    PANEL = 0, PANEL_ST = 5, BRICK = 6, BRICK_ST = 11, STUCCO = 12, DOOR_P = 17, DOOR_B = 18, DOOR_S = 19, SHOP = 20, TAR = 21,
    RMET = 22, ASPH = 23, GRASS = 24, DIRT = 25, CONC = 26, RIB = 27, GLZW = 28, GLZB = 29, RAIL = 30, GARAGE = 31, METAL = 32,
    LEAF = 33, LEAF2 = 34, BARK = 35, BIRCH = 36, CORN = 37, WOOD = 38, SAND = 39, LAMP = 40, POOL = 41, PLINTH = 42, CARWIN = 43,
    PAINT = 44, PLINTH_W = 45, ARCH = 46, LOG = 47, PLANK = 50, CHWIN = 51, GOLD = 52, DOMEB = 53, SCHW = 54, TRAM = 55,
    RAILS = 56, COBBLE = 57, BRANCH = 58, SPRUCE = 59, ROADL = 60, GARDEN = 61, STRAW = 62,
    // interior tiles (not part of the original atlas)
    WPAPER1 = 63, WPAPER2 = 64, PARQUET = 65, LINO = 66, PODYEZD = 67, WHITEW = 68, APTDOOR = 69, STEP = 70, CARPET = 71,
    ICON = 72, IKONO = 73, STOVE = 74, BOARD = 75, MARBLE = 76, WPAPER3 = 77, FABRIC = 78, STARS = 79,
    BATHTILE = 80, FLOORTILE = 81
};
extern const std::vector<int> PANEL_W, BRICK_W, STUCCO_W, IZW;
}

/* ================= GEOMETRY BUILDER ================= */
struct Em {
    bool on = false;
    double r = 0, g = 0, b = 0, thr = 0, fl = 0;
    explicit operator bool() const { return on; }
    V3 c() const { return {r, g, b}; }
};
inline Em em(V3 c, double thr) { Em e; e.on = true; e.r = c[0]; e.g = c[1]; e.b = c[2]; e.thr = thr; return e; }

struct TS {
    double u, v;
    TS(double s = 3) : u(s), v(s) {}
    TS(double a, double b) : u(a), v(b) {}
};
struct Opt {
    int top = -1;
    bool hasTopCol = false;
    V3 topCol{};
    std::string skip;
    Em em;
    bool bottom = false;
};

constexpr int FLOATS_PER_VERT = 18; // pos3 nrm3 uv2 col3 emit3 ext4

struct Builder {
    std::vector<float> V;
    double ox = 0, oz = 0, cs = 1, sn = 0;
    bool up = false;
    double fogK = 1, snowK = 1;
    void reset();
    void xfA(double x, double z, double a);
    void xf(double x, double z, double r) { xfA(x, z, r * M_PI / 2); }
    V3 tp(const V3& v) const;
    V3 td(const V3& v) const;
    void quad(V3 a, V3 b, V3 c, V3 d, int t, const V3& col, const Em& e = Em(), int r = 0);
    // quad with a custom sub-rectangle of the tile (fractions u0,v0,u1,v1)
    void quadUV(V3 a, V3 b, V3 c, V3 d, int t, const V3& col, const Em& e, double u0, double v0, double u1, double v1);
    const double* rrOverride = nullptr;
    void dquad(const V3& a, const V3& b, const V3& c, const V3& d, int t, const V3& col, const Em& e = Em());
    void wall(V3 o, V3 u, V3 v, double lu, double lv, int t, const V3& col, TS ts = TS(), const Em& e = Em());
    void wallF(V3 o, V3 u, V3 v, double lu, double lv, const std::function<int()>& t, const V3& col, TS ts,
               const std::function<Em()>& e);
    void obox(V3 o, V3 U, V3 Nn, double lu, double lv, double ln, int t, const V3& col, TS ts = TS(), const Opt& opt = Opt());
    void box(double x0, double y0, double z0, double x1, double y1, double z1, int t, const V3& col, TS ts = TS(), const Opt& opt = Opt());
    void surf(V3 p0, V3 p1, V3 p2, V3 p3, int nu, int nv, int t, const V3& col, const Em& e = Em());
    void beam(V3 p0, V3 p1, double th, int t, const V3& col, const Em& e = Em());
    void lathe(double cx, double cy, double cz, const std::vector<std::array<double, 2>>& prof, int seg, int t, const V3& col,
               const Em& e = Em(), double a0 = 0, double a1 = 2 * M_PI);
    double textW(const std::string& s, double px);
    void text(const std::string& s, V3 o, V3 u, V3 v, double px, const V3& col, const Em& e);
};
extern Builder B;

std::vector<uint32_t> utf8cp(const std::string& s);

/* ================= WORLD ================= */
struct Light {
    V3 p, d, c;
    double r, i, thr, cone, fl;
    int pri;
    // Interior lights only light interior geometry inside their room box (world space);
    // there are no point-light shadows, so this is what keeps them from leaking through walls.
    bool room = false;
    V3 lo{}, hi{};
};
extern std::vector<std::array<double, 4>> COL;
extern std::vector<V3> SMOKE;
extern std::vector<Light> LIGHTS;
extern std::string SEASON;

struct Info {
    int count;
    V3 target;
    double dist;
    std::string label;
    V3 spawn;
    std::string sound;
    double half;
};
extern const std::vector<std::string> TYPES, SOLO;
Info generate(const std::string& mode, const std::string& style, double seed);

/* ================= BUILDING RECORDS (for interiors) ================= */
struct FacadeCell { int tile; V3 col; Em e; double y0, y1; };
struct BuildingRec {
    int kind = 0; // 0 apartment-style block, 1 church, 2 izba
    std::string type, wall;
    double ox = 0, oz = 0, cs = 1, sn = 0;
    int col = -1;               // index into COL
    double w = 0, d = 0, plinth = 0, fh = 0, H = 0;
    int floors = 0, bays = 0, endBays = 0, entrSide = 0;
    std::vector<int> entr;
    std::vector<FacadeCell> cells[4]; // [j*floors+f]
    V3 tint{};
    // church / izba extras
    V3 wc{};                    // church walls / izba logs
    double ihw = 0, ihd = 0, ifb = 0, iH = 0, ipz = 0;
    int izTile = 0;             // izba window tile
    Em izWin[5]; // front 0..2, side +x, side -x
};
extern std::vector<BuildingRec> RECS;

// Usable things out in the street (world-space boxes): bus stops, kiosks.
struct WObj { sim::Obj kind; V3 lo, hi; };
extern std::vector<WObj> WOBJ;

/* ================= INTERIORS ================= */
struct IBox { double x0, z0, x1, z1, y0, y1; };                 // building-local wall / furniture
struct IFloor { double x0, z0, x1, z1; int axis; double h0, h1; }; // axis -1 flat, 0 slope along x, 1 along z
struct Cutout { V3 lo, hi, n; };                                 // world AABB of static quads hidden while inside
// Something usable: a building-local box, which flat / floor it belongs to and which room.
struct IObj {
    sim::Obj kind;
    V3 lo, hi;
    int flat = -1, floor = -1, room = -1;
};
// A room's ceiling lamps: indices into Interior::lights, and the float ranges of their
// glowing boxes in Interior::V (so a light switch can turn them on and off).
struct IRoom {
    int flat = -1, floor = -1;
    std::vector<int> lights;
    std::vector<std::pair<size_t, size_t>> lampVerts;
};
// A flat the player could live in (has a kitchen, a bedroom and a bathroom).
struct IFlat {
    int flat, floor, number;
    V3 bedSide;       // building-local standing spot next to the bed
    V3 bedLook;       // building-local direction to face from there
};
struct Interior {
    int rec = -1;
    double ox = 0, oz = 0, cs = 1, sn = 0;
    double lx0 = 0, lz0 = 0, lx1 = 0, lz1 = 0; // local footprint
    std::vector<float> V;
    std::vector<IBox> walls;
    std::vector<IFloor> floors;
    std::vector<Light> lights;
    std::vector<Cutout> cuts;
    std::vector<IObj> objs;
    std::vector<IRoom> rooms;
    std::vector<IFlat> homes;
    int homeFlat = -1, homeFloor = -1;
    void toLocal(double wx, double wz, double& lx, double& lz) const {
        double dx = wx - ox, dz = wz - oz;
        lx = dx * cs - dz * sn; lz = dx * sn + dz * cs;
    }
};
// homeFlat / homeFloor: the player's flat, whose lamps are always built (and switchable).
Interior buildInterior(int rec, double seed, int homeFlat = -1, int homeFloor = -1);

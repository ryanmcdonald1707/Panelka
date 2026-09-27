// Headless checks over many generated worlds: things Live mode relies on, and
// layout bugs that are easy to miss on screen. Run with `ctest` or ./build/world_test.
#include "panelka.h"

#include <cstdio>
#include <cstdlib>
#include <string>

static int failures = 0, checks = 0;
#define CHECK(cond, ...)                                                             \
    do {                                                                             \
        checks++;                                                                    \
        if (!(cond)) {                                                               \
            failures++;                                                              \
            if (failures <= (getenv("WT_ALL") ? 1000000 : 40)) { std::printf("FAIL %s: ", #cond); std::printf(__VA_ARGS__); std::printf("\n"); } \
        }                                                                            \
    } while (0)

static Info gen(const std::string& mode, const std::string& style, double seed) {
    COL.clear(); LIGHTS.clear(); SMOKE.clear(); B.reset();
    Info in = generate(mode, style, seed);
    B.reset();
    return in;
}

// Is the object standing in front of the window: overlapping it along the wall, within
// `depth` of the wall, and reaching above the sill?
static bool overlaps(const IObj& o, const IBox& w, double depth) {
    bool normalX = w.x1 - w.x0 < w.z1 - w.z0; // the thin axis is the wall's normal
    double gx = normalX ? depth : -.02, gz = normalX ? -.02 : depth;
    return o.lo[0] < w.x1 + gx && o.hi[0] > w.x0 - gx && o.lo[2] < w.z1 + gz && o.hi[2] > w.z0 - gz &&
           o.hi[1] > w.y0 + .15 && o.lo[1] < w.y1;
}

int main(int argc, char** argv) {
    int seeds = argc > 1 ? std::atoi(argv[1]) : 24;
    int interiors = 0, homesTotal = 0;
    for (int seed = 1; seed <= seeds; seed++) {
        gen("district", "mixed", seed);
        int kiosks = 0, stops = 0;
        for (auto& w : WOBJ) { kiosks += w.kind == sim::Obj::Kiosk; stops += w.kind == sim::Obj::BusStop; }
        CHECK(stops == 2, "seed %d: %d bus stops", seed, stops);
        CHECK(kiosks >= 1, "seed %d: no kiosk, nowhere to buy food in Live mode", seed);

        int homes = 0;
        for (size_t ri = 0; ri < RECS.size(); ri++) {
            if (RECS[ri].kind != 0) continue;
            Interior in = buildInterior((int)ri, seed);
            interiors++;
            homes += (int)in.homes.size();
            // every window opening gets glass (6 vertices per quad, one quad per window cell)
            CHECK(in.glassV.size() / FLOATS_PER_VERT / 6 >= in.windows.size(), "seed %d %s: %zu windows but %zu glass quads", seed, RECS[ri].type.c_str(),
                  in.windows.size(), in.glassV.size() / FLOATS_PER_VERT / 6);
            // tall furniture must not stand in front of a window
            for (auto& o : in.objs) {
                if (o.kind == sim::Obj::Bed || o.kind == sim::Obj::Table || o.kind == sim::Obj::Sofa || o.kind == sim::Obj::LightSwitch) continue;
                for (auto& w : in.windows) {
                    if (getenv("WT_BOX") && overlaps(o, w, .6) && RECS[ri].type == getenv("WT_BOX"))
                        std::printf("BOX %s obj [%.2f %.2f %.2f]-[%.2f %.2f %.2f] win [%.2f %.2f %.2f]-[%.2f %.2f %.2f]\n", sim::objName(o.kind), o.lo[0], o.lo[1], o.lo[2], o.hi[0], o.hi[1], o.hi[2], w.x0, w.y0, w.z0, w.x1, w.y1, w.z1);
                    CHECK(!overlaps(o, w, .6), "seed %d %s: %s covers a window (floor %d)", seed, RECS[ri].type.c_str(), sim::objName(o.kind), o.floor);
                }
            }
            // a lamp lights the room it hangs in
            for (auto& l : in.lights) {
                if (!l.room) continue;
                bool inside = l.p[0] >= l.lo[0] && l.p[0] <= l.hi[0] && l.p[1] >= l.lo[1] && l.p[1] <= l.hi[1] && l.p[2] >= l.lo[2] && l.p[2] <= l.hi[2];
                CHECK(inside, "seed %d %s: lamp outside its own room box", seed, RECS[ri].type.c_str());
            }
            // every room lamp belongs to a room record, and every switch to a room
            for (auto& o : in.objs)
                if (o.kind == sim::Obj::LightSwitch) CHECK(o.room >= 0 && o.room < (int)in.rooms.size(), "seed %d: switch without a room", seed);
            for (auto& h : in.homes) {
                int fr = 0, cb = 0, bed = 0, bath = 0;
                for (auto& o : in.objs) {
                    if (o.flat != h.flat || o.floor != h.floor) continue;
                    fr += o.kind == sim::Obj::Fridge; cb += o.kind == sim::Obj::Cupboard;
                    bed += o.kind == sim::Obj::Bed; bath += o.kind == sim::Obj::Bath;
                }
                CHECK(fr == 1 && cb == 1 && bed >= 1 && bath == 1, "seed %d: home flat %d/%d has fridge %d cupboard %d bed %d bath %d", seed, h.flat, h.floor, fr, cb, bed, bath);
            }
        }
        homesTotal += homes;
        CHECK(homes > 0, "seed %d: no flat the player could live in", seed);
    }
    // the other scene kinds still build, with their interiors
    for (const char* mode : {"old", "village"})
        for (int seed = 1; seed <= 4; seed++) {
            gen(mode, "mixed", seed);
            for (size_t ri = 0; ri < RECS.size(); ri++) {
                Interior in = buildInterior((int)ri, seed);
                CHECK(!in.V.empty(), "%s seed %d: empty interior for %s", mode, seed, RECS[ri].type.c_str());
                CHECK(!in.glassV.empty(), "%s seed %d: no window glass in the %s", mode, seed, RECS[ri].type.c_str());
            }
        }
    std::printf("%d seeds, %d interiors, %d candidate homes: %d checks, %d failed\n", seeds, interiors, homesTotal, checks, failures);
    return failures ? 1 : 0;
}

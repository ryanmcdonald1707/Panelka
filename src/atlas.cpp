// Procedural 512x512 texture atlas (32px tiles, 16x16), ported pixel-for-pixel.
#include "panelka.h"

#include <algorithm>

uint8_t atlas[AS * AS * 4];

namespace TL {
const std::vector<int> PANEL_W{1, 2, 3, 4}, BRICK_W{7, 8, 9, 10}, STUCCO_W{13, 14, 15, 16}, IZW{48, 49};
}

double H(int32_t x, int32_t y, int32_t s) {
    uint32_t h = imul((uint32_t)x, 374761393u) + imul((uint32_t)y, 668265263u) + imul((uint32_t)s, 1442695041u);
    h = imul(h ^ (h >> 13), 1274126177u);
    h ^= h >> 16;
    return h / 4294967296.0;
}

Rng::Rng(double seed) {
    // JS: seed*2654435761>>>0  (double multiply, then ToUint32)
    double p = std::trunc(seed * 2654435761.0);
    double m = std::fmod(p, 4294967296.0);
    if (m < 0) m += 4294967296.0;
    a = (uint32_t)m;
}
double Rng::operator()() {
    a = a + 0x6D2B79F5u;
    uint32_t t = imul(a ^ (a >> 15), 1u | a);
    t = (t + imul(t ^ (t >> 7), 61u | t)) ^ t;
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}

namespace {
struct C4 {
    double r, g, b;
    int a = 255;
    C4(double r_, double g_, double b_) : r(r_), g(g_), b(b_) {}
    C4(double r_, double g_, double b_, int a_) : r(r_), g(g_), b(b_), a(a_) {}
    C4 k(double f) const { return {r * f, g * f, b * f}; }
};
const C4 CLEAR(0, 0, 0, 0);
using Wall = C4 (*)(int, int);

int cl(double v) { return v < 0 ? 0 : v > 255 ? 255 : (int)v; }
template <class F> void paint(int id, F fn) {
    int tx = id % AT, ty = id / AT;
    for (int y = 0; y < T; y++)
        for (int x = 0; x < T; x++) {
            C4 c = fn(x, y);
            int i = ((ty * T + y) * AS + tx * T + x) * 4;
            atlas[i] = cl(c.r); atlas[i + 1] = cl(c.g); atlas[i + 2] = cl(c.b); atlas[i + 3] = (uint8_t)c.a;
        }
}
const int BY[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
double sq(double v) { return v * v; }

C4 wPanel(int x, int y) {
    double v = 156 + H(x, y, 1) * 20 + (H(x >> 2, y >> 2, 8) > .82 ? -9 : 0);
    if (H(x, 77, 1) > .9 && y < 26) v -= 7 * (1 - y / 26.0);
    if (x == 0 || y == 0) v = 108;
    else if (x == 1 || y == 1) v += 10;
    return {v, v - 1, v - 6};
}
C4 wBrick(int x, int y) {
    int r = y >> 2, o = (r & 1) * 4;
    if (y % 4 == 0 || (x + o) % 8 == 0) return {148, 144, 138};
    double v = 200 + (H((x + o) >> 3, r, 2) - .5) * 34 + H(x, y, 2) * 10;
    return {v, v * .95, v * .9};
}
C4 wStucco(int x, int y) {
    double v = 226 + H(x, y, 3) * 14 + (H(x >> 3, y >> 3, 3) > .75 ? -8 : 0);
    return {v, v - 2, v - 6};
}
C4 wPlinth(int x, int y) {
    if (y % 8 == 0) return {86, 84, 80};
    if (((x + ((y >> 3) & 1) * 8) % 16) == 0) return {92, 90, 86};
    double v = 120 + H(x, y, 71) * 14;
    return {v, v - 2, v - 6};
}
C4 win(Wall wall, int x, int y, const int* r, int v, int s) {
    int x0 = r[0], y0 = r[1], x1 = r[2], y1 = r[3];
    if (y == y0 - 1 && x >= x0 - 1 && x <= x1 + 1) return {212, 210, 204};
    if (y == y0 - 2 && x >= x0 - 1 && x <= x1 + 1) return wall(x, y).k(.7);
    if (x < x0 || x > x1 || y < y0 || y > y1) {
        C4 c = wall(x, y);
        if (x >= x0 && x <= x1 && y < y0 - 2 && H(x, 5, s) > .7) return c.k(.88);
        return c;
    }
    static const C4 FR[4] = {{224, 222, 212}, {224, 222, 212}, {240, 240, 238}, {116, 76, 46}};
    C4 fr = FR[v];
    int ft = v == 2 ? 2 : 1;
    if (x - x0 < ft || x1 - x < ft || y - y0 < ft || y1 - y < ft) return (v == 0 && H(x, y, s + 3) > .8) ? C4(168, 166, 158) : fr;
    int mx = (x0 + x1) >> 1, ty = y1 - 6;
    if (x == mx || (v != 2 && y == ty)) return fr;
    double t = (double)(y - y0) / (y1 - y0);
    double g0 = 50 + t * 22, g1 = 62 + t * 24, g2 = 80 + t * 26;
    if (((x - y + 64) % 12) < 2) { g0 += 30; g1 += 32; g2 += 32; }
    if (v == 1 && (x < x0 + 4 || x > x1 - 4)) { int f = (x & 1) * 16; g0 = 170 + f; g1 = 84 + f * .5; g2 = 66; }
    if (v == 3 && ((x + y) & 1) == 0 && y < ty) { g0 = 148; g1 = 148; g2 = 144; }
    if (v == 2 && y < y0 + 5 && H(x, y, s) > .45) { g0 = 64; g1 = 112; g2 = 54; }
    return {g0, g1, g2, GL};
}
C4 stuccoWin(int x, int y, int v) {
    static const int FV[4] = {0, 3, 1, 2};
    int fv = FV[v];
    if (v % 2 == 0) {
        if (y == 26 && x >= 8 && x <= 23) return {205, 200, 192};
        if (y == 27 && x >= 8 && x <= 23) return {246, 243, 236};
        if (y >= 28 && y <= 30 && std::fabs(x - 15.5) <= (31 - y) * 2.6) return y == 28 ? C4(232, 228, 220) : C4(246, 243, 236);
    } else {
        if (y >= 26 && y <= 29 && x >= 14 && x <= 17) return {248, 245, 238};
        if (y == 26 && x >= 9 && x <= 22) return {238, 234, 226};
    }
    static const int r[4] = {10, 6, 21, 25};
    return win(wStucco, x, y, r, fv, 31 + v);
}
C4 stair(Wall wall, int x, int y) {
    if (x >= 12 && x <= 19 && y >= 3 && y <= 29) {
        if (x == 12 || x == 19 || y == 3 || y == 29) return {192, 190, 184};
        return ((x - 13) % 3 == 2 || (y - 4) % 3 == 2) ? C4(166, 176, 170, GL) : C4(128 + H(x, y, 9) * 24, 158, 148, GL);
    }
    return wall(x, y);
}
C4 door(Wall wall, int x, int y, int k) {
    if (k == 2) {
        bool inA = x >= 9 && x <= 22 && (y <= 18 || (sq(x - 15.5) + sq(y - 18)) <= 42.25);
        if (inA) {
            bool inI = x >= 10 && x <= 21 && (y <= 18 || (sq(x - 15.5) + sq(y - 18)) <= 30.25);
            if (!inI) return {205, 200, 190};
            if (y > 18) return {70.0 + ((x + y) % 3) * 6, 64, 50, GL};
            if (x == 15 || x == 16) return {52, 34, 22};
            return (y % 6 == 0 || x == 12 || x == 19) ? C4(80, 52, 34) : C4(108, 70, 44);
        }
        if (y <= 1 && x >= 6 && x <= 25) return {170, 168, 162};
        return wall(x, y);
    }
    if (x >= 10 && x <= 21 && y <= 19) {
        if (x == 10 || x == 21 || y == 19) return {44, 40, 38};
        if (x == 19 && (y == 9 || y == 10)) return {200, 196, 180};
        if (y == 6 || y == 13) return {70, 48, 42};
        if (H(x, y, 55) > .9) return {120, 90, 70};
        return {88, 60, 52};
    }
    if (x >= 23 && x <= 24 && y >= 10 && y <= 13) return {160, 160, 158};
    if (x >= 13 && x <= 18 && y >= 21 && y <= 23) return (y == 22 && x == 15) ? C4(240, 240, 240) : C4(40, 70, 140);
    if (x >= 12 && x <= 19 && y >= 26 && y <= 30) {
        if (x == 12 || x == 19 || y == 26 || y == 30) return {185, 183, 178};
        return ((x - 13) % 3 == 2 || (y - 27) % 3 == 2) ? C4(160, 170, 164, GL) : C4(135, 160, 150, GL);
    }
    return wall(x, y);
}
C4 shop(int x, int y) {
    if (y >= 27 && y <= 30 && x >= 2 && x <= 29) {
        if (y == 27 || y == 30 || x == 2 || x == 29) return {60, 60, 64};
        return (H(x >> 1, y, 77) > .55 && x > 3 && x < 28) ? C4(250, 245, 230, GL) : C4(170, 40, 36, GL);
    }
    if (x >= 2 && x <= 29 && y >= 2 && y <= 24) {
        if (x == 2 || x == 29 || y == 2 || y == 24 || x == 11 || x == 20) return {96, 96, 102};
        if (y == 8 || y == 15) return {150, 150, 150, GL};
        if ((y == 9 || y == 16) && H(x, y, 5) > .3) return {120 + H(x, y, 6) * 120, 90 + H(x, y, 7) * 120, 60 + H(x, y, 8) * 100, GL};
        return ((x - y + 64) % 13) < 2 ? C4(88, 98, 108, GL) : C4(56, 66, 78, GL);
    }
    return wStucco(x, y);
}
std::function<C4(int, int)> leaves(int s, bool light) {
    std::vector<std::array<double, 3>> bl;
    for (int i = 0; i < 16; i++) bl.push_back({4 + H(i, 1, s) * 24, 3 + H(i, 2, s) * 26, 3.5 + H(i, 3, s) * 5});
    bl.push_back({16, 14, 9});
    return [bl, s, light](int x, int y) -> C4 {
        bool inn = false;
        for (auto& b : bl)
            if (sq(x - b[0]) + sq(y - b[1]) < b[2] * b[2]) { inn = true; break; }
        if (!inn || H(x, y, s) < .14) return CLEAR;
        double n = H(x, y, s + 1), k = .7 + y / 32.0 * .5;
        return light ? C4((96 + n * 40) * k, (136 + n * 40) * k, (54 + n * 20) * k) : C4((62 + n * 36) * k, (100 + n * 40) * k, (40 + n * 18) * k);
    };
}
bool crack[32][32], branchSet[32][32];

C4 wLog(int x, int y) {
    int r = y % 6;
    if (r == 0) return {58, 44, 32};
    static const double K[6] = {0, .8, 1, 1.05, .98, .86};
    double n = H(x, y, 80) * 14, k = K[r];
    if (x % 8 == 0 && H(x >> 3, y / 6, 81) > .7) return {70, 54, 40};
    return {(128 + n) * k, (98 + n) * k, (68 + n * .6) * k};
}
C4 izbaWin(int x, int y, C4 sh, C4 fr) {
    if (x >= 11 && x <= 20 && y >= 8 && y <= 21) {
        if (x == 11 || x == 20 || y == 8 || y == 21 || x == 15 || x == 16 || y == 16) return fr;
        double g0 = 52 + y, g1 = 62 + y, g2 = 78 + y;
        if (((x - y + 64) % 9) < 2) { g0 += 26; g1 += 26; g2 += 26; }
        if (y > 16 && x < 14 && H(x, y, 85) > .5) { g0 = 180; g1 = 70; g2 = 60; }
        return {g0, g1, g2, GL};
    }
    if (x >= 9 && x <= 22 && y >= 6 && y <= 23) return (x == 10 || x == 21 || y == 7 || y == 22) ? fr.k(.8) : fr;
    if (y >= 24 && y <= 29 && std::fabs(x - 15.5) <= (30 - y) * 1.6 + 2) {
        if (y == 26 && x % 3 == 0) return {60, 50, 40};
        return fr;
    }
    if (y == 5 && x >= 8 && x <= 23) return fr;
    if (y == 4 && x >= 10 && x <= 21 && x % 2 == 0) return fr;
    if (((x >= 4 && x <= 8) || (x >= 23 && x <= 27)) && y >= 8 && y <= 21) {
        if (x == 4 || x == 8 || x == 23 || x == 27 || y == 8 || y == 21) return sh.k(.7);
        if ((x + y) % 6 == 0) return sh.k(.85);
        return sh;
    }
    return wLog(x, y);
}
C4 wCob(int x, int y) {
    int r = y >> 2, o = (r & 1) * 2, cx = (x + o) >> 2;
    if (y % 4 == 0 || (x + o) % 4 == 0) return {66, 64, 62};
    double n = H(cx, r, 94) * 30 + H(x, y, 95) * 8;
    double hi = ((x + o) % 4 == 1 && y % 4 == 3) ? 14 : 0;
    return {104 + n + hi, 102 + n + hi, 98 + n + hi};
}
void walk(double x, double y, double a, double len, int d) {
    for (int i = 0; i < len; i++) {
        int ix = (int)x, iy = (int)y; // JS x|0 (truncation); keys outside the tile are never painted
        if (ix >= 0 && ix < 32 && iy >= 0 && iy < 32) branchSet[ix][iy] = true;
        x += std::cos(a) * .9; y += std::sin(a) * .9;
        a += (H(i, d, 96) - .5) * .35;
        if (d < 3 && H(i, d * 7 + (int32_t)len, 97) > .86) walk(x, y, a + (H(i, d, 98) > .5 ? .7 : -.7), len * .6, d + 1);
        if (x < 0 || x > 31 || y < 0 || y > 31) return;
    }
}
} // namespace

void buildAtlas() {
    {
        int cx = 7, cy = 0;
        for (int i = 0; i < 60; i++) {
            crack[cx][cy] = true;
            cy++;
            cx += (int)jsround(H(i, 3, 45) * 2 - 1);
            cx = (cx + 32) % 32;
            if (cy > 31) { cy = 0; cx = (cx + 13) % 32; }
        }
    }
    paint(0, wPanel); paint(6, wBrick); paint(12, wStucco);
    for (int v = 0; v < 4; v++) {
        static const int rp[4] = {7, 9, 24, 26}, rb[4] = {8, 8, 23, 26};
        paint(1 + v, [v](int x, int y) { return win(wPanel, x, y, rp, v, 11 + v); });
        paint(7 + v, [v](int x, int y) { return (y >= 27 && y <= 28 && x >= 7 && x <= 24) ? C4(176, 174, 168) : win(wBrick, x, y, rb, v, 21 + v); });
        paint(13 + v, [v](int x, int y) { return stuccoWin(x, y, v); });
    }
    paint(5, [](int x, int y) { return stair(wPanel, x, y); });
    paint(11, [](int x, int y) { return stair(wBrick, x, y); });
    paint(17, [](int x, int y) { return door(wPanel, x, y, 0); });
    paint(18, [](int x, int y) { return door(wBrick, x, y, 1); });
    paint(19, [](int x, int y) { return door(wStucco, x, y, 2); });
    paint(20, shop);
    paint(21, [](int x, int y) {
        double v = 56 + H(x, y, 40) * 12;
        if (H(x >> 2, y >> 2, 41) > .72) v += 12;
        if (H(x >> 3, y >> 3, 42) > .85) return C4(v - 6, v - 2, v + 8);
        return C4(v, v - 1, v - 3);
    });
    paint(22, [](int x, int y) {
        static const int D[4] = {-34, 14, 6, 0};
        double v = 196 + D[x & 3] + H(x, y, 43) * 8;
        if (y == 0) v -= 40;
        if (H(x >> 1, y >> 2, 44) > .9) return C4(150, 98, 68);
        return C4(v, v, v);
    });
    paint(23, [](int x, int y) {
        if (crack[x][y]) return C4(52, 52, 54);
        double v = 80 + H(x, y, 46) * 16 + (H(x >> 2, y >> 2, 47) > .8 ? -6 : 0);
        return C4(v, v, v + 3);
    });
    paint(24, [](int x, int y) {
        double n = H(x, y, 48), p = (H(x >> 2, y >> 2, 49) + H(x >> 3, y >> 3, 50)) * .5;
        if (p > .72) return C4(118 + n * 14, 104 + n * 12, 78 + n * 10);
        if (n > .9) return C4(118, 146, 72);
        return C4(78 + n * 22, 104 + n * 26, 50 + n * 12);
    });
    paint(25, [](int x, int y) {
        double n = H(x, y, 51);
        if (n > .94) return C4(160, 154, 142);
        return C4(124 + n * 16, 108 + n * 14, 84 + n * 10);
    });
    paint(26, [](int x, int y) {
        if (x == 0 || y == 0) return C4(118, 116, 112);
        double v = 152 + H(x, y, 52) * 16;
        return C4(v, v - 1, v - 5);
    });
    paint(27, [](int x, int y) {
        if (y >= 29) return C4(226, 226, 222);
        if (y <= 1) return C4(150, 148, 144);
        if (y < 6 && H(x, y >> 1, 54) > .7) return C4(160, 110, 80);
        static const int D[4] = {-42, 12, 4, -6};
        double v = 208 + D[x & 3] + H(x, y, 53) * 6;
        return C4(v, v, v);
    });
    auto glz = [](int x, int y, C4 f) {
        if (x % 8 == 0 || y <= 1 || y >= 30 || y == 20) return f;
        double g0 = 58 + y, g1 = 70 + y, g2 = 86 + y;
        if (((x - y + 64) % 10) < 2) { g0 += 28; g1 += 28; g2 += 28; }
        return C4(g0, g1, g2, GL);
    };
    paint(28, [&](int x, int y) { return glz(x, y, C4(236, 236, 232)); });
    paint(29, [&](int x, int y) { return glz(x, y, C4(112, 74, 48)); });
    paint(30, [](int x, int y) { return (x % 4 == 0 || y >= 29 || y <= 1) ? C4(64, 64, 68, 255) : CLEAR; });
    paint(31, [](int x, int y) {
        if (x == 0 || x == 31 || y == 31) return C4(70, 70, 70);
        if (x == 15 || x == 16) return C4(80, 78, 76);
        if (y == 16 && x > 2 && x < 29) return C4(150, 150, 146);
        if (x == 18 && y >= 14 && y <= 16) return C4(40, 40, 40);
        if (H(x, y, 55) > .88 || (y < 4 && H(x, y, 56) > .6)) return C4(150, 96, 66);
        double v = 196 + H(x, y, 57) * 14;
        return C4(v, v, v);
    });
    paint(32, [](int x, int y) {
        if (H(x >> 1, y >> 1, 58) > .88) return C4(140, 92, 62);
        double v = 120 + H(x, y, 59) * 18;
        return C4(v, v, v + 4);
    });
    paint(33, leaves(60, false)); paint(34, leaves(70, true));
    paint(35, [](int x, int y) {
        double v = 78 + H(x, y, 61) * 20 + ((x % 5) == 0 ? -18 : 0);
        return C4(v + 10, v - 4, v - 18);
    });
    paint(36, [](int x, int y) {
        if (H(x >> 2, y, 62) > .84 || H(x, y >> 1, 63) > .95) return C4(40, 38, 36);
        double v = 220 + H(x, y, 64) * 16;
        return C4(v, v, v - 6);
    });
    paint(37, [](int x, int y) {
        double v;
        if (y < 5) v = 150;
        else if (y < 11) v = (x % 4 < 2) ? 242 : 188;
        else if (y < 19) v = 232;
        else if (y < 25) v = 210 + (y - 19) * 5;
        else v = 246;
        v += H(x, y, 65) * 6;
        return C4(v, v - 2, v - 6);
    });
    paint(38, [](int x, int y) {
        if (y % 6 == 0) return C4(92, 64, 40);
        double v = H(x >> 3, y / 6, 66) * 30;
        return C4(146 + v + H(x, y, 67) * 10, 104 + v * .7, 64 + v * .4);
    });
    paint(39, [](int x, int y) { double n = H(x, y, 68); return C4(196 + n * 22, 180 + n * 20, 134 + n * 16); });
    paint(40, [](int, int) { return C4(255, 242, 214, GL); });
    paint(41, [](int x, int y) {
        double d = std::hypot(x - 15.5, y - 15.5) / 15.5;
        double th = (BY[(y & 3) * 4 + (x & 3)] + .5) / 16;
        return (d < 1 && (1 - d) * .9 > th) ? C4(255, 255, 255, LO) : CLEAR;
    });
    paint(42, wPlinth);
    paint(43, [](int x, int y) {
        if (x <= 1 || x >= 30 || y <= 2 || y >= 29 || x == 15 || x == 16) return C4(36, 36, 38);
        double g0 = 60 + y, g1 = 70 + y, g2 = 84 + y;
        if (((x - y + 64) % 11) < 2) { g0 += 30; g1 += 30; g2 += 30; }
        return C4(g0, g1, g2, GL);
    });
    paint(44, [](int x, int y) { double v = 232 + H(x, y, 69) * 10; return C4(v, v, v - 3); });
    paint(45, [](int x, int y) { static const int r[4] = {9, 7, 22, 24}; return win(wPlinth, x, y, r, 3, 70); });

    /* ================= NEW TILES ================= */
    paint(46, [](int x, int y) {
        bool inA = x >= 8 && x <= 23 && (y <= 17 || (sq(x - 15.5) + sq(y - 17)) <= 56.25);
        if (inA) {
            bool inI = x >= 9 && x <= 22 && (y <= 17 || (sq(x - 15.5) + sq(y - 17)) <= 42.25);
            if (!inI) return C4(200, 196, 188);
            if (x % 3 == 0 && y < 21) return C4(22, 22, 24);
            double d = 18 + y * .8;
            return C4(d + 6, d + 4, d);
        }
        return wStucco(x, y);
    });
    paint(47, wLog);
    paint(48, [](int x, int y) { return izbaWin(x, y, C4(60, 100, 170), C4(238, 236, 228)); });
    paint(49, [](int x, int y) { return izbaWin(x, y, C4(70, 130, 80), C4(90, 140, 210)); });
    paint(50, [](int x, int y) {
        if (x % 5 == 0) return C4(70, 68, 64);
        double n = H(x / 5, y >> 2, 86) * 12 + H(x, y, 87) * 8;
        double v = 170 + n;
        if (y < 5 && H(x, y, 88) > .5) v -= 30;
        return C4(v, v, v - 4);
    });
    paint(51, [](int x, int y) {
        bool inW = x >= 12 && x <= 19 && y >= 5 && (y <= 21 || (sq(x - 15.5) + sq(y - 21)) <= 12.25);
        if (inW) {
            bool inI = x >= 13 && x <= 18 && y >= 6 && (y <= 21 || (sq(x - 15.5) + sq(y - 21)) <= 6.25);
            if (!inI) return C4(214, 210, 200);
            if (x == 15 || x == 16) return C4(40, 40, 44);
            return C4(70 + y, 62 + y, 58 + y * .5, GL);
        }
        double r = std::hypot(x - 15.5, y - 21);
        if (r >= 5.2 && r <= 6.4 && y >= 21) return C4(206, 202, 194);
        if (y <= 2) return C4(200, 196, 188);
        if (x == 2 || x == 29) return C4(226, 222, 214);
        double v = 238 + H(x, y, 89) * 10;
        return C4(v, v - 2, v - 7);
    });
    paint(52, [](int x, int y) {
        int m = x % 8;
        C4 c = m < 2 ? C4(255, 232, 150) : (m == 5 || m == 6) ? C4(168, 118, 40) : C4(228, 176, 72);
        double n = H(x, y, 90) * 16;
        return C4(c.r + n - 8, c.g + n - 8, c.b);
    });
    paint(53, [](int x, int y) {
        int sx = (x + ((y >> 3) & 1) * 4) & 7, sy = y & 7;
        if ((sx == 3 && sy >= 2 && sy <= 4) || (sy == 3 && sx >= 2 && sx <= 4)) return C4(244, 204, 96);
        double n = H(x, y, 91) * 12;
        return C4(38 + n, 64 + n, 146 + n);
    });
    paint(54, [](int x, int y) {
        if (y == 6 && x >= 1 && x <= 30) return C4(212, 210, 204);
        if (y == 5 && x >= 1 && x <= 30) return wPanel(x, y).k(.7);
        if (x >= 2 && x <= 29 && y >= 7 && y <= 27) {
            if (x == 2 || x == 29 || y == 7 || y == 27 || (x - 2) % 9 == 0 || y == 21) return C4(228, 226, 218);
            double g0 = 52 + y, g1 = 64 + y, g2 = 80 + y;
            if (((x - y + 64) % 12) < 2) { g0 += 28; g1 += 30; g2 += 30; }
            if (y < 11 && H(x, y, 92) > .6) { g0 = 120; g1 = 140; g2 = 60; }
            return C4(g0, g1, g2, GL);
        }
        return wPanel(x, y);
    });
    paint(55, [](int x, int y) {
        if (y <= 3) return C4(42, 42, 44);
        if (y <= 12) { double v = H(x, y, 93) * 10; return C4(188 + v, 42, 36); }
        if (y <= 14) return C4(236, 226, 196);
        if (y <= 25) {
            int m = x % 8;
            if (m >= 1 && m <= 6) {
                double g0 = 58 + y, g1 = 68 + y, g2 = 82 + y;
                if (((x - y + 64) % 9) < 2) { g0 += 26; g1 += 26; g2 += 26; }
                return C4(g0, g1, g2, GL);
            }
        }
        return C4(236, 226, 196);
    });
    paint(57, wCob);
    paint(56, [](int x, int y) {
        if (x == 8 || x == 23) return C4(176, 178, 184);
        if (x == 9 || x == 24) return C4(110, 110, 116);
        if (x == 7 || x == 22 || x == 10 || x == 25) return C4(60, 58, 56);
        return wCob(x, y);
    });
    walk(16, 0, M_PI / 2, 34, 0); walk(16, 6, M_PI / 2 + .8, 16, 1); walk(16, 9, M_PI / 2 - .8, 16, 1);
    walk(16, 14, M_PI / 2 + .5, 12, 2); walk(16, 16, M_PI / 2 - .6, 12, 2); walk(16, 20, M_PI / 2 + .9, 9, 2);
    paint(58, [](int x, int y) { return branchSet[x][y] ? C4(62, 52, 44, 255) : CLEAR; });
    paint(59, [](int x, int y) {
        double w = (31 - y) / 31.0 * 15.5 + 1, jag = (y % 5) / 5.0 * 3;
        if (std::fabs(x - 15.5) > w - jag) return CLEAR;
        if ((x == 15 || x == 16) && y < 4) return C4(70, 52, 38);
        double n = H(x, y, 99) * 20, e = (y % 5 == 0) ? -12 : 0;
        return C4(26 + n * .5 + e, 62 + n + e, 42 + n * .6 + e);
    });
    paint(60, [](int x, int y) {
        if ((x == 15 || x == 16) && (y % 16) < 8) return C4(214, 212, 196);
        if (crack[x][y]) return C4(52, 52, 54);
        double v = 80 + H(x, y, 46) * 16;
        return C4(v, v, v + 3);
    });
    paint(61, [](int x, int y) {
        int r = x % 8;
        if (r < 2) return C4(96, 78, 58);
        double n = H(x, y, 100);
        if ((r == 4 || r == 5) && n > .45) return C4(70 + n * 40, 120 + n * 40, 50);
        return C4(122 + n * 14, 100 + n * 12, 76 + n * 10);
    });
    paint(62, [](int x, int y) {
        double n = H(x, y, 101), s = ((x * 3 + y) % 7 == 0) ? -20 : 0;
        return C4(196 + n * 30 + s, 166 + n * 26 + s, 84 + n * 20 + s);
    });

    /* ================= INTERIOR TILES (port extension) ================= */
    paint(TL::WPAPER1, [](int x, int y) { // cream stripes with small roses
        double n = H(x, y, 201) * 8;
        int cx = x % 8, cy = (y + (x / 8 % 2) * 4) % 8;
        if (cx == 0) return C4(196 + n, 176 + n, 140 + n);
        if (cx == 4 && cy == 3) return C4(186, 84, 76);
        if ((cx == 3 || cx == 5) && cy == 3) return C4(200, 120, 108);
        if (cx == 4 && (cy == 2 || cy == 4)) return C4(120, 150, 96);
        return C4(224 + n, 208 + n, 172 + n);
    });
    paint(TL::WPAPER2, [](int x, int y) { // pale green diamond lattice
        double n = H(x, y, 202) * 8;
        int d1 = (x + y) % 10, d2 = (x - y + 320) % 10;
        if (d1 == 0 || d2 == 0) return C4(126 + n, 158 + n, 144 + n);
        if (d1 == 5 && d2 == 5) return C4(232, 220, 170);
        return C4(178 + n, 206 + n, 190 + n);
    });
    paint(TL::WPAPER3, [](int x, int y) { // dusty pink bands
        double n = H(x, y, 203) * 8;
        bool band = x % 16 < 8;
        if (!band && (y % 4 == 0) && (x % 4 == 2)) return C4(236, 214, 200);
        return band ? C4(212 + n, 172 + n, 162 + n) : C4(196 + n, 150 + n, 142 + n);
    });
    paint(TL::PARQUET, [](int x, int y) { // herringbone-ish oak blocks
        int bx = x / 8, by = y / 4;
        bool alt = (bx + by) % 2;
        int gx = alt ? (y % 4) : (x % 8);
        if ((!alt && x % 8 == 0) || y % 4 == 0 || (alt && x % 8 == 0)) return C4(92, 60, 36);
        double v = H(bx, by, 204) * 40, g = (gx % 3 == 0) ? -10 : 0;
        return C4(158 + v + g, 106 + v * .7 + g, 62 + v * .4 + g);
    });
    paint(TL::LINO, [](int x, int y) {
        double n = H(x, y, 205) * 14;
        bool c = ((x / 8) + (y / 8)) % 2;
        if (H(x, y, 206) > .93) n -= 30;
        return c ? C4(180 + n, 150 + n, 110 + n) : C4(120 + n, 84 + n, 60 + n);
    });
    paint(TL::PODYEZD, [](int x, int y) { // oil-painted lower wall, whitewash above
        double n = H(x, y, 207) * 10;
        if (y < 17) {
            if (H(x >> 1, y, 208) > .95) return C4(130, 150, 140);
            return C4(62 + n, 112 + n, 102 + n);
        }
        if (y == 17) return C4(40, 72, 66);
        if (y > 20 && H(x >> 1, y >> 1, 209) > .975) return C4(80, 80, 96);
        return C4(214 + n, 212 + n, 200 + n);
    });
    paint(TL::WHITEW, [](int x, int y) {
        double n = H(x, y, 210) * 12 + (H(x >> 3, y >> 3, 211) > .8 ? -8 : 0);
        return C4(222 + n, 220 + n, 212 + n);
    });
    paint(TL::APTDOOR, [](int x, int y) { // padded leatherette door
        if (x <= 1 || x >= 30 || y >= 31) return C4(60, 40, 30);
        if (x == 26 && (y == 14 || y == 15)) return C4(210, 208, 196);
        if (x == 26 && y == 13) return C4(120, 118, 110);
        if (y >= 23 && y <= 25 && x >= 13 && x <= 18) return C4(222, 200, 120);
        int u = x - 2, v = y;
        if (u % 7 == 3 && v % 7 == 3) return C4(214, 192, 126);
        if ((u + v) % 7 == 0 || (u - v + 70) % 7 == 0) return C4(92, 36, 30);
        double n = H(x, y, 212) * 12;
        return C4(128 + n, 54 + n * .5, 42 + n * .4);
    });
    paint(TL::STEP, [](int x, int y) { // terrazzo tread with a worn nosing
        if (y >= 28) return C4(108, 106, 102);
        double n = H(x, y, 213);
        if (n > .9) return C4(90 + n * 60, 80 + n * 40, 70);
        if (n < .06) return C4(210, 208, 200);
        double v = 150 + H(x >> 1, y >> 1, 214) * 14;
        return C4(v, v - 2, v - 8);
    });
    paint(TL::CARPET, [](int x, int y) { // Persian wall carpet
        int b = std::min(std::min(x, 31 - x), std::min(y, 31 - y));
        if (b == 0) return C4(40, 30, 60);
        if (b == 1) return C4(200, 160, 70);
        if (b == 2 || b == 3) return ((x + y) % 4 < 2) ? C4(30, 50, 110) : C4(180, 40, 40);
        if (b == 4) return C4(200, 160, 70);
        double d = std::fabs(x - 15.5) + std::fabs(y - 15.5);
        if (d < 3) return C4(230, 200, 110);
        if (d < 6) return C4(30, 50, 110);
        if (d < 7) return C4(230, 200, 110);
        if (d < 10) return ((x + y) % 3 == 0) ? C4(230, 200, 110) : C4(120, 24, 30);
        if (((x * 7 + y * 3) % 11) == 0) return C4(40, 90, 70);
        double n = H(x, y, 215) * 20;
        return C4(150 + n, 28 + n * .3, 32 + n * .3);
    });
    paint(TL::ICON, [](int x, int y) { // gilded icon: saint with halo
        if (x <= 1 || x >= 30 || y <= 1 || y >= 30) return C4(214, 172, 72);
        double hd = std::hypot(x - 15.5, y - 22.5);
        if (hd < 3.2) return C4(206, 156, 116);
        if (hd < 6.2) return hd > 5.2 ? C4(170, 120, 40) : C4(246, 206, 90);
        if (y < 17 && std::fabs(x - 15.5) < 3 + (17 - y) * .45) return (x < 16) ? C4(150, 36, 36) : C4(40, 58, 130);
        if (y < 17 && std::fabs(x - 15.5) < 4 + (17 - y) * .5) return C4(90, 26, 26);
        double n = H(x, y, 216) * 20;
        return C4(160 + n, 118 + n, 44 + n * .5);
    });
    paint(TL::IKONO, [](int x, int y) { // carved gilded iconostasis frame
        int cx = x % 16;
        if (cx <= 1) return C4(150, 108, 34);
        if (y >= 26 && std::hypot(cx - 8.5, y - 26) > 6.2 && std::hypot(cx - 8.5, y - 26) < 7.4) return C4(252, 222, 130);
        if (y % 16 == 0) return C4(150, 108, 34);
        double n = H(x, y, 217) * 30, m = ((x + y) % 6 == 0) ? 20 : 0;
        return C4(206 + n + m, 160 + n + m, 62 + n * .4);
    });
    paint(TL::STOVE, [](int x, int y) { // whitewashed Russian stove
        int r = y >> 3, o = (r & 1) * 8;
        double n = H(x, y, 218) * 10;
        if (y % 8 == 0 || (x + o) % 16 == 0) return C4(204 + n, 200 + n, 190 + n);
        if (H(x >> 2, y >> 2, 219) > .92) return C4(190, 184, 170);
        return C4(236 + n * .5, 234 + n * .5, 226 + n * .5);
    });
    paint(TL::BOARD, [](int x, int y) { // school blackboard
        if (x <= 1 || x >= 30 || y <= 1 || y >= 30) return C4(120, 82, 50);
        if (y >= 8 && y % 5 == 0 && x > 4 && x < 26 && H(x, y, 220) > .3) return C4(196, 204, 196);
        double n = H(x, y, 221) * 10;
        return C4(36 + n, 58 + n, 48 + n);
    });
    paint(TL::MARBLE, [](int x, int y) {
        bool c = ((x / 16) + (y / 16)) % 2;
        double n = H(x, y, 222) * 10, vein = (std::fabs(std::sin((x + y * .6) * .35 + H(x >> 3, y >> 3, 223) * 3)) < .08) ? -30 : 0;
        return c ? C4(206 + n + vein, 200 + n + vein, 190 + n + vein) : C4(126 + n + vein * .5, 112 + n + vein * .5, 100 + n + vein * .5);
    });
    paint(TL::FABRIC, [](int x, int y) {
        double n = H(x, y, 224) * 16, w = ((x + y) % 2) ? 10 : -6;
        return C4(200 + n + w, 200 + n + w, 200 + n + w);
    });
    paint(TL::BATHTILE, [](int x, int y) { // 8px glazed wall tiles, a pale blue one here and there
        if (x % 8 == 0 || y % 8 == 0) return C4(170, 172, 168);
        double n = H(x, y, 226) * 8, sheen = (x % 8 == 1 || y % 8 == 1) ? 10 : 0;
        if (H(x >> 3, y >> 3, 227) > .8) return C4(170 + n + sheen, 204 + n + sheen, 214 + n + sheen);
        return C4(226 + n + sheen, 228 + n + sheen, 224 + n + sheen);
    });
    paint(TL::FLOORTILE, [](int x, int y) { // terracotta and cream floor tiles
        if (x % 8 == 0 || y % 8 == 0) return C4(96, 88, 80);
        double n = H(x, y, 228) * 10;
        bool c = ((x >> 3) + (y >> 3)) % 2;
        return c ? C4(176 + n, 96 + n * .6, 70 + n * .5) : C4(214 + n, 204 + n, 180 + n);
    });
    paint(TL::STARS, [](int x, int y) { // blue church vault with gold stars
        int sx = (x + ((y >> 3) & 1) * 4) & 7, sy = y & 7;
        if ((sx == 3 && sy >= 2 && sy <= 4) || (sy == 3 && sx >= 2 && sx <= 4)) return C4(240, 200, 90);
        double n = H(x, y, 225) * 12;
        return C4(30 + n, 50 + n, 110 + n);
    });
}

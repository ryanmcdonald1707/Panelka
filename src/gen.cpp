// Geometry builder + every building / prop / scene generator.
// NOTE: JS evaluates arguments left-to-right; C++ does not. Every expression that
// consumes the RNG more than once is split into sequenced locals, so a seed
// produces the same world as the browser version.
#include "panelka.h"

#include <algorithm>
#include <map>

Builder B;
std::vector<std::array<double, 4>> COL;
std::vector<V3> SMOKE;
std::vector<Light> LIGHTS;
std::string SEASON = "autumn";
std::vector<BuildingRec> RECS;
std::vector<WObj> WOBJ;

/* ================= PALETTES ================= */
using Pal = std::vector<V3>;
static const Pal PANELT{{.93, .93, .91}, {.88, .88, .86}, {.96, .93, .86}, {.86, .9, .94}, {.95, .89, .87}, {.9, .94, .87}, {.8, .8, .78}};
static const Pal BRICKT{{1, .98, .94}, {1, .97, .92}, {.88, .52, .42}, {.97, .86, .64}};
static const Pal STUCT{{.97, .84, .56}, {.96, .72, .64}, {.78, .88, .74}, {.96, .93, .84}, {.76, .85, .94}, {.94, .8, .68}};
static const Pal TENT{{.95, .78, .5}, {.88, .58, .52}, {.62, .74, .84}, {.88, .86, .7}, {.72, .82, .66}, {.93, .66, .48}, {.92, .9, .86}, {.8, .66, .8}};
static const Pal ACC{{.86, .36, .3}, {.32, .52, .82}, {.96, .76, .32}, {.42, .7, .52}, {.7, .46, .76}, {.95, .56, .3}, {.3, .66, .72}};
static const Pal BALP{{.95, .95, .95}, {.82, .86, .9}, {.9, .52, .38}, {.62, .76, .9}, {.95, .86, .52}, {.58, .72, .56}, {.72, .72, .7}};
static const Pal ROOFT{{.42, .58, .44}, {.72, .36, .3}, {.6, .62, .66}, {.5, .52, .56}};
static const Pal SLATE{{.78, .78, .75}, {.7, .72, .7}};
static const Pal CLOTH{{.9, .9, .95}, {.9, .3, .3}, {.3, .5, .9}, {.95, .85, .3}, {.6, .8, .6}};
static const Pal GARC{{.45, .6, .45}, {.45, .55, .75}, {.6, .45, .35}, {.8, .8, .78}, {.75, .4, .35}};
static const Pal FENCEC{{.55, .7, .55}, {.6, .7, .85}, {.9, .9, .85}, {.75, .6, .45}, {.7, .7, .7}, {.85, .75, .5}};
static const Pal WARM{{1, .78, .42}, {1, .88, .62}, {.62, .75, 1}, {1, .62, .34}, {.95, .92, .8}};
static const V3 STAIRC{.62, .85, .72}, SHOPC{1, .95, .82}, SODIUM{1, .66, .3}, REDL{1, .15, .1};
// Érinski signage (docs/SETTING.md). Keep list lengths: R.pick() indexes the seeded RNG stream.
static const std::vector<std::string> SHOPW{"ГРОСЕРИЯ", "АРАН", "БАННЕ", "ПОТИГЕРИЯ", "ГЛАСРИ", "БИАТОРГ", "ЛЕАБРИ", "БРОГИ", "ПОШТА", "СУИЛИ", "КАИФЕ", "ИАСКА", "БЛАХИ"};
static const std::vector<std::string> SLOGANS{"ГЛОИР ТРУДУ!", "СИОХ ТРУД МАЙ", "ГЛОИР ЕОЛАСУ", "СИОХ", "СИОХ ДОН ДОМАН!"};
static const std::map<std::string, std::string> NAMES{{"khrush", "Khrushchyovka"}, {"panel9", "Panel slab"}, {"tower", "Point tower"}, {"stalinka", "Stalinka"}, {"platten", "Plattenbau"}, {"tenement", "Tenement"}, {"school", "School"}, {"univermag", "Univermag"}, {"church", "Orthodox church"}, {"izba", "Izba"}};
const std::vector<std::string> TYPES{"khrush", "panel9", "tower", "stalinka", "platten", "tenement", "school", "univermag", "church", "izba"};
const std::vector<std::string> SOLO{"tenement", "school", "univermag", "church", "izba"};

static Em winEm(Rng& R) {
    if (!R.chance(.85)) return Em();
    int k = R.integer(0, 4);
    const V3& c = WARM[k];
    Em e = em(c, R());
    e.fl = k == 2 ? R() + .01 : 0;
    return e;
}

/* ================= PIXEL FONT (5x7 Cyrillic) ================= */
static const std::map<uint32_t, std::array<int, 7>> FONT{
    {U' ', {0, 0, 0, 0, 0, 0, 0}}, {U'!', {4, 4, 4, 4, 4, 0, 4}}, {U'А', {14, 17, 17, 31, 17, 17, 17}}, {U'Б', {31, 16, 16, 30, 17, 17, 30}}, {U'В', {30, 17, 17, 30, 17, 17, 30}}, {U'Г', {31, 16, 16, 16, 16, 16, 16}},
    {U'Д', {6, 10, 10, 10, 10, 31, 17}}, {U'Е', {31, 16, 16, 30, 16, 16, 31}}, {U'Ж', {21, 21, 14, 4, 14, 21, 21}}, {U'З', {14, 17, 1, 6, 1, 17, 14}}, {U'И', {17, 17, 19, 21, 25, 17, 17}}, {U'Й', {10, 17, 19, 21, 25, 17, 17}},
    {U'К', {17, 18, 20, 24, 20, 18, 17}}, {U'Л', {7, 9, 9, 9, 9, 9, 17}}, {U'М', {17, 27, 21, 21, 17, 17, 17}}, {U'Н', {17, 17, 17, 31, 17, 17, 17}}, {U'О', {14, 17, 17, 17, 17, 17, 14}}, {U'П', {31, 17, 17, 17, 17, 17, 17}},
    {U'Р', {30, 17, 17, 30, 16, 16, 16}}, {U'С', {14, 17, 16, 16, 16, 17, 14}}, {U'Т', {31, 4, 4, 4, 4, 4, 4}}, {U'У', {17, 17, 17, 15, 1, 17, 14}}, {U'Ф', {4, 14, 21, 21, 14, 4, 4}}, {U'Х', {17, 17, 10, 4, 10, 17, 17}},
    {U'Ц', {18, 18, 18, 18, 18, 31, 1}}, {U'Ч', {17, 17, 17, 15, 1, 1, 1}}, {U'Ш', {21, 21, 21, 21, 21, 21, 31}}, {U'Щ', {21, 21, 21, 21, 21, 31, 1}}, {U'Ь', {16, 16, 16, 30, 17, 17, 30}}, {U'Ы', {17, 17, 17, 25, 21, 21, 25}},
    {U'Э', {14, 17, 1, 7, 1, 17, 14}}, {U'Ю', {18, 21, 21, 29, 21, 21, 18}}, {U'Я', {15, 17, 17, 15, 5, 9, 17}},
    {U'0', {14, 17, 19, 21, 25, 17, 14}}, {U'1', {4, 12, 4, 4, 4, 4, 14}}, {U'2', {14, 17, 1, 2, 4, 8, 31}}, {U'3', {31, 2, 4, 2, 1, 17, 14}}, {U'4', {2, 6, 10, 18, 31, 2, 2}}, {U'5', {31, 16, 30, 1, 1, 17, 14}}, {U'6', {6, 8, 16, 30, 17, 17, 14}}, {U'7', {31, 1, 2, 4, 8, 8, 8}}, {U'8', {14, 17, 17, 14, 17, 17, 14}}, {U'9', {14, 17, 17, 15, 1, 2, 12}}};

std::vector<uint32_t> utf8cp(const std::string& s) {
    std::vector<uint32_t> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        uint32_t cp;
        int n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c >> 5) == 6) { cp = c & 31; n = 2; }
        else if ((c >> 4) == 14) { cp = c & 15; n = 3; }
        else { cp = c & 7; n = 4; }
        for (int k = 1; k < n && i + k < s.size(); k++) cp = (cp << 6) | (s[i + k] & 63);
        out.push_back(cp);
        i += n;
    }
    return out;
}

/* ================= GEOMETRY BUILDER ================= */
static const V3 Z3{0, 0, 0}, XV{1, 0, 0}, ZV{0, 0, 1};
static V3 sub(const V3& a, const V3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
static V3 cross(const V3& a, const V3& b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
static V3 neg(const V3& a) { return {-a[0], -a[1], -a[2]}; }
static double len3(const V3& a) { return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }
static V3 nrm(const V3& a) { double l = len3(a); if (l == 0) l = 1; return {a[0] / l, a[1] / l, a[2] / l}; }
static V3 mul(const V3& c, double k) { return {c[0] * k, c[1] * k, c[2] * k}; }
static V3 mix(const V3& a, const V3& b, double t) { return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t}; }
static V3 jit(const V3& c, Rng& R) { return mul(c, .94 + R() * .09); }
static V3 addv(const V3& p, const V3& v, double s) { return {p[0] + v[0] * s, p[1] + v[1] * s, p[2] + v[2] * s}; }

void Builder::reset() { V.clear(); V.shrink_to_fit(); xf(0, 0, 0); up = false; fogK = 1; snowK = 1; }
void Builder::xfA(double x, double z, double a) { ox = x; oz = z; cs = std::cos(a); sn = std::sin(a); }
V3 Builder::tp(const V3& v) const { return {ox + v[0] * cs + v[2] * sn, v[1], oz - v[0] * sn + v[2] * cs}; }
V3 Builder::td(const V3& v) const { return {v[0] * cs + v[2] * sn, v[1], -v[0] * sn + v[2] * cs}; }

void Builder::quadUV(V3 a, V3 b, V3 c, V3 d, int t, const V3& col, const Em& e, double u0, double v0, double u1, double v1) {
    const double rr[4] = {u0, v0, u1, v1};
    rrOverride = rr;
    quad(a, b, c, d, t, col, e);
    rrOverride = nullptr;
}
void Builder::quad(V3 a, V3 b, V3 c, V3 d, int t, const V3& col, const Em& e, int r) {
    a = tp(a); b = tp(b); c = tp(c); d = tp(d);
    V3 n;
    if (up) n = {0, 1, 0};
    else {
        n = cross(sub(b, a), sub(d, a));
        double l = len3(n);
        if (l < 1e-9) { n = cross(sub(c, b), sub(d, b)); l = len3(n); }
        if (l < 1e-9) return;
        n = {n[0] / l, n[1] / l, n[2] / l};
    }
    int tx = t % 16, ty = t / 16;
    static const double R01[4] = {0, 0, 1, 1}, RFL[4] = {1, 0, 0, 1};
    const double* rr = rrOverride ? rrOverride : r == 1 ? RFL : R01;
    double u0 = (tx * 32 + .5 + rr[0] * 31) / 512, u1 = (tx * 32 + .5 + rr[2] * 31) / 512;
    double v0 = (ty * 32 + .5 + rr[1] * 31) / 512, v1 = (ty * 32 + .5 + rr[3] * 31) / 512;
    double us[6], ws[6];
    if (r == 90) { double a_[6] = {u1, u1, u0, u1, u0, u0}, b_[6] = {v0, v1, v1, v0, v1, v0}; std::copy(a_, a_ + 6, us); std::copy(b_, b_ + 6, ws); }
    else { double a_[6] = {u0, u1, u1, u0, u1, u0}, b_[6] = {v0, v0, v1, v0, v1, v1}; std::copy(a_, a_ + 6, us); std::copy(b_, b_ + 6, ws); }
    const V3* vs[6] = {&a, &b, &c, &a, &c, &d};
    for (int i = 0; i < 6; i++) {
        const V3& p = *vs[i];
        double k = n[1] > .7 ? 1 : (.78 + .22 * std::min(1.0, std::max(0.0, p[1]) / 7));
        float f[FLOATS_PER_VERT] = {(float)p[0], (float)p[1], (float)p[2], (float)n[0], (float)n[1], (float)n[2], (float)us[i], (float)ws[i],
                                    (float)(col[0] * k), (float)(col[1] * k), (float)(col[2] * k),
                                    (float)(e.on ? e.r : 0), (float)(e.on ? e.g : 0), (float)(e.on ? e.b : 0),
                                    (float)(e.on ? e.thr : 0), (float)fogK, (float)(e.on ? e.fl : 0), (float)snowK};
        V.insert(V.end(), f, f + FLOATS_PER_VERT);
    }
}
void Builder::dquad(const V3& a, const V3& b, const V3& c, const V3& d, int t, const V3& col, const Em& e) {
    quad(a, b, c, d, t, col, e);
    quad(b, a, d, c, t, col, e, 1);
}
void Builder::wall(V3 o, V3 u, V3 v, double lu, double lv, int t, const V3& col, TS ts, const Em& e) {
    int nu = std::max(1, (int)jsround(lu / ts.u)), nv = std::max(1, (int)jsround(lv / ts.v));
    auto p = [&](double s, double q) { return V3{o[0] + u[0] * s + v[0] * q, o[1] + u[1] * s + v[1] * q, o[2] + u[2] * s + v[2] * q}; };
    for (int i = 0; i < nu; i++)
        for (int j = 0; j < nv; j++) {
            double a0 = lu * i / nu, a1 = lu * (i + 1) / nu, b0 = lv * j / nv, b1 = lv * (j + 1) / nv;
            quad(p(a0, b0), p(a1, b0), p(a1, b1), p(a0, b1), t, col, e);
        }
}
void Builder::wallF(V3 o, V3 u, V3 v, double lu, double lv, const std::function<int()>& t, const V3& col, TS ts, const std::function<Em()>& e) {
    int nu = std::max(1, (int)jsround(lu / ts.u)), nv = std::max(1, (int)jsround(lv / ts.v));
    auto p = [&](double s, double q) { return V3{o[0] + u[0] * s + v[0] * q, o[1] + u[1] * s + v[1] * q, o[2] + u[2] * s + v[2] * q}; };
    for (int i = 0; i < nu; i++)
        for (int j = 0; j < nv; j++) {
            double a0 = lu * i / nu, a1 = lu * (i + 1) / nu, b0 = lv * j / nv, b1 = lv * (j + 1) / nv;
            int tt = t();
            Em ee = e();
            quad(p(a0, b0), p(a1, b0), p(a1, b1), p(a0, b1), tt, col, ee);
        }
}
void Builder::obox(V3 o, V3 U, V3 Nn, double lu, double lv, double ln, int t, const V3& col, TS ts, const Opt& opt) {
    const V3 upv{0, 1, 0};
    int tt = opt.top >= 0 ? opt.top : t;
    V3 tc = opt.hasTopCol ? opt.topCol : col;
    const std::string& sk = opt.skip;
    const Em& e = opt.em;
    V3 nU = neg(U), nN = neg(Nn);
    auto has = [&](char ch) { return sk.find(ch) != std::string::npos; };
    if (!has('f')) wall(addv(o, Nn, ln), U, upv, lu, lv, t, col, ts, e);
    if (!has('b')) wall(addv(o, U, lu), nU, upv, lu, lv, t, col, ts, e);
    if (!has('l')) wall(o, Nn, upv, ln, lv, t, col, ts, e);
    if (!has('r')) wall(addv(addv(o, U, lu), Nn, ln), nN, upv, ln, lv, t, col, ts, e);
    if (!has('t')) wall(addv(addv(o, upv, lv), Nn, ln), U, nN, lu, ln, tt, tc, ts, e);
    if (opt.bottom) wall(o, U, Nn, lu, ln, t, col, ts, e);
}
void Builder::box(double x0, double y0, double z0, double x1, double y1, double z1, int t, const V3& col, TS ts, const Opt& opt) {
    obox({x0, y0, z0}, XV, ZV, x1 - x0, y1 - y0, z1 - z0, t, col, ts, opt);
}
void Builder::surf(V3 p0, V3 p1, V3 p2, V3 p3, int nu, int nv, int t, const V3& col, const Em& e) {
    auto L = [](const V3& a, const V3& b, double s) { return V3{a[0] + (b[0] - a[0]) * s, a[1] + (b[1] - a[1]) * s, a[2] + (b[2] - a[2]) * s}; };
    auto P = [&](double s, double q) { return L(L(p0, p1, s), L(p3, p2, s), q); };
    for (int i = 0; i < nu; i++)
        for (int j = 0; j < nv; j++)
            quad(P((double)i / nu, (double)j / nv), P((double)(i + 1) / nu, (double)j / nv), P((double)(i + 1) / nu, (double)(j + 1) / nv), P((double)i / nu, (double)(j + 1) / nv), t, col, e);
}
void Builder::beam(V3 p0, V3 p1, double th, int t, const V3& col, const Em& e) {
    V3 d = sub(p1, p0), dir = nrm(d), a = cross(dir, {0, 1, 0});
    if (len3(a) < 1e-4) a = {1, 0, 0};
    a = nrm(a);
    V3 b = cross(a, dir);
    double h = th / 2;
    V3 o[4] = {{a[0] + b[0], a[1] + b[1], a[2] + b[2]}, {-a[0] + b[0], -a[1] + b[1], -a[2] + b[2]}, {-a[0] - b[0], -a[1] - b[1], -a[2] - b[2]}, {a[0] - b[0], a[1] - b[1], a[2] - b[2]}};
    auto at = [&](const V3& p, int k) { return V3{p[0] + o[k][0] * h, p[1] + o[k][1] * h, p[2] + o[k][2] * h}; };
    for (int k = 0; k < 4; k++) { int k1 = (k + 1) % 4; quad(at(p0, k1), at(p0, k), at(p1, k), at(p1, k1), t, col, e); }
}
void Builder::lathe(double cx, double cy, double cz, const std::vector<std::array<double, 2>>& prof, int seg, int t, const V3& col, const Em& e, double a0, double a1) {
    auto P = [&](double r, double y, double a) { return V3{cx + r * std::cos(a), cy + y, cz + r * std::sin(a)}; };
    for (size_t i = 0; i + 1 < prof.size(); i++) {
        double r0 = prof[i][0], y0 = prof[i][1], r1 = prof[i + 1][0], y1 = prof[i + 1][1];
        for (int s = 0; s < seg; s++) {
            double b0 = a0 + (a1 - a0) * s / seg, b1 = a0 + (a1 - a0) * (s + 1) / seg;
            quad(P(r0, y0, b1), P(r0, y0, b0), P(r1, y1, b0), P(r1, y1, b1), t, col, e);
        }
    }
}
double Builder::textW(const std::string& s, double px) { return ((double)utf8cp(s).size() * 6 - 1) * px; }
void Builder::text(const std::string& s, V3 o, V3 u, V3 v, double px, const V3& col, const Em& e) {
    int cx = 0;
    int t = e ? TL::LAMP : TL::PAINT;
    auto P = [&](double a, double b) { return V3{o[0] + u[0] * a + v[0] * b, o[1] + u[1] * a + v[1] * b, o[2] + u[2] * a + v[2] * b}; };
    for (uint32_t ch : utf8cp(s)) {
        auto it = FONT.find(ch);
        const auto& g = it != FONT.end() ? it->second : FONT.at(U' ');
        for (int r = 0; r < 7; r++) {
            int bits = g[r], c = 0;
            while (c < 5) {
                if (bits & (16 >> c)) {
                    int q = c;
                    while (q + 1 < 5 && (bits & (16 >> (q + 1)))) q++;
                    double x0 = (cx + c) * px, x1 = (cx + q + 1) * px, y0 = (6 - r) * px, y1 = (7 - r) * px;
                    quad(P(x0, y0), P(x1, y0), P(x1, y1), P(x0, y1), t, col, e);
                    c = q + 1;
                } else c++;
            }
        }
        cx += 6;
    }
}

static void pyramid(double x0, double z0, double x1, double z1, double y, double top, int t, const V3& col) {
    V3 A{(x0 + x1) / 2, top, (z0 + z1) / 2};
    B.surf({x0, y, z1}, {x1, y, z1}, A, A, 1, 1, t, col); B.surf({x1, y, z1}, {x1, y, z0}, A, A, 1, 1, t, col);
    B.surf({x1, y, z0}, {x0, y, z0}, A, A, 1, 1, t, col); B.surf({x0, y, z0}, {x0, y, z1}, A, A, 1, 1, t, col);
}
static void hipRoof(double x0, double z0, double x1, double z1, double y, double rh, double e, int t, const V3& col) {
    double W = x1 - x0, D = z1 - z0, cx = (x0 + x1) / 2, cz = (z0 + z1) / 2, yr = y + rh;
    double X0 = x0 - e, X1 = x1 + e, Z0 = z0 - e, Z1 = z1 + e;
    if (W >= D) {
        double r0 = cx - (W - D) / 2, r1 = cx + (W - D) / 2;
        int nu = std::max(1, (int)jsround(W / 3)), nd = std::max(1, (int)jsround(D / 3));
        B.surf({X0, y, Z1}, {X1, y, Z1}, {r1, yr, cz}, {r0, yr, cz}, nu, 2, t, col); B.surf({X1, y, Z0}, {X0, y, Z0}, {r0, yr, cz}, {r1, yr, cz}, nu, 2, t, col);
        B.surf({X1, y, Z1}, {X1, y, Z0}, {r1, yr, cz}, {r1, yr, cz}, nd, 2, t, col); B.surf({X0, y, Z0}, {X0, y, Z1}, {r0, yr, cz}, {r0, yr, cz}, nd, 2, t, col);
    } else {
        double r0 = cz - (D - W) / 2, r1 = cz + (D - W) / 2;
        int nu = std::max(1, (int)jsround(D / 3)), nd = std::max(1, (int)jsround(W / 3));
        B.surf({X1, y, Z1}, {X1, y, Z0}, {cx, yr, r0}, {cx, yr, r1}, nu, 2, t, col); B.surf({X0, y, Z0}, {X0, y, Z1}, {cx, yr, r1}, {cx, yr, r0}, nu, 2, t, col);
        B.surf({X0, y, Z1}, {X1, y, Z1}, {cx, yr, r1}, {cx, yr, r1}, nd, 2, t, col); B.surf({X1, y, Z0}, {X0, y, Z0}, {cx, yr, r0}, {cx, yr, r0}, nd, 2, t, col);
    }
}
static const std::vector<std::array<double, 2>> ONION{{1, 0}, {1.12, .35}, {1.15, .6}, {1, 1}, {.72, 1.35}, {.42, 1.62}, {.16, 1.86}, {.03, 2.1}};
static void orthoCross(double x, double y, double z, double s) {
    V3 c{1, .85, .42};
    B.box(x - .06 * s, y, z - .06 * s, x + .06 * s, y + 2.4 * s, z + .06 * s, TL::GOLD, c, 99);
    B.box(x - .6 * s, y + 1.6 * s, z - .05 * s, x + .6 * s, y + 1.72 * s, z + .05 * s, TL::GOLD, c, 99);
    B.box(x - .3 * s, y + 2.02 * s, z - .05 * s, x + .3 * s, y + 2.12 * s, z + .05 * s, TL::GOLD, c, 99);
    B.beam({x - .42 * s, y + .72 * s, z}, {x + .42 * s, y + .98 * s, z}, .1 * s, TL::GOLD, c);
}
static void onion(double cx, double y, double cz, double r, int t, const V3& col) {
    std::vector<std::array<double, 2>> p;
    for (auto& q : ONION) p.push_back({q[0] * r, q[1] * r});
    B.lathe(cx, y, cz, p, 8, t, col);
    orthoCross(cx, y + 2.05 * r, cz, std::max(.5, r * .45));
}

/* ================= WORLD STATE ================= */
using Rect = std::array<double, 4>;
static void addCol(const Rect& r) { COL.push_back(r); }
static void light(const V3& pl, const V3* dl, const V3& c, double r, double inten, double thr, double cone, double fl, int pri) {
    Light L;
    L.p = B.tp(pl);
    L.d = dl ? nrm(B.td(*dl)) : V3{0, 0, 0};
    L.c = c; L.r = r; L.i = inten; L.thr = thr; L.cone = dl ? cone : -2; L.fl = fl; L.pri = pri ? pri : 1;
    LIGHTS.push_back(L);
}
static void winLight(const V3& pl, const V3& n, const Em& e, double r = 3.5, double inten = .8) {
    if (e) light(pl, &n, e.c(), r, inten, e.thr, -.25, e.fl, 1);
}
static Rect wRect(double cx, double cz, int rot, double x0, double z0, double x1, double z1) {
    double a = rot * M_PI / 2;
    double c = jsround(std::cos(a)), s = jsround(std::sin(a));
    double X[4], Z[4];
    const double pts[4][2] = {{x0, z0}, {x1, z0}, {x1, z1}, {x0, z1}};
    for (int i = 0; i < 4; i++) { X[i] = cx + pts[i][0] * c + pts[i][1] * s; Z[i] = cz - pts[i][0] * s + pts[i][1] * c; }
    return {*std::min_element(X, X + 4), *std::min_element(Z, Z + 4), *std::max_element(X, X + 4), *std::max_element(Z, Z + 4)};
}
static std::array<double, 2> wPt(double cx, double cz, int rot, double x, double z) {
    double a = rot * M_PI / 2;
    double c = jsround(std::cos(a)), s = jsround(std::sin(a));
    return {cx + x * c + z * s, cz - x * s + z * c};
}

/* ================= BUILDING SPECS ================= */
struct WTiles { int wall; std::vector<int> win; int stair, door; };
static WTiles WT(const std::string& w) {
    if (w == "panel") return {TL::PANEL, TL::PANEL_W, TL::PANEL_ST, TL::DOOR_P};
    if (w == "brick") return {TL::BRICK, TL::BRICK_W, TL::BRICK_ST, TL::DOOR_B};
    return {TL::STUCCO, TL::STUCCO_W, TL::STUCCO_W[0], TL::DOOR_S};
}
static std::set<int> cols(int n, int p, int o) { std::set<int> s; for (int j = 0; j < n; j++) if (j % p == o) s.insert(j); return s; }
struct Mural { int k; V3 pal[4]; V3 base; };
static Mural mkMural(Rng& R) {
    Mural m;
    m.k = R.integer(0, 2);
    for (int i = 0; i < 4; i++) m.pal[i] = R.pick(ACC);
    m.base = {.96, .95, .92};
    return m;
}
static V3 muralC(const Mural& m, double fx, double fy) {
    const V3* P = m.pal;
    if (m.k == 0) {
        double dx = fx - .5, dy = fy - .42, r = std::hypot(dx, dy * 1.4);
        if (fy < .28) return ((int)(fy * 14 + std::sin(fx * 8))) % 2 ? P[2] : P[3];
        if (r < .17) return P[0];
        double a = std::atan2(dy, dx);
        return ((int)(a / (M_PI / 7) + 14)) % 2 ? P[1] : m.base;
    }
    if (m.k == 1) { double v = std::sin(fx * 7 + fy * 11) * .5 + .5; return v > .67 ? P[0] : v > .33 ? P[1] : P[2]; }
    double q = std::fabs(fx - .5) * 3 + std::fabs(std::fmod(fy * 2.5, 1.0) - .5) * 3;
    return ((int)q) % 2 ? P[0] : m.base;
}

struct Spec {
    std::string type;
    double bw = 3.2, plinth = .7, fh = 2.7;
    std::string roof = "flat";
    bool cornice = false, shops = false, elev = false, spire = false;
    std::string accent = "none";
    bool hasMural = false;
    Mural mural{};
    double glz = .5;
    std::string balStyle = "rib", endWin = "none";
    bool hasBal[4] = {false, false, false, false};
    std::set<int> balCols[4];
    int balF[2] = {1, 99};
    std::vector<int> entr[4];
    int floors = 0;
    std::string wall;
    int bays = 0;
    double d = 0;
    V3 tint{}, roofTint{}, balTint{}, acc{}, plinthTint{}, endTint{}, sloganCol{};
    bool hasRoofTint = false, hasEndTint = false, hasSloganCol = false, hasSloganEm = false, sloganBack = false;
    std::string slogan;
    double sloganPx = 0;
    Em sloganEm;
    int endTile = -1, doorTile = -1;
    bool dormers = false, shopsAll = false, blankUpper = false, noSigns = false;
    std::vector<int> winTiles;
    double w = 0, H = 0;
    int endBays = 0, entrSide = 0;
    std::set<int> accCols;
};
static void setBal(Spec& S, int i, std::set<int> s) { S.hasBal[i] = true; S.balCols[i] = std::move(s); }

static Spec specFor(const std::string& type, Rng& R, int maxBays = 0) {
    Spec S;
    S.type = type;
    if (type == "khrush") {
        S.floors = R.chance(.15) ? 4 : 5; S.wall = R.chance(.5) ? "panel" : "brick"; S.bays = R.integer(12, 19); S.d = 11.4;
        S.tint = S.wall == "brick" ? R.pick(BRICKT) : R.pick(PANELT);
        if (S.wall == "brick" && R.chance(.5)) { S.roof = "gable"; S.roofTint = R.pick(SLATE); S.hasRoofTint = true; }
        S.balStyle = R.pick(std::vector<std::string>{"rib", "rail"}); S.glz = .55; S.balTint = R.pick(BALP);
        S.endWin = S.wall == "brick" ? "mid" : "none";
        setBal(S, 2, cols(S.bays, 2, 1)); if (R.chance(.4)) setBal(S, 0, cols(S.bays, 4, 1));
        if (S.wall == "panel" && R.chance(.3)) { S.mural = mkMural(R); S.hasMural = true; }
    } else if (type == "panel9") {
        S.floors = R.pick(std::vector<int>{9, 9, 9, 10, 12, 12, 14, 16}); S.fh = 2.8; S.wall = "panel"; S.bays = R.integer(16, 32); S.d = 12.6; S.elev = true;
        S.tint = R.pick(PANELT); S.balStyle = R.pick(std::vector<std::string>{"rib", "rib", "rail"}); S.glz = .65; S.balTint = R.pick(BALP);
        { int p = R.pick(std::vector<int>{2, 3}); int o = R.integer(0, 1); setBal(S, 2, cols(S.bays, p, o)); }
        if (R.chance(.5)) setBal(S, 0, cols(S.bays, 4, 2));
        S.accent = R.pick(std::vector<std::string>{"none", "cols", "top", "cols"}); S.acc = R.pick(ACC);
        if (R.chance(.45)) { S.mural = mkMural(R); S.hasMural = true; }
        S.shops = R.chance(.25);
        if (R.chance(.35)) S.slogan = R.pick(SLOGANS);
    } else if (type == "tower") {
        S.floors = R.integer(12, 17); S.fh = 2.8; S.wall = "panel"; S.bays = 5; S.d = 16; S.elev = true;
        S.tint = R.pick(PANELT); S.balStyle = R.pick(std::vector<std::string>{"rib", "rail"}); S.glz = .6; S.balTint = R.pick(BALP);
        for (int i = 0; i < 4; i++) setBal(S, i, {0, 4});
        S.endWin = "all"; S.accent = R.pick(std::vector<std::string>{"bands", "cols", "none"}); S.acc = R.pick(ACC);
    } else if (type == "stalinka") {
        S.floors = R.integer(5, 8); S.fh = 3.3; S.plinth = .9; S.wall = "stucco"; S.bays = R.integer(10, 17); S.d = 14;
        S.tint = R.pick(STUCT); S.roof = "hip"; S.roofTint = R.pick(ROOFT); S.hasRoofTint = true; S.cornice = true; S.shops = R.chance(.7);
        S.balStyle = "rib"; S.glz = .12; S.balTint = S.tint;
        int c = S.bays / 2;
        if (R.chance(.5)) setBal(S, 0, {c - 1, c, c + 1}); else setBal(S, 0, {c});
        S.balF[0] = 2; S.balF[1] = 3; S.endWin = "all"; S.spire = S.floors >= 7 && R.chance(.45);
    } else if (type == "platten") {
        S.floors = R.pick(std::vector<int>{5, 6, 6, 6, 11}); S.fh = 2.8; S.wall = "panel"; S.bays = R.integer(14, 24); S.d = 12;
        S.tint = R.pick(PANELT); S.accent = R.chance(.6) ? "bands" : "cols"; S.acc = R.pick(ACC);
        S.balStyle = "rib"; S.glz = .25; S.balTint = R.chance(.6) ? S.acc : R.pick(BALP);
        setBal(S, 2, cols(S.bays, 2, 0)); S.elev = S.floors > 6;
        if (R.chance(.35)) { S.mural = mkMural(R); S.hasMural = true; }
    } else if (type == "tenement") {
        S.floors = R.integer(3, 6); S.fh = 3.4; S.plinth = .8; S.wall = "stucco"; S.bays = R.integer(4, 7); S.d = 12;
        S.tint = R.pick(TENT); S.roof = "gable"; S.roofTint = R.pick(Pal{{.72, .38, .3}, {.6, .62, .66}, {.45, .58, .48}, {.62, .4, .34}}); S.hasRoofTint = true;
        S.cornice = true; S.shops = R.chance(.55);
        S.balStyle = "rail"; S.glz = .05; S.balTint = S.tint;
        if (R.chance(.5)) setBal(S, 0, {1, S.bays - 2});
        S.balF[0] = 1; S.balF[1] = 2;
        S.endTile = TL::BRICK; S.endTint = {.86, .6, .5}; S.hasEndTint = true; S.doorTile = TL::ARCH; S.dormers = R.chance(.55);
    } else if (type == "school") {
        S.floors = 3; S.fh = 3.4; S.plinth = .8; S.wall = "panel"; S.bays = R.integer(12, 18); S.d = 14; S.tint = R.pick(PANELT); S.winTiles = {TL::SCHW};
        S.accent = "top"; S.acc = R.pick(ACC); S.endWin = "mid"; S.slogan = "СКОЛА " + std::to_string(R.integer(2, 48)); S.sloganPx = .4;
        S.sloganCol = {.95, .95, .9}; S.hasSloganCol = true; S.sloganEm = em({1, .95, .8}, .05); S.hasSloganEm = true;
    } else if (type == "univermag") {
        S.floors = 2; S.fh = 4; S.plinth = .3; S.wall = "panel"; S.bays = R.integer(8, 11); S.d = 19.2; S.tint = R.pick(PANELT); S.shops = true; S.shopsAll = true; S.blankUpper = true;
        S.slogan = R.pick(std::vector<std::string>{"ИЛСИОПА", "УНИВЕРСИОП", "БИАТОРГ"}); S.sloganPx = .42; S.endWin = "all"; S.accent = "top"; S.acc = R.pick(ACC); S.noSigns = true;
    }
    if (maxBays && S.bays > maxBays) S.bays = maxBays;
    bool single = type == "tower" || type == "tenement" || type == "school" || type == "univermag";
    double div = type == "khrush" ? 4.5 : type == "panel9" ? 4 : type == "stalinka" ? 5 : type == "platten" ? 4 : 99;
    int sections = single ? 1 : std::max(type == "stalinka" ? 1 : 2, (int)jsround(S.bays / div));
    S.w = S.bays * S.bw; S.endBays = std::max(1, (int)jsround(S.d / S.bw));
    S.entrSide = type == "stalinka" ? 2 : 0;
    if (type == "tower") S.entr[0] = {2};
    else for (int i = 0; i < sections; i++) S.entr[S.entrSide].push_back((int)std::floor((i + .5) * S.bays / sections));
    if (S.accent == "cols") {
        if (type == "tower") S.accCols = {2};
        else S.accCols = std::set<int>(S.entr[S.entrSide].begin(), S.entr[S.entrSide].end());
    }
    if (!S.hasRoofTint) { S.roofTint = {1, 1, 1}; S.hasRoofTint = true; }
    S.plinthTint = S.wall == "stucco" ? mul(S.tint, 1.45) : V3{1, 1, 1};
    S.H = S.plinth + S.floors * S.fh;
    return S;
}
static V3 tintAt(const Spec& S, int si, int j, int f, int nb, bool main) {
    if (!main) {
        if (S.hasMural && S.endWin == "none") return muralC(S.mural, (j + .5) / nb, (f + .5) / S.floors);
        return S.tint;
    }
    if (S.accent == "cols" && S.accCols.count(j) && (si == S.entrSide || S.type == "tower")) return S.acc;
    if (S.accent == "bands" && f % 2 == 1) return mix(S.acc, {1, 1, 1}, .35);
    if (S.accent == "top" && f == S.floors - 1) return S.acc;
    return S.tint;
}

/* ================= BUILDING GENERATOR ================= */
struct Side {
    V3 o, u, n;
    double len, bw = 0;
    V3 P(double a, double y, double e) const { return {o[0] + u[0] * a + n[0] * e, y, o[2] + u[2] * a + n[2] * e}; }
};
static void balcony(const Side& sd, double a0, double a1, double y0, const Spec& S, Rng& R, bool top) {
    const V3 &U = sd.u, &N = sd.n;
    double dep = (S.type == "stalinka" || S.type == "tenement") ? .8 : 1.1, x0 = a0 + .16, x1 = a1 - .16, L = x1 - x0;
    B.obox(sd.P(x0, y0 - .16, 0), U, N, L, .16, dep, TL::CONC, {.85, .85, .83}, 9);
    bool glzd = R.chance(S.glz);
    V3 pc = R.chance(.18) ? R.pick(BALP) : S.balTint;
    double ph = 1.0;
    auto q = [&](int t, const V3& col, bool dbl, double ya, double yb) {
        V3 f[4] = {sd.P(x0, ya, dep), sd.P(x1, ya, dep), sd.P(x1, yb, dep), sd.P(x0, yb, dep)};
        V3 l[4] = {sd.P(x0, ya, 0), sd.P(x0, ya, dep), sd.P(x0, yb, dep), sd.P(x0, yb, 0)};
        V3 r[4] = {sd.P(x1, ya, dep), sd.P(x1, ya, 0), sd.P(x1, yb, 0), sd.P(x1, yb, dep)};
        for (V3* g : {f, l, r}) {
            if (dbl) B.dquad(g[0], g[1], g[2], g[3], t, col);
            else B.quad(g[0], g[1], g[2], g[3], t, col);
        }
    };
    if (S.balStyle == "rail" && !glzd) q(TL::RAIL, {.5, .5, .52}, true, y0, y0 + ph);
    else q(TL::RIB, pc, false, y0, y0 + ph);
    if (glzd) {
        q(R.chance(.6) ? TL::GLZW : TL::GLZB, {1, 1, 1}, false, y0 + ph, y0 + S.fh - .18);
        if (top) B.obox(sd.P(x0 - .05, y0 + S.fh - .18, 0), U, N, L + .1, .14, dep + .05, TL::METAL, {.75, .75, .75}, 9);
    } else if (R.chance(.22)) {
        int t = R.chance(.5) ? TL::WOOD : TL::PAINT;
        V3 c = R.pick(BALP);
        B.obox(sd.P(x0 + .1, y0, .1), U, N, .6, 1.5, .55, t, c, 9);
    } else if (R.chance(.2)) {
        double zl = dep * .6;
        B.obox(sd.P(x0 + .1, y0 + 1.9, zl), U, N, L - .2, .03, .03, TL::METAL, {.3, .3, .3}, 9);
        for (int k = 0; k < 3; k++) {
            if (R.chance(.3)) continue;
            double cx = x0 + .3 + k * (L - .6) / 3;
            double w = R.range(.35, .6);
            double h = R.range(.4, .8);
            B.dquad(sd.P(cx, y0 + 1.9 - h, zl + .02), sd.P(cx + w, y0 + 1.9 - h, zl + .02), sd.P(cx + w, y0 + 1.9, zl + .02), sd.P(cx, y0 + 1.9, zl + .02), TL::PAINT, R.pick(CLOTH));
        }
    }
}
static void entrance(const Side& sd, double ac, const Spec& S, Rng& R) {
    const V3 &U = sd.u, &N = sd.n;
    B.obox(sd.P(ac - 1.3, 0, 0), U, N, 2.6, .28, 1.4, TL::CONC, {.8, .8, .78}, 9);
    if (S.wall != "stucco") {
        double cy = std::min(2.55, (S.plinth + S.fh) * .68);
        B.obox(sd.P(ac - 1.5, cy, 0), U, N, 3, .16, 1.7, TL::CONC, {.85, .85, .83}, 9);
        Em le = R.chance(.8) ? em({1, .85, .55}, .02 + R() * .1) : Em();
        Opt o; o.em = le;
        B.obox(sd.P(ac - .15, cy - .25, 0), U, N, .3, .2, .2, TL::LAMP, {1, 1, 1}, 9, o);
        V3 dn{0, -1, 0};
        if (le) light(sd.P(ac, cy - .35, .3), &dn, le.c(), 8, 1.3, le.thr, .05, 0, 2);
    }
    if (S.wall != "stucco" && R.chance(.55)) B.obox(sd.P(ac + 1.8, 0, 1.2), U, N, 1.6, .45, .45, TL::WOOD, {1, 1, 1}, 9);
}
static void slogan(const Spec& S, double W, double hd, double H, int side) {
    double px = S.sloganPx ? S.sloganPx : .5, tw = B.textW(S.slogan, px);
    if (tw > W - 2) return;
    double y0 = H + .5, y = y0 + .5, s = side, z = s * (hd - .8);
    B.box(-tw / 2 - .3, y0, z - .1, tw / 2 + .3, y, z + .1, TL::METAL, {.35, .35, .38}, 99);
    int n = std::max(2, (int)jsround(tw / 4));
    for (int i = 0; i <= n; i++) { double x = -tw / 2 + tw * i / n; B.beam({x, y0, z - s * .4}, {x, y + px * 6, z - s * .2}, .08, TL::METAL, {.35, .35, .38}); }
    Em se = S.hasSloganEm ? S.sloganEm : em({1, .14, .08}, .03);
    B.text(S.slogan, {-s * tw / 2, y, z + s * .12}, {s, 0, 0}, {0, 1, 0}, px, S.hasSloganCol ? S.sloganCol : V3{1, .32, .26}, se);
    light({0, y + px * 3.5, z + s * 1.2}, nullptr, se.c(), std::max(10.0, tw * .6), 1.4, se.thr, 0, 0, 2);
}
static Opt topOpt(int top, const V3* topCol = nullptr, const Em& e = Em()) {
    Opt o; o.top = top; if (topCol) { o.hasTopCol = true; o.topCol = *topCol; } o.em = e; return o;
}
static Opt skipOpt(const char* s) { Opt o; o.skip = s; return o; }
static Opt emOpt(const Em& e) { Opt o; o.em = e; return o; }

static void genBuilding(const Spec& S, Rng& R) {
    double W = S.w, D = S.d, hw = W / 2, hd = D / 2, H = S.H;
    WTiles wt = WT(S.wall);
    if (!S.winTiles.empty()) wt.win = S.winTiles;
    if (S.doorTile >= 0) wt.door = S.doorTile;
    Side SD[4] = {{{-hw, 0, hd}, {1, 0, 0}, {0, 0, 1}, W}, {{hw, 0, hd}, {0, 0, -1}, {1, 0, 0}, D}, {{hw, 0, -hd}, {-1, 0, 0}, {0, 0, -1}, W}, {{-hw, 0, -hd}, {0, 0, 1}, {-1, 0, 0}, D}};
    BuildingRec rec;
    rec.type = S.type; rec.wall = S.wall; rec.ox = B.ox; rec.oz = B.oz; rec.cs = B.cs; rec.sn = B.sn;
    rec.w = W; rec.d = D; rec.plinth = S.plinth; rec.fh = S.fh; rec.H = H; rec.floors = S.floors; rec.bays = S.bays;
    rec.endBays = S.endBays; rec.entrSide = S.entrSide; rec.entr = S.entr[S.entrSide]; rec.tint = S.tint;
    for (int si = 0; si < 4; si++) {
        Side& sd = SD[si];
        bool main = si % 2 == 0 || S.type == "tower";
        int nb = si % 2 == 0 ? S.bays : S.endBays;
        double bw = sd.len / nb;
        sd.bw = bw;
        const std::vector<int>& ent = S.entr[si];
        const std::set<int>* bal = S.hasBal[si] ? &S.balCols[si] : nullptr;
        for (int j = 0; j < nb; j++) {
            double a0 = j * bw, a1 = a0 + bw;
            bool isE = std::find(ent.begin(), ent.end(), j) != ent.end();
            bool winOK = main || S.endWin == "all" || (S.endWin == "mid" && j > 0 && j < nb - 1);
            if (!isE && S.plinth > 0) B.quad(sd.P(a0, 0, 0), sd.P(a1, 0, 0), sd.P(a1, S.plinth, 0), sd.P(a0, S.plinth, 0), TL::PLINTH, S.plinthTint);
            for (int f = 0; f < S.floors; f++) {
                double y0 = S.plinth + f * S.fh;
                const double y1 = y0 + S.fh;
                int t;
                Em e;
                V3 col = tintAt(S, si, j, f, nb, main);
                if (isE && f == 0) { y0 = 0; t = wt.door; if (R.chance(.7)) e = em(STAIRC, R() * .2); }
                else if (isE) { t = wt.stair; if (R.chance(.85)) e = em(STAIRC, R() * .25); }
                else if (S.shops && f == 0 && (si == 0 || S.shopsAll)) { t = TL::SHOP; col = {1, 1, 1}; e = em(SHOPC, .03 + R() * .12); }
                else if (!winOK) { t = S.endTile >= 0 ? S.endTile : wt.wall; if (S.hasEndTint) col = S.endTint; }
                else if (S.blankUpper && f > 0) { t = wt.wall; }
                else if (S.type == "stalinka" && f == 0) { t = TL::PLINTH_W; col = S.plinthTint; e = winEm(R); }
                else { t = wt.win[R.integer(0, (int)wt.win.size() - 1)]; e = winEm(R); }
                V3 jc = jit(col, R);
                B.quad(sd.P(a0, y0, 0), sd.P(a1, y0, 0), sd.P(a1, y1, 0), sd.P(a0, y1, 0), t, jc, e);
                rec.cells[si].push_back({t, jc, e, y0, y1});
                if (e) {
                    double ac = (a0 + a1) / 2;
                    if (t == TL::SHOP) light(sd.P(ac, 1.8, 1.4), &sd.n, e.c(), 9, 1, e.thr, -.3, 0, 2);
                    else if (isE && f == 0) light(sd.P(ac, 2.2, .9), &sd.n, e.c(), 4, .5, e.thr, -.3, 0, 1);
                    else if (isE) winLight(sd.P(ac, y0 + S.fh * .55, .6), sd.n, e, 3.5, .6);
                    else winLight(sd.P(ac, y0 + S.fh * .5, .7), sd.n, e);
                }
                bool hasBal = bal && bal->count(j) && f >= S.balF[0] && f <= S.balF[1];
                if (!isE && winOK && !hasBal && f > 0 && S.wall != "stucco" && !S.blankUpper) {
                    if (R.chance(.05)) B.obox(sd.P(a0 + bw * .5 - .4, y0 + .2, 0), sd.u, sd.n, .8, .5, .35, TL::PAINT, {.92, .92, .9}, 9);
                    else if (R.chance(.025)) {
                        B.obox(sd.P(a0 + bw * .78, y0 + 1.7, 0), sd.u, sd.n, .08, .08, .35, TL::METAL, {.6, .6, .6}, 9);
                        B.obox(sd.P(a0 + bw * .78 - .3, y0 + 1.45, .35), sd.u, sd.n, .66, .66, .08, TL::PAINT, {.9, .9, .92}, 9);
                    }
                }
            }
            if (bal && bal->count(j) && !isE)
                for (int f = std::max(1, S.balF[0]); f <= std::min(S.floors - 1, S.balF[1]); f++) balcony(sd, a0, a1, S.plinth + f * S.fh, S, R, f == S.floors - 1);
            if (isE) entrance(sd, (a0 + a1) / 2, S, R);
            if (S.shops && !S.noSigns && si == 0 && !isE && j % 3 == 1) {
                const std::string& word = R.pick(SHOPW);
                double px = .075, tw = B.textW(word, px), ac = (a0 + a1) / 2, y = S.plinth + S.fh - .74;
                B.obox(sd.P(ac - tw / 2 - .2, y - .06, 0), sd.u, sd.n, tw + .4, .65, .12, TL::PAINT, mul(R.pick(ACC), .75), 99);
                B.text(word, sd.P(ac - tw / 2, y + .02, .135), sd.u, {0, 1, 0}, px, {1, 1, .92}, em({1, .95, .85}, .04));
            }
        }
    }
    RECS.push_back(rec);
    // ---- roof
    V3 ct = mul(S.tint, .92);
    if (S.roof == "flat") {
        B.wall({-hw, H, hd}, {1, 0, 0}, {0, 0, -1}, W, D, TL::TAR, {1, 1, 1}, 4);
        double pt = .25, ph = .5;
        Opt po = topOpt(TL::CONC, &ct);
        B.box(-hw, H, hd - pt, hw, H + ph, hd, TL::CONC, ct, TS(4, 9), po);
        B.box(-hw, H, -hd, hw, H + ph, -hd + pt, TL::CONC, ct, TS(4, 9), po);
        B.box(hw - pt, H, -hd + pt, hw, H + ph, hd - pt, TL::CONC, ct, TS(4, 9), po);
        B.box(-hw, H, -hd + pt, -hw + pt, H + ph, hd - pt, TL::CONC, ct, TS(4, 9), po);
        const Side& sd = SD[S.entrSide];
        for (int j : S.entr[S.entrSide]) {
            V3 c = sd.P((j + .5) * sd.bw, H, -3.5);
            V3 one{1, 1, 1}, sev{.7, .7, .7};
            if (S.elev) B.box(c[0] - 1.7, H, c[2] - 1.7, c[0] + 1.7, H + 2.6, c[2] + 1.7, wt.wall, S.tint, 3.2, topOpt(TL::TAR, &one));
            else B.box(c[0] - .7, H, c[2] - .7, c[0] + .7, H + 1.1, c[2] + .7, TL::CONC, ct, 9, topOpt(TL::METAL, &sev));
        }
        for (int i = R.integer(2, 5); i > 0; i--) {
            double x = R.range(-hw + 1.5, hw - 1.5);
            double z = R.range(-hd + 1.5, hd - 1.5);
            B.box(x - .3, H, z - .3, x + .3, H + .9, z + .3, TL::BRICK, {.85, .5, .4}, 9);
        }
        if (S.type != "univermag")
            for (int i = R.integer(1, 5); i > 0; i--) {
                double x = R.range(-hw + 1, hw - 1);
                double z = R.range(-hd + 1, hd - 1);
                double h = R.range(2.5, 4.5);
                V3 ac{.45, .45, .48};
                B.box(x - .05, H, z - .05, x + .05, H + h, z + .05, TL::METAL, ac, 99);
                for (int k = 0, n = R.integer(2, 3); k < n; k++) {
                    double y = H + h - .3 - k * .55, l = R.range(1, 2) * (1 - k * .2);
                    if (R.chance(.5)) B.box(x - l / 2, y, z - .03, x + l / 2, y + .05, z + .03, TL::METAL, ac, 99);
                    else B.box(x - .03, y, z - l / 2, x + .03, y + .05, z + l / 2, TL::METAL, ac, 99);
                }
            }
        if (S.floors >= 12) {
            V3 lc{1, .6, .6};
            B.box(hw - .6, H + ph, hd - .6, hw - .3, H + ph + .3, hd - .3, TL::LAMP, lc, 99, emOpt(em(REDL, 0)));
            B.box(-hw + .3, H + ph, -hd + .3, -hw + .6, H + ph + .3, -hd + .6, TL::LAMP, lc, 99, emOpt(em(REDL, 0)));
            light({hw - .45, H + ph + .6, hd - .45}, nullptr, REDL, 5, .8, 0, 0, 0, 2);
            light({-hw + .45, H + ph + .6, -hd + .45}, nullptr, REDL, 5, .8, 0, 0, 0, 2);
        }
        if (!S.slogan.empty()) slogan(S, W, hd, H, S.sloganBack ? -1 : 1);
    } else if (S.roof == "gable") {
        double e = .4, rh = D * .3, yR = H + rh;
        int nu = std::max(1, (int)jsround(W / 3));
        int gt = S.endTile >= 0 ? S.endTile : wt.wall;
        V3 gc = S.hasEndTint ? S.endTint : S.tint;
        B.surf({-hw - e, H, hd + e}, {hw + e, H, hd + e}, {hw + e, yR, 0}, {-hw - e, yR, 0}, nu, 2, TL::RMET, S.roofTint);
        B.surf({hw + e, H, -hd - e}, {-hw - e, H, -hd - e}, {-hw - e, yR, 0}, {hw + e, yR, 0}, nu, 2, TL::RMET, S.roofTint);
        B.surf({hw, H, hd}, {hw, H, -hd}, {hw, yR, 0}, {hw, yR, 0}, 3, 2, gt, gc);
        B.surf({-hw, H, -hd}, {-hw, H, hd}, {-hw, yR, 0}, {-hw, yR, 0}, 3, 2, gt, gc);
        for (int i = R.integer(2, 4); i > 0; i--) {
            double x = R.range(-hw + 2, hw - 2);
            double z = R.range(-1.5, 1.5);
            B.box(x - .35, yR - 1, z - .35, x + .35, yR + .8, z + .35, TL::BRICK, {.86, .5, .4}, 9);
        }
        if (S.dormers) {
            int n = std::max(1, S.bays / 2);
            for (int i = 0; i < n; i++) {
                double x = -hw + (i + .5) * W / n;
                Em de = winEm(R);
                B.box(x - .8, H, hd - 2.2, x + .8, H + 1.7, hd - .9, wt.win[0], S.tint, TS(1.6, 1.7), topOpt(TL::RMET, &S.roofTint, de));
                winLight({x, H + 1, hd - .3}, {0, 0, 1}, de, 3.5, .6);
            }
        }
    } else {
        hipRoof(-hw, -hd, hw, hd, H, D * .32, .5, TL::RMET, S.roofTint);
        double rx = hw - hd, rh = D * .32;
        for (int i = R.integer(3, 7); i > 0; i--) {
            double x = R.range(-rx, rx);
            double z = R.range(-hd * .45, hd * .45);
            double y = H + rh * (1 - std::fabs(z) / hd);
            B.box(x - .4, y - .8, z - .4, x + .4, y + 1.3, z + .4, TL::BRICK, {.86, .5, .4}, 9);
        }
    }
    if (S.cornice) {
        double c = .45, y0 = H - .75;
        auto co = [&](const char* sk) { Opt o = topOpt(TL::CONC, &S.tint); o.bottom = true; o.skip = sk; return o; };
        B.box(-hw - c, y0, hd, hw + c, H, hd + c, TL::CORN, S.tint, TS(3, 9), co("b"));
        B.box(-hw - c, y0, -hd - c, hw + c, H, -hd, TL::CORN, S.tint, TS(3, 9), co("f"));
        if (S.type != "tenement") {
            B.box(hw, y0, -hd, hw + c, H, hd, TL::CORN, S.tint, TS(3, 9), co("lfb"));
            B.box(-hw - c, y0, -hd, -hw, H, hd, TL::CORN, S.tint, TS(3, 9), co("rfb"));
        }
        double by = S.plinth + S.fh, b = .15;
        V3 bc = mul(S.tint, .95);
        B.box(-hw, by - .12, hd, hw, by + .12, hd + b, TL::CONC, bc, TS(4, 9), skipOpt("b"));
        B.box(-hw, by - .12, -hd - b, hw, by + .12, -hd, TL::CONC, bc, TS(4, 9), skipOpt("f"));
    }
    if (S.spire) {
        double tw = std::min(W * .28, 9.6), td = std::min(D * .75, 9.6), hx = tw / 2, hz = td / 2;
        int tf = 2;
        struct Face { V3 o, u; double len; };
        Face faces[4] = {{{-hx, 0, hz}, {1, 0, 0}, tw}, {{hx, 0, hz}, {0, 0, -1}, td}, {{hx, 0, -hz}, {-1, 0, 0}, tw}, {{-hx, 0, -hz}, {0, 0, 1}, td}};
        for (auto& fc : faces) {
            int nb = std::max(1, (int)jsround(fc.len / 3.2));
            double bw = fc.len / nb;
            auto p = [&](double a, double y) { return V3{fc.o[0] + fc.u[0] * a, y, fc.o[2] + fc.u[2] * a}; };
            for (int j = 0; j < nb; j++)
                for (int f = 0; f < tf; f++) {
                    double y0 = H + f * S.fh, a0 = j * bw;
                    int t = wt.win[R.integer(0, 3)];
                    V3 jc = jit(S.tint, R);
                    Em we = winEm(R);
                    B.quad(p(a0, y0), p(a0 + bw, y0), p(a0 + bw, y0 + S.fh), p(a0, y0 + S.fh), t, jc, we);
                }
        }
        double yt = H + tf * S.fh;
        Opt o = topOpt(TL::CONC, &S.tint); o.bottom = true;
        B.box(-hx - .3, yt - .5, -hz - .3, hx + .3, yt, hz + .3, TL::CORN, S.tint, TS(3, 9), o);
        pyramid(-hx, -hz, hx, hz, yt, yt + 7, TL::RMET, S.roofTint);
        B.box(-.15, yt + 6.5, -.15, .15, yt + 13, .15, TL::PAINT, {1, .82, .35}, 99);
        B.box(-.5, yt + 13, -.12, .5, yt + 14, .12, TL::LAMP, {1, .4, .3}, 99, emOpt(em(REDL, 0)));
        B.box(-.12, yt + 13, -.5, .12, yt + 14, .5, TL::LAMP, {1, .4, .3}, 99, emOpt(em(REDL, 0)));
        light({0, yt + 13.5, 0}, nullptr, REDL, 9, 1.2, 0, 0, 0, 2);
    }
}

/* ================= PROPS ================= */
static void tree(double x, double z, Rng& R, std::string kind = "") {
    if (kind.empty()) kind = R.pick(std::vector<std::string>{"birch", "birch", "linden", "linden", "poplar", "spruce"});
    B.xfA(x, z, R() * 6.28);
    addCol({x - .3, z - .3, x + .3, z + .3});
    if (kind == "spruce") {
        double h = R.range(7, 13), r = h * .28;
        Opt o = skipOpt("t");
        B.box(-.18, 0, -.18, .18, h * .3, .18, TL::BARK, {1, 1, 1}, TS(9, 3), o);
        B.up = true;
        V3 c{.9 + R() * .15, 1, .9};
        for (int k = 0; k < 3; k++) {
            double a = k * M_PI / 3, dx = std::cos(a) * r, dz = std::sin(a) * r;
            B.dquad({-dx, .8, -dz}, {dx, .8, dz}, {dx, h, dz}, {-dx, h, -dz}, TL::SPRUCE, c);
        }
        B.up = false;
        return;
    }
    double th, tw, cr, cb, ctop;
    int tile = TL::BARK, lt = TL::LEAF;
    if (kind == "birch") { th = R.range(6, 9); tw = .28; cr = R.range(2, 2.8); cb = th * .35; ctop = th + 1.5; tile = TL::BIRCH; lt = TL::LEAF2; }
    else if (kind == "poplar") { th = R.range(10, 15); tw = .45; cr = R.range(1.4, 2); cb = 2.5; ctop = th + 1; }
    else { th = R.range(4.5, 7); tw = .4; cr = R.range(3, 4.3); cb = 2.2; ctop = th + 2.5; }
    B.box(-tw / 2, 0, -tw / 2, tw / 2, th, tw / 2, tile, {1, 1, 1}, TS(9, 3), skipOpt("t"));
    double aut = R();
    V3 col;
    if (SEASON == "winter") { lt = TL::BRANCH; col = {1, 1, 1}; B.snowK = .5; }
    else if (SEASON == "autumn") {
        if (aut < .3) col = {1.25, 1.02, .42};
        else if (aut < .45) col = {1.3, .78, .34};
        else { double a = .95 + R() * .2; double b = .95 + R() * .1; col = {a, b, .8}; }
    } else { double a = .85 + R() * .15; double b = .95 + R() * .15; double c = .85 + R() * .1; col = {a, b, c}; }
    B.up = true;
    int n = kind == "poplar" ? 2 : 3;
    for (int k = 0; k < n; k++) {
        double a = k * M_PI / n, dx = std::cos(a) * cr, dz = std::sin(a) * cr;
        B.dquad({-dx, cb, -dz}, {dx, cb, dz}, {dx, ctop, dz}, {-dx, ctop, -dz}, lt, col);
    }
    B.up = false;
    B.snowK = 1;
}
static void lamp(double x, double z, double a) {
    B.xfA(x, z, a);
    V3 mc{.55, .55, .58};
    B.box(-.1, 0, -.1, .1, 7.5, .1, TL::METAL, mc, 99);
    B.box(-.06, 7.3, 0, .06, 7.45, 1.6, TL::METAL, mc, 99);
    B.box(-.25, 7.05, 1.3, .25, 7.3, 2, TL::LAMP, {1, 1, 1}, 99, emOpt(em(SODIUM, .02)));
    V3 dir{0, -1, .3};
    light({0, 6.9, 1.65}, &dir, SODIUM, 19, 2.1, .02, .18, 0, 3);
    addCol({x - .2, z - .2, x + .2, z + .2});
}
static void garages(double cx, double cz, int rot, int n, Rng& R) {
    B.xf(cx, cz, rot);
    double w = n * 3.4;
    for (int k = 0; k < n; k++) {
        double x0 = -w / 2 + k * 3.4;
        std::string sk = "f";
        if (k > 0) sk += 'l';
        if (k < n - 1) sk += 'r';
        V3 one{1, 1, 1};
        Opt o = topOpt(TL::TAR, &one); o.skip = sk;
        B.box(x0, 0, -3, x0 + 3.4, 2.5, 3, TL::BRICK, {.9, .88, .85}, 3, o);
        B.quad({x0, 0, 3}, {x0 + 3.4, 0, 3}, {x0 + 3.4, 2.5, 3}, {x0, 2.5, 3}, TL::GARAGE, R.pick(GARC));
    }
    addCol(wRect(cx, cz, rot, -w / 2, -3, w / 2, 3));
}
static void playground(double cx, double cz, Rng&) {
    B.xf(cx, cz, 0);
    V3 red{.85, .2, .18}, wh{.95, .95, .95};
    double rx = -4;
    for (int k = 0; k < 3; k++) B.box(rx - .8, k * 1.2, -.8, rx + .8, (k + 1) * 1.2, .8, TL::PAINT, k % 2 ? wh : red, 9, skipOpt("t"));
    pyramid(rx - .8, -.8, rx + .8, .8, 3.6, 6.2, TL::PAINT, red);
    B.box(rx - 1.6, 0, -.06, rx + 1.6, 1.4, .06, TL::PAINT, {.3, .45, .8}, 9);
    B.box(rx - .06, 0, -1.6, rx + .06, 1.4, 1.6, TL::PAINT, {.3, .45, .8}, 9);
    double sx = 3;
    V3 bl{.3, .45, .75};
    B.box(sx - 1.7, 0, -.1, sx - 1.5, 2.6, .1, TL::METAL, bl, 9); B.box(sx + 1.5, 0, -.1, sx + 1.7, 2.6, .1, TL::METAL, bl, 9); B.box(sx - 1.7, 2.6, -.1, sx + 1.7, 2.8, .1, TL::METAL, bl, 9);
    B.box(sx - .5, .6, -.02, sx - .46, 2.6, .02, TL::METAL, {.3, .3, .3}, 99); B.box(sx + .46, .6, -.02, sx + .5, 2.6, .02, TL::METAL, {.3, .3, .3}, 99);
    B.box(sx - .6, .5, -.2, sx + .6, .6, .2, TL::WOOD, {1, 1, 1}, 9);
    double mx = 2, mz = -4;
    B.box(mx - .1, 0, mz - .1, mx + .1, 2.2, mz + .1, TL::WOOD, {1, 1, 1}, 9);
    pyramid(mx - 1.5, mz - 1.5, mx + 1.5, mz + 1.5, 2.2, 3, TL::PAINT, red);
    B.box(mx - 1.5, 0, mz - 1.5, mx + 1.5, .3, mz - 1.3, TL::WOOD, {1, 1, 1}, 9); B.box(mx - 1.5, 0, mz + 1.3, mx + 1.5, .3, mz + 1.5, TL::WOOD, {1, 1, 1}, 9);
    B.box(-7, 0, 4.6, -5, .45, 5.05, TL::WOOD, {1, 1, 1}, 9); B.box(5, 0, 4.6, 7, .45, 5.05, TL::WOOD, {1, 1, 1}, 9);
    addCol({cx + rx - 1.7, cz - 1.7, cx + rx + 1.7, cz + 1.7});
    addCol({cx + sx - 1.8, cz - .3, cx + sx + 1.8, cz + .3});
}
static void laundry(double cx, double cz, int rot, Rng& R) {
    B.xf(cx, cz, rot);
    V3 mc{.4, .42, .45};
    for (double x : {-3.5, 3.5}) { B.box(x - .06, 0, -.06, x + .06, 2.2, .06, TL::METAL, mc, 9); B.box(x - .05, 2.1, -.8, x + .05, 2.2, .8, TL::METAL, mc, 9); }
    for (double z : {-.7, 0.0, .7}) {
        B.box(-3.5, 2.12, z - .015, 3.5, 2.15, z + .015, TL::METAL, {.25, .25, .25}, 99);
        double x = -3.2;
        while (x < 2.8) {
            if (R.chance(.55)) {
                double w = R.range(.4, .9);
                double h = R.range(.5, 1.1);
                B.dquad({x, 2.12 - h, z}, {x + w, 2.12 - h, z}, {x + w, 2.12, z}, {x, 2.12, z}, TL::PAINT, R.pick(CLOTH));
                x += w + .1;
            } else x += .5;
        }
    }
}
static void kiosk(double cx, double cz, int rot, Rng& R) {
    B.xf(cx, cz, rot);
    V3 c = R.pick(Pal{{.35, .55, .8}, {.9, .8, .4}, {.85, .35, .3}, {.5, .7, .5}});
    B.box(-1.5, 0, -1.2, 1.5, 2.4, 1.2, TL::PAINT, c, 9, skipOpt("ft"));
    B.quad({-1.5, 0, 1.2}, {1.5, 0, 1.2}, {1.5, 2.4, 1.2}, {-1.5, 2.4, 1.2}, TL::SHOP, {1, 1, 1}, em(SHOPC, .05));
    V3 fwd{0, 0, 1};
    light({0, 1.6, 2.2}, &fwd, SHOPC, 7, 1, .05, -.3, 0, 2);
    B.box(-1.7, 2.4, -1.4, 1.7, 2.55, 1.9, TL::METAL, c, 9);
    std::string w = R.pick(std::vector<std::string>{"ЛЕАННА", "НУАЧТА", "ТОБАК", "БЛАХИ", "КВАС"});
    double px = .07, tw = B.textW(w, px);
    B.text(w, {-tw / 2, 2.6, 1.3}, {1, 0, 0}, {0, 1, 0}, px, {1, .95, .6}, em({1, .9, .5}, .05));
    addCol(wRect(cx, cz, rot, -1.7, -1.4, 1.7, 1.9));
    Rect r = wRect(cx, cz, rot, -1.5, -1.2, 1.5, 1.2);
    WOBJ.push_back({sim::Obj::Kiosk, {r[0], 0, r[1]}, {r[2], 2.4, r[3]}});
}
static void busStop(double cx, double cz, int rot, Rng& R) {
    B.xf(cx, cz, rot);
    Mural m = mkMural(R);
    V3 g{.8, .8, .78};
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 3; j++) {
            V3 c = muralC(m, (i + .5) / 8, (j + .5) / 3);
            B.quad({-2 + i * .5, .3 + j * .75, -.7}, {-1.5 + i * .5, .3 + j * .75, -.7}, {-1.5 + i * .5, 1.05 + j * .75, -.7}, {-2 + i * .5, 1.05 + j * .75, -.7}, TL::PAINT, c);
        }
    B.box(-2.1, 0, -1, 2.1, 2.6, -.7, TL::CONC, g, TS(4, 9), skipOpt("f"));
    B.box(-2.1, 0, -.7, -1.9, 2.6, .9, TL::CONC, g, 9); B.box(1.9, 0, -.7, 2.1, 2.6, .9, TL::CONC, g, 9);
    B.box(-2.4, 2.6, -1.2, 2.4, 2.9, 1.3, TL::CONC, g, 9); B.box(-1.6, 0, -.65, 1.6, .45, -.25, TL::WOOD, {1, 1, 1}, 9);
    B.box(2.5, 0, .9, 2.6, 2.6, 1, TL::METAL, {.5, .5, .52}, 99); B.box(2.3, 2.2, .94, 2.8, 2.7, .96, TL::PAINT, {.95, .85, .3}, 99);
    addCol(wRect(cx, cz, rot, -2.4, -1.2, 2.4, 1));
    Rect r = wRect(cx, cz, rot, -2.1, -1, 2.1, .9);
    WOBJ.push_back({sim::Obj::BusStop, {r[0], 0, r[1]}, {r[2], 2.6, r[3]}});
}
static void pole(double x, double z, double h) {
    B.xf(0, 0, 0);
    B.box(x - .12, 0, z - .12, x + .12, h, z + .12, TL::WOOD, {.75, .7, .65}, TS(9, 4));
    B.box(x - .9, h - .6, z - .07, x + .9, h - .45, z + .07, TL::WOOD, {.75, .7, .65}, 9);
    addCol({x - .2, z - .2, x + .2, z + .2});
}
static void wire(V3 p0, V3 p1, double sag) {
    const int n = 4;
    V3 pts[n + 1];
    for (int i = 0; i <= n; i++) { double t = (double)i / n; pts[i] = {p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t - sag * 4 * t * (1 - t), p0[2] + (p1[2] - p0[2]) * t}; }
    B.xf(0, 0, 0);
    for (int i = 0; i < n; i++) B.beam(pts[i], pts[i + 1], .05, TL::METAL, {.2, .2, .22});
}
static void haystack(double x, double z, Rng& R) {
    B.xf(0, 0, 0);
    double r = R.range(1.6, 2.4);
    B.lathe(x, 0, z, {{r, 0}, {r * 1.05, r * .8}, {r * .7, r * 1.5}, {.05, r * 2}}, 7, TL::STRAW, {1, 1, 1});
    addCol({x - r, z - r, x + r, z + r});
}

/* ================= CHURCH ================= */
static V3 lastChurchWc;
static Rect genChurch(Rng& R) {
    V3 wc = R.chance(.7) ? V3{.98, .97, .94} : R.pick(Pal{{.8, .88, .98}, {.98, .9, .7}, {.85, .95, .85}});
    lastChurchWc = wc;
    int dt = R.chance(.6) ? TL::GOLD : TL::DOMEB;
    V3 rc = R.pick(Pal{{.45, .6, .48}, {.7, .72, .75}, {.5, .6, .75}}), pc = mul(wc, .82);
    auto cEm = [&]() { return R.chance(.6) ? em({1, .75, .4}, .15 + R() * .5) : Em(); };
    const std::array<double, 5> L5[5] = {{5.6, -3, 1, 0, 0}, {5.6, 3, 1, 0, 0}, {-5.6, -3, -1, 0, 0}, {-5.6, 3, -1, 0, 0}, {0, -11.6, 0, 0, -1}};
    for (auto& q : L5) { Em e = cEm(); V3 d{q[2], q[3], q[4]}; if (e) light({q[0], 4, q[1]}, &d, e.c(), 7, .9, e.thr, -.3, 0, 1); }
    // nave
    B.box(-5.15, 0, -7.15, 5.15, .8, 7.15, TL::CONC, pc, TS(4, 9), skipOpt("t"));
    { Opt o = skipOpt("t"); o.em = cEm(); B.box(-5, .8, -7, 5, 9, 7, TL::CHWIN, wc, TS(3.4, 8.2), o); }
    { Opt o = topOpt(TL::CONC, &wc); o.bottom = true; B.box(-5.3, 8.6, -7.3, 5.3, 9, 7.3, TL::CORN, wc, TS(3, 9), o); }
    hipRoof(-5.3, -7.3, 5.3, 7.3, 9, 2.2, .2, TL::RMET, rc);
    // apse
    B.lathe(0, 0, -7, {{4, 0}, {4, 7.5}}, 4, TL::CHWIN, wc, cEm(), M_PI, M_PI * 2);
    B.lathe(0, 0, -7, {{4.3, 7.5}, {0, 9.8}}, 4, TL::RMET, rc, Em(), M_PI, M_PI * 2);
    // main drum + onion
    B.lathe(0, 9.5, 0, {{2.6, 0}, {2.6, 4.6}}, 8, TL::CHWIN, wc, cEm());
    B.lathe(0, 9.5, 0, {{2.9, 4.6}, {2.9, 5}}, 8, TL::CORN, wc);
    onion(0, 14.5, 0, 2.8, dt, {1, 1, 1});
    if (R.chance(.55))
        for (auto& p : {std::array<double, 2>{-3.2, -4.4}, {3.2, -4.4}, {-3.2, 4.4}, {3.2, 4.4}}) {
            B.lathe(p[0], 9.5, p[1], {{1.05, 0}, {1.05, 2.8}}, 6, TL::CHWIN, wc);
            onion(p[0], 12.3, p[1], 1.2, dt, {1, 1, 1});
        }
    // bell tower
    B.box(-2.6, 0, 7, 2.6, 10, 12.2, TL::CHWIN, wc, TS(2.6, 5), skipOpt("ft"));
    B.quad({-2.6, 0, 12.2}, {2.6, 0, 12.2}, {2.6, 4.8, 12.2}, {-2.6, 4.8, 12.2}, TL::DOOR_S, wc, em({1, .75, .4}, .1));
    { V3 f{0, 0, 1}; light({0, 3.4, 13.2}, &f, {1, .75, .4}, 8, 1, .1, -.3, 0, 2); }
    B.quad({-2.6, 4.8, 12.2}, {2.6, 4.8, 12.2}, {2.6, 10, 12.2}, {-2.6, 10, 12.2}, TL::CHWIN, wc, cEm());
    { Opt o = topOpt(TL::CONC, &wc); o.bottom = true; B.box(-2.9, 9.7, 6.7, 2.9, 10.1, 12.5, TL::CORN, wc, TS(3, 9), o); }
    B.box(-2, 10.1, 7.6, 2, 15, 11.6, TL::CHWIN, wc, TS(2, 4.9), skipOpt("t"));
    { Opt o = topOpt(TL::CONC, &wc); o.bottom = true; B.box(-2.3, 15, 7.3, 2.3, 15.4, 11.9, TL::CORN, wc, TS(3, 9), o); }
    if (R.chance(.5)) { pyramid(-2.1, 7.5, 2.1, 11.7, 15.4, 23, TL::RMET, rc); onion(0, 22.4, 9.6, .8, dt, {1, 1, 1}); }
    else { B.lathe(0, 15.4, 9.6, {{1.3, 0}, {1.3, 2}}, 6, TL::CHWIN, wc); onion(0, 17.4, 9.6, 1.5, dt, {1, 1, 1}); }
    return {-5.4, -11.2, 5.4, 12.4};
}
static void churchyard(double cx, double cz, int rot, Rng& R) {
    B.xf(cx, cz, rot);
    BuildingRec rec;
    rec.kind = 1; rec.type = "church"; rec.ox = B.ox; rec.oz = B.oz; rec.cs = B.cs; rec.sn = B.sn;
    Rect b = genChurch(R);
    addCol(wRect(cx, cz, rot, b[0], b[1], b[2], b[3]));
    rec.col = (int)COL.size() - 1; rec.wc = lastChurchWc;
    RECS.push_back(rec);
    V3 w{.92, .9, .86};
    double L = -12, Rr = 12, F = 18, Bk = -16;
    B.xf(cx, cz, rot);
    Opt tc = topOpt(TL::CONC);
    B.box(L, 0, Bk, Rr, 1.2, Bk + .4, TL::STUCCO, w, TS(4, 9), tc);
    B.box(L, 0, Bk, L + .4, 1.2, F, TL::STUCCO, w, TS(4, 9), tc); B.box(Rr - .4, 0, Bk, Rr, 1.2, F, TL::STUCCO, w, TS(4, 9), tc);
    B.box(L, 0, F - .4, -2, 1.2, F, TL::STUCCO, w, TS(4, 9), tc); B.box(2, 0, F - .4, Rr, 1.2, F, TL::STUCCO, w, TS(4, 9), tc);
    B.box(-2.4, 0, F - .5, -1.8, 3.2, F + .1, TL::STUCCO, w, 9); B.box(1.8, 0, F - .5, 2.4, 3.2, F + .1, TL::STUCCO, w, 9); B.box(-2.4, 3.2, F - .5, 2.4, 3.7, F + .1, TL::STUCCO, w, 9);
    onion(0, 3.7, F - .2, .35, TL::GOLD, {1, 1, 1});
    for (auto& r : {Rect{L, Bk, Rr, Bk + .4}, Rect{L, Bk, L + .4, F}, Rect{Rr - .4, Bk, Rr, F}, Rect{L, F - .4, -2, F}, Rect{2, F - .4, Rr, F}}) addCol(wRect(cx, cz, rot, r[0], r[1], r[2], r[3]));
    for (int i = 0; i < 5; i++) {
        double x = R.chance(.5) ? R.range(L + 2, -6.5) : R.range(6.5, Rr - 2);
        double z = R.range(Bk + 2, F - 3);
        auto p = wPt(cx, cz, rot, x, z);
        tree(p[0], p[1], R, R.pick(std::vector<std::string>{"birch", "linden", "spruce"}));
    }
}

/* ================= IZBA ================= */
struct Izba { double w, d, hw, hd, H, pz; Em win[5]; V3 logC; int winTile; };
static Izba genIzba(Rng& R) {
    double w = R.range(5.6, 7);
    double d = R.range(7, 9);
    double hw = w / 2, hd = d / 2, fb = .5;
    double H = fb + R.range(2.8, 3.2);
    V3 logC = R.chance(.5) ? V3{.8, .77, .75} : V3{1, .95, .9};
    int wt = R.pick(TL::IZW);
    Em izWin[5];
    B.box(-hw - .05, 0, -hd - .05, hw + .05, fb, hd + .05, TL::CONC, {.8, .78, .74}, 9, skipOpt("t"));
    double cw = w / 3;
    for (int i = 0; i < 3; i++) {
        Em e = winEm(R);
        izWin[i] = e;
        B.quad({-hw + i * cw, fb, hd}, {-hw + (i + 1) * cw, fb, hd}, {-hw + (i + 1) * cw, H, hd}, {-hw + i * cw, H, hd}, wt, logC, e);
        winLight({-hw + (i + .5) * cw, fb + 1.5, hd + .7}, {0, 0, 1}, e, 5, .8);
    }
    B.wall({hw, fb, -hd}, {-1, 0, 0}, {0, 1, 0}, w, H - fb, TL::LOG, logC, TS(2.4, 1.4));
    for (int sx : {1, -1}) {
        double cl = d / 3;
        for (int i = 0; i < 3; i++) {
            double za = sx > 0 ? hd - i * cl : -hd + i * cl, zb = sx > 0 ? za - cl : za + cl;
            int t = i == 1 ? wt : TL::LOG;
            Em e = t == wt ? winEm(R) : Em();
            if (t == wt) izWin[sx > 0 ? 3 : 4] = e;
            B.quad({sx * hw, fb, za}, {sx * hw, fb, zb}, {sx * hw, H, zb}, {sx * hw, H, za}, t, logC, e);
            winLight({sx * (hw + .7), fb + 1.5, (za + zb) / 2}, {(double)sx, 0, 0}, e, 5, .8);
        }
    }
    double e = .5, rh = w * .42, yR = H + rh;
    int rt = R.chance(.25) ? TL::WOOD : TL::RMET;
    V3 rc = rt == TL::WOOD ? V3{.75, .7, .65} : R.pick(Pal{{.7, .4, .3}, {.45, .6, .45}, {.6, .62, .66}, {.5, .55, .7}});
    B.surf({-hw - e, H - .2, -hd - e}, {-hw - e, H - .2, hd + e}, {0, yR, hd + e}, {0, yR, -hd - e}, 3, 2, rt, rc);
    B.surf({hw + e, H - .2, hd + e}, {hw + e, H - .2, -hd - e}, {0, yR, -hd - e}, {0, yR, hd + e}, 3, 2, rt, rc);
    V3 gc = R.chance(.6) ? R.pick(Pal{{.6, .75, .9}, {.75, .85, .65}, {.95, .85, .55}, {.9, .6, .5}}) : V3{.85, .8, .7};
    B.surf({-hw, H, hd}, {hw, H, hd}, {0, yR, hd}, {0, yR, hd}, 2, 2, TL::PLANK, gc);
    B.surf({hw, H, -hd}, {-hw, H, -hd}, {0, yR, -hd}, {0, yR, -hd}, 2, 2, TL::PLANK, gc);
    B.quad({-.45, H + .45, hd + .03}, {.45, H + .45, hd + .03}, {.45, H + 1.25, hd + .03}, {-.45, H + 1.25, hd + .03}, TL::CARWIN, {1, 1, 1});
    B.box(.6, H + rh * .4, -1, 1.3, yR + .9, -.3, TL::BRICK, {.86, .5, .4}, 9);
    double pz = -hd + 1.8;
    B.box(hw, 0, pz - 1.2, hw + 1.6, fb, pz + 1.2, TL::WOOD, {1, 1, 1}, 9);
    B.box(hw + 1.4, fb, pz - 1.1, hw + 1.55, H - .2, pz - .95, TL::WOOD, {1, 1, 1}, 9); B.box(hw + 1.4, fb, pz + .95, hw + 1.55, H - .2, pz + 1.1, TL::WOOD, {1, 1, 1}, 9);
    B.box(hw - .1, H - .25, pz - 1.4, hw + 1.8, H - .05, pz + 1.4, rt, rc, 9);
    B.quad({hw + .05, fb, pz + .6}, {hw + .05, fb, pz - .6}, {hw + .05, fb + 2, pz - .6}, {hw + .05, fb + 2, pz + .6}, TL::WOOD, {.8, .6, .45});
    return {w, d, hw, hd, H, pz, {izWin[0], izWin[1], izWin[2], izWin[3], izWin[4]}, logC, wt};
}
static void shed(double x, double z, int rot, Rng&) {
    B.xf(x, z, rot);
    V3 c{.8, .77, .75};
    B.box(-1.6, 0, -1.8, 1.6, 2.3, 1.8, TL::LOG, c, TS(2.4, 1.4), skipOpt("t"));
    B.surf({-2, 2.2, -2.1}, {-2, 2.2, 2.1}, {0, 3.4, 2.1}, {0, 3.4, -2.1}, 1, 1, TL::WOOD, {.7, .66, .62});
    B.surf({2, 2.2, 2.1}, {2, 2.2, -2.1}, {0, 3.4, -2.1}, {0, 3.4, 2.1}, 1, 1, TL::WOOD, {.7, .66, .62});
    B.surf({-1.6, 2.3, 1.8}, {1.6, 2.3, 1.8}, {0, 3.4, 1.8}, {0, 3.4, 1.8}, 1, 1, TL::PLANK, {.8, .8, .75});
    B.surf({1.6, 2.3, -1.8}, {-1.6, 2.3, -1.8}, {0, 3.4, -1.8}, {0, 3.4, -1.8}, 1, 1, TL::PLANK, {.8, .8, .75});
    addCol(wRect(x, z, rot, -1.7, -1.9, 1.7, 1.9));
}
static void well(double x, double z) {
    B.xf(x, z, 0);
    V3 dk{.2, .2, .22};
    B.box(-.7, 0, -.7, .7, .9, .7, TL::LOG, {.8, .77, .75}, TS(1.4, .9), topOpt(TL::METAL, &dk));
    B.box(-.75, 0, -.08, -.62, 2.2, .08, TL::WOOD, {1, 1, 1}, 9); B.box(.62, 0, -.08, .75, 2.2, .08, TL::WOOD, {1, 1, 1}, 9);
    B.surf({-.95, 1.9, -.9}, {-.95, 1.9, .9}, {0, 2.6, .9}, {0, 2.6, -.9}, 1, 1, TL::WOOD, {.7, .66, .62});
    B.surf({.95, 1.9, .9}, {.95, 1.9, -.9}, {0, 2.6, -.9}, {0, 2.6, .9}, 1, 1, TL::WOOD, {.7, .66, .62});
    addCol({x - .8, z - .8, x + .8, z + .8});
}
static void fence(double x0, double z0, double x1, double z1, const V3& col) {
    double L = std::hypot(x1 - x0, z1 - z0);
    int n = std::max(1, (int)jsround(L / 3));
    B.xf(0, 0, 0);
    for (int i = 0; i < n; i++) {
        double a = (double)i / n, b = (double)(i + 1) / n;
        double p0 = x0 + (x1 - x0) * a, p1 = z0 + (z1 - z0) * a, q0 = x0 + (x1 - x0) * b, q1 = z0 + (z1 - z0) * b;
        B.dquad({p0, 0, p1}, {q0, 0, q1}, {q0, 1.4, q1}, {p0, 1.4, p1}, TL::PLANK, col);
    }
    addCol({std::min(x0, x1) - .1, std::min(z0, z1) - .1, std::max(x0, x1) + .1, std::max(z0, z1) + .1});
}

/* ================= GROUND ================= */
struct Skirt { int tile; V3 col; };
struct Ground {
    double h, c;
    int n;
    std::vector<uint8_t> t;
    Ground(double half, double cell) : h(half), c(cell), n((int)jsround(half * 2 / cell)), t((size_t)n * n, 0) {}
    int type(double x, double z) const {
        int i = (int)std::floor((x + h) / c), j = (int)std::floor((z + h) / c);
        return (i < 0 || j < 0 || i >= n || j >= n) ? 255 : t[j * n + i];
    }
    void mark(const Rect& r, int ty) {
        int i0 = std::max(0, (int)std::floor((r[0] + h) / c + .001)), i1 = std::min(n - 1, (int)std::ceil((r[2] + h) / c - .001) - 1);
        int j0 = std::max(0, (int)std::floor((r[1] + h) / c + .001)), j1 = std::min(n - 1, (int)std::ceil((r[3] + h) / c - .001) - 1);
        for (int j = j0; j <= j1; j++)
            for (int i = i0; i <= i1; i++) t[j * n + i] = (uint8_t)ty;
    }
    void build(Rng& R, const Skirt* skirt) {
        B.xf(0, 0, 0);
        static const int tiles[10] = {TL::GRASS, TL::ASPH, TL::DIRT, TL::SAND, TL::CONC, TL::ROADL, TL::ROADL, TL::COBBLE, TL::RAILS, TL::GARDEN};
        for (int j = 0; j < n; j++)
            for (int i = 0; i < n; i++) {
                int ty = t[j * n + i];
                if (ty == 0 && R.chance(.05)) ty = 2;
                double x = -h + i * c, z = -h + j * c, k = .92 + R() * .1;
                B.quad({x, 0, z + c}, {x + c, 0, z + c}, {x + c, 0, z}, {x, 0, z}, tiles[ty], {k, k, k}, Em(), (ty == 5 || ty == 8) ? 90 : 0);
            }
        if (skirt) {
            B.fogK = .4;
            double F = 620, y = -.05;
            int tt = skirt->tile;
            const V3& col = skirt->col;
            B.wall({-F, y, F}, {1, 0, 0}, {0, 0, -1}, 2 * F, F - h, tt, col, 48);
            B.wall({-F, y, -h}, {1, 0, 0}, {0, 0, -1}, 2 * F, F - h, tt, col, 48);
            B.wall({h, y, h}, {1, 0, 0}, {0, 0, -1}, F - h, 2 * h, tt, col, 48);
            B.wall({-F, y, h}, {1, 0, 0}, {0, 0, -1}, F - h, 2 * h, tt, col, 48);
            B.fogK = 1;
        }
    }
};
struct Placed { Spec S; double cx, cz; int rot; bool field = false; };
static void siteMarks(Ground& G, const Placed& e, bool connect) {
    const Spec& S = e.S;
    double cx = e.cx, cz = e.cz;
    int rot = e.rot;
    double hw = S.w / 2, hd = S.d / 2;
    bool back = S.type == "stalinka";
    double sg = back ? -1 : 1;
    double zA = sg * (hd + 5), zB = sg * (hd + 9);
    G.mark(wRect(cx, cz, rot, -hw - 6, std::min(zA, zB), hw + 6, std::max(zA, zB)), 1);
    double bw = S.w / S.bays;
    for (int j : S.entr[S.entrSide]) {
        double ax = back ? hw - (j + .5) * bw : -hw + (j + .5) * bw, z0 = sg * hd, z1 = sg * (hd + 5);
        G.mark(wRect(cx, cz, rot, ax - 1.2, std::min(z0, z1), ax + 1.2, std::max(z0, z1)), 4);
    }
    if (back) G.mark(wRect(cx, cz, rot, -hw, hd, hw, hd + 4), 4);
    if (connect) {
        auto e0 = wPt(cx, cz, rot, -hw - 6, sg * (hd + 7)), e1 = wPt(cx, cz, rot, hw + 6, sg * (hd + 7));
        auto m = [](const std::array<double, 2>& p) { return std::max(std::fabs(p[0]), std::fabs(p[1])); };
        auto E = m(e0) > m(e1) ? e0 : e1;
        auto sgn = [](double v) { return (double)((v > 0) - (v < 0)); };
        if (std::fabs(E[0]) > std::fabs(E[1])) { double s = sgn(E[0]) * 100; G.mark({std::min(E[0], s), E[1] - 2, std::max(E[0], s), E[1] + 2}, 1); }
        else { double s = sgn(E[1]) * 100; G.mark({E[0] - 2, std::min(E[1], s), E[0] + 2, std::max(E[1], s)}, 1); }
    }
}
static void izbaPlot(double px, int side, Rng& R, Ground& G) {
    V3 fc = R.pick(FENCEC);
    double fz = side * 6, bz = side * 38, gx = px + R.range(-6, 6);
    fence(px - 11, fz, gx - 1.3, fz, fc); fence(gx + 1.3, fz, px + 11, fz, fc);
    fence(px - 11, fz, px - 11, bz, fc); fence(px + 11, fz, px + 11, bz, fc); fence(px - 11, bz, px + 11, bz, fc);
    B.xf(0, 0, 0);
    B.box(gx - 1.45, 0, fz - .1, gx - 1.3, 1.7, fz + .1, TL::WOOD, {1, 1, 1}, 9); B.box(gx + 1.3, 0, fz - .1, gx + 1.45, 1.7, fz + .1, TL::WOOD, {1, 1, 1}, 9);
    double hx = px + R.range(-3, 3), dz = side * 14.2;
    int rot = side > 0 ? 2 : 0;
    B.xf(hx, dz, rot);
    BuildingRec rec;
    rec.kind = 2; rec.type = "izba"; rec.ox = B.ox; rec.oz = B.oz; rec.cs = B.cs; rec.sn = B.sn;
    Izba h = genIzba(R);
    addCol(wRect(hx, dz, rot, -h.hw - .1, -h.hd - .1, h.hw + 1.7, h.hd + .1));
    rec.col = (int)COL.size() - 1; rec.ihw = h.hw; rec.ihd = h.hd; rec.ifb = .5; rec.iH = h.H; rec.ipz = h.pz; rec.wc = h.logC; rec.izTile = h.winTile;
    for (int i = 0; i < 5; i++) rec.izWin[i] = h.win[i];
    RECS.push_back(rec);
    double hf = dz - side * h.hd;
    G.mark({gx - 1.2, std::min(fz, hf), gx + 1.2, std::max(fz, hf)}, 2);
    double g0 = dz + side * (h.hd + 3), g1 = side * 35;
    G.mark({px - 9, std::min(g0, g1), px + 9, std::max(g0, g1)}, 9);
    double sx = px + (R.chance(.5) ? -7 : 7);
    double sz = side * R.range(28, 33);
    shed(sx, sz, R.integer(0, 3), R);
    if (R.chance(.4)) well(px + (sx > px ? -7 : 7), side * R.range(20, 26));
    for (int i = R.integer(1, 3); i > 0; i--) {
        double x = px + R.range(-9, 9);
        double z = side * R.range(7.5, 9.5);
        tree(x, z, R, R.pick(std::vector<std::string>{"birch", "linden", "linden"}));
    }
}

/* ================= SKYLINES ================= */
static void farBlock(double w, double d, double h, const V3& tint, Rng& R, const std::vector<int>& tiles) {
    double hw = w / 2, hd = d / 2;
    TS ts(5, 5.6);
    struct F { V3 o, u; double l; };
    F faces[4] = {{{-hw, 0, hd}, {1, 0, 0}, w}, {{hw, 0, hd}, {0, 0, -1}, d}, {{hw, 0, -hd}, {-1, 0, 0}, w}, {{-hw, 0, -hd}, {0, 0, 1}, d}};
    for (auto& f : faces)
        B.wallF(f.o, f.u, {0, 1, 0}, f.l, h, [&]() { return R.pick(tiles); }, tint, ts, [&]() {
            if (!R.chance(.35)) return Em();
            V3 c = R.pick(WARM);
            return em(c, R());
        });
    B.wall({-hw, h, hd}, {1, 0, 0}, {0, 0, -1}, w, d, TL::TAR, {1, 1, 1}, 20);
}
static void chimneyStack(double x, double z, double h) {
    B.xf(0, 0, 0);
    int n = 8;
    for (int i = 0; i < n; i++) {
        double r0 = 4.6 - 2.0 * i / n, r1 = 4.6 - 2.0 * (i + 1) / n;
        B.lathe(x, h * i / n, z, {{r0, 0}, {r1, h / n}}, 8, TL::PAINT, i % 2 ? V3{.95, .95, .93} : V3{.8, .22, .18});
    }
    B.box(x - .4, h, z - .4, x + .4, h + .8, z + .4, TL::LAMP, {1, .5, .5}, 99, emOpt(em(REDL, 0)));
    SMOKE.push_back({x, h + 1, z});
}
static void skyline(const std::string& kind, Rng& R) {
    B.fogK = .38;
    if (kind == "village") {
        for (int i = 0; i < 260; i++) {
            double a = R() * M_PI * 2, r = R.range(170, 300);
            tree(std::cos(a) * r, std::sin(a) * r, R, R.chance(.65) ? "spruce" : "birch");
        }
        double a = R() * M_PI * 2;
        B.xf(0, 0, 0);
        B.lathe(std::cos(a) * 260, 0, std::sin(a) * 260, {{1.2, 0}, {1.2, 22}, {4, 26}, {4, 31}, {0, 34}}, 8, TL::PAINT, {.85, .3, .25});
        for (int i = -6; i <= 6; i++) {
            double x = i * 70, z = -330;
            B.beam({x - 5, 0, z}, {x, 40, z}, .8, TL::METAL, {.5, .5, .52});
            B.beam({x + 5, 0, z}, {x, 40, z}, .8, TL::METAL, {.5, .5, .52});
            B.beam({x - 9, 34, z}, {x + 9, 34, z}, .6, TL::METAL, {.5, .5, .52});
            if (i < 6)
                for (double dx : {-8.0, 8.0}) B.beam({x + dx, 33.5, z}, {x + 70 + dx, 33.5, z}, .15, TL::METAL, {.3, .3, .32});
        }
        B.fogK = 1;
        return;
    }
    bool old = kind == "old";
    for (double a = 0; a < M_PI * 2; a += R.range(.07, .13)) {
        double r = R.range(270, 420), x = std::cos(a) * r, z = std::sin(a) * r;
        B.xfA(x, z, std::atan2(-std::cos(a), -std::sin(a)));
        if (old) {
            double w = R.range(20, 40);
            double h = R.range(12, 22);
            V3 tint = R.pick(TENT);
            farBlock(w, 14, h, tint, R, TL::STUCCO_W);
            B.surf({-w / 2, h, 7}, {w / 2, h, 7}, {w / 2, h + 4, 0}, {-w / 2, h + 4, 0}, 2, 1, TL::RMET, {.7, .4, .32});
            B.surf({w / 2, h, -7}, {-w / 2, h, -7}, {-w / 2, h + 4, 0}, {w / 2, h + 4, 0}, 2, 1, TL::RMET, {.7, .4, .32});
            if (R.chance(.12)) { B.lathe(0, h, 0, {{3, 0}, {3, 5}}, 8, TL::STUCCO, {.95, .95, .92}); onion(0, h + 5, 0, 3.2, TL::GOLD, {1, 1, 1}); }
        } else {
            double w = R.range(18, 55);
            double d = R.range(12, 16);
            double h = R.chance(.3) ? R.range(38, 60) : R.range(15, 32);
            V3 tint = R.pick(PANELT);
            farBlock(w, d, h, tint, R, TL::PANEL_W);
        }
    }
    double a0 = R() * M_PI * 2, cx = std::cos(a0) * 380, cz = std::sin(a0) * 380;
    if (!old) {
        B.xfA(cx, cz, -a0);
        farBlock(60, 30, 24, {.8, .8, .78}, R, {TL::PANEL});
        chimneyStack(cx + 14 * std::sin(a0), cz - 14 * std::cos(a0) * -1, 110);
        chimneyStack(cx - 14 * std::sin(a0), cz + 14 * std::cos(a0) * -1, 120);
        double a1 = a0 + R.range(1.5, 4.5), tx = std::cos(a1) * 460, tz = std::sin(a1) * 460;
        B.xf(0, 0, 0);
        B.lathe(tx, 0, tz, {{9, 0}, {4, 24}, {2.6, 130}, {6, 136}, {6, 146}, {2.4, 150}, {1.5, 210}, {.3, 250}}, 8, TL::CONC, {.92, .92, .9});
        B.box(tx - 1, 250, tz - 1, tx + 1, 252, tz + 1, TL::LAMP, {1, .5, .5}, 99, emOpt(em(REDL, 0)));
        B.lathe(tx, 136, tz, {{6.05, 1}, {6.05, 9}}, 8, TL::LAMP, {.6, .62, .7}, em({1, .85, .6}, .05));
    } else {
        B.xfA(cx, cz, -a0);
        V3 t{.95, .9, .8};
        B.box(-22, 0, -22, 22, 60, 22, TL::STUCCO_W[0], t, TS(5, 5.6), topOpt(TL::TAR));
        B.box(-13, 60, -13, 13, 92, 13, TL::STUCCO_W[1], t, TS(5, 5.6), topOpt(TL::TAR));
        B.box(-7, 92, -7, 7, 112, 7, TL::STUCCO_W[2], t, TS(4, 5), skipOpt("t"));
        pyramid(-7, -7, 7, 7, 112, 128, TL::GOLD, {1, 1, 1});
        B.box(-.5, 128, -.5, .5, 150, .5, TL::GOLD, {1, 1, 1}, 99);
        B.box(-2, 150, -.4, 2, 154, .4, TL::LAMP, {1, .4, .3}, 99, emOpt(em(REDL, 0)));
    }
    B.fogK = 1;
}

/* ================= SCENES ================= */
static bool ov(const Rect& a, const Rect& b) { return a[0] < b[2] && a[2] > b[0] && a[1] < b[3] && a[3] > b[1]; }

static Info genDistrict(Rng& R, const std::string& style) {
    Ground G(132, 4);
    std::vector<Rect> placed;
    std::vector<Placed> list;
    const double LIM = 95;
    auto fits = [&](const Rect& r) {
        if (!(r[0] >= -LIM && r[2] <= LIM && r[1] >= -LIM && r[3] <= LIM)) return false;
        for (auto& p : placed) if (ov(r, p)) return false;
        return true;
    };
    auto tryPlace = [&](const Spec& S, double cx, double cz, int rot, double extra = 0) {
        bool st = S.type == "stalinka";
        double fm = (st ? 3 : 11) + extra, bm = st ? 11 : 5;
        Rect r = wRect(cx, cz, rot, -S.w / 2 - 4, -S.d / 2 - bm, S.w / 2 + 4, S.d / 2 + fm);
        if (!fits(r)) return false;
        placed.push_back(r);
        list.push_back({S, cx, cz, rot});
        return true;
    };
    struct Edge { int inR, outR; char ax; double sg; };
    const Edge EDGES[4] = {{2, 0, 'x', 1}, {3, 1, 'z', 1}, {0, 2, 'x', -1}, {1, 3, 'z', -1}};
    int stalEdge = style == "mixed" && R.chance(.6) ? R.integer(0, 3) : -1;
    for (int ei = 0; ei < 4; ei++) {
        const Edge& E = EDGES[ei];
        for (int rep = 0; rep < 2; rep++) {
            std::string type = style != "mixed" ? style : (ei == stalEdge ? "stalinka" : R.pick(std::vector<std::string>{"panel9", "panel9", "khrush", "platten"}));
            Spec S = specFor(type, R, 44);
            bool st = type == "stalinka";
            if (!st && !S.slogan.empty()) S.sloganBack = true;
            double dp = E.sg * (st ? 92 - S.d / 2 : 90 - S.d / 2 - R.range(0, 4));
            double lim = 90 - S.w / 2;
            if (lim < 0) continue;
            for (int t = 0; t < 6; t++) {
                double al = jsround(R.range(-lim, lim) / 2) * 2;
                if (tryPlace(S, E.ax == 'x' ? al : dp, E.ax == 'x' ? dp : al, st ? E.outR : E.inR)) break;
            }
        }
    }
    if (style == "mixed") {
        for (const char* tp : {"school", "univermag"}) {
            bool school = std::string(tp) == "school";
            if (R.chance(school ? .6 : .45))
                for (int t = 0; t < 30; t++) {
                    Spec S = specFor(tp, R);
                    double x = jsround(R.range(-64, 64) / 2) * 2;
                    double z = jsround(R.range(-64, 64) / 2) * 2;
                    int rot = R.integer(0, 3);
                    if (tryPlace(S, x, z, rot, school ? 14 : 0)) { if (school) list.back().field = true; break; }
                }
        }
    }
    for (int t = 0; t < 80 && list.size() < 16; t++) {
        std::string type = style != "mixed" ? style : R.pick(std::vector<std::string>{"tower", "tower", "khrush", "khrush", "panel9", "platten"});
        Spec S = specFor(type, R, type == "panel9" ? 22 : 18);
        double x = jsround(R.range(-72, 72) / 2) * 2;
        double z = jsround(R.range(-72, 72) / 2) * 2;
        int rot = R.integer(0, 3);
        tryPlace(S, x, z, rot);
    }
    G.mark({-112, 100, 112, 112}, 1); G.mark({-112, -112, 112, -100}, 1); G.mark({100, -112, 112, 112}, 1); G.mark({-112, -112, -100, 112}, 1);
    for (auto& e : list) {
        B.xf(e.cx, e.cz, e.rot);
        genBuilding(e.S, R);
        addCol(wRect(e.cx, e.cz, e.rot, -e.S.w / 2 - .2, -e.S.d / 2 - .2, e.S.w / 2 + .2, e.S.d / 2 + .2));
        RECS.back().col = (int)COL.size() - 1;
        siteMarks(G, e, true);
        R.chance(.6); // the JS passes an unused R.chance(.6) argument to siteMarks; keep the stream aligned
        if (e.field) {
            double hd = e.S.d / 2;
            G.mark(wRect(e.cx, e.cz, e.rot, -14, hd + 11, 14, hd + 25), 2);
            B.xf(e.cx, e.cz, e.rot);
            for (double x : {-13.0, 13.0}) {
                B.box(x - .08, 0, hd + 16, x + .08, 2.2, hd + 16.2, TL::PAINT, {1, 1, 1}, 99);
                B.box(x - .08, 0, hd + 19.8, x + .08, 2.2, hd + 20, TL::PAINT, {1, 1, 1}, 99);
                B.box(x - .08, 2.1, hd + 16, x + .08, 2.2, hd + 20, TL::PAINT, {1, 1, 1}, 99);
            }
        }
    }
    G.mark({-104, 104, 104, 108}, 5); G.mark({-104, -108, 104, -104}, 5); G.mark({104, -104, 108, 104}, 6); G.mark({-108, -104, -104, 104}, 6);
    G.mark({-100, 96, 100, 100}, 4); G.mark({-100, -100, 100, -96}, 4); G.mark({96, -100, 100, 100}, 4); G.mark({-100, -100, -96, 100}, 4);
    G.mark({-116, 112, 116, 116}, 4); G.mark({-116, -116, 116, -112}, 4); G.mark({112, -116, 116, 116}, 4); G.mark({-116, -116, -112, 116}, 4);
    for (int g = R.integer(1, 2); g > 0; g--)
        for (int t = 0; t < 40; t++) {
            int n = R.integer(5, 12);
            double w = n * 3.4;
            int rot = R.integer(0, 3);
            double cx = jsround(R.range(-80, 80));
            double cz = jsround(R.range(-80, 80));
            Rect r = wRect(cx, cz, rot, -w / 2 - 2, -5, w / 2 + 2, 9);
            if (fits(r)) { placed.push_back(r); garages(cx, cz, rot, n, R); G.mark(wRect(cx, cz, rot, -w / 2, 3, w / 2, 8), 1); break; }
        }
    for (int t = 0; t < 50; t++) {
        double cx = jsround(R.range(-75, 75));
        double cz = jsround(R.range(-75, 75));
        Rect r{cx - 10, cz - 8, cx + 10, cz + 8};
        if (fits(r)) { placed.push_back(r); G.mark({cx - 8, cz - 6, cx + 8, cz + 6}, 3); playground(cx, cz, R); break; }
    }
    for (int l = R.integer(1, 2); l > 0; l--)
        for (int t = 0; t < 40; t++) {
            double cx = jsround(R.range(-80, 80));
            double cz = jsround(R.range(-80, 80));
            Rect r{cx - 5, cz - 5, cx + 5, cz + 5};
            if (fits(r)) { placed.push_back(r); laundry(cx, cz, R.integer(0, 1), R); break; }
        }
    for (int t = 0; t < 30; t++) {
        double sx = R.chance(.5) ? 1 : -1;
        double sz = R.chance(.5) ? 1 : -1;
        double cx = sx * R.range(40, 86), cz = sz * 92.5;
        Rect r{cx - 2.5, cz - 2.5, cx + 2.5, cz + 2.5};
        if (fits(r)) { placed.push_back(r); kiosk(cx, cz, sz > 0 ? 0 : 2, R); break; }
    }
    busStop(R.range(-60, 60), 97.6, 0, R);
    busStop(-97.6, R.range(-60, 60), 3, R);
    for (int a = -84; a <= 84; a += 28) { lamp(a, 98, 0); lamp(a, -98, M_PI); lamp(98, a, M_PI / 2); lamp(-98, a, -M_PI / 2); }
    int nt = 0;
    for (int i = 0; i < 1500 && nt < 190; i++) {
        double x = R.range(-94, 94);
        double z = R.range(-94, 94);
        if (G.type(x, z) != 0) continue;
        bool hit = false;
        for (auto& r : placed) if (x > r[0] - 1 && x < r[2] + 1 && z > r[1] - 1 && z < r[3] + 1) { hit = true; break; }
        if (hit) continue;
        tree(x, z, R);
        nt++;
    }
    for (int a = -122; a <= 122; a += 8) {
        const double P[4][2] = {{(double)a, 121}, {(double)a, -121}, {121, (double)a}, {-121, (double)a}};
        for (auto& p : P)
            if (R.chance(.8)) {
                double x = p[0] + R.range(-1, 1);
                double z = p[1] + R.range(-1, 1);
                tree(x, z, R, R.chance(.7) ? "poplar" : "linden");
            }
    }
    Skirt sk{TL::GRASS, {.85, .85, .82}};
    G.build(R, &sk);
    skyline("city", R);
    return {(int)list.size(), {0, 6, 0}, 215, "Mikrorayon · " + std::to_string(list.size()) + " blocks", {0, 97, 0}, "city", 136};
}
static Info genOld(Rng& R) {
    Ground G(132, 4);
    const double BX[3] = {-72, 0, 72}, BZ[2] = {-32, 32}, hx = 28, hz = 20, d = 12;
    int count = 0;
    G.mark({-132, -132, 132, 132}, 7);
    std::vector<std::array<double, 2>> blocks;
    for (double bz : BZ) for (double bx : BX) blocks.push_back({bx, bz});
    int cb = R.chance(.8) ? R.integer(0, 5) : -1;
    for (auto& b : blocks) { double bx = b[0], bz = b[1]; G.mark({bx - hx - 4, bz - hz - 4, bx + hx + 4, bz + hz + 4}, 4); G.mark({bx - hx, bz - hz, bx + hx, bz + hz}, 0); }
    G.mark({-132, -8, 132, 8}, 1); G.mark({-132, -4, 132, 4}, 8);
    for (int bi = 0; bi < (int)blocks.size(); bi++) {
        double bx = blocks[bi][0], bz = blocks[bi][1];
        if (bi == cb) {
            G.mark({bx - hx, bz - hz, bx + hx, bz + hz}, 7);
            G.mark({bx - hx + 3, bz - hz + 3, bx + hx - 3, bz + hz - 3}, 0);
            churchyard(bx, bz + (bz > 0 ? 2 : -2), bz > 0 ? 2 : 0, R);
            count++;
            for (double x : {-22.0, 22.0}) for (double z : {-14.0, 14.0}) tree(bx + x, bz + z, R, "linden");
            continue;
        }
        struct Row { char ax; int rot; double fix, a0, a1; };
        const Row rows[4] = {{'x', 0, bz + hz - d / 2, bx - hx, bx + hx}, {'x', 2, bz - hz + d / 2, bx - hx, bx + hx}, {'z', 1, bx + hx - d / 2, bz - hz + d, bz + hz - d}, {'z', 3, bx - hx + d / 2, bz - hz + d, bz + hz - d}};
        for (auto& row : rows) {
            int bays = (int)std::floor((row.a1 - row.a0) / 3.2);
            double a = row.a0 + ((row.a1 - row.a0) - bays * 3.2) / 2;
            std::vector<int> segs;
            while (bays > 0) {
                int n = std::min(bays, R.integer(4, 7));
                if (bays - n > 0 && bays - n < 4) n = bays;
                segs.push_back(n);
                bays -= n;
            }
            for (int k = 0; k < (int)segs.size(); k++) {
                int n = segs[k];
                Spec S = specFor("tenement", R);
                S.bays = n; S.w = n * 3.2; S.entr[0] = {n / 2};
                if (row.ax == 'x' && (k == 0 || k == (int)segs.size() - 1)) S.endWin = "all";
                double c = a + S.w / 2;
                a += S.w;
                double cx = row.ax == 'x' ? c : row.fix, cz = row.ax == 'x' ? row.fix : c;
                int rot = row.rot;
                B.xf(cx, cz, rot);
                genBuilding(S, R);
                addCol(wRect(cx, cz, rot, -S.w / 2 - .1, -d / 2 - .1, S.w / 2 + .1, d / 2 + .1));
                RECS.back().col = (int)COL.size() - 1;
                count++;
            }
        }
        double ix0 = bx - hx + d + 2, ix1 = bx + hx - d - 2, iz0 = bz - hz + d + 2, iz1 = bz + hz - d - 2;
        for (int i = R.integer(2, 5); i > 0; i--) {
            double x = R.range(ix0, ix1);
            double z = R.range(iz0, iz1);
            tree(x, z, R, R.pick(std::vector<std::string>{"linden", "poplar", "birch"}));
        }
        if (R.chance(.5)) shed(R.range(ix0 + 2, ix1 - 2), iz0 + 2, 0, R);
    }
    for (int x = -120; x <= 120; x += 24) {
        B.xf(0, 0, 0);
        for (double z : {-8.6, 8.6}) B.box(x - .12, 0, z - .12, x + .12, 7.2, z + .12, TL::METAL, {.4, .42, .4}, 99);
        B.beam({(double)x, 6.8, -8.6}, {(double)x, 6.8, 8.6}, .06, TL::METAL, {.2, .2, .22});
        for (double z : {-2.0, 2.0}) wire({(double)x, 6.2, z}, {(double)x + 24, 6.2, z}, .25);
        addCol({x - .2, -8.8, x + .2, -8.4});
        addCol({x - .2, 8.4, x + .2, 8.8});
    }
    for (int x = -108; x <= 108; x += 24) { lamp(x, 10.2, M_PI); lamp(x + 12, -10.2, 0); }
    for (int x : {-36, 36})
        for (int z = -120; z <= 120; z += 30)
            if (std::abs(z) > 14) lamp(x + (z % 60 == 0 ? -6.2 : 6.2), z, z % 60 == 0 ? M_PI / 2 : -M_PI / 2);
    Skirt sk{TL::GRASS, {.8, .8, .76}};
    G.build(R, &sk);
    skyline("old", R);
    return {count, {0, 8, 0}, 190, "Old town · " + std::to_string(count) + " buildings", {-20, 10, -M_PI / 2}, "old", 136};
}
static Info genVillage(Rng& R) {
    Ground G(132, 4);
    int count = 0;
    G.mark({-132, -4, 132, 4}, 2);
    double chX = R.chance(.8) ? R.pick(std::vector<double>{-84, 84}) : 999;
    int chS = R.pick(std::vector<int>{1, -1});
    for (int side : {1, -1})
        for (int x = -108; x <= 108; x += 24) {
            if (side == chS && std::fabs(x - chX) < 30) continue;
            if (R.chance(.12)) continue;
            izbaPlot(x, side, R, G);
            count++;
        }
    if (chX != 999) { G.mark({chX - 14, chS * 6.0, chX + 14, chS * 40.0}, 0); churchyard(chX, chS * 24, chS > 0 ? 2 : 0, R); count++; }
    for (int x = -117; x <= 117; x += 26) {
        pole(x, 5.2, 7.4);
        if (x + 26 <= 117)
            for (double dx : {-.8, 0.0, .8}) wire({x + dx, 6.85, 5.2}, {x + 26 + dx, 6.85, 5.2}, .6);
        if (R.chance(.4)) {
            B.xf(0, 0, 0);
            B.box(x - .15, 6.1, 4.4, x + .15, 6.3, 5.2, TL::METAL, {.4, .4, .4}, 99);
            B.box(x - .25, 5.9, 3.9, x + .25, 6.1, 4.4, TL::LAMP, {1, 1, 1}, 99, emOpt(em({1, .85, .55}, .02)));
            V3 dir{0, -1, -.2};
            light({(double)x, 5.8, 4.1}, &dir, {1, .85, .55}, 15, 1.6, .02, .2, 0, 3);
        }
    }
    busStop(-125, -5.6, 0, R);
    for (int side : {1, -1}) {
        G.mark({-132, side > 0 ? 40.0 : -60.0, 132, side > 0 ? 60.0 : -40.0}, 9);
        for (int i = R.integer(3, 7); i > 0; i--) {
            double x = R.range(-110, 110);
            double z = side * R.range(64, 82);
            haystack(x, z, R);
        }
        for (int i = 0; i < 60; i++) {
            double x = R.range(-128, 128);
            double z = side * R.range(96, 128);
            tree(x, z, R, R.chance(.6) ? "spruce" : "birch");
        }
    }
    Skirt sk{TL::GARDEN, {.95, .92, .8}};
    G.build(R, &sk);
    skyline("village", R);
    return {count, {0, 4, 0}, 170, "Village · " + std::to_string(count) + " plots", {-30, 0, -M_PI / 2}, "village", 136};
}
static Info genSingle(Rng& R, const std::string& type) {
    if (type == "church") {
        Ground G(48, 4);
        G.mark({-48, 22, 48, 30}, 1); G.mark({-48, 18, 48, 22}, 4); G.mark({-2, 16, 2, 18}, 4);
        churchyard(0, 0, 0, R);
        for (int x = -40; x <= 40; x += 24) lamp(x, 19, 0);
        for (int i = 0; i < 14; i++) {
            double x = R.range(-44, 44);
            double z = R.range(-44, 14);
            if (std::fabs(x) > 14 || z < -18) tree(x, z, R);
        }
        Skirt sk{TL::GRASS, {.85, .85, .82}};
        G.build(R, &sk);
        skyline("old", R);
        return {1, {0, 10, 0}, 78, "Orthodox church", {0, 24, 0}, "village", 50};
    }
    if (type == "izba") {
        Ground G(40, 4);
        G.mark({-40, -4, 40, 4}, 2);
        izbaPlot(0, 1, R, G);
        pole(-13, -6.8, 7.4); pole(13, -6.8, 7.4);
        for (double dx : {-.8, 0.0, .8}) wire({-13 + dx, 6.85, -6.8}, {13 + dx, 6.85, -6.8}, .6);
        for (int i = 0; i < 12; i++) {
            double x = R.range(-38, 38);
            double z = R.range(-38, -10);
            tree(x, z, R);
        }
        Skirt sk{TL::GARDEN, {.95, .92, .8}};
        G.build(R, &sk);
        skyline("village", R);
        return {1, {0, 3, 18}, 34, "Izba", {0, 0, M_PI}, "village", 42};
    }
    Spec S = specFor(type, R, 26);
    bool st = S.type == "stalinka";
    double H = S.H, hw = S.w / 2, hd = S.d / 2;
    double half = std::ceil((std::max(S.w, S.d) / 2 + 30) / 4) * 4;
    Ground G(half, 4);
    double zs = hd + (st || type == "tenement" ? 4 : 14);
    B.xf(0, 0, 0);
    genBuilding(S, R);
    addCol({-hw - .2, -hd - .2, hw + .2, hd + .2});
    RECS.back().col = (int)COL.size() - 1;
    if (type == "tenement") { G.mark({-half, -half, half, half}, 7); G.mark({-hw - 4, -hd - 4, hw + 4, hd + 4}, 4); }
    Placed e{S, 0, 0, 0};
    if (type != "tenement") siteMarks(G, e, false);
    G.mark({-half, zs, half, zs + 4}, 4);
    G.mark({-half, zs + 4, half, zs + 12}, type == "tenement" ? 7 : 1);
    if (!st && type != "tenement") G.mark({hw + 2, hd + 5, hw + 6, zs}, 1);
    for (double x = -half + 10; x < half - 4; x += 24) lamp(x, zs + 1, 0);
    Rect bx{-hw - 4, -hd - (st ? 13 : 6), hw + 4, hd + (st ? 5 : 12)};
    int nt = 0;
    for (int i = 0; i < 300 && nt < 26; i++) {
        double x = R.range(-half + 2, half - 2);
        double z = R.range(-half + 2, zs - 2);
        if (G.type(x, z) != 0) continue;
        if (x > bx[0] && x < bx[2] && z > bx[1] && z < bx[3]) continue;
        tree(x, z, R);
        nt++;
    }
    if (R.chance(.5)) {
        double x = R.chance(.5) ? -hw - 10 : hw + 10;
        if (std::fabs(x) < half - 6) kiosk(x, zs - 2.6, 0, R);
    }
    Skirt sk{TL::GRASS, {.85, .85, .82}};
    G.build(R, &sk);
    skyline(type == "tenement" || st ? "old" : "city", R);
    return {1, {0, H * .42, 0}, std::max(S.w * .95, H * 1.35) + 24, NAMES.at(S.type) + " · " + std::to_string(S.floors) + " floors", {0, zs + 2, 0}, "city", half + 4};
}

Info generate(const std::string& mode, const std::string& style, double seed) {
    double off = mode == "district" ? 0 : mode == "old" ? 100000 : mode == "village" ? 200000 : 300000;
    Rng R(seed + off);
    RECS.clear();
    WOBJ.clear();
    if (mode == "district") return genDistrict(R, style);
    if (mode == "old") return genOld(R);
    if (mode == "village") return genVillage(R);
    std::string t = style == "mixed" ? R.pick(TYPES) : style;
    return genSingle(R, t);
}

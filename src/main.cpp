// Panelka — PS1 Eastern Bloc generator, raylib C++ port of the three.js page.
#include "panelka.h"
#include "raylib.h"
#include "rlgl.h"
#include "shaders.h"
#include "ui.h"
#include "game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <random>
#include <unordered_map>

// The few GL entry points rlgl does not wrap (point sprites, depth func).
#if defined(_WIN32)
#define PANELKA_GLAPI __stdcall
#else
#define PANELKA_GLAPI
#endif
extern "C" {
void PANELKA_GLAPI glEnable(unsigned int cap);
void PANELKA_GLAPI glDrawArrays(unsigned int mode, int first, int count);
void PANELKA_GLAPI glDepthFunc(unsigned int func);
}
#define GL_POINTS_ 0x0000
#define GL_PROGRAM_POINT_SIZE_ 0x8642

static std::mt19937_64 rng64((uint64_t)time(nullptr));
static double mrand() { return (rng64() >> 11) * (1.0 / 9007199254740992.0); }
static double nowMs() { return GetTime() * 1000.0; }
static double clampN(double v, double a, double b) { return v < a ? a : v > b ? b : v; }

/* ================= MATH ================= */
struct M4 { float m[16]; };
static M4 mmul(const M4& a, const M4& b) {
    M4 r;
    for (int c = 0; c < 4; c++)
        for (int rr = 0; rr < 4; rr++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}
static M4 perspective(double fovDeg, double aspect, double n, double f) {
    double top = n * std::tan(fovDeg * M_PI / 360), h = 2 * top, w = aspect * h, l = -.5 * w, r = l + w, t = top, b = t - h;
    M4 m{};
    m.m[0] = 2 * n / (r - l); m.m[5] = 2 * n / (t - b); m.m[8] = (r + l) / (r - l); m.m[9] = (t + b) / (t - b);
    m.m[10] = -(f + n) / (f - n); m.m[11] = -1; m.m[14] = -2 * f * n / (f - n);
    return m;
}
static M4 ortho(double l, double r, double t, double b, double n, double f) {
    double w = 1 / (r - l), h = 1 / (t - b), p = 1 / (f - n);
    M4 m{};
    m.m[0] = 2 * w; m.m[12] = -(r + l) * w; m.m[5] = 2 * h; m.m[13] = -(t + b) * h; m.m[10] = -2 * p; m.m[14] = -(f + n) * p; m.m[15] = 1;
    return m;
}
static M4 lookAt(V3 eye, V3 tg) {
    V3 z{eye[0] - tg[0], eye[1] - tg[1], eye[2] - tg[2]};
    double l = std::sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
    if (l == 0) z = {0, 0, 1}; else z = {z[0] / l, z[1] / l, z[2] / l};
    V3 x{z[2], 0, -z[0]}; // up(0,1,0) x z
    l = std::sqrt(x[0] * x[0] + x[2] * x[2]);
    if (l == 0) { z[2] += .0001; l = std::sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]); z = {z[0] / l, z[1] / l, z[2] / l}; x = {z[2], 0, -z[0]}; l = std::sqrt(x[0] * x[0] + x[2] * x[2]); }
    x = {x[0] / l, 0, x[2] / l};
    V3 y{z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
    M4 m{};
    m.m[0] = x[0]; m.m[4] = x[1]; m.m[8] = x[2];
    m.m[1] = y[0]; m.m[5] = y[1]; m.m[9] = y[2];
    m.m[2] = z[0]; m.m[6] = z[1]; m.m[10] = z[2];
    m.m[12] = -(x[0] * eye[0] + x[1] * eye[1] + x[2] * eye[2]);
    m.m[13] = -(y[0] * eye[0] + y[1] * eye[1] + y[2] * eye[2]);
    m.m[14] = -(z[0] * eye[0] + z[1] * eye[1] + z[2] * eye[2]);
    m.m[15] = 1;
    return m;
}
static Matrix toRl(const M4& a) {
    Matrix r;
    r.m0 = a.m[0]; r.m1 = a.m[1]; r.m2 = a.m[2]; r.m3 = a.m[3]; r.m4 = a.m[4]; r.m5 = a.m[5]; r.m6 = a.m[6]; r.m7 = a.m[7];
    r.m8 = a.m[8]; r.m9 = a.m[9]; r.m10 = a.m[10]; r.m11 = a.m[11]; r.m12 = a.m[12]; r.m13 = a.m[13]; r.m14 = a.m[14]; r.m15 = a.m[15];
    return r;
}
static V3 vmul(const V3& c, double k) { return {c[0] * k, c[1] * k, c[2] * k}; }
static V3 vmix(const V3& a, const V3& b, double t) { return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t}; }
static V3 vnrm(const V3& a) { double l = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); if (l == 0) l = 1; return {a[0] / l, a[1] / l, a[2] / l}; }

/* ================= GL HELPERS ================= */
struct Prog {
    unsigned id = 0;
    std::map<std::string, int> loc;
    int L(const char* n) {
        auto it = loc.find(n);
        if (it != loc.end()) return it->second;
        return loc[n] = rlGetLocationUniform(id, n);
    }
    void f(const char* n, float v) { rlSetUniform(L(n), &v, RL_SHADER_UNIFORM_FLOAT, 1); }
    void i(const char* n, int v) { rlSetUniform(L(n), &v, RL_SHADER_UNIFORM_INT, 1); }
    void v2(const char* n, float a, float b) { float v[2] = {a, b}; rlSetUniform(L(n), v, RL_SHADER_UNIFORM_VEC2, 1); }
    void v3(const char* n, const V3& a) { float v[3] = {(float)a[0], (float)a[1], (float)a[2]}; rlSetUniform(L(n), v, RL_SHADER_UNIFORM_VEC3, 1); }
    void m(const char* n, const M4& a) { rlSetUniformMatrix(L(n), toRl(a)); }
    void tex(const char* n, int slot, unsigned t) { rlActiveTextureSlot(slot); rlEnableTexture(t); i(n, slot); }
};
static Prog mkProg(const char* vs, const char* fs) {
    Prog p;
#if RAYLIB_VERSION_MAJOR >= 6
    p.id = rlLoadShaderProgram(vs, fs);
#else
    p.id = rlLoadShaderCode(vs, fs); // raylib 5.x name
#endif
    return p;
}

struct GpuMesh { unsigned vao = 0, vbo = 0; int count = 0; };
static GpuMesh uploadMesh(const std::vector<float>& V) {
    GpuMesh g;
    g.count = (int)(V.size() / FLOATS_PER_VERT);
    g.vao = rlLoadVertexArray();
    rlEnableVertexArray(g.vao);
    g.vbo = rlLoadVertexBuffer(V.data(), (int)(V.size() * sizeof(float)), false);
    const int st = FLOATS_PER_VERT * 4;
    const int comp[6] = {3, 3, 2, 3, 3, 4}, off[6] = {0, 3, 6, 8, 11, 14};
    for (int a = 0; a < 6; a++) { rlSetVertexAttribute(a, comp[a], RL_FLOAT, false, st, off[a] * 4); rlEnableVertexAttribute(a); }
    rlDisableVertexArray();
    return g;
}
static void freeMesh(GpuMesh& g) { if (g.vao) { rlUnloadVertexArray(g.vao); rlUnloadVertexBuffer(g.vbo); } g = GpuMesh(); }
struct PointBuf { unsigned vao = 0, vbo = 0; int n = 0; };
static PointBuf mkPoints(int n) {
    PointBuf p;
    p.n = n;
    p.vao = rlLoadVertexArray();
    rlEnableVertexArray(p.vao);
    std::vector<float> z((size_t)n * 3, 0.f);
    p.vbo = rlLoadVertexBuffer(z.data(), n * 12, true);
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, 12, 0);
    rlEnableVertexAttribute(0);
    rlDisableVertexArray();
    return p;
}

/* ================= STATE ================= */
struct State {
    bool shadows = true, points = true;
    double seed = 1;
    std::string mode = "district", style = "mixed";
    double hour = 13;
    bool clock = false;
    std::string season = "autumn";
    int res = 224;
    bool snap = true, affine = true, dither = true, crt = false, fps30 = false, orbit = true, sound = false;
    int view = 0;
    bool walk = false;
} state;
static bool* flagRef(const std::string& v) {
    if (v == "snap") return &state.snap;
    if (v == "affine") return &state.affine;
    if (v == "dither") return &state.dither;
    if (v == "crt") return &state.crt;
    if (v == "fps30") return &state.fps30;
    if (v == "shadows") return &state.shadows;
    if (v == "points") return &state.points;
    if (v == "orbit") return &state.orbit;
    if (v == "sound") return &state.sound;
    return nullptr;
}
static std::string fmtH(double h) {
    int m = (int)std::floor(h * 60) % 1440;
    char b[16];
    snprintf(b, 16, "%02d:%02d", m / 60, m % 60);
    return b;
}
static std::string numStr(double v) { // JS Number#toString for the seed
    if (v == std::floor(v) && std::fabs(v) < 1e21) { char b[40]; snprintf(b, 40, "%.0f", v); return b; }
    char b[40];
    for (int p = 1; p <= 17; p++) { snprintf(b, 40, "%.*g", p, v); if (std::strtod(b, nullptr) == v) break; }
    std::string s = b;
    size_t e = s.find('e');
    if (e != std::string::npos) { std::string ex = s.substr(e + 1); int n = std::atoi(ex.c_str()); s = s.substr(0, e) + "e" + (n >= 0 ? "+" : "-") + std::to_string(std::abs(n)); }
    return s;
}

/* ================= ENVIRONMENT ================= */
struct Env { V3 sc, amb, hor, zen, cl; double glow, stars; };
static const Env NIGHT{{.07, .09, .15}, {.1, .12, .2}, {.06, .07, .12}, {.01, .015, .04}, {.09, .1, .14}, 1, 1};
static const Env DAWN{{.8, .45, .35}, {.36, .31, .4}, {.86, .58, .52}, {.32, .38, .58}, {.92, .62, .56}, .6, 0};
static const Env DAY{{.62, .6, .55}, {.56, .58, .63}, {.72, .75, .78}, {.42, .54, .72}, {.88, .89, .91}, 0, 0};
static const Env DUSK{{.9, .5, .3}, {.4, .34, .44}, {.82, .54, .44}, {.28, .3, .52}, {.86, .52, .46}, .65, 0};
static const std::vector<std::pair<double, const Env*>> KF{{0, &NIGHT}, {5, &NIGHT}, {6.5, &DAWN}, {8.5, &DAY}, {16.5, &DAY}, {18.6, &DUSK}, {20.5, &NIGHT}, {24, &NIGHT}};
static Env envAt(double h) {
    size_t i = 0;
    while (i < KF.size() - 2 && KF[i + 1].first <= h) i++;
    double a = KF[i].first, b = KF[i + 1].first;
    const Env &A = *KF[i].second, &Bk = *KF[i + 1].second;
    double t = std::min(1.0, std::max(0.0, (h - a) / (b - a)));
    return {vmix(A.sc, Bk.sc, t), vmix(A.amb, Bk.amb, t), vmix(A.hor, Bk.hor, t), vmix(A.zen, Bk.zen, t), vmix(A.cl, Bk.cl, t), A.glow + (Bk.glow - A.glow) * t, A.stars + (Bk.stars - A.stars) * t};
}
static double lightsAt(double h) {
    static const double P[9][2] = {{0, .42}, {3, .15}, {5.5, .15}, {6.5, .35}, {8, 0}, {16.5, 0}, {19.5, .55}, {23, .55}, {24, .42}};
    int i = 0;
    while (i < 7 && P[i + 1][0] <= h) i++;
    double t = std::min(1.0, std::max(0.0, (h - P[i][0]) / (P[i + 1][0] - P[i][0])));
    return P[i][1] + (P[i + 1][1] - P[i][1]) * t;
}
struct Uni {
    V3 sunDir, sunCol, sky, gnd, fog;
    double lights = 0, snow = 0, fogNear = 60, fogFar = 400;
    V3 sHor, sZen, sSunC, sCloudC;
    double sStars = 0, sMoon = 0, sCloud = .6;
    V3 snowCol{1, 1, 1}, smokeCol{.81, .81, .81};
    bool snowVis = false;
} U;
static void applyEnv() {
    Env e = envAt(state.hour);
    double h = state.hour, a = (h - 6) / 12 * M_PI, el = std::sin(a);
    bool moon = el < -.05;
    V3 sd = moon ? vnrm({.35, .72, -.5}) : vnrm({-std::cos(a) * .85, std::max(el, .08), .42});
    bool wk = state.season == "winter";
    U.sunDir = sd; U.sunCol = vmul(e.sc, 1.15);
    V3 am = wk ? vmul(e.amb, 1.06) : e.amb;
    U.sky = vmix(am, e.zen, .15);
    U.gnd = vmul(vmix(am, wk ? V3{.9, .9, .95} : V3{.5, .45, .38}, .25), wk ? .8 : .55);
    U.fog = e.hor;
    U.lights = lightsAt(h); U.snow = wk ? 1 : 0;
    U.sHor = e.hor; U.sZen = wk ? vmix(e.zen, e.hor, .35) : e.zen; U.sSunC = e.sc; U.sCloudC = e.cl;
    U.sStars = e.stars * (wk ? .6 : 1); U.sMoon = moon ? 1 : 0;
    U.sCloud = state.season == "summer" ? .35 : state.season == "autumn" ? .6 : .9;
    double k = std::min(1.0, e.amb[1] * 1.4 + .25);
    U.snowCol = {k, k, k * 1.03};
    U.smokeCol = {e.hor[0] * .5 + .35 * k, e.hor[1] * .5 + .35 * k, e.hor[2] * .5 + .35 * k};
    U.snowVis = wk;
}

/* ================= RENDER RESOURCES ================= */
static Prog mainP, depthP, skyP, ptP;
static Shader crtShader;
static unsigned atlasTex = 0, shadowFbo = 0, shadowDepth = 0, shadowColor = 0, lTex = 0, cTex = 0;
static int LW = 1024, CW = 1024;
static const int SHADOW_RES = 2048;
static const double GRID_MIN[3] = {-136, 0, -136}, GRID_CELL = 4;
static const int GRID_N[3] = {68, 16, 68}, GRID_K = 24;
static GpuMesh staticMesh, skyMesh;
static std::vector<float> staticGeo;
static int nLights = 0;
static bool hasInfo = false;
static Info INFO;
static M4 shadowMat{};
static bool shadowDirty = true;
static double shadowHour = -99, shadowHalf = 136;
static RenderTexture2D rt{};
static int rw = 1, rh = 1;

static void renderShadow() {
    V3 d = U.sunDir;
    double h = shadowHalf, S = h * 1.45 + 30;
    M4 P = ortho(-S, S, S, -S, 1, 1400), Vw = lookAt({d[0] * 600, d[1] * 600, d[2] * 600}, {0, 0, 0});
    shadowMat = mmul(P, Vw);
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(shadowFbo);
    rlViewport(0, 0, SHADOW_RES, SHADOW_RES);
    rlClearColor(255, 255, 255, 255);
    rlEnableDepthMask();
    rlClearScreenBuffers();
    rlEnableDepthTest();
    rlDisableBackfaceCulling();
    rlDisableColorBlend();
    if (staticMesh.count) {
        rlEnableShader(depthP.id);
        depthP.m("uVP", shadowMat);
        depthP.tex("uTex", 0, atlasTex);
        rlEnableVertexArray(staticMesh.vao);
        rlDrawVertexArray(0, staticMesh.count);
        rlDisableVertexArray();
        rlDisableShader();
    }
    rlDisableFramebuffer();
    rlActiveTextureSlot(0);
    rlEnableBackfaceCulling();
    rlDisableDepthTest();
    rlEnableColorBlend();
    rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
    shadowDirty = false;
    shadowHour = state.hour;
}
/* Clustered point lights: every light goes into each grid cell its radius touches (max 24 per 4m cell, most important first). */
static std::vector<Light> LSORT; // LIGHTS in light-texture order (most important first)
static std::vector<int> LSLOT;    // LIGHTS index -> texture slot
static int lightRows = 0;
// 6 texels per light: pos+radius, colour+threshold, dir+cone, flicker+room flag, room min, room max
static void uploadLightData() {
    int LH = std::max(1, (int)std::ceil(std::max<size_t>(1, LSORT.size()) * 6.0 / LW));
    std::vector<float> ld((size_t)LW * LH * 4, 0.f);
    for (size_t i = 0; i < LSORT.size(); i++) {
        const Light& l = LSORT[i];
        float v[24] = {(float)l.p[0], (float)l.p[1], (float)l.p[2], (float)l.r, (float)(l.c[0] * l.i), (float)(l.c[1] * l.i), (float)(l.c[2] * l.i), (float)l.thr,
                       (float)l.d[0], (float)l.d[1], (float)l.d[2], (float)l.cone, (float)l.fl, l.room ? 1.f : 0.f, 0, 0,
                       (float)l.lo[0], (float)l.lo[1], (float)l.lo[2], 0, (float)l.hi[0], (float)l.hi[1], (float)l.hi[2], 0};
        std::memcpy(&ld[i * 24], v, sizeof v);
    }
    if (lTex && LH == lightRows) { rlUpdateTexture(lTex, 0, 0, LW, LH, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, ld.data()); return; }
    if (lTex) rlUnloadTexture(lTex);
    lTex = rlLoadTexture(ld.data(), LW, LH, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
    lightRows = LH;
}
static void buildLights() {
    std::vector<int> ord(LIGHTS.size());
    for (size_t i = 0; i < ord.size(); i++) ord[i] = (int)i;
    std::stable_sort(ord.begin(), ord.end(), [](int a, int b) { const Light &A = LIGHTS[a], &B_ = LIGHTS[b]; return A.pri != B_.pri ? A.pri > B_.pri : A.thr < B_.thr; });
    LSORT.clear();
    LSLOT.assign(LIGHTS.size(), 0);
    for (size_t i = 0; i < ord.size(); i++) { LSLOT[ord[i]] = (int)i; LSORT.push_back(LIGHTS[ord[i]]); }
    const std::vector<Light>& L = LSORT;
    nLights = (int)L.size();
    uploadLightData();
    int nx = GRID_N[0], ny = GRID_N[1], nz = GRID_N[2], cells = nx * ny * nz, CH = (int)std::ceil(cells * 6.0 / CW);
    std::vector<float> cd((size_t)CW * CH * 4, 0.f);
    std::vector<uint8_t> cnt(cells, 0);
    double c = GRID_CELL;
    auto cl = [](int v, int n) { return v < 0 ? 0 : v > n - 1 ? n - 1 : v; };
    for (size_t i = 0; i < L.size(); i++) {
        const Light& l = L[i];
        int x0 = (int)std::floor((l.p[0] - l.r - GRID_MIN[0]) / c), x1 = (int)std::floor((l.p[0] + l.r - GRID_MIN[0]) / c);
        int y0 = (int)std::floor((l.p[1] - l.r - GRID_MIN[1]) / c), y1 = (int)std::floor((l.p[1] + l.r - GRID_MIN[1]) / c);
        int z0 = (int)std::floor((l.p[2] - l.r - GRID_MIN[2]) / c), z1 = (int)std::floor((l.p[2] + l.r - GRID_MIN[2]) / c);
        if (x1 < 0 || z1 < 0 || y1 < 0 || x0 >= nx || z0 >= nz || y0 >= ny) continue;
        for (int y = cl(y0, ny); y <= cl(y1, ny); y++)
            for (int z = cl(z0, nz); z <= cl(z1, nz); z++)
                for (int x = cl(x0, nx); x <= cl(x1, nx); x++) {
                    int k = x + z * nx + y * nx * nz;
                    if (cnt[k] < GRID_K) { cd[(size_t)k * 24 + cnt[k]] = (float)(i + 1); cnt[k]++; }
                }
    }
    if (cTex) rlUnloadTexture(cTex);
    cTex = rlLoadTexture(cd.data(), CW, CH, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
}
static void buildSky() {
    // THREE.SphereGeometry(1500,24,12)
    const double r = 1500;
    const int ws = 24, hs = 12;
    std::vector<std::array<float, 3>> P;
    for (int iy = 0; iy <= hs; iy++)
        for (int ix = 0; ix <= ws; ix++) {
            double u = (double)ix / ws, v = (double)iy / hs;
            P.push_back({(float)(-r * std::cos(u * 2 * M_PI) * std::sin(v * M_PI)), (float)(r * std::cos(v * M_PI)), (float)(r * std::sin(u * 2 * M_PI) * std::sin(v * M_PI))});
        }
    std::vector<float> V;
    auto push = [&](int i) { for (int k = 0; k < 3; k++) V.push_back(P[i][k]); for (int k = 3; k < FLOATS_PER_VERT; k++) V.push_back(0); };
    for (int iy = 0; iy < hs; iy++)
        for (int ix = 0; ix < ws; ix++) {
            int a = iy * (ws + 1) + ix + 1, b = iy * (ws + 1) + ix, c = (iy + 1) * (ws + 1) + ix, d = (iy + 1) * (ws + 1) + ix + 1;
            if (iy != 0) { push(a); push(b); push(d); }
            if (iy != hs - 1) { push(b); push(c); push(d); }
        }
    skyMesh = uploadMesh(V);
}

/* ================= PARTICLES ================= */
static const int SN = 1600;
static std::vector<float> snowPos(SN * 3), smokePos;
static std::vector<double> smokeAge;
static std::vector<V3> smokeSrc;
static PointBuf snowBuf, smokeBuf;
static void buildSmoke() {
    if (smokeBuf.vao) { rlUnloadVertexArray(smokeBuf.vao); rlUnloadVertexBuffer(smokeBuf.vbo); smokeBuf = PointBuf(); }
    smokeAge.clear(); smokePos.clear(); smokeSrc.clear();
    if (!SMOKE.empty()) {
        int n = (int)SMOKE.size() * 40;
        smokeAge.resize(n); smokePos.assign((size_t)n * 3, 0.f);
        for (int i = 0; i < n; i++) smokeAge[i] = mrand();
        smokeSrc = SMOKE;
        smokeBuf = mkPoints(n);
    }
}

/* ================= CAMERA ================= */
struct Orbit { double yaw = .7, pitch = .5, dist = 215; V3 t{0, 6, 0}; double base = 215; V3 baseT{0, 6, 0}; } O;
struct Walker { double x = 0, z = 0, yaw = 0, pitch = 0, bob = 0, jx = 0, jy = 0, feet = 0, vy = 0; } Wk;
static V3 camPos{0, 0, 0}, camLook{0, 0, -1};
static void updOrbit() {
    double cp = std::cos(O.pitch);
    camPos = {O.t[0] + O.dist * cp * std::sin(O.yaw), O.t[1] + O.dist * std::sin(O.pitch), O.t[2] + O.dist * cp * std::cos(O.yaw)};
    if (camPos[1] < 1.5) camPos[1] = 1.5;
    camLook = O.t;
}
struct ViewDef { double p, d, ty; };
static const ViewDef VIEWS[3] = {{.52, 1, 1}, {.2, .45, 1}, {.04, .28, .35}};
static void setView(int i) {
    state.view = i;
    const ViewDef& v = VIEWS[i];
    O.pitch = v.p; O.dist = O.base * v.d;
    O.t = {O.baseT[0], O.baseT[1] * v.ty + (i == 2 ? 1.2 : 0), O.baseT[2]};
}
static void pan(double dx, double dy) {
    double s = O.dist * .0022, sy = std::sin(O.yaw), cy = std::cos(O.yaw);
    O.t[0] += (-cy * dx - sy * dy) * s;
    O.t[2] += (sy * dx - cy * dy) * s;
    O.t[0] = clampN(O.t[0], -140, 140);
    O.t[2] = clampN(O.t[2], -140, 140);
}
/* ---- interiors (walk mode) ---- */
static Interior INT;
static GpuMesh intMesh;
static std::vector<Light> baseLights;
static std::vector<std::pair<size_t, std::vector<float>>> cutSaved; // triangle float offset, original data
static void restoreCuts() {
    for (auto& c : cutSaved) rlUpdateVertexBuffer(staticMesh.vbo, c.second.data(), (int)(c.second.size() * 4), (int)(c.first * 4));
    cutSaved.clear();
}
static void applyCuts() {
    const int TF = FLOATS_PER_VERT * 3;
    for (auto& k : INT.cuts) {
        for (size_t t = 0; t + TF <= staticGeo.size(); t += TF) {
            const float* v = &staticGeo[t];
            bool in = true;
            for (int q = 0; q < 3 && in; q++)
                for (int i = 0; i < 3; i++) { float p = v[q * FLOATS_PER_VERT + i]; if (p < k.lo[i] || p > k.hi[i]) { in = false; break; } }
            if (!in || v[3] * k.n[0] + v[4] * k.n[1] + v[5] * k.n[2] < .9) continue;
            std::vector<float> orig(v, v + TF), deg = orig;
            for (int q = 1; q < 3; q++) for (int i = 0; i < 3; i++) deg[q * FLOATS_PER_VERT + i] = deg[i];
            rlUpdateVertexBuffer(staticMesh.vbo, deg.data(), TF * 4, (int)(t * 4));
            cutSaved.push_back({t, orig});
        }
    }
}
static void buildLights();
static void dropInterior(bool rebuildLights = true) {
    if (INT.rec < 0) return;
    restoreCuts();
    freeMesh(intMesh);
    INT = Interior();
    LIGHTS = baseLights;
    if (rebuildLights) buildLights();
}
static int homeRec = -1, homeFlat = -1, homeFloor = -1; // the player's flat in Live mode
static void activateInterior(int ri) {
    dropInterior(false); // the light textures are rebuilt once below

    INT = ri == homeRec ? buildInterior(ri, state.seed, homeFlat, homeFloor) : buildInterior(ri, state.seed);
    intMesh = uploadMesh(INT.V);
    applyCuts();
    LIGHTS = baseLights;
    LIGHTS.insert(LIGHTS.end(), INT.lights.begin(), INT.lights.end());
    buildLights();
    LIGHTS = baseLights;
    game::interiorLoaded();
}
// A light switch: set a room's lamps on or off (light threshold and the lamp boxes' glow).
static void setRoomLight(int room, bool on) {
    if (INT.rec < 0 || room < 0 || room >= (int)INT.rooms.size()) return;
    const float thr = on ? -1.f : 99.f;
    const IRoom& R = INT.rooms[room];
    for (int li : R.lights) {
        INT.lights[li].thr = thr;
        size_t idx = baseLights.size() + li;
        if (idx < LSLOT.size()) LSORT[LSLOT[idx]].thr = thr;
    }
    uploadLightData();
    for (auto& r : R.lampVerts) {
        for (size_t v = r.first; v < r.second; v += FLOATS_PER_VERT) INT.V[v + 14] = thr;
        rlUpdateVertexBuffer(intMesh.vbo, &INT.V[r.first], (int)((r.second - r.first) * 4), (int)(r.first * 4));
    }
}
static bool insideInterior(double x, double z) {
    if (INT.rec < 0) return false;
    double lx, lz;
    INT.toLocal(x, z, lx, lz);
    return lx > INT.lx0 && lx < INT.lx1 && lz > INT.lz0 && lz < INT.lz1;
}
// highest walkable surface under (x,z) that is at most a step above the feet
static double groundAt(double x, double z, double feet) {
    double best = 0;
    if (INT.rec >= 0) {
        double lx, lz;
        INT.toLocal(x, z, lx, lz);
        for (auto& f : INT.floors) {
            if (lx < f.x0 || lx > f.x1 || lz < f.z0 || lz > f.z1) continue;
            double h = f.axis < 0 ? f.h0 : f.axis == 0 ? f.h0 + (f.h1 - f.h0) * (lx - f.x0) / std::max(1e-6, f.x1 - f.x0) : f.h0 + (f.h1 - f.h0) * (lz - f.z0) / std::max(1e-6, f.z1 - f.z0);
            if (h <= feet + .55 && h > best) best = h;
        }
    }
    return best;
}
static bool blocked(double x, double z, double feet) {
    if (std::fabs(x) > 129 || std::fabs(z) > 129) return true;
    int skip = INT.rec >= 0 ? RECS[INT.rec].col : -1;
    for (size_t i = 0; i < COL.size(); i++) {
        if ((int)i == skip) continue;
        const auto& c = COL[i];
        if (x > c[0] - .3 && x < c[2] + .3 && z > c[1] - .3 && z < c[3] + .3) return true;
    }
    if (INT.rec >= 0) {
        double lx, lz;
        INT.toLocal(x, z, lx, lz);
        double b0 = feet + .35, b1 = feet + 1.75;
        for (auto& w : INT.walls)
            if (lx > w.x0 - .25 && lx < w.x1 + .25 && lz > w.z0 - .25 && lz < w.z1 + .25 && b1 > w.y0 && b0 < w.y1) return true;
    }
    return false;
}
// load the interior of the building the walker is approaching
static void pickInterior() {
    int best = -1;
    double bd = 1e9;
    for (size_t i = 0; i < RECS.size(); i++) {
        const BuildingRec& R = RECS[i];
        if (R.col < 0) continue;
        const auto& c = COL[R.col];
        double m = 7;
        if (Wk.x < c[0] - m || Wk.x > c[2] + m || Wk.z < c[1] - m || Wk.z > c[3] + m) continue;
        double dx = std::max({c[0] - Wk.x, 0.0, Wk.x - c[2]}), dz = std::max({c[1] - Wk.z, 0.0, Wk.z - c[3]});
        double d = dx * dx + dz * dz;
        if (d < bd) { bd = d; best = (int)i; }
    }
    if (best >= 0 && best != INT.rec && !insideInterior(Wk.x, Wk.z)) activateInterior(best);
}
static void spawnWalk() { Wk.x = INFO.spawn[0]; Wk.z = INFO.spawn[1]; Wk.yaw = INFO.spawn[2]; Wk.pitch = 0; Wk.jx = Wk.jy = 0; Wk.feet = 0; Wk.vy = 0; }
static std::map<std::string, bool> keys;
static void walkStep(double dt) {
    double mx = Wk.jx, my = Wk.jy;
    auto k = [](const char* n) { auto it = keys.find(n); return it != keys.end() && it->second; };
    if (k("w") || k("arrowup")) my += 1;
    if (k("s") || k("arrowdown")) my -= 1;
    if (k("a") || k("arrowleft")) mx -= 1;
    if (k("d") || k("arrowright")) mx += 1;
    if (game::freezeWalker()) mx = my = 0;
    double l = std::hypot(mx, my);
    if (l > 1) { mx /= l; my /= l; }
    double sp = (k("shift") ? 6.5 : 3.4) * dt, fx = -std::sin(Wk.yaw), fz = -std::cos(Wk.yaw), rx = std::cos(Wk.yaw), rz = -std::sin(Wk.yaw);
    double dx = (fx * my + rx * mx) * sp, dz = (fz * my + rz * mx) * sp;
    pickInterior();
    auto tryMove = [&](double nx, double nz) {
        double g = groundAt(nx, nz, Wk.feet);
        if (blocked(nx, nz, std::max(Wk.feet, g))) return false;
        Wk.x = nx; Wk.z = nz;
        return true;
    };
    tryMove(Wk.x + dx, Wk.z);
    tryMove(Wk.x, Wk.z + dz);
    double g = groundAt(Wk.x, Wk.z, Wk.feet);
    if (g >= Wk.feet - .45) { Wk.feet = g; Wk.vy = 0; } // walk up / down steps
    else { Wk.vy -= 9.8 * dt; Wk.feet = std::max(g, Wk.feet + Wk.vy * dt); if (Wk.feet == g) Wk.vy = 0; }
    Wk.bob += std::hypot(dx, dz) * 2.4;
    camPos = {Wk.x, Wk.feet + 1.65 + std::sin(Wk.bob) * .045, Wk.z};
    double cp = std::cos(Wk.pitch);
    camLook = {Wk.x + fx * cp, camPos[1] + std::sin(Wk.pitch), Wk.z + fz * cp};
}

/* ================= AUDIO ================= */
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void lowpass(double f, double sr, double qdb) { // WebAudio lowpass (Q in dB)
        double w0 = 2 * M_PI * f / sr, al = std::sin(w0) / (2 * std::pow(10, qdb / 20)), c = std::cos(w0), a0 = 1 + al;
        b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    void bandpass(double f, double sr, double q) {
        double w0 = 2 * M_PI * f / sr, al = std::sin(w0) / (2 * q), c = std::cos(w0), a0 = 1 + al;
        b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    double run(double x) { double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; }
};
struct Voice { int type; double f0, f1, t0, dur, vol; bool filt; Biquad bp; double phase = 0; };
static struct Audio {
    std::mutex mx;
    bool ctx = false, on = false;
    double next = 0, time = 0, sr = 44100;
    std::vector<float> noise;
    size_t ni = 0;
    Biquad lpW, lpH;
    double gW = .15, gH = 0, tW = .15, tH = 0;
    std::vector<Voice> voices;
    AudioStream stream{};
} AU;
static double polyblep(double t, double dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
}
static void audioCb(void* buf, unsigned int frames) {
    float* out = (float*)buf;
    std::lock_guard<std::mutex> lk(AU.mx);
    const double sr = AU.sr, kg = 1 - std::exp(-1 / (sr * .5));
    for (unsigned s = 0; s < frames; s++) {
        double t = AU.time;
        double n = AU.noise[AU.ni];
        AU.ni = (AU.ni + 1) % AU.noise.size();
        AU.gW += (AU.tW - AU.gW) * kg;
        AU.gH += (AU.tH - AU.gH) * kg;
        double o = AU.lpW.run(n) * AU.gW + AU.lpH.run(n) * AU.gH;
        for (auto& v : AU.voices) {
            if (t < v.t0) continue;
            double lt = t - v.t0;
            double f = (v.f1 > 0) ? (lt < v.dur ? v.f0 * std::pow(v.f1 / v.f0, lt / v.dur) : v.f1) : v.f0;
            double g;
            if (lt < .015) g = v.vol * lt / .015;
            else if (lt < v.dur) g = v.vol * std::pow(.0008 / v.vol, (lt - .015) / (v.dur - .015));
            else g = .0008;
            double dtp = f / sr, x;
            if (v.type == 0) x = std::sin(2 * M_PI * v.phase);
            else if (v.type == 1) { double p = v.phase + .5; p -= std::floor(p); x = 2 * p - 1 - polyblep(p, dtp); }
            else { x = (v.phase < .5 ? 1 : -1) + polyblep(v.phase, dtp); double p2 = v.phase + .5; p2 -= std::floor(p2); x -= polyblep(p2, dtp); }
            v.phase += dtp;
            v.phase -= std::floor(v.phase);
            if (v.filt) x = v.bp.run(x);
            o += x * g;
        }
        out[s] = (float)clampN(o, -1, 1);
        AU.time += 1 / sr;
    }
    AU.voices.erase(std::remove_if(AU.voices.begin(), AU.voices.end(), [&](const Voice& v) { return AU.time > v.t0 + v.dur + .05; }), AU.voices.end());
}
static bool auInit() {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return false;
    AU.sr = 44100;
    size_t len = (size_t)AU.sr * 3;
    AU.noise.resize(len);
    double b = 0;
    for (size_t i = 0; i < len; i++) { b = (b + .02 * (mrand() * 2 - 1)) / 1.02; AU.noise[i] = (float)(b * 3.5); }
    AU.lpW.lowpass(420, AU.sr, 1);
    AU.lpH.lowpass(110, AU.sr, 1);
    AU.gW = AU.tW = .15; AU.gH = AU.tH = 0;
    SetAudioStreamBufferSizeDefault(2048);
    AU.stream = LoadAudioStream((unsigned)AU.sr, 32, 1);
    SetAudioStreamCallback(AU.stream, audioCb);
    PlayAudioStream(AU.stream);
    AU.ctx = true;
    return true;
}
// type: 0 sine, 1 sawtooth, 2 square (caller holds the lock)
static void tone(int type, double f0, double f1, double t0, double dur, double vol, double filt) {
    Voice v{type, f0, f1, t0, dur, vol, filt > 0, Biquad()};
    if (filt > 0) v.bp.bandpass(filt, AU.sr, 1.8);
    AU.voices.push_back(v);
}
static void caw() { double t = AU.time; int n = 1 + (int)(mrand() * 3); for (int k = 0; k < n; k++) tone(1, 760, 430, t + k * .34, .24, .06, 1100); }
static void bark() { double t = AU.time; for (int k = 0; k < 2; k++) tone(2, 300, 170, t + k * .28, .14, .04, 700); }
static void crickets() { double t = AU.time; for (int k = 0; k < 8; k++) tone(0, 4400, 0, t + k * .09, .05, .015, 0); }
static void auTick(double t) {
    if (!AU.on || !AU.ctx || !hasInfo) return;
    std::lock_guard<std::mutex> lk(AU.mx);
    const std::string& snd = INFO.sound;
    AU.tW = .1 + .06 * std::sin(t * .00031) + (state.season == "winter" ? .12 : 0);
    AU.tH = snd == "village" ? 0 : .3;
    if (t > AU.next) {
        AU.next = t + 4000 + mrand() * 9000;
        bool night = envAt(state.hour).glow > .5;
        double r = mrand();
        if (night && state.season == "summer" && r < .7) crickets();
        else if (snd == "village" && night && r < .8) bark();
        else if (!night && state.season != "winter") caw();
    }
}
static void setSound(bool on) {
    state.sound = on;
    if (on) {
        if (!AU.ctx && !auInit()) { toast("Audio is unavailable here"); return; }
        ResumeAudioStream(AU.stream);
        AU.on = true; AU.next = 0;
    } else if (AU.ctx) { PauseAudioStream(AU.stream); AU.on = false; }
}

/* ================= DOWNLOADS ================= */
static std::string saveDir() {
    const char* home = getenv("HOME");
    if (home) { std::string d = std::string(home) + "/Downloads"; if (DirectoryExists(d.c_str())) return d; }
    return GetWorkingDirectory();
}
static void saveFile(const std::string& name, const std::vector<uint8_t>& data) {
    std::string p = saveDir() + "/" + name;
    if (SaveFileData(p.c_str(), (void*)data.data(), (int)data.size())) toast("Saved " + name);
    else toast("Could not save the file");
}
static uint32_t CRCT[256];
static uint32_t crc32(const std::vector<uint8_t>& d) {
    uint32_t c = 0xFFFFFFFF;
    for (uint8_t b : d) c = CRCT[(c ^ b) & 255] ^ (c >> 8);
    return c ^ 0xFFFFFFFF;
}
struct ZFile { std::string name; std::vector<uint8_t> data; };
static void put16(std::vector<uint8_t>& v, size_t o, uint16_t x) { v[o] = x & 255; v[o + 1] = x >> 8; }
static void put32(std::vector<uint8_t>& v, size_t o, uint32_t x) { for (int i = 0; i < 4; i++) v[o + i] = (x >> (8 * i)) & 255; }
static std::vector<uint8_t> makeZip(const std::vector<ZFile>& files) {
    std::vector<uint8_t> out, central;
    uint32_t off = 0;
    for (auto& f : files) {
        uint32_t crc = crc32(f.data), n = (uint32_t)f.data.size();
        std::vector<uint8_t> h(30, 0);
        put32(h, 0, 0x04034b50); put16(h, 4, 20); put16(h, 12, 0x21); put32(h, 14, crc); put32(h, 18, n); put32(h, 22, n); put16(h, 26, (uint16_t)f.name.size());
        out.insert(out.end(), h.begin(), h.end()); out.insert(out.end(), f.name.begin(), f.name.end()); out.insert(out.end(), f.data.begin(), f.data.end());
        std::vector<uint8_t> c(46, 0);
        put32(c, 0, 0x02014b50); put16(c, 4, 20); put16(c, 6, 20); put16(c, 14, 0x21); put32(c, 16, crc); put32(c, 20, n); put32(c, 24, n); put16(c, 28, (uint16_t)f.name.size()); put32(c, 42, off);
        central.insert(central.end(), c.begin(), c.end()); central.insert(central.end(), f.name.begin(), f.name.end());
        off += 30 + (uint32_t)f.name.size() + n;
    }
    std::vector<uint8_t> e(22, 0);
    put32(e, 0, 0x06054b50); put16(e, 8, (uint16_t)files.size()); put16(e, 10, (uint16_t)files.size()); put32(e, 12, (uint32_t)central.size()); put32(e, 16, off);
    out.insert(out.end(), central.begin(), central.end()); out.insert(out.end(), e.begin(), e.end());
    return out;
}
static std::string f3(double v) { // (Math.round(v*1000)/1000).toString()
    long long n = (long long)std::floor(v * 1000 + .5);
    if (n == 0) return "0";
    std::string s = n < 0 ? "-" : "";
    unsigned long long a = (unsigned long long)std::llabs(n);
    s += std::to_string(a / 1000);
    unsigned f = (unsigned)(a % 1000);
    if (f) { char b[8]; snprintf(b, 8, ".%03u", f); std::string fs = b; while (fs.back() == '0') fs.pop_back(); s += fs; }
    return s;
}
static std::string toFixed(double x, int digits) { // JS toFixed: round half up on the exact binary value
    char b[400];
    snprintf(b, sizeof b, "%.60f", std::fabs(x));
    std::string s = b;
    size_t dot = s.find('.');
    std::string ip = s.substr(0, dot), fp = s.substr(dot + 1);
    bool up = fp[digits] >= '5';
    std::string keep = ip + fp.substr(0, digits);
    if (up) {
        int i = (int)keep.size() - 1;
        while (i >= 0) { if (keep[i] == '9') { keep[i] = '0'; i--; } else { keep[i]++; break; } }
        if (i < 0) keep = "1" + keep;
    }
    std::string r = keep.substr(0, keep.size() - digits) + (digits ? "." + keep.substr(keep.size() - digits) : "");
    bool zero = r.find_first_not_of("0.") == std::string::npos;
    return (x < 0 && !zero ? "-" : "") + r;
}
static std::string buildObj() {
    size_t nv = staticGeo.size() / FLOATS_PER_VERT;
    std::unordered_map<std::string, int> vm, tm, nm;
    std::string vL, tL, nL, fL;
    std::vector<std::string> idx(nv);
    auto c2 = [](double v) { return toFixed(std::min(1.0, v), 2); };
    for (size_t i = 0; i < nv; i++) {
        const float* f = &staticGeo[i * FLOATS_PER_VERT];
        std::string vk = f3(f[0]) + " " + f3(f[1]) + " " + f3(f[2]) + " " + c2(f[8]) + " " + c2(f[9]) + " " + c2(f[10]);
        auto it = vm.find(vk);
        int vi;
        if (it == vm.end()) { vi = (int)vm.size() + 1; vm[vk] = vi; vL += "v " + vk + "\n"; } else vi = it->second;
        std::string tk = toFixed(f[6], 5) + " " + toFixed(f[7], 5);
        auto it2 = tm.find(tk);
        int ti;
        if (it2 == tm.end()) { ti = (int)tm.size() + 1; tm[tk] = ti; tL += "vt " + tk + "\n"; } else ti = it2->second;
        std::string nk = f3(f[3]) + " " + f3(f[4]) + " " + f3(f[5]);
        auto it3 = nm.find(nk);
        int ni;
        if (it3 == nm.end()) { ni = (int)nm.size() + 1; nm[nk] = ni; nL += "vn " + nk + "\n"; } else ni = it3->second;
        idx[i] = std::to_string(vi) + "/" + std::to_string(ti) + "/" + std::to_string(ni);
    }
    for (size_t i = 0; i + 2 < nv; i += 3) fL += "f " + idx[i] + " " + idx[i + 1] + " " + idx[i + 2] + "\n";
    return "# Panelka export, seed " + numStr(state.seed) + " (" + state.mode + ")\nmtllib panelka.mtl\no panelka\nusemtl atlas\n" + vL + tL + nL + fL;
}
static void exportObj() {
    if (staticGeo.empty()) return;
    std::string obj = buildObj();
    Image img = GenImageColor(AS, AS, BLANK);
    uint8_t* px = (uint8_t*)img.data;
    for (int y = 0; y < AS; y++)
        for (int x = 0; x < AS; x++) {
            int s = ((AS - 1 - y) * AS + x) * 4, d = (y * AS + x) * 4;
            px[d] = atlas[s]; px[d + 1] = atlas[s + 1]; px[d + 2] = atlas[s + 2]; px[d + 3] = atlas[s + 3] < 128 ? 0 : 255;
        }
    int sz = 0;
    unsigned char* png = ExportImageToMemory(img, ".png", &sz);
    UnloadImage(img);
    if (!png) { toast("Could not pack the atlas"); return; }
    std::vector<uint8_t> pngv(png, png + sz);
    MemFree(png);
    std::string readme = "Panelka export (seed " + numStr(state.seed) + ", " + state.mode + ", " + state.season + ")\n\nUnits are metres, Y is up. One mesh, one material.\nVertex colours (OBJ \"v x y z r g b\") tint the atlas; Blender imports them as a colour attribute.\nUse nearest/point filtering and alpha clip on panelka_atlas.png for the PS1 look.\nLights are not exported; smoke and snow particles are not included.\n";
    std::string mtl = "newmtl atlas\nKd 1 1 1\nmap_Kd panelka_atlas.png\nmap_d panelka_atlas.png\n";
    auto bytes = [](const std::string& s) { return std::vector<uint8_t>(s.begin(), s.end()); };
    saveFile("panelka_" + state.mode + "_" + numStr(state.seed) + ".zip", makeZip({{"panelka.obj", bytes(obj)}, {"panelka.mtl", bytes(mtl)}, {"panelka_atlas.png", pngv}, {"README.txt", bytes(readme)}}));
}
static void captureShot() {
    int s = std::max(2, (int)jsround(1080.0 / rh));
    Image img = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&img);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
    ImageResizeNN(&img, rw * s, rh * s);
    int sz = 0;
    unsigned char* png = ExportImageToMemory(img, ".png", &sz);
    UnloadImage(img);
    if (!png) return;
    std::vector<uint8_t> v(png, png + sz);
    MemFree(png);
    std::string h = fmtH(state.hour);
    h.erase(std::remove(h.begin(), h.end(), ':'), h.end());
    saveFile("panelka_" + numStr(state.seed) + "_" + h + ".png", v);
}

/* ================= UI ================= */
Theme TH;
Color hexc(unsigned h, float a) { return {(unsigned char)(h >> 16), (unsigned char)(h >> 8 & 255), (unsigned char)(h & 255), (unsigned char)(a * 255 + .5)}; }
static void initTheme() {
    bool dark = false;
    if (const char* e = getenv("PANELKA_THEME")) dark = std::string(e) == "dark";
#if defined(__linux__)
    else if (FILE* p = popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r")) {
        char b[128] = {0};
        if (fgets(b, sizeof b, p)) dark = strstr(b, "dark") != nullptr;
        pclose(p);
    }
#endif
    if (dark) TH = {hexc(0x1d4175), hexc(0x132c52), hexc(0xe6e1d4), hexc(0xece8dc), hexc(0x9fb0cc), hexc(0xc8372d), hexc(0xf0a33a), {0, 0, 0, 140}};
    else TH = {hexc(0x24508f), hexc(0x1a3b6b), hexc(0xf2efe6), hexc(0xf2efe6), hexc(0xb9c6dc), hexc(0xc8372d), hexc(0xf0a33a), {10, 20, 40, 89}};
}
static std::map<int, Font> fonts;
static Font bootFont;
Font& F(int size) {
    auto it = fonts.find(size);
    if (it != fonts.end()) return it->second;
    return fonts.begin()->second;
}
float tw(int size, const std::string& s, float sp) { return MeasureTextEx(F(size), s.c_str(), (float)size, sp).x + (s.empty() ? 0 : sp); }
// Draw text whose CSS line box starts at y with the given line-height.
void txt(int size, const std::string& s, float x, float y, float lh, Color c, float sp) {
    DrawTextEx(F(size), s.c_str(), {std::round(x), std::round(y + (lh - size) / 2)}, (float)size, sp, c);
}
Color fade(Color c, float a) { c.a = (unsigned char)(c.a * clampN(a, 0, 1)); return c; }
void rr(Rectangle r, float rad, Color c) {
    if (r.width <= 0 || r.height <= 0) return;
    float m = std::min(r.width, r.height);
    DrawRectangleRounded(r, std::min(1.f, 2 * rad / m), 12, c);
}
void rrLine(Rectangle r, float rad, float th, Color c) {
    float m = std::min(r.width, r.height);
    DrawRectangleRoundedLinesEx({r.x + th / 2, r.y + th / 2, r.width - th, r.height - th}, std::min(1.f, 2 * (rad - th / 2) / (m - th)), 12, th, c);
}
static void softShadow(Rectangle r, float rad, float oy, float blur, Color c) {
    const int N = 8;
    for (int i = 0; i < N; i++) {
        float e = -blur / 2 + blur * (i + .5f) / N;
        Rectangle q{r.x - e, r.y + oy - e, r.width + 2 * e, r.height + 2 * e};
        rr(q, rad + std::max(0.f, e), fade(c, 1.f / N * 1.1f));
    }
}
void plate(Rectangle r, float rad, float inset, float innerRad, Color bg, float alpha) {
    softShadow(r, rad, 8, 18, fade(TH.shadow, alpha));
    rr({r.x, r.y + 3, r.width, r.height}, rad, fade(TH.enamelDeep, alpha));
    rr(r, rad, fade(bg, alpha));
    rrLine({r.x + inset, r.y + inset, r.width - 2 * inset, r.height - 2 * inset}, innerRad, 2, fade(TH.plate, .9f * alpha));
}

struct SegDef { std::string k; bool multi, fx; std::vector<std::pair<std::string, std::string>> b; std::vector<bool> solo; };
static std::vector<SegDef> SEGS = {
    {"mode", false, false, {{"district", "Mikrorayon"}, {"old", "Old town"}, {"village", "Village"}, {"single", "One building"}}, {}},
    {"style", false, false, {{"mixed", "Mixed"}, {"khrush", "Khrushchyovka"}, {"panel9", "Panel slab"}, {"tower", "Tower"}, {"stalinka", "Stalinka"}, {"platten", "Plattenbau"}, {"tenement", "Tenement"}, {"school", "School"}, {"univermag", "Univermag"}, {"church", "Church"}, {"izba", "Izba"}}, {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1}},
    {"season", false, false, {{"summer", "Summer"}, {"autumn", "Autumn"}, {"winter", "Winter"}}, {}},
    {"fx", true, true, {{"snap", "Wobble"}, {"affine", "Warp"}, {"dither", "Dither"}, {"crt", "CRT"}, {"fps30", "30 fps"}}, {}},
    {"res", false, false, {{"224", "224"}, {"320", "320"}, {"480", "480"}}, {}},
    {"ex", true, true, {{"shadows", "Shadows"}, {"points", "Light sources"}, {"orbit", "Spin"}, {"sound", "Sound"}}, {}},
};
enum Ids { ID_WALK = 1, ID_LIVE, ID_VIEW, ID_HIDE, ID_SHOW, ID_ROLL, ID_SEED, ID_HOUR, ID_CLOCK, ID_SHOT, ID_EXP, ID_SEG0 = 100 };

static struct UIState {
    bool panelHidden = false, hudHidden = false, showHidden = true;
    bool seedFocus = false;
    std::string seedText, seedCommitted;
    double hintOp = 1, toastOp = 0, hintUntil = 7000, toastUntil = 0;
    std::string hint, toastS;
    int pressId = 0;
    bool sliderDrag = false;
    double sliderVal = 13;
    std::string sub = "Procedural Eastern Bloc", stats;
    float panelScroll = 0;
    bool loadOn = false;
    double loadOp = 0;
    double bootStart = 0, bootOutAt = -1;
    bool bootGone = false;
} ui;
static void showHint(const std::string& s, double ms = 6000) { ui.hint = s; ui.hintOp = 1; ui.hintUntil = nowMs() + ms; }
static void hideHint() { ui.hintUntil = 0; }
void toast(const std::string& s) { ui.toastS = s; ui.toastUntil = nowMs() + 2600; }
static void setUI(bool on) { ui.panelHidden = !on; ui.hudHidden = !on; ui.showHidden = on; }
static std::string* segVal(const std::string& k) {
    static std::string res;
    if (k == "mode") return &state.mode;
    if (k == "style") return &state.style;
    if (k == "season") return &state.season;
    res = std::to_string(state.res);
    return &res;
}
static void syncMode() {
    if (state.mode != "single" && std::find(SOLO.begin(), SOLO.end(), state.style) != SOLO.end()) state.style = "mixed";
}

/* ================= REGEN ================= */
static double regenAt = -1, lastI = 0;
static bool firstGen = true, wantShot = false, wantExport = false;
static int exportDelayFrames = 0;
static void regen() { ui.loadOn = true; regenAt = nowMs() + 40; }
static bool pendingLive = false; // start Live mode once the scene being built is ready
static void startLife();
static void doRegen() {
    if (game::active()) { game::stop(); homeRec = homeFlat = homeFloor = -1; }
    COL.clear(); LIGHTS.clear(); SMOKE.clear(); SEASON = state.season; B.reset();
    INFO = generate(state.mode, state.style, state.seed);
    hasInfo = true;
    freeMesh(staticMesh);
    staticGeo = std::move(B.V);
    staticMesh = uploadMesh(staticGeo);
    size_t tris = staticGeo.size() / FLOATS_PER_VERT / 3;
    B.reset();
    buildSmoke(); buildLights();
    baseLights = LIGHTS;
    cutSaved.clear();
    freeMesh(intMesh);
    INT = Interior();
    shadowHalf = INFO.half ? INFO.half : 136;
    shadowDirty = true;
    O.base = INFO.dist; O.baseT = INFO.target;
    setView(state.view);
    if (state.walk) spawnWalk();
    char b[96];
    snprintf(b, sizeof b, "%.1fk polys · %d lights", tris / 1000.0, nLights);
    ui.stats = b;
    ui.sub = INFO.label;
    ui.seedText = ui.seedCommitted = numStr(state.seed);
    ui.loadOn = false;
    if (firstGen) { firstGen = false; ui.bootOutAt = nowMs() + 900; }
    if (pendingLive) { pendingLive = false; startLife(); }
}
static void newSeed() { state.seed = std::floor(mrand() * 99999) + 1; regen(); }
static void setWalk(bool on) {
    state.walk = on;
    if (on) {
        if (!hasInfo) return;
        spawnWalk();
        ui.panelHidden = true; ui.showHidden = false;
        showHint("WASD moves · drag to look · Shift runs", 7000);
    } else { Wk.jx = Wk.jy = 0; dropInterior(); }
}
static void endLife() {
    game::stop();
    homeRec = homeFlat = homeFloor = -1;
    setWalk(false);
}
// Live mode needs a Mikrorayon: switch to one first if needed, then start once it is built.
static void startLife() {
    if (state.mode != "district" || !hasInfo) {
        state.mode = "district";
        syncMode();
        pendingLive = true;
        regen();
        return;
    }
    state.clock = false;
    state.walk = true;
    std::string why;
    if (!game::start(state.seed, why)) { toast(why); endLife(); return; }
    ui.panelHidden = true; ui.showHidden = false;
    showHint("E uses things · 1-9 picks · Tab opens your notebook · drag to look", 9000);
}
static void commitSeed() { // input 'change' event
    if (ui.seedText == ui.seedCommitted) return;
    const char* s = ui.seedText.c_str();
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    std::string num;
    if (*s == '+' || *s == '-') num += *s++;
    while (*s >= '0' && *s <= '9') num += *s++;
    double v = (num.empty() || num == "+" || num == "-") ? NAN : std::strtod(num.c_str(), nullptr);
    if (v > 0) { state.seed = v; ui.seedCommitted = ui.seedText; regen(); }
    else ui.seedText = ui.seedCommitted = numStr(state.seed);
}
static void clickSeg(const SegDef& g, const std::string& v) {
    if (g.multi) {
        if (v == "sound") { setSound(!state.sound); return; }
        bool* f = flagRef(v);
        *f = !*f;
        if (v == "shadows") shadowDirty = true;
        if (v == "orbit") lastI = 0;
        return;
    }
    if (g.k == "res") {
        state.res = std::atoi(v.c_str());
        return;
    }
    *segVal(g.k) = v;
    if (g.k == "mode") syncMode();
    if (g.k == "season") applyEnv();
    regen();
}

/* ---- layout (CSS box model of the original page) ---- */
struct SegLayout { int g; std::vector<std::pair<int, Rectangle>> btns; float h; };
static SegLayout layoutSeg(int gi, float x, float y, float w, bool single) {
    const SegDef& g = SEGS[gi];
    SegLayout L{gi, {}, 0};
    std::vector<int> vis;
    for (size_t i = 0; i < g.b.size(); i++) if (g.solo.empty() || !g.solo[i] || single) vis.push_back((int)i);
    const float bh = 18 * 1.05f + 11, gap = 4;
    std::vector<std::vector<int>> lines(1);
    float lw = 0;
    for (int i : vis) {
        float nw = std::min(w, tw(18, g.b[i].second) + 16);
        if (!lines.back().empty() && lw + gap + nw > w + .01f) { lines.emplace_back(); lw = 0; }
        lw += (lines.back().empty() ? 0 : gap) + nw;
        lines.back().push_back(i);
    }
    float cy = y;
    for (auto& ln : lines) {
        float used = 0;
        for (size_t k = 0; k < ln.size(); k++) used += std::min(w, tw(18, g.b[ln[k]].second) + 16) + (k ? gap : 0);
        float extra = (w - used) / ln.size(), cx = x;
        for (int i : ln) {
            float bw = std::min(w, tw(18, g.b[i].second) + 16) + extra;
            L.btns.push_back({i, {cx, cy, bw, bh}});
            cx += bw + gap;
        }
        cy += bh + gap;
    }
    L.h = lines.size() * bh + (lines.size() - 1) * gap;
    return L;
}
struct Layout {
    Rectangle brand, walk, live, view, hide, show, panel, roll, seedLbl, seed, hour, hourL, clock, stats, shot, exp;
    std::vector<SegLayout> segs;
    std::vector<std::pair<std::string, float>> labels; // label text, y
    std::vector<float> labelYs;
    float contentH = 0;
    bool wide = false;
    bool typeRow = true;
} LY;
static void doLayout() {
    float W = (float)GetScreenWidth(), Hh = (float)GetScreenHeight();
    // HUD
    float nw = tw(34, "Panelka", 1), sw = std::min(tw(17, ui.sub), W * .46f);
    LY.brand = {12, 10, std::max(nw, sw) + 32, 8 + 34 * .9f + 2 + 17 * 1.05f + 7};
    float cx = W - 12;
    auto chip = [&](const std::string& s) { float w = tw(19, s) + 26; cx -= w; Rectangle r{cx, 10, w, 36}; cx -= 6; return r; };
    LY.hide = chip("Hide");
    if (game::active()) {
        LY.view = LY.walk = Rectangle{0, 0, 0, 0};
        LY.live = chip("End life");
    } else {
        LY.view = state.walk ? Rectangle{0, 0, 0, 0} : chip("View");
        LY.walk = chip(state.walk ? "Fly" : "Walk");
        LY.live = chip("Live");
    }
    float shw = tw(19, "Show controls") + 26;
    LY.show = {W - 12 - shw, Hh - 12 - 36, shw, 36};
    // panel
    LY.wide = W >= 700;
    float pw = std::min(580.f, W), px = (W - pw) / 2, iw = pw - 32, ix = px + 16;
    bool single = state.mode == "single";
    LY.typeRow = !(state.mode == "old" || state.mode == "village");
    LY.segs.clear(); LY.labels.clear();
    float y = 0; // relative to content top
    const float segH = 18 * 1.05f + 11;
    // row 1: roll + seed
    float rwid = tw(21, "New seed") + 28;
    LY.roll = {ix, y, rwid, 34};
    float sl = tw(17, "Seed");
    LY.seedLbl = {ix + rwid + 8, y, sl, 34};
    LY.seed = {ix + rwid + 8 + sl + 6, y + 3, iw - rwid - 8 - sl - 6, 28};
    y += 34 + 9;
    auto segRow = [&](const char* lbl, int gi, float rightW) {
        SegLayout L = layoutSeg(gi, ix + 54 + 8, y, iw - 62 - rightW, single);
        float h = std::max(L.h, 17.f);
        for (auto& b : L.btns) b.second.y += (h - L.h) / 2;
        LY.labels.push_back({lbl, y + (h - 17) / 2});
        LY.segs.push_back(L);
        float yy = y;
        y += h + 9;
        return std::pair<float, float>(yy, h);
    };
    segRow("Scene", 0, 0);
    if (LY.typeRow) segRow("Type", 1, 0);
    // time row
    {
        float tgw = tw(18, "Run clock") + 16, h = segH;
        LY.labels.push_back({"Time", y + (h - 17) / 2});
        float rx = ix + 62, rwd = iw - 62 - 8 - 46 - 8 - tgw;
        LY.hour = {rx, y + (h - 28) / 2, std::max(80.f, rwd), 28};
        LY.hourL = {rx + rwd + 8, y + (h - 20) / 2, 46, 20};
        LY.clock = {ix + iw - tgw, y, tgw, h};
        y += h + 9;
    }
    segRow("Season", 2, 0);
    segRow("PS1", 3, 0);
    {
        float stw = tw(17, ui.stats);
        auto r = segRow("Lines", 4, stw + 8);
        LY.stats = {ix + iw - stw, r.first + (r.second - 17) / 2, stw, 17};
    }
    {
        float s1 = tw(18, "Save photo") + 16, s2 = tw(18, "Export OBJ") + 16;
        auto r = segRow("Extras", 5, s1 + s2 + 16);
        LY.shot = {ix + iw - s1 - 8 - s2, r.first + (r.second - segH) / 2, s1, segH};
        LY.exp = {ix + iw - s2, r.first + (r.second - segH) / 2, s2, segH};
    }
    y -= 9;
    LY.contentH = y;
    float ph = std::min(y + 28, Hh * .5f);
    float bottom = LY.wide ? 14 : 0;
    LY.panel = {px, Hh - bottom - ph, pw, ph};
    float maxScroll = std::max(0.f, y + 28 - ph);
    ui.panelScroll = (float)clampN(ui.panelScroll, 0, maxScroll);
    float oy = LY.panel.y + 14 - ui.panelScroll;
    auto sh = [&](Rectangle& r) { r.y += oy; };
    sh(LY.roll); sh(LY.seedLbl); sh(LY.seed); sh(LY.hour); sh(LY.hourL); sh(LY.clock); sh(LY.stats); sh(LY.shot); sh(LY.exp);
    for (auto& s : LY.segs) for (auto& b : s.btns) b.second.y += oy;
    for (auto& l : LY.labels) l.second += oy;
}
static bool inR(Vector2 m, Rectangle r) { return r.width > 0 && CheckCollisionPointRec(m, r); }
static bool inPanelClip(Vector2 m) { return inR(m, LY.panel); }
static int hitWidget(Vector2 m) {
    if (!ui.showHidden && inR(m, LY.show)) return ID_SHOW;
    if (!ui.hudHidden) {
        if (inR(m, LY.walk)) return ID_WALK;
        if (inR(m, LY.live)) return ID_LIVE;
        if (!state.walk && inR(m, LY.view)) return ID_VIEW;
        if (inR(m, LY.hide)) return ID_HIDE;
    }
    if (!ui.panelHidden && inPanelClip(m)) {
        if (inR(m, LY.roll)) return ID_ROLL;
        if (inR(m, LY.seed)) return ID_SEED;
        if (inR(m, LY.hour)) return ID_HOUR;
        if (inR(m, LY.clock)) return ID_CLOCK;
        if (inR(m, LY.shot)) return ID_SHOT;
        if (inR(m, LY.exp)) return ID_EXP;
        for (auto& s : LY.segs)
            for (auto& b : s.btns) if (inR(m, b.second)) return ID_SEG0 + s.g * 20 + b.first;
    }
    return 0;
}
static bool overUI(Vector2 m) {
    if (!ui.showHidden && inR(m, LY.show)) return true;
    if (!ui.hudHidden && (inR(m, LY.brand) || inR(m, LY.walk) || inR(m, LY.live) || inR(m, LY.view) || inR(m, LY.hide))) return true;
    if (!ui.panelHidden && inR(m, LY.panel)) return true;
    return false;
}
static void activate(int id) {
    switch (id) {
    case ID_WALK: setWalk(!state.walk); break;
    case ID_LIVE: if (game::active()) endLife(); else startLife(); break;
    case ID_VIEW: setView((state.view + 1) % 3); lastI = nowMs(); break;
    case ID_HIDE: setUI(false); break;
    case ID_SHOW: setUI(true); break;
    case ID_ROLL: newSeed(); break;
    case ID_CLOCK: state.clock = !state.clock; break;
    case ID_SHOT: wantShot = true; break;
    case ID_EXP: if (!staticGeo.empty()) { toast("Packing the scene..."); wantExport = true; exportDelayFrames = 2; } break;
    default:
        if (id >= ID_SEG0) { int g = (id - ID_SEG0) / 20, b = (id - ID_SEG0) % 20; clickSeg(SEGS[g], SEGS[g].b[b].first); }
    }
}
static void setSliderFromMouse(float mx) {
    const Rectangle& r = LY.hour;
    double v = clampN((mx - r.x - 9) / (r.width - 18), 0, 1) * 24;
    v = jsround(v / .05) * .05;
    ui.sliderVal = v;
    state.hour = v; // 'input' event
}

static void drawSegBtn(Rectangle r, const std::string& label, bool on, bool fx) {
    Color bg = on ? (fx ? TH.amber : TH.plate) : TH.enamelDeep;
    Color fg = on ? (fx ? hexc(0x2a1a05) : TH.enamelDeep) : TH.dim;
    rr(r, 6, bg);
    float w = tw(18, label);
    txt(18, label, r.x + (r.width - w) / 2, r.y + 6, 18 * 1.05f, fg);
}
static void drawChip(Rectangle r, const std::string& label, bool on) {
    plate(r, 10, 4, 7, on ? TH.amber : TH.enamel);
    float w = tw(19, label);
    txt(19, label, r.x + (r.width - w) / 2, r.y + 9, 19, on ? hexc(0x2a1a05) : TH.ink);
}
static void drawBootIcon(float x, float y, float s, float alpha) {
    auto R = [&](int rx, int ry, int w, int h, unsigned c) { DrawRectangle((int)(x + rx * s), (int)(y + ry * s), (int)(w * s), (int)(h * s), fade(hexc(c), alpha)); };
    R(1, 3, 26, 19, 0xb9b4a6); R(0, 1, 28, 2, 0x7e796c);
    const int am[4][2] = {{3, 5}, {15, 9}, {21, 13}, {9, 17}};
    for (auto& p : am) R(p[0], p[1], 3, 3, 0xf0a33a);
    const int dk[11][2] = {{9, 5}, {15, 5}, {21, 5}, {3, 9}, {9, 9}, {21, 9}, {3, 13}, {9, 13}, {15, 13}, {3, 17}, {21, 17}};
    for (auto& p : dk) R(p[0], p[1], 3, 3, 0x2a3346);
    R(15, 17, 3, 5, 0x5a3b2e);
}
static void drawUI(double t) {
    float W = (float)GetScreenWidth(), Hh = (float)GetScreenHeight();
    Vector2 m = GetMousePosition();
    // HUD
    if (!ui.hudHidden) {
        plate(LY.brand, 10, 4, 7, TH.enamel);
        txt(34, "Panelka", LY.brand.x + 16, LY.brand.y + 8, 34 * .9f, TH.ink, 1);
        std::string sub = ui.sub;
        float maxW = W * .46f;
        if (tw(17, sub) > maxW) { while (!sub.empty() && tw(17, sub + "…") > maxW) { do sub.pop_back(); while (!sub.empty() && (sub.back() & 0xC0) == 0x80); } sub += "…"; }
        txt(17, sub, LY.brand.x + 16, LY.brand.y + 8 + 34 * .9f + 2, 17 * 1.05f, TH.dim);
        if (game::active()) drawChip(LY.live, "End life", false);
        else {
            drawChip(LY.walk, state.walk ? "Fly" : "Walk", state.walk);
            drawChip(LY.live, "Live", false);
            if (!state.walk) drawChip(LY.view, "View", false);
        }
        drawChip(LY.hide, "Hide", false);
    }
    // panel
    if (!ui.panelHidden) {
        const Rectangle& P = LY.panel;
        if (LY.wide) {
            plate(P, 14, 5, 10, TH.enamel);
        } else {
            Rectangle ext{P.x, P.y, P.width, P.height + 40};
            softShadow(ext, 14, 8, 18, TH.shadow);
            rr(ext, 14, TH.enamel);
            rrLine({P.x + 5, P.y + 5, P.width - 10, P.height + 30}, 10, 2, fade(TH.plate, .9f));
        }
        BeginScissorMode((int)P.x, (int)P.y, (int)P.width, (int)P.height);
        // row 1
        bool rollDown = ui.pressId == ID_ROLL && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && inR(m, LY.roll);
        Rectangle rl = LY.roll;
        if (rollDown) rl.y += 2; else rr({rl.x, rl.y + 2, rl.width, rl.height}, 6, hexc(0x7c1f18));
        rr(rl, 6, TH.red);
        txt(21, "New seed", rl.x + 14, rl.y + 7, 21, WHITE);
        txt(17, "Seed", LY.seedLbl.x, LY.seedLbl.y, 34, TH.dim);
        rr(LY.seed, 6, TH.plate);
        {
            std::string s = ui.seedText;
            float maxw = LY.seed.width - 16;
            size_t start = 0;
            while (start < s.size() && tw(21, s.substr(start)) > maxw) start++;
            std::string vis = s.substr(start);
            txt(21, vis, LY.seed.x + 8, LY.seed.y + 4, 21, TH.enamelDeep);
            if (ui.seedFocus) {
                if (std::fmod(t, 1000) < 500) { float cx = LY.seed.x + 8 + tw(21, vis) + 1; DrawRectangle((int)cx, (int)LY.seed.y + 5, 1, 19, TH.enamelDeep); }
                rrLine({LY.seed.x - 5, LY.seed.y - 5, LY.seed.width + 10, LY.seed.height + 10}, 9, 3, TH.amber);
            }
        }
        // labels
        for (auto& l : LY.labels) txt(17, l.first, LY.panel.x + 16, l.second, 17, TH.dim);
        // segs
        for (auto& s : LY.segs) {
            const SegDef& g = SEGS[s.g];
            for (auto& b : s.btns) {
                const std::string& v = g.b[b.first].first;
                bool on = g.multi ? *flagRef(v) : (*segVal(g.k) == v);
                drawSegBtn(b.second, g.b[b.first].second, on, g.fx);
            }
        }
        // slider
        {
            const Rectangle& r = LY.hour;
            Rectangle tr{r.x, r.y + 10, r.width, 8};
            const float st[6] = {0, .27f, .36f, .68f, .77f, .88f};
            const unsigned sc[6] = {0x12203b, 0xe9876a, 0xb7c9df, 0xb7c9df, 0xe9876a, 0x12203b};
            BeginScissorMode((int)(tr.x + 4), (int)tr.y, (int)(tr.width - 8), 8);
            for (int i = 0; i < 6; i++) {
                float x0 = tr.x + st[i] * tr.width, x1 = i < 5 ? tr.x + st[i + 1] * tr.width : tr.x + tr.width;
                Color c0 = hexc(sc[i]), c1 = hexc(i < 5 ? sc[i + 1] : sc[5]);
                DrawRectangleGradientH((int)x0, (int)tr.y, (int)std::ceil(x1 - x0), 8, c0, c1);
            }
            EndScissorMode();
            BeginScissorMode((int)P.x, (int)P.y, (int)P.width, (int)P.height);
            // rounded ends
            DrawCircle((int)(tr.x + 4), (int)(tr.y + 4), 4, hexc(0x12203b));
            DrawCircle((int)(tr.x + tr.width - 4), (int)(tr.y + 4), 4, hexc(0x12203b));
            float tx = r.x + (float)(ui.sliderVal / 24) * (r.width - 18);
            Rectangle th{tx, r.y + 3, 18, 22};
            rr(th, 4, TH.enamelDeep);
            rr({th.x + 2, th.y + 2, th.width - 4, th.height - 4}, 2, TH.plate);
        }
        {
            std::string hl = fmtH(state.hour);
            txt(20, hl, LY.hourL.x + LY.hourL.width - tw(20, hl), LY.hourL.y, 20, TH.ink);
        }
        drawSegBtn(LY.clock, "Run clock", state.clock, true);
        txt(17, ui.stats, LY.stats.x, LY.stats.y, 17, TH.dim);
        auto act = [&](Rectangle r, const char* s) { rr(r, 6, TH.plate); float w = tw(18, s); txt(18, s, r.x + (r.width - w) / 2, r.y + 6, 18 * 1.05f, TH.enamelDeep); };
        act(LY.shot, "Save photo");
        act(LY.exp, "Export OBJ");
        EndScissorMode();
    }
    if (!ui.showHidden) drawChip(LY.show, "Show controls", false);
    // hint + toast
    auto pill = [&](const std::string& s, float top, double op) {
        if (op <= 0.001 || s.empty()) return;
        float w = std::min(tw(19, s) + 20, W * .94f);
        Rectangle r{(W - w) / 2, top, w, 27};
        rr(r, 4, fade({0, 0, 0, 158}, (float)op));
        txt(19, s, r.x + 10, r.y + 4, 19, fade(WHITE, (float)op));
    };
    pill(ui.hint, 92, ui.hintOp);
    pill(ui.toastS, 124, ui.toastOp);
    // loading
    if (ui.loadOp > 0.001) {
        int dots = (int)(std::fmod(t, 1000) / 250);
        std::string s = "NOW LOADING" + std::string(dots, '.');
        float w = tw(30, s, 2) + 32;
        Rectangle r{(W - w) / 2, (Hh - 42) / 2, w, 42};
        DrawRectangleRec(r, fade(BLACK, (float)ui.loadOp));
        txt(30, s, r.x + 16, r.y + 6, 30, fade(WHITE, (float)ui.loadOp), 2);
    }
    // boot
    if (!ui.bootGone) {
        double op = 1;
        if (ui.bootOutAt >= 0 && t >= ui.bootOutAt) op = 1 - (t - ui.bootOutAt) / 600;
        if (op <= 0) { ui.bootGone = true; return; }
        DrawRectangle(0, 0, (int)W, (int)Hh, fade(BLACK, (float)op));
        double at = (t - ui.bootStart) / 1100;
        double stp = std::floor(clampN(at, 0, 1) * 8) / 8; // steps(8)
        float ry = (float)(14 * (1 - stp)), ra = (float)(stp * op);
        std::string title = "ПАНЕЛЬКА";
        Vector2 ts = MeasureTextEx(bootFont, title.c_str(), 54, 6);
        float totalH = 88 + 10 + 54 + 10 + 20, y0 = (Hh - totalH) / 2;
        drawBootIcon((W - 112) / 2, y0 + ry, 4, ra);
        DrawTextEx(bootFont, title.c_str(), {std::round((W - ts.x - 6) / 2), std::round(y0 + 98 + ry)}, 54, 6, fade(hexc(0xe9e4d6), ra));
        std::string sb = "procedural eastern bloc";
        txt(20, sb, (W - tw(20, sb)) / 2, y0 + 98 + 54 + 10, 20, fade(hexc(0x8f8a7d), (float)op));
    }
}

/* ================= MAIN ================= */
int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "Panelka — PS1 Eastern Bloc generator");
    SetWindowMinSize(320, 240);
    SetExitKey(KEY_NULL);
    SetTargetFPS(0);
    {
        Image ic = GenImageColor(28, 22, BLANK);
        auto R = [&](int x, int y, int w, int h, unsigned c) { ImageDrawRectangle(&ic, x, y, w, h, hexc(c)); };
        R(1, 3, 26, 19, 0xb9b4a6); R(0, 1, 28, 2, 0x7e796c); R(3, 5, 3, 3, 0xf0a33a); R(15, 9, 3, 3, 0xf0a33a); R(9, 5, 3, 3, 0x2a3346); R(15, 17, 3, 5, 0x5a3b2e);
        SetWindowIcon(ic);
        UnloadImage(ic);
    }
    initTheme();
    for (int n = 0; n < 256; n++) { uint32_t c = n; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >> 1) : c >> 1; CRCT[n] = c; }

    std::string dir = GetApplicationDirectory();
    auto asset = [&](const char* f) {
        std::string p = dir + "assets/" + f;
        if (!FileExists(p.c_str())) p = dir + "../assets/" + f;
        return p;
    };
    std::vector<int> cps;
    for (int c = 32; c < 127; c++) cps.push_back(c);
    for (int c = 0xA0; c < 0x180; c++) cps.push_back(c); // Latin-1 + Extended-A: fadas and háčeks (Érinska, Ó Kovač)
    cps.push_back(0x2026);
    std::string vt = asset("VT323-Regular.ttf");
    for (int s : {17, 18, 19, 20, 21, 30, 34}) fonts[s] = FileExists(vt.c_str()) ? LoadFontEx(vt.c_str(), s, cps.data(), (int)cps.size()) : GetFontDefault();
    {
        std::vector<int> bc;
        for (uint32_t c : utf8cp("ПАНЕЛЬКА")) bc.push_back((int)c);
        std::string lm = asset("LiberationMono-Regular.ttf");
        bootFont = FileExists(lm.c_str()) ? LoadFontEx(lm.c_str(), 54, bc.data(), (int)bc.size()) : GetFontDefault();
        SetTextureFilter(bootFont.texture, TEXTURE_FILTER_BILINEAR);
    }

    buildAtlas();
    atlasTex = rlLoadTexture(atlas, AS, AS, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    rlTextureParameters(atlasTex, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST);
    rlTextureParameters(atlasTex, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
    mainP = mkProg(VS, FS); depthP = mkProg(DVS, DFS); skyP = mkProg(SVS, SFS); ptP = mkProg(PVS, PFS);
    crtShader = LoadShaderFromMemory(nullptr, CRTFS);
    shadowFbo = rlLoadFramebuffer();
    shadowDepth = rlLoadTextureDepth(SHADOW_RES, SHADOW_RES, false);
    shadowColor = rlLoadTexture(nullptr, SHADOW_RES, SHADOW_RES, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    rlFramebufferAttach(shadowFbo, shadowColor, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(shadowFbo, shadowDepth, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    if (!rlFramebufferComplete(shadowFbo)) TraceLog(LOG_WARNING, "PANELKA: shadow framebuffer incomplete");
    { std::vector<float> one(4, 0.f); lTex = rlLoadTexture(one.data(), 1, 1, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1); lightRows = -1; cTex = rlLoadTexture(one.data(), 1, 1, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1); }
    glEnable(GL_PROGRAM_POINT_SIZE_);
    buildSky();
    for (int i = 0; i < SN; i++) { snowPos[i * 3] = (float)((mrand() - .5) * 120); snowPos[i * 3 + 1] = (float)(mrand() * 50); snowPos[i * 3 + 2] = (float)((mrand() - .5) * 120); }
    snowBuf = mkPoints(SN);

    game::host.interior = []() -> const Interior& { return INT; };
    game::host.loadHome = [](int rec, int flat, int floor) { homeRec = rec; homeFlat = flat; homeFloor = floor; activateInterior(rec); };
    game::host.place = [](double x, double z, double feet, double yaw, double pitch) {
        Wk.x = x; Wk.z = z; Wk.feet = feet; Wk.yaw = yaw; Wk.pitch = pitch; Wk.vy = 0; Wk.jx = Wk.jy = 0;
    };
    game::host.blocked = [](double x, double z, double feet) { return blocked(x, z, feet); };
    game::host.roomLight = setRoomLight;
    game::host.setHour = [](double h) { state.hour = h; ui.sliderVal = h; };

    state.seed = std::floor(mrand() * 99999) + 1;
    // Debug hooks: PANELKA_SEED/MODE/STYLE/SEASON/HOUR/VIEW/WALK/FX, PANELKA_AUTOSHOT=<png> (capture window, then exit)
    const char* autoShot = getenv("PANELKA_AUTOSHOT");
    if (const char* e = getenv("PANELKA_SEED")) state.seed = std::atof(e);
    if (const char* e = getenv("PANELKA_MODE")) state.mode = e;
    if (const char* e = getenv("PANELKA_STYLE")) state.style = e;
    if (const char* e = getenv("PANELKA_SEASON")) state.season = e;
    if (const char* e = getenv("PANELKA_HOUR")) state.hour = std::atof(e);
    if (const char* e = getenv("PANELKA_VIEW")) state.view = std::atoi(e);
    if (const char* e = getenv("PANELKA_FX")) for (auto& f : {"snap", "affine", "dither", "crt", "fps30", "shadows", "points", "orbit"}) { if (strstr(e, (std::string("-") + f).c_str())) *flagRef(f) = false; else if (strstr(e, (std::string("+") + f).c_str())) *flagRef(f) = true; }
    int autoFrames = 0;
    ui.hint = "Drag to orbit · scroll to zoom · right-drag to pan";
    ui.bootStart = nowMs();
    ui.hintUntil = nowMs() + 7000;
    ui.sliderVal = state.hour;
    applyEnv();
    syncMode();
    regen();

    double lastR = nowMs(), hourSync = 0;
    struct Ptr { bool active = false; int b = 0; bool sh = false; Vector2 p{}; } ptr;
    bool bootFirstFrame = true;

    while (!WindowShouldClose()) {
        double t = nowMs();
        // generation runs only after the loading overlay (or boot screen) has been on screen
        if (regenAt >= 0 && t >= regenAt && !bootFirstFrame) { regenAt = -1; doRegen(); t = nowMs(); lastR = t; }
        bootFirstFrame = false;

        /* ---- render-resolution (resize) ---- */
        int sw = GetScreenWidth(), shh = GetScreenHeight();
        double s = std::min(sw, shh) / (double)state.res;
        int nrw = std::max(1, (int)jsround(sw / s)), nrh = std::max(1, (int)jsround(shh / s));
        if (nrw != rw || nrh != rh || rt.id == 0) {
            if (rt.id) UnloadRenderTexture(rt);
            rw = nrw; rh = nrh;
            rt = LoadRenderTexture(rw, rh);
            SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);
        }

        /* ---- input ---- */
        doLayout();
        Vector2 m = GetMousePosition(), md = GetMouseDelta();
        bool bootBlocks = !ui.bootGone && (ui.bootOutAt < 0 || t < ui.bootOutAt);
        if (!IsWindowFocused()) keys.clear();
        int hover = bootBlocks ? 0 : hitWidget(m);
        SetMouseCursor(hover == ID_SEED ? MOUSE_CURSOR_IBEAM : hover ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
        bool pressL = IsMouseButtonPressed(MOUSE_BUTTON_LEFT), pressR = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT), pressM = IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
        if (bootBlocks) {
            if (pressL && ui.bootOutAt < 0) ui.bootOutAt = t;
            else if (pressL) ui.bootOutAt = std::min(ui.bootOutAt, t);
        } else {
            if ((pressL || pressR || pressM) && !ptr.active) {
                int hitId = hitWidget(m);
                if (hitId != ID_SEED && ui.seedFocus) { ui.seedFocus = false; commitSeed(); }
                if (hitId && pressL) {
                    ui.pressId = hitId;
                    if (hitId == ID_SEED) ui.seedFocus = true;
                    if (hitId == ID_HOUR) { ui.sliderDrag = true; setSliderFromMouse(m.x); }
                } else if (!overUI(m) && !game::wantsMouse()) {
                    ptr.active = true;
                    ptr.b = pressR ? 2 : pressM ? 1 : 0;
                    ptr.sh = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                    ptr.p = m;
                    lastI = t;
                    hideHint();
                }
            }
            if (ui.sliderDrag) {
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { if (md.x != 0) setSliderFromMouse(m.x); }
                else ui.sliderDrag = false;
            }
            if (ui.pressId && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                if (ui.pressId != ID_SEED && ui.pressId != ID_HOUR && hitWidget(m) == ui.pressId) activate(ui.pressId);
                ui.pressId = 0;
            }
            if (ptr.active) {
                double dx = m.x - ptr.p.x, dy = m.y - ptr.p.y;
                ptr.p = m;
                if (dx != 0 || dy != 0) {
                    lastI = t;
                    if (state.walk) { if (!game::freezeWalker()) { Wk.yaw -= dx * .005; Wk.pitch = clampN(Wk.pitch - dy * .004, -1.2, 1.2); } }
                    else if (ptr.b == 2 || ptr.sh) pan(dx, dy);
                    else { O.yaw -= dx * .006; O.pitch = clampN(O.pitch + dy * .005, .02, 1.45); }
                }
                int btn = ptr.b == 2 ? MOUSE_BUTTON_RIGHT : ptr.b == 1 ? MOUSE_BUTTON_MIDDLE : MOUSE_BUTTON_LEFT;
                if (!IsMouseButtonDown(btn)) ptr.active = false;
            }
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                if (!ui.panelHidden && inR(m, LY.panel)) ui.panelScroll -= wheel * 40;
                else if (!overUI(m) && !state.walk) {
                    O.dist = clampN(O.dist * std::exp(-wheel * 100 * .0012), 6, 480);
                    lastI = t;
                    hideHint();
                }
            }
        }
        // keyboard
        {
            static const std::pair<int, const char*> KM[] = {{KEY_W, "w"}, {KEY_A, "a"}, {KEY_S, "s"}, {KEY_D, "d"}, {KEY_UP, "arrowup"}, {KEY_DOWN, "arrowdown"}, {KEY_LEFT, "arrowleft"}, {KEY_RIGHT, "arrowright"}, {KEY_R, "r"}, {KEY_H, "h"}};
            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            for (auto& k : KM) if (IsKeyReleased(k.first)) keys[k.second] = false;
            if (IsKeyReleased(KEY_LEFT_SHIFT) || IsKeyReleased(KEY_RIGHT_SHIFT)) keys["shift"] = shift;
            if (ui.seedFocus) {
                int ch;
                while ((ch = GetCharPressed()) > 0) {
                    int n = 0;
                    const char* u8 = CodepointToUTF8(ch, &n);
                    ui.seedText.append(u8, n);
                }
                if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !ui.seedText.empty()) {
                    do ui.seedText.pop_back(); while (!ui.seedText.empty() && (ui.seedText.back() & 0xC0) == 0x80);
                }
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) commitSeed();
            } else {
                while (GetCharPressed() > 0) {}
                for (auto& k : KM)
                    if (IsKeyPressed(k.first)) {
                        keys[k.second] = true;
                        keys["shift"] = shift;
                        if (!state.walk) {
                            if (k.first == KEY_R) newSeed();
                            if (k.first == KEY_H) setUI(ui.panelHidden);
                        }
                    }
                if (IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT)) keys["shift"] = true;
            }
        }

        /* ---- timers ---- */
        if (ui.hintUntil && t > ui.hintUntil) ui.hintUntil = 0;
        double dtf = GetFrameTime();
        ui.hintOp = clampN(ui.hintOp + (ui.hintUntil ? 1 : -1) * dtf / .5, 0, 1);
        ui.toastOp = clampN(ui.toastOp + (t < ui.toastUntil ? 1 : -1) * dtf / .5, 0, 1);
        ui.loadOp = clampN(ui.loadOp + (ui.loadOn ? 1 : -1) * dtf / .15, 0, 1);

        /* ---- scene update + render (the 30 fps toggle skips whole frames like the page) ---- */
        bool renderScene = !(state.fps30 && t - lastR < 31);
        if (renderScene) {
            double dt = std::min(.1, (t - lastR) / 1000);
            lastR = t;
            if (state.clock) {
                state.hour = std::fmod(state.hour + dt * 24 / 240, 24);
                if (t - hourSync > 250) { hourSync = t; ui.sliderVal = jsround(state.hour / .05) * .05; }
            }
            applyEnv();
            // particles
            if (!smokeAge.empty()) {
                for (size_t i = 0; i < smokeAge.size(); i++) {
                    smokeAge[i] += dt * .035;
                    if (smokeAge[i] > 1) smokeAge[i] -= 1;
                    const V3& e = smokeSrc[i / 40];
                    double g = smokeAge[i];
                    smokePos[i * 3] = (float)(e[0] + g * g * 70 + std::sin(i * 1.3) * g * 7);
                    smokePos[i * 3 + 1] = (float)(e[1] + g * 80);
                    smokePos[i * 3 + 2] = (float)(e[2] + std::cos(i * 1.7) * g * 7);
                }
                rlUpdateVertexBuffer(smokeBuf.vbo, smokePos.data(), (int)(smokePos.size() * 4), 0);
            }
            if (state.walk) {
                walkStep(dt);
                game::update(dt, camPos, camLook, Wk.feet);
            }
            else {
                if (state.orbit && !ptr.active && t - lastI > 2500) O.yaw += dt * .06;
                updOrbit();
            }
            if (U.snowVis) {
                V3 c = camPos;
                for (int i = 0; i < SN; i++) {
                    double x = snowPos[i * 3], y = snowPos[i * 3 + 1], z = snowPos[i * 3 + 2];
                    y -= dt * (1.3 + (i % 7) * .12);
                    x += std::sin(i + t * .0007) * dt * .4;
                    if (x - c[0] > 60) x -= 120; else if (x - c[0] < -60) x += 120;
                    if (z - c[2] > 60) z -= 120; else if (z - c[2] < -60) z += 120;
                    if (y < c[1] - 12 || y < 0) y += 50;
                    if (y > c[1] + 40) y -= 50;
                    snowPos[i * 3] = (float)x; snowPos[i * 3 + 1] = (float)y; snowPos[i * 3 + 2] = (float)z;
                }
                rlUpdateVertexBuffer(snowBuf.vbo, snowPos.data(), SN * 12, 0);
            }
            double wf = state.season == "winter" ? .85 : 1;
            if (state.walk) { U.fogNear = 25 * wf; U.fogFar = 210 * wf; }
            else { U.fogNear = std::max(30.0, O.dist * .35) * wf; U.fogFar = std::max(240.0, 130 + O.dist * 1.6) * wf; }
            float time = (float)(t / 1000);
            if (state.shadows && (shadowDirty || std::fabs(state.hour - shadowHour) > .02)) renderShadow();

            M4 proj = perspective(60, (double)sw / shh, state.walk ? .08 : .4, 3000), view = lookAt(camPos, camLook);
            BeginTextureMode(rt);
            rlClearColor(0, 0, 0, 255);
            rlClearScreenBuffers();
            rlDrawRenderBatchActive();
            rlDisableColorBlend();
            // sky (renderOrder -10, no depth)
            {
                M4 vr = view;
                vr.m[12] = vr.m[13] = vr.m[14] = 0;
                rlDisableDepthTest();
                rlDisableBackfaceCulling();
                rlEnableShader(skyP.id);
                skyP.m("uViewRot", vr); skyP.m("projectionMatrix", proj);
                skyP.v3("uHor", U.sHor); skyP.v3("uZen", U.sZen); skyP.v3("uSunDir", U.sunDir); skyP.v3("uSunC", U.sSunC); skyP.v3("uCloudC", U.sCloudC);
                skyP.f("uCloud", (float)U.sCloud); skyP.f("uStars", (float)U.sStars); skyP.f("uTime", time); skyP.f("uQ", state.dither ? 1.f : 0.f); skyP.f("uMoon", (float)U.sMoon);
                rlEnableVertexArray(skyMesh.vao);
                rlDrawVertexArray(0, skyMesh.count);
                rlDisableVertexArray();
                rlDisableShader();
            }
            rlEnableDepthTest();
            glDepthFunc(0x0203); // GL_LEQUAL, as three.js
            rlEnableDepthMask();
            rlEnableBackfaceCulling();
            if (staticMesh.count) {
                Prog& p = mainP;
                rlEnableShader(p.id);
                p.m("viewMatrix", view); p.m("projectionMatrix", proj);
                p.v2("uRes", (float)rw, (float)rh);
                p.f("uSnap", state.snap); p.f("uAffine", state.affine); p.f("uQ", state.dither);
                p.f("uTime", time); p.f("uLights", (float)U.lights); p.f("uSnow", (float)U.snow);
                p.v3("uSunDir", U.sunDir); p.v3("uSunCol", U.sunCol); p.v3("uSky", U.sky); p.v3("uGnd", U.gnd); p.v3("uFog", U.fog);
                p.f("uFogNear", (float)U.fogNear); p.f("uFogFar", (float)U.fogFar);
                p.m("uShadowMat", shadowMat); p.f("uShadowOn", state.shadows); p.f("uShadowTexel", 1.f / SHADOW_RES);
                p.f("uPointOn", state.points); p.f("uInterior", 0);
                p.v3("uGridMin", {GRID_MIN[0], GRID_MIN[1], GRID_MIN[2]}); p.v3("uGridN", {(double)GRID_N[0], (double)GRID_N[1], (double)GRID_N[2]}); p.f("uCell", (float)GRID_CELL);
                p.i("uLW", LW); p.i("uCW", CW);
                p.tex("uTex", 0, atlasTex); p.tex("uShadow", 1, shadowDepth); p.tex("uLTex", 2, lTex); p.tex("uCTex", 3, cTex);
                rlEnableVertexArray(staticMesh.vao);
                rlDrawVertexArray(0, staticMesh.count);
                if (state.walk && intMesh.count) {
                    p.f("uInterior", 1);
                    rlEnableVertexArray(intMesh.vao);
                    rlDrawVertexArray(0, intMesh.count);
                }
                rlDisableVertexArray();
                rlDisableShader();
                for (int sl = 3; sl >= 0; sl--) { rlActiveTextureSlot(sl); rlDisableTexture(); }
            }
            // snow (opaque points, no depth write) then smoke (transparent)
            rlDisableDepthMask();
            auto drawPts = [&](PointBuf& b, float size, V3 col, float op) {
                rlEnableShader(ptP.id);
                ptP.m("viewMatrix", view); ptP.m("projectionMatrix", proj);
                ptP.f("size", size); ptP.f("scale", rh * .5f); ptP.v3("diffuse", col); ptP.f("opacity", op);
                rlEnableVertexArray(b.vao);
                glDrawArrays(GL_POINTS_, 0, b.n);
                rlDisableVertexArray();
                rlDisableShader();
            };
            if (U.snowVis && !(state.walk && insideInterior(camPos[0], camPos[2]))) drawPts(snowBuf, .32f, U.snowCol, 1);
            if (smokeBuf.vao) {
                rlEnableColorBlend();
                rlSetBlendFactorsSeparate(0x0302, 0x0303, 1, 0x0303, 0x8006, 0x8006);
                rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
                drawPts(smokeBuf, 16, U.smokeCol, .5f);
                rlSetBlendMode(RL_BLEND_ALPHA);
            }
            rlEnableDepthMask();
            glDepthFunc(0x0201); // back to raylib's GL_LESS
            rlDisableDepthTest();
            rlEnableColorBlend();
            EndTextureMode();
            if (wantShot) { wantShot = false; captureShot(); }
        }

        /* ---- composite ---- */
        BeginDrawing();
        ClearBackground(BLACK);
        rlDrawRenderBatchActive();
        rlDisableColorBlend();
        DrawTexturePro(rt.texture, {0, 0, (float)rw, (float)-rh}, {0, 0, (float)sw, (float)shh}, {0, 0}, 0, WHITE);
        rlDrawRenderBatchActive();
        rlEnableColorBlend();
        if (state.crt) {
            float scr[2] = {(float)sw, (float)shh}, sl = (float)shh / rh;
            SetShaderValue(crtShader, GetShaderLocation(crtShader, "uScreen"), scr, SHADER_UNIFORM_VEC2);
            SetShaderValue(crtShader, GetShaderLocation(crtShader, "uSl"), &sl, SHADER_UNIFORM_FLOAT);
            BeginShaderMode(crtShader);
            DrawRectangle(0, 0, sw, shh, WHITE);
            EndShaderMode();
        }
        game::draw(sw, shh);
        drawUI(t);
        EndDrawing();

        if (wantExport && --exportDelayFrames <= 0) { wantExport = false; exportObj(); }
        if (autoShot && ui.bootGone) {
            if (autoFrames == 1 && getenv("PANELKA_WALK")) setWalk(true);
            if (autoFrames == 1 && getenv("PANELKA_LIVE")) startLife();
            if (autoFrames == 2 && state.walk)
                if (const char* p = getenv("PANELKA_POS")) { // "x,z,yaw,pitch": stand anywhere (world, radians)
                    double x = 0, z = 0, yw = 0, pt = 0;
                    sscanf(p, "%lf,%lf,%lf,%lf", &x, &z, &yw, &pt);
                    Wk.x = x; Wk.z = z; Wk.yaw = yw; Wk.pitch = pt; Wk.feet = 0; Wk.vy = 0;
                }
            if (autoFrames == 2 && game::active()) { // PANELKA_LIVE_RUN="Stove:1,wait:30" · PANELKA_LIVE_LOOK=Stove
                if (const char* r = getenv("PANELKA_LIVE_RUN")) game::runScript(r);
                if (const char* l = getenv("PANELKA_LIVE_LOOK"))
                    for (int k = 1; k <= (int)sim::Obj::Kiosk; k++)
                        if (std::string(sim::objName((sim::Obj)k)) == l) game::faceObject((sim::Obj)k);
            }
            if (autoFrames == 2 && getenv("PANELKA_ENTER")) { // "<rec>,<floor>,<spot 0 lobby|1 landing|2 flat|3 church/izba>"
                int ri = 0, k = 0, spot = 0;
                sscanf(getenv("PANELKA_ENTER"), "%d,%d,%d", &ri, &k, &spot);
                if (spot == 3) { for (size_t i = 0; i < RECS.size(); i++) if (RECS[i].kind != 0) { ri = (int)i; break; } }
                else { int n = 0; for (size_t i = 0; i < RECS.size(); i++) if (RECS[i].kind == 0 && n++ == ri) { ri = (int)i; break; } }
                const BuildingRec& R = RECS[ri];
                double lx = 0, lz = 0, ly = 0, dirx = 0, dirz = 0;
                if (R.kind == 0) {
                    double bw = R.w / R.bays, hw = R.w / 2, hd = R.d / 2, Ls = R.d / 2;
                    int j = R.entr[0];
                    double a = (j + .5) * bw, e = spot == 0 ? .9 : spot == 1 ? Ls - .8 : 1.6;
                    if (spot == 2) a = (j + (j + 2 < R.bays ? 2.0 : -1.0)) * bw;
                    ly = spot == 0 ? 0 : R.plinth + k * R.fh;
                    double sgn = R.entrSide == 0 ? 1 : -1;
                    lx = sgn * (-hw + a); lz = sgn * (hd - e);
                    dirx = 0; dirz = spot == 0 ? -sgn : sgn;
                } else if (R.kind == 1) { lz = 11.0; dirz = -1; }
                else { lx = R.ihw + .9; lz = R.ipz; ly = R.ifb; dirx = -1; }
                activateInterior(ri);
                Wk.x = R.ox + lx * R.cs + lz * R.sn; Wk.z = R.oz - lx * R.sn + lz * R.cs; Wk.feet = ly;
                double wx = dirx * R.cs + dirz * R.sn, wz = -dirx * R.sn + dirz * R.cs;
                Wk.yaw = std::atan2(-wx, -wz);
                if (const char* y = getenv("PANELKA_YAW")) Wk.yaw += std::atof(y);
                if (const char* y = getenv("PANELKA_PITCH")) Wk.pitch = std::atof(y);
            }
            if (++autoFrames == 40) { TakeScreenshot(autoShot); break; }
        }
        auTick(t);
    }
    if (AU.ctx) { UnloadAudioStream(AU.stream); CloseAudioDevice(); }
    CloseWindow();
    return 0;
}

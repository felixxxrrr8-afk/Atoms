// ===================================== МИР ЯДЕР: РАСПАД, ДЕЛЕНИЕ, ЦЕПНАЯ РЕАКЦИЯ ===============
// Ядра здесь — сгустки протонов и нейтронов, а не атомы: химия на них не влияет, важны только ядерные силы.
// Распад случаен: каждое ядро за время dt распадается с вероятностью λ·dt, λ = ln2/T½ — отсюда экспонента N(t).
// Нейтроны переносятся методом Монте-Карло, как в расчётах реакторов: свободный пробег до столкновения выбирается
// из экспоненты с макроскопическим сечением Σ = Σ nᵢσᵢ(E) (реальные плотности ядер и сечения), в столкновении —
// рассеяние (нейтрон теряет энергию, на лёгких ядрах водорода — больше всего), захват или деление U-235:
// два осколка, 2–3 новых нейтрона и ≈200 МэВ. Если в среднем от деления до деления доживает больше одного нейтрона
// (k > 1) — число нейтронов растёт экспоненциально: цепная реакция.
// Единицы мира: длина — см, время — нс или с (для распада), энергия — МэВ. Ядра на рисунке увеличены в сотни тысяч раз.
namespace nuc {
// ---- изотопы: реальные периоды полураспада (NUBASE 2020), тип и энергия распада
enum { D_STABLE, D_ALPHA, D_BETAM, D_BETAP, D_GAMMA };
struct Iso { const char* name; int Z, A; double T12; int mode; double Q; int daughter; };
// daughter — индекс дочернего изотопа в этой таблице (−1 — вычисляется из Z, A)
static const Iso ISO[] = {
    {"U-238", 92, 238, 1.409e17, D_ALPHA, 4.27, 2},        // 0: 4.468 млрд лет
    {"U-235", 92, 235, 2.221e16, D_ALPHA, 4.68, -1},       // 1: 704 млн лет
    {"Th-234", 90, 234, 2.082e6, D_BETAM, 0.27, 3},        // 2: 24.1 сут
    {"Pa-234m", 91, 234, 70.2, D_BETAM, 2.27, 4},          // 3: 1.17 мин
    {"U-234", 92, 234, 7.747e12, D_ALPHA, 4.86, 5},        // 4: 245 тыс. лет
    {"Th-230", 90, 230, 2.379e12, D_ALPHA, 4.77, 6},       // 5: 75.4 тыс. лет
    {"Ra-226", 88, 226, 5.049e10, D_ALPHA, 4.87, 7},       // 6: 1600 лет
    {"Rn-222", 86, 222, 3.304e5, D_ALPHA, 5.59, 8},        // 7: 3.82 сут
    {"Po-218", 84, 218, 185.9, D_ALPHA, 6.11, 9},          // 8: 3.10 мин
    {"Pb-214", 82, 214, 1608, D_BETAM, 1.02, 10},          // 9: 26.8 мин
    {"Bi-214", 83, 214, 1194, D_BETAM, 3.27, 11},          // 10: 19.9 мин
    {"Po-214", 84, 214, 1.643e-4, D_ALPHA, 7.83, 12},      // 11: 164 мкс
    {"Pb-210", 82, 210, 7.006e8, D_BETAM, 0.064, 13},      // 12: 22.2 года
    {"Bi-210", 83, 210, 4.330e5, D_BETAM, 1.16, 14},       // 13: 5.01 сут
    {"Po-210", 84, 210, 1.1956e7, D_ALPHA, 5.41, 15},      // 14: 138 сут
    {"Pb-206", 82, 206, 0, D_STABLE, 0, -1},               // 15
    {"I-131", 53, 131, 6.930e5, D_BETAM, 0.97, 17},        // 16: 8.02 сут
    {"Xe-131", 54, 131, 0, D_STABLE, 0, -1},               // 17
    {"Cs-137", 55, 137, 9.492e8, D_BETAM, 1.18, 19},       // 18: 30.1 года
    {"Ba-137m", 56, 137, 153.1, D_GAMMA, 0.662, 20},       // 19: 2.55 мин
    {"Ba-137", 56, 137, 0, D_STABLE, 0, -1},               // 20
    {"C-14", 6, 14, 1.808e11, D_BETAM, 0.156, 22},         // 21: 5730 лет
    {"N-14", 7, 14, 0, D_STABLE, 0, -1},                   // 22
    {"Co-60", 27, 60, 1.663e8, D_BETAM, 2.82, 24},         // 23: 5.27 года
    {"Ni-60", 28, 60, 0, D_STABLE, 0, -1},                 // 24
    {"F-18", 9, 18, 6586, D_BETAP, 1.66, 26},              // 25: 110 мин (позитронная томография)
    {"O-18", 8, 18, 0, D_STABLE, 0, -1},                   // 26
    {"Pu-239", 94, 239, 7.609e11, D_ALPHA, 5.24, -1},      // 27: 24 тыс. лет
    {"Ba-141", 56, 141, 1096, D_BETAM, 3.2, -1},           // 28: осколки деления
    {"Kr-92", 36, 92, 1.84, D_BETAM, 6.0, -1},
    {"Xe-140", 54, 140, 13.6, D_BETAM, 4.1, -1},
    {"Sr-94", 38, 94, 75.3, D_BETAM, 3.5, -1},
    {"Cs-137", 55, 137, 9.492e8, D_BETAM, 1.18, 19},
    {"Rb-96", 37, 96, 0.203, D_BETAM, 11.6, -1},
    {"Te-134", 52, 134, 2508, D_BETAM, 1.5, -1},
    {"Zr-100", 40, 100, 7.1, D_BETAM, 3.3, -1},
};
constexpr int NISO = (int)(sizeof(ISO) / sizeof(ISO[0]));
static const char* ELSYM(int Z) { return Z >= 1 && Z <= 118 ? ZD[Z].sym : "?"; }
// ---- ядра на сцене
struct Nuc { double x, y, z, vx, vy, vz; int iso; int Z, A; float split = -1; float glow = 0; bool alive = true; };
enum { P_N, P_ALPHA, P_EM, P_EP, P_NU, P_GAMMA, P_FRAG };
struct Part { double x, y, z, dx, dy, dz, E; float life; int kind; int gen = 0; double tau = 0; double w = 1; int Z = 0, A = 0; };
static std::vector<Nuc> nuclei;
static std::vector<Part> parts;
static double t = 0;                // время мира, с (распад) или нс (нейтроны)
static int scene = 0;               // 0 период полураспада, 1 цепочка радона, 2 цепная реакция (быстрые нейтроны), 3 реактор, 4 разлёт сборки
static double timeScale = 1;        // сколько секунд мира за секунду экрана (распад) или нс (нейтроны)
static int isoPick = 16;            // изотоп для сцены «период полураспада»
static int N0 = 0;                  // начальное число ядер
static std::vector<float> histN, histT, histTheory;   // N(t) и теория N0·2^(−t/T½)
static std::vector<float> histNeut;                   // число нейтронов (логарифм) во времени
static std::vector<std::array<int, 16>> chainHist;    // населённости цепочки радона
static int histStride = 1, histSkip = 0;              // история прореживается вдвое при переполнении: на графике весь опыт от t = 0
// ---- материалы и области для переноса нейтронов (реальные плотности ядер, 1/см³)
enum { M_VOID, M_U235, M_UO2, M_WATER, M_GRAPH, M_B4C, M_N };
struct Region { int shape; double c[3], s[3]; int mat; };   // shape: 0 шар (s[0] — радиус), 1 коробка (s — полуразмеры), 2 цилиндр по y (s[0] — радиус, s[1] — полувысота)
static std::vector<Region> regions;
static double enrich = 0.9;         // обогащение урана по U-235 (доля)
static double sphereR = 9.0;        // радиус сферы урана, см (критический для U-235 ≈ 8.7 см)
static double rodIns = 0.0;         // регулирующие стержни: доля введения
static bool moderator = true;
static double fissions = 0, energyMeV = 0, neutronsW = 0; static long long births = 0;
// поколения нейтронов (кольцо): сколько рождено и сколько ещё летит; k = рождено в g+1 / рождено в g по последнему
// поколению, все нейтроны которого уже поглотились или вылетели (их дети учтены полностью)
constexpr int GEN_RING = 512;
static double genBorn[GEN_RING], genAlive[GEN_RING]; static int genTop = 0;
static inline void genAdd(int g, double w) { if (g > genTop) { for (int q = genTop + 1; q <= g; q++) genBorn[q % GEN_RING] = genAlive[q % GEN_RING] = 0; genTop = g; } genBorn[g % GEN_RING] += w; genAlive[g % GEN_RING] += w; }
static inline void genDie(int g, double w) { genAlive[g % GEN_RING] -= w; }
static double expandR = 0, expandV = 0, eInt = 0; static bool exploding = false;   // разлёт сверхкритической сборки
static std::vector<std::array<float, 5>> fisFlash;              // x y z возраст сила
static std::mt19937_64 rnd(99);
static inline double U() { return std::uniform_real_distribution<double>(0.0, 1.0)(rnd); }
static inline void isoDir(double& dx, double& dy, double& dz) { const double c = 2 * U() - 1, s = std::sqrt(1 - c * c), p = 2 * PI * U(); dx = s * std::cos(p); dy = s * std::sin(p); dz = c; }

// ---- сечения, барн (1 барн = 10⁻²⁴ см²) как функции энергии нейтрона E (МэВ): тепловые значения при 0.0253 эВ,
//      закон 1/v для поглощения, быстрые — усреднённые по спектру деления (ENDF/B-VIII)
static inline double inv_v(double E) { return std::sqrt(2.53e-8 / std::max(E, 1e-12)); }
struct XS { double f, c, s; };   // деление, захват, рассеяние
static XS xsU235(double E) { return {std::max(1.2, 585.0 * inv_v(E)), std::max(0.09, 99.0 * inv_v(E)), E < 1e-4 ? 15.0 : 4.5}; }
static XS xsU238(double E) {
    double c = std::max(0.07, 2.68 * inv_v(E));
    // резонансный захват 6.7 эВ … 200 эВ: резонансы узкие, и в толстом твэле поток на них проседает (самоэкранирование) —
    // эффективно ≈ 8 барн, резонансный интеграл ≈ 30 барн вместо 275 у бесконечно разбавленного урана
    if (E > 5e-6 && E < 2e-4) c += 8;
    return {E > 1.4 ? 0.55 : 0.0, c, E < 1e-4 ? 9.0 : 5.0};
}
static XS xsH(double E) { return {0, 0.332 * inv_v(E), 20.0 / std::sqrt(1 + E / 0.1)}; }
static XS xsO(double E) { return {0, 1.9e-4 * inv_v(E), E < 1e-3 ? 3.8 : 2.5}; }
static XS xsC(double E) { return {0, 0.0035 * inv_v(E), E < 1e-3 ? 4.7 : 2.6}; }
static XS xsB10(double E) { return {0, 3840.0 * inv_v(E), 2.0}; }
// состав материала: до трёх нуклидов (A, плотность ядер 1/см³)
struct Comp { int n; int A[3]; double dens[3]; };
static Comp matComp(int m) {
    switch (m) {
    case M_U235: return {2, {235, 238, 0}, {4.88e22 * enrich, 4.88e22 * (1 - enrich), 0}};           // металл 19 г/см³
    case M_UO2: return {3, {235, 238, 16}, {2.34e22 * enrich, 2.34e22 * (1 - enrich), 4.68e22}};    // топливо UO2
    case M_WATER: return {2, {1, 16, 0}, {6.69e22, 3.34e22, 0}};
    case M_GRAPH: return {1, {12, 0, 0}, {8.5e22, 0, 0}};
    case M_B4C: return {2, {10, 12, 0}, {2.2e22, 2.75e22, 0}};   // карбид бора стержней (природный бор, 20% B-10)
    }
    return {0, {0, 0, 0}, {0, 0, 0}};
}
static XS xsOf(int A, double E) {
    switch (A) { case 235: return xsU235(E); case 238: return xsU238(E); case 1: return xsH(E); case 16: return xsO(E); case 12: return xsC(E); case 10: return xsB10(E); }
    return {0, 0, 0};
}
// плотность при разлёте сборки (масса та же, радиус больше)
static double densityScale() { return exploding && expandR > 0 ? std::pow(sphereR / expandR, 3.0) : 1.0; }
static int regionAt(double x, double y, double z) {
    for (int k = (int)regions.size() - 1; k >= 0; k--) {   // последние — поверх (стержни внутри воды)
        const Region& r = regions[k]; const double dx = x - r.c[0], dy = y - r.c[1], dz = z - r.c[2];
        const double R = r.shape == 0 && r.mat == M_U235 && exploding ? expandR : r.s[0];
        if (r.shape == 0 && dx * dx + dy * dy + dz * dz < R * R) return k;
        if (r.shape == 1 && std::fabs(dx) < r.s[0] && std::fabs(dy) < r.s[1] && std::fabs(dz) < r.s[2]) return k;
        if (r.shape == 2 && dx * dx + dz * dz < r.s[0] * r.s[0] && std::fabs(dy) < r.s[1]) return k;
    }
    return -1;
}
static double sigmaTot(int reg, double E) {
    if (reg < 0) return 0;
    const Comp c = matComp(regions[reg].mat); const double ds = regions[reg].mat == M_U235 ? densityScale() : 1.0;
    double S = 0; for (int q = 0; q < c.n; q++) { const XS x = xsOf(c.A[q], E); S += c.dens[q] * ds * (x.f + x.c + x.s) * 1e-24; }
    return S;
}
// энергия нейтрона деления по спектру Уатта: p(E) ∝ e^{−E/0.988}·sinh√(2.249E)
static double wattE() {
    for (;;) { const double E = -1.5 * std::log(std::max(1e-12, U())) + 0.01 * U(); if (U() * 3.0 < std::exp(-E / 0.988) * std::sinh(std::sqrt(2.249 * E)) / std::exp(-E / 1.5) * 0.9) return std::min(E, 12.0); }
}
static inline double speedCmNs(double E) { return 1.383 * std::sqrt(E); }   // v = √(2E/m): 1 МэВ → 1.38 см/нс
// ---- декоративные ядра: сгустки нуклонов (кэш упаковки по A)
static const std::vector<std::array<float, 3>>& nucleonPack(int A) {
    static std::map<int, std::vector<std::array<float, 3>>> cache;
    auto it = cache.find(A); if (it != cache.end()) return it->second;
    std::vector<std::array<float, 3>> p(A); std::mt19937 r(A);
    std::uniform_real_distribution<float> u(-1, 1);
    const float R = std::cbrt((float)A) * 0.62f;
    for (auto& q : p) { do { q = {u(r) * R, u(r) * R, u(r) * R}; } while (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] > R * R); }
    for (int it2 = 0; it2 < 60; it2++) for (int a = 0; a < A; a++) for (int b = a + 1; b < A; b++) {   // расталкивание: нуклоны не перекрываются
        float d[3] = {p[a][0] - p[b][0], p[a][1] - p[b][1], p[a][2] - p[b][2]}, l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) + 1e-6f;
        if (l < 1.0f) { const float k = 0.5f * (1.0f - l) / l; for (int c = 0; c < 3; c++) { p[a][c] += d[c] * k; p[b][c] -= d[c] * k; } }
    }
    for (auto& q : p) { const float l = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]); if (l > R) for (float& v : q) v *= R / l; }
    return cache[A] = p;
}
}   // namespace nuc

// ---- распад
static int nucDaughter(int iso) {
    using namespace nuc;
    const Iso& a = ISO[iso]; if (a.daughter >= 0) return a.daughter;
    int Z = a.Z, A = a.A;
    if (a.mode == D_ALPHA) { Z -= 2; A -= 4; } else if (a.mode == D_BETAM) Z++; else if (a.mode == D_BETAP) Z--;
    for (int k = 0; k < NISO; k++) if (ISO[k].Z == Z && ISO[k].A == A) return k;
    return -1;   // вне таблицы: показываем как стабильное ядро (Z, A)
}
static void nucEmit(double x, double y, double z, int kind, double E, int Zp = 0, int Ap = 0) {
    nuc::Part p; p.x = x; p.y = y; p.z = z; nuc::isoDir(p.dx, p.dy, p.dz); p.E = E; p.kind = kind; p.life = 0; p.Z = Zp; p.A = Ap;
    nuc::parts.push_back(p);
}
static void nucDecay(nuc::Nuc& n) {
    using namespace nuc;
    const Iso& a = ISO[n.iso];
    const int d = nucDaughter(n.iso);
    switch (a.mode) {
    case D_ALPHA: nucEmit(n.x, n.y, n.z, P_ALPHA, a.Q * (n.A - 4) / n.A, 2, 4); break;
    case D_BETAM: nucEmit(n.x, n.y, n.z, P_EM, a.Q * 0.35); nucEmit(n.x, n.y, n.z, P_NU, a.Q * 0.65); break;
    case D_BETAP: nucEmit(n.x, n.y, n.z, P_EP, a.Q * 0.35); nucEmit(n.x, n.y, n.z, P_NU, a.Q * 0.65); break;
    case D_GAMMA: nucEmit(n.x, n.y, n.z, P_GAMMA, a.Q); break;
    }
    if (d >= 0) { n.iso = d; n.Z = ISO[d].Z; n.A = ISO[d].A; }
    else { if (a.mode == D_ALPHA) { n.Z -= 2; n.A -= 4; } else if (a.mode == D_BETAM) n.Z++; else if (a.mode == D_BETAP) n.Z--; n.iso = -1; }
    n.glow = 1;
    if (a.mode != D_ALPHA && a.mode != D_BETAM && a.mode != D_BETAP) return;
    if (d >= 0 && ISO[d].mode == D_GAMMA) return;
    if (U() < 0.5) nucEmit(n.x, n.y, n.z, P_GAMMA, 0.3);   // дочернее ядро часто остаётся возбуждённым
}

// ---- сцены мира ядер
static void nucSampleBox(int iso, int count, double L) {
    using namespace nuc;
    nuclei.clear(); parts.clear(); fisFlash.clear();
    const int m = (int)std::ceil(std::cbrt((double)count)); const double a = L / m;
    for (int k = 0; k < count; k++) {
        const int i = k % m, j = (k / m) % m, q = k / (m * m);
        Nuc n{}; n.x = (i + 0.5) * a + (U() - 0.5) * 0.3 * a; n.y = (j + 0.5) * a + (U() - 0.5) * 0.3 * a; n.z = (q + 0.5) * a + (U() - 0.5) * 0.3 * a;
        n.iso = iso; n.Z = ISO[iso].Z; n.A = ISO[iso].A; nuclei.push_back(n);
    }
    N0 = count;
}
// области материалов: шар урана в пустоте (сцены 2, 4) или решётка твэлов UO2 в воде со стержнями сверху (реактор)
static void nucGeometry() {
    using namespace nuc;
    regions.clear();
    const double C = 20;
    if (scene == 2 || scene == 4) { regions.push_back({0, {C, C, C}, {sphereR, 0, 0}, M_U235}); return; }
    // исследовательский реактор: решётка 9×9 твэлов UO2 с шагом 2.6 см в воде (она же отражатель вокруг зоны),
    // в пяти каналах — стержни с бором, входящие сверху
    regions.push_back({1, {C, C, C}, {19.5, 19.5, 19.5}, moderator ? M_WATER : M_VOID});
    const double pitch = 2.6, half = 16;
    for (int i = 0; i < 9; i++) for (int k = 0; k < 9; k++) {
        const double x = C + (i - 4) * pitch, z = C + (k - 4) * pitch;
        if (((i == 2 || i == 6) && (k == 2 || k == 6)) || (i == 4 && k == 4)) {   // каналы стержней
            const double h = half * rodIns; if (h > 0.1) regions.push_back({2, {x, C + half - h, z}, {1.0, h, 0}, M_B4C});
            continue;
        }
        regions.push_back({2, {x, C, z}, {0.9, half, 0}, M_UO2});
    }
}
// декоративные ядра урана: в шаре или в твэлах (U-235 — по обогащению)
static void nucDecor() {
    using namespace nuc;
    nuclei.clear();
    const double C = 20;
    auto add = [&](double x, double y, double z) { Nuc n{}; n.x = x; n.y = y; n.z = z; n.iso = U() < enrich ? 1 : 0; n.Z = 92; n.A = ISO[n.iso].A; nuclei.push_back(n); };
    if (scene == 2 || scene == 4) {
        for (int k = 0; k < 700; k++) {
            double x, y, z; do { x = U() * 2 - 1; y = U() * 2 - 1; z = U() * 2 - 1; } while (x * x + y * y + z * z > 1);
            add(C + x * sphereR, C + y * sphereR, C + z * sphereR);
        }
        return;
    }
    for (const Region& r : regions) if (r.mat == M_UO2)
        for (int q = 0; q < 10; q++) { const double a = U() * 2 * PI, rr = 0.7 * std::sqrt(U()); add(r.c[0] + rr * std::cos(a), C - 15 + 30 * U(), r.c[2] + rr * std::sin(a)); }
}
// перестроить материалы и ядра (стержни, обогащение, вода) — нейтроны в полёте остаются
static void nucReactorBuild() { nucGeometry(); nucDecor(); }
// выпустить нейтроны из центра (источник, как радий-бериллиевый)
static void nucSource(int n) {
    using namespace nuc;
    for (int k = 0; k < n; k++) { Part p{}; p.x = p.y = p.z = 20; isoDir(p.dx, p.dy, p.dz); p.E = 1.5 + U(); p.kind = P_N; p.tau = -std::log(std::max(1e-12, U())); p.w = 1; p.gen = genTop; parts.push_back(p); genAdd(p.gen, 1); births++; }
}
// начать опыт заново с текущими параметрами (изотоп, радиус, обогащение, стержни)
static void nucRestart() {
    using namespace nuc;
    t = 0; histN.clear(); histT.clear(); histTheory.clear(); histNeut.clear(); chainHist.clear(); histStride = 1; histSkip = 0;
    parts.clear(); fisFlash.clear();
    fissions = energyMeV = neutronsW = 0; births = 0; genTop = 0; for (int q = 0; q < GEN_RING; q++) genBorn[q] = genAlive[q] = 0;
    exploding = false; expandR = sphereR; expandV = 0; eInt = 0;
    switch (scene) {
    case 0: nucSampleBox(isoPick, 343, 24); break;
    case 1: nucSampleBox(7, 343, 24); break;
    default: nucGeometry(); nucDecor(); nucSource(scene == 3 ? 150 : scene == 4 ? 12 : 20); break;   // у реактора побольше: меньше случайных колебаний
    }
    S = Sim(); S.Lx = S.Ly = S.Lz = scene <= 1 ? 24 : 40;   // ящик для камеры (у реакторных сцен 1 единица = 1 см)
}
static void nucReset(int sc) {
    using namespace nuc;
    scene = sc;
    switch (sc) {   // параметры по умолчанию
    case 0: timeScale = ISO[isoPick].T12 / 8; break;                    // за 8 с экрана — один период полураспада
    case 1: timeScale = 3000; break;                                     // радон-222: 50 минут мира за секунду
    case 2: sphereR = 10.0; enrich = 0.9; timeScale = 4; break;          // k ≈ 1.1 (критический радиус модели ≈ 8.9 см); 4 нс за секунду экрана
    case 3: enrich = 0.20; rodIns = 0.55; moderator = true; timeScale = 20000; break;   // стержни на 55% — k ≈ 1; 20 мкс за секунду
    case 4: sphereR = 12.0; enrich = 0.9; timeScale = 12; break;
    }
    nucRestart();
}
// ---- шаг: распад (сцены 0–1) или перенос нейтронов (2–4)
static void nucStepDecay(double dts) {
    using namespace nuc;
    t += dts;
    for (auto& n : nuclei) {
        if (n.iso < 0 || ISO[n.iso].mode == D_STABLE) continue;
        // за один кадр может пройти много периодов (Po-214: 164 мкс) — последовательные распады в пределах шага;
        // время до распада — экспоненциальное с λ = ln2/T½ (у ядра нет «возраста»)
        double left = dts;
        for (int g = 0; g < 8 && n.iso >= 0 && ISO[n.iso].mode != D_STABLE; g++) {
            const double lam = std::log(2.0) / ISO[n.iso].T12, tt = -std::log(std::max(1e-300, U())) / lam;
            if (tt > left) break;
            left -= tt; nucDecay(n);
        }
    }
}
static void nucFission(double x, double y, double z, int gen, double w, int A) {
    using namespace nuc;
    static const int FR[5][5] = {{28, 29, 3, 0, 0}, {30, 31, 2, 0, 0}, {32, 33, 3, 0, 0}, {34, 35, 2, 0, 0}, {28, 29, 3, 0, 0}};
    const int f = (int)(U() * 4.0); const int nn = A == 239 ? (U() < 0.88 ? 3 : 2) : (U() < 0.43 ? 3 : 2);   // ν = 2.43 (U-235), 2.88 (Pu-239)
    fissions += w; energyMeV += 200 * w; eInt += 200 * w;
    for (int k = 0; k < nn; k++) { Part p{}; p.x = x; p.y = y; p.z = z; isoDir(p.dx, p.dy, p.dz); p.E = wattE(); p.kind = P_N; p.tau = -std::log(std::max(1e-12, U())); p.w = w; p.gen = gen + 1;
        parts.push_back(p); births++; genAdd(p.gen, w); }
    // осколки: пара ядер, разлетаются с ≈170 МэВ (отображение) и светятся
    for (int s = 0; s < 2; s++) { Part p{}; p.x = x; p.y = y; p.z = z; isoDir(p.dx, p.dy, p.dz); if (s) { p.dx = -parts[parts.size() - 1].dx; p.dy = -parts[parts.size() - 1].dy; p.dz = -parts[parts.size() - 1].dz; }
        p.kind = P_FRAG; p.E = 80; p.Z = ISO[FR[f][s]].Z; p.A = ISO[FR[f][s]].A; parts.push_back(p); }
    for (int k = 0; k < 2; k++) nucEmit(x, y, z, P_GAMMA, 1.5);
    if (fisFlash.size() < 300) fisFlash.push_back({(float)x, (float)y, (float)z, 0.0f, 1.0f});
    // ближайшее декоративное ядро делится вместе с событием
    int best = -1; double bd = 9.0;
    for (int i = 0; i < (int)nuclei.size(); i++) { const Nuc& n = nuclei[i]; if (!n.alive || n.split >= 0 || n.A != A) continue; const double d = (n.x - x) * (n.x - x) + (n.y - y) * (n.y - y) + (n.z - z) * (n.z - z); if (d < bd) { bd = d; best = i; } }
    if (best >= 0) nuclei[best].split = 0;
}
static void nucStepNeutrons(double dtn) {
    using namespace nuc;
    t += dtn;
    // разлёт сверхкритической сборки: давление энергии деления расталкивает шар, плотность падает, k < 1 — реакция гаснет
    if (scene == 4 && fissions > 0) {
        const double V = 4.0 / 3 * PI * expandR * expandR * expandR, M = 19.05 * 4.0 / 3 * PI * sphereR * sphereR * sphereR;   // г
        const double E_J = eInt * 1.602e-13, p = (1.0 / 3.0) * E_J / (V * 1e-6);   // Па (излучение, γ = 4/3)
        if (p > 1e10) exploding = true;   // выше 10 ГПа металл уже не держит
        if (exploding) {
            const double a = 4 * PI * expandR * expandR * 1e-4 * p / (M * 1e-3) * 1e2 * 1e-18;   // см/нс²
            const double R0 = expandR;
            expandV += a * dtn; expandR = std::min(expandR + expandV * dtn, 60.0);
            const double f = expandR / R0;   // металл разлетается вместе с шаром
            for (auto& n : nuclei) { n.x = 20 + (n.x - 20) * f; n.y = 20 + (n.y - 20) * f; n.z = 20 + (n.z - 20) * f; }
        }
    }
    const int np = (int)parts.size();
    for (int k = 0; k < np; k++) {
        Part& p = parts[k];
        if (p.kind != P_N) continue;
        double tl = dtn;   // оставшееся время шага, нс (после столкновения скорость другая)
        for (int guard = 0; guard < 80 && tl > 0 && p.kind == P_N; guard++) {
            const double v = speedCmNs(p.E), reach = v * tl;
            const int reg = regionAt(p.x, p.y, p.z);
            const double St = sigmaTot(reg, p.E);
            // путь до столкновения (оптическая толщина τ) или до конца шага; граница области — мелкими шагами
            const double step = std::min(reach, St > 0 ? p.tau / St : reach);
            const double sub = std::min(step, 0.5);
            p.x += p.dx * sub; p.y += p.dy * sub; p.z += p.dz * sub; tl -= sub / v; p.tau -= St * sub;
            if (p.x < 0 || p.y < 0 || p.z < 0 || p.x > 40 || p.y > 40 || p.z > 40) { p.kind = -1; genDie(p.gen, p.w); break; }   // утечка
            if (St <= 0 || p.tau > 1e-9) continue;
            // столкновение: нуклид по вкладу в Σ, затем реакция по сечениям
            const Comp c = matComp(regions[reg].mat); const double ds = regions[reg].mat == M_U235 ? densityScale() : 1.0;
            double pick = U() * St, acc = 0; int A = c.A[0]; XS xs = xsOf(A, p.E);
            for (int q = 0; q < c.n; q++) { const XS x = xsOf(c.A[q], p.E); acc += c.dens[q] * ds * (x.f + x.c + x.s) * 1e-24; if (pick <= acc) { A = c.A[q]; xs = x; break; } }
            const double tot = xs.f + xs.c + xs.s, r = U() * tot;
            // деление и захват добавляют частицы в parts — массив может переехать, поэтому всё нужное копируется заранее,
            // а после события к p больше не обращаемся
            if (r < xs.f + xs.c) {
                const double x = p.x, y = p.y, z = p.z, w = p.w; const int g = p.gen;
                p.kind = -1; genDie(g, w);
                if (r < xs.f) nucFission(x, y, z, g, w, A);
                else if (A == 10) nucEmit(x, y, z, P_ALPHA, 1.47, 2, 4);   // B-10 + n → Li-7 + α
                else nucEmit(x, y, z, P_GAMMA, 2.0);
                break;
            }
            else {   // упругое рассеяние: изотропно в системе центра масс, E' = E·(A² + 2A·cosθ + 1)/(A + 1)²
                double ux, uy, uz; isoDir(ux, uy, uz);
                const double mu = 2 * U() - 1, E1 = p.E * (A * A + 2 * A * mu + 1) / ((A + 1.0) * (A + 1.0));
                const double vcm = 1.0 / (A + 1);   // направление в лабораторной системе: v_lab = v_cm + v'
                double lx = p.dx * vcm + ux * A / (A + 1.0), ly = p.dy * vcm + uy * A / (A + 1.0), lz = p.dz * vcm + uz * A / (A + 1.0), ll = std::sqrt(lx * lx + ly * ly + lz * lz) + 1e-12;
                p.dx = lx / ll; p.dy = ly / ll; p.dz = lz / ll;
                p.E = std::max(2.53e-8 * (0.5 + U()), E1);   // тепловой уровень: газ ядер при 300 K
                p.tau = -std::log(std::max(1e-12, U()));
            }
        }
    }
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Part& p) { return p.kind < 0; }), parts.end());
    // слишком много нейтронов на экране — «русская рулетка»: половина остаётся с удвоенным весом (сумма не меняется)
    int nn = 0; for (auto& p : parts) if (p.kind == P_N) nn++;
    if (nn > 2500) for (auto& p : parts) if (p.kind == P_N) {
        if (U() < 0.5) { genDie(p.gen, p.w); p.kind = -1; } else { genAlive[p.gen % GEN_RING] += p.w; p.w *= 2; }
    }
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Part& p) { return p.kind < 0; }), parts.end());
    neutronsW = 0; for (auto& p : parts) if (p.kind == P_N) neutronsW += p.w;
}
// частицы распада и осколки: полёт и затухание (время экрана)
static void nucStepVisual(double frameDt) {
    using namespace nuc;
    for (auto& p : parts) {
        if (p.kind == P_N) continue;
        const double v = p.kind == P_ALPHA ? 6 : p.kind == P_FRAG ? 5 : p.kind == P_GAMMA || p.kind == P_NU ? 30 : 14;   // условные скорости рисунка
        p.x += p.dx * v * frameDt; p.y += p.dy * v * frameDt; p.z += p.dz * v * frameDt; p.life += (float)frameDt;
        const float maxLife = p.kind == P_FRAG ? 1.6f : p.kind == P_ALPHA ? 3.0f : 1.2f;
        if (p.life > maxLife) p.kind = -1;
    }
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Part& p) { return p.kind < 0; }), parts.end());
    for (auto& n : nuclei) { n.glow = std::max(0.0f, n.glow - (float)frameDt * 0.8f); if (n.split >= 0) { n.split += (float)frameDt; if (n.split > 0.8f) n.alive = false; } }
    for (auto& f : fisFlash) f[3] += (float)frameDt;
    fisFlash.erase(std::remove_if(fisFlash.begin(), fisFlash.end(), [](const std::array<float, 5>& f) { return f[3] > 0.7f; }), fisFlash.end());
}
static void nucStep(double frameDt) {
    using namespace nuc;
    if (scene <= 1) nucStepDecay(frameDt * timeScale);
    else { const int sub = 8; for (int s = 0; s < sub; s++) nucStepNeutrons(frameDt * timeScale / sub); }
    if (++histSkip >= histStride) {
        histSkip = 0; histT.push_back((float)t);
        if (scene <= 1) {
            const int iso = scene == 0 ? isoPick : 7;
            int n = 0; for (auto& q : nuclei) if (q.iso == iso) n++;
            histN.push_back((float)n); histTheory.push_back((float)(N0 * std::pow(2.0, -t / ISO[iso].T12)));
            if (scene == 1) { std::array<int, 16> c{}; for (auto& q : nuclei) if (q.iso >= 7 && q.iso <= 15) c[q.iso - 7]++; chainHist.push_back(c); }
        } else histNeut.push_back((float)std::log10(std::max(1.0, neutronsW)));
        if (histT.size() >= 1200) {   // каждая вторая точка, дальше пишем вдвое реже
            auto thin = [](auto& v) { for (size_t k = 0; 2 * k < v.size(); k++) v[k] = v[2 * k]; v.resize((v.size() + 1) / 2); };
            thin(histT); thin(histN); thin(histTheory); thin(histNeut); thin(chainHist); histStride *= 2;
        }
    }
    nucStepVisual(frameDt);
}
// k-эффективный методом поколений (как в программах расчёта реакторов): n нейтронов из мест делений прошлого
// поколения летят до поглощения или утечки; k — сколько нейтронов деления рождено на один исходный.
// Без рисования и с той же геометрией, сечениями и спектром, что и сцена.
static double nucKeff(int n, int gens) {
    using namespace nuc;
    std::vector<std::array<double, 3>> src, next;
    for (int k = 0; k < n; k++) src.push_back({20.0, 20.0, 20.0});
    double kSum = 0; int kCnt = 0;
    for (int g = 0; g < gens; g++) {
        next.clear(); double born = 0;
        for (auto& s : src) {
            double x = s[0], y = s[1], z = s[2], dx, dy, dz, E = wattE(); isoDir(dx, dy, dz);
            for (int guard = 0; guard < 4000; guard++) {
                const int reg = regionAt(x, y, z); const double St = sigmaTot(reg, E);
                const double tau = -std::log(std::max(1e-12, U())), step = St > 0 ? std::min(tau / St, 0.5) : 0.5;
                x += dx * step; y += dy * step; z += dz * step;
                if (x < 0 || y < 0 || z < 0 || x > 40 || y > 40 || z > 40) break;
                if (St <= 0 || step < tau / St - 1e-12) continue;   // долетели до границы шага, а не до столкновения
                const Comp c = matComp(regions[reg].mat); double pick = U() * St, acc = 0; int A = c.A[0]; XS xs = xsOf(A, E);
                for (int q = 0; q < c.n; q++) { const XS xq = xsOf(c.A[q], E); acc += c.dens[q] * (xq.f + xq.c + xq.s) * 1e-24; if (pick <= acc) { A = c.A[q]; xs = xq; break; } }
                const double tot = xs.f + xs.c + xs.s, r = U() * tot;
                if (r < xs.f) { born += A == 239 ? 2.88 : 2.43; next.push_back({x, y, z}); break; }
                if (r < xs.f + xs.c) break;
                double ux, uy, uz; isoDir(ux, uy, uz); const double mu = 2 * U() - 1, E1 = E * (A * A + 2 * A * mu + 1) / ((A + 1.0) * (A + 1.0));
                double lx = dx / (A + 1.0) + ux * A / (A + 1.0), ly = dy / (A + 1.0) + uy * A / (A + 1.0), lz = dz / (A + 1.0) + uz * A / (A + 1.0), ll = std::sqrt(lx * lx + ly * ly + lz * lz) + 1e-12;
                dx = lx / ll; dy = ly / ll; dz = lz / ll; E = std::max(2.53e-8 * (0.5 + U()), E1);
            }
        }
        const double k = born / std::max<size_t>(1, src.size());
        if (g >= 2) { kSum += k; kCnt++; }
        if (next.empty()) return kCnt ? kSum / kCnt : k;
        src.clear(); for (int q = 0; q < n; q++) src.push_back(next[(size_t)(U() * next.size()) % next.size()]);
    }
    return kCnt ? kSum / kCnt : 0;
}
// коэффициент размножения по балансу нейтронов: рождено делениями / потеряно (поглощено и вылетело) за последнее время
static double nucK() {
    using namespace nuc;
    for (int g = genTop - 1; g >= 0 && g > genTop - GEN_RING + 2; g--)
        if (genAlive[g % GEN_RING] < 1e-6 && genBorn[g % GEN_RING] > 30) return genBorn[(g + 1) % GEN_RING] / genBorn[g % GEN_RING];
    return 0;
}

// ---- рисование: сфера-спрайт (атлас texCore), цвет протонов и нейтронов, цвет нейтрона по энергии
static inline void nucBall(double x, double y, double z, float R, float r, float g, float b, float a = 1) {
    float sx, sy, d, s; if (!project(x, y, z, sx, sy, d, s)) return;
    const float Rp = R * s; if (Rp < 0.4f) return;
    quadAtom(sx, sy, Rp, r * a, g * a, b * a); if (Rp > 2) quadSpec(sx, sy, Rp, 0.7f * a);
}
static void nucEnergyColor(double E, float& r, float& g, float& b) {   // быстрый — белый, тепловой — синий
    const float u = (float)clampv((std::log10(std::max(E, 1e-9)) + 8.0) / 8.3, 0.0, 1.0);
    r = 0.35f + 0.65f * u; g = 0.55f + 0.45f * u; b = 1.0f;
}
static void nucDrawNucleus(const nuc::Nuc& n, float scale) {
    float sx, sy, d, s; if (!project(n.x, n.y, n.z, sx, sy, d, s)) return;
    const float Rn = 0.22f * std::cbrt((float)n.A) * scale;
    // устойчивое ядро (конец цепочки распадов) — тусклее: видно, как радиоактивных становится меньше
    const bool stable = n.iso < 0 || nuc::ISO[n.iso].mode == nuc::D_STABLE; const float dim = stable ? 0.45f : 1.0f;
    if (Rn * s < 9.0f) {   // издалека — один шар: цвет по заряду ядра
        const float f = (0.55f + 0.45f * n.glow) * dim; nucBall(n.x, n.y, n.z, Rn, (0.75f + 0.25f * n.glow) * f, 0.45f * f, 0.40f * f); return;
    }
    const auto& pk = nuc::nucleonPack(n.A); const float k = Rn / (std::cbrt((float)n.A) * 0.62f + 0.5f), rb = 0.55f * k;
    // рисуем дальние нуклоны первыми (в пределах одного ядра)
    std::vector<std::pair<float, int>> order; order.reserve(pk.size());
    for (int q = 0; q < (int)pk.size(); q++) order.push_back({(float)viewDepth(n.x + pk[q][0] * k, n.y + pk[q][1] * k, n.z + pk[q][2] * k), q});
    std::sort(order.begin(), order.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
    const float stretch = n.split >= 0 ? 1.0f + 1.8f * n.split : 1.0f;   // делящееся ядро вытягивается «гантелью»
    for (auto& o : order) {
        const int q = o.second; const bool proton = q < n.Z; const float glow = (1.0f + 0.6f * n.glow) * dim;
        const float px = pk[q][0] * k * stretch, py = pk[q][1] * k, pz = pk[q][2] * k;
        if (proton) nucBall(n.x + px, n.y + py, n.z + pz, rb, std::min(1.0f, 0.95f * glow), 0.22f * glow, 0.20f * glow);
        else nucBall(n.x + px, n.y + py, n.z + pz, rb, 0.62f * glow, 0.66f * glow, 0.72f * glow);
    }
}
static void nucDraw() {
    using namespace nuc;
    glEnable(GL_SCISSOR_TEST); glScissor((int)sceneX, (int)(winH - sceneY - sceneH), (int)sceneW, (int)sceneH);
    rectFill(sceneX, sceneY, sceneW, sceneH, C_SCENE);
    // области материалов: прозрачные контуры (шар — окружность, коробка — рёбра, цилиндры — отрезки)
    glEnable(GL_LINE_SMOOTH);
    for (const Region& r : regions) {
        RGBA c = r.mat == M_WATER ? hexc(0x3F7FD8, 0.55f) : r.mat == M_B4C ? hexc(0x9A9A9A, 0.9f) : r.mat == M_GRAPH ? hexc(0x777777, 0.6f) : hexc(0x78D878, 0.6f);
        MonoAtoms colored; col(c);
        if (r.shape == 0) {
            float sx, sy, d, s; if (!project(r.c[0], r.c[1], r.c[2], sx, sy, d, s)) continue;
            const float R = (float)((exploding ? expandR : r.s[0]) * s);
            glColor4f(c.r, c.g, c.b, 0.08f + (exploding ? 0.25f : 0.0f)); discPx(sx, sy, R, 64); col(c); circlePx(sx, sy, R, 96);
        } else if (r.shape == 1) {
            const double x0 = r.c[0] - r.s[0], x1 = r.c[0] + r.s[0], y0 = r.c[1] - r.s[1], y1 = r.c[1] + r.s[1], z0 = r.c[2] - r.s[2], z1 = r.c[2] + r.s[2];
            const double P[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, {x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}};
            const int E[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
            glBegin(GL_LINES); for (auto& e : E) line3(P[e[0]][0], P[e[0]][1], P[e[0]][2], P[e[1]][0], P[e[1]][1], P[e[1]][2]); glEnd();
        } else {
            glLineWidth(r.mat == M_B4C ? 3.0f : 2.0f);
            for (int q = 0; q < 12; q++) { const double a = 2 * PI * q / 12; glBegin(GL_LINES); line3(r.c[0] + r.s[0] * std::cos(a), r.c[1] - r.s[1], r.c[2] + r.s[0] * std::sin(a), r.c[0] + r.s[0] * std::cos(a), r.c[1] + r.s[1], r.c[2] + r.s[0] * std::sin(a)); glEnd(); }
            glLineWidth(1);
        }
    }
    glDisable(GL_LINE_SMOOTH);
    // ядра и частицы — спрайтами от дальних к ближним
    const float nscale = scene <= 1 ? 0.75f : 0.9f;
    std::vector<std::pair<float, int>> ord;
    for (int i = 0; i < (int)nuclei.size(); i++) if (nuclei[i].alive) ord.push_back({(float)viewDepth(nuclei[i].x, nuclei[i].y, nuclei[i].z), i});
    std::sort(ord.begin(), ord.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
    glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    {
        MonoAtoms colored;
        for (auto& o : ord) nucDrawNucleus(nuclei[o.second], nscale);
        for (auto& p : parts) {
            if (p.kind == P_FRAG) { Nuc f{}; f.x = p.x; f.y = p.y; f.z = p.z; f.Z = p.Z; f.A = p.A; f.glow = std::max(0.0f, 1.0f - p.life); nucDrawNucleus(f, nscale * 0.9f); }
            else if (p.kind == P_ALPHA) { Nuc f{}; f.x = p.x; f.y = p.y; f.z = p.z; f.Z = 2; f.A = 4; f.glow = 0.5f; nucDrawNucleus(f, nscale * 1.3f); }
            else if (p.kind == P_N) { float r, g, b; nucEnergyColor(p.E, r, g, b); nucBall(p.x, p.y, p.z, 0.22f, r, g, b); }
        }
        flushQuads(texCore);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    {   // электроны, позитроны, нейтрино, гамма-кванты — светящиеся следы; вспышки делений
        MonoAtoms colored;
        glEnable(GL_LINE_SMOOTH); glLineWidth(1.6f);
        for (auto& p : parts) {
            if (p.kind != P_EM && p.kind != P_EP && p.kind != P_NU && p.kind != P_GAMMA) continue;
            float r = 0.4f, g = 0.6f, b = 1.0f; const float a = std::max(0.0f, 1.0f - p.life / 1.2f);
            if (p.kind == P_EP) { r = 1.0f; g = 0.45f; b = 0.8f; } else if (p.kind == P_NU) { r = g = b = 0.5f; } else if (p.kind == P_GAMMA) { r = 1.0f; g = 0.95f; b = 0.4f; }
            const double L = p.kind == P_GAMMA ? 2.5 : 1.6;
            glColor4f(r, g, b, a * (p.kind == P_NU ? 0.35f : 0.9f));
            if (p.kind == P_GAMMA) {   // волнистая линия фотона
                double ex, ey, ez; ex = -p.dy; ey = p.dx; ez = 0; double el = std::sqrt(ex * ex + ey * ey) + 1e-9; ex /= el; ey /= el;
                glBegin(GL_LINE_STRIP);
                for (int q = 0; q <= 24; q++) { const double s = -L * q / 24.0, w = 0.25 * std::sin(q * 1.3); float X, Y, D, Sc; if (project(p.x + p.dx * s + ex * w, p.y + p.dy * s + ey * w, p.z + p.dz * s + ez * w, X, Y, D, Sc)) glVertex2f(X, Y); }
                glEnd();
            } else { glBegin(GL_LINES); line3(p.x, p.y, p.z, p.x - p.dx * L, p.y - p.dy * L, p.z - p.dz * L); glEnd(); }
        }
        glLineWidth(1); glDisable(GL_LINE_SMOOTH);
        for (auto& f : fisFlash) {
            float sx, sy, d, s; if (!project(f[0], f[1], f[2], sx, sy, d, s)) continue;
            const float u = f[3] / 0.7f; quadUV(sx, sy, (0.6f + 3.0f * u) * s, 1.0f, 0.85f, 0.55f, 0.6f * (1 - u));
        }
        if (exploding) { float sx, sy, d, s; if (project(20, 20, 20, sx, sy, d, s)) quadUV(sx, sy, (float)(expandR * 2.2 * s), 1.0f, 0.8f, 0.5f, 0.35f); }
        flushQuads(texGlow);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_SCISSOR_TEST);
}
// ---- подписи времени мира
static std::string nucTimeStr(double sec) {
    const double a = std::fabs(sec);
    if (a < 1e-6) return fmt("%.1f нс", sec * 1e9);
    if (a < 1e-3) return fmt("%.1f мкс", sec * 1e6);
    if (a < 1) return fmt("%.1f мс", sec * 1e3);
    if (a < 120) return fmt("%.1f с", sec);
    if (a < 7200) return fmt("%.1f мин", sec / 60);
    if (a < 3 * 86400) return fmt("%.1f ч", sec / 3600);
    if (a < 3.156e7 * 2) return fmt("%.1f сут", sec / 86400);
    if (a < 3.156e7 * 2e3) return fmt("%.1f года", sec / 3.156e7);
    if (a < 3.156e7 * 2e6) return fmt("%.1f тыс. лет", sec / 3.156e10);
    if (a < 3.156e7 * 2e9) return fmt("%.1f млн лет", sec / 3.156e13);
    return fmt("%.2f млрд лет", sec / 3.156e16);
}
static std::string nucWorldTime() { using namespace nuc; return scene <= 1 ? nucTimeStr(t) : nucTimeStr(t * 1e-9); }
// энергия: МэВ → Дж и тротиловый эквивалент (1 т ТНТ = 4.184·10⁹ Дж)
static std::string nucEnergyStr(double MeV) {
    const double J = MeV * 1.602e-13;
    if (J < 1e3) return fmt("%.3g Дж", J);
    if (J < 4.184e9) return fmt("%.3g кДж (%.3g кг ТНТ)", J / 1e3, J / 4.184e6);
    return fmt("%.3g ГДж (%.3g т ТНТ)", J / 1e9, J / 4.184e9);
}


// ===================================== СТРОЕНИЕ АТОМА: ЭЛЕКТРОННЫЕ ОБЛАКА ===================
// Электрон в атоме — не шарик на орбите, а стоячая волна ψ; вероятность найти его в точке — |ψ|². Для электрона
// в поле ядра уравнение Шрёдингера решается точно: ψ = R_nl(r)·Y_lm(θ,φ). Радиальная часть имеет n − l − 1 узлов
// (оболочки), угловая задаёт форму: s — шар, p — «гантели», d — «четырёхлистники», f — ещё сложнее.
// В многоэлектронном атоме внутренние электроны экранируют ядро, и электрон «видит» эффективный заряд Z_eff —
// по правилам Слейтера (1930). Заполнение подоболочек — по конфигурации основного состояния, внутри подоболочки —
// по правилу Хунда (сначала по одному электрону в каждую орбиталь).
// Облако рисуется как светящийся газ: ψ считается в узлах сетки, и сквозь неё проходят лучи — каждый собирает свечение
// и теряет яркость в плотных местах (излучение и поглощение). Доли разного знака ψ — разного цвета, а поверхность,
// внутри которой электрон бывает в 85% случаев, — глянцевая оболочка. Картинка считается процессором в текстуру.
// id окна: 1700–1799.

// ---- действительные сферические гармоники на единичной сфере. Общий множитель каждого l опущен, но внутри l орбитали
// нормированы одинаково: сумма квадратов по всем 2l + 1 орбиталям не зависит от направления (теорема Унзольда),
// и заполненная подоболочка — шар. У f прежние множители были не согласованы, и полная 4f-оболочка урана выходила кривой
static int orbCount(int l) { return 2 * l + 1; }
static double realY(int l, int m, double x, double y, double z) {
    switch (l) {
    case 0: return 1;
    case 1: return m == 0 ? x : m == 1 ? y : z;
    case 2: switch (m) { case 0: return x * y; case 1: return x * z; case 2: return y * z; case 3: return (3 * z * z - 1) / 2.0 / std::sqrt(3.0); default: return (x * x - y * y) / 2; }
    default: switch (m) {   // множители: 1, √(3/2), 2√15, √15, √(5/2) — относительно f_z³
        case 0: return z * (5 * z * z - 3) * 0.2; case 1: return x * (5 * z * z - 1) * 0.24494897; case 2: return y * (5 * z * z - 1) * 0.24494897;
        case 3: return x * y * z * 1.54919334; case 4: return z * (x * x - y * y) * 0.77459667;
        case 5: return x * (x * x - 3 * y * y) * 0.31622777; default: return y * (3 * x * x - y * y) * 0.31622777; }
    }
}
// подпись орбитали: буква и нижний индекс
static const char* ORB_SUB[4][7] = {{""}, {"x", "y", "z"}, {"xy", "xz", "yz", "z²", "x²−y²"}, {"z³", "xz²", "yz²", "xyz", "z(x²−y²)", "x(x²−3y²)", "y(3x²−y²)"}};
static float drawOrbName(const Font& f, float x, float y, int n, int l, int m, RGBA c) {
    float x0 = x;
    if (n > 0) x += drawTextRaw(f, x, y, std::to_string(n), c);
    x += drawTextRaw(f, x, y, std::string(1, "spdf"[l]), c);
    if (l > 0 && m >= 0) x += drawTextRaw(fontXS, x, y + f.h * 0.34f, ORB_SUB[l][m], c);
    return x - x0;
}
// обобщённые многочлены Лагерра L_k^(a)(x)
static double laguerre(int k, double a, double x) {
    if (k <= 0) return 1;
    double L0 = 1, L1 = 1 + a - x;
    for (int j = 1; j < k; j++) { const double L2 = ((2 * j + 1 + a - x) * L1 - (j + a) * L0) / (j + 1); L0 = L1; L1 = L2; }
    return L1;
}
// радиальная функция водородоподобного атома с зарядом Z (r — в боровских радиусах)
static double radialR(int n, int l, double Z, double r) {
    const double rho = 2 * Z * r / n;
    return std::pow(rho, l) * std::exp(-rho / 2) * laguerre(n - l - 1, 2 * l + 1, rho);
}
// эффективный заряд ядра для электрона (n, l) по правилам Слейтера: группы (1s)(2s2p)(3s3p)(3d)(4s4p)(4d)(4f)(5s5p)…;
// электроны своей группы экранируют на 0.35 (в 1s — 0.30), внешние — ничего; для s и p оболочка n−1 — 0.85,
// глубже — 1.00; для d и f все внутренние группы — 1.00
static double slaterZeff(int Z, const int occ[8][4], int n, int l) {
    static const int G[13][2] = {{1, 0}, {2, 0}, {3, 0}, {3, 2}, {4, 0}, {4, 2}, {4, 3}, {5, 0}, {5, 2}, {5, 3}, {6, 0}, {6, 2}, {7, 0}};
    auto grp = [](int nn, int ll) { const int key = ll <= 1 ? 0 : ll; for (int g = 0; g < 13; g++) if (G[g][0] == nn && G[g][1] == key) return g; return 12; };
    const int g0 = grp(n, l); double s = 0;
    for (int nn = 1; nn <= 7; nn++) for (int ll = 0; ll < 4; ll++) {
        int c = occ[nn][ll]; if (nn == n && ll == l) c--;   // сам себя электрон не экранирует
        if (c <= 0) continue;
        const int g = grp(nn, ll);
        if (g == g0) s += c * (n == 1 ? 0.30 : 0.35);
        else if (g > g0) continue;
        else if (l >= 2 || nn < n - 1) s += c;
        else s += c * (nn == n - 1 ? 0.85 : 1.0);
    }
    return std::max(1.0, Z - s);
}
// эффективное главное квантовое число n* (тоже из правил Слейтера): начиная с n = 4 внешние электроны держатся ближе,
// чем у водородоподобной оболочки с тем же n; для n = 7 — продолжение ряда
static double slaterNstar(int n) { static const double NS[8] = {1, 1, 2, 3, 3.7, 4.0, 4.2, 4.3}; return NS[clampv(n, 1, 7)]; }
// средний радиус орбитали Слейтера ⟨r⟩ = n*(2n* + 1)/(2·Z_eff), боровские радиусы
static double slaterMeanR(int n, double Zs) { const double ns = slaterNstar(n); return ns * (2 * ns + 1) / (2 * Zs); }
// заряд для водородоподобной радиальной функции R_nl: узлов у неё столько же, сколько у настоящей оболочки (n − l − 1),
// а средний радиус — как у орбитали Слейтера. С одним Z_eff при n = 6–7 внешние оболочки выходили в 4–5 раз шире
// настоящих (уран рисовался радиусом 18 Å)
static double radialZ(int n, int l, double Zs) { return (3.0 * n * n - l * (l + 1)) / (2 * slaterMeanR(n, Zs)); }
// электроны подоболочки по орбиталям — правило Хунда: сначала по одному в каждую, потом парами
static int hundOcc(int c, int l, int m) { const int k = orbCount(l); return (c > m ? 1 : 0) + (c > k + m ? 1 : 0); }
// радиус, внутри которого доля frac вероятности подоболочки (n, l) при заряде Z (по распределению r²R²)
static double radialQuantile(int n, int l, double Z, double frac) {
    const double rmax = (6.0 * n * n + 6) / Z; const int NR = 1500;
    std::vector<double> cdf(NR + 1, 0.0);
    for (int k = 1; k <= NR; k++) { const double r = rmax * (k - 0.5) / NR, R = radialR(n, l, Z, r); cdf[k] = cdf[k - 1] + r * r * R * R; }
    if (cdf[NR] <= 0) return rmax;
    for (int k = 1; k <= NR; k++) if (cdf[k] >= frac * cdf[NR]) return rmax * k / NR;
    return rmax;
}

// цвета орбиталей как на школьных рисунках: s — красные, p — жёлто-оранжевые, d — синие, f — зелёные;
// у p, d, f две доли разного знака ψ — два оттенка
static void orbColor(int l, bool neg, float& r, float& g, float& b) {
    static const float C[4][2][3] = {{{1.00f, 0.36f, 0.36f}, {1.00f, 0.36f, 0.36f}}, {{1.00f, 0.86f, 0.30f}, {1.00f, 0.52f, 0.16f}},
                                     {{0.36f, 0.50f, 1.00f}, {0.45f, 0.86f, 1.00f}}, {{0.30f, 0.86f, 0.34f}, {0.70f, 0.95f, 0.45f}}};
    const float* c = C[clampv(l, 0, 3)][neg ? 1 : 0]; r = c[0]; g = c[1]; b = c[2];
}
// цвета знака ψ одной орбитали (облако и поверхность): плюс — тёплый, минус — холодный
static const float PSI_POS[3] = {1.00f, 0.56f, 0.18f}, PSI_NEG[3] = {0.24f, 0.60f, 1.00f};

// ---- радиальные функции по таблице: ln r от 10⁻⁸ до 200 боровских радиусов — от размера нуклона до края атома
// (при приближении разрез считается в миллионах точек, и формула с Лагерром в каждой была бы слишком медленной)
namespace rtab {
constexpr int K = 8192; constexpr double R0 = 1e-8, R1 = 200;
static const double L0 = std::log(R0), DL = (std::log(R1) - std::log(R0)) / (K - 1);
static inline double r(int k) { return std::exp(L0 + k * DL); }
// место в таблице: номер узла i и доля f до следующего (ln r считается один раз на точку для всех подоболочек)
struct Pos { int i; float f; };
static inline Pos pos(double rr) {
    const double x = (std::log(std::max(rr, R0)) - L0) / DL; const int i = (int)x;
    if (i >= K - 1) return {K - 2, 1.0f};
    return {i, (float)(x - i)};
}
static inline float at(const std::vector<float>& t, Pos p) { return t[p.i] + p.f * (t[p.i + 1] - t[p.i]); }
static inline float at(const std::vector<float>& t, double rr) { return at(t, pos(rr)); }
}
// x^1.6 на [0, 1] по таблице (яркость оболочки)
static inline float pow16(float x) {
    static const std::array<float, 1025> T = [] { std::array<float, 1025> t{}; for (int i = 0; i <= 1024; i++) t[i] = (float)std::pow(i / 1024.0, 1.6); return t; }();
    const float s = clampv(x, 0.0f, 1.0f) * 1024; const int i = std::min(1023, (int)s); return T[i] + (s - i) * (T[i + 1] - T[i]);
}
// подоболочка атома для рисунка: светится там, где вероятность найти её электрон на расстоянии r (r²R²) близка
// к наибольшей; rad — r²R² по таблице (наибольшее — 1), pn — переход к вероятности на единицу ln r (r³R² / ∫r²R²dr)
struct ShellArt {
    int n, l, occ; double Zs, Zr, amx = 1, rPeak = 1, pn = 1; float c[3], w;
    bool round = false;                    // s или заполненная подоболочка — шар: угловую сумму считать не нужно
    std::vector<std::pair<int, int>> ms;   // занятые орбитали и электронов на каждой (правило Хунда)
    std::vector<float> rad;
};
// весь атом: подоболочки, радиус рисунка, оболочка 1s и плотность облака (относительно плотности у ядра)
struct AtomArt { std::vector<ShellArt> sh; double half = 0.3, rIn = 1; int nmax = 1; std::vector<float> core; };
static float shellGlow(const ShellArt& s, rtab::Pos p, double ux, double uy, double uz) {
    float x = rtab::at(s.rad, p);
    if (!s.round) { double sa = 0; for (auto& mk : s.ms) { const double Y = realY(s.l, mk.first, ux, uy, uz); sa += mk.second * Y * Y; } x *= (float)(sa / s.amx); }
    return s.w * pow16(x);
}
static const AtomArt& atomArt(int Z) {
    static std::map<int, AtomArt> cache;
    auto it = cache.find(Z); if (it != cache.end()) return it->second;
    if (cache.size() > 24) cache.clear();
    AtomArt& A = cache[Z]; int occ[8][4]; zOccupancy(Z, occ);
    for (int n = 1; n <= 7; n++) for (int l = 0; l < 4 && l < n; l++) if (occ[n][l] > 0) A.nmax = std::max(A.nmax, n);
    std::vector<double> rho(rtab::K, 0.0);
    for (int n = 1; n <= 7; n++) for (int l = 0; l < 4 && l < n; l++) {
        const int e = occ[n][l]; if (e <= 0) continue;
        ShellArt s; s.n = n; s.l = l; s.occ = e; s.Zs = slaterZeff(Z, occ, n, l); s.Zr = radialZ(n, l, s.Zs);
        s.w = n == A.nmax ? 0.8f : (n == A.nmax - 1 ? 0.9f : 0.7f); orbColor(l, false, s.c[0], s.c[1], s.c[2]);
        for (int m = 0; m < orbCount(l); m++) { const int k = hundOcc(e, l, m); if (k) s.ms.push_back({m, k}); }
        s.round = l == 0 || e == 2 * orbCount(l);
        s.rad.resize(rtab::K); double mx = 0, norm = 0;
        for (int k = 0; k < rtab::K; k++) {
            const double r = rtab::r(k), R = radialR(n, l, s.Zr, r), v = r * r * R * R;
            s.rad[k] = (float)v; norm += v * r * rtab::DL; if (v > mx) { mx = v; s.rPeak = r; }
        }
        for (int k = 0; k < rtab::K; k++) { const double r = rtab::r(k); rho[k] += e * s.rad[k] / (r * r) / norm; s.rad[k] = (float)(s.rad[k] / mx); }
        s.pn = mx / norm;
        // наибольшая угловая сумма Σ e·Y² по направлениям (у заполненной подоболочки она одна и та же везде)
        double amx = 0; std::mt19937 rg(11 + n * 7 + l);
        for (int k = 0; k < 1500; k++) {
            double v[3] = {std::normal_distribution<double>()(rg), std::normal_distribution<double>()(rg), std::normal_distribution<double>()(rg)};
            const double lv = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) + 1e-12;
            double sa = 0; for (auto& mk : s.ms) { const double Y = realY(l, mk.first, v[0] / lv, v[1] / lv, v[2] / lv); sa += mk.second * Y * Y; }
            amx = std::max(amx, sa);
        }
        s.amx = std::max(1e-300, amx);
        if (n == A.nmax) A.half = std::max(A.half, 1.08 * radialQuantile(n, l, s.Zr, 0.85));
        if (n == 1) A.rIn = 1 / s.Zr;
        A.sh.push_back(s);
    }
    A.core.resize(rtab::K);
    for (int k = 0; k < rtab::K; k++) A.core[k] = (float)(rho[k] / std::max(1e-300, rho[0]));
    return A;
}
// одна орбиталь (n, l) атома Z: R(r) / max|R| по таблице
struct OrbArt { double Zs = 1, Zr = 1; std::vector<float> R; };
static const OrbArt& orbArt(int Z, int n, int l) {
    static std::map<std::array<int, 3>, OrbArt> cache;
    const std::array<int, 3> key{Z, n, l}; auto it = cache.find(key); if (it != cache.end()) return it->second;
    if (cache.size() > 24) cache.clear();
    OrbArt& O = cache[key]; int occ[8][4]; zOccupancy(Z, occ); if (occ[n][l] == 0) occ[n][l] = 1;
    O.Zs = slaterZeff(Z, occ, n, l); O.Zr = radialZ(n, l, O.Zs); O.R.resize(rtab::K); double mx = 0;
    for (int k = 0; k < rtab::K; k++) { const double R = radialR(n, l, O.Zr, rtab::r(k)); O.R[k] = (float)R; mx = std::max(mx, std::fabs(R)); }
    for (float& v : O.R) v = (float)(v / std::max(1e-300, mx));
    return O;
}

// ---- облако на сетке: ψ одной орбитали или яркость и цвет оболочек всего атома в узлах N³ куба [−half, half]³
// (боровские радиусы; вертикаль рисунка — ось z орбитали)
struct VolGrid {
    int N = 0; float half = 1;
    bool atom = false;                   // весь атом: в q по четыре числа на узел — яркость (0…1) и цвет r g b; иначе a — ψ / max|ψ|
    std::vector<float> a, q;
    float iso = 0;                       // |ψ|² поверхности, внутри которой 85% вероятности
};
static inline float volAt(const std::vector<float>& v, int N, float fx, float fy, float fz) {
    const int x0 = clampv((int)fx, 0, N - 2), y0 = clampv((int)fy, 0, N - 2), z0 = clampv((int)fz, 0, N - 2);
    const float tx = fx - x0, ty = fy - y0, tz = fz - z0;
    const size_t i = ((size_t)z0 * N + y0) * N + x0, sy = N, sz = (size_t)N * N;
    const float c00 = v[i] + tx * (v[i + 1] - v[i]), c10 = v[i + sy] + tx * (v[i + sy + 1] - v[i + sy]);
    const float c01 = v[i + sz] + tx * (v[i + sz + 1] - v[i + sz]), c11 = v[i + sz + sy] + tx * (v[i + sz + sy + 1] - v[i + sz + sy]);
    const float c0 = c00 + ty * (c10 - c00), c1 = c01 + ty * (c11 - c01);
    return c0 + tz * (c1 - c0);
}
// то же для четырёх чисел на узел разом (яркость и цвет атома): одно чтение памяти вместо четырёх
static inline void volAt4(const std::vector<float>& v, int N, float fx, float fy, float fz, float* o) {
    const int x0 = clampv((int)fx, 0, N - 2), y0 = clampv((int)fy, 0, N - 2), z0 = clampv((int)fz, 0, N - 2);
    const float tx = fx - x0, ty = fy - y0, tz = fz - z0;
    const size_t i = (((size_t)z0 * N + y0) * N + x0) * 4, sy = (size_t)N * 4, sz = (size_t)N * N * 4;
    const float w[8] = {(1 - tx) * (1 - ty) * (1 - tz), tx * (1 - ty) * (1 - tz), (1 - tx) * ty * (1 - tz), tx * ty * (1 - tz),
                        (1 - tx) * (1 - ty) * tz, tx * (1 - ty) * tz, (1 - tx) * ty * tz, tx * ty * tz};
    const size_t off[8] = {i, i + 4, i + sy, i + sy + 4, i + sz, i + sz + 4, i + sz + sy, i + sz + sy + 4};
    o[0] = o[1] = o[2] = o[3] = 0;
    for (int k = 0; k < 8; k++) { const float* p = &v[off[k]]; o[0] += w[k] * p[0]; o[1] += w[k] * p[1]; o[2] += w[k] * p[2]; o[3] += w[k] * p[3]; }
}
// одна орбиталь (n, l, m) атома Z
static void buildOrbGrid(VolGrid& G, int Z, int n, int l, int m, int N) {
    const double Zr = orbArt(Z, n, l).Zr;
    G = VolGrid(); G.N = N; G.half = (float)(1.06 * radialQuantile(n, l, Zr, 0.995)); G.a.assign((size_t)N * N * N, 0.0f);
    const double h = 2.0 * G.half / (N - 1);
#pragma omp parallel for schedule(static)
    for (int z = 0; z < N; z++) for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        const double X = -G.half + x * h, Y = -G.half + y * h, Zc = -G.half + z * h, r = std::sqrt(X * X + Y * Y + Zc * Zc);
        const double ang = r > 1e-9 ? realY(l, m, X / r, Zc / r, Y / r) : (l == 0 ? 1.0 : 0.0);
        G.a[((size_t)z * N + y) * N + x] = (float)(radialR(n, l, Zr, r) * ang);
    }
    float mx = 0; for (float v : G.a) mx = std::max(mx, std::fabs(v));
    if (mx > 0) for (float& v : G.a) v /= mx;
    // уровень поверхности: плотности по убыванию, пока не наберётся 85% суммы
    std::vector<float> d(G.a.size()); double tot = 0;
    for (size_t k = 0; k < d.size(); k++) { d[k] = G.a[k] * G.a[k]; tot += d[k]; }
    std::sort(d.begin(), d.end(), std::greater<float>());
    double acc = 0; G.iso = d.empty() ? 0 : d[0];
    for (float v : d) { acc += v; G.iso = v; if (acc >= 0.85 * tot) break; }
}
// весь атом: каждая подоболочка светится своим цветом там, где вероятность найти её электрон на расстоянии r
// (r²·|ψ|²) близка к наибольшей, — оболочки видны слоями, как на радиальном графике справа. По самой плотности
// внутренние оболочки, в тысячи раз плотнее внешних, слились бы в одну белую точку. Внешний слой чуть приглушён,
// чтобы сквозь него были видны внутренние
static void buildAtomGrid(VolGrid& G, int Z, int N) {
    const AtomArt& A = atomArt(Z); const double half = A.half;
    G = VolGrid(); G.N = N; G.half = (float)half; G.atom = true;
    const size_t NN = (size_t)N * N * N; G.q.assign(NN * 4, 0.0f);
    const double h = 2.0 * half / (N - 1);
#pragma omp parallel for schedule(dynamic, 2)
    for (int z = 0; z < N; z++) for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        const double X = -half + x * h, Y = -half + y * h, Zc = -half + z * h, r = std::sqrt(X * X + Y * Y + Zc * Zc);
        const double ux = r > 1e-9 ? X / r : 0, uy = r > 1e-9 ? Zc / r : 0, uz = r > 1e-9 ? Y / r : 1;
        double I = 0, cr = 0, cg = 0, cb = 0; const rtab::Pos p = rtab::pos(r);
        for (const ShellArt& s : A.sh) {
            const double v = shellGlow(s, p, ux, uy, uz);
            I += v; cr += v * s.c[0]; cg += v * s.c[1]; cb += v * s.c[2];
        }
        // к краю рисунка яркость плавно гаснет: шар без резкой границы
        const double t = clampv((1.0 - r / half) / 0.18, 0.0, 1.0), fade = t * t * (3 - 2 * t);
        float* o = &G.q[(((size_t)z * N + y) * N + x) * 4];
        o[0] = (float)(std::min(1.0, I) * fade);
        if (I > 1e-9) { o[1] = (float)(cr / I); o[2] = (float)(cg / I); o[3] = (float)(cb / I); }
    }
}

// ---- рисунок облака лучами: квадрат w×w, поворот (yaw, pitch) как у v3(); RGBA с предумноженной альфой.
// Луч идёт от зрителя вглубь; на каждом шаге облако добавляет свечение и поглощает свет из-за себя.
// V — половина видимого окна (меньше G.half — облако увеличено)
static void volRender(const VolGrid& G, int w, float yaw, float pitch, std::vector<unsigned char>& img, bool cutaway = true, float V = -1) {
    img.assign((size_t)w * w * 4, 0);
    if (G.N < 2) return;
    const float c1 = std::cos(yaw), s1 = std::sin(yaw), c2 = std::cos(pitch), s2 = std::sin(pitch), H = G.half;
    if (V <= 0) V = H;
    const float dir[3] = {-s1 * c2, s2, c1 * c2};                     // шаг вглубь экрана в координатах облака
    const float k = (G.N - 1) / (2 * H), step = 1.1f * 2 * H / (G.N - 1), hn = step / H;
    // свет — сверху слева, чуть от зрителя; нормаль переводится в координаты экрана
    const float Lx = -0.45f, Ly = 0.55f, Lz = -0.70f, ll = std::sqrt(Lx * Lx + Ly * Ly + Lz * Lz), lx = Lx / ll, ly = Ly / ll, lz = Lz / ll;
    const float hx = lx, hy = ly, hz = lz - 1, hl = std::sqrt(hx * hx + hy * hy + hz * hz);
    auto toView = [&](float x, float y, float z, float& X, float& Y, float& D) {
        const float x1 = c1 * x + s1 * z, z1 = -s1 * x + c1 * z; X = x1; Y = c2 * y - s2 * z1; D = s2 * y + c2 * z1; };
    // непрозрачность шага и яркость — по таблице от плотности (0…1): степени и экспоненты на каждом шаге луча
    // съедали бо́льшую часть времени кадра
    // (узлы таблицы — по √d: у слабого сияния вокруг облака нужна точность)
    constexpr int LN = 1024; float lutA[LN], lutB[LN];
    for (int i = 0; i < LN; i++) {
        const float s = (i + 0.5f) / LN, d = s * s;
        if (G.atom) { lutA[i] = 1 - std::exp(-2.2f * d * hn); lutB[i] = 0.2f + 0.75f * d; }
        else { lutA[i] = 1 - std::exp(-4.5f * std::pow(d, 0.7f) * hn); lutB[i] = 0.25f + 0.9f * std::pow(d, 0.35f); }
    }
    auto lut = [&](float d) { return std::min(LN - 1, (int)(std::sqrt(d) * LN)); };
#pragma omp parallel for schedule(dynamic, 4)
    for (int py = 0; py < w; py++) for (int px = 0; px < w; px++) {
        const float X = ((px + 0.5f) / w * 2 - 1) * V, Y = -((py + 0.5f) / w * 2 - 1) * V, q = H * H - X * X - Y * Y;
        if (q <= 0) continue;
        const float Dm = std::sqrt(q);
        // точка на глубине D: x = c1·X + s1·s2·Y − s1·c2·D …
        const float bx = c1 * X + s1 * s2 * Y, by = c2 * Y, bz = s1 * X - c1 * s2 * Y;
        float T = 1, Cr = 0, Cg = 0, Cb = 0, prev = 0; int hits = 0;
        // весь атом — с вырезанной четвертью, как рисуют разрез Земли: справа сверху ближняя половина снята плоскостью
        // через ядро, и на срезе слои оболочек видны кольцами; остальное — светящийся шар целиком
        const bool cut = G.atom && cutaway && X > 0 && Y > 0;
        if (cut) {
            float s4[4]; volAt4(G.q, G.N, (bx + H) * k, (by + H) * k, (bz + H) * k, s4);
            const float face = 0.95f * std::pow(s4[0], 0.8f), op = 0.55f * s4[0];
            Cr += face * s4[1]; Cg += face * s4[2]; Cb += face * s4[3];
            T *= 1 - op;
        }
        for (float D = cut ? 0.5f * step : -Dm; D <= Dm; D += step) {
            const float x = bx + dir[0] * D, y = by + dir[1] * D, z = bz + dir[2] * D;
            const float fx = (x + H) * k, fy = (y + H) * k, fz = (z + H) * k;
            if (G.atom) {
                float s4[4]; volAt4(G.q, G.N, fx, fy, fz, s4); const float a = s4[0]; if (a < 1e-3f) continue;
                const int li = lut(a); const float al = lutA[li], br = lutB[li];   // почти прозрачное свечение: внутренние слои видны сквозь внешние
                Cr += T * al * br * s4[1]; Cg += T * al * br * s4[2]; Cb += T * al * br * s4[3];
                T *= 1 - al;
            } else {
                const float psi = volAt(G.a, G.N, fx, fy, fz), d = psi * psi;
                const float* c = psi >= 0 ? PSI_POS : PSI_NEG;
                if (hits < 2 && d >= G.iso && prev < G.iso) {   // вход в поверхность 85%: глянцевая оболочка
                    // точка входа — между прошлым и этим шагом по линейной оценке: без этого на гладкой доле видны «слои» шага
                    const float t = D > -Dm + 0.5f * step ? clampv((G.iso - prev) / std::max(1e-12f, d - prev), 0.0f, 1.0f) - 1 : 0;
                    const float qx = fx + t * step * k * dir[0], qy = fy + t * step * k * dir[1], qz = fz + t * step * k * dir[2];
                    const float pq = volAt(G.a, G.N, qx, qy, qz), e = 1.0f;
                    const float gx = volAt(G.a, G.N, qx + e, qy, qz) - volAt(G.a, G.N, qx - e, qy, qz);
                    const float gy = volAt(G.a, G.N, qx, qy + e, qz) - volAt(G.a, G.N, qx, qy - e, qz);
                    const float gz = volAt(G.a, G.N, qx, qy, qz + e) - volAt(G.a, G.N, qx, qy, qz - e);
                    float nx = -pq * gx, ny = -pq * gy, nz = -pq * gz, nl = std::sqrt(nx * nx + ny * ny + nz * nz);   // наружу — туда, где |ψ|² убывает
                    float vx = 0, vy = 0, vz = 1;
                    if (nl > 1e-12f) { nx /= nl; ny /= nl; nz /= nl; toView(nx, ny, nz, vx, vy, vz); }
                    // при входе внутрь поверхности её наружная нормаль смотрит на зрителя; иначе это ложное пересечение —
                    // |ψ|² чуть перешла порог из-за интерполяции сетки у узла, — и в тени оно давало тёмные «царапины» на долях
                    if (vz < -0.08f) {   // почти касательные настоящие попадания и так почти прозрачны (см. edge ниже)
                        const float dif = std::max(0.0f, vx * lx + vy * ly + vz * lz), sp = std::pow(std::max(0.0f, (vx * hx + vy * hy + vz * hz) / hl), 40.0f);
                        // у края (луч идёт вскользь) оболочка прозрачнее: контур мягкий, без лесенки пикселей
                        const float edge = clampv((std::fabs(vz) - 0.04f) / 0.30f, 0.0f, 1.0f), fe = edge * edge * (3 - 2 * edge);
                        const float rim = std::pow(1 - std::fabs(vz), 3.0f), as = (hits == 0 ? 0.62f : 0.4f) * (0.15f + 0.85f * fe);
                        const float sh = 0.16f + 0.84f * dif;
                        Cr += T * as * (c[0] * sh + 0.75f * sp + 0.30f * rim * c[0]);
                        Cg += T * as * (c[1] * sh + 0.75f * sp + 0.30f * rim * c[1]);
                        Cb += T * as * (c[2] * sh + 0.75f * sp + 0.30f * rim * c[2]);
                        T *= 1 - as;
                        hits++;
                    }
                }
                prev = d;
                if (d > 1e-5f) {   // светящийся газ: внутри поверхности ярче, вокруг — слабое сияние
                    const int li = lut(d); const float al = lutA[li], br = lutB[li];
                    Cr += T * al * br * c[0]; Cg += T * al * br * c[1]; Cb += T * al * br * c[2];
                    T *= 1 - al;
                }
            }
            if (T < 0.01f) break;
        }
        if (G.atom) { Cr = 1 - std::exp(-2.0f * Cr); Cg = 1 - std::exp(-2.0f * Cg); Cb = 1 - std::exp(-2.0f * Cb); }   // мягкое насыщение ярких мест, как у плёнки
        unsigned char* o = &img[((size_t)py * w + px) * 4];
        o[0] = (unsigned char)(255 * std::min(1.0f, Cr)); o[1] = (unsigned char)(255 * std::min(1.0f, Cg));
        o[2] = (unsigned char)(255 * std::min(1.0f, Cb)); o[3] = (unsigned char)(255 * std::min(1.0f, 1 - T));
    }
}
// угловая форма орбитали — поверхность r = |Y_lm(направление)| / max|Y|: лучи ищут её пересечение, свет — как на
// глянцевой модели; край сглажен по тому, насколько близко луч прошёл к поверхности
static float shapeYmax(int l, int m) {
    static std::map<int, float> cache; const int key = l * 16 + m;
    auto it = cache.find(key); if (it != cache.end()) return it->second;
    double mx = 0;
    for (int i = 0; i <= 90; i++) for (int j = 0; j < 180; j++) {
        const double th = PI * i / 90, ph = 2 * PI * j / 180;
        mx = std::max(mx, std::fabs(realY(l, m, std::sin(th) * std::cos(ph), std::sin(th) * std::sin(ph), std::cos(th))));
    }
    return cache[key] = (float)std::max(1e-9, mx);
}
// в картинку img (ширина iw) — клетка с левым верхним углом (ox, oy) и стороной w. psiSign = 0 — цвета по типу орбитали
// (таблица форм), ±1 — цвета знака ψ, как у облака рядом (знак внешней доли радиальной функции — psiSign)
static void shapeRender(int l, int m, float yaw, float pitch, std::vector<unsigned char>& img, int iw, int ox, int oy, int w, int psiSign = 0) {
    const float c1 = std::cos(yaw), s1 = std::sin(yaw), c2 = std::cos(pitch), s2 = std::sin(pitch), inv = 1 / shapeYmax(l, m), V = 1.06f;
    auto colorOf = [&](bool neg, float& r, float& g, float& b) {
        if (!psiSign) { orbColor(l, neg, r, g, b); return; }
        const float* c = neg != (psiSign < 0) ? PSI_NEG : PSI_POS; r = c[0]; g = c[1]; b = c[2];
    };
    const float dir[3] = {-s1 * c2, s2, c1 * c2}, pix = 2 * V / w;
    const float Lx = -0.45f, Ly = 0.55f, Lz = -0.70f, ll = std::sqrt(Lx * Lx + Ly * Ly + Lz * Lz), lx = Lx / ll, ly = Ly / ll, lz = Lz / ll;
    const float hx = lx, hy = ly, hz = lz - 1, hl = std::sqrt(hx * hx + hy * hy + hz * hz);
    // f < 0 — внутри формы; вертикаль рисунка — ось z орбитали
    auto F = [&](float x, float y, float z, float& Yv) { const float r = std::sqrt(x * x + y * y + z * z); if (r < 1e-6f) { Yv = 1; return -1.0f; }
                                                        Yv = (float)realY(l, m, x / r, z / r, y / r) * inv; return r - std::fabs(Yv); };
#pragma omp parallel for schedule(dynamic, 2)
    for (int py = 0; py < w; py++) for (int px = 0; px < w; px++) {
        const int ix = ox + px, iy = oy + py; if (ix < 0 || iy < 0 || ix >= iw || iy * iw + ix >= (int)(img.size() / 4)) continue;
        const float X = ((px + 0.5f) / w * 2 - 1) * V, Y = -((py + 0.5f) / w * 2 - 1) * V, q = 1.0f - X * X - Y * Y;
        if (q <= -4 * pix) continue;
        const float Dm = std::sqrt(std::max(0.0f, q)) + 2 * pix;
        const float bx = c1 * X + s1 * s2 * Y, by = c2 * Y, bz = s1 * X - c1 * s2 * Y;
        float Yv = 0, fmin = 1e9f, fp = F(bx - dir[0] * Dm, by - dir[1] * Dm, bz - dir[2] * Dm, Yv), Dp = -Dm; bool hit = false; float Dh = 0;
        for (float D = -Dm + 0.018f; D <= Dm; D += 0.018f) {
            const float f = F(bx + dir[0] * D, by + dir[1] * D, bz + dir[2] * D, Yv); fmin = std::min(fmin, f);
            if (f < 0 && fp >= 0) {   // вход в форму: уточнить делением отрезка пополам
                float a = Dp, b = D;
                for (int it = 0; it < 7; it++) { const float mid = 0.5f * (a + b); if (F(bx + dir[0] * mid, by + dir[1] * mid, bz + dir[2] * mid, Yv) < 0) b = mid; else a = mid; }
                Dh = b; hit = true; break;
            }
            fp = f; Dp = D;
        }
        float cr = 0, cg = 0, cb = 0, al = 0;
        if (hit) {
            const float x = bx + dir[0] * Dh, y = by + dir[1] * Dh, z = bz + dir[2] * Dh, e = 0.002f;
            float Yc; F(x, y, z, Yc);
            float d1, d2;
            const float gx = F(x + e, y, z, d1) - F(x - e, y, z, d2), gy = F(x, y + e, z, d1) - F(x, y - e, z, d2), gz = F(x, y, z + e, d1) - F(x, y, z - e, d2);
            const float gl = std::sqrt(gx * gx + gy * gy + gz * gz) + 1e-12f, nx = gx / gl, ny = gy / gl, nz = gz / gl;
            const float x1 = c1 * nx + s1 * nz, z1 = -s1 * nx + c1 * nz, vx = x1, vy = c2 * ny - s2 * z1, vz = s2 * ny + c2 * z1;
            float r, g, b; colorOf(Yc < 0, r, g, b);
            const float dif = std::max(0.0f, vx * lx + vy * ly + vz * lz), sp = std::pow(std::max(0.0f, (vx * hx + vy * hy + vz * hz) / hl), 36.0f);
            const float rim = std::pow(1 - std::fabs(vz), 2.5f), rr = std::sqrt(x * x + y * y + z * z);
            const float ao = 0.55f + 0.45f * std::min(1.0f, rr / 0.35f);   // там, где доли сходятся у центра, — тень
            const float sh = (0.20f + 0.80f * dif) * ao;
            cr = r * sh + 0.65f * sp + 0.22f * rim; cg = g * sh + 0.65f * sp + 0.22f * rim; cb = b * sh + 0.65f * sp + 0.22f * rim; al = 1;
        } else if (fmin < 1.5f * pix) {   // промах рядом с краем: полупрозрачная кромка
            al = clampv(1 - fmin / (1.5f * pix), 0.0f, 1.0f) * 0.8f; float r, g, b; colorOf(Yv < 0, r, g, b);
            cr = 0.35f * r * al; cg = 0.35f * g * al; cb = 0.35f * b * al;
        }
        if (al <= 0) continue;
        unsigned char* o = &img[((size_t)iy * iw + ix) * 4];
        o[0] = (unsigned char)(255 * std::min(1.0f, cr)); o[1] = (unsigned char)(255 * std::min(1.0f, cg));
        o[2] = (unsigned char)(255 * std::min(1.0f, cb)); o[3] = (unsigned char)(255 * std::min(1.0f, al));
    }
}
// ---- разрез через ядро (при приближении): плоскость экрана, проходящая через ядро; окно [−V, V]² боровских радиусов,
// картинка w×w. Считается по формулам, а не по сетке, поэтому подробности видны при любом увеличении: у атома —
// кольца оболочек и почти однородное облако внутри оболочки 1s (у s-электронов плотность у ядра наибольшая),
// у одной орбитали — доли, узлы и контур поверхности 85% (iso — её уровень |ψ|²/max|ψ|², как у объёмного рисунка)
static void sliceRender(int Z, int mode, int n, int l, int m, double V, float iso, float yaw, float pitch, int w, std::vector<unsigned char>& img) {
    img.assign((size_t)w * w * 4, 0);
    const float c1 = std::cos(yaw), s1 = std::sin(yaw), c2 = std::cos(pitch), s2 = std::sin(pitch);
    const AtomArt* A = mode == 0 ? &atomArt(Z) : nullptr; const OrbArt* O = mode == 0 ? nullptr : &orbArt(Z, n, l);
    const float yInv = mode == 0 ? 1.0f : 1.0f / shapeYmax(l, m);
    float sc[3]; orbColor(0, false, sc[0], sc[1], sc[2]);   // облако у ядра — от s-электронов
    std::vector<float> dv; std::vector<char> pos; if (O) { dv.assign((size_t)w * w, -1.0f); pos.assign((size_t)w * w, 1); }
#pragma omp parallel for schedule(dynamic, 4)
    for (int py = 0; py < w; py++) for (int px = 0; px < w; px++) {
        const double X = ((px + 0.5) / w * 2 - 1) * V, Y = -((py + 0.5) / w * 2 - 1) * V;
        const double x = c1 * X + s1 * s2 * Y, y = c2 * Y, z = s1 * X - c1 * s2 * Y, r = std::sqrt(X * X + Y * Y);   // точка плоскости в осях атома
        const double ux = r > 1e-30 ? x / r : 0, uy = r > 1e-30 ? z / r : 0, uz = r > 1e-30 ? y / r : 1;     // оси орбитали: вертикаль рисунка — z
        float Cr = 0, Cg = 0, Cb = 0;
        const rtab::Pos p = rtab::pos(r);
        if (A) {
            float I = 0, cr = 0, cg = 0, cb = 0;
            for (const ShellArt& s : A->sh) { const float v = shellGlow(s, p, ux, uy, uz); I += v; cr += v * s.c[0]; cg += v * s.c[1]; cb += v * s.c[2]; }
            if (I > 1e-6f) { const float face = 0.95f * std::pow(std::min(1.0f, I), 0.8f) / I; Cr = face * cr; Cg = face * cg; Cb = face * cb; }
            const float core = 0.42f * std::pow(rtab::at(A->core, p), 0.3f);   // плотность облака относительно ядра: гаснет за оболочкой 1s
            Cr += core * sc[0]; Cg += core * sc[1]; Cb += core * sc[2];
            Cr = 1 - std::exp(-2.0f * Cr); Cg = 1 - std::exp(-2.0f * Cg); Cb = 1 - std::exp(-2.0f * Cb);
        } else {
            const float psi = rtab::at(O->R, p) * (float)realY(l, m, ux, uy, uz) * yInv, d = psi * psi;
            dv[(size_t)py * w + px] = d - iso; pos[(size_t)py * w + px] = psi >= 0;
            const float* c = psi >= 0 ? PSI_POS : PSI_NEG, I = d > 1e-7f ? 0.95f * std::pow(d, 0.35f) : 0.0f;
            Cr = 1 - std::exp(-1.8f * I * c[0]); Cg = 1 - std::exp(-1.8f * I * c[1]); Cb = 1 - std::exp(-1.8f * I * c[2]);
        }
        unsigned char* o = &img[((size_t)py * w + px) * 4];
        o[0] = (unsigned char)(255 * std::min(1.0f, Cr)); o[1] = (unsigned char)(255 * std::min(1.0f, Cg)); o[2] = (unsigned char)(255 * std::min(1.0f, Cb)); o[3] = 255;
    }
    if (!O) return;
    // контур поверхности 85%: пиксель, у которого сосед по другую сторону от уровня, — светлее, цвета своей доли
    for (int py = 0; py + 1 < w; py++) for (int px = 0; px + 1 < w; px++) {
        const float a = dv[(size_t)py * w + px], b = dv[(size_t)py * w + px + 1], c = dv[(size_t)(py + 1) * w + px];
        if ((a >= 0) == (b >= 0) && (a >= 0) == (c >= 0)) continue;
        unsigned char* o = &img[((size_t)py * w + px) * 4];
        const float* cc = pos[(size_t)py * w + px] ? PSI_POS : PSI_NEG;
        for (int k = 0; k < 3; k++) o[k] = (unsigned char)std::min(255.0f, 0.35f * o[k] + 255 * (0.45f + 0.5f * cc[k]) * 0.75f);
    }
}
// текстуры рисунков: номер места → текстура (содержимое меняется, размер — по надобности)
static GLuint slotTex[8] = {0}; static int slotW[8] = {0}, slotH[8] = {0};
static void imgToTex(int slot, const std::vector<unsigned char>& img, int w, int h) {
    if (slot < 0 || slot >= 8) return;
    if (!slotTex[slot]) {
        glGenTextures(1, &slotTex[slot]); glBindTexture(GL_TEXTURE_2D, slotTex[slot]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, slotTex[slot]);
    if (slotW[slot] != w || slotH[slot] != h) { glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data()); slotW[slot] = w; slotH[slot] = h; }
    else glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
}
// рисунок из места slot — прямоугольником (x, y, w, h), с предумноженной альфой; a — непрозрачность всего рисунка
static void drawSlot(int slot, float x, float y, float w, float h, float a = 1) {
    if (slot < 0 || slot >= 8 || !slotTex[slot] || a <= 0) return;
    MonoAtoms atomsColored;
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, slotTex[slot]); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); glColor4f(a, a, a, a);
    glBegin(GL_QUADS); glTexCoord2f(0, 0); glVertex2f(x, y); glTexCoord2f(1, 0); glVertex2f(x + w, y); glTexCoord2f(1, 1); glVertex2f(x + w, y + h); glTexCoord2f(0, 1); glVertex2f(x, y + h); glEnd();
    glDisable(GL_TEXTURE_2D); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
// облака на сетке — по запросу, с запоминанием последних
static const VolGrid& atomGrid(int Z, int N) {
    static std::map<std::pair<int, int>, VolGrid> cache;
    auto key = std::make_pair(Z, N); auto it = cache.find(key); if (it != cache.end()) return it->second;
    if (cache.size() > 24) cache.clear();
    VolGrid& G = cache[key]; buildAtomGrid(G, Z, N); return G;
}
static const VolGrid& orbitalGrid(int Z, int n, int l, int m, int N) {
    static std::map<std::array<int, 5>, VolGrid> cache;
    std::array<int, 5> key{Z, n, l, m, N}; auto it = cache.find(key); if (it != cache.end()) return it->second;
    if (cache.size() > 24) cache.clear();
    VolGrid& G = cache[key]; buildOrbGrid(G, Z, n, l, m, N); return G;
}

// ---- вид: поворот вокруг вертикали (рыскание) и наклон; экранные координаты
struct View3 { float cx, cy, s, yaw, pitch; };
static inline void v3(const View3& v, float x, float y, float z, float& X, float& Y, float& D) {
    const float c1 = std::cos(v.yaw), s1 = std::sin(v.yaw), c2 = std::cos(v.pitch), s2 = std::sin(v.pitch);
    const float x1 = c1 * x + s1 * z, z1 = -s1 * x + c1 * z, y2 = c2 * y - s2 * z1, z2 = s2 * y + c2 * z1;
    X = v.cx + x1 * v.s; Y = v.cy - y2 * v.s; D = z2;
}
// ядро — крошечная точка в центре (в масштабе рисунка оно было бы в 10⁴–10⁵ раз меньше облака)
static void drawNucleusDot(const View3& v, float R) {
    MonoAtoms atomsColored;
    glColor4f(1.0f, 0.95f, 0.9f, 0.95f); discPx(v.cx, v.cy, R, 16);
}
// оси x, y, z (серые) — как на рисунках орбиталей; a — непрозрачность (при приближении оси гаснут)
static void drawAxes3(const View3& v, float len, float a = 1) {
    if (a <= 0) return;
    const char* nm[3] = {"x", "z", "y"}; const float dir[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    glEnable(GL_LINE_SMOOTH);
    for (int k = 0; k < 3; k++) {
        float X0, Y0, D0, X1, Y1, D1; v3(v, -dir[k][0] * len, -dir[k][1] * len, -dir[k][2] * len, X0, Y0, D0); v3(v, dir[k][0] * len, dir[k][1] * len, dir[k][2] * len, X1, Y1, D1);
        col(withA(C_DIM, 0.55f * a)); glBegin(GL_LINES); arrowPx(X0, Y0, X1, Y1, uiPx(6)); glEnd();
        drawText(fontXS, X1 + uiPx(3), Y1 - fontXS.h, nm[k], withA(C_DIM, a));
    }
    glDisable(GL_LINE_SMOOTH);
}
// миниатюра атома (таблица Менделеева, карточка атома): облако внешней части атома, медленно вращается
static void drawAtomMini(int Z, float x, float y, float s, float t) {
    if (Z < 1 || Z > 118) return;
    static std::vector<unsigned char> img; static int lastZ = -1, lastW = 0; static float lastT = -1;
    const int w = clampv((int)s, 32, 144);
    if (Z != lastZ || w != lastW || std::fabs(t - lastT) > 1.0f / 30) {   // вращение медленное: хватает 30 кадров в секунду
        volRender(atomGrid(Z, 44), w, t * 0.5f, 0.35f, img, false); imgToTex(0, img, w, w); lastZ = Z; lastW = w; lastT = t;
    }
    pushClip(x, y, s, s);
    const float m = s * 0.08f; drawSlot(0, x + m, y + m, s - 2 * m, s - 2 * m);
    drawNucleusDot({x + s / 2, y + s / 2, s * 0.36f, 0, 0}, std::max(1.0f, s * 0.012f));
    popClip();
}

// ---- приближение: от облака электронов к ядру и внутрь нуклона
namespace nuc { static const std::vector<std::array<float, 3>>& nucleonPack(int A); }   // world_nuclear.inl: укладка нуклонов в ядре
static void qkColor(int col, bool anti, float& r, float& g, float& b);                   // world_quark.inl: цвет кварка
namespace avz {
constexpr double FM = 1.8897261e-5;   // фемтометр в боровских радиусах
constexpr double R_NUCLEON = 0.84;    // радиус протона, фм
constexpr double PACK = 1.94;         // единица укладки nucleonPack в фм: радиус ядра 1.2·A^(1/3) фм
}
static int massNumber(int Z) { return std::max(Z, (int)std::lround(ZD[clampv(Z, 1, 118)].mass)); }
static double nucleusRadiusFm(int A) { return 1.2 * std::cbrt((double)A); }
static inline float smooth01(float a, float b, float x) { const float t = clampv((x - a) / (b - a), 0.0f, 1.0f); return t * t * (3 - 2 * t); }
// длина для подписи (в метрах): Å, пм или фм
static std::string lenLabel(double lenM) {
    if (lenM >= 0.99e-11) return fmt("%g Å", lenM / 1e-10);   // 0.2 Å, а не 20 пм — на масштабе атома привычнее ангстремы
    if (lenM >= 0.99e-12) return fmt("%g пм", lenM / 1e-12);
    return fmt("%g фм", lenM / 1e-15);
}
// радиус оболочки для подписи: Å, а меньше 0.1 Å — пикометры
static std::string radLabel(double rA) { return rA >= 0.1 ? fmt("%.2f Å", rA) : fmt("%.1f пм", rA * 100); }
// увеличение: ×250, ×3.4·10⁴
static std::string zoomLabel(double z) {
    if (z < 1000) return fmt("×%.0f", z);
    const int e = (int)std::floor(std::log10(z)); static const char* SUP[10] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    std::string es; for (char ch : std::to_string(e)) es += SUP[ch - '0'];
    return fmt("×%.1f·10", z / std::pow(10.0, e)) + es;
}
// во сколько раз: два знака и пробелы между тысячами (66 000)
static std::string bigRatio(double v) {
    const double p = std::pow(10.0, std::floor(std::log10(std::max(v, 1.0))) - 1); const long long n = std::llround(std::round(v / p) * p);
    const std::string s = std::to_string(n); std::string o;
    for (size_t i = 0; i < s.size(); i++) { if (i && (s.size() - i) % 3 == 0) o += ' '; o += s[i]; }
    return o;
}
// линейка масштаба: «круглая» длина (1, 2, 5 × 10ⁿ), подпись над ней
static void drawScaleBar(float x, float y, double pxPerBohr) {
    const double pxPerM = pxPerBohr / 5.29177e-11, want = uiPx(100) / pxPerM;
    const double dec = std::pow(10.0, std::floor(std::log10(want))), mm = want / dec, L = dec * (mm < 2 ? 1 : mm < 5 ? 2 : 5);
    const float lp = (float)(L * pxPerM);
    rectFill(x, y, lp, 1, C_TEXT); rectFill(x, y - uiPx(4), 1, uiPx(5), C_TEXT); rectFill(x + lp - 1, y - uiPx(4), 1, uiPx(5), C_TEXT);
    drawTextC(fontXS, x + lp / 2, y - uiPx(4) - fontXS.h, lenLabel(L), C_TEXT);
}
// внутри нуклона: три кварка (протон — u u d, нейтрон — u d d) и «Y»-струна глюонного поля; оболочка нуклона по мере
// приближения (tq: 0 → 1) становится прозрачной. (X, Y) — центр нуклона на экране, pf — пикселей на фм
static void drawNucleonInside(float X, float Y, float pf, float yaw, float pitch, bool proton, float tq, double t) {
    const float Rp = (float)(avz::R_NUCLEON * pf); const View3 vq{X, Y, pf, yaw, pitch};
    float P[3][2]; const int dq[3] = {0, proton ? 0 : 1, 1};   // 1 — d-кварк
    for (int j = 0; j < 3; j++) {   // треугольник ≈ 0.35 фм, плоскость медленно кувыркается, кварки дрожат
        const double a = 0.7 * t + 2.0944 * j, rr = 0.36 * (1 + 0.10 * std::sin(2.3 * t + 1.7 * j)), tilt = 0.5 + 0.3 * std::sin(0.4 * t);
        float D; v3(vq, (float)(rr * std::cos(a)), (float)(rr * std::sin(a) * std::cos(tilt)), (float)(rr * std::sin(a) * std::sin(tilt)), P[j][0], P[j][1], D);
    }
    const float Jx = (P[0][0] + P[1][0] + P[2][0]) / 3, Jy = (P[0][1] + P[1][1] + P[2][1]) / 3;
    MonoAtoms atomsColored;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);   // свечение: струна от каждого кварка к узлу «Y» белеет к узлу
    for (int j = 0; j < 3; j++) {
        float r, g, b; qkColor(j, false, r, g, b);
        const float dx = Jx - P[j][0], dy = Jy - P[j][1], L = std::sqrt(dx * dx + dy * dy); const int ns = std::max(2, (int)(L / uiPx(3)));
        for (int k = 0; k <= ns; k++) { const float u = (float)k / ns; quadUV(P[j][0] + dx * u, P[j][1] + dy * u, 0.10f * pf, r + (1 - r) * 0.6f * u, g + (1 - g) * 0.6f * u, b + (1 - b) * 0.6f * u, 0.3f * tq); }
        quadUV(P[j][0], P[j][1], 0.30f * pf, r, g, b, 0.55f * tq);
    }
    flushQuads(texGlow);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    for (int j = 0; j < 3; j++) {
        float r, g, b; qkColor(j, false, r, g, b); const float R = 0.085f * pf;
        quadUV(P[j][0], P[j][1], R, r * tq, g * tq, b * tq, tq, 0.0f, AT_W); if (R > 2) quadSpec(P[j][0], P[j][1], R, 0.8f * tq);
    }
    flushQuads(texCore);
    const float a = 1 - 0.78f * tq, nr = proton ? 0.95f : 0.62f, ng = proton ? 0.30f : 0.66f, nb = proton ? 0.25f : 0.74f;
    quadUV(X, Y, Rp, nr * a, ng * a, nb * a, a, 0.0f, AT_W); if (Rp > 2) quadSpec(X, Y, Rp, 0.15f + 0.45f * a);
    flushQuads(texCore);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH); col(withA(proton ? hexc(0xF26450) : hexc(0x9EA8B8), 0.5f * tq)); circlePx(X, Y, Rp, 96); glDisable(GL_LINE_SMOOTH);
    if (tq > 0.4f) for (int j = 0; j < 3; j++) {   // подпись аромата — на тёмной подложке: рядом светлая струна
        const float lx = P[j][0] + 0.11f * pf, ly = P[j][1] - 0.11f * pf - fontS.h, lw = textW(fontS, dq[j] ? "d" : "u");
        rectFill(lx - uiPx(3), ly, lw + uiPx(6), fontS.h, {0, 0, 0, 0.5f * tq});
        drawText(fontS, lx, ly, dq[j] ? "d" : "u", withA(C_TEXT_HI, tq));
    }
}
// ядро из нуклонов вокруг центра вида (v.s — пикселей на фм): протоны красные, нейтроны серые. cut — ближняя половина
// срезана плоскостью разреза; tq > 0 — кадр съезжает к центральному нуклону, и тот открывается
static void drawNucleusArt(const View3& v, int Z, int A, float tq, bool cut, double t) {
    const auto& pk = nuc::nucleonPack(A); const float u = (float)avz::PACK, Rp = (float)(avz::R_NUCLEON * v.s);
    int c = 0; float best = 1e30f;
    for (int k = 0; k < A; k++) { const float d = pk[k][0] * pk[k][0] + pk[k][1] * pk[k][1] + pk[k][2] * pk[k][2]; if (d < best) { best = d; c = k; } }
    const float ox = pk[c][0] * u * tq, oy = pk[c][1] * u * tq, oz = pk[c][2] * u * tq;
    struct It { float X, Y, D; int k; }; std::vector<It> items; items.reserve(A);
    for (int k = 0; k < A; k++) {
        float X, Y, D; v3(v, pk[k][0] * u - ox, pk[k][1] * u - oy, pk[k][2] * u - oz, X, Y, D);
        if (cut && k != c && D < -0.35f) continue;   // D — фм от плоскости разреза, к зрителю — минус
        items.push_back({X, Y, D, k});
    }
    std::sort(items.begin(), items.end(), [](const It& a, const It& b) { return a.D > b.D; });
    MonoAtoms atomsColored; glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    for (const It& q : items) {
        const bool pr = q.k < Z;
        if (q.k == c && tq > 0.02f) { flushQuads(texCore); drawNucleonInside(q.X, q.Y, v.s, v.yaw, v.pitch, pr, tq, t); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); continue; }
        const float a = 1 - 0.6f * tq;   // у кварков соседние нуклоны приглушены: смотрим в центральный
        quadUV(q.X, q.Y, Rp, (pr ? 0.95f : 0.62f) * a, (pr ? 0.30f : 0.66f) * a, (pr ? 0.25f : 0.74f) * a, a, 0.0f, AT_W); if (Rp > 2) quadSpec(q.X, q.Y, Rp, 0.6f * a);
    }
    flushQuads(texCore); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
// радиус первой внутренней доли орбитали (по таблице): у s — первый узел, у остальных — первый горб |R|
static double orbInnerR(const OrbArt& O, int n, int l) {
    if (l == 0 && n == 1) return 1 / O.Zr;
    for (int k = 1; k + 1 < rtab::K; k++) {
        if (l == 0 ? (O.R[k] > 0) != (O.R[k - 1] > 0) : std::fabs(O.R[k + 1]) < std::fabs(O.R[k])) return rtab::r(k);
    }
    return 1 / O.Zr;
}

// ---- окно «Строение атома» (F7)
static int avMode = 0, avN = 2, avL = 1, avM = 0;   // режим: 0 облако атома, 1 одна орбиталь, 2 все формы (элемент — avZ)
static float avYaw = 0.6f, avPitch = 0.38f; static bool avDrag = false; static float avMx = 0, avMy = 0; static double avClock = 0;
// приближение: в кадре — шар радиуса H0/avZoom (H0 — радиус рисунка атома или орбитали); колесо мыши, + и −, кнопки уровней
static double avZoom = 1, avZoomGoal = 1;
static PR avRect;
// ширина подписи орбитали (как рисует drawOrbName)
static float orbNameW(const Font& f, int n, int l, int m) {
    float w = 0; if (n > 0) w += textWRaw(f, std::to_string(n));
    w += textWRaw(f, std::string(1, "spdf"[l])); if (l > 0 && m >= 0) w += textWRaw(fontXS, ORB_SUB[l][m]);
    return w;
}
static void drawAtomView(double frameDt) {
    const float pad = uiPx(16), headH = uiPx(46);
    const float W = std::min((float)winW - uiPx(24), uiPx(1180)), H = std::min((float)winH - uiPx(16), uiPx(780));
    const float x0 = std::floor((winW - W) / 2), y0 = std::floor(std::max(uiPx(8), (winH - H) / 2));
    avRect = {x0, y0, W, H};
    avZ = clampv(avZ, 1, 118);
    rectFill(0, 0, (float)winW, (float)winH, {0, 0, 0, 0.6f});
    boxPanel(x0, y0, W, H, C_PANEL, C_LINE_H); rectFill(x0, y0, W, uiPx(2), C_ACC);
    const int t = typeOfZ(avZ);
    drawText(fontL, x0 + pad, y0 + uiPx(12), fmt("Строение атома — %s, %s (Z = %d)", EL[t].sym, T(EL[t].name), avZ), C_TEXT_HI);
    drawTextR(fontXS, x0 + W - pad, y0 + uiPx(17), avMode == 2 ? "мышь — повернуть · Esc / F7 — закрыть" : "мышь — повернуть · колесо — ближе к ядру · Esc / F7 — закрыть", C_DIM);
    // слева — вид, справа — управление и пояснения
    const float vs = std::min(H - headH - pad, W * 0.58f), vx = x0 + pad, vy = y0 + headH;
    boxPanel(vx, vy, vs, vs, hexc(0x030303), C_LINE);
    int occ[8][4]; zOccupancy(avZ, occ);
    if (occ[avN][avL] == 0 && avMode == 1) {   // выбранной подоболочки у атома нет — внешняя занятая
        for (int n = 7; n >= 1; n--) { bool f = false; for (int l = 3; l >= 0; l--) if (occ[n][l]) { avN = n; avL = l; f = true; break; } if (f) break; }
        avM = 0;
    }
    // ---- приближение: масштаб держится и при смене элемента (можно сравнивать ядра); плавно, по логарифму
    const bool zoomable = avMode != 2;
    const double H0 = avMode == 0 ? atomGrid(avZ, 64).half : avMode == 1 ? orbitalGrid(avZ, avN, avL, avM, 80).half : 1.0;
    static double lastH0 = -1;
    if (lastH0 > 0 && std::fabs(H0 - lastH0) > 1e-12 && avZoomGoal > 1.001) { const double k = H0 / lastH0; avZoomGoal *= k; avZoom *= k; }
    lastH0 = H0;
    const double zMax = H0 / avz::FM;   // в кадре не меньше 1 фм: нуклон почти во весь вид
    if (!zoomable) avZoom = avZoomGoal = 1;
    avZoomGoal = clampv(avZoomGoal, 1.0, zMax); avZoom = clampv(avZoom, 1.0, zMax);
    {
        const double lz = std::log(avZoom), lg = std::log(avZoomGoal);
        avZoom = std::fabs(lg - lz) < 1e-4 ? avZoomGoal : std::exp(lz + (lg - lz) * (1 - std::exp(-frameDt * 8)));
    }
    const double s = H0 / avZoom;                                                  // половина видимого окна, боровские радиусы
    const float fs = zoomable ? smooth01(1.4f, 2.2f, (float)avZoom) : 0.0f;        // доля разреза вместо объёмного облака
    const float mg = vs * 0.04f * (1 - fs), iw = vs - 2 * mg;                      // поле вокруг рисунка (у разреза — во весь вид)
    const double pxB = iw / 2 / s, pf = pxB * avz::FM;                             // пикселей на боровский радиус и на фемтометр
    const int A = massNumber(avZ); const double RN = nucleusRadiusFm(A), RNpx = RN * pf;
    const float tq = zoomable ? smooth01(40.0f, 110.0f, (float)(avz::R_NUCLEON * pf)) : 0.0f;   // нуклон открывается: кварки
    // кнопки уровней — в правом нижнем углу вида (щелчок по ним не поворачивает вид)
    const float zbH = uiPx(22); PR zr{0, 0, 0, 0};
    const char* ZL[4] = {avMode == 0 ? "атом" : "орбиталь", avMode == 0 ? "1s" : "центр", "ядро", "кварки"};
    float zbW[4], zbSum = 0;
    for (int k = 0; k < 4; k++) { zbW[k] = std::max(uiPx(40), textW(fontU, ZL[k]) + uiPx(16)); zbSum += zbW[k] + uiPx(4); }
    if (zoomable) zr = {vx + vs - uiPx(8) - zbSum + uiPx(4), vy + vs - zbH - uiPx(8), zbSum - uiPx(4), zbH};
    const bool hov = ui.mx >= vx && ui.mx < vx + vs && ui.my >= vy && ui.my < vy + vs;
    if (hov && ui.pressed && !inPR(zr)) { avDrag = true; avMx = (float)ui.mx; avMy = (float)ui.my; }
    if (!ui.down) avDrag = false;
    if (avDrag) { avYaw += (ui.mx - avMx) * 0.01f; avPitch = clampv(avPitch + (ui.my - avMy) * 0.01f, -1.4f, 1.4f); avMx = (float)ui.mx; avMy = (float)ui.my; }
    else if (avZoom < 1.3) avYaw += (float)frameDt * 0.25f;   // крутится, только пока виден весь атом: разрез и ядро стоят
    if (zoomable && hov && ui.wheel != 0) { avZoomGoal = clampv(avZoomGoal * std::pow(1.6, ui.wheel / 120.0), 1.0, zMax); ui.wheel = 0; }
    avClock += frameDt;
    pushClip(vx, vy, vs, vs);
    static std::vector<unsigned char> img, img2, imgS;
    const bool moving = avDrag || std::fabs(std::log(avZoom / avZoomGoal)) > 1e-3;
    const float turn = avDrag ? 0.003f : 0.012f;   // вращение мышью — сразу, медленный автоповорот — ступеньками меньше градуса
    static float y3 = 0, p3 = 0;   // поворот, с которым посчитан объёмный рисунок (по нему же — оси: иначе они «плывут»)
    if (avMode != 2) {
        if (fs < 0.999f) {   // объёмное облако: весь атом и первые шаги приближения
            const int res = clampv((int)(vs * 0.5f), 160, 360);
            static int k3[7] = {-1}; static double z3 = -1;
            const int key[7] = {avMode, avZ, avN, avL, avM, res, (int)vs};
            if (memcmp(key, k3, sizeof(key)) != 0 || std::fabs(avYaw - y3) > turn || std::fabs(avPitch - p3) > turn || z3 <= 0 || std::fabs(std::log(avZoom / z3)) > 0.002) {
                memcpy(k3, key, sizeof(key)); y3 = avYaw; p3 = avPitch; z3 = avZoom;
                const VolGrid& G = avMode == 0 ? atomGrid(avZ, 64) : orbitalGrid(avZ, avN, avL, avM, 80);
                volRender(G, res, y3, p3, img, true, (float)(G.half / avZoom)); imgToTex(1, img, res, res);
            }
            drawSlot(1, vx + mg, vy + mg, iw, iw, 1 - fs);
            if (avMode == 1 && fs < 0.5f) drawAxes3({vx + vs / 2, vy + vs / 2, iw / 2, y3, p3}, 1.0f, 1 - 2 * fs);
        }
        if (fs > 0.001f) {   // разрез через ядро: при движении — вдвое реже по пикселям, после — чётко
            const int res = moving ? clampv((int)(vs * 0.5f), 120, 360) : clampv((int)vs, 160, 760);
            static int kS[7] = {-1}; static float yS = 0, pS = 0; static double zS = -1;
            const int key[7] = {avMode, avZ, avN, avL, avM, res, (int)vs};
            if (memcmp(key, kS, sizeof(key)) != 0 || std::fabs(avYaw - yS) > turn || std::fabs(avPitch - pS) > turn || zS <= 0 || std::fabs(std::log(avZoom / zS)) > 1e-4) {
                memcpy(kS, key, sizeof(key)); yS = avYaw; pS = avPitch; zS = avZoom;
                const float iso = avMode == 1 ? orbitalGrid(avZ, avN, avL, avM, 80).iso : 0.0f;
                sliceRender(avZ, avMode, avN, avL, avM, s, iso, yS, pS, res, imgS); imgToTex(3, imgS, res, res);
            }
            drawSlot(3, vx + mg, vy + mg, iw, iw, fs);
        }
        if (avMode == 0 && fs > 0.5f) {   // подписи колец оболочек (внизу слева от центра), если кольцо видно и не теснится
            const AtomArt& Aa = atomArt(avZ); float lastR = -1e9f;
            for (int n = 1; n <= Aa.nmax; n++) {
                double rr = 0; int cnt = 0; std::string nm;
                for (const ShellArt& sh : Aa.sh) if (sh.n == n) { rr += sh.rPeak; cnt++; nm += (nm.empty() ? "" : " ") + std::to_string(n) + "spdf"[sh.l]; }
                if (!cnt) continue;
                const float Rpx = (float)(rr / cnt * pxB);
                if (Rpx < uiPx(26) || Rpx > iw * 0.47f || Rpx - lastR < uiPx(16)) continue;
                lastR = Rpx;
                const float lx = vx + vs / 2 - Rpx * 0.7071f + uiPx(3), ly = vy + vs / 2 + Rpx * 0.7071f, lw = textW(fontXS, nm);
                rectFill(lx - uiPx(3), ly - uiPx(1), lw + uiPx(6), fontXS.h + uiPx(2), {0, 0, 0, 0.5f * fs});   // подложка: кольца яркие
                drawText(fontXS, lx, ly, nm, withA(C_TEXT_HI, fs));
            }
        }
        // ядро: точка → шар → нуклоны → внутри нуклона кварки
        const View3 vn{vx + vs / 2, vy + vs / 2, (float)pf, avYaw, avPitch};
        if (RNpx < 1.6) drawNucleusDot(vn, uiPx(2.2f));
        else if (RNpx < 14) {
            MonoAtoms atomsColored; const float fp = (float)avZ / A, R = (float)RNpx;
            glBlendFunc(GL_SRC_ALPHA, GL_ONE); quadUV(vn.cx, vn.cy, 3 * R, 1.0f, 0.75f, 0.6f, 0.22f); flushQuads(texGlow);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            quadAtom(vn.cx, vn.cy, R, 0.62f + 0.33f * fp, 0.66f - 0.36f * fp, 0.74f - 0.49f * fp); if (R > 2) quadSpec(vn.cx, vn.cy, R, 0.6f);
            flushQuads(texCore); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else drawNucleusArt(vn, avZ, A, tq, fs > 0.5f, avClock);
    }
    if (avMode == 1 && RNpx < 14) {   // угловая форма — в углу, цвета знака ψ (как у облака: знак внешней доли — (−1)^(n−l−1))
        const int ws = (int)uiPx(130);
        static int kI[4] = {-1}; static float yI = 0, pI = 0;
        const int key[4] = {avL, avM, ws, avN};
        if (memcmp(key, kI, sizeof(key)) != 0 || std::fabs(avYaw - yI) > turn || std::fabs(avPitch - pI) > turn) {
            memcpy(kI, key, sizeof(key)); yI = avYaw; pI = avPitch;
            img2.assign((size_t)ws * ws * 4, 0); shapeRender(avL, avM, yI, pI, img2, ws, 0, 0, ws, (avN - avL - 1) % 2 == 0 ? 1 : -1); imgToTex(2, img2, ws, ws);
        }
        drawSlot(2, vx + vs - ws - uiPx(10), vy + uiPx(10), (float)ws, (float)ws);
        drawTextR(fontXS, vx + vs - uiPx(12), vy + ws + uiPx(12), "угловая часть |Y|", C_DIM);
    }
    if (avMode == 2) {   // все формы s, p, d, f — как таблица в учебнике
        const int iw2 = (int)vs; const int rows = 4; const float rh = vs / rows;
        static int kF[3] = {-1}; static float yF = 0, pF = 0;
        const int key[3] = {iw2, 0, 0};
        if (memcmp(key, kF, sizeof(key)) != 0 || std::fabs(avYaw - yF) > turn || std::fabs(avPitch - pF) > turn) {
            memcpy(kF, key, sizeof(key)); yF = avYaw; pF = avPitch;
            img.assign((size_t)iw2 * iw2 * 4, 0);
            for (int l = 0; l < 4; l++) for (int m = 0; m < orbCount(l); m++) {
                const float cw = vs / 7, cx = vs / 2 + (m - (orbCount(l) - 1) / 2.0f) * cw, cy = rh * l + rh * 0.44f;
                const int w = (int)(std::min(cw, rh) * 0.86f);
                shapeRender(l, m, yF, pF, img, iw2, (int)(cx - w / 2), (int)(cy - w / 2), w);
            }
            imgToTex(1, img, iw2, iw2);
        }
        drawSlot(1, vx, vy, (float)iw2, (float)iw2);
        for (int l = 0; l < 4; l++) for (int m = 0; m < orbCount(l); m++) {
            const float cw = vs / 7, cx = vx + vs / 2 + (m - (orbCount(l) - 1) / 2.0f) * cw, cy = vy + rh * l + rh * 0.44f;
            drawOrbName(fontU, cx - orbNameW(fontU, 0, l, m) / 2, cy + rh * 0.38f, 0, l, m, C_TEXT);   // подпись — по центру под формой
        }
    }
    // ---- надписи на виде: что сейчас в кадре (сверху слева), масштаб и знаки ψ (снизу слева)
    if (zoomable) {
        std::string cap;
        const bool proton = massNumber(avZ) > 0 && [&] {   // центральный нуклон — протон?
            const auto& pk = nuc::nucleonPack(A); int c = 0; float best = 1e30f;
            for (int k = 0; k < A; k++) { const float d = pk[k][0] * pk[k][0] + pk[k][1] * pk[k][1] + pk[k][2] * pk[k][2]; if (d < best) { best = d; c = k; } }
            return c < avZ; }();
        if (tq > 0.5f) cap = proton ? T("протон: кварки u, u, d — их держит глюонное поле") : T("нейтрон: кварки u, d, d — их держит глюонное поле");
        else if (RNpx >= 14)
            cap = fmt("ядро %s-%d: %d протонов и %d нейтронов · радиус %.1f фм — в %s раз меньше атома", EL[t].sym, A, avZ, A - avZ, RN,
                      bigRatio(slaterMeanR(atomArt(avZ).sh.back().n, atomArt(avZ).sh.back().Zs) / RN / avz::FM).c_str());
        else if (fs < 0.5f) cap = avMode == 0 ? fmt("весь атом, четверть вырезана · яркость — вероятность встретить электрон оболочки на этом расстоянии (r²|ψ|²) · радиус рисунка %.2f Å", H0 * 0.529177) : "";
        else if (avMode == 0) cap = s < 0.4 * atomArt(avZ).rIn ? T("глубже оболочки 1s облако почти однородно: электрон бывает и у самого ядра") : T("разрез через ядро: кольца — оболочки атома");
        else cap = s < 0.8 * orbInnerR(orbArt(avZ, avN, avL), avN, avL) && avL > 0 ? T("у ядра у этой орбитали узел: электрона в центре не бывает")
                                                                                       : T("разрез через ядро: цвет — знак ψ, светлая линия — граница 85%, тёмные линии — узлы");
        if (!cap.empty()) {
            const float cwid = vs * 0.6f, hgt = drawWrapped(fontXS, vx + uiPx(12), vy + uiPx(10), cwid, cap, {0, 0, 0, 0});
            rectFill(vx + uiPx(6), vy + uiPx(6), cwid + uiPx(12), hgt + uiPx(8), {0, 0, 0, 0.55f});
            drawWrapped(fontXS, vx + uiPx(12), vy + uiPx(10), cwid, cap, C_TEXT);
        }
        const float by = vy + vs - uiPx(14);
        rectFill(vx + uiPx(6), by - uiPx(8) - fontXS.h, uiPx(190), fontXS.h + uiPx(14), {0, 0, 0, 0.45f});   // подложка: разрез бывает светлым
        drawScaleBar(vx + uiPx(14), by, pxB);
        drawText(fontXS, vx + uiPx(14) + uiPx(130), by - fontXS.h, zoomLabel(avZoom), C_TEXT);
        if (avMode == 1 && RNpx < 14) {   // легенда знаков ψ
            MonoAtoms atomsColored; const float lx = vx + uiPx(12), ly = by - uiPx(44);
            rectFill(lx, ly + uiPx(4), uiPx(10), uiPx(10), {PSI_POS[0], PSI_POS[1], PSI_POS[2], 1});
            float xx = lx + uiPx(14) + drawText(fontXS, lx + uiPx(14), ly + uiPx(2), "ψ > 0", C_TEXT) + uiPx(12);
            rectFill(xx, ly + uiPx(4), uiPx(10), uiPx(10), {PSI_NEG[0], PSI_NEG[1], PSI_NEG[2], 1});
            xx += uiPx(14) + drawText(fontXS, xx + uiPx(14), ly + uiPx(2), "ψ < 0", C_TEXT) + uiPx(12);
            drawText(fontXS, xx, ly + uiPx(2), fs < 0.5f ? "оболочка — 85% вероятности" : "светлая линия — граница 85%", C_DIM);
        }
    }
    popClip();
    if (zoomable) {   // кнопки уровней приближения
        const double goals[4] = {1.0,
                                 avMode == 0 ? H0 / (2.4 * atomArt(avZ).rIn) : H0 / (2.5 * orbInnerR(orbArt(avZ, avN, avL), avN, avL)),
                                 H0 / (3.0 * RN * avz::FM), zMax};
        const char* hints[4] = {"Весь атом (орбиталь) целиком", "Ближе к ядру: внутренняя оболочка 1s (у орбитали — её внутренние доли)",
                                "Ядро: протоны и нейтроны", "Внутри протона: кварки"};
        float bx = zr.x;
        for (int k = 0; k < 4; k++) {
            const bool on = std::fabs(std::log(avZoomGoal / clampv(goals[k], 1.0, zMax))) < 0.05;
            if (uiButton(1760 + k, bx, zr.y, zbW[k], zbH, ZL[k], on, false, hints[k])) avZoomGoal = clampv(goals[k], 1.0, zMax);
            bx += zbW[k] + uiPx(4);
        }
    }
    // ---- правая колонка
    const float cx = vx + vs + pad, cw = x0 + W - pad - cx; float yy = vy;
    auto btn = [&](int id, float x, float w, const std::string& label, bool on, const char* hint) { return uiButton(id, x, yy, w, uiPx(26), label, on, false, hint); };
    {
        const float bw = uiPx(34);
        if (btn(1700, cx, bw, "◀", false, "Предыдущий элемент")) avZ = avZ > 1 ? avZ - 1 : 118;
        if (btn(1701, cx + bw + uiPx(4), bw, "▶", false, "Следующий элемент")) avZ = avZ < 118 ? avZ + 1 : 1;
        if (btn(1702, cx + 2 * (bw + uiPx(4)), cw - 2 * (bw + uiPx(4)), "элемент из таблицы (E)", false, "Элемент, выбранный в таблице Менделеева")) avZ = EL[customType].Z;
        yy += uiPx(32);
        const char* MN[3] = {"облако атома", "орбиталь", "формы s p d f"};
        const float mw = (cw - 2 * uiPx(4)) / 3;
        for (int k = 0; k < 3; k++) if (btn(1703 + k, cx + k * (mw + uiPx(4)), mw, MN[k], avMode == k,
                                               k == 0 ? "Все электроны атома: облака оболочек" : k == 1 ? "Одна орбиталь: облако |ψ|², знак ψ и поверхность 85%" : "Формы всех орбиталей s, p, d и f")) avMode = k;
        yy += uiPx(34);
    }
    drawText(fontU, cx, yy, fmt("конфигурация: %s", zElectronConfig(avZ).c_str()), C_TEXT_HI); yy += fontU.h + uiPx(6);
    const AtomArt& Aa = atomArt(avZ);
    // подоболочки: электроны, Z_eff и средний радиус ⟨r⟩ по Слейтеру; выбор для режима «орбиталь». Ширина кнопок — по самой
    // длинной подписи: у тяжёлых атомов «91.7 · 0.9 пм» не влезало в прежние 58 пикселей
    {
        uiSection(cx, yy, cw, "подоболочки: электроны · Z_eff · средний радиус");
        float bw = uiPx(58);
        for (const ShellArt& sh : Aa.sh) bw = std::max(bw, textW(fontXS, fmt("%.1f · %s", sh.Zs, radLabel(0.529177 * slaterMeanR(sh.n, sh.Zs)).c_str())) + uiPx(10));
        const int per = std::max(1, (int)((cw + uiPx(4)) / (bw + uiPx(4))));
        bw = std::floor((cw + uiPx(4)) / per - uiPx(4));   // ровные колонки на всю ширину
        int colN = 0;
        for (const ShellArt& sh : Aa.sh) {
            if (colN == per) { colN = 0; yy += uiPx(44); }
            const float bx = cx + colN * (bw + uiPx(4)); colN++;
            if (uiButton(1710 + sh.n * 4 + sh.l, bx, yy, bw, uiPx(40), "", avMode == 1 && avN == sh.n && avL == sh.l, false, "Показать эту подоболочку в режиме «орбиталь»")) { avMode = 1; avN = sh.n; avL = sh.l; avM = 0; }
            drawOrbName(fontUB, bx + uiPx(5), yy + uiPx(3), sh.n, sh.l, -1, C_TEXT_HI);
            drawTextR(fontXS, bx + bw - uiPx(4), yy + uiPx(4), fmt("%d e", sh.occ), C_DIM);
            drawText(fontXS, bx + uiPx(5), yy + uiPx(22), fmt("%.1f · %s", sh.Zs, radLabel(0.529177 * slaterMeanR(sh.n, sh.Zs)).c_str()), C_DIM);
        }
        yy += uiPx(48);
    }
    if (avMode == 1) {   // орбитали подоболочки: ширина кнопки — по подписи (5f x(x²−3y²) длиннее 5f z³)
        uiSection(cx, yy, cw, "орбиталь (m)");
        float bx = cx;
        for (int m = 0; m < orbCount(avL); m++) {
            const float bw = std::max(uiPx(64), orbNameW(fontU, avN, avL, m) + uiPx(28));
            if (bx + bw > cx + cw + 0.5f) { bx = cx; yy += uiPx(30); }
            const int e = hundOcc(occ[avN][avL], avL, m);
            if (uiButton(1750 + m, bx, yy, bw, uiPx(26), "", avM == m, false, "Электронов на орбитали: 0, 1 или 2 (принцип Паули)")) avM = m;
            drawOrbName(fontU, bx + uiPx(6), yy + uiPx(5), avN, avL, m, C_TEXT_HI);
            drawTextR(fontXS, bx + bw - uiPx(5), yy + uiPx(7), e == 2 ? "↑↓" : e == 1 ? "↑" : "·", C_TEXT);
            bx += bw + uiPx(4);
        }
        yy += uiPx(34);
    }
    // радиальное распределение по логарифму r: вероятность на единицу ln r (r³R²) — у каждой оболочки свой горб, и у тяжёлых
    // атомов внутренние оболочки не сливаются у нуля; подсвечено то, что сейчас в кадре
    {
        uiSection(cx, yy, cw, "где электроны: вероятность на расстоянии r");
        const float ph = uiPx(120); PR in{cx, yy, cw, ph};
        boxPanel(in.x, in.y, in.w, in.h, C_PANEL2, C_LINE);
        double rlo = 1e30, rhi = 0;
        for (const ShellArt& sh : Aa.sh) { const double mr = slaterMeanR(sh.n, sh.Zs); rlo = std::min(rlo, mr); rhi = std::max(rhi, mr); }
        rlo *= 0.2; rhi *= 3.0;
        const double L0 = std::log(rlo), L1 = std::log(rhi);
        auto xOf = [&](double r) { return in.x + in.w * (float)((std::log(r) - L0) / (L1 - L0)); };
        constexpr int NP = 200; std::vector<std::vector<float>> curves; std::vector<int> ls; float mx = 0;
        for (const ShellArt& sh : Aa.sh) {
            std::vector<float> cv(NP);
            for (int k = 0; k < NP; k++) { const double r = std::exp(L0 + (L1 - L0) * (k + 0.5) / NP); cv[k] = (float)(sh.occ * r * rtab::at(sh.rad, r) * sh.pn); mx = std::max(mx, cv[k]); }
            curves.push_back(cv); ls.push_back(sh.l);
        }
        if (zoomable && avZoom > 1.05) { const float xv = clampv(xOf(s), in.x, in.x + in.w); rectFill(in.x, in.y, xv - in.x, in.h, withA(C_ACC, 0.10f)); rectFill(xv, in.y, 1, in.h, withA(C_ACC, 0.7f)); }
        MonoAtoms atomsColored;
        glEnable(GL_LINE_SMOOTH); glLineWidth(1.5f);
        for (size_t q = 0; q < curves.size(); q++) {
            float r, g, b; orbColor(ls[q], false, r, g, b); glColor4f(r, g, b, 0.9f);
            glBegin(GL_LINE_STRIP); for (int k = 0; k < NP; k++) glVertex2f(in.x + in.w * (k + 0.5f) / NP, in.y + in.h - uiPx(4) - (in.h - uiPx(8)) * curves[q][k] / std::max(1e-9f, mx)); glEnd();
        }
        glLineWidth(1); glDisable(GL_LINE_SMOOTH);
        for (int e = -5; e <= 2; e++) {   // деления — степени десяти
            const double rA = std::pow(10.0, e), r = rA / 0.529177; if (r < rlo || r > rhi) continue;
            const float xx = xOf(r); rectFill(xx, in.y + in.h - uiPx(4), 1, uiPx(4), C_DIM);
            drawTextC(fontXS, xx, in.y + in.h + uiPx(2), lenLabel(rA * 1e-10), C_DIM);
        }
        yy += ph + fontXS.h + uiPx(8);
        float lx = cx; const char* LN[4] = {"s", "p", "d", "f"};
        for (int l = 0; l < 4; l++) { float r, g, b; orbColor(l, false, r, g, b); rectFill(lx, yy + uiPx(4), uiPx(10), uiPx(3), {r, g, b, 1}); drawText(fontXS, lx + uiPx(14), yy, LN[l], C_TEXT); lx += uiPx(38); }
        drawTextR(fontXS, cx + cw, yy, "шкала r — логарифмическая", C_DIM);
        yy += fontXS.h + uiPx(8);
    }
    const char* note = nullptr;
    if (zoomable && fs > 0.5f) {
        if (tq > 0.5f) note = "Протон и нейтрон — не точки: в каждом три кварка (у протона u, u, d, у нейтрона u, d, d), их держит глюонное поле. Цвет кварка — его «цветовой заряд», вместе три цвета дают «белый» нуклон. Вырвать кварк нельзя: струна поля рвётся и рождает новую пару (сцены «Кварки» в меню).";
        else if (RNpx >= 14) note = "Ядро в десятки тысяч раз меньше атома, но в нём почти вся масса. Протоны (красные) и нейтроны (серые) держит сильное взаимодействие; плоскость разреза снимает ближнюю половину ядра. Колесо — ещё ближе: внутри нуклона кварки.";
        else if (avMode == 0 && s < 0.4 * Aa.rIn) note = "Глубже оболочки 1s облако почти однородно: у s-электронов плотность у самого ядра наибольшая, они бывают даже внутри ядра (поэтому ядро может захватить электрон). Само ядро пока — точка: оно ещё в тысячи раз меньше.";
        else if (avMode == 0) note = "Разрез через ядро: каждая оболочка — кольцо, где электрон этой оболочки встречается чаще всего. Чем ближе к ядру, тем оболочки теснее и ярче: внутренние электроны видят почти весь заряд ядра. Колесо мыши — ближе к ядру.";
        else note = "Разрез орбитали через ядро: цвет — знак ψ, яркость — |ψ|², светлая линия — граница, внутри которой электрон бывает в 85% случаев. Тёмные круги — узлы радиальной части, тёмные прямые и конусы — угловой. У s-орбитали плотность у ядра наибольшая, у p, d и f в центре узел.";
    }
    if (!note) note = avMode == 2
        ? "Это угловые части орбиталей: расстояние от центра до поверхности пропорционально |Y|. Цвет долей — знак волновой функции: при перекрытии долей одного знака возникает связь. Число форм растёт как 2l + 1: одна s, три p, пять d, семь f."
        : avMode == 1
        ? "Одна орбиталь: облако светится там, где |ψ|² велико, цвет — знак ψ. Глянцевая оболочка ограничивает место, где электрон бывает в 85% случаев. Тёмные провалы между долями и оболочками — узлы волны: там электрона не бывает никогда."
        : "Электрон не летает по орбите: это стоячая волна, и облако показывает, где его можно найти. Каждая оболочка светится своим цветом (s, p, d, f), тёмные слои между ними — узлы волны. Внутренние электроны экранируют ядро, поэтому внешние видят заряд Z_eff меньше Z и держатся дальше.";
    drawWrapped(fontXS, cx, yy, cw, T(note), C_DIM);
    if (ui.pressed && !inPR(avRect)) atomViewOn = false;
}

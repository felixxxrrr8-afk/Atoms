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

// ---- действительные сферические гармоники на единичной сфере (без нормировки: для формы и выборки она не нужна)
static int orbCount(int l) { return 2 * l + 1; }
static double realY(int l, int m, double x, double y, double z) {
    switch (l) {
    case 0: return 1;
    case 1: return m == 0 ? x : m == 1 ? y : z;
    case 2: switch (m) { case 0: return x * y; case 1: return x * z; case 2: return y * z; case 3: return (3 * z * z - 1) / 2.0 / std::sqrt(3.0); default: return (x * x - y * y) / 2; }
    default: switch (m) {
        case 0: return z * (5 * z * z - 3) * 0.2; case 1: return x * (5 * z * z - 1) * 0.25; case 2: return y * (5 * z * z - 1) * 0.25; case 3: return x * y * z * 2.5;
        case 4: return z * (x * x - y * y) * 1.2; case 5: return x * (x * x - 3 * y * y) * 0.4; default: return y * (3 * x * x - y * y) * 0.4; }
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
    int occ[8][4]; zOccupancy(Z, occ); if (occ[n][l] == 0) occ[n][l] = 1;
    const double Zf = slaterZeff(Z, occ, n, l);
    G = VolGrid(); G.N = N; G.half = (float)(1.06 * radialQuantile(n, l, Zf, 0.995)); G.a.assign((size_t)N * N * N, 0.0f);
    const double h = 2.0 * G.half / (N - 1);
#pragma omp parallel for schedule(static)
    for (int z = 0; z < N; z++) for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        const double X = -G.half + x * h, Y = -G.half + y * h, Zc = -G.half + z * h, r = std::sqrt(X * X + Y * Y + Zc * Zc);
        const double ang = r > 1e-9 ? realY(l, m, X / r, Zc / r, Y / r) : (l == 0 ? 1.0 : 0.0);
        G.a[((size_t)z * N + y) * N + x] = (float)(radialR(n, l, Zf, r) * ang);
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
    int occ[8][4]; zOccupancy(Z, occ);
    struct Sh { int n, l; double Zf, peak; float c[3]; float w; std::vector<std::pair<int, int>> ms; };
    std::vector<Sh> sh; int nmax = 1;
    for (int n = 1; n <= 7; n++) for (int l = 0; l < 4 && l < n; l++) if (occ[n][l] > 0) nmax = std::max(nmax, n);
    double half = 0.3;
    for (int n = 1; n <= 7; n++) for (int l = 0; l < 4 && l < n; l++) {
        const int e = occ[n][l]; if (e <= 0) continue;
        Sh s; s.n = n; s.l = l; s.Zf = slaterZeff(Z, occ, n, l); s.w = n == nmax ? 0.8f : (n == nmax - 1 ? 0.9f : 0.7f);
        orbColor(l, false, s.c[0], s.c[1], s.c[2]);
        for (int m = 0; m < orbCount(l); m++) { const int k = hundOcc(e, l, m); if (k) s.ms.push_back({m, k}); }
        // наибольшая яркость подоболочки: максимум r²R² по радиусу × максимум Σ occ·Y² по направлениям
        double rmx = 0; const double rq = radialQuantile(n, l, s.Zf, 0.999);
        for (int k = 1; k <= 600; k++) { const double r = rq * k / 600, R = radialR(n, l, s.Zf, r); rmx = std::max(rmx, r * r * R * R); }
        double amx = 0; std::mt19937 rg(11 + n * 7 + l);
        for (int k = 0; k < 1500; k++) {
            double v[3] = {std::normal_distribution<double>()(rg), std::normal_distribution<double>()(rg), std::normal_distribution<double>()(rg)};
            const double lv = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) + 1e-12, x = v[0] / lv, y = v[1] / lv, z = v[2] / lv;
            double sa = 0; for (auto& mk : s.ms) { const double Y = realY(l, mk.first, x, y, z); sa += mk.second * Y * Y; }
            amx = std::max(amx, sa);
        }
        s.peak = std::max(1e-300, rmx * amx);
        if (n == nmax) half = std::max(half, 1.08 * radialQuantile(n, l, s.Zf, 0.85));
        sh.push_back(s);
    }
    G = VolGrid(); G.N = N; G.half = (float)half; G.atom = true;
    const size_t NN = (size_t)N * N * N; G.q.assign(NN * 4, 0.0f);
    const double h = 2.0 * half / (N - 1);
#pragma omp parallel for schedule(dynamic, 2)
    for (int z = 0; z < N; z++) for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        const double X = -half + x * h, Y = -half + y * h, Zc = -half + z * h, r = std::sqrt(X * X + Y * Y + Zc * Zc);
        const double ux = r > 1e-9 ? X / r : 0, uy = r > 1e-9 ? Zc / r : 0, uz = r > 1e-9 ? Y / r : 1;
        double I = 0, cr = 0, cg = 0, cb = 0;
        for (const Sh& s : sh) {
            const double R = radialR(s.n, s.l, s.Zf, r); double sa = 0;
            for (auto& mk : s.ms) { const double Yv = realY(s.l, mk.first, ux, uy, uz); sa += mk.second * Yv * Yv; }
            const double v = s.w * std::pow(std::min(1.0, r * r * R * R * sa / s.peak), 1.6);
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
// Луч идёт от зрителя вглубь; на каждом шаге облако добавляет свечение и поглощает свет из-за себя
static void volRender(const VolGrid& G, int w, float yaw, float pitch, std::vector<unsigned char>& img, bool cutaway = true) {
    img.assign((size_t)w * w * 4, 0);
    if (G.N < 2) return;
    const float c1 = std::cos(yaw), s1 = std::sin(yaw), c2 = std::cos(pitch), s2 = std::sin(pitch), H = G.half;
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
        const float X = ((px + 0.5f) / w * 2 - 1) * H, Y = -((py + 0.5f) / w * 2 - 1) * H, q = H * H - X * X - Y * Y;
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
                    if (nl > 1e-12f) {
                        nx /= nl; ny /= nl; nz /= nl; float vx, vy, vz; toView(nx, ny, nz, vx, vy, vz);
                        const float dif = std::max(0.0f, vx * lx + vy * ly + vz * lz), sp = std::pow(std::max(0.0f, (vx * hx + vy * hy + vz * hz) / hl), 40.0f);
                        // у края (луч идёт вскользь) оболочка прозрачнее: контур мягкий, без лесенки пикселей
                        const float edge = clampv((std::fabs(vz) - 0.04f) / 0.30f, 0.0f, 1.0f), fe = edge * edge * (3 - 2 * edge);
                        const float rim = std::pow(1 - std::fabs(vz), 3.0f), as = (hits == 0 ? 0.62f : 0.4f) * (0.15f + 0.85f * fe);
                        const float sh = 0.16f + 0.84f * dif;
                        Cr += T * as * (c[0] * sh + 0.75f * sp + 0.30f * rim * c[0]);
                        Cg += T * as * (c[1] * sh + 0.75f * sp + 0.30f * rim * c[1]);
                        Cb += T * as * (c[2] * sh + 0.75f * sp + 0.30f * rim * c[2]);
                        T *= 1 - as;
                    }
                    hits++;
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
// в картинку img (ширина iw) — клетка с левым верхним углом (ox, oy) и стороной w
static void shapeRender(int l, int m, float yaw, float pitch, std::vector<unsigned char>& img, int iw, int ox, int oy, int w) {
    const float c1 = std::cos(yaw), s1 = std::sin(yaw), c2 = std::cos(pitch), s2 = std::sin(pitch), inv = 1 / shapeYmax(l, m), V = 1.06f;
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
            float r, g, b; orbColor(l, Yc < 0, r, g, b);
            const float dif = std::max(0.0f, vx * lx + vy * ly + vz * lz), sp = std::pow(std::max(0.0f, (vx * hx + vy * hy + vz * hz) / hl), 36.0f);
            const float rim = std::pow(1 - std::fabs(vz), 2.5f), rr = std::sqrt(x * x + y * y + z * z);
            const float ao = 0.55f + 0.45f * std::min(1.0f, rr / 0.35f);   // там, где доли сходятся у центра, — тень
            const float sh = (0.20f + 0.80f * dif) * ao;
            cr = r * sh + 0.65f * sp + 0.22f * rim; cg = g * sh + 0.65f * sp + 0.22f * rim; cb = b * sh + 0.65f * sp + 0.22f * rim; al = 1;
        } else if (fmin < 1.5f * pix) {   // промах рядом с краем: полупрозрачная кромка
            al = clampv(1 - fmin / (1.5f * pix), 0.0f, 1.0f) * 0.8f; float r, g, b; orbColor(l, Yv < 0, r, g, b);
            cr = 0.35f * r * al; cg = 0.35f * g * al; cb = 0.35f * b * al;
        }
        if (al <= 0) continue;
        unsigned char* o = &img[((size_t)iy * iw + ix) * 4];
        o[0] = (unsigned char)(255 * std::min(1.0f, cr)); o[1] = (unsigned char)(255 * std::min(1.0f, cg));
        o[2] = (unsigned char)(255 * std::min(1.0f, cb)); o[3] = (unsigned char)(255 * std::min(1.0f, al));
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
// рисунок из места slot — прямоугольником (x, y, w, h), с предумноженной альфой
static void drawSlot(int slot, float x, float y, float w, float h) {
    if (slot < 0 || slot >= 8 || !slotTex[slot]) return;
    MonoAtoms atomsColored;
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, slotTex[slot]); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); glColor4f(1, 1, 1, 1);
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
// оси x, y, z (серые) — как на рисунках орбиталей
static void drawAxes3(const View3& v, float len) {
    const char* nm[3] = {"x", "z", "y"}; const float dir[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    glEnable(GL_LINE_SMOOTH);
    for (int k = 0; k < 3; k++) {
        float X0, Y0, D0, X1, Y1, D1; v3(v, -dir[k][0] * len, -dir[k][1] * len, -dir[k][2] * len, X0, Y0, D0); v3(v, dir[k][0] * len, dir[k][1] * len, dir[k][2] * len, X1, Y1, D1);
        col(withA(C_DIM, 0.55f)); glBegin(GL_LINES); arrowPx(X0, Y0, X1, Y1, uiPx(6)); glEnd();
        drawText(fontXS, X1 + uiPx(3), Y1 - fontXS.h, nm[k], C_DIM);
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

// ---- окно «Строение атома» (F7)
static int avMode = 0, avN = 2, avL = 1, avM = 0;   // режим: 0 облако атома, 1 одна орбиталь, 2 все формы (элемент — avZ)
static float avYaw = 0.6f, avPitch = 0.38f; static bool avDrag = false; static float avMx = 0, avMy = 0; static double avClock = 0;
static PR avRect;
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
    drawTextR(fontXS, x0 + W - pad, y0 + uiPx(17), "мышь — повернуть · Esc / F7 — закрыть", C_DIM);
    // слева — вид, справа — управление и пояснения
    const float vs = std::min(H - headH - pad, W * 0.58f), vx = x0 + pad, vy = y0 + headH;
    boxPanel(vx, vy, vs, vs, hexc(0x030303), C_LINE);
    const bool hov = ui.mx >= vx && ui.mx < vx + vs && ui.my >= vy && ui.my < vy + vs;
    if (hov && ui.pressed) { avDrag = true; avMx = (float)ui.mx; avMy = (float)ui.my; }
    if (!ui.down) avDrag = false;
    if (avDrag) { avYaw += (ui.mx - avMx) * 0.01f; avPitch = clampv(avPitch + (ui.my - avMy) * 0.01f, -1.4f, 1.4f); avMx = (float)ui.mx; avMy = (float)ui.my; }
    else avYaw += (float)frameDt * 0.25f;
    avClock += frameDt;
    int occ[8][4]; zOccupancy(avZ, occ);
    if (occ[avN][avL] == 0 && avMode == 1) {   // выбранной подоболочки у атома нет — внешняя занятая
        for (int n = 7; n >= 1; n--) { bool f = false; for (int l = 3; l >= 0; l--) if (occ[n][l]) { avN = n; avL = l; f = true; break; } if (f) break; }
        avM = 0;
    }
    pushClip(vx, vy, vs, vs);
    static std::vector<unsigned char> img, img2;
    const int res = clampv((int)(vs * 0.45f), 160, 320);   // лучей вдвое реже, чем пикселей: облако мягкое, текстура растягивается
    // картинка считается заново, только если что-то изменилось или вид повернулся заметно (автоповорот — через кадр)
    static int lastKey[7] = {-1}; static float lastYaw = 1e9f, lastPitch = 1e9f;
    const int key[7] = {avMode, avZ, avN, avL, avM, res, (int)vs};
    const float turn = avDrag ? 0.003f : 0.012f;   // вращение мышью — сразу, медленный автоповорот — ступеньками меньше градуса
    const bool fresh = memcmp(key, lastKey, sizeof(key)) != 0 || std::fabs(avYaw - lastYaw) > turn || std::fabs(avPitch - lastPitch) > turn;
    if (fresh) { memcpy(lastKey, key, sizeof(key)); lastYaw = avYaw; lastPitch = avPitch; }
    if (avMode == 0) {
        const VolGrid& G = atomGrid(avZ, 64);
        if (fresh) { volRender(G, res, avYaw, avPitch, img); imgToTex(1, img, res, res); }
        const float m = vs * 0.04f; drawSlot(1, vx + m, vy + m, vs - 2 * m, vs - 2 * m);
        drawNucleusDot({vx + vs / 2, vy + vs / 2, 1, 0, 0}, uiPx(2.2f));
        drawText(fontXS, vx + uiPx(8), vy + vs - fontXS.h - uiPx(6), fmt("четверть вырезана · яркость — вероятность встретить электрон оболочки на этом расстоянии (r²|ψ|²) · радиус %.2f Å", G.half * 0.529), C_DIM);
    } else if (avMode == 1) {
        const VolGrid& G = orbitalGrid(avZ, avN, avL, avM, 80);
        if (fresh) { volRender(G, res, avYaw, avPitch, img); imgToTex(1, img, res, res); }
        const float m = vs * 0.04f; drawSlot(1, vx + m, vy + m, vs - 2 * m, vs - 2 * m);
        View3 v{vx + vs / 2, vy + vs / 2, (vs / 2 - m), avYaw, avPitch};
        drawAxes3(v, 1.0f);
        drawNucleusDot(v, uiPx(2.2f));
        // угловая форма — в углу
        const int ws = (int)uiPx(130);
        if (fresh) { img2.assign((size_t)ws * ws * 4, 0); shapeRender(avL, avM, avYaw, avPitch, img2, ws, 0, 0, ws); imgToTex(2, img2, ws, ws); }
        drawSlot(2, vx + vs - ws - uiPx(10), vy + uiPx(10), (float)ws, (float)ws);
        drawTextR(fontXS, vx + vs - uiPx(12), vy + ws + uiPx(12), "угловая часть |Y|", C_DIM);
        {   // легенда знаков ψ
            MonoAtoms atomsColored; const float lx = vx + uiPx(10), ly = vy + vs - uiPx(26);
            rectFill(lx, ly + uiPx(4), uiPx(10), uiPx(10), {PSI_POS[0], PSI_POS[1], PSI_POS[2], 1});
            float xx = lx + uiPx(14) + drawText(fontXS, lx + uiPx(14), ly + uiPx(2), "ψ > 0", C_TEXT) + uiPx(12);
            rectFill(xx, ly + uiPx(4), uiPx(10), uiPx(10), {PSI_NEG[0], PSI_NEG[1], PSI_NEG[2], 1});
            xx += uiPx(14) + drawText(fontXS, xx + uiPx(14), ly + uiPx(2), "ψ < 0", C_TEXT) + uiPx(12);
            drawText(fontXS, xx, ly + uiPx(2), "оболочка — 85% вероятности", C_DIM);
        }
    } else {   // все формы s, p, d, f — как таблица в учебнике
        const int iw = (int)vs; const int rows = 4; const float rh = vs / rows;
        if (fresh) {
            img.assign((size_t)iw * iw * 4, 0);
            for (int l = 0; l < 4; l++) for (int m = 0; m < orbCount(l); m++) {
                const float cw = vs / 7, cx = vs / 2 + (m - (orbCount(l) - 1) / 2.0f) * cw, cy = rh * l + rh * 0.44f;
                const int w = (int)(std::min(cw, rh) * 0.86f);
                shapeRender(l, m, avYaw, avPitch, img, iw, (int)(cx - w / 2), (int)(cy - w / 2), w);
            }
            imgToTex(1, img, iw, iw);
        }
        drawSlot(1, vx, vy, (float)iw, (float)iw);
        for (int l = 0; l < 4; l++) for (int m = 0; m < orbCount(l); m++) {
            const float cw = vs / 7, cx = vx + vs / 2 + (m - (orbCount(l) - 1) / 2.0f) * cw, cy = vy + rh * l + rh * 0.44f;
            const float nw = uiPx(40); drawOrbName(fontU, cx - nw / 2, cy + rh * 0.38f, 0, l, m, C_TEXT);
        }
    }
    popClip();
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
    // подоболочки: электроны, Z_eff, средний радиус ⟨r⟩ = a0·(3n² − l(l+1))/(2Z_eff); выбор для режима «орбиталь»
    {
        uiSection(cx, yy, cw, "подоболочки: электроны · Z_eff · средний радиус");
        float bx = cx; const float bw = uiPx(58);
        for (int n = 1; n <= 7; n++) for (int l = 0; l < 4 && l < n; l++) {
            if (!occ[n][l]) continue;
            const double Zf = slaterZeff(avZ, occ, n, l), rA = 0.529 * (3.0 * n * n - l * (l + 1)) / (2 * Zf);
            if (bx + bw > cx + cw) { bx = cx; yy += uiPx(44); }
            const int id = 1710 + n * 4 + l;
            if (uiButton(id, bx, yy, bw, uiPx(40), "", avMode == 1 && avN == n && avL == l, false, "Показать эту подоболочку в режиме «орбиталь»")) { avMode = 1; avN = n; avL = l; avM = 0; }
            drawOrbName(fontUB, bx + uiPx(5), yy + uiPx(3), n, l, -1, C_TEXT_HI);
            drawTextR(fontXS, bx + bw - uiPx(4), yy + uiPx(4), fmt("%d e", occ[n][l]), C_DIM);
            drawText(fontXS, bx + uiPx(5), yy + uiPx(22), fmt("%.1f · %.2fÅ", Zf, rA), C_DIM);
            bx += bw + uiPx(4);
        }
        yy += uiPx(48);
    }
    if (avMode == 1) {
        uiSection(cx, yy, cw, "орбиталь (m)");
        float bx = cx; const float bw = uiPx(64);
        for (int m = 0; m < orbCount(avL); m++) {
            if (bx + bw > cx + cw) { bx = cx; yy += uiPx(30); }
            const int e = hundOcc(occ[avN][avL], avL, m);
            if (uiButton(1750 + m, bx, yy, bw, uiPx(26), "", avM == m, false, "Электронов на орбитали: 0, 1 или 2 (принцип Паули)")) avM = m;
            drawOrbName(fontU, bx + uiPx(6), yy + uiPx(5), avN, avL, m, C_TEXT_HI);
            drawTextR(fontXS, bx + bw - uiPx(4), yy + uiPx(7), e == 2 ? "↑↓" : e == 1 ? "↑" : "·", C_TEXT);
            bx += bw + uiPx(4);
        }
        yy += uiPx(34);
    }
    // радиальное распределение: вероятность найти электрон на расстоянии r от ядра, r²R²
    {
        uiSection(cx, yy, cw, "где электроны: вероятность на расстоянии r");
        const float ph = uiPx(120); PR in{cx, yy, cw, ph};
        boxPanel(in.x, in.y, in.w, in.h, C_PANEL2, C_LINE);
        double rmaxA = 0; for (int n = 1; n <= 7; n++) for (int l = 0; l < 4; l++) if (occ[n][l]) rmaxA = std::max(rmaxA, 0.529 * (3.0 * n * n + 4) / slaterZeff(avZ, occ, n, l));
        rmaxA = std::min(rmaxA * 1.3, 6.0);
        std::vector<std::vector<float>> curves; std::vector<int> ls; float mx = 0;
        for (int n = 1; n <= 7; n++) for (int l = 0; l < 4; l++) if (occ[n][l]) {
            const double Zf = slaterZeff(avZ, occ, n, l); std::vector<float> cv(160);
            double norm = 0; for (int k = 0; k < 400; k++) { const double r = (k + 0.5) / 400 * 40 / Zf * n; const double R = radialR(n, l, Zf, r); norm += r * r * R * R * (40.0 / Zf * n / 400); }
            for (int k = 0; k < 160; k++) { const double r = rmaxA / 0.529 * (k + 0.5) / 160, R = radialR(n, l, Zf, r); cv[k] = (float)(occ[n][l] * r * r * R * R / std::max(1e-30, norm)); mx = std::max(mx, cv[k]); }
            curves.push_back(cv); ls.push_back(l);
        }
        MonoAtoms atomsColored;
        glEnable(GL_LINE_SMOOTH); glLineWidth(1.5f);
        for (size_t q = 0; q < curves.size(); q++) {
            float r, g, b; orbColor(ls[q], false, r, g, b); glColor4f(r, g, b, 0.9f);
            glBegin(GL_LINE_STRIP); for (int k = 0; k < 160; k++) glVertex2f(in.x + in.w * k / 159.0f, in.y + in.h - uiPx(4) - (in.h - uiPx(8)) * curves[q][k] / std::max(1e-9f, mx)); glEnd();
        }
        glLineWidth(1); glDisable(GL_LINE_SMOOTH);
        drawText(fontXS, in.x + uiPx(4), in.y + in.h + uiPx(2), "0", C_DIM); drawTextR(fontXS, in.x + in.w, in.y + in.h + uiPx(2), fmt("%.1f Å", rmaxA), C_DIM);
        yy += ph + fontXS.h + uiPx(8);
        float lx = cx; const char* LN[4] = {"s", "p", "d", "f"};
        for (int l = 0; l < 4; l++) { float r, g, b; orbColor(l, false, r, g, b); rectFill(lx, yy + uiPx(4), uiPx(10), uiPx(3), {r, g, b, 1}); drawText(fontXS, lx + uiPx(14), yy, LN[l], C_TEXT); lx += uiPx(38); }
        yy += fontXS.h + uiPx(8);
    }
    const char* note = avMode == 2
        ? "Это угловые части орбиталей: расстояние от центра до поверхности пропорционально |Y|. Цвет долей — знак волновой функции: при перекрытии долей одного знака возникает связь. Число форм растёт как 2l + 1: одна s, три p, пять d, семь f."
        : avMode == 1
        ? "Одна орбиталь: облако светится там, где |ψ|² велико, цвет — знак ψ. Глянцевая оболочка ограничивает место, где электрон бывает в 85% случаев. Тёмные провалы между долями и оболочками — узлы волны: там электрона не бывает никогда."
        : "Электрон не летает по орбите: это стоячая волна, и облако показывает, где его можно найти. Каждая оболочка светится своим цветом (s, p, d, f), тёмные слои между ними — узлы волны. Внутренние электроны экранируют ядро, поэтому внешние видят заряд Z_eff меньше Z и держатся дальше.";
    drawWrapped(fontXS, cx, yy, cw, T(note), C_DIM);
    if (ui.pressed && !inPR(avRect)) atomViewOn = false;
}

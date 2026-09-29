// ===================================== СТРОЕНИЕ АТОМА: ЭЛЕКТРОННЫЕ ОБЛАКА ===================
// Электрон в атоме — не шарик на орбите, а стоячая волна ψ; вероятность найти его в точке — |ψ|². Для электрона
// в поле ядра уравнение Шрёдингера решается точно: ψ = R_nl(r)·Y_lm(θ,φ). Радиальная часть имеет n − l − 1 узлов
// (оболочки), угловая задаёт форму: s — шар, p — «гантели», d — «четырёхлистники», f — ещё сложнее.
// В многоэлектронном атоме внутренние электроны экранируют ядро, и электрон «видит» эффективный заряд Z_eff —
// по правилам Слейтера (1930). Заполнение подоболочек — по конфигурации основного состояния, внутри подоболочки —
// по правилу Хунда (сначала по одному электрону в каждую орбиталь). Облако рисуется методом Монте-Карло:
// точки ставятся случайно с плотностью |ψ|², поэтому плотные места облака — там, где электрон бывает чаще.
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

// ---- облако точек
struct OrbCloud { std::vector<float> p; std::vector<unsigned char> tag; float R = 1; int nmax = 1; };   // tag: n·8 + l·2 + (ψ < 0)
static std::mt19937 orbRng(7);
static inline double orbU() { return std::uniform_real_distribution<double>(0.0, 1.0)(orbRng); }
// точки одной орбитали (n, l, m) при заряде Z: r — по радиальной плотности r²R², направление — по |Y|² (отбор)
static void sampleOrbital(OrbCloud& c, int n, int l, int m, double Z, int count) {
    const double rmax = (6.0 * n * n + 6) / Z; const int NR = 1200;
    std::vector<double> cdf(NR + 1, 0.0);
    for (int k = 1; k <= NR; k++) { const double r = rmax * (k - 0.5) / NR, R = radialR(n, l, Z, r); cdf[k] = cdf[k - 1] + r * r * R * R; }
    if (cdf[NR] <= 0) return;
    double ymax = 0;
    for (int k = 0; k < 800; k++) { double v[3] = {orbU() * 2 - 1, orbU() * 2 - 1, orbU() * 2 - 1}, l2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
        if (l2 < 1e-6 || l2 > 1) continue; l2 = std::sqrt(l2); const double y = realY(l, m, v[0] / l2, v[1] / l2, v[2] / l2); ymax = std::max(ymax, y * y); }
    ymax *= 1.15; if (ymax <= 0) ymax = 1;
    for (int s = 0; s < count; s++) {
        const double u = orbU() * cdf[NR]; const int k = (int)(std::lower_bound(cdf.begin(), cdf.end(), u) - cdf.begin());
        const double r = rmax * (std::max(0, k - 1) + orbU()) / NR;
        double d[3], y = 0;
        for (int tr = 0; tr < 200; tr++) {
            double v[3] = {orbU() * 2 - 1, orbU() * 2 - 1, orbU() * 2 - 1}, l2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
            if (l2 < 1e-6 || l2 > 1) continue; l2 = std::sqrt(l2);
            for (int q = 0; q < 3; q++) d[q] = v[q] / l2;
            y = realY(l, m, d[0], d[1], d[2]);
            if (orbU() * ymax <= y * y) break;
        }
        const double R = radialR(n, l, Z, r), sgn = y * R;
        // ось z орбитали — вертикаль рисунка (вторая координата вида), как у «лепестков»
        c.p.push_back((float)(r * d[0])); c.p.push_back((float)(r * d[2])); c.p.push_back((float)(r * d[1]));
        c.tag.push_back((unsigned char)(n * 8 + l * 2 + (sgn < 0 ? 1 : 0)));
    }
    c.nmax = std::max(c.nmax, n);
}
// радиус, внутри которого 95% точек
static void cloudRadius(OrbCloud& c) {
    std::vector<float> r; for (size_t k = 0; k + 2 < c.p.size(); k += 3) r.push_back(std::sqrt(c.p[k] * c.p[k] + c.p[k + 1] * c.p[k + 1] + c.p[k + 2] * c.p[k + 2]));
    if (r.empty()) { c.R = 1; return; }
    std::nth_element(r.begin(), r.begin() + (long)(r.size() * 0.95), r.end()); c.R = std::max(0.3f, r[(size_t)(r.size() * 0.95)]);
}
// облако всего атома Z: все занятые орбитали. Внутренние оболочки сжаты у ядра, и при честном числе точек
// (пропорционально электронам) они слились бы в одно яркое пятно — поэтому точек у внешних оболочек больше
// (вес √e·n²), а внутренние рисуются бледнее; форма каждой оболочки при этом настоящая
static const OrbCloud& atomCloud(int Z, int total) {
    static std::map<std::pair<int, int>, OrbCloud> cache;
    auto key = std::make_pair(Z, total); auto it = cache.find(key); if (it != cache.end()) return it->second;
    OrbCloud& c = cache[key]; orbRng.seed(1000 + Z);
    int occ[8][4]; zOccupancy(Z, occ);
    double wsum = 0;
    for (int n = 1; n <= 7; n++) for (int l = 0; l < 4 && l < n; l++) for (int m = 0; m < orbCount(l); m++) { const int k = hundOcc(occ[n][l], l, m); if (k) wsum += std::sqrt((double)k) * n * n; }
    for (int n = 1; n <= 7; n++) for (int l = 0; l < 4 && l < n; l++) {
        const int e = occ[n][l]; if (e <= 0) continue;
        const double Zf = slaterZeff(Z, occ, n, l);
        for (int m = 0; m < orbCount(l); m++) { const int k = hundOcc(e, l, m); if (k) sampleOrbital(c, n, l, m, Zf, std::max(8, (int)(total * std::sqrt((double)k) * n * n / wsum))); }
    }
    // радиус рисунка — по внешней оболочке
    std::vector<float> r;
    for (size_t q = 0, t = 0; q + 2 < c.p.size(); q += 3, t++) if ((c.tag[t] >> 3) == c.nmax) r.push_back(std::sqrt(c.p[q] * c.p[q] + c.p[q + 1] * c.p[q + 1] + c.p[q + 2] * c.p[q + 2]));
    if (!r.empty()) { std::nth_element(r.begin(), r.begin() + (long)(r.size() * 0.92), r.end()); c.R = std::max(0.3f, r[(size_t)(r.size() * 0.92)]); } else cloudRadius(c);
    return c;
}
static const OrbCloud& orbitalCloud(int Z, int n, int l, int m, int total) {
    static std::map<std::array<int, 5>, OrbCloud> cache;
    std::array<int, 5> key{Z, n, l, m, total}; auto it = cache.find(key); if (it != cache.end()) return it->second;
    OrbCloud& c = cache[key]; orbRng.seed(7 + Z * 97 + n * 13 + l * 5 + m);
    int occ[8][4]; zOccupancy(Z, occ); if (occ[n][l] == 0) occ[n][l] = 1;
    sampleOrbital(c, n, l, m, slaterZeff(Z, occ, n, l), total);
    cloudRadius(c);
    return c;
}

// ---- вид: поворот вокруг вертикали (рыскание) и наклон; экранные координаты
struct View3 { float cx, cy, s, yaw, pitch; };
static inline void v3(const View3& v, float x, float y, float z, float& X, float& Y, float& D) {
    const float c1 = std::cos(v.yaw), s1 = std::sin(v.yaw), c2 = std::cos(v.pitch), s2 = std::sin(v.pitch);
    const float x1 = c1 * x + s1 * z, z1 = -s1 * x + c1 * z, y2 = c2 * y - s2 * z1, z2 = s2 * y + c2 * z1;
    X = v.cx + x1 * v.s; Y = v.cy - y2 * v.s; D = z2;
}
// цвета орбиталей как на школьных рисунках: s — красные, p — жёлто-оранжевые, d — синие, f — зелёные;
// у p, d, f две доли разного знака ψ — два оттенка
static void orbColor(int l, bool neg, float& r, float& g, float& b) {
    static const float C[4][2][3] = {{{1.00f, 0.36f, 0.36f}, {1.00f, 0.36f, 0.36f}}, {{1.00f, 0.86f, 0.30f}, {1.00f, 0.58f, 0.20f}},
                                     {{0.36f, 0.44f, 1.00f}, {0.45f, 0.82f, 1.00f}}, {{0.30f, 0.86f, 0.34f}, {0.58f, 0.78f, 0.66f}}};
    const float* c = C[clampv(l, 0, 3)][neg ? 1 : 0]; r = c[0]; g = c[1]; b = c[2];
}
// облако: точки — мягкие светящиеся пятнышки (складываются), плотные места ярче
static void drawCloud(const OrbCloud& c, const View3& v, float size, float alpha, bool bySign) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    MonoAtoms atomsColored;
    const float k = v.s / c.R;
    View3 w = v; w.s = k;
    for (size_t q = 0, t = 0; q + 2 < c.p.size(); q += 3, t++) {
        float X, Y, D; v3(w, c.p[q], c.p[q + 1], c.p[q + 2], X, Y, D);
        const int n = c.tag[t] >> 3, l = (c.tag[t] >> 1) & 3; float r, g, b; orbColor(l, bySign && (c.tag[t] & 1), r, g, b);
        const float inner = n < c.nmax ? 0.45f : 1.0f;   // внутренние оболочки бледнее
        quadUV(X, Y, size * (1.0f - 0.18f * D / c.R), r, g, b, alpha * inner * (1.0f - 0.25f * D / c.R));
    }
    flushQuads(texGlow);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
// ядро — крошечная точка в центре (в масштабе рисунка оно было бы в 10⁴–10⁵ раз меньше облака)
static void drawNucleusDot(const View3& v, float R) {
    MonoAtoms atomsColored;
    glColor4f(1.0f, 0.95f, 0.9f, 0.95f); discPx(v.cx, v.cy, R, 16);
}
// «лепестки» орбитали — поверхность r(θ,φ) = |Y_lm|, доли разного знака разного цвета (как на рисунках в учебниках)
static void drawLobes(int l, int m, const View3& v, float alpha) {
    const int NT = 36, NP = 64;
    static std::map<std::pair<int, int>, std::vector<float>> meshes;   // x y z r (r со знаком)
    auto key = std::make_pair(l, m);
    auto it = meshes.find(key);
    if (it == meshes.end()) {
        std::vector<float> M((NT + 1) * (NP + 1) * 4); double ymax = 0;
        for (int i = 0; i <= NT; i++) for (int j = 0; j <= NP; j++) {
            const double th = PI * i / NT, ph = 2 * PI * j / NP, x = std::sin(th) * std::cos(ph), y = std::sin(th) * std::sin(ph), z = std::cos(th);
            const double Y = realY(l, m, x, z, y);   // ось z орбитали — вертикаль экрана
            ymax = std::max(ymax, std::fabs(Y));
            float* q = &M[((size_t)i * (NP + 1) + j) * 4]; q[0] = (float)x; q[1] = (float)y; q[2] = (float)z; q[3] = (float)Y;
        }
        for (size_t k = 0; k < M.size(); k += 4) { const float r = (float)(std::fabs(M[k + 3]) / ymax); M[k] *= r; M[k + 1] *= r; M[k + 2] *= r; M[k + 3] = M[k + 3] < 0 ? -r : r; }
        it = meshes.emplace(key, std::move(M)).first;
    }
    const std::vector<float>& M = it->second;
    struct Tri { float d; float p[3][2]; float c[4]; };
    std::vector<Tri> tris; tris.reserve(NT * NP * 2);
    const float Lx = -0.45f, Ly = 0.55f, Lz = 0.70f;   // свет сверху-слева-спереди (в координатах вида)
    auto vtx = [&](int i, int j, float& X, float& Y, float& D, float* w) { const float* q = &M[((size_t)i * (NP + 1) + j) * 4]; v3(v, q[0], q[1], q[2], X, Y, D); w[0] = q[0]; w[1] = q[1]; w[2] = q[2]; return q[3]; };
    for (int i = 0; i < NT; i++) for (int j = 0; j < NP; j++) {
        float X[4], Y[4], D[4], W[4][3], s[4];
        s[0] = vtx(i, j, X[0], Y[0], D[0], W[0]); s[1] = vtx(i + 1, j, X[1], Y[1], D[1], W[1]);
        s[2] = vtx(i + 1, j + 1, X[2], Y[2], D[2], W[2]); s[3] = vtx(i, j + 1, X[3], Y[3], D[3], W[3]);
        // нормаль в экранных координатах по диагоналям четырёхугольника
        const float ax = X[2] - X[0], ay = -(Y[2] - Y[0]), az = D[2] - D[0], bx = X[3] - X[1], by = -(Y[3] - Y[1]), bz = D[3] - D[1];
        float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx; const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (nl < 1e-9f) continue;
        nx /= nl; ny /= nl; nz /= nl; if (nz > 0) { nx = -nx; ny = -ny; nz = -nz; }   // к зрителю
        // Ламберт + блик Блинна — Фонга (полупрозрачные глянцевые «лепестки», как на рисунках орбиталей)
        const float dif = std::max(0.0f, -(nx * Lx + ny * Ly + nz * -Lz)), sh = 0.42f + 0.62f * dif;
        const float hx = Lx, hy = Ly, hz = -Lz - 1.0f, hl = std::sqrt(hx * hx + hy * hy + hz * hz);
        const float spec = 0.55f * std::pow(std::max(0.0f, (nx * hx + ny * hy + nz * hz) / hl), 24.0f);
        float r, g, b; orbColor(l, (s[0] + s[1] + s[2] + s[3]) < 0, r, g, b);
        r = std::min(1.0f, r * sh + spec); g = std::min(1.0f, g * sh + spec); b = std::min(1.0f, b * sh + spec);
        const float dm = 0.25f * (D[0] + D[1] + D[2] + D[3]);
        Tri t1{dm, {{X[0], Y[0]}, {X[1], Y[1]}, {X[2], Y[2]}}, {r, g, b, alpha}};
        Tri t2{dm, {{X[0], Y[0]}, {X[2], Y[2]}, {X[3], Y[3]}}, {r, g, b, alpha}};
        tris.push_back(t1); tris.push_back(t2);
    }
    std::sort(tris.begin(), tris.end(), [](const Tri& a, const Tri& b) { return a.d > b.d; });
    MonoAtoms atomsColored;
    glDisable(GL_TEXTURE_2D); glBegin(GL_TRIANGLES);
    for (auto& t : tris) { glColor4f(t.c[0], t.c[1], t.c[2], t.c[3]); for (auto& p : t.p) glVertex2f(p[0], p[1]); }
    glEnd();
}
// оси x, y, z (серые) — как на рисунках орбиталей
static void drawAxes3(const View3& v, float len) {
    const char* nm[3] = {"x", "z", "y"}; const float dir[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    glEnable(GL_LINE_SMOOTH);
    for (int k = 0; k < 3; k++) {
        float X0, Y0, D0, X1, Y1, D1; v3(v, -dir[k][0] * len, -dir[k][1] * len, -dir[k][2] * len, X0, Y0, D0); v3(v, dir[k][0] * len, dir[k][1] * len, dir[k][2] * len, X1, Y1, D1);
        col(withA(C_DIM, 0.7f)); glBegin(GL_LINES); arrowPx(X0, Y0, X1, Y1, uiPx(6)); glEnd();
        drawText(fontXS, X1 + uiPx(3), Y1 - fontXS.h, nm[k], C_DIM);
    }
    glDisable(GL_LINE_SMOOTH);
}
// миниатюра атома (таблица Менделеева, карточка атома): облако внешней части атома, медленно вращается
static void drawAtomMini(int Z, float x, float y, float s, float t) {
    if (Z < 1 || Z > 118) return;
    const OrbCloud& c = atomCloud(Z, 7000);
    View3 v{x + s / 2, y + s / 2, s * 0.36f, t * 0.5f, 0.35f};
    pushClip(x, y, s, s);
    drawCloud(c, v, std::max(1.8f, s * 0.024f), 0.12f, false);
    drawNucleusDot(v, std::max(1.2f, s * 0.012f));
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
    boxPanel(vx, vy, vs, vs, hexc(0x050505), C_LINE);
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
    if (avMode == 0) {
        const OrbCloud& c = atomCloud(avZ, 60000);
        View3 v{vx + vs / 2, vy + vs / 2, vs * 0.36f, avYaw, avPitch};
        drawCloud(c, v, uiPx(3.2f), 0.075f, false);
        drawNucleusDot(v, uiPx(2.5f));
        drawText(fontXS, vx + uiPx(8), vy + vs - fontXS.h - uiPx(6), fmt("облако — %d тыс. случайных точек с плотностью |ψ|²; радиус рисунка %.2f Å", 60, c.R * 0.529), C_DIM);
    } else if (avMode == 1) {
        const OrbCloud& c = orbitalCloud(avZ, avN, avL, avM, 16000);
        View3 v{vx + vs / 2, vy + vs / 2, vs * 0.40f, avYaw, avPitch};
        drawAxes3(v, 1.15f);
        drawCloud(c, v, uiPx(2.4f), 0.22f, true);
        drawNucleusDot(v, uiPx(2.5f));
        View3 lv{vx + vs - uiPx(80), vy + uiPx(80), uiPx(60), avYaw, avPitch};   // угловая форма в углу
        drawLobes(avL, avM, lv, 0.95f);
        drawText(fontXS, vx + vs - uiPx(150), vy + uiPx(150), "угловая часть |Y|", C_DIM);
    } else {   // все формы s, p, d, f — как таблица в учебнике
        const int rows = 4; const float rh = vs / rows;
        for (int l = 0; l < 4; l++) for (int m = 0; m < orbCount(l); m++) {
            const float cw = vs / 7, cx = vx + vs / 2 + (m - (orbCount(l) - 1) / 2.0f) * cw, cy = vy + rh * l + rh * 0.46f;
            View3 v{cx, cy, std::min(cw, rh) * 0.40f, avYaw, avPitch};
            drawAxes3(v, 1.05f); drawLobes(l, m, v, 0.95f);
            const float nw = uiPx(40); drawOrbName(fontU, cx - nw / 2, cy + rh * 0.36f, 0, l, m, C_TEXT);
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
                                               k == 0 ? "Все электроны атома: облака оболочек" : k == 1 ? "Одна орбиталь: облако |ψ|² и её форма" : "Формы всех орбиталей s, p, d и f")) avMode = k;
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
        : "Электрон не летает по орбите: это стоячая волна, и облако показывает, где его можно найти. Плотные места — где он бывает чаще, пустые шары и «провалы» — узлы волны. Внутренние электроны экранируют ядро, поэтому внешние видят заряд Z_eff меньше Z и держатся дальше.";
    drawWrapped(fontXS, cx, yy, cw, T(note), C_DIM);
    if (ui.pressed && !inPR(avRect)) atomViewOn = false;
}

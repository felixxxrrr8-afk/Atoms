// ===================================== КВАРКИ И АДРОНЫ =====================================
// Сцены 110–114: внутри протона. Кварки связаны глюонным полем, которое между удаляющимися кварками стягивается
// в «струну» с постоянным натяжением σ ≈ 0.9 ГэВ/фм (около 15 тонн силы!) — потенциал Корнелла
//   V(r) = σ·r − (4/3)·αs·ħc / r
// (у бариона три кварка соединены струной в виде буквы Y с узлом в точке Ферма треугольника). Кварки движутся
// релятивистски: dx/dt = p/E, dp/dt = F, E = √(p² + m²); массы — «конституентные» (u, d ≈ 0.34 ГэВ).
// Цвет: каждый кварк красный, зелёный или синий, антикварк — антицвет; адрон всегда «белый». Глюон переносит
// пару цвет–антицвет: кварки обмениваются цветами. Когда струна вытянута больше чем на ~1.2 фм, энергии в ней хватает
// на новую пару кварк–антикварк — струна рвётся на две (механизм Швингера), и свободного кварка не получить.
// Единицы: фм (10⁻¹⁵ м), фм/c (3.3·10⁻²⁴ с), ГэВ.
namespace qk {
constexpr double HBARC = 0.19733, SIGMA = 0.9, ALPHAS = 0.30, RSOFT = 0.12;   // ГэВ·фм, ГэВ/фм, —, фм (сглаживание кулона)
enum { FU, FD, FS, FC, FB, FN };
static const char* FNAME[FN] = {"u", "d", "s", "c", "b"};
static const double FMASS[FN] = {0.336, 0.340, 0.486, 1.55, 4.73};   // ГэВ
static const int FQ3[FN] = {2, -1, -1, 2, -1};                        // заряд ×3 (в зарядах протона)
struct Quark { double x, y, z, px, py, pz; int f; bool anti; int col; int had; float flash; bool alive = true; bool pin = false; };
struct Str { int a, b, c; };           // струна: мезон — a, b (c = −1); барион — a, b, c
struct Gluon { int from, to; float u; int colFrom, colTo; };
struct Hadr { double x, y, z, vx, vy, vz; std::string name; int q3; float age; };   // адрон, вылетевший из струи
struct Spark { double x, y, z; float age; };
static std::vector<Quark> q; static std::vector<Str> strs; static std::vector<Gluon> gl; static std::vector<Hadr> out; static std::vector<Spark> sparks;
static int scene = 0; static double t = 0, timeScale = 1.5, L = 6;
static double J[3] = {0, 0, 0};        // узел Y-струны (для рисунка)
static bool pulling = true; static double pullF = 1.5;   // разрыв струны: тянуть антикварк, сила (ГэВ/фм)
static int breaks = 0; static bool neutronMode = false;
static std::vector<int> build;         // конструктор адронов: кварки (f + 10·anti)
static double decayT = 0; static int decayStage = 0;   // бета-распад нейтрона: фаза анимации
static std::vector<float> hist;        // энергия струны мезона по времени
}
// цвета для рисунка: кварк — R, G, B; антикварк — дополнительный (антикрасный = голубой и т. д.)
static void qkColor(int col, bool anti, float& r, float& g, float& b) {
    static const float C[3][3] = {{1.0f, 0.25f, 0.22f}, {0.25f, 0.95f, 0.30f}, {0.30f, 0.45f, 1.0f}};
    r = C[col][0]; g = C[col][1]; b = C[col][2];
    if (anti) { r = 1.15f - r; g = 1.15f - g; b = 1.15f - b; }
}
static int qkAdd(int f, bool anti, int col, double x, double y, double z, int had) {
    qk::Quark k{}; k.x = x; k.y = y; k.z = z; k.f = f; k.anti = anti; k.col = col; k.had = had; k.flash = 0;
    qk::q.push_back(k); return (int)qk::q.size() - 1;
}
// точка Ферма (узел Y-струны): итерации Вайсфельда; если угол треугольника ≥ 120°, узел — в этой вершине
static void qkFermat(const qk::Quark& a, const qk::Quark& b, const qk::Quark& c, double* j) {
    const double P[3][3] = {{a.x, a.y, a.z}, {b.x, b.y, b.z}, {c.x, c.y, c.z}};
    for (int v = 0; v < 3; v++) {   // вершина с тупым углом ≥ 120°
        const double* A = P[v]; const double* B = P[(v + 1) % 3]; const double* Cc = P[(v + 2) % 3];
        double u[3], w[3], lu = 0, lw = 0, d = 0;
        for (int k = 0; k < 3; k++) { u[k] = B[k] - A[k]; w[k] = Cc[k] - A[k]; lu += u[k] * u[k]; lw += w[k] * w[k]; d += u[k] * w[k]; }
        if (lu > 1e-12 && lw > 1e-12 && d / std::sqrt(lu * lw) <= -0.5) { for (int k = 0; k < 3; k++) j[k] = A[k]; return; }
    }
    for (int k = 0; k < 3; k++) j[k] = (P[0][k] + P[1][k] + P[2][k]) / 3;
    for (int it = 0; it < 30; it++) {
        double num[3] = {0, 0, 0}, den = 0;
        for (auto& p : P) { const double d = std::sqrt((p[0] - j[0]) * (p[0] - j[0]) + (p[1] - j[1]) * (p[1] - j[1]) + (p[2] - j[2]) * (p[2] - j[2])) + 1e-9; for (int k = 0; k < 3; k++) num[k] += p[k] / d; den += 1 / d; }
        for (int k = 0; k < 3; k++) j[k] = num[k] / den;
    }
}
// силы: струна (натяжение σ вдоль каждого отрезка к узлу или к партнёру) и кулоновская часть на малых расстояниях
static void qkForces(std::vector<double>& F) {
    using namespace qk;
    F.assign(q.size() * 3, 0.0);
    auto coul = [&](int a, int b, double k) {   // притяжение −k·ħc/√(r² + a²)
        const double dx = q[b].x - q[a].x, dy = q[b].y - q[a].y, dz = q[b].z - q[a].z, r2 = dx * dx + dy * dy + dz * dz + RSOFT * RSOFT;
        const double f = k * HBARC / (r2 * std::sqrt(r2));
        F[a * 3] += f * dx; F[a * 3 + 1] += f * dy; F[a * 3 + 2] += f * dz; F[b * 3] -= f * dx; F[b * 3 + 1] -= f * dy; F[b * 3 + 2] -= f * dz;
    };
    auto pullTo = [&](int a, const double* p) {   // натяжение струны от кварка к точке p
        const double dx = p[0] - q[a].x, dy = p[1] - q[a].y, dz = p[2] - q[a].z, r = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (r < 1e-9) return; F[a * 3] += SIGMA * dx / r; F[a * 3 + 1] += SIGMA * dy / r; F[a * 3 + 2] += SIGMA * dz / r;
    };
    for (auto& s : strs) {
        if (s.c < 0) {
            const double pb[3] = {q[s.b].x, q[s.b].y, q[s.b].z}, pa[3] = {q[s.a].x, q[s.a].y, q[s.a].z};
            pullTo(s.a, pb); pullTo(s.b, pa); coul(s.a, s.b, 4.0 / 3 * ALPHAS);
        } else {
            qkFermat(q[s.a], q[s.b], q[s.c], J);
            pullTo(s.a, J); pullTo(s.b, J); pullTo(s.c, J);
            coul(s.a, s.b, 2.0 / 3 * ALPHAS); coul(s.b, s.c, 2.0 / 3 * ALPHAS); coul(s.a, s.c, 2.0 / 3 * ALPHAS);
        }
    }
}
static double qkStringLen(const qk::Str& s) {
    using namespace qk;
    auto d = [&](int a, const double* p) { return std::sqrt((q[a].x - p[0]) * (q[a].x - p[0]) + (q[a].y - p[1]) * (q[a].y - p[1]) + (q[a].z - p[2]) * (q[a].z - p[2])); };
    if (s.c < 0) { const double pb[3] = {q[s.b].x, q[s.b].y, q[s.b].z}; return d(s.a, pb); }
    double j[3]; qkFermat(q[s.a], q[s.b], q[s.c], j); return d(s.a, j) + d(s.b, j) + d(s.c, j);
}
// адрон по составу: имя, масса (МэВ), спин основного состояния; пустое имя — такого бесцветного сочетания нет
struct HadInfo { const char* name; double mass; const char* spin; };
static HadInfo qkIdentify(std::vector<int> v) {   // элементы: аромат + 10, если антикварк
    using namespace qk;
    std::sort(v.begin(), v.end());
    auto is = [&](std::initializer_list<int> w) { std::vector<int> x(w); std::sort(x.begin(), x.end()); return x == v; };
    if (v.size() == 3) {
        if (is({FU, FU, FD})) return {"протон p", 938.27, "1/2"};  if (is({FU, FD, FD})) return {"нейтрон n", 939.57, "1/2"};
        if (is({FU, FU, FU})) return {"Δ⁺⁺", 1232, "3/2"};         if (is({FD, FD, FD})) return {"Δ⁻", 1232, "3/2"};
        if (is({FU, FD, FS})) return {"лямбда Λ⁰", 1115.68, "1/2"}; if (is({FU, FU, FS})) return {"сигма Σ⁺", 1189.37, "1/2"};
        if (is({FD, FD, FS})) return {"сигма Σ⁻", 1197.45, "1/2"}; if (is({FU, FS, FS})) return {"кси Ξ⁰", 1314.86, "1/2"};
        if (is({FD, FS, FS})) return {"кси Ξ⁻", 1321.71, "1/2"};   if (is({FS, FS, FS})) return {"омега Ω⁻", 1672.45, "3/2"};
        if (is({FU, FD, FC})) return {"Λc⁺", 2286.46, "1/2"};      if (is({FU, FD, FB})) return {"Λb⁰", 5619.6, "1/2"};
        if (is({10 + FU, 10 + FU, 10 + FD})) return {"антипротон", 938.27, "1/2"}; if (is({10 + FU, 10 + FD, 10 + FD})) return {"антинейтрон", 939.57, "1/2"};
        return {"", 0, ""};
    }
    if (v.size() == 2 && ((v[0] < 10) != (v[1] < 10))) {
        const int a = v[0], b = v[1] - 10;   // кварк a и антикварк b
        const int lo = std::min(a, b), hi = std::max(a, b);
        if (a == b) { if (a <= FD) return {a == FU ? "пион π⁰ (uū)" : "пион π⁰ (dđ)", 134.98, "0"}; if (a == FS) return {"фи-мезон φ", 1019.46, "1"}; if (a == FC) return {"J/ψ (чармоний)", 3096.9, "1"}; return {"ипсилон Υ", 9460.3, "1"}; }
        if (lo == FU && hi == FD) return a == FU ? HadInfo{"пион π⁺", 139.57, "0"} : HadInfo{"пион π⁻", 139.57, "0"};
        if (lo == FU && hi == FS) return a == FU ? HadInfo{"каон K⁺", 493.68, "0"} : HadInfo{"каон K⁻", 493.68, "0"};
        if (lo == FD && hi == FS) return a == FD ? HadInfo{"каон K⁰", 497.61, "0"} : HadInfo{"антикаон K⁰", 497.61, "0"};
        if (lo == FU && hi == FC) return a == FC ? HadInfo{"D⁰", 1864.8, "0"} : HadInfo{"анти-D⁰", 1864.8, "0"};
        if (lo == FD && hi == FC) return a == FC ? HadInfo{"D⁺", 1869.7, "0"} : HadInfo{"D⁻", 1869.7, "0"};
        if (lo == FS && hi == FC) return a == FC ? HadInfo{"Ds⁺", 1968.4, "0"} : HadInfo{"Ds⁻", 1968.4, "0"};
        if (lo == FU && hi == FB) return a == FB ? HadInfo{"B⁻", 5279.3, "0"} : HadInfo{"B⁺", 5279.3, "0"};
        if (lo == FD && hi == FB) return a == FB ? HadInfo{"анти-B⁰", 5279.7, "0"} : HadInfo{"B⁰", 5279.7, "0"};
        return {"", 0, ""};
    }
    return {"", 0, ""};
}
// ---- сцены
static void qkMeson(int f1, int f2, double cx, double cy, double cz, double sep, int col) {
    using namespace qk;
    const int a = qkAdd(f1, false, col, cx - sep / 2, cy, cz, 0), b = qkAdd(f2, true, col, cx + sep / 2, cy, cz, 0);
    strs.push_back({a, b, -1});
}
static void qkBaryon(int f1, int f2, int f3, double cx, double cy, double cz, double R) {
    using namespace qk;
    int id[3]; const int f[3] = {f1, f2, f3};
    for (int k = 0; k < 3; k++) { const double a = 2 * PI * k / 3 + 0.3; id[k] = qkAdd(f[k], false, k, cx + R * std::cos(a), cy + R * std::sin(a), cz + 0.2 * R * (k - 1), 0); }
    // начальные импульсы: кварки движутся по кругу — «дыхание» протона
    for (int k = 0; k < 3; k++) { const double a = 2 * PI * k / 3 + 0.3; q[id[k]].px = -0.35 * std::sin(a); q[id[k]].py = 0.35 * std::cos(a); q[id[k]].pz = 0.12 * (k - 1); }
    strs.push_back({id[0], id[1], id[2]});
}
static void qkReset(int sc) {
    using namespace qk;
    scene = clampv(sc, 0, 4); t = 0; q.clear(); strs.clear(); gl.clear(); out.clear(); sparks.clear(); hist.clear(); breaks = 0; decayT = 0; decayStage = 0;
    switch (scene) {
    case 0: L = 2.4; timeScale = 1.5; qkBaryon(FU, neutronMode ? FD : FU, FD, L / 2, L / 2, L / 2, 0.5); break;
    case 1: L = 8; timeScale = 1.2; qkMeson(FU, FD, L / 2 - 1.5, L / 2, L / 2, 0.4, 0); q[0].pin = true; pulling = true; break;   // кварк удерживаем, антикварк тянем
    case 2: {   // два протона летят навстречу почти со скоростью света
        L = 30; timeScale = 3;
        qkBaryon(FU, FU, FD, L / 2 - 6, L / 2, L / 2, 0.4); qkBaryon(FU, FU, FD, L / 2 + 6, L / 2, L / 2, 0.4);
        for (int k = 0; k < 3; k++) { q[k].px += 12; q[3 + k].px -= 12; }   // 12 ГэВ/c на кварк
        break; }
    case 3: L = 2.6; timeScale = 1.5; if (build.empty()) build = {FU, FU, FD}; {
        const HadInfo h = qkIdentify(build);
        if (build.size() == 3 && h.name[0]) { const bool an = build[0] >= 10; qkBaryon(build[0] % 10, build[1] % 10, build[2] % 10, L / 2, L / 2, L / 2, 0.45); if (an) for (auto& k : q) k.anti = true; }
        else if (build.size() == 2 && h.name[0]) { const int a = build[0] < 10 ? build[0] : build[1], b = (build[0] < 10 ? build[1] : build[0]) % 10; qkMeson(a, b, L / 2, L / 2, L / 2, 0.5, 0); q[0].py = 0.3; q[1].py = -0.3; }
        else for (size_t k = 0; k < build.size(); k++) qkAdd(build[k] % 10, build[k] >= 10, (int)k % 3, L / 2 + 0.6 * std::cos(2 * PI * k / std::max<size_t>(1, build.size())), L / 2 + 0.6 * std::sin(2 * PI * k / std::max<size_t>(1, build.size())), L / 2, -1);
        } break;
    case 4: L = 3; timeScale = 1.5; qkBaryon(FU, FD, FD, L / 2, L / 2, L / 2, 0.5); break;
    }
    S = Sim(); S.Lx = S.Ly = S.Lz = L;   // ящик для камеры
}
// разрыв струны мезона/кварковой нити: пара кварк–антикварк рождается в случайной точке средней части струны
static void qkBreakString(int si, double u = -1) {
    using namespace qk;
    Str s = strs[si]; if (s.c >= 0) return;
    if (u < 0) u = 0.3 + 0.4 * urand();
    const double x = q[s.a].x + u * (q[s.b].x - q[s.a].x), y = q[s.a].y + u * (q[s.b].y - q[s.a].y), z = q[s.a].z + u * (q[s.b].z - q[s.a].z);
    const double r = urand(); const int f = r < 0.4 ? FU : (r < 0.8 ? FD : FS);   // странная пара реже: она тяжелее
    const int col = q[s.a].col;
    const double dx = q[s.b].x - q[s.a].x, dy = q[s.b].y - q[s.a].y, dz = q[s.b].z - q[s.a].z, l = std::sqrt(dx * dx + dy * dy + dz * dz) + 1e-9;
    const int nb = qkAdd(f, true, col, x - 0.06 * dx / l, y - 0.06 * dy / l, z - 0.06 * dz / l, 0);   // антикварк — к кварку a
    const int nq = qkAdd(f, false, col, x + 0.06 * dx / l, y + 0.06 * dy / l, z + 0.06 * dz / l, 0);  // кварк — к антикварку b
    q[nb].flash = q[nq].flash = 1;
    strs[si] = {s.a, nb, -1}; strs.push_back({nq, s.b, -1});
    sparks.push_back({x, y, z, 0}); breaks++;
}
static void qkStep1(double dt) {
    using namespace qk;
    std::vector<double> F; qkForces(F);
    if (scene == 1 && pulling && !strs.empty()) {   // внешняя «рука» тянет антикварк первого мезона вправо
        const int b = strs[0].b; F[b * 3] += pullF;
    }
    for (size_t k = 0; k < q.size(); k++) {
        Quark& c = q[k]; if (!c.alive || c.pin) continue;
        c.px += F[k * 3] * dt; c.py += F[k * 3 + 1] * dt; c.pz += F[k * 3 + 2] * dt;
        const double m = FMASS[c.f], E = std::sqrt(c.px * c.px + c.py * c.py + c.pz * c.pz + m * m);
        c.x += c.px / E * dt; c.y += c.py / E * dt; c.z += c.pz / E * dt;   // скорость p/E < c
        c.flash = std::max(0.0f, c.flash - (float)dt);
    }
    t += dt;
    // обмен глюонами: пары кварков одного адрона меняются цветами (адрон остаётся белым)
    if (scene != 2 && urand() < 0.6 * dt) for (auto& s : strs) {
        if (urand() > 0.5) continue;
        const int a = s.a, b = s.c >= 0 ? (urand() < 0.5 ? s.b : s.c) : s.b;
        gl.push_back({a, b, 0, q[a].col, q[b].col});
    }
    for (auto& g : gl) {
        g.u += (float)(dt / 0.35);
        if (g.u >= 1 && g.u < 1e9) {   // глюон долетел: кварки меняются цветами (у мезона оба берут новый цвет и антицвет)
            if (!q[g.to].anti && !q[g.from].anti) std::swap(q[g.from].col, q[g.to].col);
            else { const int nc = (q[g.from].col + 1 + (urand() < 0.5 ? 0 : 1)) % 3; q[g.from].col = nc; q[g.to].col = nc; }
            q[g.to].flash = 0.6f; g.u = 1e9f;
        }
    }
    gl.erase(std::remove_if(gl.begin(), gl.end(), [](const Gluon& g) { return g.u > 1e8f; }), gl.end());
    // рвутся перетянутые струны мезонов: энергии σ·L хватает на новую пару. В столкновении — как в модели Лунда:
    // длинная струна с большой инвариантной массой рвётся в случайных местах с частотой ~0.8 разрыва на фм за фм/c
    if (scene == 2) for (size_t si = 0; si < strs.size(); si++) {
        const Str s = strs[si]; if (s.c >= 0) continue;
        const double len = qkStringLen(s); if (len < 0.3) continue;
        auto en = [&](const Quark& c) { return std::sqrt(c.px * c.px + c.py * c.py + c.pz * c.pz + FMASS[c.f] * FMASS[c.f]); };
        const double E = en(q[s.a]) + en(q[s.b]) + SIGMA * len, px = q[s.a].px + q[s.b].px, py = q[s.a].py + q[s.b].py, pz = q[s.a].pz + q[s.b].pz;
        if (E * E - (px * px + py * py + pz * pz) > 1.5 * 1.5 && urand() < 0.8 * len * dt) qkBreakString((int)si, 0.1 + 0.8 * urand());
    }
    else for (size_t si = 0; si < strs.size(); si++) if (strs[si].c < 0 && qkStringLen(strs[si]) > 1.25 + 0.35 * urand()) {
        if (scene == 1 && strs.size() >= 4) { pulling = false; continue; }   // хватит: видно, что кварк не вырвать
        qkBreakString((int)si);
        if (scene == 1 && si == 0) pulling = true;
    }
    for (auto& s : sparks) s.age += (float)dt;
    sparks.erase(std::remove_if(sparks.begin(), sparks.end(), [](const Spark& s) { return s.age > 1.2f; }), sparks.end());
    // столкновение: когда протоны сошлись, кварки обмениваются цветовыми связями — натягиваются струны между
    // разлетающимися кварками разных протонов, а затем рвутся на цепочки мезонов (струи)
    if (scene == 2 && strs.size() == 2 && std::fabs(q[0].x - q[3].x) < 0.6) {
        strs.clear();
        strs.push_back({0, 3, -1}); strs.push_back({1, 4, -1}); strs.push_back({2, 5, -1});
        for (int k = 3; k < 6; k++) { q[k].anti = true; }   // условно: второй протон отдаёт «антицветную» сторону связи
        for (int k = 0; k < 6; k++) { q[k].py += 1.5 * (urand() - 0.5); q[k].pz += 1.5 * (urand() - 0.5); q[k].flash = 1; }
        sparks.push_back({L / 2, L / 2, L / 2, 0});
    }
    if (scene == 2) {   // кусок струны с малой инвариантной массой (≲ 1.2 ГэВ) — готовый мезон: улетает как частица
        for (size_t si = 0; si < strs.size();) {
            const Str s = strs[si]; const double len = s.c < 0 ? qkStringLen(s) : 1e9;
            if (s.c >= 0 || len > 0.8) { si++; continue; }
            const int a = s.a, b = s.b;
            auto en = [&](const Quark& c) { return std::sqrt(c.px * c.px + c.py * c.py + c.pz * c.pz + FMASS[c.f] * FMASS[c.f]); };
            const double E = en(q[a]) + en(q[b]) + SIGMA * len;
            const double px = q[a].px + q[b].px, py = q[a].py + q[b].py, pz = q[a].pz + q[b].pz;
            if (E * E - (px * px + py * py + pz * pz) > 1.2 * 1.2) { si++; continue; }
            const HadInfo h = qkIdentify({q[a].anti ? q[a].f + 10 : q[a].f, q[b].anti ? q[b].f + 10 : q[b].f});
            std::string nm = h.name[0] ? h.name : "мезон"; { const size_t sp = nm.find(' '); if (sp != std::string::npos) nm = nm.substr(sp + 1); const size_t br = nm.find(" ("); if (br != std::string::npos) nm = nm.substr(0, br); }   // коротко: π⁺, K⁰, φ
            out.push_back({0.5 * (q[a].x + q[b].x), 0.5 * (q[a].y + q[b].y), 0.5 * (q[a].z + q[b].z), px / E, py / E, pz / E, nm, (q[a].anti ? -FQ3[q[a].f] : FQ3[q[a].f]) + (q[b].anti ? -FQ3[q[b].f] : FQ3[q[b].f]), 0});
            q[a].alive = q[b].alive = false; strs.erase(strs.begin() + si);
        }
        for (auto& h : out) { h.x += h.vx * dt; h.y += h.vy * dt; h.z += h.vz * dt; h.age += (float)dt; }
    }
    if (scene == 4) {   // бета-распад нейтрона: d → u + W⁻, W⁻ → e⁻ + ν̄
        decayT += dt;
        if (decayStage == 0 && decayT > 6) { decayStage = 1; q[2].f = FU; q[2].flash = 1.5f; sparks.push_back({q[2].x, q[2].y, q[2].z, 0}); decayT = 0; }
        else if (decayStage == 1 && decayT > 8) { decayStage = 2; decayT = 0; }
        else if (decayStage == 2 && decayT > 4) { qkReset(4); }
    }
}
static void qkStep(double frameDt) {
    using namespace qk;
    const double want = timeScale * frameDt; int nst = (int)std::ceil(want / 0.004); nst = clampv(nst, 1, 400);
    for (int s = 0; s < nst; s++) qkStep1(want / nst);
    if (scene == 1 && !strs.empty()) { hist.push_back((float)(SIGMA * qkStringLen(strs[0]))); if (hist.size() > 400) hist.erase(hist.begin()); }
}
// ---- рисунок
static void qkLabel(const qk::Quark& c, float sx, float sy) {
    const std::string s = qk::FNAME[c.f]; const float w = textW(fontXS, s);
    drawText(fontXS, sx - w / 2, sy - fontXS.h / 2, s, C_TEXT_HI);
    if (c.anti) rectFill(sx - w / 2, sy - fontXS.h / 2 - uiPx(1), w, 1, C_TEXT_HI);   // черта антикварка
}
static void qkDraw() {
    using namespace qk;
    glEnable(GL_SCISSOR_TEST); glScissor((int)sceneX, (int)(winH - sceneY - sceneH), (int)sceneW, (int)sceneH);
    rectFill(sceneX, sceneY, sceneW, sceneH, C_SCENE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    {   // струны: светящиеся трубки, у каждого кварка — его цвет, к узлу/середине цвет «белеет»
        MonoAtoms colored;
        auto tube = [&](const Quark& a, const double* p, float r0, float g0, float b0) {
            const double dx = p[0] - a.x, dy = p[1] - a.y, dz = p[2] - a.z, l = std::sqrt(dx * dx + dy * dy + dz * dz);
            const int ns = std::max(2, (int)(l / 0.04));
            for (int k = 0; k <= ns; k++) {
                const double u = (double)k / ns; float sx, sy, d, s;
                if (!project(a.x + u * dx, a.y + u * dy, a.z + u * dz, sx, sy, d, s)) continue;
                const float w = (float)u; quadUV(sx, sy, 0.13f * s, r0 + (1 - r0) * 0.6f * w, g0 + (1 - g0) * 0.6f * w, b0 + (1 - b0) * 0.6f * w, 0.35f);
            }
        };
        for (auto& st : strs) {
            float r, g, b;
            if (st.c < 0) {
                const Quark &A = q[st.a], &B = q[st.b]; const double mid[3] = {0.5 * (A.x + B.x), 0.5 * (A.y + B.y), 0.5 * (A.z + B.z)};
                qkColor(A.col, A.anti, r, g, b); tube(A, mid, r, g, b); qkColor(B.col, B.anti, r, g, b); tube(B, mid, r, g, b);
            } else {
                double j[3]; qkFermat(q[st.a], q[st.b], q[st.c], j);
                for (int k : {st.a, st.b, st.c}) { qkColor(q[k].col, q[k].anti, r, g, b); tube(q[k], j, r, g, b); }
            }
        }
        // глюоны: волнистые линии в два цвета
        for (auto& g : gl) {
            if (g.u > 1) continue;
            const Quark &A = q[g.from], &B = q[g.to]; const double u = g.u;
            const double x = A.x + u * (B.x - A.x), y = A.y + u * (B.y - A.y), z = A.z + u * (B.z - A.z);
            float sx, sy, d, s; if (!project(x, y, z, sx, sy, d, s)) continue;
            float r, gg, b; qkColor(g.colFrom, false, r, gg, b); quadUV(sx, sy, 0.22f * s, r, gg, b, 0.8f);
            qkColor(g.colTo, false, r, gg, b); quadUV(sx + 0.06f * s, sy, 0.14f * s, r, gg, b, 0.8f);
        }
        for (auto& sp : sparks) { float sx, sy, d, s; if (project(sp.x, sp.y, sp.z, sx, sy, d, s)) quadUV(sx, sy, (0.3f + 1.5f * sp.age) * s, 1.0f, 0.9f, 0.6f, 0.5f * (1 - sp.age / 1.2f)); }
        flushQuads(texGlow);
    }
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    {   // кварки — от дальних к ближним
        MonoAtoms colored;
        std::vector<std::pair<float, int>> ord;
        for (int k = 0; k < (int)q.size(); k++) if (q[k].alive) ord.push_back({(float)viewDepth(q[k].x, q[k].y, q[k].z), k});
        std::sort(ord.begin(), ord.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
        for (auto& o : ord) { const Quark& c = q[o.second]; float r, g, b; qkColor(c.col, c.anti, r, g, b); const float f = 1 + 0.6f * c.flash; nucBall(c.x, c.y, c.z, 0.13f, std::min(1.0f, r * f), std::min(1.0f, g * f), std::min(1.0f, b * f)); }
        for (auto& h : out) nucBall(h.x, h.y, h.z, 0.16f, 0.85f, 0.85f, 0.85f);
        flushQuads(texCore);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (auto& c : q) if (c.alive) { float sx, sy, d, s; if (project(c.x, c.y, c.z, sx, sy, d, s) && 0.13f * s > 7) qkLabel(c, sx, sy); }
    {   // подписи адронов — без наложения друг на друга
        std::vector<PR> used;
        for (auto& h : out) {
            float sx, sy, d, s; if (!project(h.x, h.y, h.z, sx, sy, d, s) || h.age > 12) continue;
            const PR r{sx + uiPx(5), sy - fontXS.h, textW(fontXS, h.name) + uiPx(2), fontXS.h}; bool hit = false;
            for (auto& u : used) if (r.x < u.x + u.w && u.x < r.x + r.w && r.y < u.y + u.h && u.y < r.y + r.h) { hit = true; break; }
            if (hit) continue; used.push_back(r); drawText(fontXS, r.x, r.y, h.name, withA(C_TEXT, 0.9f));
        }
    }
    if (scene == 4 && decayStage >= 1) {   // W⁻ → e⁻ + ν̄: электрон и антинейтрино улетают
        MonoAtoms colored; glEnable(GL_LINE_SMOOTH); glLineWidth(1.8f);
        const double u = std::min(1.0, decayT / 5.0), cx = q[2].x, cy = q[2].y, cz = q[2].z;
        glColor4f(0.45f, 0.65f, 1.0f, 0.9f); glBegin(GL_LINES); line3(cx, cy, cz, cx + 3 * u, cy + 1.2 * u, cz); glEnd();
        glColor4f(0.6f, 0.6f, 0.6f, 0.6f); glBegin(GL_LINES); line3(cx, cy, cz, cx - 2.5 * u, cy + 2 * u, cz + 0.5 * u); glEnd();
        glLineWidth(1); glDisable(GL_LINE_SMOOTH);
        float sx, sy, d, s;
        if (project(cx + 3 * u, cy + 1.2 * u, cz, sx, sy, d, s)) drawText(fontXS, sx + uiPx(4), sy, "e⁻", C_TEXT_HI);
        if (project(cx - 2.5 * u, cy + 2 * u, cz + 0.5 * u, sx, sy, d, s)) drawText(fontXS, sx + uiPx(4), sy, "антинейтрино", C_TEXT);
    }
    // масштаб
    { const double pps = pxPerSigma(), Lb = niceStep(uiPx(110) / std::max(1e-9, pps)); const float lp = (float)(Lb * pps), x = sceneX + uiPx(12), y = sceneY + sceneH - uiPx(22);
      rectFill(x, y, lp, 1, withA(C_TEXT, 0.85f)); rectFill(x, y - uiPx(4), 1, uiPx(5), withA(C_TEXT, 0.85f)); rectFill(x + lp - 1, y - uiPx(4), 1, uiPx(5), withA(C_TEXT, 0.85f));
      drawText(fontXS, x + lp / 2 - uiPx(10), y - uiPx(4) - fontXS.h, fmt("%g фм", Lb), withA(C_TEXT, 0.85f)); }
    glDisable(GL_SCISSOR_TEST);
}
static std::string qkTimeStr() { return fmt("%.2f фм/c (%.2g с)", qk::t, qk::t * 3.3356e-24); }
static void qkDefaults(int s) { using namespace qk; if (s == 0) neutronMode = false; if (s == 1) pullF = 1.5; if (s == 3) build = {FU, FU, FD}; }
static const char* QK_TITLES[5] = {
    "Протон изнутри: три кварка (uud) связаны струной глюонного поля в виде буквы Y; цвета кварков меняются, но вместе всегда «белые»",
    "Разрыв струны: кварк держим, антикварк тянем — натяжение 0.9 ГэВ/фм не падает, и струна рвётся, рождая новую пару: одиночный кварк не вырвать",
    "Столкновение протонов: при ударе кварки обмениваются связями, струны растягиваются и рвутся на цепочки мезонов — рождаются струи адронов",
    "Конструктор адронов: соберите частицу из кварков — бывают только бесцветные сочетания: три кварка (барион) или кварк и антикварк (мезон)",
    "Бета-распад нейтрона изнутри: d-кварк слабым взаимодействием превращается в u, испуская W⁻ → электрон + антинейтрино, — нейтрон становится протоном"};
static void qkPanel(float x, float y, float w, float h) {
    using namespace qk;
    rectFill(x, y, w, h, C_PANEL); lineV(x, y, y + h, C_LINE);
    const float pad = uiPx(10), cx = x + pad, cw = w - 2 * pad;
    float yy = scrollBegin(6, cx, y + uiPx(6), cw, h - uiPx(10));
    const float bh = uiPx(26), sh = uiPx(30);
    drawText(fontL, cx, yy, "Кварки и адроны", C_TEXT_HI); yy += fontL.h + uiPx(6);
    auto row = [&](const char* k, const std::string& v) { drawText(fontU, cx, yy, k, C_DIM); drawTextR(fontM, cx + cw, yy - uiPx(1), v, C_TEXT_HI); yy += fontU.h + uiPx(5); };
    row("время", qkTimeStr());
    auto content = [&](std::initializer_list<int> ids) {   // состав адрона по номерам кварков
        std::vector<int> v; for (int k : ids) if (k >= 0 && k < (int)q.size()) v.push_back(q[k].anti ? q[k].f + 10 : q[k].f); return qkIdentify(v);
    };
    if (scene == 0 || scene == 4) {
        const HadInfo hi = content({0, 1, 2});
        row("частица", hi.name[0] ? hi.name : "—"); row("масса", fmt("%.1f МэВ", hi.mass));
        row("сумма масс кварков", fmt("%.0f МэВ (голые u, d — ~10 МэВ)", 1000 * (FMASS[q[0].f] + FMASS[q[1].f] + FMASS[q[2].f])));
        int q3 = 0; for (int k = 0; k < 3; k++) q3 += FQ3[q[k].f]; row("заряд", fmt("%+d", q3 / 3));
    } else if (scene == 1) {
        row("разрывов струны", fmt("%d", breaks)); row("мезонов", fmt("%d", (int)strs.size()));
        if (!strs.empty()) row("длина первой струны", fmt("%.2f фм", qkStringLen(strs[0])));
        row("натяжение струны", "0.9 ГэВ/фм ≈ 15 т");
    } else if (scene == 2) {
        row("родилось адронов", fmt("%d", (int)out.size())); row("разрывов струн", fmt("%d", breaks));
        int ch = 0; for (auto& o : out) ch += o.q3; row("заряд адронов", fmt("%+d", ch / 3));
    } else {
        std::string comp; for (int v : build) { comp += v >= 10 ? std::string("анти-") + FNAME[v % 10] : std::string(FNAME[v]); comp += " "; }
        row("состав", comp.empty() ? std::string("—") : comp);
        const HadInfo hi = qkIdentify(build);
        row("частица", hi.name[0] ? hi.name : (build.empty() ? "—" : "не бывает"));
        if (hi.name[0]) { row("масса", fmt("%.1f МэВ", hi.mass)); row("спин", hi.spin); }
        int q3 = 0; for (int v : build) q3 += v >= 10 ? -FQ3[v % 10] : FQ3[v]; row("заряд", q3 % 3 == 0 ? fmt("%+d", q3 / 3) : fmt("%+d/3", q3));
    }
    yy += uiPx(4);
    uiSection(cx, yy, cw, "управление");
    if (uiButton(1900, cx, yy, cw / 2 - uiPx(3), bh, "заново (R)", false, false, "Начать сцену заново")) qkReset(scene);
    if (uiButton(1901, cx + cw / 2 + uiPx(3), yy, cw / 2 - uiPx(3), bh, P.paused ? "пуск (пробел)" : "пауза (пробел)", P.paused, false, "Остановить или запустить время")) P.paused = !P.paused;
    yy += bh + uiPx(6);
    {
        double lg = std::log10(timeScale);
        if (uiSlider(1902, cx, yy, cw, sh - uiPx(4), "скорость времени", &lg, -1, 1.5, false, fmt("%.3g фм/c за 1 с", timeScale), "1 фм/c — время, за которое свет проходит размер протона: 3·10⁻²⁴ с")) timeScale = std::pow(10.0, lg);
        yy += sh;
    }
    if (scene == 0) { if (uiCheck(1903, cx, yy, cw, uiPx(22), "нейтрон (udd)", &neutronMode, "Нейтрон вместо протона: один u-кварк заменён d-кварком")) qkReset(0); yy += uiPx(26); }
    if (scene == 1) {
        if (uiCheck(1904, cx, yy, cw, uiPx(22), "тянуть антикварк", &pulling, "Постоянная внешняя сила тянет антикварк вправо")) {} yy += uiPx(26);
        double f = pullF; if (uiSlider(1905, cx, yy, cw, sh - uiPx(4), "сила", &f, 0.5, 4, false, fmt("%.1f ГэВ/фм", f), "Чтобы оторвать кварк, сила должна превышать натяжение струны 0.9 ГэВ/фм")) pullF = f; yy += sh;
    }
    if (scene == 3) {   // клавиатура кварков
        const float bw = std::floor((cw - 4 * uiPx(4)) / 5);
        for (int a = 0; a < 2; a++) {
            for (int f = 0; f < FN; f++) {
                const std::string lab = a ? std::string("анти-") + FNAME[f] : FNAME[f];
                if (uiButton(1910 + a * 5 + f, cx + f * (bw + uiPx(4)), yy, bw, bh, lab, false, false, "Добавить кварк (до трёх)") && build.size() < 3) { build.push_back(f + 10 * a); qkReset(3); }
            }
            yy += bh + uiPx(4);
        }
        if (uiButton(1920, cx, yy, cw, bh, "очистить", false, false, "Убрать все кварки")) { build.clear(); qkReset(3); }
        yy += bh + uiPx(6);
        const HadInfo hi = qkIdentify(build);
        if (!build.empty() && !hi.name[0]) yy += drawWrapped(fontXS, cx, yy, cw, "Такого адрона нет: одиночный кварк, два кварка или кварк с двумя антикварками несут цвет, а в природе наблюдаются только бесцветные частицы (конфайнмент).", C_TEXT) + uiPx(8);
    }
    if (scene == 1 && hist.size() > 2) {   // энергия, запасённая в струне: растёт линейно, пока струна не лопнет
        uiSection(cx, yy, cw, "энергия струны σ·r (ГэВ)");
        PR in{cx, yy, cw, uiPx(100)}; boxPanel(in.x, in.y, in.w, in.h, C_PANEL2, C_LINE);
        drawSeries(in, hist, 0, 1.8, C_TEXT_HI, 1.5f, LS_SOLID, 400);
        yy += in.h + uiPx(4); yy += drawWrapped(fontXS, cx, yy, cw, "растёт как у пружины, но с постоянной силой; обрыв — когда энергии хватает на пару кварк–антикварк (~1.1 ГэВ)", C_DIM) + uiPx(8);
        // потенциал Корнелла V(r) = σr − (4/3)αs·ħc/r
        uiSection(cx, yy, cw, "потенциал кварк–антикварк V(r)");
        PR pv{cx, yy, cw, uiPx(100)}; boxPanel(pv.x, pv.y, pv.w, pv.h, C_PANEL2, C_LINE);
        std::vector<float> v(120); for (int k = 0; k < 120; k++) { const double r = 0.05 + 1.95 * k / 119.0; v[k] = (float)(SIGMA * r - 4.0 / 3 * ALPHAS * HBARC / r); }
        drawSeries(pv, v, -1.0, 2.0, C_TEXT_HI, 1.5f, LS_SOLID, -1);
        if (!strs.empty()) { const double r = qkStringLen(strs[0]); const float px = mapX(pv, r, 0.05, 2.0); lineV(px, pv.y, pv.y + pv.h, withA(C_ACC, 0.8f)); }
        yy += pv.h + uiPx(4); yy += drawWrapped(fontXS, cx, yy, cw, "0…2 фм: вблизи — как кулон (−1/r), дальше — прямая; вертикаль — длина струны сейчас", C_DIM) + uiPx(8);
    }
    static const char* NOTES[5] = {
        "Кварки внутри протона движутся почти со скоростью света; масса протона (938 МэВ) — в основном энергия глюонного поля и движения кварков, "
        "а не масса самих u и d. Глюон переносит «цвет»: пролетая между кварками, он перекрашивает их, но у адрона цвета всегда в сумме дают белый.",
        "Электрические заряды притягиваются всё слабее с расстоянием, а кварки — с постоянной силой около 15 тонн. Энергия растёт с длиной струны, "
        "и когда её хватает на новую пару кварк–антикварк, струна рвётся — вместо свободного кварка получаются два мезона.",
        "Упрощённая модель струн, как в генераторах событий Большого адронного коллайдера: кварки разлетаются почти со скоростью света, струны "
        "между ними рвутся снова и снова, и каждый кусок становится мезоном (π, K). Поэтому детекторы видят не кварки, а узкие струи адронов.",
        "Заряд u-кварка +2/3, d и s — −1/3. Три кварка трёх разных цветов или кварк с антикварком одного цвета — бесцветны: это барионы (протон, нейтрон, Λ, Ω) "
        "и мезоны (π, K, J/ψ). Масса и название — справочные (Particle Data Group).",
        "Слабое взаимодействие меняет «аромат» кварка: d (−1/3) → u (+2/3) с испусканием W⁻-бозона массой 80 ГэВ. Такой тяжёлый бозон живёт 3·10⁻²⁵ с "
        "и сразу превращается в электрон и антинейтрино. Свободный нейтрон живёт в среднем 15 минут; здесь время сжато."};
    yy += drawWrapped(fontXS, cx, yy, cw, NOTES[clampv(scene, 0, 4)], C_DIM) + uiPx(12);
    scrollEnd(6, yy);
}
static std::string qkReport() {
    using namespace qk;
    switch (scene) {
    case 0: case 4: { int q3 = 0; for (int k = 0; k < 3 && k < (int)q.size(); k++) q3 += FQ3[q[k].f]; return fmt("t = %s · кварков %d, заряд %+d, узел струны на (%.2f, %.2f, %.2f) фм", qkTimeStr().c_str(), (int)q.size(), q3 / 3, J[0] - L / 2, J[1] - L / 2, J[2] - L / 2); }
    case 1: return fmt("t = %s · разрывов %d, мезонов %d", qkTimeStr().c_str(), breaks, (int)strs.size());
    case 2: return fmt("t = %s · адронов %d, разрывов струн %d", qkTimeStr().c_str(), (int)out.size(), breaks);
    default: { const HadInfo hi = qkIdentify(build); return fmt("состав %d кварка(ов): %s", (int)build.size(), hi.name[0] ? hi.name : "не бывает"); }
    }
}

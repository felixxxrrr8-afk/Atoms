// ===================================== ENERGIES ========================================
struct Energies {
    double ek = 0, enb = 0, egrav = 0, ebond = 0, epist = 0, enh = 0, vir = 0, amax = 0; int amaxI = -1;
    double efo = 0;                           // потенциальная энергия атомов в полях объектов (притягатели, барьеры…)
    double Pavg = 0; bool pavgInit = false;   // давление, усреднённое по времени (τ = 0.5)
    double fwall = 0, fpiston = 0;
    double T = 0, Pvir = 0, Pwall = 0, P = 0;
    int nmob = 0, ndof = 0, capped = 0;
    double total() const { return ek + enb + egrav + ebond + epist + enh + efo; }
} EN;
struct ChemStats { long long assoc = 0, exch = 0, diss = 0; double heat = 0; } CH;   // счётчики реакций
static double Wext = 0, Eref = 0;     // работа внешних сил/термостатов и опорная энергия для дрейфа
static double heatWallQ[2] = {0, 0};  // теплота, переданная веществу горячей [0] и холодной [1] тепловыми стенками (ε, нарастающим итогом)
constexpr int OMP_MIN = 500;          // простые циклы короче идут в одном потоке: запуск потоков дороже самой работы
static bool energyRefValid = false;
static bool conserving = true;        // false в режиме NPT (баростат не сохраняет энергию)

// «пинцет»
static int grabbed = -1; static double grabX = 0, grabY = 0, grabZ = 0;
static int followAtom = -1;             // камера следит за этим атомом (двойной клик)
// кисти (нагрев/охлаждение, ластик, вспышка) действуют на атомы вблизи луча взгляда через курсор
static int heatBrush = 0; static double brushO[3] = {0, 0, -1}, brushD[3] = {0, 0, 1};
static double brushF[3] = {0, 0, 1}, brushCut = -1e30;   // при разрезе кисть действует только за плоскостью
static inline double rayDist2(double px, double py, double pz, const double* o, const double* d) {
    double wx = px - o[0], wy = py - o[1], wz = pz - o[2], t = wx * d[0] + wy * d[1] + wz * d[2];
    return wx * wx + wy * wy + wz * wz - t * t;
}
// поршень, который тянут мышью
static bool pistonGrab = false; static double pistonTarget = 0;

// ===================================== ОБЪЕКТЫ ПОЛЯ: интерфейс для UI =====================
// Смысл полей FieldObj по видам (strength — «сила» в единицах, указанных в FO_UNITS):
//   FO_ATTRACT  гладкая яма U = −A(1 − r²/R²)³ при r < R, A = strength (ε) — консервативная сила
//   FO_REPEL    гладкий холм U = +A(1 − r²/R²)² (A = strength, ε): при A ≫ kT — непроницаемый шар
//   FO_TRAP     узкая гармоническая ловушка («оптический пинцет»): U = −A(1 − r²/R²)², k = 4A/R² у дна
//   FO_HEATER / FO_COOLER — локальный термостат Ланжевена к Tset внутри R, strength = частота столкновений γ (1/τ)
//   FO_WIND     постоянная сила strength (ε/σ) вдоль (dx,dy,dz) в шаре радиуса R (гладкий край)
//   FO_VORTEX   вихрь: тангенциальная сила strength·(ρ/R)(1−ρ²/R²)² вокруг оси (dx,dy,dz)
//   FO_EMITTER  источник: strength атомов типа elem за τ, скорость «сопла» √(5kTset/m) вдоль (dx,dy,dz)
//   FO_SINK     сток: атомы (и небольшие молекулы) внутри R удаляются с частотой strength (1/τ)
//   FO_BARRIER  непроницаемая мягкая стенка — пластина в плоскости через (x,y,z) с нормалью (dx,dy,dz):
//               все точки плоскости на расстоянии ≤ R от отрезка (x,y,z)–(x2,y2,z2)
//               (отрезок нулевой длины — диск радиуса R; R ≥ 1000 — бесконечная плоскость); strength — жёсткость ε_w
// Консервативные объекты (ATTRACT, REPEL, TRAP, BARRIER) дают потенциальную энергию EN.efo; когда пользователь
// двигает/меняет объект, скачок энергии относится к внешней работе W (баланс E − W сохраняется).
// Новый или вновь включённый объект «набирает силу» за ~1τ (FieldObj::ramp 0 → 1): атомы раздвигаются мягко.
static const char* FO_NAMES[FO_N] = {"притягатель", "отталкиватель", "нагреватель", "охладитель", "ветер", "вихрь",
                                     "ловушка", "источник", "сток", "барьер"};
static const char* FO_HINTS[FO_N] = {
    "Притягатель: гладкая потенциальная яма глубиной A (ε) и радиусом R.\nАтомы собираются в каплю вокруг центра; при kT > A — вырываются",
    "Отталкиватель: гладкий потенциальный «холм» высотой A (ε).\nПри A ≫ kT — непроницаемое препятствие (обтекание, ударные волны)",
    "Нагреватель: локальный термостат Ланжевена — атомы внутри R\nприходят к температуре зоны Tset (теплота учитывается как внешняя)",
    "Охладитель: локальный термостат к низкой Tset —\nна нём конденсируется пар и растут кристаллы",
    "Ветер: постоянная сила вдоль направления в области радиуса R\n(работа силы — внешняя). Создаёт поток и перепад давления",
    "Вихрь: тангенциальная сила вокруг оси — закручивает газ и жидкость",
    "Ловушка: узкая гармоническая яма («оптический пинцет»):\nловит отдельные атомы и зародыши",
    "Источник: испускает атомы выбранного элемента пучком\n(скорость сопла √(5kT/m)), не создавая перекрытий",
    "Сток: удаляет атомы (и небольшие молекулы), попавшие в радиус R.\nВместе с источником — открытая система",
    "Барьер: непроницаемая мягкая стенка-пластина"};
static const char* FO_UNITS[FO_N] = {"ε", "ε", "1/τ", "1/τ", "ε/σ", "ε/σ", "ε", "ат/τ", "1/τ", "ε"};
// диапазон «силы» для слайдера (логарифмический)
static void foStrengthRange(int kind, double& lo, double& hi) {
    switch (kind) {
    case FO_ATTRACT: case FO_TRAP: lo = 0.1; hi = 40; break;
    case FO_REPEL: lo = 0.5; hi = 100; break;
    case FO_HEATER: case FO_COOLER: lo = 0.05; hi = 20; break;
    case FO_WIND: case FO_VORTEX: lo = 0.02; hi = 10; break;
    case FO_EMITTER: lo = 0.1; hi = 200; break;
    case FO_SINK: lo = 0.1; hi = 100; break;
    default: lo = 0.2; hi = 20; break;
    }
}
static inline bool foUsesDir(int k) { return k == FO_WIND || k == FO_EMITTER || k == FO_VORTEX || k == FO_BARRIER; }
static inline bool foUsesT(int k) { return k == FO_HEATER || k == FO_COOLER || k == FO_EMITTER; }
static inline bool foIsSegment(int k) { return k == FO_BARRIER; }
static inline bool foConservative(int k) { return k == FO_ATTRACT || k == FO_REPEL || k == FO_TRAP || k == FO_BARRIER; }
// объект с разумными умолчаниями под размер ящика; (x,y,z) — центр (для барьера — середина)
static FieldObj makeFieldObj(int kind, double x, double y, double z) {
    FieldObj o; o.kind = clampv(kind, 0, FO_N - 1); o.x = x; o.y = y; o.z = z;
    const double Lm = std::min({S.Lx, S.Ly, S.Lz});
    const double Rd = clampv(0.18 * Lm, 2.5, 8.0);
    o.R = Rd; o.dx = 1; o.dy = 0; o.dz = 0;
    switch (o.kind) {
    case FO_ATTRACT: o.strength = 3.0; o.R = 1.3 * Rd; break;
    case FO_REPEL: o.strength = 12.0; o.R = 0.6 * Rd; break;
    case FO_TRAP: o.strength = 8.0; o.R = 2.2; break;
    case FO_HEATER: o.strength = 2.0; o.Tset = std::max(1.5, 3.0 * P.Tset); break;
    case FO_COOLER: o.strength = 2.0; o.Tset = std::max(0.02, 0.25 * P.Tset); break;
    case FO_WIND: o.strength = 0.6; break;
    case FO_VORTEX: o.strength = 1.0; o.dx = 0; o.dy = 1; o.dz = 0; break;   // ось вдоль y
    case FO_EMITTER: o.strength = 5.0; o.R = 1.5; o.Tset = std::max(0.5, P.Tset); o.elem = E_AR; break;
    case FO_SINK: o.strength = 20.0; o.R = 0.5 * Rd; break;
    case FO_BARRIER: {   // вертикальный отрезок/пластина через (x,y,z), нормаль вдоль x
        o.strength = 2.0; const double h = 0.3 * S.Ly;
        o.y = y - h; o.x2 = x; o.y2 = y + h; o.z2 = o.z;
        o.R = 0.3 * S.Lz; break; }
    }
    return o;
}

// ---- страж устойчивости: при не-конечных величинах или скачке T в 50× — откат к последнему хорошему снимку
// physAlert — текст для показа; physAlertTime — момент тревоги по physClock() (секунды); physRollbacks — счётчик откатов
static std::string physAlert; static double physAlertTime = -1e9; static int physRollbacks = 0;
static double physClock() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static inline bool physAlertActive(double sec = 6.0) { return !physAlert.empty() && physClock() - physAlertTime < sec; }

// ===================================== PAIR TABLES ======================================
struct PairP { double sig2, eps4, rc2, shift; };
static PairP PT[NEL][NEL];
static double rcMax = 2.5, rcMax2 = 6.25;
static double rcCoul = cfg::RC_COUL, coulShift = 0, coulFs = 0;   // DSF: erfc(αr_c)/r_c и сдвиг силы на обрезке
static bool anyCharge = false, anyBondable = false, anyMetal = false;
// erfc(x) при x ≥ 0 через общий множитель e^{−x²} (Абрамовиц — Стиган 7.1.26, абсолютная ошибка < 1.5·10⁻⁷)
static inline double erfcE(double x, double e2) {
    const double t = 1.0 / (1.0 + 0.3275911 * x);
    return t * (0.254829592 + t * (-0.284496736 + t * (1.421413741 + t * (-1.453152027 + t * 1.061405429)))) * e2;
}
static double dsfA = cfg::DSF_A, kappaQ = cfg::KAPPA_Q;   // меняются только в проверочных режимах (--water)
// DSF на единичные заряды (U/K) и dU/ds, s = r² — по формуле
static inline double coulDSFexact(double r2, double& dUds) {
    const double r = std::sqrt(r2), x = dsfA * r, e2 = std::exp(-x * x), ec = erfcE(x, e2), ir = 1.0 / r;
    dUds = -0.5 * ((ec * ir + 1.1283791670955126 * dsfA * e2) * ir - coulFs) * ir;
    return ec * ir - coulShift + coulFs * (r - rcCoul);
}
// Кулон считается для сотен тысяч пар за шаг, поэтому — по таблице: значения U и dU/ds в узлах равномерной сетки по s,
// между узлами — кубический сплайн Эрмита. Сила — производная того же сплайна, то есть точно консервативна
// (ошибка самой кривой < 10⁻⁸); ближе 0.3σ — по формуле
namespace dsfT { constexpr int N = 4096; constexpr double S0 = 0.09; struct Node { double u, d; }; static Node T[N + 1]; static double ds = 1, ids = 1; }
static void buildDsfTable() {
    dsfT::ds = (rcCoul * rcCoul - dsfT::S0) / dsfT::N; dsfT::ids = 1 / dsfT::ds;
    for (int k = 0; k <= dsfT::N; k++) dsfT::T[k].u = coulDSFexact(dsfT::S0 + k * dsfT::ds, dsfT::T[k].d);
}
// Кулон DSF для пары с произведением зарядов qq (r < r_c): энергия и ff = −(dU/dr)/r = −2·dU/ds
static inline double coulDSF(double qq, double r2, double& ff) {
    const double k = cfg::K_COUL * qq;
    if (r2 < dsfT::S0) { double d; const double u = coulDSFexact(r2, d); ff = -2 * k * d; return k * u; }
    double t = (r2 - dsfT::S0) * dsfT::ids; int i = (int)t; if (i >= dsfT::N) i = dsfT::N - 1; t -= i;
    const dsfT::Node& a = dsfT::T[i]; const dsfT::Node& b = dsfT::T[i + 1];
    const double d0 = a.d * dsfT::ds, d1 = b.d * dsfT::ds, t2 = t * t, t3 = t2 * t;
    const double u = (2 * t3 - 3 * t2 + 1) * a.u + (t3 - 2 * t2 + t) * d0 + (3 * t2 - 2 * t3) * b.u + (t3 - t2) * d1;
    const double du = ((6 * t2 - 6 * t) * (a.u - b.u) + (3 * t2 - 4 * t + 1) * d0 + (3 * t2 - 2 * t) * d1) * dsfT::ids;
    ff = -2 * k * du; return k * u;
}
// Собственная энергия заряда в методе DSF (Вольф): −K·(erfc(αr_c)/2r_c + α/√π)·Q². Затухание erfc(αr) ослабляет
// кулон на малых расстояниях, а эта поправка возвращает иону энергию его экранирующего окружения (в воде — гидратацию).
// Считается для полного заряда ионов (H3O+, Cl−, Na+…), а не для частичных зарядов нейтральных молекул:
// перенос протона HCl + H2O → H3O+ + Cl− без неё обходился бы на ~3.7 эВ дороже, чем на самом деле
static inline double ionSelfK() { return cfg::K_COUL * (std::erfc(dsfA * rcCoul) / (2 * rcCoul) + dsfA / std::sqrt(PI)); }
// ближайший образ по одной оси: d — разность координат, L — период, iL = 1/L (быстрее std::nearbyint)
static inline double minImg(double d, double L, double iL) { const double s = d * iL; return d - L * (double)(long long)(s + (s >= 0 ? 0.5 : -0.5)); }
static bool present[NEL];
static bool nlValid = false;          // список Верле действителен

// ---- Металлическая связь: многочастичный потенциал второго момента (Гупта / Клери–Розато)
//   U = Σ_i [ ½Σ_j A·f(r_ij) − w_i·√ρ_i ],  ρ_i = Σ_j w_j·ξ²·g(r_ij)
//   f = e^{−p(r/r0−1)}·S(r), g = e^{−2q(r/r0−1)}·S(r), S — гладкое выключение на [1.25·r0, 1.65·r0]
//   w = 1 − (занятая валентность)/(валентность): атом, связанный с неметаллом (окисленный), теряет металлическую связь.
//   Показатели p и q — из подгонки Клери и Розато (Phys. Rev. B 48, 22, 1993) к упругим постоянным; A и ξ подобраны так,
//   что ГЦК-решётка имеет равновесное расстояние r0 = 2·r_мет и реальную энергию когезии E_coh — поэтому металлы
//   плавятся около своих настоящих температур (золото ≈ 1300 K).
namespace gup { constexpr double R1 = 1.25, R2 = 1.65; }
struct GupP { double A = 0, xi2 = 0, p = 10, q = 3, r0 = 1, ir0 = 1, r1 = 0, r2 = 0; };
static GupP GP[NEL][NEL];
static double gupA[NEL], gupXi[NEL], gupP[NEL], gupQ[NEL];
static void tbsmaPQ(int Z, double& p, double& q) {
    switch (Z) {
    case 28: p = 16.999; q = 1.189; return;   // Ni
    case 29: p = 10.960; q = 2.278; return;   // Cu
    case 45: p = 18.450; q = 1.867; return;   // Rh
    case 46: p = 10.867; q = 3.742; return;   // Pd
    case 47: p = 10.928; q = 3.139; return;   // Ag
    case 77: p = 16.980; q = 2.691; return;   // Ir
    case 78: p = 10.612; q = 4.004; return;   // Pt
    case 79: p = 10.229; q = 4.036; return;   // Au
    case 13: p = 8.612; q = 2.516; return;    // Al
    case 82: p = 9.576; q = 3.648; return;    // Pb
    case 22: p = 11.418; q = 1.643; return;   // Ti
    case 27: p = 11.604; q = 2.286; return;   // Co
    }
    p = 10.0; q = 3.0;
}
static inline double taper(double r, double r1, double r2, double& dS) {   // S = 1 − 10t³ + 15t⁴ − 6t⁵
    if (r <= r1) { dS = 0; return 1; }
    if (r >= r2) { dS = 0; return 0; }
    double L = r2 - r1, t = (r - r1) / L, t2 = t * t, u = 1 - t;
    dS = -30 * t2 * u * u / L;
    // та же функция в виде (1−t)³(1 + 3t + 6t²): без вычитания близких чисел S ≥ 0 точно, иначе у края ρ < 0 и √ρ = NaN
    return u * u * u * (1 + 3 * t + 6 * t2);
}
static inline void gupFG(const GupP& g, double r, double& f, double& df, double& h, double& dh) {
    double dS, S = taper(r, g.r1, g.r2, dS), x = r * g.ir0 - 1;
    double e1 = std::exp(-g.p * x), e2 = std::exp(-2 * g.q * x);
    f = e1 * S; df = e1 * (dS - g.p * g.ir0 * S);
    h = e2 * S; dh = e2 * (dS - 2 * g.q * g.ir0 * S);
}
static void calibrateGupta(double r0, double Ecoh, double p, double q, double& A, double& xi) {
    // оболочки ГЦК-решётки (в долях расстояния до ближайших соседей) и их заселённости
    const double sh[5] = {1.0, std::sqrt(2.0), std::sqrt(3.0), 2.0, std::sqrt(5.0)}; const int nsh[5] = {12, 6, 24, 12, 24};
    GupP g; g.p = p; g.q = q; g.r0 = r0; g.ir0 = 1 / r0; g.r1 = gup::R1 * r0; g.r2 = gup::R2 * r0;
    auto sums = [&](double s, double& R, double& G) { R = G = 0; for (int k = 0; k < 5; k++) { double f, df, h, dh; gupFG(g, s * r0 * sh[k], f, df, h, dh); R += nsh[k] * f; G += nsh[k] * h; } };
    double R, G, Rp, Gp, Rm, Gm, e = 1e-5; sums(1, R, G); sums(1 + e, Rp, Gp); sums(1 - e, Rm, Gm);
    double dR = (Rp - Rm) / (2 * e), dG = (Gp - Gm) / (2 * e);
    // энергия на атом E(s) = ½A·R − ξ√G (каждая пара делится между двумя атомами, как в metalForces);
    // dE/ds = 0 при s = 1 и E(1) = −E_coh
    xi = Ecoh / (std::sqrt(G) - dG * R / (2 * std::sqrt(G) * dR));
    A = xi * dG / (std::sqrt(G) * dR);
}
static void buildGupta() {
    for (int a = 0; a < NEL; a++) {
        gupA[a] = gupXi[a] = 0; tbsmaPQ(EL[a].Z, gupP[a], gupQ[a]);
        if (EL[a].metal && EL[a].ecoh > 0) calibrateGupta(2 * EL[a].rmet / 3.405, EL[a].ecoh * cfg::EV, gupP[a], gupQ[a], gupA[a], gupXi[a]);
    }
    for (int a = 0; a < NEL; a++) for (int b = 0; b < NEL; b++) {
        GupP g;
        if (gupXi[a] > 0 && gupXi[b] > 0) {   // смешанные пары: средние показатели, среднее геометрическое множителей
            g.A = std::sqrt(gupA[a] * gupA[b]); g.xi2 = gupXi[a] * gupXi[b]; g.p = 0.5 * (gupP[a] + gupP[b]); g.q = 0.5 * (gupQ[a] + gupQ[b]);
            g.r0 = (EL[a].rmet + EL[b].rmet) / 3.405; g.ir0 = 1 / g.r0; g.r1 = gup::R1 * g.r0; g.r2 = gup::R2 * g.r0;
        }
        GP[a][b] = g;
    }
}
static inline bool isMetalT(int t) { return gupXi[t] > 0; }

static void buildPairTables() {
    for (int a = 0; a < NEL; a++) for (int b = 0; b < NEL; b++) {
        // правило Лоренца–Бертло: σ_ij = (σ_i+σ_j)/2, ε_ij = √(ε_i ε_j)
        double sig = 0.5 * (EL[a].sig + EL[b].sig);
        if (EL[a].metal != EL[b].metal && !EL[a].fixed && !EL[b].fixed) sig *= 0.85;   // контакт металл–неметалл короче ван-дер-ваальсова
        double eps = std::sqrt(EL[a].eps * EL[b].eps) * P.epsScale;
        double rcf = std::min(EL[a].rcf, EL[b].rcf);
        double rc = std::min(rcf * sig, cfg::RC_LJ_MAX);
        double sr6 = std::pow(sig / rc, 6.0);
        PairP& p = PT[a][b];
        p.sig2 = sig * sig; p.eps4 = 4 * eps; p.rc2 = rc * rc;
        p.shift = 4 * eps * (sr6 * sr6 - sr6);   // сдвиг: U(rc) = 0
        if (EL[a].metal && EL[b].metal) { p.eps4 = 0; p.rc2 = 0; p.shift = 0; }   // металл–металл: потенциал Гупты вместо LJ
    }
    buildGupta();
    if (P.epsScale != 1.0) for (int a = 0; a < NEL; a++) for (int b = 0; b < NEL; b++) { GP[a][b].A *= P.epsScale; GP[a][b].xi2 *= P.epsScale * P.epsScale; }
    // явные параметры пар вместо смешивания (NBFIX): H···H между молекулами — размер реальной H2 (≈2.1 Å);
    // водород у электроотрицательных атомов (O, N, F, Cl, S) почти «голый», как в SPC/Fw: водородную связь
    // H···O ≈ 1.8 Å держит притяжение зарядов. Ядро — только отталкивание (WCA, σ = 1.55 Å, действует ближе 1.74 Å):
    // без него протон под притяжением чужого кислорода соскальзывал бы к нему, растягивая свою связь Морзе;
    // в природе этому мешает отталкивание электронных оболочек, а переносит протон только химическое событие.
    auto nbfix = [](int a, int b, double sig, double eps, double rcf) {
        if (a < 0 || b < 0) return;
        eps *= P.epsScale; double rc = rcf * sig, sr6 = std::pow(sig / rc, 6.0);
        PairP p{sig * sig, 4 * eps, rc * rc, 4 * eps * (sr6 * sr6 - sr6)}; PT[a][b] = p; PT[b][a] = p;
    };
    nbfix(E_H, E_H, 0.62, 0.05, 2.5);
    for (int t : std::initializer_list<int>{E_O, E_N, E_CL, E_CLM, E_F, typeOfZ(16), typeOfZ(35), typeOfZ(53)}) nbfix(E_H, t, 1.55 / 3.405, 10.0, 1.12246);
    const double x = dsfA * rcCoul, e2 = std::exp(-x * x), ec = erfcE(x, e2);
    coulShift = ec / rcCoul;
    coulFs = ec / (rcCoul * rcCoul) + 1.1283791670955126 * dsfA * e2 / rcCoul;
    buildDsfTable();
    nlValid = false;
}
// Пересчитать, какие типы присутствуют, радиус обрезки и базовый шаг
static void updatePresence() {
    for (int k = 0; k < NEL; k++) present[k] = false;
    anyCharge = false; anyBondable = false; anyMetal = false;
    int cnt[NEL] = {0};
    for (int i = 0; i < S.n; i++) {
        present[S.ty[i]] = true; cnt[S.ty[i]]++;
        if (EL[S.ty[i]].fq != 0) anyCharge = true;
        if (isMetalT(S.ty[i])) anyMetal = true;
    }
    // химия возможна, если есть пара присутствующих типов, способных связаться; заряды — если у такой пары разная χ
    for (int a = 0; a < NEL; a++) if (present[a] && EL[a].val > 0) for (int b = a; b < NEL; b++) if (present[b] && BT[a][b].maxOrder > 0) {
        if (a == b && cnt[a] < 2) continue;
        anyBondable = true; if (EL[a].chi != EL[b].chi) anyCharge = true;
    }
    for (int i = 0; i < S.n && !anyBondable; i++) if (S.nbc[i]) anyBondable = true;   // уже есть связи (например, загруженные)
    if (anyBondable) for (int i = 0; i < S.n && !anyCharge; i++) if (S.q[i] != 0) anyCharge = true;
    rcMax = 1.0;
    for (int a = 0; a < NEL; a++) if (present[a]) for (int b = 0; b < NEL; b++) if (present[b]) {
        if (EL[a].fixed && EL[b].fixed) continue;
        if (a == b && cnt[a] < 2) continue;   // одна броуновская частица не взаимодействует сама с собой

        rcMax = std::max(rcMax, std::sqrt(PT[a][b].rc2));
        if (GP[a][b].xi2 > 0) rcMax = std::max(rcMax, GP[a][b].r2);
    }
    if (anyCharge) rcMax = std::max(rcMax, rcCoul);
    rcMax2 = rcMax * rcMax;
    P.dtBase = anyBondable ? cfg::DT_CHEM : cfg::DT_LJ;
    if (P.dt > P.dtBase || P.dt <= 0) P.dt = P.dtBase;
    nlValid = false;
}

// ===================================== TOPOLOGY ========================================
static inline bool bonded(int i, int j) {
    for (int k = 0; k < S.nbc[i]; k++) if (S.nb[i][k] == j) return true;
    return false;
}
static inline int bondSlot(int i, int j) {
    for (int k = 0; k < S.nbc[i]; k++) if (S.nb[i][k] == j) return k;
    return -1;
}
static inline int bondOrder(int i, int j) { int k = bondSlot(i, j); return k < 0 ? 0 : S.bo[i][k]; }
static inline bool isGhost(int i, int j) { for (int k = 0; k < S.ghc[i]; k++) if (S.gh[i][k] == j) return true; return false; }
static void addGhost(int i, int j) {
    if (isGhost(i, j) || S.ghc[i] >= 6 || S.ghc[j] >= 6) return;
    S.gh[i][S.ghc[i]++] = j; S.gh[j][S.ghc[j]++] = i;
}
static void removeGhost(int i, int j) {
    for (int k = 0; k < S.ghc[i]; k++) if (S.gh[i][k] == j) { S.gh[i][k] = S.gh[i][--S.ghc[i]]; break; }
    for (int k = 0; k < S.ghc[j]; k++) if (S.gh[j][k] == i) { S.gh[j][k] = S.gh[j][--S.ghc[j]]; break; }
}
// исключения 1-2 и 1-3 (атомы одной молекулы не взаимодействуют через LJ/Кулон на коротком расстоянии)
// + «призраки»: только что разорванная пара не отталкивается, пока не разойдётся до минимума LJ
static inline bool mayExclude(int i, int j) { return S.ghc[i] || (S.nbc[i] && S.nbc[j]); }
static inline bool excluded(int i, int j) {
    if (S.ghc[i] && isGhost(i, j)) return true;
    if (!S.nbc[i] || !S.nbc[j]) return false;
    if (bonded(i, j)) return true;
    for (int k = 0; k < S.nbc[i]; k++) if (bonded(S.nb[i][k], j)) return true;
    return false;
}
// пара 1-4 (через три связи): её LJ и Кулон ослаблены вдвое, как в силовых полях OPLS и AMBER, — иначе притяжение
// водорода OH к соседнему кислороду той же молекулы сминало бы углы (H3PO4, H2SO4, сахара)
constexpr double SCALE14 = 0.5;
static inline bool pair14(int i, int j) {
    for (int k = 0; k < S.nbc[i]; k++) { const int a = S.nb[i][k]; for (int l = 0; l < S.nbc[j]; l++) if (bonded(a, S.nb[j][l])) return true; }
    return false;
}
static inline int usedVal(int i) { int s = 0; for (int k = 0; k < S.nbc[i]; k++) s += S.bo[i][k]; return s; }
// свободная валентность («радикальные» места): ниже обычной валентности — сколько связей не хватает; у атома
// с расширенным октетом — есть ли неспаренный электрон (SF3·, PCl4·, ClF2·), у насыщенного — 0; у перегруженного — < 0
static inline int freeVal(int i) {
    const int t = S.ty[i], v = EL[t].val, u = usedVal(i);
    if (u < v || !HYPER[t]) return v - u;
    return u < valMax(t) ? ((VE[t] - u) & 1) : v - u + (valMax(t) - v);
}
// насыщенный атом j может «распарить» неподелённую пару и принять связь от радикала-партнёра (SF2 + F· → SF3·):
// только p-элементы 3-го периода и ниже, только с партнёром электроотрицательнее самого атома и если уже связанные
// соседи тоже электроотрицательны (SF4, PCl5, ClO2, ICl3, H2SO4 существуют, а H2SF, PH3Cl2 и цепочки Cl3, Br3 — нет)
static inline bool canExpand(int j, int partner) {
    const int t = S.ty[j], u = usedVal(j); const double cp = EL[S.ty[partner]].chi;
    if (!HYPER[t] || u < EL[t].val || u >= valMax(t) || ((VE[t] - u) & 1) != 0 || S.nbc[j] >= cfg::MAXB || cp < 2.9 || cp < EL[t].chi + 0.25) return false;
    for (int k = 0; k < S.nbc[j]; k++) if (EL[S.ty[S.nb[j][k]]].chi < 2.5) return false;
    return true;
}
// сколько связей атома сверх обычной валентности (у гипервалентных — «цена» EPROM за каждую)
static inline int hyperOver(int i) { const int t = S.ty[i]; return HYPER[t] ? std::max(0, usedVal(i) - EL[t].val) : 0; }

static void updateCharge(int i) {
    const Element& e = EL[S.ty[i]];
    double q = e.fq;
    // приращение заряда на связь κ·Δχ·min(1, |Δχ|): у сильно полярных связей (O–H, H–F) — линейно по Δχ, у слабо полярных
    // квадратично — C–H даёт ≈ 0.04 e, C–O 0.28 e, как инкременты связей в силовых полях MMFF94 и OPLS
    if (e.chi > 0) for (int k = 0; k < S.nbc[i]; k++) { const double d = EL[S.ty[S.nb[i][k]]].chi - e.chi; q += kappaQ * d * std::min(1.0, std::fabs(d)); }
    S.q[i] = q;
}
// изменить порядок связи i–j на d (+1 / −1); при нуле связь удаляется
static void changeBond(int i, int j, int d) {
    int ki = bondSlot(i, j), kj = bondSlot(j, i);
    if (ki < 0) {
        if (d <= 0 || S.nbc[i] >= cfg::MAXB || S.nbc[j] >= cfg::MAXB) return;
        S.nb[i][S.nbc[i]] = j; S.bo[i][S.nbc[i]] = (unsigned char)d; S.bc[i][S.nbc[i]] = 0; S.nbc[i]++;
        S.nb[j][S.nbc[j]] = i; S.bo[j][S.nbc[j]] = (unsigned char)d; S.bc[j][S.nbc[j]] = 0; S.nbc[j]++;
        return;
    }
    int o = S.bo[i][ki] + d;
    if (o > 0) { S.bo[i][ki] = (unsigned char)o; S.bo[j][kj] = (unsigned char)o; return; }
    int li = S.nbc[i] - 1; S.nb[i][ki] = S.nb[i][li]; S.bo[i][ki] = S.bo[i][li]; S.bc[i][ki] = S.bc[i][li]; S.nbc[i]--;
    int lj = S.nbc[j] - 1; S.nb[j][kj] = S.nb[j][lj]; S.bo[j][kj] = S.bo[j][lj]; S.bc[j][kj] = S.bc[j][lj]; S.nbc[j]--;
}
static void removeAllBonds(int i) {
    while (S.nbc[i] > 0) { int j = S.nb[i][0]; while (bondSlot(i, j) >= 0) changeBond(i, j, -1); updateCharge(j); }
    updateCharge(i);
}

// вектор r_j − r_i с минимальным образом (периодические границы)
static inline void dvec(int i, int j, double& dx, double& dy, double& dz) {
    dx = S.x[j] - S.x[i]; dy = S.y[j] - S.y[i]; dz = S.z[j] - S.z[i];
    if (P.boundary == B_PERIODIC) { dx = minImg(dx, S.Lx, 1.0 / S.Lx); dy = minImg(dy, S.Ly, 1.0 / S.Ly); dz = minImg(dz, S.Lz, 1.0 / S.Lz); }
}
static inline double dist2(int i, int j) { double dx, dy, dz; dvec(i, j, dx, dy, dz); return dx * dx + dy * dy + dz * dz; }

// Атом не движется: неподвижный по природе (стенка) или закреплённый пользователем (S.pin).
// Интегратор его не сдвигает, скорость = 0, в кинетическую энергию, T и число степеней свободы он не входит.
static inline bool frozenAt(int i) { return S.pin[i] != 0 || EL[S.ty[i]].fixed; }
// закрепить / открепить атом; кинетическая энергия закрепляемого атома уходит во внешнюю работу W
static void setPin(int i, bool on) {
    if (i < 0 || i >= S.n) return;
    if (on && !S.pin[i] && !EL[S.ty[i]].fixed) {
        Wext -= 0.5 * EL[S.ty[i]].m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]);
        S.vx[i] = S.vy[i] = S.vz[i] = 0;
    }
    S.pin[i] = on ? 1 : 0;
}

// ===================================== ATOMS ==========================================
static int addAtom(int type, double x, double y, double z, double vx, double vy, double vz) {
    int i = S.n; S.resize(i + 1); S.n = i + 1;
    if (EL[type].fixed) vx = vy = vz = 0;
    S.x[i] = x; S.y[i] = y; S.z[i] = z; S.vx[i] = vx; S.vy[i] = vy; S.vz[i] = vz;
    S.fx[i] = S.fy[i] = S.fz[i] = 0; S.bx[i] = S.by[i] = S.bz[i] = 0; S.ux[i] = x; S.uy[i] = y; S.uz[i] = z;
    S.ty[i] = type; S.nbc[i] = 0; S.ghc[i] = 0; S.ep[i] = 0; S.q[i] = EL[type].fq; S.lpk[i] = 0;
    nlValid = false;
    return i;
}
static void removeAtom(int i) {
    removeAllBonds(i);
    while (S.ghc[i]) removeGhost(i, S.gh[i][0]);
    int last = S.n - 1;
    if (i != last) {
        // переместить последний атом на место i и исправить ссылки у его партнёров
        S.gh[i] = S.gh[last]; S.ghc[i] = S.ghc[last]; S.bc[i] = S.bc[last];
        for (int k = 0; k < S.ghc[i]; k++) { int j = S.gh[i][k]; for (int m = 0; m < S.ghc[j]; m++) if (S.gh[j][m] == last) S.gh[j][m] = i; }
        for (auto* v : {&S.x, &S.y, &S.z, &S.vx, &S.vy, &S.vz, &S.fx, &S.fy, &S.fz, &S.bx, &S.by, &S.bz, &S.ux, &S.uy, &S.uz, &S.q, &S.ep}) (*v)[i] = (*v)[last];
        S.ty[i] = S.ty[last]; S.nb[i] = S.nb[last]; S.bo[i] = S.bo[last]; S.nbc[i] = S.nbc[last]; S.pin[i] = S.pin[last];
        S.lp[i] = S.lp[last]; S.lpk[i] = S.lpk[last];
        for (int k = 0; k < S.nbc[i]; k++) { int j = S.nb[i][k]; int s = bondSlot(j, last); if (s >= 0) S.nb[j][s] = i; }
    }
    if (grabbed == i) grabbed = -1;
    else if (grabbed == last) grabbed = i;
    if (followAtom == i) followAtom = -1;
    else if (followAtom == last) followAtom = i;
    S.n = last; S.resize(last);
    nlValid = false;
}
// все атомы молекулы (компонента связности по химическим связям)
static void moleculeOf(int i, std::vector<int>& out) {
    out.clear(); out.push_back(i);
    for (size_t k = 0; k < out.size(); k++) {
        int a = out[k];
        for (int m = 0; m < S.nbc[a]; m++) { int b = S.nb[a][m]; if (std::find(out.begin(), out.end(), b) == out.end()) out.push_back(b); }
    }
}
static void removeMolecule(int i) {
    std::vector<int> m; moleculeOf(i, m);
    std::sort(m.begin(), m.end(), std::greater<int>());   // по убыванию: перестановка «последний → i» не задевает оставшиеся
    for (int a : m) removeAtom(a);
}

// ===================================== CELL LIST + VERLET LIST ==========================
// Сетка ячеек (размер ≥ половины r_c + skin, соседи — в кубе 5×5×5 ячеек) → список соседей Верле для каждого атома.
// Список пересобирается, только когда какой-то атом сместился больше чем на skin/2.
static std::vector<int> cellStart, cellAtoms, cellOf;
static int cnx = 1, cny = 1, cnz = 1;
static void buildCells(double cs) {
    int nx = std::max(1, (int)(S.Lx / cs)), ny = std::max(1, (int)(S.Ly / cs)), nz = std::max(1, (int)(S.Lz / cs));
    nx = std::min(nx, 120); ny = std::min(ny, 120); nz = std::min(nz, 120);
    cnx = nx; cny = ny; cnz = nz;
    const int nc = nx * ny * nz, n = S.n;
    cellStart.assign(nc + 1, 0); cellAtoms.resize(n); cellOf.resize(n);
    const double sx = nx / S.Lx, sy = ny / S.Ly, sz = nz / S.Lz;
    for (int i = 0; i < n; i++) {
        int ix = clampv((int)(S.x[i] * sx), 0, nx - 1), iy = clampv((int)(S.y[i] * sy), 0, ny - 1);
        int iz = clampv((int)(S.z[i] * sz), 0, nz - 1);
        int c = (iz * ny + iy) * nx + ix; cellOf[i] = c; cellStart[c + 1]++;
    }
    for (int c = 0; c < nc; c++) cellStart[c + 1] += cellStart[c];
    std::vector<int> fill(cellStart.begin(), cellStart.end() - 1);
    for (int i = 0; i < n; i++) cellAtoms[fill[cellOf[i]]++] = i;
}
static std::vector<int> nlStart, nlIdx, nlCnt;
static std::vector<double> nlRX, nlRY, nlRZ;
static int nlN = -1, nlRebuilds = 0;
static double nlAffine = 1.0;   // накопленное аффинное масштабирование (баростат) со времени сборки списка
static void buildNeighborList() {
    const double rl = rcMax + cfg::SKIN, rl2 = rl * rl;
    buildCells(0.5 * rl);
    const int n = S.n, nx = cnx, ny = cny, nz = cnz; const bool per = isPer();
    const double Lx = S.Lx, Ly = S.Ly, Lz = S.Lz, iLx = 1 / Lx, iLy = 1 / Ly, iLz = 1 / Lz;
    nlCnt.assign(n, 0); nlStart.assign(n + 1, 0);
    // ячейки-соседи по одной оси: ±2 от своей; в периодическом ящике меньше пяти ячеек — все (без повторов)
    auto around = [per](int ic, int nn, int* out) {
        int m = 0;
        if (per && nn < 5) { for (int k = 0; k < nn; k++) out[m++] = k; return m; }
        for (int d = -2; d <= 2; d++) { int j = ic + d; if (per) j = (j + nn) % nn; else if (j < 0 || j >= nn) continue; out[m++] = j; }
        return m;
    };
    // один проход: каждый поток собирает соседей своего куска атомов, затем куски склеиваются по порядку
    const int nt = std::max(1, omp_get_max_threads());
    static std::vector<std::vector<int>> part; if ((int)part.size() < nt) part.resize(nt);
#pragma omp parallel for schedule(static, 1)
    for (int t = 0; t < nt; t++) {
        const int a0 = (int)((long long)n * t / nt), a1 = (int)((long long)n * (t + 1) / nt);
        std::vector<int>& out = part[t]; out.clear();
        for (int i = a0; i < a1; i++) {
            const size_t before = out.size();
            const bool fi = EL[S.ty[i]].fixed; const double xi = S.x[i], yi = S.y[i], zi = S.z[i];
            const int c = cellOf[i], cx = c % nx, cy = (c / nx) % ny, cz = c / (nx * ny);
            int ox[5], oy[5], oz[5]; const int mx = around(cx, nx, ox), my = around(cy, ny, oy), mz = around(cz, nz, oz);
            for (int a = 0; a < mz; a++) for (int b = 0; b < my; b++) for (int d = 0; d < mx; d++) {
                const int cc = (oz[a] * ny + oy[b]) * nx + ox[d];
                for (int p = cellStart[cc]; p < cellStart[cc + 1]; p++) {
                    const int j = cellAtoms[p]; if (j == i || (fi && EL[S.ty[j]].fixed)) continue;
                    double dx = S.x[j] - xi, dy = S.y[j] - yi, dz = S.z[j] - zi;
                    if (per) { dx = minImg(dx, Lx, iLx); dy = minImg(dy, Ly, iLy); dz = minImg(dz, Lz, iLz); }
                    if (dx * dx + dy * dy + dz * dz < rl2) out.push_back(j);
                }
            }
            nlCnt[i] = (int)(out.size() - before);
        }
    }
    for (int i = 0; i < n; i++) nlStart[i + 1] = nlStart[i] + nlCnt[i];
    nlIdx.resize(std::max(1, nlStart[n]));
#pragma omp parallel for schedule(static, 1)
    for (int t = 0; t < nt; t++) {
        const int a0 = (int)((long long)n * t / nt);
        if (!part[t].empty()) memcpy(nlIdx.data() + nlStart[a0], part[t].data(), part[t].size() * sizeof(int));
    }
    nlRX = S.ux; nlRY = S.uy; nlRZ = S.uz; nlN = n; nlValid = true; nlRebuilds++; nlAffine = 1.0;
}
// наибольшее смещение атома (в квадрате) со времени сборки списка соседей
static double nlMaxDisp2() {
    if (!nlValid || nlN != S.n) return 1e30;
    double m = 0;
    for (int i = 0; i < S.n; i++) { const double dx = S.ux[i] - nlRX[i], dy = S.uy[i] - nlRY[i], dz = S.uz[i] - nlRZ[i]; m = std::max(m, dx * dx + dy * dy + dz * dz); }
    return m;
}
static void ensureNeighborList() {
    bool need = !nlValid || nlN != S.n;
    // сжатие ящика баростатом сближает все пары на (1 − μ)·r_list — это съедает часть «кожи»
    const double half = 0.5 * (cfg::SKIN - std::max(0.0, 1.0 - nlAffine) * (rcMax + cfg::SKIN));
    if (half <= 0.02) need = true;
    if (!need) {
        const double lim = half * half; int bad = 0;
#pragma omp parallel for reduction(| : bad) if (S.n > OMP_MIN)
        for (int i = 0; i < S.n; i++) {
            double dx = S.ux[i] - nlRX[i], dy = S.uy[i] - nlRY[i], dz = S.uz[i] - nlRZ[i];
            if (dx * dx + dy * dy + dz * dz > lim) bad = 1;
        }
        need = bad != 0;
    }
    if (need) buildNeighborList();
}

// ===================================== FORCES ==========================================
// Энергия пары (LJ + Кулон DSF), с учётом исключений; r2 = |r_ij|²
static inline double pairEnergy(int i, int j, double r2) {
    if (r2 > rcMax2 || r2 < 1e-12) return 0;
    if (mayExclude(i, j) && excluded(i, j)) return 0;
    const PairP& pp = PT[S.ty[i]][S.ty[j]];
    double e = 0;
    if (r2 < pp.rc2) { double sr2 = pp.sig2 / r2, sr6 = sr2 * sr2 * sr2; e += pp.eps4 * (sr6 * sr6 - sr6) - pp.shift; }
    double qq = S.q[i] * S.q[j];
    if (qq != 0 && r2 < rcCoul * rcCoul) { double ff; e += coulDSF(qq, r2, ff); }
    if (S.nbc[i] && S.nbc[j] && pair14(i, j)) e *= SCALE14;
    return e;
}
// Потенциал стенки 9-3 (интеграл LJ по полупространству): U = ε_w[(2/15)(σ/d)^9 − (σ/d)^3]
static inline void wallTerm(double d, double s, double& U, double& F) {
    U = 0; F = 0;
    bool attr = P.wallAttr > 0.01;
    double ew = attr ? 1.5 * P.wallAttr : 1.0, dc = attr ? 2.5 * s : 0.8584 * s;  // 0.8584σ — минимум, WCA-обрезка
    if (d >= dc) return;
    if (d < 0.25 * s) d = 0.25 * s;
    double a = s / d, a3 = a * a * a, a9 = a3 * a3 * a3;
    double b = s / dc, b3 = b * b * b, b9 = b3 * b3 * b3;
    U = ew * ((2.0 / 15.0) * a9 - a3) - ew * ((2.0 / 15.0) * b9 - b3);
    F = ew * (1.2 * a9 - 3.0 * a3) / d;   // F = −dU/dd, направлена от стенки
}
// Связь: Морзе U = D[(1 − e^{−a(r−r0)})² − 1] + гладкое ядро 400·(s/r − 1)² при r < s = 0.6·r0
// (у одного Морзе при r → 0 барьер всего ~D, и очень «горячая» пара могла бы пройти сквозь друг друга)
static inline double bondPot(double D, double r0, double a, double r, double& dUdr) {
    double e = std::exp(-a * (r - r0));
    double U = D * ((1 - e) * (1 - e) - 1); dUdr = 2 * D * a * e * (1 - e);
    double s = 0.6 * r0;
    if (r < s) { double q = s / r - 1; U += 400 * q * q; dUdr += 800 * q * (-s / (r * r)); }
    return U;
}
static inline double bondPotT(const BondT& bt, int o, double r, double& dUdr) { return bondPot(bt.D[o], bt.r0[o], bt.a[o], r, dUdr); }
static inline double morseU(const BondT& bt, int o, double r) { double d; return bondPotT(bt, o, r, d); }
// Равновесный валентный угол (град) атома типа t с k связями, по VSEPR; 0 — угловой член не нужен
static double waterAngle = 104.5;   // меняется только в проверочном режиме --water
static double vseprAngle(int t, int k) {
    if (k < 2) return 0;
    if (t == E_O) return k == 2 ? waterAngle : 113.0;                      // H2O; H3O+ — плоская пирамида
    if (t == E_N) return k == 2 ? 115.0 : (k == 3 ? 107.0 : 109.47);       // NH3 — пирамида, NH4+ — тетраэдр
    if (t == E_C) return k == 2 ? 180.0 : (k == 3 ? 120.0 : 109.47);       // sp, sp2, sp3
    switch (EL[t].Z) {
    case 14: case 32: case 50: return k == 2 ? 180.0 : (k == 3 ? 120.0 : 109.47);   // Si, Ge, Sn
    case 15: case 33: case 51: return k == 2 ? 110.0 : (k == 3 ? 98.0 : 109.47);    // P, As, Sb
    case 16: return k == 2 ? 92.0 : 0;                                              // H2S
    case 34: case 52: return k == 2 ? 91.0 : 0;                                     // H2Se, H2Te
    case 5: case 13: case 31: return k == 4 ? 109.47 : 120.0;                       // BF3 плоская, BH4− тетраэдр
    case 4: case 12: case 30: case 48: case 80: return k == 2 ? 180.0 : 0;          // BeH2, HgCl2 — линейные
    }
    return 0;
}
static inline double theta0Of(int c) { return vseprAngle(S.ty[c], S.nbc[c]); }
// U = k(cosθ − cosθ0)² для угла a–c–b; при F != nullptr добавляет силы
static inline double angleTerm(int c, int a, int b, double c0, bool addForce) {
    double ax, ay, az, bx, by, bz; dvec(c, a, ax, ay, az); dvec(c, b, bx, by, bz);
    double ra = std::sqrt(ax * ax + ay * ay + az * az), rb = std::sqrt(bx * bx + by * by + bz * bz);
    if (ra < 1e-9 || rb < 1e-9) return 0;
    double cs = (ax * bx + ay * by + az * bz) / (ra * rb);
    if (addForce) {
        double k2 = 2 * cfg::K_ANGLE * (cs - c0), iab = 1.0 / (ra * rb);
        // ∂cosθ/∂r_a = b/(|a||b|) − cosθ·a/|a|²
        double gax = bx * iab - cs * ax / (ra * ra), gay = by * iab - cs * ay / (ra * ra), gaz = bz * iab - cs * az / (ra * ra);
        double gbx = ax * iab - cs * bx / (rb * rb), gby = ay * iab - cs * by / (rb * rb), gbz = az * iab - cs * bz / (rb * rb);
        S.bx[a] -= k2 * gax; S.by[a] -= k2 * gay; S.bz[a] -= k2 * gaz;
        S.bx[b] -= k2 * gbx; S.by[b] -= k2 * gby; S.bz[b] -= k2 * gbz;
        S.bx[c] += k2 * (gax + gbx); S.by[c] += k2 * (gay + gby); S.bz[c] += k2 * (gaz + gbz);
    }
    return cfg::K_ANGLE * (cs - c0) * (cs - c0);
}
// ---- Геометрия гипервалентных центров (SF6, PCl5, SF4, ClF3, XeF2, XeF4, SO2, H2SO4…) — модель VSEPR:
// электронные «облака» центра — связи и неподелённые пары — отталкиваются, как точки на сфере:
//   U = K·Σ w_a·w_b / (|u_a − u_b| + δ) − U_min,   u — единичные векторы от центра.
// Неподелённая пара «толще» связи (w = 1.35), кратная связь толще одинарной, неспаренный электрон — половина пары.
// Так сами собой выходят тригональная бипирамида PCl5 и октаэдр SF6, «качели» SF4, T-образная ClF3, линейная XeF2,
// квадратная XeF4. Пары безмассовые: их направления каждый раз доводятся к минимуму U, а сила действует на атомы
// через векторы связей. U_min — энергия идеальной фигуры с тем же набором облаков: U ≥ 0 и не меняет теплоты реакций.
namespace vs { constexpr double K = 2.5 * cfg::EV, D = 0.05, W_LP = 1.35, W_ONE = 0.65; }
static inline bool vseprCenter(int c) { return S.nbc[c] >= 2 && hyperOver(c) > 0; }
static inline double vsBondW(int o) { return o >= 3 ? 1.4 : (o == 2 ? 1.3 : 1.0); }
// Четыре связи и две пары (XeF4, ICl4−): у простой модели отталкивания чуть выгоднее искажённая фигура с парами под
// углом, а в природе пары всегда напротив друг друга (квадрат) — это правило VSEPR задаём явно: вторая пара = −первая.
static inline bool vsTransPair(int k, int nl) { return k == 4 && nl == 2; }
// энергия облаков и, если g != nullptr, её градиент по векторам связей r[a] (от центра к соседу);
// L — направления пар (подстраиваются за iters шагов наискорейшего спуска по сфере)
static double vsEnergy(int k, const double (*r)[3], const double* w, int nd, const double* wl, double (*L)[3], int iters, double (*g)[3], bool trans = false) {
    double u[cfg::MAXB][3], len[cfg::MAXB];
    for (int a = 0; a < k; a++) { len[a] = std::sqrt(r[a][0] * r[a][0] + r[a][1] * r[a][1] + r[a][2] * r[a][2]); for (int c = 0; c < 3; c++) u[a][c] = r[a][c] / std::max(1e-12, len[a]); }
    // вклад пары векторов x, y с весом ww: энергия и производная по x
    auto pairE = [](const double* x, const double* y, double ww, double* dx) {
        double d[3] = {x[0] - y[0], x[1] - y[1], x[2] - y[2]}, l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        const double e = vs::K * ww / (l + vs::D);
        if (dx) { const double f = -vs::K * ww / ((l + vs::D) * (l + vs::D) * std::max(l, 1e-9)); dx[0] += f * d[0]; dx[1] += f * d[1]; dx[2] += f * d[2]; }
        return e;
    };
    for (int it = 0; it < iters; it++)   // пары — к минимуму при зафиксированных связях
        for (int m = 0; m < nd; m++) {
            if (trans && m == 1) continue;   // вторая пара — просто напротив первой
            double G[3] = {0, 0, 0};
            for (int a = 0; a < k; a++) pairE(L[m], u[a], wl[m] * w[a], G);
            for (int q = 0; q < nd; q++) if (q != m) pairE(L[m], L[q], wl[m] * wl[q], G);
            if (trans && m == 0) {   // L1 = −L0: к градиенту по L0 добавляется −(градиент по L1)
                double G1[3] = {0, 0, 0};
                for (int a = 0; a < k; a++) pairE(L[1], u[a], wl[1] * w[a], G1);
                for (int q = 2; q < nd; q++) pairE(L[1], L[q], wl[1] * wl[q], G1);
                for (int c = 0; c < 3; c++) G[c] -= G1[c];
            }
            const double gl = G[0] * L[m][0] + G[1] * L[m][1] + G[2] * L[m][2], step = 0.15 / vs::K;
            double nl = 0; for (int c = 0; c < 3; c++) { L[m][c] -= step * (G[c] - gl * L[m][c]); nl += L[m][c] * L[m][c]; }
            nl = 1.0 / std::sqrt(std::max(nl, 1e-24)); for (int c = 0; c < 3; c++) L[m][c] *= nl;
            if (trans && m == 0) for (int c = 0; c < 3; c++) L[1][c] = -L[0][c];
        }
    double E = 0, du[cfg::MAXB][3] = {};
    for (int a = 0; a < k; a++) {
        for (int b = a + 1; b < k; b++) { double t[3] = {0, 0, 0}; E += pairE(u[a], u[b], w[a] * w[b], g ? t : nullptr); if (g) for (int c = 0; c < 3; c++) { du[a][c] += t[c]; du[b][c] -= t[c]; } }
        for (int m = 0; m < nd; m++) E += pairE(u[a], L[m], w[a] * wl[m], g ? du[a] : nullptr);
    }
    for (int m = 0; m < nd; m++) for (int q = m + 1; q < nd; q++) E += pairE(L[m], L[q], wl[m] * wl[q], nullptr);
    if (g) for (int a = 0; a < k; a++) {   // dU/dr = (I − u uᵀ)·dU/du / |r|
        const double pu = du[a][0] * u[a][0] + du[a][1] * u[a][1] + du[a][2] * u[a][2];
        for (int c = 0; c < 3; c++) g[a][c] = (du[a][c] - pu * u[a][c]) / std::max(1e-12, len[a]);
    }
    return E;
}
// начальные направления пар: против суммы связей и случайно; лучшая из нескольких попыток после долгого спуска
static void vsInitLP(int k, const double (*r)[3], const double* w, int nd, const double* wl, double (*L)[3], bool trans) {
    double best = 1e300, B[4][3];
    for (int tr = 0; tr < 5; tr++) {
        double T[4][3];
        for (int m = 0; m < nd; m++) {
            double v[3] = {grand(), grand(), grand()};
            if (m == 0 && tr == 0) { v[0] = v[1] = v[2] = 0; for (int a = 0; a < k; a++) for (int c = 0; c < 3; c++) v[c] -= r[a][c]; }
            double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l < 1e-6) { v[0] = 1; v[1] = v[2] = 0; l = 1; }
            for (int c = 0; c < 3; c++) T[m][c] = v[c] / l;
        }
        if (trans) for (int c = 0; c < 3; c++) T[1][c] = -T[0][c];
        const double e = vsEnergy(k, r, w, nd, wl, T, 80, nullptr, trans);
        if (e < best) { best = e; memcpy(B, T, sizeof(B)); }
    }
    memcpy(L, B, sizeof(double) * 3 * nd);
}
// идеальная фигура для набора облаков: все направления свободны, спуск из нескольких случайных начал;
// dirs (если задан) — лучшие направления: сначала k связей, затем пары
static double vsIdeal(int k, const double* w, int nd, const double* wl, double (*dirs)[3], bool trans) {
    double best = 1e300; const int tot = k + nd;
    for (int tr = 0; tr < 8; tr++) {
        double P[cfg::MAXB + 4][3], W[cfg::MAXB + 4];
        for (int a = 0; a < tot; a++) { double v[3] = {grand(), grand(), grand()}, l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) + 1e-12; for (int c = 0; c < 3; c++) P[a][c] = v[c] / l; W[a] = a < k ? w[a] : wl[a - k]; }
        if (trans) for (int c = 0; c < 3; c++) P[k + 1][c] = -P[k][c];
        auto gradOf = [&](int a, double* G) {
            G[0] = G[1] = G[2] = 0;
            for (int b = 0; b < tot; b++) if (b != a) {
                double d[3] = {P[a][0] - P[b][0], P[a][1] - P[b][1], P[a][2] - P[b][2]}, l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                const double f = -vs::K * W[a] * W[b] / ((l + vs::D) * (l + vs::D) * std::max(l, 1e-9)); for (int c = 0; c < 3; c++) G[c] += f * d[c];
            }
        };
        for (int it = 0; it < 3000; it++) for (int a = 0; a < tot; a++) {
            if (trans && a == k + 1) continue;
            double G[3]; gradOf(a, G);
            if (trans && a == k) { double G1[3]; gradOf(k + 1, G1); for (int c = 0; c < 3; c++) G[c] -= G1[c]; }
            const double gp = G[0] * P[a][0] + G[1] * P[a][1] + G[2] * P[a][2]; double nl = 0;
            for (int c = 0; c < 3; c++) { P[a][c] -= 0.05 / vs::K * (G[c] - gp * P[a][c]); nl += P[a][c] * P[a][c]; }
            nl = 1.0 / std::sqrt(nl); for (int c = 0; c < 3; c++) P[a][c] *= nl;
            if (trans && a == k) for (int c = 0; c < 3; c++) P[k + 1][c] = -P[k][c];
        }
        double E = 0;
        for (int a = 0; a < tot; a++) for (int b = a + 1; b < tot; b++) {
            double d[3] = {P[a][0] - P[b][0], P[a][1] - P[b][1], P[a][2] - P[b][2]}; E += vs::K * W[a] * W[b] / (std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) + vs::D);
        }
        if (E < best) { best = E; if (dirs) memcpy(dirs, P, sizeof(double) * 3 * tot); }
    }
    return best;
}
// энергия идеальной фигуры (кэш по составу облаков)
static double vsEnergyMin(int k, const double* w, int nd, const double* wl) {
    static std::map<std::vector<int>, double> cache;
    std::vector<int> key{nd}; std::vector<int> ws;
    for (int a = 0; a < k; a++) ws.push_back((int)std::lround(w[a] * 100));
    std::sort(ws.begin(), ws.end()); key.insert(key.end(), ws.begin(), ws.end());
    for (int m = 0; m < nd; m++) key.push_back((int)std::lround(wl[m] * 100));
    auto it = cache.find(key); if (it != cache.end()) return it->second;
    int nl = 0; for (int m = 0; m < nd; m++) if (wl[m] >= vs::W_LP) nl++;
    return cache[key] = vsIdeal(k, w, nd, wl, nullptr, vsTransPair(k, nl));
}
// облака центра c: векторы связей, веса, число пар и неспаренный электрон
static int vsSetup(int c, double (*r)[3], double* w, double* wl, int& key) {
    const int k = S.nbc[c], t = S.ty[c];
    for (int a = 0; a < k; a++) { dvec(c, S.nb[c][a], r[a][0], r[a][1], r[a][2]); w[a] = vsBondW(S.bo[c][a]); }
    const int nbE = std::max(0, VE[t] - usedVal(c)), nl = std::min(3, nbE / 2), unp = nbE & 1, nd = nl + unp;
    for (int m = 0; m < nd; m++) wl[m] = m < nl ? vs::W_LP : vs::W_ONE;
    key = 1 + nl * 2 + unp;
    return nd;
}
// энергия (и силы в быстрые силы S.b*) VSEPR-центра c
static double vseprAt(int c, bool addForce, int iters) {
    double r[cfg::MAXB][3], w[cfg::MAXB], wl[4], g[cfg::MAXB][3]; int key;
    const int k = S.nbc[c], nd = vsSetup(c, r, w, wl, key), nl = (key - 1) / 2;
    const bool trans = vsTransPair(k, nl);
    double (*L)[3] = reinterpret_cast<double (*)[3]>(S.lp[c].data());
    double Emin;
#pragma omp critical(vsepr)   // кэш идеальных фигур и генератор случайных чисел — общие для потоков
    {
        if (S.lpk[c] != key) { if (nd) vsInitLP(k, r, w, nd, wl, L, trans); S.lpk[c] = (unsigned char)key; iters = std::max(iters, 30); }
        Emin = vsEnergyMin(k, w, nd, wl);
    }
    const double E = vsEnergy(k, r, w, nd, wl, L, iters, addForce ? g : nullptr, trans) - Emin;
    if (addForce) for (int a = 0; a < k; a++) {
        const int j = S.nb[c][a];
        S.bx[j] -= g[a][0]; S.by[j] -= g[a][1]; S.bz[j] -= g[a][2];
        S.bx[c] += g[a][0]; S.by[c] += g[a][1]; S.bz[c] += g[a][2];
    }
    return E;
}
// ---- π-связь держит плоскость: у двойной связи между атомами с тремя соседями (C=C, C=N) плоскости заместителей
// обоих атомов совпадают. U = V·(1 − (n_i·n_j)²), n — нормаль к плоскости двух других соседей атома;
// V = 2.7 эВ — барьер вращения этилена. Одинарная связь между двумя такими атомами (сопряжение, как в бутадиене
// и в кольце бензола) — 0.25 эВ.
static inline bool hasMulti(int i) { for (int k = 0; k < S.nbc[i]; k++) if (S.bo[i][k] >= 2) return true; return false; }
static inline double piBarrier(int i, int j, int o) {
    if (S.nbc[i] != 3 || S.nbc[j] != 3) return 0;
    if (o == 2) return 2.7 * cfg::EV;
    return o == 1 && hasMulti(i) && hasMulti(j) ? 0.25 * cfg::EV : 0;
}
// нормаль плоскости атома i по двум его соседям, кроме j: m = v1 × v2
static inline bool piPlane(int i, int j, int& a, int& b, double* v1, double* v2, double* m, double& ml) {
    a = b = -1;
    for (int k = 0; k < S.nbc[i]; k++) { int q = S.nb[i][k]; if (q == j) continue; if (a < 0) a = q; else b = q; }
    if (a < 0 || b < 0) return false;
    dvec(i, a, v1[0], v1[1], v1[2]); dvec(i, b, v2[0], v2[1], v2[2]);
    m[0] = v1[1] * v2[2] - v1[2] * v2[1]; m[1] = v1[2] * v2[0] - v1[0] * v2[2]; m[2] = v1[0] * v2[1] - v1[1] * v2[0];
    ml = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
    return ml > 1e-9;
}
static double piTerm(int i, int j, int o, bool addForce) {
    const double V = piBarrier(i, j, o); if (V <= 0) return 0;
    int ai, bi, aj, bj; double v1[3], v2[3], w1[3], w2[3], mi[3], mj[3], li, lj;
    if (!piPlane(i, j, ai, bi, v1, v2, mi, li) || !piPlane(j, i, aj, bj, w1, w2, mj, lj)) return 0;
    const double ni[3] = {mi[0] / li, mi[1] / li, mi[2] / li}, nj[3] = {mj[0] / lj, mj[1] / lj, mj[2] / lj};
    const double c = ni[0] * nj[0] + ni[1] * nj[1] + ni[2] * nj[2];
    if (addForce) {
        // dU/dn_i = −2Vc·n_j; dU/dm = (I − n nᵀ)·dU/dn / |m|; m = v1 × v2: dU/dv1 = v2 × G, dU/dv2 = G × v1
        auto apply = [&](int ctr, int a, int b, const double* n, const double* other, double ml, const double* p1, const double* p2) {
            double Gn[3] = {-2 * V * c * other[0], -2 * V * c * other[1], -2 * V * c * other[2]};
            const double gn = Gn[0] * n[0] + Gn[1] * n[1] + Gn[2] * n[2];
            double G[3] = {(Gn[0] - gn * n[0]) / ml, (Gn[1] - gn * n[1]) / ml, (Gn[2] - gn * n[2]) / ml};
            double d1[3] = {p2[1] * G[2] - p2[2] * G[1], p2[2] * G[0] - p2[0] * G[2], p2[0] * G[1] - p2[1] * G[0]};
            double d2[3] = {G[1] * p1[2] - G[2] * p1[1], G[2] * p1[0] - G[0] * p1[2], G[0] * p1[1] - G[1] * p1[0]};
            S.bx[a] -= d1[0]; S.by[a] -= d1[1]; S.bz[a] -= d1[2];
            S.bx[b] -= d2[0]; S.by[b] -= d2[1]; S.bz[b] -= d2[2];
            S.bx[ctr] += d1[0] + d2[0]; S.by[ctr] += d1[1] + d2[1]; S.bz[ctr] += d1[2] + d2[2];
        };
        apply(i, ai, bi, ni, nj, li, v1, v2);
        apply(j, aj, bj, nj, ni, lj, w1, w2);
    }
    return V * (1 - c * c);
}
// все π-члены связей, у которых хотя бы один конец в множестве P (для ΔU химических событий)
static double piEnergyOf(const std::vector<int>& P) {
    double E = 0;
    for (int i : P) for (int k = 0; k < S.nbc[i]; k++) {
        const int j = S.nb[i][k];
        if (j < i && std::find(P.begin(), P.end(), j) != P.end()) continue;   // пара внутри P — один раз
        E += piTerm(i, j, S.bo[i][k], false);
    }
    return E;
}
static double angleEnergyAt(int c, bool addForce = false) {
    if (vseprCenter(c)) return vseprAt(c, addForce, addForce ? 8 : 30);
    double th = theta0Of(c); if (th <= 0) return 0;
    double c0 = std::cos(th * PI / 180), E = 0; int k = S.nbc[c];
    for (int a = 0; a < k; a++) for (int b = a + 1; b < k; b++) E += angleTerm(c, S.nb[c][a], S.nb[c][b], c0, addForce);
    return E;
}
// ---- водородная связь возникает сама из притяжения частичных зарядов (H+ ··· O−, N−, F−, Cl−):
//      отдельного члена в потенциале нет, как в моделях воды SPC и TIP. Эти функции нужны химии и анализу.
static inline bool hbAcceptor(int t) { return t == E_O || t == E_N || t == E_CLM || t == E_F; }
static inline int hbDonor(int h) {   // атом D, к которому ковалентно привязан водород h (O, N или F), иначе −1
    if (S.ty[h] != E_H || S.nbc[h] != 1) return -1;
    int d = S.nb[h][0]; return (S.ty[d] == E_O || S.ty[d] == E_N || S.ty[d] == E_F) ? d : -1;
}
static inline bool isWaterO(int i) {
    return S.ty[i] == E_O && S.nbc[i] == 2 && S.ty[S.nb[i][0]] == E_H && S.ty[S.nb[i][1]] == E_H;
}
// ---- металлическая связь (Гупта): доля «металличности» атома и силы.
// Атом металла, связанный с неметаллом, отдаёт этой связи часть электронов: его металлическая связь слабеет на
// W_BOND·(занятая валентность)/(валентность). Полная потеря (W_BOND = 1) делала бы хемосорбцию невыгодной: у платины
// зонная энергия ~9 эВ на атом, и одна связь Pt–H (3.8 эВ) стоила бы ~5.8 эВ металлической связи; с 0.2 атом H
// садится на Pt с выигрышем ~2.7 эВ, как в опыте
constexpr double W_BOND = 0.2;
static std::vector<double> gRho, gW;
static inline double metalW(int i) { const Element& e = EL[S.ty[i]]; if (e.val <= 0) return 1.0; return std::max(0.0, 1.0 - W_BOND * usedVal(i) / e.val); }
static double metalForces(double& vir) {
    const int n = S.n; gRho.assign(n, 0.0); gW.assign(n, 0.0);
    for (int i = 0; i < n; i++) if (isMetalT(S.ty[i])) gW[i] = metalW(i);
    // проход 1: электронная плотность ρ_i от металлических соседей
#pragma omp parallel for schedule(dynamic, 64)
    for (int i = 0; i < n; i++) {
        const int ti = S.ty[i]; if (!isMetalT(ti)) continue;
        double rho = 0;
        for (int p = nlStart[i]; p < nlStart[i + 1]; p++) {
            int j = nlIdx[p]; if (gW[j] <= 0 || !isMetalT(S.ty[j])) continue;
            const GupP& g = GP[ti][S.ty[j]]; double dx, dy, dz; dvec(i, j, dx, dy, dz); double r2 = dx * dx + dy * dy + dz * dz;
            if (r2 >= g.r2 * g.r2 || r2 < 1e-12) continue;
            double f, df, h, dh; gupFG(g, std::sqrt(r2), f, df, h, dh); rho += gW[j] * g.xi2 * h;
        }
        gRho[i] = rho;
    }
    // проход 2: силы (отталкивание пар + вложение), каждый поток пишет только в «свой» атом
    double E = 0, V = 0;
#pragma omp parallel for schedule(dynamic, 64) reduction(+ : E, V)
    for (int i = 0; i < n; i++) {
        const int ti = S.ty[i]; if (!isMetalT(ti)) continue;
        double fxi = 0, fyi = 0, fzi = 0, ei = 0, vi = 0, wi = gW[i], si = gRho[i] > 1e-30 ? 0.5 / std::sqrt(gRho[i]) : 0;
        for (int p = nlStart[i]; p < nlStart[i + 1]; p++) {
            int j = nlIdx[p]; if (!isMetalT(S.ty[j])) continue;
            const GupP& g = GP[ti][S.ty[j]]; double dx, dy, dz; dvec(i, j, dx, dy, dz); double r2 = dx * dx + dy * dy + dz * dz;
            if (r2 >= g.r2 * g.r2 || r2 < 1e-12) continue;
            double r = std::sqrt(r2), f, df, h, dh; gupFG(g, r, f, df, h, dh);
            double dU = g.A * df;                                   // dU/dr
            if (wi > 0 && gW[j] > 0) { double sj = gRho[j] > 1e-30 ? 0.5 / std::sqrt(gRho[j]) : 0; dU -= g.xi2 * dh * wi * gW[j] * (si + sj); }
            fxi += dU * dx / r; fyi += dU * dy / r; fzi += dU * dz / r;
            ei += 0.5 * g.A * f; vi += -0.5 * dU * r;
        }
        if (wi > 0) ei -= wi * std::sqrt(std::max(0.0, gRho[i]));
        S.fx[i] += fxi; S.fy[i] += fyi; S.fz[i] += fzi; S.ep[i] += ei; E += ei; V += vi;
    }
    vir += V;
    return E;
}
// энергия вложения атомов металла вблизи множества C (для точного ΔU химических событий)
static double metalEmbedLocal(const std::vector<int>& C) {
    if (!anyMetal) return 0;
    std::vector<int> M;
    for (int c : C) {
        if (!isMetalT(S.ty[c])) continue;
        M.push_back(c);
        for (int p = nlStart[c]; p < nlStart[c + 1]; p++) { int j = nlIdx[p]; if (isMetalT(S.ty[j]) && dist2(c, j) < GP[S.ty[c]][S.ty[j]].r2 * GP[S.ty[c]][S.ty[j]].r2) M.push_back(j); }
    }
    std::sort(M.begin(), M.end()); M.erase(std::unique(M.begin(), M.end()), M.end());
    double E = 0;
    for (int k : M) {
        double wk = metalW(k); if (wk <= 0) continue;
        double rho = 0;
        for (int p = nlStart[k]; p < nlStart[k + 1]; p++) {
            int j = nlIdx[p]; if (!isMetalT(S.ty[j])) continue; double wj = metalW(j); if (wj <= 0) continue;
            const GupP& g = GP[S.ty[k]][S.ty[j]]; double r2 = dist2(k, j); if (r2 >= g.r2 * g.r2 || r2 < 1e-12) continue;
            double f, df, h, dh; gupFG(g, std::sqrt(r2), f, df, h, dh); rho += wj * g.xi2 * h;
        }
        E -= wk * std::sqrt(std::max(0.0, rho));
    }
    return E;
}
static inline void tweezerForce(double& Fx, double& Fy, double& Fz) {
    int g = grabbed; double m = EL[S.ty[g]].m, k = cfg::TWEEZER_K, c = 2 * std::sqrt(k);
    double dx = grabX - S.x[g], dy = grabY - S.y[g], dz = grabZ - S.z[g];
    if (isPer()) { dx -= S.Lx * std::nearbyint(dx / S.Lx); dy -= S.Ly * std::nearbyint(dy / S.Ly); dz -= S.Lz * std::nearbyint(dz / S.Lz); }
    Fx =m * (k * dx - c * S.vx[g]); Fy = m * (k * dy - c * S.vy[g]); Fz = m * (k * dz - c * S.vz[g]);
}

// ===================================== ОБЪЕКТЫ ПОЛЯ: силы ================================
static std::vector<double> foFx, foFy, foFz;         // неконсервативная сила объектов (ветер, вихрь) — для работы W
static bool foNC = false;                            // на последнем расчёте сил были неконсервативные силы
static bool foAnyOn() { for (auto& o : fieldObjs) if (o.on) return true; return false; }
// вектор от точки (cx,cy,cz) к атому i с минимальным образом
static inline void foRel(double cx, double cy, double cz, int i, double& rx, double& ry, double& rz) {
    rx = S.x[i] - cx; ry = S.y[i] - cy; rz = S.z[i] - cz;
    if (isPer()) { rx -= S.Lx * std::nearbyint(rx / S.Lx); ry -= S.Ly * std::nearbyint(ry / S.Ly); rz -= S.Lz * std::nearbyint(rz / S.Lz); }
}
static inline void foUnitDir(const FieldObj& o, double& ux, double& uy, double& uz) {
    ux = o.dx; uy = o.dy; uz = o.dz;
    double l = std::sqrt(ux * ux + uy * uy + uz * uz);
    if (l < 1e-12) { ux = 1; uy = 0; uz = 0; if (o.kind == FO_VORTEX) { ux = 0; uy = 1; } return; }
    ux /= l; uy /= l; uz /= l;
}
// Барьер: вектор от ближайшей точки барьера (область плоскости в пределах R от отрезка) к атому
static inline void barrierVec(const FieldObj& o, int i, double& vx, double& vy, double& vz) {
    double nx, ny, nz; foUnitDir(o, nx, ny, nz);
    double ax = o.x2 - o.x, ay = o.y2 - o.y, az = o.z2 - o.z, an = ax * nx + ay * ny + az * nz;
    ax -= an * nx; ay -= an * ny; az -= an * nz;   // отрезок проецируется в плоскость барьера
    double L = std::sqrt(ax * ax + ay * ay + az * az);
    double rx, ry, rz; foRel(o.x + 0.5 * ax, o.y + 0.5 * ay, o.z + 0.5 * az, i, rx, ry, rz);
    double h = rx * nx + ry * ny + rz * nz, qx = rx - h * nx, qy = ry - h * ny, qz = rz - h * nz;
    double ex = qx, ey = qy, ez = qz;
    if (o.R < 1000) {
        if (L > 1e-9) { double t = clampv((qx * ax + qy * ay + qz * az) / L, -0.5 * L, 0.5 * L) / L; ex = qx - t * ax; ey = qy - t * ay; ez = qz - t * az; }
        double e = std::sqrt(ex * ex + ey * ey + ez * ez);
        if (e > o.R) { double k = 1 - o.R / e; ex *= k; ey *= k; ez *= k; } else ex = ey = ez = 0;
    } else ex = ey = ez = 0;   // бесконечная плоскость
    vx = h * nx + ex; vy = h * ny + ey; vz = h * nz + ez;
}
// консервативная энергия атома i в поле объекта o; F != nullptr — добавить силу
static double foConsAtom(const FieldObj& o, int i, double* F) {
    const double A = o.strength * clampv(o.ramp, 0.0, 1.0);   // плавное включение: только что поставленный объект раздвигает атомы мягко
    if (A == 0) return 0;
    if (o.kind == FO_BARRIER) {
        // стенка 9-3 (только отталкивание, обрезка в минимуме (2/5)^{1/6}σ); ближе ds = 0.6σ — гладкий параболический
        // «гребень» U = U(ds) + f_s(ds² − d²)/(2ds): сила ограничена (≤ 175ε_w/σ) и плавно обращается в 0 в плоскости
        // барьера, высота ≈ 62ε_w — непроницаем при обычных T, но без «взрыва» при установке поперёк вещества
        double vx, vy, vz; barrierVec(o, i, vx, vy, vz);
        const double s = std::max(0.3, EL[S.ty[i]].sig), dm = 0.858374 * s, ds = 0.6 * s;
        double d = std::sqrt(vx * vx + vy * vy + vz * vz);
        if (d >= dm) return 0;
        double dd = std::max(d, ds), a = s / dd, a3 = a * a * a, a9 = a3 * a3 * a3;
        double U = A * ((2.0 / 15.0) * a9 - a3 + 1.0540926), f = A * (1.2 * a9 - 3.0 * a3) / dd;   // f = −dU/dd
        if (d < ds) { U += f * (ds * ds - d * d) / (2 * ds); if (F) { double g = f / ds; F[0] += g * vx; F[1] += g * vy; F[2] += g * vz; } }
        else if (F) { f /= d; F[0] += f * vx; F[1] += f * vy; F[2] += f * vz; }
        return U;
    }
    double rx, ry, rz; foRel(o.x, o.y, o.z, i, rx, ry, rz);
    const double R2 = o.R * o.R, r2 = rx * rx + ry * ry + rz * rz;
    if (r2 >= R2 || R2 <= 0) return 0;
    const double u = 1 - r2 / R2;
    double U = 0, g = 0;   // g: F = g·r
    switch (o.kind) {
    case FO_ATTRACT: U = -A * u * u * u; g = -6 * A * u * u / R2; break;
    case FO_REPEL: U = A * u * u; g = 4 * A * u / R2; break;
    case FO_TRAP: U = -A * u * u; g = -4 * A * u / R2; break;
    default: return 0;
    }
    if (F) { F[0] += g * rx; F[1] += g * ry; F[2] += g * rz; }
    return U;
}
// неконсервативная сила (ветер, вихрь) на атом i
static inline void foNCAtom(const FieldObj& o, int i, double* F) {
    double rx, ry, rz; foRel(o.x, o.y, o.z, i, rx, ry, rz);
    const double R2 = o.R * o.R, r2 = rx * rx + ry * ry + rz * rz;
    if (r2 >= R2 || R2 <= 0) return;
    double ux, uy, uz; foUnitDir(o, ux, uy, uz);
    if (o.kind == FO_WIND) {   // плоская «вершина» и гладкий край: 1 при r < 0.6R, 0 при r ≥ R
        double dS, w = taper(std::sqrt(r2), 0.6 * o.R, o.R, dS);
        F[0] += o.strength * w * ux; F[1] += o.strength * w * uy; F[2] += o.strength * w * uz;
    } else if (o.kind == FO_VORTEX) {   // |F| = A·(ρ/R)·(1 − r²/R²)², направление û × r
        double u = 1 - r2 / R2, k = o.strength * u * u / o.R;
        F[0] += k * (uy * rz - uz * ry); F[1] += k * (uz * rx - ux * rz); F[2] += k * (ux * ry - uy * rx);
    }
}
// энергия атомов в поле объекта o при полной силе (ramp = 1)
static double foUnitEnergy(const FieldObj& o) {
    FieldObj u = o; u.ramp = 1; double E = 0; const int n = S.n;
#pragma omp parallel for reduction(+ : E)
    for (int i = 0; i < n; i++) if (!frozenAt(i)) E += foConsAtom(u, i, nullptr);
    return E;
}
// Плавное включение: работа «руки» W = ∫U₁ dλ по формуле трапеций — половина в конце шага (applyFieldObjs),
// половина при следующем расчёте сил (новые координаты); так ошибка расщепления O(Δλ) взаимно сокращается.
static std::vector<std::pair<int, double>> foRampPend;   // (объект, Δλ), ждущие второй половины работы
// Силы и энергия объектов поля. Правки объектов пользователем учитывает UI (foEditBegin/End → W);
// физика начисляет в W только работу своего плавного включения.
static void fieldObjForces() {
    const int n = S.n;
    EN.efo = 0; foNC = false;
    if (!foRampPend.empty()) {
        for (auto& pr : foRampPend) if (pr.first < (int)fieldObjs.size() && fieldObjs[pr.first].on) Wext += 0.5 * pr.second * foUnitEnergy(fieldObjs[pr.first]);
        foRampPend.clear();
    }
    if (fieldObjs.empty()) return;
    bool anyCons = false;
    for (auto& o : fieldObjs) if (o.on) { if (foConservative(o.kind)) anyCons = true; if (o.kind == FO_WIND || o.kind == FO_VORTEX) foNC = true; }
    if (!anyCons && !foNC) return;
    if (foNC) { foFx.assign(n, 0.0); foFy.assign(n, 0.0); foFz.assign(n, 0.0); }
    const FieldObj* objs = fieldObjs.data(); const int no = (int)fieldObjs.size();
    double E = 0;
#pragma omp parallel for schedule(static) reduction(+ : E)
    for (int i = 0; i < n; i++) {
        if (frozenAt(i)) continue;
        double F[3] = {0, 0, 0}, G[3] = {0, 0, 0}, e = 0;
        for (int k = 0; k < no; k++) {
            const FieldObj& o = objs[k]; if (!o.on) continue;
            if (foConservative(o.kind)) e += foConsAtom(o, i, F);
            else if (o.kind == FO_WIND || o.kind == FO_VORTEX) foNCAtom(o, i, G);
        }
        S.fx[i] += F[0] + G[0]; S.fy[i] += F[1] + G[1]; S.fz[i] += F[2] + G[2];
        if (foNC) { foFx[i] = G[0]; foFy[i] = G[1]; foFz[i] = G[2]; }
        S.ep[i] += e; E += e;
    }
    EN.efo = E;
}

// Силы делятся на быстрые (химические связи и валентные углы: колебания O–H с периодом 9 фс) и медленные
// (всё остальное). Интегратор RESPA (Tuckerman, Berne, Martyna 1992) двигает быстрые внутренним шагом dt/4,
// а дорогие парные силы считает раз за внешний шаг. Медленные — в S.fx, быстрые — в S.bx.
static std::vector<double> tbuf;   // силы и энергии пар по потокам: fx | fy | fz | ep для каждого потока
static double virFast = 0;
// Связи, углы и π-члены не выходят за пределы молекулы, поэтому быстрые силы считаются по молекулам параллельно:
// потоки, занятые разными молекулами, не пишут в один атом. Состав молекул пересобирается при каждом полном
// расчёте сил (он идёт после любой смены связей), внутренние шаги RESPA берут его готовым.
static std::vector<int> fgStart, fgAtoms, fgPar; static int fgN = -1;
static void fastGroups() {
    const int n = S.n; fgPar.resize(n);
    for (int i = 0; i < n; i++) fgPar[i] = i;
    auto root = [&](int a) { while (fgPar[a] != a) { fgPar[a] = fgPar[fgPar[a]]; a = fgPar[a]; } return a; };
    for (int i = 0; i < n; i++) for (int k = 0; k < S.nbc[i]; k++) { const int a = root(i), b = root(S.nb[i][k]); if (a != b) fgPar[a] = b; }
    for (int i = 0; i < n; i++) fgPar[i] = root(i);
    fgStart.assign(n + 1, 0);
    for (int i = 0; i < n; i++) if (S.nbc[i]) fgStart[fgPar[i] + 1]++;   // одиночные атомы быстрых сил не имеют
    for (int i = 0; i < n; i++) fgStart[i + 1] += fgStart[i];
    fgAtoms.resize(fgStart[n]);
    std::vector<int> fill(fgStart.begin(), fgStart.end() - 1);
    for (int i = 0; i < n; i++) if (S.nbc[i]) fgAtoms[fill[fgPar[i]]++] = i;
    // пустые группы убираются: fgStart — начала непустых молекул
    int m = 0;
    for (int r = 0; r < n; r++) if (fgStart[r + 1] > fgStart[r]) fgStart[m++] = fgStart[r];
    fgStart[m] = (int)fgAtoms.size(); fgStart.resize(m + 1);
    fgN = n;
}
static void computeFast(bool withEp) {
    const int n = S.n;
    if (withEp || fgN != n) fastGroups();
    std::fill(S.bx.begin(), S.bx.begin() + n, 0.0); std::fill(S.by.begin(), S.by.begin() + n, 0.0); std::fill(S.bz.begin(), S.bz.begin() + n, 0.0);
    double eb = 0, vir = 0; int capped = 0;
    const int ng = (int)fgStart.size() - 1;
#pragma omp parallel for schedule(dynamic, 16) reduction(+ : eb, vir) if (ng > 32)
    for (int g = 0; g < ng; g++) for (int p = fgStart[g]; p < fgStart[g + 1]; p++) {
        const int i = fgAtoms[p];
        for (int k = 0; k < S.nbc[i]; k++) {
            int j = S.nb[i][k]; if (j < i) continue;
            const BondT& bt = BT[S.ty[i]][S.ty[j]]; int o = S.bo[i][k];
            double dx, dy, dz; dvec(i, j, dx, dy, dz);
            double r = std::sqrt(dx * dx + dy * dy + dz * dz); if (r < 1e-9) continue;
            double dUdr, U = bondPotT(bt, o, r, dUdr) + S.bc[i][k];   // + сдвиг (не даёт силы)
            double ff = -dUdr / r;                                   // −(dU/dr)/r
            S.bx[i] -= ff * dx; S.by[i] -= ff * dy; S.bz[i] -= ff * dz;
            S.bx[j] += ff * dx; S.by[j] += ff * dy; S.bz[j] += ff * dz;
            eb += U; vir += ff * r * r;
            if (withEp) { S.ep[i] += 0.5 * U; S.ep[j] += 0.5 * U; }
            if (S.nbc[i] == 3 && S.nbc[j] == 3) { const double Up = piTerm(i, j, o, true); eb += Up; if (withEp) { S.ep[i] += 0.5 * Up; S.ep[j] += 0.5 * Up; } }
        }
        if (S.nbc[i] >= 2) { double U = angleEnergyAt(i, true); eb += U; if (withEp) S.ep[i] += U; }
    }
#pragma omp parallel for reduction(+ : capped) if (n > OMP_MIN)
    for (int i = 0; i < n; i++) {   // аварийное ограничение
        double f2 = S.bx[i] * S.bx[i] + S.by[i] * S.by[i] + S.bz[i] * S.bz[i];
        if (f2 > cfg::F_CAP * cfg::F_CAP) { double s = cfg::F_CAP / std::sqrt(f2); S.bx[i] *= s; S.by[i] *= s; S.bz[i] *= s; capped++; }
    }
    EN.ebond = eb; virFast = vir; EN.capped += capped;
}
static void computeSlow() {
    ensureNeighborList();
    const int n = S.n; const bool per = isPer();
    const double Lx = S.Lx, Ly = S.Ly, Lz = S.Lz, rcc2 = rcCoul * rcCoul;
    double enb = 0, vir = 0;
    // --- молекулы (компоненты связности): исключения 1-2, 1-3 и ослабление 1-4 проверяются только внутри одной
    static std::vector<int> molId; molId.resize(n);
    for (int i = 0; i < n; i++) molId[i] = i;
    auto root = [&](int a) { while (molId[a] != a) { molId[a] = molId[molId[a]]; a = molId[a]; } return a; };
    for (int i = 0; i < n; i++) for (int k = 0; k < S.nbc[i]; k++) { const int a = root(i), b = root(S.nb[i][k]); if (a != b) molId[a] = b; }
    for (int i = 0; i < n; i++) molId[i] = root(i);
    // --- собственная энергия ионов в методе DSF (сил не даёт, меняется только при переносе заряда)
    if (anyCharge) {
        static std::vector<double> qs; static std::vector<int> cnt; qs.assign(n, 0.0); cnt.assign(n, 0);
        for (int i = 0; i < n; i++) { qs[molId[i]] += S.q[i]; cnt[molId[i]]++; }
        for (int i = 0; i < n; i++) if (cnt[i] > 0 && cnt[i] <= 64) { const long Q = std::lround(qs[i]); if (Q) enb -= ionSelfK() * Q * Q; }
    }
    // --- парные силы: каждая пара — один раз (j > i), силы копятся в буфере своего потока, затем складываются.
    // Всё, что нужно о соседе j, упаковано в одну запись (одна строка кэша вместо шести массивов)
    struct PackA { double x, y, z, q; int ty, mol, flags; };   // flags: 1 — есть связи, 2 — есть «призраки»
    static std::vector<PackA> pk; pk.resize(n);
#pragma omp parallel for schedule(static) if (n > OMP_MIN)
    for (int i = 0; i < n; i++) pk[i] = {S.x[i], S.y[i], S.z[i], S.q[i], S.ty[i], molId[i], (S.nbc[i] ? 1 : 0) | (S.ghc[i] ? 2 : 0)};
    const double iLx = 1 / Lx, iLy = 1 / Ly, iLz = 1 / Lz;
    const int nt = std::max(1, omp_get_max_threads());
    tbuf.assign((size_t)nt * 4 * n, 0.0);
#pragma omp parallel reduction(+ : enb, vir)
    {
        double* B = tbuf.data() + (size_t)omp_get_thread_num() * 4 * n;
        double *bfx = B, *bfy = B + n, *bfz = B + 2 * n, *bep = B + 3 * n;
#pragma omp for schedule(dynamic, 64)
        for (int i = 0; i < n; i++) {
            double fxi = 0, fyi = 0, fzi = 0, ei = 0;
            const PackA& A = pk[i]; const PairP* PTi = PT[A.ty];
            const double xi = A.x, yi = A.y, zi = A.z, qi = A.q;
            const bool bi = (A.flags & 1) != 0, gi = (A.flags & 2) != 0;
            for (int p = nlStart[i]; p < nlStart[i + 1]; p++) {
                const int j = nlIdx[p]; if (j < i) continue;
                const PackA& Bj = pk[j];
                double dx = Bj.x - xi, dy = Bj.y - yi, dz = Bj.z - zi;
                if (per) { dx = minImg(dx, Lx, iLx); dy = minImg(dy, Ly, iLy); dz = minImg(dz, Lz, iLz); }
                const double r2 = dx * dx + dy * dy + dz * dz;
                if (r2 > rcMax2 || r2 < 1e-12) continue;
                const bool same = bi && (Bj.flags & 1) && A.mol == Bj.mol;
                if ((gi || (Bj.flags & 2) || same) && excluded(i, j)) continue;
                const PairP& pp = PTi[Bj.ty];
                double ff = 0, e = 0;   // ff = −(dU/dr)/r
                if (r2 < pp.rc2) {      // Леннард-Джонс: U = 4ε[(σ/r)^12 − (σ/r)^6] − U(rc)
                    double sr2 = pp.sig2 / r2, sr6 = sr2 * sr2 * sr2;
                    e += pp.eps4 * (sr6 * sr6 - sr6) - pp.shift;
                    ff += pp.eps4 * (12 * sr6 * sr6 - 6 * sr6) / r2;
                }
                const double qq = qi * Bj.q;
                if (qq != 0 && r2 < rcc2) { double fc; e += coulDSF(qq, r2, fc); ff += fc; }
                if (same && pair14(i, j)) { e *= SCALE14; ff *= SCALE14; }
                fxi -= ff * dx; fyi -= ff * dy; fzi -= ff * dz;
                bfx[j] += ff * dx; bfy[j] += ff * dy; bfz[j] += ff * dz; bep[j] += 0.5 * e;
                ei += 0.5 * e; enb += e; vir += ff * r2;
            }
            bfx[i] += fxi; bfy[i] += fyi; bfz[i] += fzi; bep[i] += ei;
        }
    }
#pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        double fx = 0, fy = 0, fz = 0, ep = 0;
        for (int t = 0; t < nt; t++) { const double* B = tbuf.data() + (size_t)t * 4 * n; fx += B[i]; fy += B[n + i]; fz += B[2 * n + i]; ep += B[3 * n + i]; }
        S.fx[i] = fx; S.fy[i] = fy; S.fz[i] = fz; S.ep[i] = ep;
    }
    // --- стенки (9-3), гравитация
    double ewall = 0, egrav = 0, fw = 0, fp = 0;
    const double g = P.gravity;
    if (!per) {
#pragma omp parallel for reduction(+ : ewall, egrav, fw, fp) if (n > OMP_MIN)
        for (int i = 0; i < n; i++) {
            const Element& e = EL[S.ty[i]]; if (e.fixed) continue;
            double U, F, s = e.sig, eu = 0;
            wallTerm(S.x[i], s, U, F);      S.fx[i] += F; eu += U; fw += F;
            wallTerm(Lx - S.x[i], s, U, F); S.fx[i] -= F; eu += U; fw += F;
            wallTerm(S.y[i], s, U, F);      S.fy[i] += F; eu += U; fw += F;
            wallTerm(Ly - S.y[i], s, U, F); S.fy[i] -= F; eu += U; fw += F; fp += F;
            wallTerm(S.z[i], s, U, F);      S.fz[i] += F; eu += U; fw += F;
            wallTerm(Lz - S.z[i], s, U, F); S.fz[i] -= F; eu += U; fw += F;
            ewall += eu; S.ep[i] += eu;
            if (g != 0) { S.fy[i] -= e.m * g; egrav += e.m * g * S.y[i]; }   // гравитация только в ящике со дном
        }
    }
    // --- металлическая связь (многочастичная)
    if (anyMetal) enb += metalForces(vir);
    // --- внешнее электрическое поле: F = qE (работа учитывается как внешняя)
    if (P.efield != 0) for (int i = 0; i < n; i++) if (S.q[i] != 0 && !EL[S.ty[i]].fixed) S.fx[i] += S.q[i] * P.efield;
    // --- объекты поля (притягатели, барьеры, ветер…)
    fieldObjForces();
    // --- пинцет: пружина с критическим затуханием
    if (grabbed >= 0 && grabbed < n) { double Fx, Fy, Fz; tweezerForce(Fx, Fy, Fz); S.fx[grabbed] += Fx; S.fy[grabbed] += Fy; S.fz[grabbed] += Fz; }
    // --- аварийное ограничение силы
    int capped = 0;
    for (int i = 0; i < n; i++) {
        if (frozenAt(i)) continue;
        double f2 = S.fx[i] * S.fx[i] + S.fy[i] * S.fy[i] + S.fz[i] * S.fz[i];
        if (f2 > cfg::F_CAP * cfg::F_CAP) { double s = cfg::F_CAP / std::sqrt(f2); S.fx[i] *= s; S.fy[i] *= s; S.fz[i] *= s; capped++; }
    }
    EN.enb = enb + ewall; EN.egrav = egrav; EN.vir = vir; EN.fwall = fw; EN.fpiston = fp; EN.capped += capped;
}
// наибольшие ускорения от медленных и от быстрых сил — для выбора шага
static double amaxSlow = 0, amaxFast = 0;
static void measureAccel() {
    double as = 0, af = 0, at = 0; int ai = -1;
    for (int i = 0; i < S.n; i++) {
        if (frozenAt(i)) continue;
        const double im2 = 1.0 / (EL[S.ty[i]].m * EL[S.ty[i]].m);
        const double s2 = (S.fx[i] * S.fx[i] + S.fy[i] * S.fy[i] + S.fz[i] * S.fz[i]) * im2, f2 = (S.bx[i] * S.bx[i] + S.by[i] * S.by[i] + S.bz[i] * S.bz[i]) * im2;
        const double tx = S.fx[i] + S.bx[i], ty = S.fy[i] + S.by[i], tz = S.fz[i] + S.bz[i], t2 = (tx * tx + ty * ty + tz * tz) * im2;
        as = std::max(as, s2); af = std::max(af, f2); if (t2 > at) { at = t2; ai = i; }
    }
    amaxSlow = std::sqrt(as); amaxFast = std::sqrt(af); EN.amax = std::sqrt(at); EN.amaxI = ai;
}
static void computeForces() {
    computeSlow();
    computeFast(true);
    EN.vir += virFast;
    measureAccel();
}

// ===================================== INTEGRATOR / THERMOSTATS =========================
static double kinetic(int* nmob = nullptr) {
    double K = 0; int m = 0;
#pragma omp parallel for reduction(+ : K, m) if (S.n > OMP_MIN)
    for (int i = 0; i < S.n; i++) {
        if (frozenAt(i)) continue;
        K += 0.5 * EL[S.ty[i]].m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]); m++;
    }
    if (nmob) *nmob = m;
    return K;
}
// число степеней свободы: d·N − d, если полный импульс сохраняется (периодический ящик без неподвижных
// и закреплённых атомов и без объектов поля), иначе d·N
static int dofCount(int nmob) {
    const bool momentum = isPer() && nmob == S.n && !foAnyOn();
    return std::max(1, DIM * nmob - (momentum ? DIM : 0));
}
static void scaleVel(double s) {
#pragma omp parallel for if (S.n > OMP_MIN)
    for (int i = 0; i < S.n; i++) { S.vx[i] *= s; S.vy[i] *= s; S.vz[i] *= s; }
}
// Нозе–Гувер (разбиение Троттера): dξ/dt = (2K − N_f kT0)/Q, dv/dt = F/m − ξv
static void nhHalf(double dt) {
    int nm; double K = kinetic(&nm); int nf = dofCount(nm);
    double T0 = P.Tset, Q = nf * T0 * 0.25;
    if (nm == 0 || Q <= 0) return;
    double G = (2 * K - nf * T0) / Q; S.xi += G * dt / 4;
    double s = std::exp(-S.xi * dt / 2); scaleVel(s); K *= s * s;
    S.eta += S.xi * dt / 2;
    G = (2 * K - nf * T0) / Q; S.xi += G * dt / 4;
}
static void measure();
static bool chemistryStep();
static void relaxBondOffsets(double dt);

// сила на поршень от мыши: пружина к положению курсора
static double pistonGrabForce() {
    if (!pistonGrab || P.boundary != B_PISTON) return 0;
    double k = 20.0, M = S.pistonM;
    return M * (k * (pistonTarget - S.Ly) - 2 * std::sqrt(k) * S.pistonV);
}
static bool gEvCheck = false; static FILE* gEvLog = nullptr; static double gEvSum = 0;   // отладка баланса энергии в событиях
static double pistonAccel() { return (EN.fpiston + pistonGrabForce() - P.pExt * pistonArea() - S.pistonM * P.gravity) / S.pistonM; }

static void barostatCRescale(double dtB);
static void applyFieldObjs(double dt);
static void physGuard(double K, double T);
static double guardDtScale = 1.0;   // временное ограничение шага после отката (плавно возвращается к 1)
static int baroCount = 0;

static bool respaOn = true;          // многошаговый интегратор для веществ со связями (переключатель во вкладке «Физика»)
static double slowStepA = 0.012;     // внешний шаг RESPA: смещение от медленных сил a·dt² не больше этого (σ)
static std::vector<double> stepUx, stepUy, stepUz, stepFx, stepFy, stepFz;   // начало шага: положения и силы ветра — для работы внешних сил
// полушаг быстрых сил (связи, углы)
static void kickFast(double h) {
    const int n = S.n;
#pragma omp parallel for if (n > OMP_MIN)
    for (int i = 0; i < n; i++) {
        if (frozenAt(i)) continue;
        const double im = h / EL[S.ty[i]].m;
        S.vx[i] += S.bx[i] * im; S.vy[i] += S.by[i] * im; S.vz[i] += S.bz[i] * im;
    }
}
static void drift(double h) {
    const int n = S.n;
#pragma omp parallel for if (n > OMP_MIN)
    for (int i = 0; i < n; i++) {
        if (frozenAt(i)) continue;
        S.x[i] += h * S.vx[i]; S.y[i] += h * S.vy[i]; S.z[i] += h * S.vz[i];
        S.ux[i] += h * S.vx[i]; S.uy[i] += h * S.vy[i]; S.uz[i] += h * S.vz[i];
    }
}
static void mdStep() {
    const double dt = P.dt; const int n = S.n;
    const bool per = isPer();
    const bool respa = respaOn && anyBondable;   // без связей быстрых сил нет — обычный шаг Верле
    const int nIn = respa ? cfg::RESPA_N : 1; const double h = dt / nIn;
    if (P.thermostat == TH_NOSE) nhHalf(dt);
    // работа поля E и ветра считается по смещению атомов за шаг: W = F·Δx (ветер — по формуле трапеций)
    const bool nc = foNC && (int)foFx.size() == n, track = nc || P.efield != 0;
    if (track) { stepUx = S.ux; stepUy = S.uy; stepUz = S.uz; }
    if (nc) { stepFx = foFx; stepFy = foFy; stepFz = foFz; }
    // Velocity Verlet (с RESPA — внешний полушаг медленных сил): v += dt/2·F/m
    double kpin = 0;
#pragma omp parallel for reduction(+ : kpin) if (n > OMP_MIN)
    for (int i = 0; i < n; i++) {
        if (frozenAt(i)) {   // закреплённый атом стоит; если ему только что задали скорость — она гасится (внешняя работа)
            if (S.pin[i] && (S.vx[i] != 0 || S.vy[i] != 0 || S.vz[i] != 0)) {
                kpin += 0.5 * EL[S.ty[i]].m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]); S.vx[i] = S.vy[i] = S.vz[i] = 0;
            }
            continue;
        }
        const double im = 0.5 * dt / EL[S.ty[i]].m;
        double fx = S.fx[i], fy = S.fy[i], fz = S.fz[i];
        if (!respa) { fx += S.bx[i]; fy += S.by[i]; fz += S.bz[i]; }
        S.vx[i] += fx * im; S.vy[i] += fy * im; S.vz[i] += fz * im;
    }
    Wext -= kpin;
    // поршень — массивная верхняя стенка: M·a = F_газа − P_внеш·A − M·g
    if (P.boundary == B_PISTON) {
        S.pistonV += 0.5 * dt * pistonAccel(); S.Ly += dt * S.pistonV;
        if (S.Ly < 3) { S.Ly = 3; S.pistonV = std::fabs(S.pistonV); }
        if (S.Ly > 400) { S.Ly = 400; S.pistonV = -std::fabs(S.pistonV); }
    }
    // перемещение; с RESPA — n внутренних шагов, на каждом только быстрые силы
    for (int k = 0; k < nIn; k++) {
        if (respa) kickFast(0.5 * h);
        drift(h);
        if (respa) { computeFast(false); kickFast(0.5 * h); }
    }
    // границы
#pragma omp parallel for if (n > OMP_MIN)
    for (int i = 0; i < n; i++) {
        if (per) {
            if (S.x[i] < 0 || S.x[i] >= S.Lx) S.x[i] -= S.Lx * std::floor(S.x[i] / S.Lx);
            if (S.y[i] < 0 || S.y[i] >= S.Ly) S.y[i] -= S.Ly * std::floor(S.y[i] / S.Ly);
            if (S.z[i] < 0 || S.z[i] >= S.Lz) S.z[i] -= S.Lz * std::floor(S.z[i] / S.Lz);
        } else {   // упругое отражение (страховка, обычно работает потенциал стенки)
            if (S.x[i] < 0) { S.x[i] = -S.x[i]; S.vx[i] = std::fabs(S.vx[i]); }
            if (S.x[i] > S.Lx) { S.x[i] = 2 * S.Lx - S.x[i]; S.vx[i] = -std::fabs(S.vx[i]); }
            if (S.y[i] < 0) { S.y[i] = -S.y[i]; S.vy[i] = std::fabs(S.vy[i]); }
            if (S.y[i] > S.Ly) { S.y[i] = std::max(0.0, 2 * S.Ly - S.y[i]); S.vy[i] = -std::fabs(S.vy[i]); }
            if (S.z[i] < 0) { S.z[i] = -S.z[i]; S.vz[i] = std::fabs(S.vz[i]); }
            if (S.z[i] > S.Lz) { S.z[i] = 2 * S.Lz - S.z[i]; S.vz[i] = -std::fabs(S.vz[i]); }
        }
    }
    computeForces();
    if (track) {
        double wq = 0, wnc = 0; const bool ncNow = nc && (int)foFx.size() == n;
#pragma omp parallel for reduction(+ : wq, wnc) if (n > OMP_MIN)
        for (int i = 0; i < n; i++) {
            if (frozenAt(i)) continue;
            const double dx = S.ux[i] - stepUx[i], dy = S.uy[i] - stepUy[i], dz = S.uz[i] - stepUz[i];
            wq += S.q[i] * dx;
            if (ncNow) wnc += 0.5 * ((stepFx[i] + foFx[i]) * dx + (stepFy[i] + foFy[i]) * dy + (stepFz[i] + foFz[i]) * dz);
        }
        Wext += P.efield * wq + wnc;
    }
#pragma omp parallel for if (n > OMP_MIN)
    for (int i = 0; i < n; i++) {
        if (frozenAt(i)) continue;
        const double im = 0.5 * dt / EL[S.ty[i]].m;
        double fx = S.fx[i], fy = S.fy[i], fz = S.fz[i];
        if (!respa) { fx += S.bx[i]; fy += S.by[i]; fz += S.bz[i]; }
        S.vx[i] += fx * im; S.vy[i] += fy * im; S.vz[i] += fz * im;
    }
    if (P.boundary == B_PISTON) {
        S.pistonV += 0.5 * dt * pistonAccel();
        Wext += pistonGrabForce() * S.pistonV * dt;   // работа руки над поршнем
    }
    if (P.thermostat == TH_NOSE) nhHalf(dt);

    // работа пинцета (внешняя сила) — для баланса энергии
    if (grabbed >= 0 && grabbed < S.n) {
        double Fx, Fy, Fz; tweezerForce(Fx, Fy, Fz);
        Wext += (Fx * S.vx[grabbed] + Fy * S.vy[grabbed] + Fz * S.vz[grabbed]) * dt;
    }

    int nm; double K = kinetic(&nm); int nf = dofCount(nm);
    double T = nm ? 2 * K / nf : 0;
    {   // мгновенное давление сильно шумит (удары о стенку) — экспоненциальное среднее по времени
        double Pin = per ? (2 * K + EN.vir) / (DIM * boxVolume()) : EN.fwall / wallArea();
        EN.Pavg = EN.pavgInit ? EN.Pavg + (Pin - EN.Pavg) * std::min(1.0, dt / 0.5) : Pin; EN.pavgInit = true;
    }
    // Берендсен: λ² = 1 + (dt/τ)(T0/T − 1)
    if (P.thermostat == TH_BERENDSEN && T > 1e-9) {
        double l2 = clampv(1 + dt / P.tauT * (P.Tset / T - 1), 0.81, 1.21);
        scaleVel(std::sqrt(l2)); Wext += K * (l2 - 1); K *= l2;
    }
    // Бусси–Донадио–Парринелло (CSVR): стохастическое масштабирование, даёт правильный канонический ансамбль
    //   K' = K + (1−c)(K₀(R₁² + ΣR²)/N_f − K) + 2R₁√(c(1−c)K₀K/N_f),  c = e^{−dt/τ}
    if (P.thermostat == TH_BUSSI && K > 1e-12 && nm > 0) {
        double K0 = 0.5 * nf * P.Tset, c = std::exp(-dt / P.tauT), r1 = grand();
        double sr2 = nf > 1 ? 2.0 * std::gamma_distribution<double>(0.5 * (nf - 1), 1.0)(rng) : 0.0;   // сумма квадратов N_f−1 нормальных чисел
        double Kn = K + (1 - c) * (K0 * (r1 * r1 + sr2) / nf - K) + 2 * r1 * std::sqrt(c * (1 - c) * K0 * K / nf);
        if (Kn > 1e-12) { scaleVel(std::sqrt(Kn / K)); Wext += Kn - K; K = Kn; }
    }
    // Ланжевен: трение и случайные толчки каждому атому, v → c·v + √((1−c²)kT/m)·ξ
    if (P.thermostat == TH_LANGEVIN && nm > 0) {
        double c = std::exp(-dt / P.tauT), s = std::sqrt(1 - c * c), dK = 0;
        for (int i = 0; i < n; i++) {
            const Element& e = EL[S.ty[i]]; if (frozenAt(i)) continue;
            double sd = s * std::sqrt(P.Tset / e.m), k0 = S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i];
            S.vx[i] = c * S.vx[i] + sd * grand(); S.vy[i] = c * S.vy[i] + sd * grand(); S.vz[i] = c * S.vz[i] + sd * grand();
            dK += 0.5 * e.m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i] - k0);
        }
        Wext += dK; K += dK;
    }
    // постоянная мощность нагрева/охлаждения: dK/dt = P·N (виден плато при фазовом переходе)
    if (P.thermostat == TH_POWER && K > 1e-9) {
        double dK = P.heatPower * nm * dt;
        double l2 = clampv(1 + dK / K, 0.5, 2.0);
        scaleVel(std::sqrt(l2)); Wext += K * (l2 - 1); K *= l2;
    }
    // тепловые стенки: отражённая молекула получает скорость из распределения стенки
    if (P.heatWalls && !per) {
        for (int i = 0; i < n; i++) {
            const Element& e = EL[S.ty[i]]; if (frozenAt(i)) continue;
            double s = e.sig * 1.2, K0 = 0.5 * e.m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]);
            int hit = -1;   // 0 — горячая стенка, 1 — холодная
            auto ray = [&](double Tw) { return std::sqrt(-2 * Tw / e.m * std::log(std::max(1e-12, urand()))); };
            auto tang = [&](double Tw) { return grand() * std::sqrt(Tw / e.m); };
            if (P.heatWalls == 1) {
                if (S.x[i] < s && S.vx[i] < 0) { S.vx[i] = ray(P.Thot); S.vy[i] = tang(P.Thot); S.vz[i] = tang(P.Thot); hit = 0; }
                else if (S.x[i] > S.Lx - s && S.vx[i] > 0) { S.vx[i] = -ray(P.Tcold); S.vy[i] = tang(P.Tcold); S.vz[i] = tang(P.Tcold); hit = 1; }
            } else if (P.heatWalls == 2) {
                if (S.y[i] < s && S.vy[i] < 0) { S.vy[i] = ray(P.Thot); S.vx[i] = tang(P.Thot); S.vz[i] = tang(P.Thot); hit = 0; }
            }
            if (hit >= 0) { double dK = 0.5 * e.m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]) - K0; Wext += dK; heatWallQ[hit] += dK; }
        }
    }
    // кисть нагрева / охлаждения (локальный термостат вдоль луча под курсором)
    if (heatBrush) {
        // нагрев — до ≈2500 K (пламя: так поджигают смеси), охлаждение — почти до абсолютного нуля
        double R2 = P.brushR * P.brushR, Tb = heatBrush > 0 ? std::max(2500.0 / cfg::U_T_K, 3 * P.Tset) : 0.01;
        for (int i = 0; i < n; i++) {
            const Element& e = EL[S.ty[i]]; if (frozenAt(i)) continue;
            if (rayDist2(S.x[i], S.y[i], S.z[i], brushO, brushD) > R2) continue;
            if ((S.x[i] - brushO[0]) * brushF[0] + (S.y[i] - brushO[1]) * brushF[1] + (S.z[i] - brushO[2]) * brushF[2] < brushCut) continue;
            double v2 = S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i];
            double K0 = 0.5 * e.m * v2, Ti = e.m * v2 / 3;   // «температура» атома
            if (K0 < 1e-6) { S.vx[i] += grand() * 0.1; S.vy[i] += grand() * 0.1; S.vz[i] += grand() * 0.1; }
            else { double l = std::sqrt(clampv(1 + 0.05 * (Tb / Ti - 1), 0.8, 1.3)); S.vx[i] *= l; S.vy[i] *= l; S.vz[i] *= l; }
            Wext += 0.5 * e.m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]) - K0;
        }
    }
    // химия: события меняют топологию → пересчитать силы
    if (P.chemistry && anyBondable) {
        double Eb = gEvCheck ? EN.enb + EN.ebond + EN.egrav + kinetic() : 0;
        long long a0 = CH.assoc, e0 = CH.exch, d0 = CH.diss;
        if (chemistryStep()) {
            computeForces();
            if (gEvCheck) {   // отладка: полная энергия не должна измениться ни на одном событии
                double Ea = EN.enb + EN.ebond + EN.egrav + kinetic();
                if (std::fabs(Ea - Eb) > 1e-6 && gEvLog) fprintf(gEvLog, "t=%.4f step %lld: ΔE=%.6e  (assoc %lld exch %lld diss %lld)\n", S.t, S.step, Ea - Eb, CH.assoc - a0, CH.exch - e0, CH.diss - d0);
                gEvSum += Ea - Eb;
            }
        }
    }
    if (anyBondable) relaxBondOffsets(dt);

    // баростат C-rescale (Бернетти–Бусси 2020): стохастическое масштабирование ящика, правильный NPT-ансамбль
    // (только периодические границы; вызывается раз в NB шагов с шагом NB·dt)
    if (P.npt && per) {
        const int NB = 5;
        if (++baroCount >= NB) { baroCount = 0; barostatCRescale(NB * dt); }
    } else baroCount = 0;
    // объекты поля: локальные термостаты, источники и стоки атомов
    if (!fieldObjs.empty()) applyFieldObjs(dt);
    // авто-dt: смещение за шаг ≤ 0.02σ и a·dt² ≤ 0.003σ (жёсткие столкновения горячих лёгких атомов)
    double vmax2 = 0;
    for (int i = 0; i < S.n; i++) if (!frozenAt(i)) vmax2 = std::max(vmax2, S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]);
    double vmax = std::sqrt(vmax2);
    // с RESPA ускорение от связей ограничивает внутренний шаг dt/n, а внешний — только столкновения и медленные силы
    const bool respaNow = respaOn && anyBondable;
    const double aLim = respaNow ? std::min(amaxSlow > 0 ? std::sqrt(slowStepA / amaxSlow) : 1e9, amaxFast > 0 ? cfg::RESPA_N * std::sqrt(0.003 / amaxFast) : 1e9)
                                 : (EN.amax > 0 ? std::sqrt(0.003 / EN.amax) : 1e9);
    // смещение самого быстрого атома за внешний шаг: 0.02σ, а с RESPA — 0.05σ (лёгкие атомы H колеблются внутри своих
    // связей, и это ведёт внутренний шаг; внешнему достаточно не проскакивать столкновения)
    double dtLim = std::min(vmax > 0 ? (respaNow ? 0.05 : 0.02) / vmax : 1e9, aLim);
    const double dtTop = P.dtBase * guardDtScale;   // после отката страж временно ограничивает шаг
    // Уменьшение — сразу до безопасного значения (иначе при резком росте сил энергия успевает «разогнаться»),
    // рост — ступенями по 10% не чаще раза в 100 шагов: интегратор Верле сохраняет энергию, только пока шаг постоянен,
    // и «пила» из частых мелких изменений давала бы заметный дрейф
    static long long dtChanged = 0;
    if (P.dt > dtLim) { P.dt = std::max(P.dtBase / 64, 0.85 * dtLim); dtChanged = S.step; }
    else if (P.dt > dtTop) { P.dt = std::max(P.dtBase / 64, dtTop); dtChanged = S.step; }
    else if (P.dt < 0.8 * std::min(dtTop, dtLim) && S.step - dtChanged >= 100) { P.dt = std::min({dtTop, 0.9 * dtLim, P.dt * 1.1}); dtChanged = S.step; }

    S.t += dt; S.step++;
    // страж устойчивости: не-конечные величины или «взрыв» → откат к хорошему снимку
    { int nm2; double K2 = kinetic(&nm2); physGuard(K2, nm2 ? 2 * K2 / dofCount(nm2) : 0); }
}

static void measure() {
    int nm; EN.ek = kinetic(&nm); EN.nmob = nm; EN.ndof = dofCount(nm);
    EN.T = nm ? 2 * EN.ek / EN.ndof : 0;                        // равнораспределение: K = (N_f/2)·kT
    EN.Pvir = (2 * EN.ek + EN.vir) / (DIM * boxVolume());       // вириал: PV = (2K + Σ r·F)/d (все вклады: пары, связи, H-связи, 3-частичные, металл)
    EN.Pwall = EN.fwall / wallArea();                            // механическое давление на стенки
    EN.P = EN.pavgInit ? EN.Pavg : (isPer() ? EN.Pvir : EN.Pwall);
    EN.epist = P.boundary == B_PISTON ? 0.5 * S.pistonM * S.pistonV * S.pistonV + P.pExt * pistonArea() * S.Ly + S.pistonM * P.gravity * S.Ly : 0;
    if (P.thermostat == TH_NOSE) {
        double Q = EN.ndof * P.Tset * 0.25;
        EN.enh = 0.5 * Q * S.xi * S.xi + EN.ndof * P.Tset * S.eta;   // энергия «бани» Нозе–Гувера
    } else EN.enh = 0;
}
static int energyEpoch = 0;   // растёт при каждом сбросе опорной энергии (правка сцены пользователем)
static void resetEnergyRef() { EN.pavgInit = false; measure(); Wext = 0; Eref = EN.total(); energyRefValid = true; conserving = true; EN.capped = 0; energyEpoch++; }

// ===================================== БАРОСТАТ C-RESCALE ================================
// Бернетти, Бусси (J. Chem. Phys. 153, 114107, 2020), изотропный вариант для ε = ln V:
//   dε = −(β/τp)(P0 − P_int − kT/V)·dt + √(2kTβ/(V·τp))·dW,   r → r·e^{dε/d},  v → v·e^{−dε/d}
// Стационарное распределение — точное NPT (не зависит от β и τp, они задают только скорость релаксации).
// Изменение энергии при масштабировании относится к внешней работе W, так что E − W по-прежнему проверяет интегратор.
static double baroBeta = 0.1;   // «сжимаемость» β (σ³/ε): параметр скорости отклика объёма
static void barostatCRescale(double dtB) {
    int nm; double K = kinetic(&nm); if (nm == 0 || S.n == 0) return;
    const double V = boxVolume();
    const double kT = canonicalTh(P.thermostat) || P.thermostat == TH_BERENDSEN ? P.Tset : 2 * K / dofCount(nm);
    const double Pint = (2 * K + EN.vir) / (DIM * V);
    const double tau = std::max(0.05, P.tauP);
    double deps = -(baroBeta / tau) * (P.pExt - Pint - kT / V) * dtB + std::sqrt(2 * kT * baroBeta * dtB / (V * tau)) * grand();
    deps = clampv(deps, -0.01, 0.01);
    // ящик не должен стать меньше двух радиусов обрезки (минимальный образ) и больше разумного
    const double Lmin = 2 * (rcMax + cfg::SKIN) + 0.1, Lnow = std::min({S.Lx, S.Ly, S.Lz});
    if (deps < 0 && Lnow * std::exp(deps / DIM) < Lmin) deps = 0;
    if (deps > 0 && V > 5e7) deps = 0;
    if (deps == 0) return;
    const double mu = std::exp(deps / DIM), imu = 1.0 / mu;
    const double E0 = K + EN.enb + EN.ebond + EN.egrav + EN.efo;
    S.Lx *= mu; S.Ly *= mu; S.Lz *= mu;
    const int n = S.n;
#pragma omp parallel for
    for (int i = 0; i < n; i++) {
        S.x[i] *= mu; S.y[i] *= mu; S.z[i] *= mu; S.ux[i] *= mu; S.uy[i] *= mu; S.uz[i] *= mu;
        if (!frozenAt(i)) { S.vx[i] *= imu; S.vy[i] *= imu; S.vz[i] *= imu; }
    }
    for (size_t i = 0; i < nlRX.size(); i++) { nlRX[i] *= mu; nlRY[i] *= mu; nlRZ[i] *= mu; }
    nlAffine *= mu;
    computeForces();
    Wext += kinetic() + EN.enb + EN.ebond + EN.egrav + EN.efo - E0;
}

// ===================================== ОБЪЕКТЫ ПОЛЯ: термостаты, источники, стоки ==========
// Потенциальная энергия системы (для учёта работы при добавлении/удалении атомов)
static inline double potentialNow() { return EN.enb + EN.ebond + EN.egrav + EN.efo; }
static void applyFieldObjs(double dt) {
    // 1) нагреватели / охладители: Ланжевен к Tset, частота γ·w(r), w = 1 внутри 0.7R и плавно → 0 к краю
    for (auto& o : fieldObjs) {
        if (o.kind != FO_HEATER && o.kind != FO_COOLER) continue;
        if (!o.on) { o.pw *= 0.97; continue; }
        const double R2 = o.R * o.R, Tz = std::max(0.0, o.Tset);
        double dK = 0;
        for (int i = 0; i < S.n; i++) {
            if (frozenAt(i)) continue;
            double rx, ry, rz; foRel(o.x, o.y, o.z, i, rx, ry, rz);
            double r2 = rx * rx + ry * ry + rz * rz; if (r2 >= R2) continue;
            double dS, w = taper(std::sqrt(r2), 0.7 * o.R, o.R, dS);
            double c = std::exp(-std::max(0.0, o.strength) * w * dt), m = EL[S.ty[i]].m, sd = std::sqrt((1 - c * c) * Tz / m);
            double k0 = S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i];
            S.vx[i] = c * S.vx[i] + sd * grand(); S.vy[i] = c * S.vy[i] + sd * grand(); S.vz[i] = c * S.vz[i] + sd * grand();
            dK += 0.5 * m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i] - k0);
        }
        Wext += dK;
        o.pw += (dK / std::max(dt, 1e-9) - o.pw) * std::min(1.0, dt / 5.0);   // сглаженная мощность (τ = 5: шум Ланжевена велик)
    }
    // 2) источники и стоки меняют число атомов
    bool changed = false; double Ebefore = 0; bool haveE = false;
    auto energyNow = [&]() { return kinetic() + potentialNow(); };
    for (auto& o : fieldObjs) {
        if (o.kind == FO_SINK) {
            if (!o.on) { o.pw *= 0.97; continue; }
            const double R2 = o.R * o.R, p = 1 - std::exp(-std::max(0.0, o.strength) * dt);
            std::vector<int> kill;
            for (int i = 0; i < S.n; i++) {
                if (frozenAt(i)) continue;
                double rx, ry, rz; foRel(o.x, o.y, o.z, i, rx, ry, rz);
                if (rx * rx + ry * ry + rz * rz >= R2 || urand() > p) continue;
                // целиком удаляется небольшая молекула (≤ 12 атомов); из сетки — только сам атом
                std::vector<int> m; m.push_back(i); bool big = false;
                for (size_t k = 0; k < m.size() && !big; k++) for (int q = 0; q < S.nbc[m[k]]; q++) {
                    int b = S.nb[m[k]][q]; if (std::find(m.begin(), m.end(), b) == m.end()) { m.push_back(b); if (m.size() > 12) { big = true; break; } }
                }
                if (big) kill.push_back(i); else for (int a : m) if (!frozenAt(a)) kill.push_back(a);
            }
            if (!kill.empty()) {
                if (!haveE) { Ebefore = energyNow(); haveE = true; }
                std::sort(kill.begin(), kill.end(), std::greater<int>()); kill.erase(std::unique(kill.begin(), kill.end()), kill.end());
                for (int a : kill) removeAtom(a);
                o.cnt += (long long)kill.size(); changed = true;
            }
            o.pw += ((double)kill.size() / std::max(dt, 1e-9) - o.pw) * std::min(1.0, dt / 2.0);
        } else if (o.kind == FO_EMITTER) {
            if (!o.on) { o.pw *= 0.97; continue; }
            int t = clampv(o.elem, 0, NEL - 1); if (EL[t].fixed) t = E_AR;
            o.acc = std::min(o.acc + std::max(0.0, o.strength) * dt, 4.0);
            int emitted = 0;
            while (o.acc >= 1.0 && S.n < 60000) {
                double ux, uy, uz; foUnitDir(o, ux, uy, uz);
                // точка в диске радиуса R поперёк направления, без перекрытий (до 6 попыток)
                bool placed = false;
                for (int tr = 0; tr < 6 && !placed; tr++) {
                    double px, py, pz;
                    {   // случайная точка в диске ⟂ u
                        double ax = std::fabs(ux) < 0.9 ? 1 : 0, ay = ax ? 0 : 1, az = 0;
                        double e1x = uy * az - uz * ay, e1y = uz * ax - ux * az, e1z = ux * ay - uy * ax, l1 = std::sqrt(e1x * e1x + e1y * e1y + e1z * e1z);
                        e1x /= l1; e1y /= l1; e1z /= l1;
                        double e2x = uy * e1z - uz * e1y, e2y = uz * e1x - ux * e1z, e2z = ux * e1y - uy * e1x;
                        double rr = o.R * std::sqrt(urand()), ph = 2 * PI * urand();
                        double c1 = rr * std::cos(ph), c2 = rr * std::sin(ph);
                        px = o.x + c1 * e1x + c2 * e2x; py = o.y + c1 * e1y + c2 * e2y; pz = o.z + c1 * e1z + c2 * e2z;
                    }
                    if (isPer()) { px -= S.Lx * std::floor(px / S.Lx); py -= S.Ly * std::floor(py / S.Ly); pz -= S.Lz * std::floor(pz / S.Lz); }
                    else {
                        const double mg = 0.6 * EL[t].sig;
                        if (px < mg || py < mg || pz < mg || px > S.Lx - mg || py > S.Ly - mg || pz > S.Lz - mg) continue;
                    }
                    bool ok = true;
                    for (int j = 0; j < S.n && ok; j++) {
                        double dx = S.x[j] - px, dy = S.y[j] - py, dz = S.z[j] - pz;
                        if (isPer()) { dx -= S.Lx * std::nearbyint(dx / S.Lx); dy -= S.Ly * std::nearbyint(dy / S.Ly); dz -= S.Lz * std::nearbyint(dz / S.Lz); }
                        double s = 0.9 * std::sqrt(PT[t][S.ty[j]].sig2); if (s < 0.5) s = 0.5;
                        if (dx * dx + dy * dy + dz * dz < s * s) ok = false;
                    }
                    if (!ok) continue;
                    if (!haveE) { Ebefore = energyNow(); haveE = true; }
                    // скорость «сопла»: ½mu² = 5/2·kT (энтальпия идеального газа) + небольшой тепловой разброс
                    const double m = EL[t].m, Tz = std::max(0.01, o.Tset), u = std::sqrt(5 * Tz / m), sd = std::sqrt(0.1 * Tz / m);
                    addAtom(t, px, py, pz, u * ux + sd * grand(), u * uy + sd * grand(), u * uz + sd * grand());
                    placed = true; emitted++;
                }
                o.acc -= 1.0;
                if (!placed) { o.acc += 1.0; break; }   // место занято — попробуем на следующем шаге
            }
            if (emitted) { o.cnt += emitted; changed = true; }
            o.pw += ((double)emitted / std::max(dt, 1e-9) - o.pw) * std::min(1.0, dt / 2.0);
        }
    }
    if (changed) {   // пересчёт: новые типы, соседи, силы; изменение энергии — внешняя работа
        updatePresence(); nlValid = false; computeForces();
        if (haveE) Wext += energyNow() - Ebefore;
    }
    // 3) плавное включение консервативных объектов: U = λ·U₁, работа dW = U₁·dλ (половина — сейчас, половина —
    //    при следующем расчёте сил, см. fieldObjForces)
    for (int k = 0; k < (int)fieldObjs.size(); k++) {
        FieldObj& o = fieldObjs[k];
        if (!o.on) { o.ramp = 0; continue; }
        double nr = std::min(1.0, std::max(0.0, o.ramp) + dt / 1.0), dl = nr - o.ramp;
        if (dl > 0 && foConservative(o.kind)) { Wext += 0.5 * dl * foUnitEnergy(o); foRampPend.push_back({k, dl}); }
        o.ramp = nr;
    }
}

// ===================================== СТРАЖ УСТОЙЧИВОСТИ ================================
// Раз в 200 шагов — снимок «хорошего» состояния (кольцо из двух). Если в состоянии появились не-конечные числа
// или T за ≤ 400 шагов выросла в 50× (и при этом нарушен баланс E − W) — откат к более старому снимку, шаг dt
// уменьшается вдвое и лишь постепенно возвращается. Песочница не «умирает» даже при неудачных параметрах.
struct PhysSnap { Sim s; double Wext = 0, Eref = 0, T = 0, dt = 0, E = 0; int epoch = -1; bool ok = false; };
static PhysSnap guardSnap[2];
static int guardCounter = 0;
static inline double energyForGuard(double K) {
    double e = K + EN.enb + EN.ebond + EN.egrav + EN.efo;
    if (P.boundary == B_PISTON) e += 0.5 * S.pistonM * S.pistonV * S.pistonV + P.pExt * pistonArea() * S.Ly + S.pistonM * P.gravity * S.Ly;
    return e;
}
static void guardRepair() {   // снимка нет: убрать атомы с не-конечными координатами, обнулить не-конечные скорости
    for (int i = S.n - 1; i >= 0; i--) {
        if (!std::isfinite(S.x[i]) || !std::isfinite(S.y[i]) || !std::isfinite(S.z[i])) { removeAtom(i); continue; }
        if (!std::isfinite(S.vx[i]) || !std::isfinite(S.vy[i]) || !std::isfinite(S.vz[i])) S.vx[i] = S.vy[i] = S.vz[i] = 0;
        for (int k = 0; k < S.nbc[i]; k++) if (!std::isfinite(S.bc[i][k])) S.bc[i][k] = 0;
        if (!std::isfinite(S.q[i])) updateCharge(i);
    }
    if (!std::isfinite(S.xi) || !std::isfinite(S.eta)) S.xi = S.eta = 0;
    if (!std::isfinite(S.pistonV)) S.pistonV = 0;
}
static bool guardEnabled = true;   // страж включён (переключатель во вкладке «Физика»)
static void physGuard(double K, double T) {
    if (!guardEnabled) { guardSnap[0].ok = guardSnap[1].ok = false; return; }
    const double Ep = EN.enb + EN.ebond + EN.egrav + EN.efo;
    const char* why = nullptr;
    if (!std::isfinite(K) || !std::isfinite(Ep) || !std::isfinite(S.Lx * S.Ly * S.Lz)) why = "не-конечные числа";
    else {
        const PhysSnap& r = guardSnap[0];
        if (r.ok && T > 2.0 && T > 50 * std::max(0.05, r.T)) {
            // резкий нагрев: настоящий (внешний) или численный? численный нарушает баланс E − W
            bool energyBad = r.epoch != energyEpoch || std::fabs((energyForGuard(K) - Wext) - (r.E - r.Wext)) > 0.3 * K;
            if (energyBad) why = "температура выросла в 50 раз";
        }
    }
    if (!why) {
        guardDtScale = std::min(1.0, guardDtScale * 1.0007);
        // снимок раз в 200 шагов, а после правки сцены пользователем (новая опорная энергия) — сразу
        if (++guardCounter >= 200 || guardSnap[0].epoch != energyEpoch) {
            guardCounter = 0;
            guardSnap[1] = std::move(guardSnap[0]);
            PhysSnap& s = guardSnap[0];
            s.s = S; s.Wext = Wext; s.Eref = Eref; s.T = T; s.dt = P.dt; s.E = energyForGuard(K); s.epoch = energyEpoch; s.ok = true;
        }
        return;
    }
    // откат: к более старому снимку (более свежий мог уже содержать зародыш неустойчивости);
    // только к снимкам текущей сцены (после последней правки пользователя)
    auto usable = [](const PhysSnap& s) { return s.ok && s.epoch == energyEpoch; };
    int k = usable(guardSnap[1]) ? 1 : (usable(guardSnap[0]) ? 0 : -1);
    physRollbacks++; physAlertTime = physClock();
    guardDtScale = std::max(1.0 / 32, guardDtScale * 0.5);
    if (k < 0) {
        guardRepair();
        P.dt = std::max(P.dtBase / 64, P.dt * 0.5);
        updatePresence(); nlValid = false; computeForces(); resetEnergyRef();
        physAlert = fmt("Неустойчивость (%s): повреждённые атомы убраны, шаг dt уменьшен до %.4f", ::T(why), P.dt);
        return;
    }
    PhysSnap sn = guardSnap[k];
    S = sn.s; Wext = sn.Wext; Eref = sn.Eref;
    guardSnap[0] = sn; guardSnap[1].ok = false; guardCounter = 0;
    if (grabbed >= S.n) grabbed = -1;
    if (followAtom >= S.n) followAtom = -1;
    updatePresence();
    P.dt = std::max(P.dtBase / 64, std::min(P.dt, sn.dt) * 0.5);
    nlValid = false; computeForces(); EN.pavgInit = false;
    physAlert = fmt("Неустойчивость (%s) — откат к t = %.2f, шаг dt уменьшен до %.4f", ::T(why), S.t, P.dt);
}

// ===================================== РЕАЛЬНЫЕ ЕДИНИЦЫ ==================================
// Аргоноподобная шкала (см. cfg::U_*): ε/k = 139.8 K, σ = 0.3405 нм, единица массы 10 а.е.м.
static inline double toKelvin(double T) { return T * cfg::U_T_K; }
static inline double toAtm(double Pr) { return Pr * cfg::U_P_ATM; }
static inline double toBar(double Pr) { return Pr * cfg::U_P_ATM * 1.01325; }
static inline double toMPa(double Pr) { return Pr * cfg::U_P_ATM * 0.101325; }
static inline double toNm(double L) { return L * cfg::U_L_NM; }
static inline double toPs(double t) { return t * cfg::U_T_PS; }
static inline double toKJmol(double e) { return e * cfg::U_KJMOL; }   // энергия на частицу (ε) → кДж/моль
static inline double toEV(double e) { return e / cfg::EV; }            // 1 эВ = 83.0 ε
// объём, доступный веществу
static double realVolumeCm3() { const double s = cfg::U_L_NM * 1e-7; return boxVolume() * s * s * s; }
// массовая плотность смеси в ящике (г/см³) по реальным массам подвижных атомов
static double massDensityNow() {
    double m = 0; for (int i = 0; i < S.n; i++) if (!EL[S.ty[i]].fixed) m += EL[S.ty[i]].m * 10.0;   // а.е.м.
    return m * 1.66053907e-24 / std::max(1e-40, realVolumeCm3());
}
// числовая плотность подвижных атомов ρ* = N/V (σ⁻³) и в моль/л
static double numberDensity() { int c = 0; for (int i = 0; i < S.n; i++) if (!EL[S.ty[i]].fixed) c++; return c / std::max(1e-12, boxVolume()); }
static double molarConc() { return numberDensity() / (std::pow(cfg::U_L_NM * 1e-8, 3) * 6.02214076e23) ; }   // моль/л (1 л = 10⁻³ м³)


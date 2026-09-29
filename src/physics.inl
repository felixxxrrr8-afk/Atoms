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
static double rcCoul = cfg::RC_COUL, coulShift = 0, coulFs = 0;   // сдвиг энергии и силы (shifted-force)
static bool anyCharge = false, anyBondable = false, anyMetal = false;
static bool present[NEL];
static bool nlValid = false;          // список Верле действителен

// ---- Металлическая связь: многочастичный потенциал второго момента (Гупта / Клери–Розато)
//   U = Σ_i [ ½Σ_j A·f(r_ij) − w_i·√ρ_i ],  ρ_i = Σ_j w_j·ξ²·g(r_ij)
//   f = e^{−p(r/r0−1)}·S(r), g = e^{−2q(r/r0−1)}·S(r), S — гладкое выключение на [1.25·r0, 1.65·r0]
//   w = 1 − (занятая валентность)/(валентность): атом, связанный с неметаллом (окисленный), теряет металлическую связь.
//   A и ξ подобраны так, что ГЦК-решётка имеет равновесное расстояние r0 = 2·r_мет и энергию когезии E_coh (1 эВ = 4ε).
namespace gup { constexpr double P = 10.0, Q = 3.0, R1 = 1.25, R2 = 1.65; }
struct GupP { double A = 0, xi2 = 0, r0 = 1, ir0 = 1, r1 = 0, r2 = 0; };
static GupP GP[NEL][NEL];
static double gupA[NEL], gupXi[NEL];
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
    double e1 = std::exp(-gup::P * x), e2 = std::exp(-2 * gup::Q * x);
    f = e1 * S; df = e1 * (dS - gup::P * g.ir0 * S);
    h = e2 * S; dh = e2 * (dS - 2 * gup::Q * g.ir0 * S);
}
static void calibrateGupta(double r0, double Ecoh, double& A, double& xi) {
    // оболочки ГЦК-решётки (в долях расстояния до ближайших соседей) и их заселённости
    const double sh[5] = {1.0, std::sqrt(2.0), std::sqrt(3.0), 2.0, std::sqrt(5.0)}; const int nsh[5] = {12, 6, 24, 12, 24};
    GupP g; g.r0 = r0; g.ir0 = 1 / r0; g.r1 = gup::R1 * r0; g.r2 = gup::R2 * r0;
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
        gupA[a] = gupXi[a] = 0;
        if (EL[a].metal && EL[a].ecoh > 0) calibrateGupta(2 * EL[a].rmet / 3.405, EL[a].ecoh * cfg::EV, gupA[a], gupXi[a]);
    }
    for (int a = 0; a < NEL; a++) for (int b = 0; b < NEL; b++) {
        GupP g;
        if (gupXi[a] > 0 && gupXi[b] > 0) {
            g.A = std::sqrt(gupA[a] * gupA[b]); g.xi2 = gupXi[a] * gupXi[b];
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
        double rc = rcf * sig;
        double sr6 = std::pow(sig / rc, 6.0);
        PairP& p = PT[a][b];
        p.sig2 = sig * sig; p.eps4 = 4 * eps; p.rc2 = rc * rc;
        p.shift = 4 * eps * (sr6 * sr6 - sr6);   // сдвиг: U(rc) = 0
        if (EL[a].metal && EL[b].metal) { p.eps4 = 0; p.rc2 = 0; p.shift = 0; }   // металл–металл: потенциал Гупты вместо LJ
    }
    buildGupta();
    if (P.epsScale != 1.0) for (int a = 0; a < NEL; a++) for (int b = 0; b < NEL; b++) { GP[a][b].A *= P.epsScale; GP[a][b].xi2 *= P.epsScale * P.epsScale; }
    // явные параметры пар вместо смешивания (NBFIX): H···H между молекулами — размер реальной H2 (≈2.1 Å),
    // а малое σ водорода остаётся только для водородной связи H···O
    auto nbfix = [](int a, int b, double sig, double eps) {
        eps *= P.epsScale; double rc = 2.5 * sig, sr6 = std::pow(sig / rc, 6.0);
        PairP p{sig * sig, 4 * eps, rc * rc, 4 * eps * (sr6 * sr6 - sr6)}; PT[a][b] = p; PT[b][a] = p;
    };
    nbfix(E_H, E_H, 0.62, 0.05);
    double e = cfg::K_COUL * std::exp(-rcCoul / cfg::L_DEBYE);
    coulShift = e / rcCoul;
    coulFs = e * (1.0 / (rcCoul * rcCoul) + 1.0 / (cfg::L_DEBYE * rcCoul));
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
static inline int usedVal(int i) { int s = 0; for (int k = 0; k < S.nbc[i]; k++) s += S.bo[i][k]; return s; }
static inline int freeVal(int i) { return EL[S.ty[i]].val - usedVal(i); }

static void updateCharge(int i) {
    const Element& e = EL[S.ty[i]];
    double q = e.fq;
    if (e.chi > 0) for (int k = 0; k < S.nbc[i]; k++) q += cfg::KAPPA_Q * (EL[S.ty[S.nb[i][k]]].chi - e.chi);
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
    if (P.boundary == B_PERIODIC) {
        dx -= S.Lx * std::nearbyint(dx / S.Lx); dy -= S.Ly * std::nearbyint(dy / S.Ly); dz -= S.Lz * std::nearbyint(dz / S.Lz);
    }
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
    S.fx[i] = S.fy[i] = S.fz[i] = 0; S.ux[i] = x; S.uy[i] = y; S.uz[i] = z;
    S.ty[i] = type; S.nbc[i] = 0; S.ghc[i] = 0; S.ep[i] = 0; S.q[i] = EL[type].fq;
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
        for (auto* v : {&S.x, &S.y, &S.z, &S.vx, &S.vy, &S.vz, &S.fx, &S.fy, &S.fz, &S.ux, &S.uy, &S.uz, &S.q, &S.ep}) (*v)[i] = (*v)[last];
        S.ty[i] = S.ty[last]; S.nb[i] = S.nb[last]; S.bo[i] = S.bo[last]; S.nbc[i] = S.nbc[last]; S.pin[i] = S.pin[last];
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
// Сетка ячеек (размер ≥ r_c + skin) → список соседей Верле для каждого атома.
// Список пересобирается, только когда какой-то атом сместился больше чем на skin/2 — это в разы быстрее
// (27 соседних ячеек против ~80 реальных соседей).
static std::vector<int> cellStart, cellAtoms, cellOf, neighList, neighCnt;
static int cnx = 0, cny = 0, cnz = 0; static bool cper = false;
static void buildCells(double cs) {
    const bool per = isPer();
    int nx = std::max(1, (int)(S.Lx / cs)), ny = std::max(1, (int)(S.Ly / cs)), nz = std::max(1, (int)(S.Lz / cs));
    if (per) { if (nx < 3) nx = 1; if (ny < 3) ny = 1; if (nz < 3) nz = 1; }
    nx = std::min(nx, 200); ny = std::min(ny, 200); nz = std::min(nz, 200);
    if (nx != cnx || ny != cny || nz != cnz || per != cper) {
        cnx = nx; cny = ny; cnz = nz; cper = per;
        size_t nc = (size_t)nx * ny * nz;
        neighList.assign(nc * 27, 0); neighCnt.assign(nc, 0);
        for (int iz = 0; iz < nz; iz++) for (int iy = 0; iy < ny; iy++) for (int ix = 0; ix < nx; ix++) {
            int c = (iz * ny + iy) * nx + ix, cnt = 0;
            for (int dz = -1; dz <= 1; dz++) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int jx = ix + dx, jy = iy + dy, jz = iz + dz;
                if (per) { jx = (jx + nx) % nx; jy = (jy + ny) % ny; jz = (jz + nz) % nz; }
                else if (jx < 0 || jy < 0 || jz < 0 || jx >= nx || jy >= ny || jz >= nz) continue;
                int id = (jz * ny + jy) * nx + jx; bool dup = false;
                for (int k = 0; k < cnt; k++) if (neighList[(size_t)c * 27 + k] == id) dup = true;
                if (!dup) neighList[(size_t)c * 27 + cnt++] = id;
            }
            neighCnt[c] = cnt;
        }
    }
    int nc = nx * ny * nz, n = S.n;
    cellStart.assign(nc + 1, 0); cellAtoms.resize(n); cellOf.resize(n);
    double sx = nx / S.Lx, sy = ny / S.Ly, sz = nz / S.Lz;
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
    buildCells(rl);
    const int n = S.n;
    nlCnt.assign(n, 0); nlStart.assign(n + 1, 0);
    auto scan = [&](int i, int* out) {
        int cnt = 0; const bool fi = EL[S.ty[i]].fixed; const int c = cellOf[i];
        for (int kk = 0; kk < neighCnt[c]; kk++) {
            const int cc = neighList[(size_t)c * 27 + kk];
            for (int p = cellStart[cc]; p < cellStart[cc + 1]; p++) {
                int j = cellAtoms[p]; if (j == i || (fi && EL[S.ty[j]].fixed)) continue;
                if (dist2(i, j) < rl2) { if (out) out[cnt] = j; cnt++; }
            }
        }
        return cnt;
    };
#pragma omp parallel for schedule(dynamic, 64)
    for (int i = 0; i < n; i++) nlCnt[i] = scan(i, nullptr);
    for (int i = 0; i < n; i++) nlStart[i + 1] = nlStart[i] + nlCnt[i];
    nlIdx.resize(std::max(1, nlStart[n]));
#pragma omp parallel for schedule(dynamic, 64)
    for (int i = 0; i < n; i++) scan(i, nlIdx.data() + nlStart[i]);
    nlRX = S.ux; nlRY = S.uy; nlRZ = S.uz; nlN = n; nlValid = true; nlRebuilds++; nlAffine = 1.0;
}
static void ensureNeighborList() {
    bool need = !nlValid || nlN != S.n;
    // сжатие ящика баростатом сближает все пары на (1 − μ)·r_list — это съедает часть «кожи»
    const double half = 0.5 * (cfg::SKIN - std::max(0.0, 1.0 - nlAffine) * (rcMax + cfg::SKIN));
    if (half <= 0.02) need = true;
    if (!need) {
        const double lim = half * half; int bad = 0;
#pragma omp parallel for reduction(| : bad)
        for (int i = 0; i < S.n; i++) {
            double dx = S.ux[i] - nlRX[i], dy = S.uy[i] - nlRY[i], dz = S.uz[i] - nlRZ[i];
            if (dx * dx + dy * dy + dz * dz > lim) bad = 1;
        }
        need = bad != 0;
    }
    if (need) buildNeighborList();
}

// ===================================== FORCES ==========================================
// Энергия пары (LJ + экранированный Кулон), с учётом исключений; r2 = |r_ij|²
static inline double pairEnergy(int i, int j, double r2) {
    if (r2 > rcMax2 || r2 < 1e-12) return 0;
    if (mayExclude(i, j) && excluded(i, j)) return 0;
    const PairP& pp = PT[S.ty[i]][S.ty[j]];
    double e = 0;
    if (r2 < pp.rc2) { double sr2 = pp.sig2 / r2, sr6 = sr2 * sr2 * sr2; e += pp.eps4 * (sr6 * sr6 - sr6) - pp.shift; }
    double qq = S.q[i] * S.q[j];
    if (qq != 0 && r2 < rcCoul * rcCoul) { double r = std::sqrt(r2); e += qq * (cfg::K_COUL * std::exp(-r / cfg::L_DEBYE) / r - coulShift + coulFs * (r - rcCoul)); }
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
// Связь: Морзе U = D[(1 − e^{−a(r−r0)})² − 1] + гладкое ядро 20·(s/r − 1)² при r < s = 0.6·r0
// (у одного Морзе при r → 0 барьер всего ~D, и очень «горячая» пара могла бы пройти сквозь друг друга)
static inline double bondPot(double D, double r0, double r, double& dUdr) {
    double e = std::exp(-cfg::MORSE_A * (r - r0));
    double U = D * ((1 - e) * (1 - e) - 1); dUdr = 2 * D * cfg::MORSE_A * e * (1 - e);
    double s = 0.6 * r0;
    if (r < s) { double q = s / r - 1; U += 20 * q * q; dUdr += 40 * q * (-s / (r * r)); }
    return U;
}
static inline double morseU(double D, double r0, double r) { double d; return bondPot(D, r0, r, d); }
// Равновесный валентный угол (град) атома типа t с k связями, по VSEPR; 0 — угловой член не нужен
static double vseprAngle(int t, int k) {
    if (k < 2) return 0;
    if (t == E_O) return k == 2 ? 104.5 : 113.0;                           // H2O; H3O+ — плоская пирамида
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
        S.fx[a] -= k2 * gax; S.fy[a] -= k2 * gay; S.fz[a] -= k2 * gaz;
        S.fx[b] -= k2 * gbx; S.fy[b] -= k2 * gby; S.fz[b] -= k2 * gbz;
        S.fx[c] += k2 * (gax + gbx); S.fy[c] += k2 * (gay + gby); S.fz[c] += k2 * (gaz + gbz);
    }
    return cfg::K_ANGLE * (cs - c0) * (cs - c0);
}
static double angleEnergyAt(int c, bool addForce = false) {
    double th = theta0Of(c); if (th <= 0) return 0;
    double c0 = std::cos(th * PI / 180), E = 0; int k = S.nbc[c];
    for (int a = 0; a < k; a++) for (int b = a + 1; b < k; b++) E += angleTerm(c, S.nb[c][a], S.nb[c][b], c0, addForce);
    return E;
}
// ---- водородная связь D–H···A (DREIDING, трёхчастичная, направленная)
static inline bool hbAcceptor(int t) { return t == E_O || t == E_N || t == E_CLM || t == E_F; }
static inline int hbDonor(int h) {   // атом D, к которому ковалентно привязан водород h (O, N или F), иначе −1
    if (S.ty[h] != E_H || S.nbc[h] != 1) return -1;
    int d = S.nb[h][0]; return (S.ty[d] == E_O || S.ty[d] == E_N || S.ty[d] == E_F) ? d : -1;
}
// U = D·[5x¹² − 6x¹⁰]·S(R)·cos⁴θ при θ > 90°, x = R0/R, S — гладкое выключение на [R_on, R_off]
static double hbTerm(int d, int h, int a, bool addForce, double* vir) {
    if ((S.ghc[h] && isGhost(h, a)) || (S.ghc[d] && isGhost(d, a))) return 0;   // только что разорванные пары — не H-связь
    double ux, uy, uz, vx, vy, vz; dvec(h, d, ux, uy, uz); dvec(h, a, vx, vy, vz);   // u = D − H, v = A − H
    double wx = vx - ux, wy = vy - uy, wz = vz - uz, R2 = wx * wx + wy * wy + wz * wz;
    if (R2 >= cfg::HB_ROFF * cfg::HB_ROFF) return 0;
    double lu = std::sqrt(ux * ux + uy * uy + uz * uz), lv = std::sqrt(vx * vx + vy * vy + vz * vz), R = std::sqrt(R2);
    if (lu < 1e-9 || lv < 1e-9 || R < 1e-9) return 0;
    double c = (ux * vx + uy * vy + uz * vz) / (lu * lv);
    if (c >= 0) return 0;
    double g = c * c * c * c, gp = 4 * c * c * c;
    // радиальная часть DREIDING при R ≥ R0; внутри — плоское дно −D (отталкивание дают LJ и Кулон, так нет жёсткой стенки)
    double base = -cfg::HB_D, dbase = 0;
    if (R > cfg::HB_R0) {
        double x = cfg::HB_R0 / R, x2 = x * x, x10 = x2 * x2 * x2 * x2 * x2, x12 = x10 * x2;
        base = cfg::HB_D * (5 * x12 - 6 * x10); dbase = cfg::HB_D * (-60 * x12 + 60 * x10) / R;
    }
    double Sw = 1, dS = 0;
    if (R > cfg::HB_RON) { double L = cfg::HB_ROFF - cfg::HB_RON, t = (R - cfg::HB_RON) / L; Sw = (1 - t) * (1 - t) * (1 + 2 * t); dS = -6 * t * (1 - t) / L; }
    double f = base * Sw, fp = dbase * Sw + base * dS;
    if (addForce) {
        double k1 = fp * g / R, k2 = f * gp, iuv = 1.0 / (lu * lv);
        // ∂c/∂r_D = v/(|u||v|) − c·u/|u|²,  ∂c/∂r_A = u/(|u||v|) − c·v/|v|²,  ∂R/∂r_A = w/R = −∂R/∂r_D
        double cDx = vx * iuv - c * ux / (lu * lu), cDy = vy * iuv - c * uy / (lu * lu), cDz = vz * iuv - c * uz / (lu * lu);
        double cAx = ux * iuv - c * vx / (lv * lv), cAy = uy * iuv - c * vy / (lv * lv), cAz = uz * iuv - c * vz / (lv * lv);
        double FAx = -(k1 * wx + k2 * cAx), FAy = -(k1 * wy + k2 * cAy), FAz = -(k1 * wz + k2 * cAz);
        double FDx = -(-k1 * wx + k2 * cDx), FDy = -(-k1 * wy + k2 * cDy), FDz = -(-k1 * wz + k2 * cDz);
        S.fx[a] += FAx; S.fy[a] += FAy; S.fz[a] += FAz; S.fx[d] += FDx; S.fy[d] += FDy; S.fz[d] += FDz;
        S.fx[h] -= FAx + FDx; S.fy[h] -= FAy + FDy; S.fz[h] -= FAz + FDz;
        if (vir) *vir += ux * FDx + uy * FDy + uz * FDz + vx * FAx + vy * FAy + vz * FAz;   // Σ r·F относительно H
    }
    return f * g;
}
// энергия всех водородных связей, где h — водород (используется при расчёте ΔU химических событий)
static double hbEnergyOfH(int h) {
    int d = hbDonor(h); if (d < 0) return 0;
    double E = 0;
    for (int p = nlStart[h]; p < nlStart[h + 1]; p++) { int a = nlIdx[p]; if (a != d && hbAcceptor(S.ty[a]) && !bonded(a, d)) E += hbTerm(d, h, a, false, nullptr); }
    return E;
}
// ---- трёхчастичный член сетки воды (только кислороды молекул H2O)
static inline bool isWaterO(int i) {
    return S.ty[i] == E_O && S.nbc[i] == 2 && S.ty[S.nb[i][0]] == E_H && S.ty[S.nb[i][1]] == E_H;
}
// U3 с центром в i; при addForce добавляет силы
static double swCentered(int i, bool addForce, double* vir) {
    if (!isWaterO(i)) return 0;
    int nb[24]; double rv[24][3], rr[24]; int k = 0;
    for (int p = nlStart[i]; p < nlStart[i + 1] && k < 24; p++) {
        int j = nlIdx[p]; if (S.ty[j] != E_O || !isWaterO(j)) continue;
        double dx, dy, dz; dvec(i, j, dx, dy, dz); double r = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (r >= cfg::SW_A || r < 1e-6) continue;
        nb[k] = j; rv[k][0] = dx; rv[k][1] = dy; rv[k][2] = dz; rr[k] = r; k++;
    }
    if (k < 2) return 0;
    const double c0 = -1.0 / 3.0, lam = cfg::SW_LAMBDA, gam = cfg::SW_GAMMA, a = cfg::SW_A;
    double phi[24], dphi[24];
    for (int q = 0; q < k; q++) { double s = rr[q] - a; phi[q] = std::exp(gam / s); dphi[q] = -phi[q] * gam / (s * s); }
    double E = 0;
    for (int j = 0; j < k; j++) for (int m = j + 1; m < k; m++) {
        double cs = (rv[j][0] * rv[m][0] + rv[j][1] * rv[m][1] + rv[j][2] * rv[m][2]) / (rr[j] * rr[m]);
        double D = cs - c0, e = lam * phi[j] * phi[m] * D * D;
        E += e;
        if (!addForce) continue;
        // ∂E/∂r_j = λ[φ'_j φ_m D² r̂_j + 2φ_jφ_m D ∂cos/∂r_j], ∂cos/∂r_j = r_m/(r_j r_m) − cos·r_j/r_j²
        double A1 = lam * dphi[j] * phi[m] * D * D / rr[j], A2 = lam * phi[j] * dphi[m] * D * D / rr[m], B = 2 * lam * phi[j] * phi[m] * D;
        double inv = 1.0 / (rr[j] * rr[m]);
        double Fj[3], Fm[3];
        for (int c = 0; c < 3; c++) {
            double gj = rv[m][c] * inv - cs * rv[j][c] / (rr[j] * rr[j]), gm = rv[j][c] * inv - cs * rv[m][c] / (rr[m] * rr[m]);
            Fj[c] = -(A1 * rv[j][c] + B * gj); Fm[c] = -(A2 * rv[m][c] + B * gm);
        }
        int J = nb[j], M = nb[m];
        S.fx[J] += Fj[0]; S.fy[J] += Fj[1]; S.fz[J] += Fj[2]; S.fx[M] += Fm[0]; S.fy[M] += Fm[1]; S.fz[M] += Fm[2];
        S.fx[i] -= Fj[0] + Fm[0]; S.fy[i] -= Fj[1] + Fm[1]; S.fz[i] -= Fj[2] + Fm[2];
        if (vir) *vir += rv[j][0] * Fj[0] + rv[j][1] * Fj[1] + rv[j][2] * Fj[2] + rv[m][0] * Fm[0] + rv[m][1] * Fm[1] + rv[m][2] * Fm[2];
    }
    return E;
}
// ---- металлическая связь (Гупта): доля «металличности» атома и силы
static std::vector<double> gRho, gW;
static inline double metalW(int i) { const Element& e = EL[S.ty[i]]; if (e.val <= 0) return 1.0; return std::max(0.0, 1.0 - (double)usedVal(i) / e.val); }
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

static void computeForces() {
    ensureNeighborList();
    const int n = S.n; const bool per = isPer();
    const double Lx = S.Lx, Ly = S.Ly, Lz = S.Lz, invL = 1.0 / cfg::L_DEBYE, rcc2 = rcCoul * rcCoul, rcc = rcCoul;
    double enb = 0, vir = 0;
    // --- парные силы: каждый поток считает силу только на «свой» атом i → без гонок данных
#pragma omp parallel for schedule(dynamic, 64) reduction(+ : enb, vir)
    for (int i = 0; i < n; i++) {
        double fxi = 0, fyi = 0, fzi = 0, ei = 0, vi = 0;
        const int ti = S.ty[i];
        const double xi = S.x[i], yi = S.y[i], zi = S.z[i], qi = S.q[i];
        const bool bi = S.nbc[i] != 0, gi = S.ghc[i] != 0;
        for (int p = nlStart[i]; p < nlStart[i + 1]; p++) {
            const int j = nlIdx[p];
            double dx = S.x[j] - xi, dy = S.y[j] - yi, dz = S.z[j] - zi;
            if (per) { dx -= Lx * std::nearbyint(dx / Lx); dy -= Ly * std::nearbyint(dy / Ly); dz -= Lz * std::nearbyint(dz / Lz); }
            const double r2 = dx * dx + dy * dy + dz * dz;
            if (r2 > rcMax2 || r2 < 1e-12) continue;
            if ((gi || (bi && S.nbc[j])) && excluded(i, j)) continue;
            const PairP& pp = PT[ti][S.ty[j]];
            double ff = 0, e = 0;   // ff = −(dU/dr)/r
            if (r2 < pp.rc2) {      // Леннард-Джонс: U = 4ε[(σ/r)^12 − (σ/r)^6] − U(rc)
                double sr2 = pp.sig2 / r2, sr6 = sr2 * sr2 * sr2;
                e += pp.eps4 * (sr6 * sr6 - sr6) - pp.shift;
                ff += pp.eps4 * (12 * sr6 * sr6 - 6 * sr6) / r2;
            }
            const double qq = qi * S.q[j];
            if (qq != 0 && r2 < rcc2) {   // Юкава со сдвигом силы: U − U(rc) − (r−rc)·U'(rc)
                double r = std::sqrt(r2), ex = cfg::K_COUL * qq * std::exp(-r * invL);
                e += ex / r - qq * coulShift + qq * coulFs * (r - rcc);
                ff += ex * (1.0 / r + invL) / r2 - qq * coulFs / r;
            }
            fxi -= ff * dx; fyi -= ff * dy; fzi -= ff * dz; ei += 0.5 * e; vi += 0.5 * ff * r2;
        }
        S.fx[i] = fxi; S.fy[i] = fyi; S.fz[i] = fzi; S.ep[i] = ei; enb += ei; vir += vi;
    }
    // --- стенки (9-3), гравитация
    double ewall = 0, egrav = 0, fw = 0, fp = 0;
    const double g = P.gravity;
    if (!per) {
#pragma omp parallel for reduction(+ : ewall, egrav, fw, fp)
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
    // --- химические связи (Морзе) и валентные углы
    double eb = 0;
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < S.nbc[i]; k++) {
            int j = S.nb[i][k]; if (j < i) continue;
            const BondT& bt = BT[S.ty[i]][S.ty[j]]; int o = S.bo[i][k];
            double D = bt.D[o], r0 = bt.r0[o];
            double dx, dy, dz; dvec(i, j, dx, dy, dz);
            double r = std::sqrt(dx * dx + dy * dy + dz * dz); if (r < 1e-9) continue;
            double dUdr, U = bondPot(D, r0, r, dUdr) + S.bc[i][k];   // + сдвиг (не даёт силы)
            double ff = -dUdr / r;                                   // −(dU/dr)/r
            S.fx[i] -= ff * dx; S.fy[i] -= ff * dy; S.fz[i] -= ff * dz;
            S.fx[j] += ff * dx; S.fy[j] += ff * dy; S.fz[j] += ff * dz;
            eb += U; vir += ff * r * r; S.ep[i] += 0.5 * U; S.ep[j] += 0.5 * U;
        }
        if (S.nbc[i] >= 2) { double U = angleEnergyAt(i, true); eb += U; S.ep[i] += U; }
    }
    // --- водородные связи (есть только у водорода, связанного с O/N)
    double ehb = 0;
    if (anyBondable) {
        for (int h = 0; h < n; h++) {
            int d = hbDonor(h); if (d < 0) continue;
            for (int p = nlStart[h]; p < nlStart[h + 1]; p++) {
                int a = nlIdx[p]; if (a == d || !hbAcceptor(S.ty[a]) || bonded(a, d)) continue;
                double U = hbTerm(d, h, a, true, &vir);
                if (U != 0) { ehb += U; S.ep[h] += U / 3; S.ep[d] += U / 3; S.ep[a] += U / 3; }
            }
        }
        if (present[E_O]) for (int i = 0; i < n; i++) if (S.ty[i] == E_O) { double U = swCentered(i, true, &vir); if (U != 0) { ehb += U; S.ep[i] += U; } }
    }
    enb += ehb;
    // --- металлическая связь (многочастичная)
    if (anyMetal) enb += metalForces(vir);
    // --- внешнее электрическое поле: F = qE (работа учитывается как внешняя)
    if (P.efield != 0) for (int i = 0; i < n; i++) if (S.q[i] != 0 && !EL[S.ty[i]].fixed) S.fx[i] += S.q[i] * P.efield;
    // --- объекты поля (притягатели, барьеры, ветер…)
    fieldObjForces();
    // --- пинцет: пружина с критическим затуханием
    if (grabbed >= 0 && grabbed < n) { double Fx, Fy, Fz; tweezerForce(Fx, Fy, Fz); S.fx[grabbed] += Fx; S.fy[grabbed] += Fy; S.fz[grabbed] += Fz; }
    // --- аварийное ограничение силы + максимальное ускорение для авто-dt
    int capped = 0; double amax2 = 0; int amaxI = -1;
    for (int i = 0; i < n; i++) {
        if (frozenAt(i)) continue;
        double f2 = S.fx[i] * S.fx[i] + S.fy[i] * S.fy[i] + S.fz[i] * S.fz[i];
        if (f2 > cfg::F_CAP * cfg::F_CAP) { double s = cfg::F_CAP / std::sqrt(f2); S.fx[i] *= s; S.fy[i] *= s; S.fz[i] *= s; capped++; f2 = cfg::F_CAP * cfg::F_CAP; }
        double im = 1.0 / EL[S.ty[i]].m; if (f2 * im * im > amax2) { amax2 = f2 * im * im; amaxI = i; }
    }
    EN.amax = std::sqrt(amax2); EN.amaxI = amaxI;
    EN.enb = enb + ewall; EN.egrav = egrav; EN.ebond = eb; EN.vir = vir; EN.fwall = fw; EN.fpiston = fp; EN.capped += capped;
}

// ===================================== INTEGRATOR / THERMOSTATS =========================
static double kinetic(int* nmob = nullptr) {
    double K = 0; int m = 0;
#pragma omp parallel for reduction(+ : K, m)
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
#pragma omp parallel for
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

static void mdStep() {
    const double dt = P.dt; const int n = S.n;
    const bool per = isPer();
    if (P.thermostat == TH_NOSE) nhHalf(dt);
    // Velocity Verlet: v(t+dt/2) = v + dt/2·F/m;  x(t+dt) = x + dt·v(t+dt/2)
    const bool nc = foNC && (int)foFx.size() == n;
    double qv = 0, wnc = 0, kpin = 0;   // Σ q·v_x — работа электрического поля за шаг: E·Σq·Δx; wnc — работа ветра/вихря
#pragma omp parallel for reduction(+ : qv, wnc, kpin)
    for (int i = 0; i < n; i++) {
        if (frozenAt(i)) {   // закреплённый атом стоит; если ему только что задали скорость — она гасится (внешняя работа)
            if (S.pin[i] && (S.vx[i] != 0 || S.vy[i] != 0 || S.vz[i] != 0)) {
                kpin += 0.5 * EL[S.ty[i]].m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]); S.vx[i] = S.vy[i] = S.vz[i] = 0;
            }
            continue;
        }
        double im = 1.0 / EL[S.ty[i]].m;
        S.vx[i] += 0.5 * dt * S.fx[i] * im; S.vy[i] += 0.5 * dt * S.fy[i] * im; S.vz[i] += 0.5 * dt * S.fz[i] * im;
        S.x[i] += dt * S.vx[i]; S.y[i] += dt * S.vy[i]; S.z[i] += dt * S.vz[i];
        S.ux[i] += dt * S.vx[i]; S.uy[i] += dt * S.vy[i]; S.uz[i] += dt * S.vz[i];
        qv += S.q[i] * S.vx[i];
        if (nc) wnc += foFx[i] * S.vx[i] + foFy[i] * S.vy[i] + foFz[i] * S.vz[i];
    }
    if (P.efield != 0) Wext += P.efield * qv * dt;
    Wext -= kpin;
    // поршень — массивная верхняя стенка: M·a = F_газа − P_внеш·A − M·g
    if (P.boundary == B_PISTON) {
        S.pistonV += 0.5 * dt * pistonAccel(); S.Ly += dt * S.pistonV;
        if (S.Ly < 3) { S.Ly = 3; S.pistonV = std::fabs(S.pistonV); }
        if (S.Ly > 400) { S.Ly = 400; S.pistonV = -std::fabs(S.pistonV); }
    }
    // границы
#pragma omp parallel for
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
    // работа неконсервативных сил объектов (ветер, вихрь) по формуле трапеций: ½(F(t) + F(t+dt))·Δx
    if (foNC && (int)foFx.size() == n) {
        double w1 = 0;
#pragma omp parallel for reduction(+ : w1)
        for (int i = 0; i < n; i++) if (!frozenAt(i)) w1 += foFx[i] * S.vx[i] + foFy[i] * S.vy[i] + foFz[i] * S.vz[i];
        wnc += w1;
    } else if (nc) wnc *= 2;
    if (nc || foNC) Wext += 0.5 * wnc * dt;
#pragma omp parallel for
    for (int i = 0; i < n; i++) {
        if (frozenAt(i)) continue;
        double im = 1.0 / EL[S.ty[i]].m;
        S.vx[i] += 0.5 * dt * S.fx[i] * im; S.vy[i] += 0.5 * dt * S.fy[i] * im; S.vz[i] += 0.5 * dt * S.fz[i] * im;
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
        double R2 = P.brushR * P.brushR, Tb = heatBrush > 0 ? std::max(4.0, 3 * P.Tset) : 0.01;
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
    double dtLim = std::min(vmax > 0 ? 0.02 / vmax : 1e9, EN.amax > 0 ? std::sqrt(0.003 / EN.amax) : 1e9);
    const double dtTop = P.dtBase * guardDtScale;   // после отката страж временно ограничивает шаг
    // уменьшение — сразу до безопасного значения (раньше не более чем вдвое за шаг: при резком росте сил
    // несколько шагов подряд шли со слишком большим dt и энергия «разгонялась»); рост — плавно, на 1% за шаг
    if (P.dt > dtLim) P.dt = std::max(P.dtBase / 64, std::min(dtLim, P.dt * 0.8));
    else if (P.dt < dtTop) P.dt = std::min({dtTop, P.dt * 1.01, dtLim});
    else if (P.dt > dtTop) P.dt = std::max(P.dtBase / 64, dtTop);

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
static inline double toEV(double e) { return e / cfg::EV; }            // химическая шкала: 1 эВ = 4ε
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


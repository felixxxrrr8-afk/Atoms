// ===================================== ОРБИТАЛИ АТОМОВ В МОЛЕКУЛАХ =========================
// Валентные электроны атома в молекуле занимают «облака» — гибридные орбитали: каждая связь (σ), каждая неподелённая
// пара и неспаренный электрон. Облака расходятся как можно дальше друг от друга (VSEPR): два — по прямой (sp),
// три — треугольником (sp²), четыре — тетраэдром (sp³). Связи смотрят на соседей, остальные облака — в свободные стороны;
// у плоского радикала (CH3·) неспаренный электрон сидит на p-орбитали поперёк плоскости, у двойной связи π-облака
// лежат над ней и под ней. Направления считаются по текущему положению атомов, поэтому орбитали поворачиваются вместе
// с молекулой. В химии (chemistry.inl) они решают, возникнет ли связь: партнёры должны сойтись вдоль свободной
// орбитали — отсюда стерический фактор реакций. Радикал CH3 берёт атом по оси своей p-орбитали, атом Cl отрывает H
// у метана, подходя по линии связи C–H с тыла, радикал присоединяется к двойной связи сбоку, по π-облаку, а протон
// садится на неподелённую пару воды.
enum { ORB_PAIR, ORB_ONE, ORB_P };   // неподелённая пара, неспаренный электрон, p-орбиталь плоского радикала (две доли)
struct AtomOrbs {
    int nb = 0;                  // σ-связей
    int pairs = 0, single = 0;   // неподелённых пар и неспаренных электронов
    int n = 0;                   // свободных облаков с направлением (у одиночного атома их нет: облако сферическое)
    double d[6][3] = {}; unsigned char kind[6] = {};
};
namespace orb { constexpr double CONE = 0.5; }   // связь возникает в конусе ±60° вокруг оси свободной орбитали

static inline void vUnit(double* v) { const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 1e-12) { v[0] /= l; v[1] /= l; v[2] /= l; } }
static inline double vDot(const double* a, const double* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static inline void vCross(const double* a, const double* b, double* c) { c[0] = a[1] * b[2] - a[2] * b[1]; c[1] = a[2] * b[0] - a[0] * b[2]; c[2] = a[0] * b[1] - a[1] * b[0]; }
// вектор между атомами: по настоящим координатам (химия) или по рисуемым (render.inl подменяет на время рисования)
static void (*orbVec)(int, int, double&, double&, double&) = dvec;
static inline void bondUnit(int i, int j, double* u) { orbVec(i, j, u[0], u[1], u[2]); vUnit(u); }

// формальный заряд атома по его связям и заряду, без обхода молекулы: ионы-частицы, катионные центры H3O+ и NH4+,
// анионный кислород (OH−, алкоголят) и «голый» протон
static int orbCharge(int i) {
    const int t = S.ty[i];
    if (EL[t].fq != 0) return (int)std::lround(EL[t].fq);
    if (t == E_H) return S.nbc[i] == 0 && S.q[i] > 0.5 ? 1 : 0;
    const int u = usedVal(i);
    if (!HYPER[t] && EL[t].val > 0 && u > EL[t].val) return u - EL[t].val;
    if (t == E_O && S.nbc[i] <= 1 && S.q[i] < -0.8) return -1;
    return 0;
}
// единичный вектор поперёк оси a: к другим соседям атома j (соседа по оси), иначе — к оси мира, меньше всего похожей на a
static void perpRef(int i, int j, const double* a, double* e) {
    for (int k = 0; k < S.nbc[j]; k++) {
        const int q = S.nb[j][k]; if (q == i) continue;
        double w[3]; bondUnit(j, q, w); const double c = vDot(w, a);
        e[0] = w[0] - c * a[0]; e[1] = w[1] - c * a[1]; e[2] = w[2] - c * a[2];
        if (vDot(e, e) > 1e-4) { vUnit(e); return; }
    }
    const double ax = std::fabs(a[0]), ay = std::fabs(a[1]), az = std::fabs(a[2]);
    double w[3] = {0, 0, 0}; if (ax <= ay && ax <= az) w[0] = 1; else if (ay <= az) w[1] = 1; else w[2] = 1;
    const double c = vDot(w, a); e[0] = w[0] - c * a[0]; e[1] = w[1] - c * a[1]; e[2] = w[2] - c * a[2]; vUnit(e);
}
// нормаль к плоскости трёх связей атома (для sp²-атома — ось его p-орбитали); false — у атома не три связи
static bool sp2Normal(int i, double* n) {
    if (S.nbc[i] != 3) return false;
    double u[3][3]; for (int a = 0; a < 3; a++) bondUnit(i, S.nb[i][a], u[a]);
    double c[3]; n[0] = n[1] = n[2] = 0;
    for (int a = 0; a < 3; a++) { vCross(u[a], u[(a + 1) % 3], c); n[0] += c[0]; n[1] += c[1]; n[2] += c[2]; }
    if (vDot(n, n) < 1e-8) return false;
    vUnit(n); return true;
}
// облака атома i; false — у атома нет модели орбиталей (металл, стенка, благородный газ)
static bool atomOrbitals(int i, AtomOrbs& o) {
    const int t = S.ty[i]; const Element& e = EL[t];
    o = AtomOrbs();
    if (e.fixed || e.metal || e.Z < 1 || e.cat == CAT_NOB || VE[t] <= 0) return false;
    const int k = S.nbc[i], u0 = usedVal(i), fc = orbCharge(i), nbE = std::max(0, VE[t] - u0 - fc);
    // неспаренных электронов столько, сколько атому не хватает связей до его валентности (атом O — два, OH· — один,
    // CH3· — один); у иона лишний или недостающий электрон это число меняет (OH− и H3O+ насыщены), у атома с
    // расширенным октетом неспаренный остаётся, только если электронов нечётно (SF3·)
    int single = nbE & 1;
    if (!(HYPER[t] && u0 > EL[t].val)) {
        const int fr = EL[t].val - u0 + fc;
        if (fr > single) single = std::min(nbE, fr);
        if ((nbE - single) & 1) single--;
    }
    o.nb = k; o.single = single; o.pairs = std::min(4, (nbE - single) / 2);
    const int f = std::min(6 - k, o.pairs + o.single);
    if (k == 0 || f <= 0) return true;
    auto put = [&](double x, double y, double z) {
        if (o.n >= 6) return;
        double v[3] = {x, y, z}; vUnit(v);
        o.d[o.n][0] = v[0]; o.d[o.n][1] = v[1]; o.d[o.n][2] = v[2];
        o.kind[o.n] = (unsigned char)(o.n < o.pairs ? ORB_PAIR : ORB_ONE); o.n++;
    };
    // гипервалентный центр (SF4, ClF3, XeF2…): направления пар уже найдены минимумом отталкивания облаков (physics.inl)
    if (vseprCenter(i)) {
        const int nl = std::min(3, nbE / 2), unp = nbE & 1, key = 1 + nl * 2 + unp;
        if (S.lpk[i] == key) { for (int m = 0; m < nl + unp; m++) put(S.lp[i][3 * m], S.lp[i][3 * m + 1], S.lp[i][3 * m + 2]); return true; }
    }
    double u[cfg::MAXB][3], s[3] = {0, 0, 0};
    for (int a = 0; a < k; a++) { bondUnit(i, S.nb[i][a], u[a]); s[0] += u[a][0]; s[1] += u[a][1]; s[2] += u[a][2]; }
    const double sl = std::sqrt(vDot(s, s));
    if (k == 1) {
        const double* a = u[0]; double e1[3], e2[3]; perpRef(i, S.nb[i][0], a, e1); vCross(a, e1, e2);
        if (f == 1) put(-a[0], -a[1], -a[2]);   // sp: напротив связи (N≡N, C≡O)
        else if (f == 2) for (int sg = -1; sg <= 1; sg += 2)   // sp²: под 120° к связи, в плоскости соседей соседа (карбонил)
            put(-0.5 * a[0] + sg * 0.866 * e1[0], -0.5 * a[1] + sg * 0.866 * e1[1], -0.5 * a[2] + sg * 0.866 * e1[2]);
        else for (int m = 0; m < 3; m++) {   // sp³: конус под 109.5° к связи, в заторможенной конформации к соседям соседа
            const double ph = PI / 3 + 2 * PI / 3 * m, c = std::cos(ph) * 0.9428, sn = std::sin(ph) * 0.9428;
            put(-a[0] / 3 + c * e1[0] + sn * e2[0], -a[1] / 3 + c * e1[1] + sn * e2[1], -a[2] / 3 + c * e1[2] + sn * e2[2]);
        }
        return true;
    }
    if (k == 2 && sl > 0.15) {   // уголок: облака — по биссектрисе снаружи (sp²) или парой над и под плоскостью (sp³, вода)
        double b[3] = {-s[0] / sl, -s[1] / sl, -s[2] / sl}, n[3]; vCross(u[0], u[1], n); vUnit(n);
        if (f == 1) put(b[0], b[1], b[2]);
        else for (int sg = -1; sg <= 1 && o.n < f; sg += 2) put(0.577 * b[0] + sg * 0.816 * n[0], 0.577 * b[1] + sg * 0.816 * n[1], 0.577 * b[2] + sg * 0.816 * n[2]);
        if (f >= 3) put(b[0], b[1], b[2]);
        return true;
    }
    if (k == 2) {   // по прямой: свободные облака — кольцом поперёк оси
        double e1[3], e2[3]; perpRef(i, S.nb[i][0], u[0], e1); vCross(u[0], e1, e2);
        for (int m = 0; m < f; m++) { const double ph = 2 * PI * m / f; put(std::cos(ph) * e1[0] + std::sin(ph) * e2[0], std::cos(ph) * e1[1] + std::sin(ph) * e2[1], std::cos(ph) * e1[2] + std::sin(ph) * e2[2]); }
        return true;
    }
    if (k == 3 && f == 1) {
        double n[3];
        if (sl < 0.35 && sp2Normal(i, n)) {   // почти плоский: p-орбиталь поперёк плоскости, доля побольше — со стороны пирамиды
            if (vDot(n, s) > 0) { n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2]; }
            put(n[0], n[1], n[2]); o.kind[o.n - 1] = ORB_P; return true;
        }
        put(-s[0], -s[1], -s[2]); return true;   // пирамида (NH3, H3O+): облако по оси напротив связей
    }
    if (sl > 0.1) put(-s[0], -s[1], -s[2]);   // прочие (редкие) случаи: одно облако напротив связей
    return true;
}

// Может ли атом i принять новую связь в направлении u (единичный вектор от i к партнёру): u должен войти в свободную
// орбиталь. У одиночного атома облако сферическое — годится любое направление; у атома с одной связью и несколькими
// свободными облаками (OH·, Cl в Cl2) они сливаются в «шапку» — годится вся задняя полусфера
static bool orbitalOpen(int i, const double* u) {
    AtomOrbs o; if (!atomOrbitals(i, o) || o.nb == 0) return true;
    if (o.n == 0) return false;
    if (o.nb == 1 && o.n >= 2) { double a[3]; bondUnit(i, S.nb[i][0], a); return vDot(u, a) <= 0.1; }
    for (int m = 0; m < o.n; m++) {
        double c = vDot(u, o.d[m]); if (o.kind[m] == ORB_P) c = std::fabs(c);
        if (c >= orb::CONE) return true;
    }
    return false;
}
// перенос атома j от B к нападающему: он подходит к j по линии связи j–B с тыла (переходное состояние X···j···B —
// почти прямая, угол больше 120°)
static bool orbitalBackside(int j, int B, const double* u) { double v[3]; bondUnit(j, B, v); return vDot(u, v) <= -0.5; }
// присоединение по кратной связи j=B: нападающий подходит к j сбоку (не вдоль оси связи), а у плоского sp²-атома —
// по оси его p-орбитали, из которой сложено π-облако
static bool orbitalPiSide(int j, int B, const double* u) {
    double v[3]; bondUnit(j, B, v); if (std::fabs(vDot(u, v)) > 0.8) return false;
    double n[3]; if (sp2Normal(j, n)) return std::fabs(vDot(u, n)) >= orb::CONE;
    return true;
}

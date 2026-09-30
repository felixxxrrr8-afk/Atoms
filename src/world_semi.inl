// ===================================== ПОЛУПРОВОДНИКИ: ДИОД, СВЕТОДИОД, ТРАНЗИСТОРЫ =================================
// Сцены 120–123: сечение кристалла кремния (и прямозонных полупроводников светодиода) размером в микрометры.
// Расчёт — дрейфово-диффузионная модель, как в программах проектирования микросхем (TCAD):
//   ∇·(ε∇φ) = −q(p − n + N_D − N_A)                         — уравнение Пуассона
//   ∂n/∂t = ∇·Jn/q − R + G,   Jn = qμn·n·E + qDn·∇n          — непрерывность для электронов (и так же для дырок)
// Потоки — по схеме Шарфеттера — Гуммеля (устойчива при любых перепадах концентраций), шаг по времени неявный,
// уравнения решаются по очереди (метод Гуммеля) с последовательной верхней релаксацией. Рекомбинация —
// Шокли — Рид — Холл и излучательная (светодиод), генерация — светом (солнечный элемент).
// Параметры — справочные для кремния при 300 K: μn = 1400, μp = 450 см²/(В·с), nᵢ = 1.0·10¹⁰ см⁻³, ε = 11.7.
// Точки на рисунке — носители: каждая точка — несколько электронов или дырок; их плотность равна рассчитанной,
// а движутся они со скоростью потока J/(q·n).
namespace sc {
constexpr int NX = 96, NY = 48;
constexpr double Q = 1.602176634e-19, EPS0 = 8.8541878e-14, VT = 0.025852;   // Кл, Ф/см, kT/q при 300 K (В)
static int scene = 0;                         // 0 диод, 1 светодиод / солнечный элемент, 2 МОП-транзистор, 3 биполярный транзистор
static double Lx = 2e-4, Ly = 1e-4, hx = 1, hy = 1, depth = 1e-4;   // размеры (см), глубина сечения 1 мкм
static double eps = 11.7 * EPS0, ni = 1.0e10, mun = 1400, mup = 450, tauN = 1e-9, tauP = 1e-9, Brad = 0;   // материал
static double Eg = 1.12;                      // ширина запрещённой зоны, эВ
static std::vector<double> phi, n, p, C, gen; // потенциал (В), концентрации (см⁻³), легирование N_D − N_A, генерация
static std::vector<int> contact;              // −1 внутренний узел, иначе номер контакта
static std::vector<char> gateTop;             // верхний узел под затвором (граница с окислом)
static double Vc[4] = {0, 0, 0, 0};           // напряжения на контактах, В
static double Ic[4] = {0, 0, 0, 0};           // токи через контакты (сглаженные), А
static int nContacts = 2;
static const char* CNAME[4] = {"", "", "", ""};
static double Vg = 0, Cox = 0, gateWf = 0.55;  // затвор: напряжение, ёмкость окисла (Ф/см²), сдвиг работы выхода (n+ поликремний)
static double t = 0, dt = 1e-12, timeScale = 20e-12;   // время (с), шаг, секунд модели за секунду экрана
static double V1 = 0, V2 = 0;                 // ползунки сцены: диод — V; МОП — Vg, Vd; биполярный — Vbe, Vce
static bool light = false; static double lightP = 1.0;   // солнечный элемент: свет, интенсивность (в «солнцах»)
static int ledColor = 0, mode121 = 0;          // светодиод: 0 красный, 1 зелёный, 2 синий; режим: 0 светодиод, 1 солнечный элемент
static std::vector<std::pair<double, double>> ivPts;   // измеренные точки ВАХ (напряжение, ток)
static double settle = 0;                      // время с последнего изменения напряжений (для точек ВАХ)
static double photonAcc = 0; static double photonsPerS = 0;
// точки-носители для рисунка
struct Dot { float x, y; signed char s; };      // s: −1 электрон, +1 дырка
static std::vector<Dot> dots;
struct Photon { float x, y, dx, dy, life; };
static std::vector<Photon> photons;
static double dotW = 1;                         // сколько носителей в одной точке
static inline int id(int i, int j) { return j * NX + i; }
}

// B(x) = x/(eˣ − 1) — функция Бернулли схемы Шарфеттера — Гуммеля
static inline double scBern(double x) {
    const double a = std::fabs(x);
    if (a < 1e-6) return 1 - 0.5 * x;
    if (x > 700) return x * std::exp(-x);
    if (x < -700) return -x;
    return x / std::expm1(x);
}
// равновесные концентрации при легировании c = N_D − N_A
static inline void scEq(double c, double& n0, double& p0) {
    using namespace sc;
    if (c >= 0) { n0 = 0.5 * c + std::sqrt(0.25 * c * c + ni * ni); p0 = ni * ni / n0; }
    else { p0 = -0.5 * c + std::sqrt(0.25 * c * c + ni * ni); n0 = ni * ni / p0; }
}
// потенциал омического контакта: равновесный (φ = Vt·ln(n0/nᵢ), уровень Ферми — ноль) плюс приложенное напряжение
static inline double scContactPhi(int k) { using namespace sc; double n0, p0; scEq(C[k], n0, p0); return VT * std::log(n0 / ni) + Vc[contact[k]]; }
// материал и геометрия сцены
static void scBuild() {
    using namespace sc;
    const int NN = NX * NY;
    phi.assign(NN, 0); n.assign(NN, 0); p.assign(NN, 0); C.assign(NN, 0); gen.assign(NN, 0); contact.assign(NN, -1); gateTop.assign(NN, 0);
    Brad = 0; tauN = tauP = 1e-9; mun = 1400; mup = 450; Eg = 1.12; ni = 1.0e10; eps = 11.7 * EPS0; Cox = 0;
    for (auto& c : CNAME) c = "";
    switch (scene) {
    case 0: case 1: {   // p–n переход: p слева, n справа, контакты на торцах
        Lx = 2e-4; Ly = 1e-4;
        if (scene == 1 && mode121 == 0) {   // прямозонный полупроводник светодиода: ширина зоны задаёт цвет
            static const double EG[3] = {1.91, 2.34, 2.70};   // AlGaInP (650 нм), InGaN (530 нм), InGaN (460 нм)
            Eg = EG[clampv(ledColor, 0, 2)]; ni = 2.0e19 * std::exp(-Eg / (2 * VT)); mun = 4000; mup = 200; eps = 12.5 * EPS0; Brad = 2e-10; tauN = tauP = 1e-8;
        }
        const double NA = 1e16, ND = 1e16;
        for (int j = 0; j < NY; j++) for (int i = 0; i < NX; i++) C[id(i, j)] = i < NX / 2 ? -NA : ND;
        for (int j = 0; j < NY; j++) { contact[id(0, j)] = 0; contact[id(NX - 1, j)] = 1; }
        nContacts = 2; CNAME[0] = "p (анод)"; CNAME[1] = "n (катод)";
        break; }
    case 2: {   // n-МОП транзистор: исток и сток n+, подложка p, затвор над каналом через окисел 20 нм
        Lx = 2e-4; Ly = 1e-4;
        const double NA = 3e16, NDp = 1e19;
        for (int j = 0; j < NY; j++) for (int i = 0; i < NX; i++) {
            const double x = (i + 0.5) / NX * Lx, y = (j + 0.5) / NY * Ly;   // y — вглубь от поверхности
            const bool sd = y < 0.2e-4 && (x < 0.55e-4 || x > 1.45e-4);
            C[id(i, j)] = sd ? NDp : -NA;
        }
        for (int i = 0; i < NX; i++) {
            const double x = (i + 0.5) / NX * Lx;
            if (x < 0.35e-4) contact[id(i, 0)] = 0; else if (x > 1.65e-4) contact[id(i, 0)] = 1;
            else if (x > 0.55e-4 && x < 1.45e-4) gateTop[id(i, 0)] = 1;
            contact[id(i, NY - 1)] = 2;
        }
        Cox = 3.9 * EPS0 / 20e-7; mun = 600;   // у поверхности подвижность электронов меньше, чем в объёме
        nContacts = 3; CNAME[0] = "исток S"; CNAME[1] = "сток D"; CNAME[2] = "подложка B";   // буквы — как на схемах (и «сток» не путается со стоком атомов)
        break; }
    case 3: {   // биполярный n–p–n: эмиттер n+ | база p (0.3 мкм) | коллектор n. Вывод базы — сверху, над островком p+:
        // скачок легирования p+/p отталкивает электроны, и они не уходят в вывод базы, а пролетают к коллектору
        Lx = 2e-4; Ly = 1e-4;
        for (int j = 0; j < NY; j++) for (int i = 0; i < NX; i++) {
            const double x = (i + 0.5) / NX * Lx, y = (j + 0.5) / NY * Ly;
            C[id(i, j)] = x < 0.6e-4 ? 1e19 : (x < 0.9e-4 ? (y < 0.12e-4 && x > 0.66e-4 && x < 0.84e-4 ? -1e19 : -1e17) : 1e16);
        }
        for (int j = 0; j < NY; j++) { contact[id(0, j)] = 0; contact[id(NX - 1, j)] = 2; }
        for (int i = 0; i < NX; i++) { const double x = (i + 0.5) / NX * Lx; if (x > 0.68e-4 && x < 0.82e-4) contact[id(i, 0)] = 1; }
        tauN = tauP = 1e-8;
        nContacts = 3; CNAME[0] = "эмиттер"; CNAME[1] = "база"; CNAME[2] = "коллектор";
        break; }
    }
    hx = Lx / (NX - 1); hy = Ly / (NY - 1);
    for (int k = 0; k < NN; k++) { double n0, p0; scEq(C[k], n0, p0); n[k] = n0; p[k] = p0; phi[k] = VT * std::log(n0 / ni); }
    // точка на рисунке: столько носителей, чтобы в самой слабо легированной области была примерно одна точка на клетку
    double cmin = 1e30; for (int k = 0; k < NN; k++) cmin = std::min(cmin, std::fabs(C[k]));
    dotW = cmin * hx * hy * depth / 1.2;
}
// напряжения на контактах из ползунков сцены
static void scApplyBias() {
    using namespace sc;
    switch (scene) {
    case 0: case 1: Vc[0] = V1; Vc[1] = 0; break;          // анод под напряжением V, катод заземлён
    case 2: Vc[0] = 0; Vc[1] = V2; Vc[2] = 0; Vg = V1; break;   // исток и подложка — земля, сток Vd, затвор Vg
    case 3: Vc[0] = 0; Vc[1] = V1; Vc[2] = V2; break;       // эмиттер — земля, база Vbe, коллектор Vce
    }
    for (int k = 0; k < NX * NY; k++) if (contact[k] >= 0) { double n0, p0; scEq(C[k], n0, p0); n[k] = n0; p[k] = p0; phi[k] = scContactPhi(k); }
    settle = 0;
}
// прогонка (алгоритм Томаса): a — под диагональю, b — диагональ, c — над диагональю, d — правая часть; ответ — в d
static void scThomas(int N, double* a, double* b, double* c, double* d) {
    for (int i = 1; i < N; i++) { const double m = a[i] / b[i - 1]; b[i] -= m * c[i - 1]; d[i] -= m * d[i - 1]; }
    d[N - 1] /= b[N - 1];
    for (int i = N - 2; i >= 0; i--) d[i] = (d[i] - c[i] * d[i + 1]) / b[i];
}
// Узлы решаются целыми линиями — строками вдоль x и столбцами вдоль y по очереди (линейная релаксация): вдоль линии
// уравнения связаны точно (прогонкой), соседние линии берутся с прошлой итерации. Для приборов, где ток идёт вдоль x,
// это сходится в десятки раз быстрее поузловой релаксации.
// Пуассон с «замороженными» уровнями Ферми: n = n_old·e^{(φ−φ_old)/Vt}, p = p_old·e^{−(φ−φ_old)/Vt} (метод Ньютона)
static void scPoisson(int sweeps) {
    using namespace sc;
    static std::vector<double> phiOld; phiOld = phi;
    const double ax = 1 / (hx * hx), ay = 1 / (hy * hy), qe = Q / eps, rob = 2 * hy * Cox / eps;
    // невязка F и диагональ якобиана J в узле (i, j) при текущем φ
    auto FJ = [&](int i, int j, double& F, double& J) {
        const int k = id(i, j); const double pc = phi[k];
        const double pl = i > 0 ? phi[k - 1] : phi[k + 1], pr = i < NX - 1 ? phi[k + 1] : phi[k - 1];
        double lap = ax * (pl + pr - 2 * pc), diag = -2 * ax - 2 * ay;
        if (j == 0 && gateTop[k]) {   // граница с окислом: ε·∂φ/∂y = Cox·(Vg + Φ_ms − φ) — «призрачный» узел над поверхностью
            const double pdn = phi[k + NX], ghost = pdn + rob * (Vg + gateWf - pc);
            lap += ay * (ghost + pdn - 2 * pc); diag -= rob * ay;
        } else {
            const double pu = j > 0 ? phi[k - NX] : phi[k + NX], pd = j < NY - 1 ? phi[k + NX] : phi[k - NX];
            lap += ay * (pu + pd - 2 * pc);
        }
        const double e = std::exp(clampv((pc - phiOld[k]) / VT, -40.0, 40.0)), nn = n[k] * e, pp = p[k] / e;
        F = lap + qe * (pp - nn + C[k]); J = diag - qe * (pp + nn) / VT;
    };
    for (int s = 0; s < sweeps; s++) {
        // строки: поправки δ вдоль x
#pragma omp parallel for schedule(static)
        for (int j = 0; j < NY; j++) {
            double A[NX], B[NX], Cc[NX], D[NX];
            for (int i = 0; i < NX; i++) {
                const int k = id(i, j);
                if (contact[k] >= 0) { A[i] = Cc[i] = 0; B[i] = 1; D[i] = 0; continue; }
                double F, J; FJ(i, j, F, J);
                A[i] = i > 0 ? (i == NX - 1 ? 2 * ax : ax) : 0; Cc[i] = i < NX - 1 ? (i == 0 ? 2 * ax : ax) : 0; B[i] = J; D[i] = -F;
            }
            scThomas(NX, A, B, Cc, D);
            for (int i = 0; i < NX; i++) phi[id(i, j)] += clampv(D[i], -0.3, 0.3);
        }
        // столбцы: поправки вдоль y (у затвора — граничное условие окисла)
#pragma omp parallel for schedule(static)
        for (int i = 0; i < NX; i++) {
            double A[NY], B[NY], Cc[NY], D[NY];
            for (int j = 0; j < NY; j++) {
                const int k = id(i, j);
                if (contact[k] >= 0) { A[j] = Cc[j] = 0; B[j] = 1; D[j] = 0; continue; }
                double F, J; FJ(i, j, F, J);
                A[j] = j > 0 ? (j == NY - 1 ? 2 * ay : ay) : 0; Cc[j] = j < NY - 1 ? (j == 0 ? 2 * ay : ay) : 0; B[j] = J; D[j] = -F;
            }
            scThomas(NY, A, B, Cc, D);
            for (int j = 0; j < NY; j++) phi[id(i, j)] += clampv(D[j], -0.3, 0.3);
        }
    }
    for (int k = 0; k < NX * NY; k++) if (contact[k] < 0) { const double e = std::exp(clampv((phi[k] - phiOld[k]) / VT, -40.0, 40.0)); n[k] *= e; p[k] /= e; }
}
// непрерывность для электронов (s = −1) или дырок (s = +1): неявный шаг по времени, потоки Шарфеттера — Гуммеля.
// Поток из узла k в сосед m: c·(u_k·B(−ψ) − u_m·B(ψ)), ψ = ±(φ_m − φ_k)/Vt (электроны скатываются к большему φ,
// дырки — к меньшему). Рекомбинация: ШРХ и излучательная, R = (np − nᵢ²)·[1/(τ(n + p + 2nᵢ)) + B], линеаризована
static void scContinuity(int s, int sweeps, double dtc) {
    using namespace sc;
    std::vector<double>& u = s < 0 ? n : p; const std::vector<double>& other = s < 0 ? p : n;
    static std::vector<double> old; old = u;
    const double D = (s < 0 ? mun : mup) * VT, cx = D / (hx * hx), cy = D / (hy * hy), sg = s < 0 ? 1.0 : -1.0;
    // строка уравнения узла k: диагональ, коэффициенты при соседях по линии и правая часть с соседями вне линии
    auto row = [&](int k, int i, int j, bool alongX, double& diag, double& lo, double& hi, double& rhs) {
        diag = 1 / dtc; rhs = old[k] / dtc + gen[k]; lo = hi = 0;
        auto nb = [&](int m, double c, double* coef) {
            const double psi = sg * (phi[m] - phi[k]) / VT; diag += c * scBern(-psi);
            if (coef) *coef = -c * scBern(psi); else rhs += c * scBern(psi) * u[m];
        };
        if (i > 0) nb(k - 1, cx, alongX ? &lo : nullptr); if (i < NX - 1) nb(k + 1, cx, alongX ? &hi : nullptr);
        if (j > 0) nb(k - NX, cy, alongX ? nullptr : &lo); if (j < NY - 1) nb(k + NX, cy, alongX ? nullptr : &hi);
        const double o = other[k], rr = 1 / (tauN * (u[k] + o + 2 * ni)) + Brad;
        diag += o * rr; rhs += ni * ni * rr;
    };
    for (int it = 0; it < sweeps; it++) {
#pragma omp parallel for schedule(static)
        for (int j = 0; j < NY; j++) {
            double A[NX], B[NX], Cc[NX], R[NX];
            for (int i = 0; i < NX; i++) {
                const int k = id(i, j);
                if (contact[k] >= 0) { A[i] = Cc[i] = 0; B[i] = 1; R[i] = u[k]; continue; }
                row(k, i, j, true, B[i], A[i], Cc[i], R[i]);
            }
            scThomas(NX, A, B, Cc, R);
            for (int i = 0; i < NX; i++) u[id(i, j)] = std::max(1e-30, R[i]);
        }
#pragma omp parallel for schedule(static)
        for (int i = 0; i < NX; i++) {
            double A[NY], B[NY], Cc[NY], R[NY];
            for (int j = 0; j < NY; j++) {
                const int k = id(i, j);
                if (contact[k] >= 0) { A[j] = Cc[j] = 0; B[j] = 1; R[j] = u[k]; continue; }
                row(k, i, j, false, B[j], A[j], Cc[j], R[j]);
            }
            scThomas(NY, A, B, Cc, R);
            for (int j = 0; j < NY; j++) u[id(i, j)] = std::max(1e-30, R[j]);
        }
    }
}
// полный ток через вертикальное сечение между столбцами ix и ix+1 (слева направо, А). В установившемся режиме он одинаков
// в любом сечении; внутри прибора его считать надёжнее, чем у контакта, где ток основных носителей — малая разность
// огромных дрейфового и диффузионного потоков
static double scCut(int ix) {
    using namespace sc;
    const double Dn = mun * VT, Dp = mup * VT; double I = 0;
    for (int j = 0; j < NY; j++) {
        const int k = id(ix, j), m = k + 1; const double psi = (phi[m] - phi[k]) / VT;
        const double Gn = Dn / hx * (n[k] * scBern(-psi) - n[m] * scBern(psi)), Gp = Dp / hx * (p[k] * scBern(psi) - p[m] * scBern(-psi));
        I += Q * (Gp - Gn) * hy * (j == 0 || j == NY - 1 ? 0.5 : 1.0) * depth;
    }
    return I;
}
// токи через контакты — сумма потоков из узлов контакта в соседние узлы прибора (А); положительный — ток,
// втекающий в прибор через этот контакт
static void scCurrents() {
    using namespace sc;
    double I[4] = {0, 0, 0, 0};
    const double Dn = mun * VT, Dp = mup * VT;
    for (int j = 0; j < NY; j++) for (int i = 0; i < NX; i++) {
        const int k = id(i, j), c = contact[k]; if (c < 0) continue;
        auto flux = [&](int m, double h, double area) {
            if (contact[m] >= 0) return;
            const double psi = (phi[m] - phi[k]) / VT;
            const double Gn = Dn / h * (n[k] * scBern(-psi) - n[m] * scBern(psi));   // поток электронов k → m, см⁻²·с⁻¹
            const double Gp = Dp / h * (p[k] * scBern(psi) - p[m] * scBern(-psi));   // поток дырок k → m
            I[c] += Q * (Gp - Gn) * area;
        };
        if (i > 0) flux(k - 1, hx, hy * depth); if (i < NX - 1) flux(k + 1, hx, hy * depth);
        if (j > 0) flux(k - NX, hy, hx * depth); if (j < NY - 1) flux(k + NX, hy, hx * depth);
    }
    // ток прибора надёжнее по сечениям внутри: диод — через переход; МОП — через середину канала;
    // биполярный — через эмиттер и коллектор (база — остаток)
    if (scene <= 1) { I[0] = scCut(NX / 2); I[1] = -I[0]; }
    else if (scene == 2) { I[1] = -scCut(NX / 2); I[0] = -I[1] - I[2]; }
    else {   // сечения — сразу за эмиттерным переходом (в n+ эмиттере ток — разность огромных потоков) и в коллекторе
        const double ie = scCut((int)(0.61 * NX / 2)), icol = scCut((int)(1.5 * NX / 2)); I[0] = ie; I[2] = -icol; I[1] = -I[0] - I[2];
    }
    const double a = std::min(1.0, dt / 20e-12);
    for (int c = 0; c < nContacts; c++) Ic[c] += a * (I[c] - Ic[c]);
}
// генерация светом (солнечный элемент): поглощение по закону Бугера — вглубь от освещённой верхней грани
static void scLight() {
    using namespace sc;
    std::fill(gen.begin(), gen.end(), 0.0);
    if (!(scene == 1 && mode121 == 1 && light)) return;
    // солнце: ~2.7·10¹⁷ фотонов/(см²·с) с энергией выше ширины зоны кремния; поглощение α ≈ 10⁴ см⁻¹ (свет ~600 нм)
    const double flux = 2.7e17 * lightP * 300, alpha = 1e4;   // ×300 — концентратор: в микрометровом приборе иначе ток слишком мал
    for (int j = 0; j < NY; j++) for (int i = 0; i < NX; i++) gen[id(i, j)] = flux * alpha * std::exp(-alpha * j * hy);
}
static void scReset(int sc_) {
    using namespace sc;
    scene = clampv(sc_, 0, 3); t = 0; ivPts.clear(); dots.clear(); photons.clear(); photonAcc = photonsPerS = 0;
    for (double& i : Ic) i = 0;
    scBuild(); scApplyBias(); scLight();
    dt = 1e-12; timeScale = 40e-12;
}
// один шаг времени: Пуассон — электроны — дырки (метод Гуммеля), токи
static void scStep1() {
    using namespace sc;
    scPoisson(3);
    scContinuity(-1, 2, dt);
    scContinuity(+1, 2, dt);
    scCurrents();
    // излучение светодиода: число излучательных актов в секунду
    if (Brad > 0) {
        double R = 0; for (int k = 0; k < NX * NY; k++) R += Brad * std::max(0.0, n[k] * p[k] - ni * ni) * hx * hy * depth;
        photonsPerS = R;
    }
    t += dt; settle += dt;
}

// ---- точки-носители: плотность точек повторяет рассчитанную концентрацию, движутся они со скоростью потока Γ/n
static double scTarget(int k, int s) {   // сколько точек вида s (−1 электрон, +1 дырка) должно быть в клетке k
    using namespace sc;
    return std::min(3.0, (s < 0 ? n[k] : p[k]) * hx * hy * depth / dotW);
}
// скорость потока носителей вида s в клетке (i, j) — центральные разности потоков Шарфеттера — Гуммеля, см/с
static void scVel(int i, int j, int s, double& vx, double& vy) {
    using namespace sc;
    const std::vector<double>& u = s < 0 ? n : p; const double D = (s < 0 ? mun : mup) * VT, sg = s < 0 ? 1.0 : -1.0;
    auto flux = [&](int k, int m, double h) { const double psi = sg * (phi[m] - phi[k]) / VT; return D / h * (u[k] * scBern(-psi) - u[m] * scBern(psi)); };
    const int k = id(i, j); const double uk = std::max(u[k], 1e-30);
    const double fxp = i < NX - 1 ? flux(k, k + 1, hx) : 0, fxm = i > 0 ? flux(k - 1, k, hx) : 0;
    const double fyp = j < NY - 1 ? flux(k, k + NX, hy) : 0, fym = j > 0 ? flux(k - NX, k, hy) : 0;
    vx = 0.5 * (fxp + fxm) / uk; vy = 0.5 * (fyp + fym) / uk;
    const double vmax = 2e7; vx = clampv(vx, -vmax, vmax); vy = clampv(vy, -vmax, vmax);   // выше дрейфовой скорости насыщения носители не бегут
}
static void scDots(double simDt, double frameDt) {
    using namespace sc;
    std::mt19937_64& R = rng;
    // движение: поток + немного теплового дрожания; вышедшие за контакт исчезают
    for (auto& d : dots) {
        const int i = clampv((int)(d.x / Lx * NX), 0, NX - 1), j = clampv((int)(d.y / Ly * NY), 0, NY - 1);
        double vx, vy; scVel(i, j, d.s, vx, vy);
        const double D = (d.s < 0 ? mun : mup) * VT, jit = 0.3 * std::sqrt(2 * D * simDt);
        d.x += (float)(vx * simDt + jit * std::normal_distribution<double>(0, 1)(R));
        d.y += (float)(vy * simDt + jit * std::normal_distribution<double>(0, 1)(R));
        d.x = clampv(d.x, 0.0f, (float)Lx); d.y = clampv(d.y, 0.0f, (float)Ly);
    }
    // рождение и исчезновение: в каждой клетке число точек подтягивается к расчётному
    static std::vector<int> cnt[2]; static std::vector<std::vector<int>> who[2];
    for (int s = 0; s < 2; s++) { cnt[s].assign(NX * NY, 0); if ((int)who[s].size() != NX * NY) who[s].assign(NX * NY, {}); for (auto& w : who[s]) w.clear(); }
    for (int q = 0; q < (int)dots.size(); q++) {
        const Dot& d = dots[q]; const int k = id(clampv((int)(d.x / Lx * NX), 0, NX - 1), clampv((int)(d.y / Ly * NY), 0, NY - 1)), s = d.s < 0 ? 0 : 1;
        cnt[s][k]++; who[s][k].push_back(q);
    }
    std::vector<char> kill(dots.size(), 0); std::uniform_real_distribution<double> U(0, 1);
    for (int k = 0; k < NX * NY; k++) for (int s = 0; s < 2; s++) {
        const double tg = scTarget(k, s ? 1 : -1); const int c = cnt[s][k];
        const int want = (int)tg + (U(R) < tg - (int)tg ? 1 : 0);
        if (c > want && U(R) < 0.25) kill[who[s][k][(size_t)(U(R) * c) % c]] = 1;
        else if (c < want && U(R) < 0.25) {
            const int i = k % NX, j = k / NX;
            dots.push_back({(float)((i + U(R)) * Lx / NX), (float)((j + U(R)) * Ly / NY), (signed char)(s ? 1 : -1)}); kill.push_back(0);
        }
    }
    size_t w = 0; for (size_t q = 0; q < dots.size(); q++) if (!kill[q]) dots[w++] = dots[q]; dots.resize(w);
    // светодиод: фотоны рождаются там, где идёт излучательная рекомбинация
    if (Brad > 0) {
        // за пикосекунды модели настоящих фотонов вылетает мало (≈10¹⁰ в секунду), поэтому рисуется их поток:
        // частота нарисованных фотонов пропорциональна излучению, но не больше 60 в секунду экрана
        photonAcc += frameDt * std::min(60.0, 30.0 * photonsPerS / 1e10);
        while (photonAcc > 1 && photons.size() < 300) {
            photonAcc -= 1;
            double best = 0, pick = U(R) * photonsPerS; int kk = 0;
            for (int k = 0; k < NX * NY; k++) { best += Brad * std::max(0.0, n[k] * p[k] - ni * ni) * hx * hy * depth; if (best >= pick) { kk = k; break; } }
            const double a = 2 * PI * U(R);
            photons.push_back({(float)((kk % NX + U(R)) * Lx / NX), (float)((kk / NX + U(R)) * Ly / NY), (float)std::cos(a), (float)std::sin(a), 0});
        }
        if (photonAcc > 3) photonAcc = 3;
    }
    // солнечный элемент: входящие сверху фотоны (для рисунка) — там, где свет рождает пары
    if (scene == 1 && mode121 == 1 && light && photons.size() < 200 && U(R) < 0.6 * lightP)
        photons.push_back({(float)(U(R) * Lx), 0.0f, 0.0f, 1.0f, 0});
}
static void scStep(double frameDt) {
    using namespace sc;
    const double want = timeScale * frameDt; int nst = (int)std::ceil(want / dt - 1e-9); nst = clampv(nst, 1, 20);
    const auto c0 = std::chrono::high_resolution_clock::now(); int done = 0;
    for (int s = 0; s < nst; s++) {
        scStep1(); done++;
        if (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - c0).count() > 0.012) break;
    }
    scDots(done * dt, frameDt);
    for (auto& ph : photons) ph.life += (float)frameDt;
    photons.erase(std::remove_if(photons.begin(), photons.end(), [](const Photon& p) { return p.life > 1.2f; }), photons.end());
    // точка вольт-амперной характеристики — когда ток установился
    const double Iv = scene == 2 ? Ic[1] : (scene == 3 ? Ic[2] : Ic[0]), Vv = scene == 3 ? V1 : (scene == 2 ? V2 : V1);
    if (settle > 400e-12) {
        bool have = false; for (auto& pt : ivPts) if (std::fabs(pt.first - Vv) < 1e-6) { pt.second = Iv; have = true; }
        if (!have) { ivPts.push_back({Vv, Iv}); std::sort(ivPts.begin(), ivPts.end()); }
    }
}
// ---- рисунок: сечение прибора, области легирования, носители, ионы примеси, контакты
static PR scArea;
static void scToPx(double x, double y, float& px, float& py) { px = scArea.x + (float)(x / sc::Lx) * scArea.w; py = scArea.y + (float)(y / sc::Ly) * scArea.h; }
static std::string scAmp(double I) {
    const double a = std::fabs(I);
    if (a < 1e-12) return fmt("%.2f фА", I * 1e15);
    if (a < 1e-9) return fmt("%.2f пА", I * 1e12);
    if (a < 1e-6) return fmt("%.2f нА", I * 1e9);
    if (a < 1e-3) return fmt("%.2f мкА", I * 1e6);
    return fmt("%.2f мА", I * 1e3);
}
static void scDraw() {
    using namespace sc;
    glEnable(GL_SCISSOR_TEST); glScissor((int)sceneX, (int)(winH - sceneY - sceneH), (int)sceneW, (int)sceneH);
    rectFill(sceneX, sceneY, sceneW, sceneH, C_SCENE);
    const float top = uiPx(80), bottom = uiPx(60), sideM = uiPx(90);
    float w = sceneW - 2 * sideM, h = w * (float)(Ly / Lx);
    if (h > sceneH - top - bottom) { h = sceneH - top - bottom; w = h * (float)(Lx / Ly); }
    scArea = {std::floor(sceneX + (sceneW - w) / 2), std::floor(sceneY + top), std::floor(w), std::floor(h)};
    const float cw = scArea.w / NX, ch = scArea.h / NY;
    {   // легирование: p — тёплый оттенок, n — холодный; n+ и p+ ярче
        MonoAtoms colored;
        glBegin(GL_QUADS);
        for (int j = 0; j < NY; j++) for (int i = 0; i < NX; i++) {
            const double c = C[id(i, j)], a = clampv((std::log10(std::fabs(c) + 1) - 14.5) / 5.0, 0.08, 0.5);
            if (c > 0) glColor4f(0.10f, 0.22f, 0.55f, (float)a); else glColor4f(0.55f, 0.16f, 0.10f, (float)a);
            const float x0 = scArea.x + i * cw, y0 = scArea.y + j * ch;
            glVertex2f(x0, y0); glVertex2f(x0 + cw + 0.5f, y0); glVertex2f(x0 + cw + 0.5f, y0 + ch + 0.5f); glVertex2f(x0, y0 + ch + 0.5f);
        }
        glEnd();
        // ионы примеси (неподвижные): «+» донор, «−» акцептор — каждая шестая клетка; в обеднённом слое остаются только они
        glBegin(GL_LINES);
        for (int j = 1; j < NY; j += 3) for (int i = (j / 3) & 1 ? 1 : 2; i < NX; i += 3) {
            const double c = C[id(i, j)]; const float x = scArea.x + (i + 0.5f) * cw, y = scArea.y + (j + 0.5f) * ch, r = std::max(1.5f, uiPx(2.2f));
            if (c > 0) { glColor4f(0.45f, 0.62f, 1.0f, 0.55f); glVertex2f(x - r, y); glVertex2f(x + r, y); glVertex2f(x, y - r); glVertex2f(x, y + r); }
            else { glColor4f(1.0f, 0.55f, 0.40f, 0.55f); glVertex2f(x - r, y); glVertex2f(x + r, y); }
        }
        glEnd();
        // носители
        glPointSize(std::max(2.0f, uiPx(2.6f))); glEnable(GL_POINT_SMOOTH); glBegin(GL_POINTS);
        for (auto& d : dots) {
            float px, py; scToPx(d.x, d.y, px, py);
            if (d.s < 0) glColor4f(0.45f, 0.80f, 1.0f, 0.95f); else glColor4f(1.0f, 0.55f, 0.25f, 0.95f);
            glVertex2f(px, py);
        }
        glEnd(); glDisable(GL_POINT_SMOOTH); glPointSize(1);
        // фотоны: светодиод — цвет по ширине запрещённой зоны, солнечный элемент — жёлтые сверху
        glEnable(GL_LINE_SMOOTH); glLineWidth(1.6f);
        float pr = 1, pg = 0.9f, pb = 0.3f;
        if (Brad > 0) { if (ledColor == 0) { pr = 1.0f; pg = 0.25f; pb = 0.2f; } else if (ledColor == 1) { pr = 0.3f; pg = 1.0f; pb = 0.35f; } else { pr = 0.35f; pg = 0.5f; pb = 1.0f; } }
        for (auto& ph : photons) {
            const float a = std::max(0.0f, 1.0f - ph.life / 1.2f); glColor4f(pr, pg, pb, a);
            const double L0 = (double)ph.life * (Brad > 0 ? 0.9e-4 : 0.8e-4);
            float x0, y0; scToPx(ph.x + ph.dx * L0, ph.y + ph.dy * L0, x0, y0);
            glBegin(GL_LINE_STRIP);
            for (int q = 0; q <= 12; q++) { const float s = q / 12.0f, wv = 0.18f * std::sin(q * 1.6f) * uiPx(6); glVertex2f(x0 - ph.dx * s * uiPx(16) - ph.dy * wv, y0 - ph.dy * s * uiPx(16) + ph.dx * wv); }
            glEnd();
        }
        glLineWidth(1); glDisable(GL_LINE_SMOOTH);
    }
    rectLine(scArea.x, scArea.y, scArea.w, scArea.h, C_LINE_H);
    // контакты: металлические полосы по краям; затвор — над окислом
    auto metal = [&](float x, float y, float ww, float hh) { rectFill(x, y, ww, hh, hexc(0xB8B8B8)); rectLine(x, y, ww, hh, hexc(0xE0E0E0)); };
    const float mt = uiPx(7);
    auto label = [&](float x, float y, const std::string& s) { drawText(fontXS, x - textW(fontXS, s) / 2, y, s, C_TEXT); };
    if (scene <= 1 || scene == 3) {
        metal(scArea.x - mt, scArea.y, mt, scArea.h); metal(scArea.x + scArea.w, scArea.y, mt, scArea.h);
        const int cr = scene == 3 ? 2 : 1;
        label(scArea.x - mt / 2, scArea.y + scArea.h + uiPx(6), fmt("%s  %+.2f В", T(CNAME[0]), Vc[0]));
        label(scArea.x + scArea.w + mt / 2, scArea.y + scArea.h + uiPx(6), fmt("%s  %+.2f В", T(CNAME[cr]), Vc[cr]));
    }
    if (scene == 3) {   // вывод базы сверху
        float x0, y0, x1, y1; scToPx(0.68e-4, 0, x0, y0); scToPx(0.82e-4, 0, x1, y1);
        metal(x0, y0 - mt, x1 - x0, mt); label((x0 + x1) / 2, y0 - mt - fontXS.h - uiPx(4), fmt("%s  %+.2f В", T(CNAME[1]), Vc[1]));
    }
    if (scene == 2) {
        float x0, y0, x1, y1;
        scToPx(0, 0, x0, y0); scToPx(0.35e-4, 0, x1, y1); metal(x0, y0 - mt, x1 - x0, mt); label((x0 + x1) / 2, y0 - mt - fontXS.h - uiPx(4), fmt("%s 0 В", T(CNAME[0])));
        scToPx(1.65e-4, 0, x0, y0); scToPx(2e-4, 0, x1, y1); metal(x0, y0 - mt, x1 - x0, mt); label((x0 + x1) / 2, y0 - mt - fontXS.h - uiPx(4), fmt("%s %+.2f В", T(CNAME[1]), Vc[1]));
        scToPx(0.55e-4, 0, x0, y0); scToPx(1.45e-4, 0, x1, y1);
        rectFill(x0, y0 - uiPx(5), x1 - x0, uiPx(5), hexc(0x5A5A5A));   // окисел SiO2
        metal(x0, y0 - uiPx(5) - mt * 1.4f, x1 - x0, mt * 1.4f); label((x0 + x1) / 2, y0 - uiPx(5) - mt * 1.4f - fontXS.h - uiPx(4), fmt("затвор G %+.2f В", Vg));
        metal(scArea.x, scArea.y + scArea.h, scArea.w, mt); label(scArea.x + scArea.w / 2, scArea.y + scArea.h + mt + uiPx(4), fmt("%s 0 В", T(CNAME[2])));
    }
    // масштаб
    {
        const float lp = (float)(0.5e-4 / Lx) * scArea.w, x = scArea.x, y = scArea.y + scArea.h + uiPx(40);
        rectFill(x, y, lp, 1, withA(C_TEXT, 0.85f)); rectFill(x, y - uiPx(4), 1, uiPx(5), withA(C_TEXT, 0.85f)); rectFill(x + lp - 1, y - uiPx(4), 1, uiPx(5), withA(C_TEXT, 0.85f));
        drawText(fontXS, x + lp + uiPx(6), y - fontXS.h / 2, "0.5 мкм", withA(C_TEXT, 0.85f));
    }
    glDisable(GL_SCISSOR_TEST);
}
static std::string scTimeStr() { return fmt("%.0f пс", sc::t * 1e12); }
// значения по умолчанию при входе в сцену
static void scDefaults(int s) {
    using namespace sc;
    switch (s) {
    case 0: V1 = 0.0; break;
    case 1: V1 = 1.9; mode121 = 0; ledColor = 0; light = false; lightP = 1; break;
    case 2: V1 = 1.5; V2 = 1.0; break;
    case 3: V1 = 0.70; V2 = 2.0; break;
    }
}
// ток прибора для показаний: диод — через анод, МОП — через сток, биполярный — через коллектор
static inline double scI() { using namespace sc; return scene == 2 ? Ic[1] : (scene == 3 ? Ic[2] : Ic[0]); }
static void scPanel(float x, float y, float w, float h) {
    using namespace sc;
    rectFill(x, y, w, h, C_PANEL); lineV(x, y, y + h, C_LINE);
    const float pad = uiPx(10), cx = x + pad, cw = w - 2 * pad;
    float yy = scrollBegin(6, cx, y + uiPx(6), cw, h - uiPx(10));
    const float bh = uiPx(26), sh = uiPx(30);
    drawText(fontL, cx, yy, "Полупроводник", C_TEXT_HI); yy += fontL.h + uiPx(6);
    auto row = [&](const char* k, const std::string& v) { drawText(fontU, cx, yy, k, C_DIM); drawTextR(fontM, cx + cw, yy - uiPx(1), v, C_TEXT_HI); yy += fontU.h + uiPx(5); };
    row("время", scTimeStr());
    bool changed = false;
    if (scene <= 1) {
        row("напряжение", fmt("%+.2f В", V1));
        row("ток", scAmp(Ic[0]));
        const double Vbi = VT * std::log(1e16 * 1e16 / (ni * ni));
        row("контактная разность", fmt("%.2f В", Vbi));
        const double Wd = std::sqrt(std::max(0.0, 2 * eps * (Vbi - V1) / Q * (2 / 1e16))) * 1e4;
        row("обеднённый слой (теория)", V1 < Vbi ? fmt("%.2f мкм", Wd) : std::string("—"));
        if (scene == 1 && mode121 == 0) {
            row("излучает", fmt("%.2g фотонов/с", photonsPerS));
            row("свет", fmt("%.0f нм · %.2f эВ", 1239.84 / Eg, Eg));
        }
        if (scene == 1 && mode121 == 1) row("мощность P = V·I", fmt("%.3g мкВт", -V1 * Ic[0] * 1e6));
    } else if (scene == 2) {
        row("затвор Vg", fmt("%+.2f В", V1)); row("сток Vd", fmt("%+.2f В", V2));
        row("ток стока", scAmp(Ic[1])); row("порог (теория)", "≈ 0.32 В");
    } else {
        row("база Vбэ", fmt("%+.2f В", V1)); row("коллектор Vкэ", fmt("%+.2f В", V2));
        row("ток базы", scAmp(Ic[1])); row("ток коллектора", scAmp(Ic[2]));
        row("усиление β = Iк/Iб", std::fabs(Ic[1]) > 1e-15 ? fmt("%.0f", Ic[2] / Ic[1]) : std::string("—"));
    }
    yy += uiPx(4);
    uiSection(cx, yy, cw, "управление");
    if (uiButton(1880, cx, yy, cw / 2 - uiPx(3), bh, "заново (R)", false, false, "Начать сцену заново")) scReset(scene);
    if (uiButton(1881, cx + cw / 2 + uiPx(3), yy, cw / 2 - uiPx(3), bh, P.paused ? "пуск (пробел)" : "пауза (пробел)", P.paused, false, "Остановить или запустить время")) P.paused = !P.paused;
    yy += bh + uiPx(6);
    {
        double lg = std::log10(timeScale * 1e12);
        if (uiSlider(1882, cx, yy, cw, sh - uiPx(4), "скорость времени", &lg, 0, 3, false, fmt("%.3g пс за 1 с", timeScale * 1e12), "Сколько пикосекунд проходит за секунду на экране")) timeScale = std::pow(10.0, lg) * 1e-12;
        yy += sh;
    }
    if (scene <= 1) {
        const bool led = scene == 1 && mode121 == 0;
        double v = V1; const double lo = led ? -1 : (scene == 1 ? -0.2 : -2), hi = led ? Eg + 0.25 : (scene == 1 ? 0.7 : 0.85);
        if (uiSlider(1883, cx, yy, cw, sh - uiPx(4), "напряжение на аноде", &v, lo, hi, false, fmt("%+.2f В", v), "Плюс на p-области — прямое включение: барьер перехода снижается")) { V1 = v; changed = true; }
        yy += sh;
    }
    if (scene == 1) {
        static const char* MODES[2] = {"светодиод", "солнечный элемент"};
        if (uiCycle(1884, cx, yy, cw, bh, "прибор", MODES[mode121], false, "Светодиод — прямозонный полупроводник, излучает при прямом токе;\nсолнечный элемент — кремний, свет рождает пары электрон–дырка")) {
            mode121 ^= 1; V1 = mode121 ? 0.4 : 1.9; light = mode121 == 1; scReset(1);
        }
        yy += bh + uiPx(6);
        if (mode121 == 0) {
            static const char* COLS[3] = {"красный (AlGaInP)", "зелёный (InGaN)", "синий (InGaN)"};
            if (uiCycle(1885, cx, yy, cw, bh, "цвет", COLS[ledColor], false, "Цвет свечения задаёт ширина запрещённой зоны: E = hν")) { ledColor = (ledColor + 1) % 3; V1 = ledColor == 0 ? 1.9 : (ledColor == 1 ? 2.3 : 2.7); scReset(1); }
            yy += bh + uiPx(6);
        } else {
            if (uiCheck(1886, cx, yy, cw, uiPx(22), "свет", &light, "Солнечный свет (с концентратором ×300) падает сверху")) scLight();
            yy += uiPx(26);
        }
    }
    if (scene == 2) {
        double a = V1, b = V2;
        if (uiSlider(1887, cx, yy, cw, sh - uiPx(4), "затвор Vg", &a, 0, 3, false, fmt("%+.2f В", a), "Выше порога под затвором появляется слой электронов — канал")) { V1 = a; changed = true; }
        yy += sh;
        if (uiSlider(1888, cx, yy, cw, sh - uiPx(4), "сток Vd", &b, 0, 3, false, fmt("%+.2f В", b), "Напряжение сток–исток гонит электроны по каналу")) { V2 = b; changed = true; }
        yy += sh;
    }
    if (scene == 3) {
        double a = V1, b = V2;
        if (uiSlider(1889, cx, yy, cw, sh - uiPx(4), "база Vбэ", &a, 0, 0.85, false, fmt("%+.2f В", a), "Прямое напряжение эмиттерного перехода: электроны впрыскиваются в тонкую базу")) { V1 = a; changed = true; }
        yy += sh;
        if (uiSlider(1890, cx, yy, cw, sh - uiPx(4), "коллектор Vкэ", &b, 0, 5, false, fmt("%+.2f В", b), "Коллектор собирает электроны, пролетевшие базу")) { V2 = b; changed = true; }
        yy += sh;
    }
    if (changed) scApplyBias();
    if (uiButton(1891, cx, yy, cw, bh, "очистить график", false, false, "Стереть точки вольт-амперной характеристики")) ivPts.clear();
    yy += bh + uiPx(8);
    // вольт-амперная характеристика: точки ставятся сами, когда ток после смены напряжения установился
    {
        const bool logI = scene <= 1;
        uiSection(cx, yy, cw, scene == 2 ? "Id(Vd) при текущем Vg" : (scene == 3 ? "Iк(Vбэ)" : "ток |I|, логарифм, от напряжения"));
        PR in{cx, yy, cw, uiPx(120)}; boxPanel(in.x, in.y, in.w, in.h, C_PANEL2, C_LINE);
        double vlo = 1e9, vhi = -1e9, ilo = 1e30, ihi = -1e30;
        for (auto& pt : ivPts) { vlo = std::min(vlo, pt.first); vhi = std::max(vhi, pt.first); const double v = logI ? std::log10(std::max(1e-15, std::fabs(pt.second))) : pt.second; ilo = std::min(ilo, v); ihi = std::max(ihi, v); }
        if (ivPts.size() >= 1) {
            if (vhi - vlo < 0.1) { vlo -= 0.1; vhi += 0.1; } if (ihi - ilo < 1e-12) { ihi += logI ? 1 : 1e-6; ilo -= logI ? 1 : 0; }
            if (!logI) ilo = std::min(0.0, ilo);
            col(C_TEXT_HI); glPointSize(std::max(3.0f, uiPx(3.5f))); glBegin(GL_POINTS);
            for (auto& pt : ivPts) { const double v = logI ? std::log10(std::max(1e-15, std::fabs(pt.second))) : pt.second; glVertex2f(mapX(in, pt.first, vlo, vhi), mapY(in, v, ilo, ihi)); }
            glEnd(); glPointSize(1);
            glBegin(GL_LINE_STRIP); for (auto& pt : ivPts) { const double v = logI ? std::log10(std::max(1e-15, std::fabs(pt.second))) : pt.second; glVertex2f(mapX(in, pt.first, vlo, vhi), mapY(in, v, ilo, ihi)); } glEnd();
            drawText(fontXS, in.x + uiPx(4), in.y + in.h + uiPx(2), logI ? fmt("%.1f…%.1f В · ток от %s до %s", vlo, vhi, scAmp(std::pow(10.0, ilo)).c_str(), scAmp(std::pow(10.0, ihi)).c_str())
                                                                        : fmt("%.1f…%.1f В · до %s", vlo, vhi, scAmp(ihi).c_str()), C_DIM);
        } else drawText(fontXS, in.x + uiPx(6), in.y + uiPx(6), "меняйте напряжение — точки появятся сами", C_DIM);
        yy += in.h + fontXS.h + uiPx(10);
    }
    // зонная диаграмма вдоль прибора: дно зоны проводимости, потолок валентной и квазиуровни Ферми
    {
        uiSection(cx, yy, cw, scene == 2 ? "зоны вдоль канала (у поверхности)" : "зонная диаграмма вдоль прибора");
        PR in{cx, yy, cw, uiPx(120)}; boxPanel(in.x, in.y, in.w, in.h, C_PANEL2, C_LINE);
        const int j = scene == 2 ? 1 : NY / 2;
        std::vector<float> ec(NX), ev(NX), fn(NX), fp(NX); double lo = 1e9, hi = -1e9;
        for (int i = 0; i < NX; i++) {
            const int k = id(i, j); const double ei = -phi[k];
            ec[i] = (float)(ei + Eg / 2); ev[i] = (float)(ei - Eg / 2);
            fn[i] = (float)(ei + VT * std::log(std::max(n[k], 1e-30) / ni)); fp[i] = (float)(ei - VT * std::log(std::max(p[k], 1e-30) / ni));
            lo = std::min({lo, (double)ev[i], (double)fp[i]}); hi = std::max({hi, (double)ec[i], (double)fn[i]});
        }
        lo -= 0.1; hi += 0.1;
        drawSeries(in, ec, lo, hi, C_TEXT_HI, 1.5f, LS_SOLID, -1); drawSeries(in, ev, lo, hi, C_TEXT_HI, 1.5f, LS_SOLID, -1);
        drawSeries(in, fn, lo, hi, withA(C_TEXT, 0.9f), 1.2f, LS_DASH, -1); drawSeries(in, fp, lo, hi, withA(C_DIM, 0.9f), 1.2f, LS_DOT, -1);
        yy += in.h + uiPx(4);
        yy += drawWrapped(fontXS, cx, yy, cw, "сплошные — края зон (энергия электрона вверх), пунктир — уровень Ферми электронов, точки — дырок; в равновесии они совпадают", C_DIM) + uiPx(8);
    }
    static const char* NOTES[4] = {
        "Электроны из n-области и дырки из p-области диффундируют навстречу и рекомбинируют — у перехода остаются только неподвижные ионы примеси: "
        "обеднённый слой с электрическим полем. Прямое напряжение снижает барьер, и ток растёт как e^(V/0.026 В) (формула Шокли); обратное — расширяет слой, ток почти нулевой.",
        "В прямозонном полупроводнике электрон, встретив дырку, отдаёт энергию ширины запрещённой зоны одним фотоном: красный светодиод — 1.9 эВ (650 нм), синий — 2.7 эВ (460 нм). "
        "В солнечном элементе наоборот: фотон рождает пару электрон–дырка, поле перехода разводит их — течёт ток, а элемент отдаёт мощность.",
        "Положительный затвор отталкивает дырки подложки и притягивает электроны: при Vg выше порога у поверхности появляется тонкий слой электронов — канал, "
        "и между истоком и стоком течёт ток. При большом Vd канал у стока сужается («перекрытие») и ток насыщается — так работает ключ в любом процессоре.",
        "Эмиттер впрыскивает электроны в тонкую базу p-типа; почти все они пролетают её раньше, чем встретят дырку, и их подхватывает поле коллекторного перехода. "
        "Лишь малая часть рекомбинирует в базе — это и есть ток базы. Поэтому маленький ток базы управляет большим током коллектора."};
    yy += drawWrapped(fontXS, cx, yy, cw, NOTES[clampv(scene, 0, 3)], C_DIM) + uiPx(12);
    scrollEnd(6, yy);
}
static std::string scReport() {
    using namespace sc;
    switch (scene) {
    case 0: case 1: return fmt("t = %s · V = %+.2f В · ток %s%s", scTimeStr().c_str(), V1, scAmp(Ic[0]).c_str(), Brad > 0 ? fmt(" · %.2g фотонов/с", photonsPerS).c_str() : "");
    case 2: return fmt("t = %s · Vg = %.2f В, Vd = %.2f В · ток стока %s", scTimeStr().c_str(), V1, V2, scAmp(Ic[1]).c_str());
    default: return fmt("t = %s · Vбэ = %.2f В · Iб = %s, Iк = %s", scTimeStr().c_str(), V1, scAmp(Ic[1]).c_str(), scAmp(Ic[2]).c_str());
    }
}
static const char* SC_TITLES[4] = {
    "Диод: p–n переход — в прямом направлении ток растёт экспоненциально, в обратном почти не идёт",
    "Светодиод: электроны и дырки встречаются в переходе и излучают свет цвета ширины запрещённой зоны. Повтор режима — солнечный элемент",
    "Транзистор (n-МОП): напряжение на затворе собирает под окислом слой электронов — канал между истоком и стоком",
    "Биполярный транзистор n–p–n: малый ток базы управляет в десятки раз большим током коллектора"};


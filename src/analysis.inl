// ===================================== ANALYSIS ========================================
struct Series {
    std::vector<float> v;
    void push(float x) { v.push_back(x); if ((int)v.size() > cfg::HIST) v.erase(v.begin()); }
    void clear() { v.clear(); }
};
// ---- Агрегатное состояние каждого атома (для раскраски «фаза»; действительно, если atomPhase.size() == S.n):
//   PH_GAS 0 — газ, PH_LIQ 1 — жидкость, PH_SOLID 2 — твёрдое, PH_WALL 3 — стенка / закреплённый атом
enum { PH_GAS, PH_LIQ, PH_SOLID, PH_WALL };
static const char* PH_NAMES[] = {"газ", "жидкость", "твёрдое", "стенка"};
static std::vector<unsigned char> atomPhase;
static double phaseFrac[3] = {0, 0, 0};          // доли газ / жидкость / твёрдое (подвижные атомы без H)
static std::vector<std::string> phaseLog;        // журнал фазовых переходов (новые — в конце)
// типы локальной структуры (цвет «по порядку»)
enum { ST_OTHER, ST_FCC, ST_HCP, ST_BCC, ST_SC, ST_HEX2D, ST_SQ2D, ST_ICE, ST_N };
static const char* ST_NAMES[] = {"аморфн.", "ГЦК", "ГПУ", "ОЦК", "ПК/NaCl", "гекс.", "квадр.", "лёд"};
namespace A {
static const int GR_BINS = 120; static const double GR_RMAX = 5.0;
static std::vector<double> gr(GR_BINS, 0.0); static bool grInit = false;
static const int VH_BINS = 40; static std::vector<double> vh(VH_BINS, 0.0); static int vhType = E_AR; static double vhMax = 1;
static std::vector<float> ordMag, ordHue; static std::vector<unsigned char> coord, stype;
static double psiMean = 0, fCryst = 0, fGas = 0, meanCoord = 0; static int stCount[ST_N] = {0};
static Series sT, sP, sEk, sEp, sEt, sTime;
static std::vector<float> msdT, msdV, msdBig; static std::vector<double> x0, y0, z0; static double t0 = 0; static int msdN = -1;
static double D = 0, Dbig = 0;
static std::map<std::string, int> mol, mol0;
static std::vector<std::string> species; static std::vector<Series> conc; static Series concT;
static double progress = 0; static std::string progressLabel;
static int ionsTotal = 0, ionsFree = 0;
static std::vector<std::pair<float, float>> arr; static double arrTsum = 0, arrTime = 0; static long long arrEv = 0;
static double arrEa = 0; static bool arrFit = false;
static const int TP_BINS = 24; static std::vector<double> tprof(TP_BINS, 0.0); static int tprofAxis = 0;
static std::vector<double> eHist; static double Cv = 0;
static std::string phase = "—";
static double lastT = 0;
}

static void resetPhaseTracking();
static void resetAnalysis() {
    resetPhaseTracking();
    A::grInit = false; std::fill(A::gr.begin(), A::gr.end(), 0.0); std::fill(A::vh.begin(), A::vh.end(), 0.0);
    std::fill(A::tprof.begin(), A::tprof.end(), 0.0);
    A::sT.clear(); A::sP.clear(); A::sEk.clear(); A::sEp.clear(); A::sEt.clear(); A::sTime.clear();
    A::msdN = -1; A::msdT.clear(); A::msdV.clear(); A::msdBig.clear(); A::D = A::Dbig = 0;
    A::mol.clear(); A::mol0.clear(); A::species.clear(); A::conc.clear(); A::concT.clear();
    A::arr.clear(); A::arrTsum = 0; A::arrTime = 0; A::arrEv = CH.assoc + CH.exch; A::arrFit = false;
    A::eHist.clear(); A::Cv = 0; A::phase = "—"; A::lastT = S.t;
    A::ordMag.clear(); A::ordHue.clear(); A::coord.clear(); A::stype.clear();
    rxStats.clear(); rxRecent.clear();
}
static void resetMSD() {
    A::x0 = S.ux; A::y0 = S.uy; A::z0 = S.uz;
    A::t0 = S.t; A::msdN = S.n; A::msdT.clear(); A::msdV.clear(); A::msdBig.clear();
}
// g(r): гистограмма расстояний, нормированная на идеальный газ той же плотности:
//   2D: g = h/(N_ref·ρ·2πr·dr),  3D: g = h/(N_ref·ρ·4πr²·dr)
static void computeGr() {
    int n = S.n; if (n < 2) return;
    std::vector<int> mob; for (int i = 0; i < n; i++) if (!EL[S.ty[i]].fixed && S.ty[i] != E_BIG) mob.push_back(i);
    int nm = (int)mob.size(); if (nm < 2) return;
    int stride = std::max(1, nm / 500);
    const double dr = A::GR_RMAX / A::GR_BINS, rmax2 = A::GR_RMAX * A::GR_RMAX;
    std::vector<double> h(A::GR_BINS, 0.0); int used = 0;
#pragma omp parallel
    {
        std::vector<double> hl(A::GR_BINS, 0.0); int ul = 0;
#pragma omp for nowait
        for (int a = 0; a < nm; a += stride) {
            int i = mob[a]; ul++;
            for (int b = 0; b < nm; b++) {
                int j = mob[b]; if (j == i || (mayExclude(i, j) && excluded(i, j))) continue;   // только межмолекулярные пары
                double r2 = dist2(i, j);
                if (r2 < rmax2) hl[(int)(std::sqrt(r2) / dr)] += 1;
            }
        }
#pragma omp critical
        { for (int k = 0; k < A::GR_BINS; k++) h[k] += hl[k]; used += ul; }
    }
    double rho = nm / boxVolume();
    for (int k = 0; k < A::GR_BINS; k++) {
        double r = (k + 0.5) * dr, shell = DIM == 3 ? 4 * PI * r * r * dr : 2 * PI * r * dr;
        double g = h[k] / (used * rho * shell);
        A::gr[k] = A::grInit ? 0.8 * A::gr[k] + 0.2 * g : g;
    }
    A::grInit = true;
}
// Присоединённые функции Лежандра P_l^m(x), m = 0..l (рекуррентные формулы)
static void legendreAll(int l, double x, double* Pl) {
    double sx = std::sqrt(std::max(0.0, 1 - x * x));
    for (int m = 0; m <= l; m++) {
        double pmm = 1, f = 1;
        for (int i = 1; i <= m; i++) { pmm *= -f * sx; f += 2; }
        if (l == m) { Pl[m] = pmm; continue; }
        double pm1 = x * (2 * m + 1) * pmm;
        if (l == m + 1) { Pl[m] = pm1; continue; }
        double pll = 0;
        for (int ll = m + 2; ll <= l; ll++) { pll = ((2 * ll - 1) * x * pm1 - (ll + m - 1) * pmm) / (ll - m); pmm = pm1; pm1 = pll; }
        Pl[m] = pll;
    }
}
static double KLM[7][7];   // нормировка Y_lm: √((2l+1)/4π · (l−m)!/(l+m)!)
static void initKlm() {
    for (int l : {4, 6}) for (int m = 0; m <= l; m++) {
        double r = 1; for (int k = l - m + 1; k <= l + m; k++) r /= k;
        KLM[l][m] = std::sqrt((2 * l + 1) / (4 * PI) * r);
    }
}
typedef std::complex<double> cd;
// Локальный порядок.
//  2D: ψ6 = (1/n)Σ e^{6iθ} — гексатический параметр; фаза ψ6 = ориентация зерна.
//  3D: Стейнхардт q_lm = (1/n)Σ Y_lm(r̂); усреднение Лехнера–Деллаго q̄_l; «твёрдые связи» тен Вольде–Френкеля
//      (d6 = q6(i)·q6(j)* нормированное > 0.7); атом кристаллический при ≥ 7 твёрдых связях.
//      Тип решётки: ОЦК — 14 соседей, ПК — 6, иначе по q̄4: ГЦК (q̄4 ≈ 0.19) / ГПУ (q̄4 ≈ 0.10).
static void computeOrder() {
    int n = S.n;
    A::ordMag.assign(n, 0); A::ordHue.assign(n, 0); A::coord.assign(n, 0); A::stype.assign(n, 0);
    for (int& c : A::stCount) c = 0;
    if (n == 0) return;
    ensureNeighborList();
    // соседи: ближе 1.35σ_ij, не связаны химически; одноимённые ионы (Na⁺–Na⁺, Cl⁻–Cl⁻) не считаются —
    // тогда окружение иона в NaCl — октаэдр из противоионов (как в простой кубической решётке)
    auto isNb = [](int i, int j, double r2) {
        double qi = EL[S.ty[i]].fq, qj = EL[S.ty[j]].fq;
        if (qi * qj > 0) return false;
        double s = 1.35 * 0.5 * (EL[S.ty[i]].sig + EL[S.ty[j]].sig); return r2 < s * s && !bonded(i, j);
    };
    double sm = 0, sc = 0; int ngas = 0, nm = 0;
    const int gasCut = DIM == 3 ? 2 : 1;
    if (DIM == 2) {
        // ψ6 = (1/n)Σe^{6iθ} — гексагональный порядок, ψ4 — квадратный
#pragma omp parallel for reduction(+ : sm, sc, ngas, nm)
        for (int i = 0; i < n; i++) {
            if (EL[S.ty[i]].fixed) continue;
            double r6 = 0, i6 = 0, r4 = 0, i4 = 0; int cnt = 0;
            for (int p = nlStart[i]; p < nlStart[i + 1]; p++) {
                int j = nlIdx[p]; double dx, dy, dz; dvec(i, j, dx, dy, dz); double r2 = dx * dx + dy * dy;
                if (isNb(i, j, r2)) { double th = std::atan2(dy, dx); r6 += std::cos(6 * th); i6 += std::sin(6 * th); r4 += std::cos(4 * th); i4 += std::sin(4 * th); cnt++; }
            }
            if (cnt) { r6 /= cnt; i6 /= cnt; r4 /= cnt; i4 /= cnt; }
            double m6 = std::sqrt(r6 * r6 + i6 * i6), m4 = std::sqrt(r4 * r4 + i4 * i4);
            unsigned char st = ST_OTHER;
            if (cnt >= 5 && m6 > 0.7) st = ST_HEX2D; else if (cnt == 4 && m4 > 0.8) st = ST_SQ2D;
            bool sq = st == ST_SQ2D;
            A::ordMag[i] = (float)(sq ? m4 : m6); A::ordHue[i] = (float)(sq ? std::atan2(i4, r4) : std::atan2(i6, r6));
            A::coord[i] = (unsigned char)std::min(cnt, 20); A::stype[i] = st;
            if (S.ty[i] != E_H) { sm += m6; sc += cnt; nm++; if (cnt <= gasCut) ngas++; }   // водород не участвует в статистике фазы
        }
    } else {
        // Стейнхардт: q_lm = (1/n)Σ Y_lm(r̂); усреднение Лехнера–Деллаго; «твёрдые связи» тен Вольде–Френкеля
        std::vector<std::array<cd, 7>> q6(n); std::vector<std::array<cd, 5>> q4(n);
        std::vector<std::vector<int>> nbl(n);
#pragma omp parallel for schedule(dynamic, 64)
        for (int i = 0; i < n; i++) {
            q6[i].fill(0); q4[i].fill(0);
            if (EL[S.ty[i]].fixed) continue;
            double P6[7], P4[5];
            for (int p = nlStart[i]; p < nlStart[i + 1]; p++) {
                int j = nlIdx[p]; double dx, dy, dz; dvec(i, j, dx, dy, dz); double r2 = dx * dx + dy * dy + dz * dz;
                if (!isNb(i, j, r2) || EL[S.ty[j]].fixed) continue;
                nbl[i].push_back(j);
                double r = std::sqrt(r2), ct = dz / r, ph = std::atan2(dy, dx);
                legendreAll(6, ct, P6); legendreAll(4, ct, P4);
                for (int m = 0; m <= 6; m++) q6[i][m] += KLM[6][m] * P6[m] * cd(std::cos(m * ph), std::sin(m * ph));
                for (int m = 0; m <= 4; m++) q4[i][m] += KLM[4][m] * P4[m] * cd(std::cos(m * ph), std::sin(m * ph));
            }
            int k = (int)nbl[i].size();
            if (k) { for (auto& v : q6[i]) v /= (double)k; for (auto& v : q4[i]) v /= (double)k; }
        }
        // Σ_{m=−l..l} a_m b_m* = Re(a0 b0*) + 2Σ_{m>0} Re(a_m b_m*)  (так как q_{l,−m} = (−1)^m q_{lm}*)
        auto dot6 = [&](int i, int j) { double s = (q6[i][0] * std::conj(q6[j][0])).real(); for (int m = 1; m <= 6; m++) s += 2 * (q6[i][m] * std::conj(q6[j][m])).real(); return s; };
#pragma omp parallel for schedule(dynamic, 64) reduction(+ : sm, sc, ngas, nm)
        for (int i = 0; i < n; i++) {
            if (EL[S.ty[i]].fixed) continue;
            int k = (int)nbl[i].size();
            std::array<cd, 7> b6 = q6[i]; std::array<cd, 5> b4 = q4[i];
            int solid = 0; double ni = std::sqrt(std::max(1e-30, dot6(i, i)));
            for (int j : nbl[i]) {
                for (int m = 0; m <= 6; m++) b6[m] += q6[j][m];
                for (int m = 0; m <= 4; m++) b4[m] += q4[j][m];
                double nj = std::sqrt(std::max(1e-30, dot6(j, j)));
                if (dot6(i, j) / (ni * nj) > 0.7) solid++;
            }
            double s6 = std::norm(b6[0]), s4 = std::norm(b4[0]);
            for (int m = 1; m <= 6; m++) s6 += 2 * std::norm(b6[m]);
            for (int m = 1; m <= 4; m++) s4 += 2 * std::norm(b4[m]);
            double qb6 = std::sqrt(4 * PI / 13 * s6) / (k + 1), qb4 = std::sqrt(4 * PI / 9 * s4) / (k + 1);
            A::ordMag[i] = (float)qb6; A::ordHue[i] = (float)qb4; A::coord[i] = (unsigned char)std::min(k, 20);
            unsigned char st = ST_OTHER;
            if (k == 6 && solid >= 5) st = ST_SC;                       // октаэдр: ПК или ионный NaCl
            else if (solid >= 7) st = k >= 13 ? ST_BCC : (qb4 > 0.13 ? ST_FCC : ST_HCP);
            A::stype[i] = st;
            if (S.ty[i] != E_H) { sm += qb6; sc += k; nm++; if (k <= gasCut) ngas++; }
        }
        // Лёд: тетраэдрический порядок кислородов воды (Эррингтон–Дебенедетти)
        //   q = 1 − 3/8·Σ_{j<k}(cosψ_jk + 1/3)² по 4 ближайшим O; q ≈ 1 во льду, ≈ 0.5–0.6 в жидкой воде
        if (present[E_O]) {
#pragma omp parallel for schedule(dynamic, 64)
            for (int i = 0; i < n; i++) {
                if (S.ty[i] != E_O || S.nbc[i] != 2) continue;
                std::pair<double, int> best[4]; int nb = 0;
                for (int p = nlStart[i]; p < nlStart[i + 1]; p++) {
                    int j = nlIdx[p]; if (S.ty[j] != E_O || bonded(i, j)) continue;
                    double r2 = dist2(i, j); if (r2 > 1.3 * 1.3) continue;
                    if (nb < 4) best[nb++] = {r2, j};
                    else { int w = 0; for (int q = 1; q < 4; q++) if (best[q].first > best[w].first) w = q; if (r2 < best[w].first) best[w] = {r2, j}; }
                }
                if (nb < 4) continue;
                double v[4][3];
                for (int q = 0; q < 4; q++) { double dx, dy, dz; dvec(i, best[q].second, dx, dy, dz); double r = std::sqrt(dx * dx + dy * dy + dz * dz); v[q][0] = dx / r; v[q][1] = dy / r; v[q][2] = dz / r; }
                double s = 0;
                for (int a = 0; a < 4; a++) for (int b = a + 1; b < 4; b++) { double c = v[a][0] * v[b][0] + v[a][1] * v[b][1] + v[a][2] * v[b][2] + 1.0 / 3; s += c * c; }
                double qt = 1 - 3.0 / 8 * s;
                A::ordMag[i] = (float)qt;
                if (qt > 0.8) A::stype[i] = ST_ICE;
            }
            for (int i = 0; i < n; i++) if (S.ty[i] == E_H && S.nbc[i] == 1 && S.ty[S.nb[i][0]] == E_O) A::stype[i] = A::stype[S.nb[i][0]];
        }
    }
    int ncr = 0;
    for (int i = 0; i < n; i++) if (!EL[S.ty[i]].fixed) { A::stCount[A::stype[i]]++; if (A::stype[i] != ST_OTHER && S.ty[i] != E_H) ncr++; }
    if (nm) { A::psiMean = sm / nm; A::meanCoord = sc / nm; A::fCryst = (double)ncr / nm; A::fGas = (double)ngas / nm; }
}
// ---- Опорные данные модели (LJ с обрезкой 2.5σ и сдвигом — именно этот потенциал у аргона в программе)
//  3D: Tc = 1.0779, ρc = 0.319, pc = 0.0935; ρ_ж(T) — корреляция Vrabec et al. (Mol. Phys. 2006), ρ_г — по закону
//      прямолинейного диаметра; тройная точка T ≈ 0.62, ρ_ж ≈ 0.83, ρ_тв ≈ 0.95 (для полного LJ: 1.31 / 0.69).
//  2D: Tc ≈ 0.459 (Smit, Frenkel 1991); ширина бинодали 1.056·(Tc − T)^{1/8}, диаметр 0.33 + 0.775(Tc − T) —
//      подогнано по нашим прогонам «плёнка в ящике» (--phystest coex): T = 0.40 → 0.745 / 0.004, T = 0.44 → 0.67 / 0.017.
struct LJRef { double Tc, rc, pc, Tt, rlT, rsT, rgT; };
static LJRef ljRef() {
    if (DIM == 3) return {1.0779, 0.319, 0.0935, 0.62, 0.83, 0.95, 0.003};
    return {0.459, 0.33, 0.0, 0.41, 0.73, 0.80, 0.006};
}
// бинодаль жидкость–пар: плотности ρ_ж, ρ_г при T (false — выше Tc)
static bool ljBinodal(double T, double& rl, double& rg) {
    LJRef r = ljRef(); if (T >= r.Tc) { rl = rg = r.rc; return false; }
    double x = r.Tc - T;
    if (DIM == 3) {
        rl = r.rc + 0.5649 * std::cbrt(x) + 0.1314 * x + 0.0413 * x * std::sqrt(x);
        // пар: по прямолинейному диаметру у Tc, вдали от неё — идеальный газ при давлении насыщения
        // ln p_s = 3.1664 − 5.9809/T + 0.01498/T⁴ (Vrabec et al.)
        // (корреляция годится при T ≳ 0.5; ниже — Клапейрон–Клаузиус с теплотой сублимации ≈ 6.5ε)
        const double Te = std::max(T, 0.5);
        double ps = std::exp(3.1664 - 5.9809 / Te + 0.01498 / (Te * Te * Te * Te));
        if (T < 0.5) ps *= std::exp(-6.5 * (1 / std::max(T, 0.02) - 2.0));
        rg = std::max(2 * r.rc + 2 * 0.2067 * x - rl, ps / std::max(T, 0.02));
    } else {   // 2D: β = 1/8 (класс Изинга)
        double w = 1.056 * std::pow(x, 0.125), d = r.rc + 0.775 * x;
        rl = d + 0.5 * w; rg = d - 0.5 * w;
    }
    rg = std::max(rg, 0.0);
    return true;
}
// линии плавления: ρ_ж на границе кристаллизации и ρ_тв на границе плавления (T ≥ Tt); ниже Tt — плотность твёрдого
// на линии сублимации (приближённо, наклоны — как у полного LJ по Hansen–Verlet / Agrawal–Kofke)
static double ljFreeze(double T) { LJRef r = ljRef(); return r.rlT + (DIM == 3 ? 0.20 : 0.35) * (T - r.Tt); }
static double ljMelt(double T) { LJRef r = ljRef(); return T >= r.Tt ? r.rsT + (DIM == 3 ? 0.14 : 0.30) * (T - r.Tt) : r.rsT + (DIM == 3 ? 0.19 : 0.29) * (r.Tt - T); }
// чистое LJ-вещество (один подвижный тип без зарядов, связей и металла) — его тип, иначе −1
static int pureLJType() {
    int t = -1;
    for (int i = 0; i < S.n; i++) {
        if (frozenAt(i)) continue;
        if (t < 0) t = S.ty[i]; else if (S.ty[i] != t) return -1;
        if (S.nbc[i] || S.q[i] != 0) return -1;
    }
    if (t < 0 || isMetalT(t) || EL[t].rcf < 2.0) return -1;
    return t;
}
// приведённое состояние (T*, ρ*) в собственных σ, ε вещества (для чистого) или в единицах аргона
static void reducedState(double& Tr, double& rr) {
    int t = pureLJType(); double V = boxVolume(), rho = EN.nmob / std::max(1e-12, V);
    if (t >= 0) { double e = EL[t].eps * P.epsScale, s = EL[t].sig; Tr = EN.T / e; rr = rho * (DIM == 3 ? s * s * s : s * s); }
    else { Tr = EN.T; rr = rho; }
}
// область фазовой диаграммы модели для (T*, ρ*)
static const char* ljRegion(double T, double rho) {
    LJRef r = ljRef();
    if (T >= r.Tt) {
        if (rho >= ljMelt(T)) return "твёрдое";
        if (rho >= ljFreeze(T)) return "твёрдое + жидкость";
        double rl, rg;
        if (!ljBinodal(T, rl, rg)) return "сверхкритический флюид";
        if (rho <= rg) return "газ";
        if (rho < rl) return "газ + жидкость";
        return "жидкость";
    }
    if (rho >= ljMelt(T)) return "твёрдое";
    double rl, rg; ljBinodal(T, rl, rg);
    return rho <= rg ? "газ" : "твёрдое + газ";
}
// ===================================== АГРЕГАТНОЕ СОСТОЯНИЕ АТОМОВ ========================
// Классификация (дёшево, раз в анализ):
//   • стенка/закреплённый — PH_WALL;
//   • твёрдое — кристаллическое окружение (Штейнхардт/тен Вольде в 3D, ψ6/ψ4 в 2D, лёд), либо плотное
//     аморфное окружение с почти нулевой подвижностью за последние ~2τ (стекло);
//   • газ — мало соседей (3D ≤ 2, 2D ≤ 1) вне больших ковалентных сеток;
//   • иначе жидкость. Затем сглаживание «голосованием» соседей; водород наследует состояние своего партнёра.
namespace PH {
static std::vector<double> rx, ry, rz, disp; static double rt = -1; static int rn = -1;   // подвижность за окно
static std::vector<int> comp;                                                          // размер молекулы атома
static double fs = 0, fl = 0, fg = 0; static bool init = false;                        // сглаженные доли
static double fsSlow = 0, fgSlow = 0;                                                  // медленно сглаженные — для тренда
static int stS = -1, stG = -1;                                                         // состояния с гистерезисом
// начало перехода: ext — экстремум доли в текущем состоянии; когда доля отошла от него на 0.15, запоминаем (H, T, f)
struct Anchor { double H = 0, T = 0, f = 0, t = 0, ext = 0; bool ok = false, set = false; };
static Anchor aS, aG;
static std::vector<std::pair<float, float>> trace;                                     // (ρ*, T*) — трасса на фазовой диаграмме
static double lastTraceT = -1;
static double latentS = 0, latentG = 0;                                                // последние теплоты перехода (ε/атом)
}
static double enthalpyNow() {   // H = U + PV (без энергии «бани» термостата)
    double U = EN.ek + EN.enb + EN.ebond + EN.egrav + EN.efo;
    return U + std::max(0.0, EN.P) * boxVolume();
}
static void phaseLogAdd(const std::string& s) { phaseLog.push_back(s); if (phaseLog.size() > 60) phaseLog.erase(phaseLog.begin()); }
static void updateAtomPhase() {
    const int n = S.n;
    atomPhase.assign(n, PH_LIQ);
    if (n == 0 || (int)A::coord.size() != n) { phaseFrac[0] = phaseFrac[1] = phaseFrac[2] = 0; return; }
    const bool d3 = DIM == 3;
    // подвижность: смещение (развёрнутые координаты) за окно ≥ 2τ
    if (PH::rn != n || PH::rt < 0 || S.t < PH::rt) { PH::rx = S.ux; PH::ry = S.uy; PH::rz = S.uz; PH::rt = S.t; PH::rn = n; PH::disp.assign(n, 1e9); }
    else if (S.t - PH::rt >= 2.0) {
        PH::disp.resize(n);
        for (int i = 0; i < n; i++) { double dx = S.ux[i] - PH::rx[i], dy = S.uy[i] - PH::ry[i], dz = S.uz[i] - PH::rz[i]; PH::disp[i] = std::sqrt(dx * dx + dy * dy + dz * dz); }
        PH::rx = S.ux; PH::ry = S.uy; PH::rz = S.uz; PH::rt = S.t;
    }
    // размеры молекул (ковалентные сетки > 12 атомов — конденсированное вещество)
    {
        std::vector<int> par(n); for (int i = 0; i < n; i++) par[i] = i;
        auto find = [&](int a) { while (par[a] != a) { par[a] = par[par[a]]; a = par[a]; } return a; };
        for (int i = 0; i < n; i++) for (int k = 0; k < S.nbc[i]; k++) { int a = find(i), b = find(S.nb[i][k]); if (a != b) par[a] = b; }
        std::vector<int> sz(n, 0); for (int i = 0; i < n; i++) sz[find(i)]++;
        PH::comp.resize(n); for (int i = 0; i < n; i++) PH::comp[i] = sz[find(i)];
    }
    // газ: соседей не больше, чем у атома в паре/тройке (в жидкости у Tt — 10–12 в 3D и 5–6 в 2D; у поверхности вдвое меньше)
    const int gasCut = d3 ? 3 : 2, dense = d3 ? 4 : 3;
    const double slowCut = d3 ? 0.3 : 0.25;   // σ за окно ≥ 2τ: колебания в кристалле ≈ 0.1σ, диффузия жидкости ≥ 0.35σ
    std::vector<unsigned char> raw(n, PH_LIQ);
    for (int i = 0; i < n; i++) {
        if (frozenAt(i)) { raw[i] = PH_WALL; continue; }
        const int c = A::coord[i];
        const bool cryst = A::stype[i] != ST_OTHER;
        const bool slow = PH::disp.size() == (size_t)n && PH::disp[i] < slowCut;
        if (PH::comp[i] > 12) raw[i] = (cryst || slow) ? PH_SOLID : PH_LIQ;
        else if (c <= gasCut) raw[i] = PH_GAS;
        else if (cryst || (c >= dense && slow)) raw[i] = PH_SOLID;
        else raw[i] = PH_LIQ;
    }
    // сглаживание (2 прохода): атом среди твёрдых соседей — твёрдый (поверхность кристалла, молекулярные кристаллы),
    // «случайно упорядоченный» атом среди жидких — жидкий. Водород и газ не сглаживаются.
    std::vector<unsigned char> cur = raw;
    for (int pass = 0; pass < 2; pass++) {
#pragma omp parallel for schedule(dynamic, 64)
        for (int i = 0; i < n; i++) {
            unsigned char r = cur[i]; atomPhase[i] = r;
            if (r == PH_WALL || r == PH_GAS || S.ty[i] == E_H) continue;
            int ns = 0, tot = 0;
            for (int p = nlStart[i]; p < nlStart[i + 1]; p++) {
                int j = nlIdx[p]; if (S.ty[j] == E_H || cur[j] == PH_WALL) continue;
                double s = 1.35 * 0.5 * (EL[S.ty[i]].sig + EL[S.ty[j]].sig); if (dist2(i, j) > s * s) continue;
                tot++; if (cur[j] == PH_SOLID) ns++;
            }
            if (r == PH_LIQ && tot >= 2 && 2 * ns >= tot) atomPhase[i] = PH_SOLID;
            else if (r == PH_SOLID && tot >= 3 && A::stype[i] == ST_OTHER && 4 * ns < tot) atomPhase[i] = PH_LIQ;
        }
        cur = atomPhase;
    }
    // выше критической температуры (для чистого LJ-вещества) жидкость и газ неразличимы — флюид (относим к газу)
    { double Tr, rr; reducedState(Tr, rr); if (pureLJType() >= 0 && Tr > ljRef().Tc) for (int i = 0; i < n; i++) if (atomPhase[i] == PH_LIQ) atomPhase[i] = PH_GAS; }
    // водород — как его партнёр
    for (int i = 0; i < n; i++) if (S.ty[i] == E_H && S.nbc[i] > 0 && atomPhase[i] != PH_WALL) atomPhase[i] = atomPhase[S.nb[i][0]] == PH_WALL ? PH_SOLID : atomPhase[S.nb[i][0]];
    // доли фаз (подвижные атомы без водорода)
    int cnt[3] = {0, 0, 0};
    for (int i = 0; i < n; i++) if (atomPhase[i] != PH_WALL && S.ty[i] != E_H) cnt[atomPhase[i]]++;
    int tot = cnt[0] + cnt[1] + cnt[2];
    for (int k = 0; k < 3; k++) phaseFrac[k] = tot ? (double)cnt[k] / tot : 0;
}
// Детектор переходов: гистерезис по сглаженным долям, теплота перехода по скачку энтальпии на «плато»
static void detectTransitions() {
    int nm = 0; for (int i = 0; i < S.n; i++) if (atomPhase.size() == (size_t)S.n && atomPhase[i] != PH_WALL && S.ty[i] != E_H) nm++;
    if (nm < 8) { PH::init = false; return; }
    if (!PH::init) {
        PH::fs = PH::fsSlow = phaseFrac[2]; PH::fl = phaseFrac[1]; PH::fg = PH::fgSlow = phaseFrac[0]; PH::init = true;
        PH::stS = PH::stG = -1; PH::aS = PH::Anchor(); PH::aG = PH::Anchor();
    }
    const double a = 0.3;   // сглаживание по нескольким отсчётам
    PH::fs += a * (phaseFrac[2] - PH::fs); PH::fl += a * (phaseFrac[1] - PH::fl); PH::fg += a * (phaseFrac[0] - PH::fg);
    PH::fsSlow += 0.05 * (PH::fs - PH::fsSlow); PH::fgSlow += 0.05 * (PH::fg - PH::fgSlow);
    const double H = enthalpyNow(), T = EN.T;
    // якорь: следим за экстремумом доли в текущем состоянии; отход на 0.15 — начало перехода (H, T, f запоминаются);
    // возврат к экстремуму — ложная тревога, якорь снимается
    auto anchorUpd = [&](PH::Anchor& an, double f, int st) {
        if (!an.ok) { an = PH::Anchor(); an.ext = f; an.ok = true; }
        if (!an.set) {
            an.ext = st == 1 ? std::max(an.ext, f) : std::min(an.ext, f);
            if (std::fabs(f - an.ext) >= 0.15) { an.H = H; an.T = T; an.f = f; an.t = S.t; an.set = true; }
        } else if (std::fabs(f - an.ext) < 0.05) an.set = false;
    };
    // запись: время, вид перехода, T в начале → в конце перехода
    auto stamp = [&](const PH::Anchor& an, const std::string& what) {
        double T0 = an.set ? an.T : T;
        return fmt("t=%.1fτ (%.0f пс)  ", S.t, toPs(S.t)) + ::T(what) + fmt(" при T* %.3f→%.3f (%.0f→%.0f K)", T0, T, toKelvin(T0), toKelvin(T));
    };
    // ΔH на атом перешедшего вещества по окну перехода за вычетом «явной» теплоты c·ΔT (c ≈ d·k);
    // только если T на этом окне почти постоянна (плато: нагрев постоянной мощностью, поршень, NPT)
    auto latent = [&](const PH::Anchor& an, double fNow) {
        if (!an.set) return 0.0;
        double df = std::fabs(fNow - an.f); if (df < 0.1) return 0.0;
        if (std::fabs(T - an.T) > 0.15 * std::max(T, an.T)) return 0.0;
        return (H - an.H - DIM * nm * (T - an.T)) / (nm * df);
    };
    // твёрдое: HIGH > 0.5, LOW < 0.25
    int s = PH::fs > 0.5 ? 1 : (PH::fs < 0.25 ? 0 : PH::stS);
    if (PH::stS < 0) PH::stS = s < 0 ? (PH::fs >= 0.375 ? 1 : 0) : s;
    else if (s != PH::stS && s >= 0) {
        double L = latent(PH::aS, PH::fs);
        bool viaGas = PH::fg > PH::fl;
        std::string what = s == 0 ? (viaGas ? "сублимация (твёрдое → газ)" : "плавление") : (viaGas ? "десублимация (газ → твёрдое)" : "кристаллизация");
        std::string msg = stamp(PH::aS, what) + fmt(": твёрдое %.0f%% → %.0f%%", 100 * PH::aS.ext, 100 * PH::fs);
        if (L != 0) { msg += fmt(",  ΔH ≈ %+.2f ε/ат (%+.2f кДж/моль)", L, toKJmol(L)); PH::latentS = L; }
        else msg += ::T(" (плато T не выражено — ΔH не оценивается)");
        phaseLogAdd(msg); PH::stS = s; PH::aS = PH::Anchor();
    }
    anchorUpd(PH::aS, PH::fs, PH::stS);
    // газ: HIGH > 0.6, LOW < 0.3
    int g = PH::fg > 0.6 ? 1 : (PH::fg < 0.3 ? 0 : PH::stG);
    if (PH::stG < 0) PH::stG = g < 0 ? (PH::fg >= 0.45 ? 1 : 0) : g;
    else if (g != PH::stG && g >= 0) {
        double L = latent(PH::aG, PH::fg);
        std::string what = g == 1 ? (PH::fs > PH::fl ? "возгонка (твёрдое → газ)" : "кипение / испарение") : "конденсация";
        std::string msg = stamp(PH::aG, what) + fmt(": газ %.0f%% → %.0f%%", 100 * PH::aG.ext, 100 * PH::fg);
        if (L != 0) { msg += fmt(",  ΔH ≈ %+.2f ε/ат (%+.2f кДж/моль)", L, toKJmol(L)); PH::latentG = L; }
        else msg += ::T(" (плато T не выражено — ΔH не оценивается)");
        phaseLogAdd(msg); PH::stG = g; PH::aG = PH::Anchor();
    }
    anchorUpd(PH::aG, PH::fg, PH::stG);
    // трасса на диаграмме T–ρ
    if (PH::lastTraceT < 0 || S.t < PH::lastTraceT || S.t - PH::lastTraceT > 0.25) {
        PH::trace.push_back({(float)numberDensity(), (float)EN.T}); PH::lastTraceT = S.t;
        if (PH::trace.size() > 400) PH::trace.erase(PH::trace.begin());
    }
}
static void resetPhaseTracking() { PH::init = false; PH::trace.clear(); PH::lastTraceT = -1; PH::rn = -1; phaseLog.clear(); PH::latentS = PH::latentG = 0; }

static void computeMolecules() {
    int n = S.n; std::vector<int> par(n); for (int i = 0; i < n; i++) par[i] = i;
    auto find = [&](int a) { while (par[a] != a) { par[a] = par[par[a]]; a = par[a]; } return a; };
    for (int i = 0; i < n; i++) for (int k = 0; k < S.nbc[i]; k++) { int a = find(i), b = find(S.nb[i][k]); if (a != b) par[a] = b; }
    std::unordered_map<int, std::array<int, NEL>> comp;
    for (int i = 0; i < n; i++) { if (EL[S.ty[i]].fixed) continue; auto it = comp.find(find(i)); if (it == comp.end()) { std::array<int, NEL> z{}; it = comp.emplace(find(i), z).first; } it->second[S.ty[i]]++; }
    A::mol.clear();
    for (auto& kv : comp) { int cnt[NEL]; int tot = 0; for (int t = 0; t < NEL; t++) { cnt[t] = kv.second[t]; tot += cnt[t]; } if (tot) A::mol[formulaOf(cnt)]++; }
    for (auto& kv : A::mol) if (std::find(A::species.begin(), A::species.end(), kv.first) == A::species.end() && A::species.size() < 12) {
        A::species.push_back(kv.first); Series s; s.v.assign(A::concT.v.size(), 0.f); A::conc.push_back(s);
    }
    A::concT.push(0);
    for (size_t k = 0; k < A::species.size(); k++) { auto it = A::mol.find(A::species[k]); A::conc[k].push(it == A::mol.end() ? 0.f : (float)it->second); }
    if (A::mol0.empty()) A::mol0 = A::mol;
    // степень протекания: доля израсходованного исходного вещества (многоатомные молекулы)
    A::progress = 0; A::progressLabel.clear();
    for (auto& kv : A::mol0) {
        bool poly = false; for (char ch : kv.first) if (ch >= '2' && ch <= '9') poly = true;
        if (kv.first.size() >= 3 && (poly || kv.first == "HCl")) {
            auto it = A::mol.find(kv.first); int cur = it == A::mol.end() ? 0 : it->second;
            double p = 1.0 - (double)cur / std::max(1, kv.second);
            if (p > A::progress) { A::progress = p; A::progressLabel = kv.first; }
        }
    }
    // растворённые ионы: рядом (r < 1.4σ) нет ни одного противоиона
    A::ionsTotal = 0; A::ionsFree = 0;
    if (present[E_NA] || present[E_CLM]) {
        std::vector<int> ions; for (int i = 0; i < n; i++) if (S.ty[i] == E_NA || S.ty[i] == E_CLM) ions.push_back(i);
        for (int i : ions) {
            A::ionsTotal++; bool paired = false; int want = S.ty[i] == E_NA ? E_CLM : E_NA;
            for (int j : ions) if (S.ty[j] == want && dist2(i, j) < 1.96) { paired = true; break; }
            if (!paired) A::ionsFree++;
        }
    }
}
static void computeVelHist() {
    int cnt[NEL] = {0};
    for (int i = 0; i < S.n; i++) if (!frozenAt(i)) cnt[S.ty[i]]++;
    int best = E_AR; for (int t = 0; t < NEL; t++) if (cnt[t] > cnt[best]) best = t;
    if (best != A::vhType) std::fill(A::vh.begin(), A::vh.end(), 0.0);
    A::vhType = best;
    double m = EL[best].m, T = std::max(EN.T, 0.005);
    A::vhMax = 4.0 * std::sqrt(T / m);
    std::vector<double> h(A::VH_BINS, 0.0); int c = 0;
    for (int i = 0; i < S.n; i++) if (S.ty[i] == best && !S.pin[i]) {
        double v = std::sqrt(S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]);
        int b = (int)(v / A::vhMax * A::VH_BINS); if (b < A::VH_BINS) h[b] += 1; c++;
    }
    double dv = A::vhMax / A::VH_BINS;
    for (int k = 0; k < A::VH_BINS; k++) { double f = c ? h[k] / (c * dv) : 0; A::vh[k] = 0.85 * A::vh[k] + 0.15 * f; }
}
// теоретическое распределение Максвелла–Больцмана по модулю скорости
static double maxwellF(double v, double m, double T) {
    if (DIM == 3) return 4 * PI * v * v * std::pow(m / (2 * PI * T), 1.5) * std::exp(-m * v * v / (2 * T));
    return m * v / T * std::exp(-m * v * v / (2 * T));
}
static void computeTProfile() {
    A::tprofAxis = (P.heatWalls == 2 || (P.gravity > 0 && !isPer())) ? 1 : 0;
    std::vector<double> ke(A::TP_BINS, 0.0); std::vector<int> c(A::TP_BINS, 0);
    for (int i = 0; i < S.n; i++) {
        const Element& e = EL[S.ty[i]]; if (frozenAt(i)) continue;
        double u = A::tprofAxis ? S.y[i] / S.Ly : S.x[i] / S.Lx;
        int b = clampv((int)(u * A::TP_BINS), 0, A::TP_BINS - 1);
        ke[b] += e.m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]) / DIM; c[b]++;   // kT = m v²/d
    }
    for (int k = 0; k < A::TP_BINS; k++) { double T = c[k] ? ke[k] / c[k] : 0; A::tprof[k] = 0.9 * A::tprof[k] + 0.1 * T; }
}
// Словесное описание состояния по долям фаз атомов (сглаженным) и степени кристалличности
static void updatePhase() {
    std::string p;
    const double tot = phaseFrac[0] + phaseFrac[1] + phaseFrac[2];
    if (EN.nmob == 0 || tot < 0.5) { A::phase = EN.nmob == 0 ? "—" : A::phase; return; }
    const double fg = PH::init ? PH::fg : phaseFrac[0], fl = PH::init ? PH::fl : phaseFrac[1], fs = PH::init ? PH::fs : phaseFrac[2];
    const double fc = A::fCryst;
    auto solidName = [&]() { return fc > 0.5 ? "кристалл" : (fc > 0.15 ? "поликристалл (зёрна, границы, дефекты)" : "стекло (аморфное твёрдое)"); };
    if (fs > 0.12 && fl > 0.12 && fg > 0.12) p = "три фазы: твёрдое + жидкость + пар";
    else if (fs >= 0.85) p = solidName();
    else if (fl >= 0.85) p = "жидкость";
    else if (fg >= 0.85) {
        double Tr, rr; reducedState(Tr, rr);
        p = pureLJType() >= 0 && Tr > ljRef().Tc && rr > 0.5 * ljRef().rc ? "сверхкритический флюид" : "газ";
    }
    else {
        // две фазы: какие и куда идёт процесс (тренд долей за последние ~10 отсчётов)
        const double ts = PH::init ? PH::fs - PH::fsSlow : 0, tg = PH::init ? PH::fg - PH::fgSlow : 0, e = 0.004;
        const bool SL = std::min(fs, fl) >= fg, LG = std::min(fl, fg) >= fs;
        if (SL) p = ts < -e ? "плавление: кристалл + жидкость" : (ts > e ? "кристаллизация: жидкость + кристалл" : "твёрдое + жидкость (сосуществование)");
        else if (LG) p = tg > e ? "испарение: жидкость + пар" : (tg < -e ? "конденсация: пар + капли жидкости" : "жидкость + пар (сосуществование)");
        else p = tg > e ? "сублимация: твёрдое + пар" : (tg < -e ? "десублимация: пар → твёрдое (иней)" : "твёрдое + пар");
    }
    A::phase = p;
}
// Раз в несколько кадров: вся аналитика
static int anaTick = 0;
static void analysisTick() {
    anaTick++;
    measure();
    A::sT.push((float)EN.T); A::sP.push((float)EN.P);
    A::sEk.push((float)EN.ek); A::sEp.push((float)(EN.enb + EN.egrav + EN.ebond)); A::sEt.push((float)EN.total()); A::sTime.push((float)S.t);
    if (anaTick % 3 == 0) { computeGr(); computeVelHist(); computeTProfile(); }
    computeOrder();
    updateAtomPhase(); detectTransitions();
    // MSD(t) = <|r(t) − r(0)|²>;  D = MSD/(2d·t)
    if (A::msdN != S.n) resetMSD();
    {
        double s = 0, sb = 0; int c = 0, cb = 0;
        for (int i = 0; i < S.n; i++) {
            if (frozenAt(i)) continue;
            double dx = S.ux[i] - A::x0[i], dy = S.uy[i] - A::y0[i], dz = S.uz[i] - A::z0[i], d2 = dx * dx + dy * dy + dz * dz;
            if (S.ty[i] == E_BIG) { sb += d2; cb++; } else { s += d2; c++; }
        }
        if ((int)A::msdT.size() >= 1200) {   // прореживание вдвое
            std::vector<float> a, b, d; for (size_t k = 0; k < A::msdT.size(); k += 2) { a.push_back(A::msdT[k]); b.push_back(A::msdV[k]); d.push_back(A::msdBig[k]); }
            A::msdT = a; A::msdV = b; A::msdBig = d;
        }
        A::msdT.push_back((float)(S.t - A::t0)); A::msdV.push_back(c ? (float)(s / c) : 0.f); A::msdBig.push_back(cb ? (float)(sb / cb) : 0.f);
        size_t m = A::msdT.size();
        if (m > 10) {
            size_t k0 = m / 2; double dt = A::msdT[m - 1] - A::msdT[k0];
            if (dt > 0) { A::D = (A::msdV[m - 1] - A::msdV[k0]) / (2 * DIM * dt); A::Dbig = (A::msdBig[m - 1] - A::msdBig[k0]) / (2 * DIM * dt); }
        }
    }
    if (anaTick % 5 == 0) computeMolecules();
    // Аррениус: скорость реакций (события/время/атом) при средней T
    {
        double dt = S.t - A::lastT; A::lastT = S.t;
        A::arrTsum += EN.T * dt; A::arrTime += dt;
        long long ev = CH.assoc + CH.exch;
        if (A::arrTime > 2.0) {
            long long d = ev - A::arrEv;
            if (d > 0 && EN.nmob > 0) {
                double Tavg = A::arrTsum / A::arrTime, rate = d / A::arrTime / EN.nmob;
                A::arr.push_back({(float)(1.0 / Tavg), (float)std::log(rate)});
                if (A::arr.size() > 80) A::arr.erase(A::arr.begin());
            }
            A::arrEv = ev; A::arrTsum = 0; A::arrTime = 0;
            // линейная регрессия ln k = ln A − Ea/T → Ea = −наклон (только при заметном разбросе T)
            float lo = 1e9f, hi = -1e9f; for (auto& p : A::arr) { lo = std::min(lo, p.first); hi = std::max(hi, p.first); }
            if (A::arr.size() >= 4 && hi - lo > 0.08f * hi) {
                double sx = 0, sy = 0, sxx = 0, sxy = 0; int k = (int)A::arr.size();
                for (auto& p : A::arr) { sx += p.first; sy += p.second; sxx += p.first * p.first; sxy += p.first * p.second; }
                double den = k * sxx - sx * sx;
                if (std::fabs(den) > 1e-9) { A::arrEa = -(k * sxy - sx * sy) / den; A::arrFit = true; }
            }
        }
    }
    // теплоёмкость по флуктуациям (канонический ансамбль, только в равновесии): C_v = <δE²>/(kT²)
    if (canonicalTh(P.thermostat) && std::fabs(EN.T - P.Tset) < 0.1 * P.Tset) {
        A::eHist.push_back(EN.total() - EN.enh);
        if (A::eHist.size() > 300) A::eHist.erase(A::eHist.begin());
        if (A::eHist.size() > 50 && EN.nmob > 0) {
            // дисперсия относительно линейного тренда (медленная релаксация — не флуктуация)
            int k = (int)A::eHist.size(); double sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (int q = 0; q < k; q++) { sx += q; sy += A::eHist[q]; sxx += (double)q * q; sxy += q * A::eHist[q]; }
            double b = (k * sxy - sx * sy) / std::max(1e-12, k * sxx - sx * sx), a = (sy - b * sx) / k, v = 0;
            for (int q = 0; q < k; q++) { double r = A::eHist[q] - (a + b * q); v += r * r; }
            v /= k;
            A::Cv = v / (P.Tset * P.Tset) / EN.nmob;
        }
    } else { A::eHist.clear(); A::Cv = 0; }
    updatePhase();
}


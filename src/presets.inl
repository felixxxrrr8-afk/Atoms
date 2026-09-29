// ===================================== PRESETS / UNDO ==================================
struct Snapshot { Sim s; Params p; std::vector<FieldObj> fo; int selFo = -1; };
static std::vector<Snapshot> undoStack;
static void onDimChanged();
static void pushUndo() { S.dim = DIM; undoStack.push_back({S, P, fieldObjs, selFieldObj}); if (undoStack.size() > 20) undoStack.erase(undoStack.begin()); }
struct ScriptEv { double t; int action; bool done; };
static std::vector<ScriptEv> script;
static int currentPreset = 1, presetVariant = 0;
static double sceneAspect = 0.66;
static std::string toast; static double toastTime = 0;
static bool trailsOn = false, bondsOn = true;
static double atomVis = 1.0;           // визуальный масштаб шаров
static int lmbTool = 0;                // инструмент ЛКМ: 0 добавить/пинцет, 1 ластик, 2 нагрев, 3 охлаждение, 4 камера
static bool ptOn = false, menuOn = false;   // открыты таблица Менделеева / меню сцен
static int colorMode = 0;
static int selPal = 1;
// режим «штампа» молекулы библиотеки: выбрана структура из вкладки «Химия» (первая ячейка палитры).
// UI: ЛКМ в сцене (инструмент «добавить») → pushUndo(); insertMolecule(chemStampMol, x, y, z) — один раз за щелчок
static inline bool chemStampActive() { return chemStampMol >= 0 && selPal == 0; }
static bool viewFitPending = true;
static bool fullscreen = false; static void toggleFullscreen();
static void showToast(const std::string& s) { toast = LANG == LANG_RU ? s : std::string(Tsv(s, "showToast")); toastTime = 3.0; }   // точный литерал переводится
static void resetMSD();
static void popUndo() {
    if (undoStack.empty()) return;
    S = undoStack.back().s; bool pz = P.paused; P = undoStack.back().p; P.paused = pz;
    fieldObjs = undoStack.back().fo; selFieldObj = clampv(undoStack.back().selFo, -1, (int)fieldObjs.size() - 1); undoStack.pop_back();
    if (S.dim != DIM) { DIM = S.dim; onDimChanged(); viewFitPending = true; }
    grabbed = -1; buildPairTables(); updatePresence(); computeForces(); resetEnergyRef(); resetMSD();
}

static void worldReset(double Lx, double Ly, double Lz, int boundary) {
    S = Sim(); S.Lx = Lx; S.Ly = Ly; S.Lz = DIM == 3 ? Lz : 1; S.dim = DIM; P.boundary = boundary;
    P.npt = false; P.heatWalls = 0; P.gravity = 0; P.catalyst = false; P.chemistry = true;
    P.thermostat = TH_BUSSI; P.tauT = 0.5; P.epsScale = 1.0; P.wallAttr = 0.6; P.heatPower = 0.03; P.efield = 0; P.eaScale = 1.0;
    P.acidBase = true; P.surfCat = true; rxT0 = 0; chemPT = 0;
    grabbed = -1; followAtom = -1; flashes.clear(); CH = ChemStats(); script.clear(); trailsOn = false; P.substeps = 8;
    pistonGrab = false; nlValid = false; fieldObjs.clear(); selFieldObj = -1; heatWallQ[0] = heatWallQ[1] = 0;
    buildPairTables();
}
static void finishPreset() {
    wrapAll(); zeroMomentum(); updatePresence(); P.dt = P.dtBase;
    computeForces(); resetEnergyRef(); resetAnalysis(); resetMSD();
}
// presetTitle — всегда целый русский литерал (на экран — через перевод); presetLoaded — состояние загружено из файла
static std::string presetTitle; static bool presetLoaded = false;
static int presetVariants(int k) {
    if (k == 2) return DIM == 3 ? 6 : 3;
    if (k == 7 || k == 9 || k == 17 || k == 22 || k == 27) return 2;
    return 1;
}
// шарик металла (ГЦК в 3D, треугольная решётка в 2D) радиуса R (σ) с центром (cx, cy, cz)
static void metalBall(int t, double cx, double cy, double cz, double R, double T) {
    const double d = 2 * EL[t].rmet / 3.405;
    if (DIM == 3) {
        const double a = d * std::sqrt(2.0); const int m = (int)(R / a) + 2;
        const double bs[4][3] = {{0, 0, 0}, {0.5, 0.5, 0}, {0.5, 0, 0.5}, {0, 0.5, 0.5}};
        for (int k = -m; k <= m; k++) for (int j = -m; j <= m; j++) for (int i = -m; i <= m; i++) for (auto& b : bs) {
            double x = (i + b[0]) * a, y = (j + b[1]) * a, z = (k + b[2]) * a;
            if (x * x + y * y + z * z > R * R) continue;
            double vx, vy, vz; thermalVel(t, T, vx, vy, vz); addAtom(t, cx + x, cy + y, cz + z, vx, vy, vz);
        }
    } else {
        const int m = (int)(R / d) + 2;
        for (int j = -m; j <= m; j++) for (int i = -m; i <= m; i++) {
            double x = (i + 0.5 * (j & 1)) * d, y = j * d * std::sqrt(3.0) / 2;
            if (x * x + y * y > R * R) continue;
            double vx, vy, vz; thermalVel(t, T, vx, vy, vz); addAtom(t, cx + x, cy + y, 0, vx, vy, 0);
        }
    }
}
// водный раствор: ящик с периодическими границами, ионы/молекулы добавляются до воды
static void waterBox(double L3, double L2x) {
    if (DIM == 3) { worldReset(L3, L3, L3, B_PERIODIC); P.substeps = 3; }
    else { worldReset(L2x, L2x * sceneAspect, 1, B_PERIODIC); P.substeps = 5; }
}
static int waterCount() { return (int)((DIM == 3 ? 0.55 : 0.64) * boxVolume()); }
// стенка-перегородка из неподвижных атомов в плоскости x = x0
static void partition(double x0) {
    for (double y = 0.5; y < S.Ly; y += 0.9) {
        if (DIM == 2) addAtom(E_WALL, x0, y, 0, 0, 0, 0);
        else for (double z = 0.5; z < S.Lz; z += 0.9) addAtom(E_WALL, x0, y, z, 0, 0, 0);
    }
}
// шаблон «один атом типа t» (для элементов, которых нет в палитре веществ)
static Tmpl atomTmpl(int t) { Tmpl m; m.label = EL[t].sym; m.a = {{t, 0, 0, 0}}; return m; }
// пористая перегородка в плоскости x = x0: объекты-барьеры (чисто отталкивающие, без адсорбции), между ними
// nSlits щелей шириной slit (2D — отрезки, 3D — полосы через весь ящик по z). Барьеры сразу в полную силу.
static void porousMembrane(double x0, double slit, int nSlits) {
    const double p = S.Ly / nSlits;
    for (int k = 0; k <= nSlits; k++) {
        double y0 = k == 0 ? -1.0 : (k - 0.5) * p + 0.5 * slit, y1 = k == nSlits ? S.Ly + 1.0 : (k + 0.5) * p - 0.5 * slit;
        FieldObj o = makeFieldObj(FO_BARRIER, x0, 0.5 * (y0 + y1), 0.5 * S.Lz);
        if (DIM == 2) { o.x = o.x2 = x0; o.y = y0; o.y2 = y1; o.z = o.z2 = 0; o.R = 0; }
        else { o.x = o.x2 = x0; o.y = o.y2 = 0.5 * (y0 + y1); o.z = -1.0; o.z2 = S.Lz + 1.0; o.R = 0.5 * (y1 - y0); }
        o.dx = 1; o.dy = 0; o.dz = 0; o.strength = 3.0; o.ramp = 1.0;
        fieldObjs.push_back(o);
    }
}
// ---- Живые измерения сцен 21–25(строка A::sceneNote под заголовком; обновляется в analysisTick)
namespace SM {
static double tPrev = -1;                                   // время прошлого отсчёта (−1 — отсчётов ещё не было)
static double q0 = 0, q1 = 0, qh = 0, qc = 0; static bool qInit = false;   // теплопроводность: теплота стенок и сглаженные мощности
static double tOpen = -1, xOpen = 0, T0 = 0, Ep0 = 0;      // момент разрыва мембраны, её положение, T и Eпот/атом до разрыва
static std::vector<std::pair<double, double>> front;        // ударная волна: (t, x) фронта
static double theoW = 0, rhoR0 = 0, rho21 = 1, speedW = 0, T1 = 1;   // теория: скорость фронта, плотность и T перед ним, сжатие; измеренная скорость
static std::vector<double> avg[2];                          // барометрическая формула: профили плотности, усреднённые за ~10τ
static double V0 = 0;                                       // спекание: начальное расстояние между центрами частиц
static double adT0 = 0, adV0 = 0;                           // адиабатическое сжатие: T и V перед первой ступенью давления
static std::vector<double> vprof;                           // течение Пуазейля: профиль vx(y), усреднённый за ~5τ
}
static void sceneMeasureReset() { SM::tPrev = -1; SM::qInit = false; SM::tOpen = -1; SM::front.clear(); SM::speedW = 0; SM::V0 = 0; SM::avg[0].clear(); SM::avg[1].clear();
                                  SM::adT0 = SM::adV0 = 0; SM::vprof.clear(); }
// двухатомная частица по таблице связей (I2, HI …)
static Tmpl diTmpl(const char* l, int t1, int t2, int o) { Tmpl m; m.label = l; double r = r0of(t1, t2, o); m.a = {{t1, -r / 2, 0, 0}, {t2, r / 2, 0, 0}}; m.b = {{0, 1, o}}; return m; }
static void loadPreset(int k, int variant) {
    currentPreset = k; presetVariant = variant % std::max(1, presetVariants(k));
    const bool d3 = DIM == 3; const double asp = sceneAspect;
    switch (k) {
    case 1: {   // идеальный газ: PV ≈ NkT
        if (d3) worldReset(30, 30, 30, B_WALLS); else worldReset(70, 70 * asp, 1, B_WALLS);
        P.Tset = 2.0; P.thermostat = TH_NVE; colorMode = 1;
        fillBox(palette[findPal("Ar")], d3 ? 800 : 400, P.Tset);
        presetTitle = "1 · Идеальный газ: сравните P с NkT/V (Z = PV/NkT ≈ 1)"; break; }
    case 2: {   // кристалл со свободной поверхностью в ящике с вакуумом, нагрев постоянной мощностью
        int v = presetVariant; double T0 = 0.1;
        if (!d3) {
            if (v == 0) {        // гексагональная (треугольная) — устойчивая плотная упаковка в 2D
                double a = 1.10; int nx = 32, ny = 34; double cw = nx * a, ch = ny * a * std::sqrt(3.0) / 2, L = cw * 1.9;
                worldReset(L, std::max(L * asp, ch * 1.6), 1, B_PERIODIC);
                hexLattice2D(E_AR, (S.Lx - cw) / 2, (S.Ly - ch) / 2, nx, ny, a, T0);
            } else if (v == 1) { // квадратная — неустойчива для LJ, перестраивается в гексагональную
                double a = 1.08; int nx = 30, ny = 30; double cw = nx * a, L = cw * 1.9;
                worldReset(L, std::max(L * asp, cw * 1.5), 1, B_PERIODIC);
                squareLattice2D(E_AR, -1, (S.Lx - cw) / 2, (S.Ly - cw) / 2, nx, ny, a, T0);
            } else {             // ионный кристалл NaCl
                double a = 0.92; int nx = 20, ny = 20; double cw = nx * a, L = cw * 2.2;
                worldReset(L, std::max(L * asp, cw * 1.6), 1, B_PERIODIC);
                squareLattice2D(E_NA, E_CLM, (S.Lx - cw) / 2, (S.Ly - cw) / 2, nx, ny, a, T0);
            }
            static const char* const t2[] = {"2 · Плавление: гексагональная решётка, нагрев P=const → плато T(t). Повтор 2 — другая решётка",
                                             "2 · Плавление: квадратная (неустойчива) решётка, нагрев P=const → плато T(t). Повтор 2 — другая решётка",
                                             "2 · Плавление: NaCl (ионная) решётка, нагрев P=const → плато T(t). Повтор 2 — другая решётка"};
            presetTitle = t2[clampv(v, 0, 2)];
        } else {
            int kind = v, nx, ny, nz; double a;
            switch (kind) {
            case L_FCC: a = 1.56; nx = ny = nz = 7; break;
            case L_HCP: a = 1.10; nx = 10; ny = 6; nz = 6; break;
            case L_BCC: a = 1.26; nx = ny = nz = 9; break;
            case L_SC: a = 1.05; nx = ny = nz = 11; break;
            case L_NACL: a = 0.92; nx = ny = nz = 10; break;
            default: a = 2.08; nx = ny = nz = 3; break;   // лёд
            }
            double cx = kind == L_NACL ? nx * a : nx * latticeCellX(kind, a), cy = kind == L_NACL ? ny * a : ny * latticeCellY(kind, a), cz = kind == L_NACL ? nz * a : nz * latticeCellZ(kind, a);
            double L = std::max({cx, cy, cz}) * (kind == L_ICE ? 2.6 : 2.0);
            worldReset(L, L, L, B_PERIODIC);
            if (kind == L_ICE) { T0 = 0.05; iceLattice3D((L - cx) / 2, (L - cy) / 2, (L - cz) / 2, nx, ny, nz, a, T0); }
            else lattice3D(kind, kind == L_NACL ? E_NA : E_AR, kind == L_NACL ? E_CLM : -1, 0, (L - cx) / 2, (L - cy) / 2, (L - cz) / 2, nx, ny, nz, a, T0);
            static const char* const t3[] = {   // по L_FCC, L_HCP, L_BCC, L_SC, L_NACL, L_ICE
                "2 · Плавление: ГЦК, нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд",
                "2 · Плавление: ГПУ, нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд",
                "2 · Плавление: ОЦК (для LJ неустойчива → перестраивается в плотную упаковку), нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд",
                "2 · Плавление: ПК (для LJ неустойчива → коллапс), нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд",
                "2 · Плавление: NaCl, нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд",
                "2 · Плавление: лёд, нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд"};
            presetTitle = t3[clampv(kind, 0, 5)];
        }
        P.Tset = T0; P.thermostat = TH_POWER; P.heatPower = (!d3 && v == 2) || (d3 && v == L_NACL) ? 0.03 : 0.012; colorMode = 3;
        if (d3 && v == L_ICE) { P.heatPower = 0.01; colorMode = 3; }
        break; }
    case 3: {   // кипение и испарение с поверхности под гравитацией
        if (d3) {
            // крайние слои — на равновесном расстоянии от стенок (минимум потенциала 9-3 ≈ 0.86σ)
            worldReset(10.5 * 1.6 + 1.72, 30, 10.5 * 1.6 + 1.72, B_WALLS); P.Tset = 0.8;
            lattice3D(L_FCC, E_AR, -1, 0, 0.46, 0.46, 0.46, 11, 4, 11, 1.6, 0.8);
            P.Thot = 1.2;
        } else {
            worldReset(80, 80 * asp, 1, B_WALLS); P.Tset = 0.45;
            double a = 1.12; hexLattice2D(E_AR, 1.0, 0.3, (int)(78 / a), 14, a, 0.45);
            P.Thot = 1.1;
        }
        P.gravity = 0.012; P.wallAttr = 0.8; P.thermostat = TH_BERENDSEN; colorMode = 1; if (d3) P.substeps = 6;
        script.push_back({8.0, 1, false});   // через 8τ: горячее дно, термостат выключен
        presetTitle = "3 · Кипение: жидкость под гравитацией, горячее дно, испарение с поверхности"; break; }
    case 4: {   // конденсация пересыщенного пара
        int N = d3 ? 2000 : 1300; double rho = d3 ? 0.05 : 0.12;
        if (d3) { double L = std::cbrt(N / rho); worldReset(L, L, L, B_PERIODIC); }
        else { double L = std::sqrt(N / rho / asp); worldReset(L, L * asp, 1, B_PERIODIC); }
        P.Tset = d3 ? 1.4 : 1.2; fillBox(palette[findPal("Ar")], N, P.Tset, 0.0);
        script.push_back({3.0, 2, false}); colorMode = 1;
        presetTitle = "4 · Конденсация: пар охлаждается → зародыши → круглые капли (поверхностное натяжение)"; break; }
    case 5: {   // смешивание двух газов
        if (d3) worldReset(40, 22, 22, B_WALLS); else worldReset(80, 80 * asp, 1, B_WALLS);
        P.Tset = 1.5; partition(S.Lx / 2);
        int N = d3 ? 700 : 500;
        fillRandom(palette[findPal("Ar")], N, 1, 1, 1, S.Lx / 2 - 1, S.Ly - 1, S.Lz - 1, 1.5);
        fillRandom(palette[findPal("Ne")], N, S.Lx / 2 + 1, 1, 1, S.Lx - 1, S.Ly - 1, S.Lz - 1, 1.5);
        script.push_back({2.0, 3, false}); colorMode = 0;
        presetTitle = "5 · Диффузия: перегородка исчезнет через 2τ, смотрите MSD(t) и D"; break; }
    case 6: {   // ионный кристалл NaCl в воде
        if (d3) {
            worldReset(10, 10, 10, B_PERIODIC); P.substeps = 3;
            double a = 0.92; int m = 4, cw = 0; (void)cw;
            lattice3D(L_NACL, E_NA, E_CLM, 0, S.Lx / 2 - m * a / 2 - 0.25 * a, S.Ly / 2 - m * a / 2 - 0.25 * a, S.Lz / 2 - m * a / 2 - 0.25 * a, m, m, m, a, 0.3);
            fillBox(palette[findPal("H2O")], (int)(0.55 * S.Lx * S.Ly * S.Lz), 0.7, 0.0, 0.9);
        } else {
            worldReset(40, 40 * asp, 1, B_PERIODIC); P.substeps = 5;
            double a = 0.92; int m = 6;
            squareLattice2D(E_NA, E_CLM, S.Lx / 2 - m * a / 2, S.Ly / 2 - m * a / 2, m, m, a, 0.3);
            fillBox(palette[findPal("H2O")], (int)(0.64 * S.Lx * S.Ly), 0.7, 0.0, 0.9);
        }
        P.Tset = d3 ? 0.7 : 0.9; P.tauT = 1.0; colorMode = 0;   // в 2D ионы связаны сильнее — вода горячее
        presetTitle = "6 · NaCl в горячей воде: ионы уходят в раствор, вокруг них гидратные оболочки (ускорьте слайдером «скорость»)"; break; }
    case 7: {   // горение водорода / хлороводород
        if (d3) worldReset(26, 26, 26, B_WALLS); else worldReset(70, 70 * asp, 1, B_WALLS);
        P.Tset = 1.6; P.thermostat = TH_NVE; colorMode = 0;
        if (presetVariant == 0) {
            fillBox(palette[findPal("H2")], 520, P.Tset); fillBox(palette[findPal("O2")], 260, P.Tset);
            presetTitle = "7 · 2H2 + O2 → 2H2O: искра через 1.5τ (L — ещё вспышка, повтор 7 — H2+Cl2)";
        } else {
            fillBox(palette[findPal("H2")], 360, P.Tset); fillBox(palette[findPal("Cl2")], 360, P.Tset);
            presetTitle = "7b · H2 + Cl2 → 2HCl: цепная реакция от вспышки света (L)";
        }
        script.push_back({1.5, 4, false}); break; }
    case 8: {   // броуновское движение
        int N = d3 ? 2000 : 1600;
        if (d3) { double L = std::cbrt(N / 0.3); worldReset(L, L, L, B_PERIODIC); }
        else { double L = std::sqrt(N / 0.35 / asp); worldReset(L, L * asp, 1, B_PERIODIC); }
        P.Tset = 1.0;
        addAtom(E_BIG, S.Lx / 2, S.Ly / 2, S.Lz / 2, 0, 0, 0);
        fillBox(palette[findPal("Ar")], N, 1.0, 0.0);
        trailsOn = true; colorMode = 1; if (d3) P.substeps = 6;
        presetTitle = "8 · Броуновское движение: тяжёлая частица среди атомов, MSD ∝ t"; break; }
    case 9: {   // закалка: поликристалл с дефектами / стекло из бинарной смеси
        bool glass = presetVariant == 1;
        if (d3) {
            double rho = glass ? 1.05 : 0.95, a = std::cbrt(4.0 / rho); int m = 8;
            worldReset(m * a, m * a, m * a, B_PERIODIC);
            lattice3D(L_FCC, E_AR, glass ? E_NE : -1, 0.5, -0.25 * a, -0.25 * a, -0.25 * a, m, m, m, a, 2.0);
        } else {
            double rho = glass ? 0.95 : 0.85, a = std::sqrt(2.0 / (std::sqrt(3.0) * rho)); int nx = 40, ny = 46;
            worldReset(nx * a, ny * a * std::sqrt(3.0) / 2, 1, B_PERIODIC);
            hexLattice2D(E_AR, 0, 0, nx, ny, a, 2.0, glass ? E_NE : -1, 0.5);
        }
        P.Tset = 2.0; script.push_back({3.0, 5, false}); colorMode = glass ? 4 : 3; if (d3) P.substeps = 5;
        presetTitle = glass ? "9b · Стекло: бинарная смесь Ar/Ne, резкая закалка (повтор 9 — поликристалл)"
                            : "9 · Закалка: поликристалл, границы зёрен, дислокации ([ ] — растянуть до разрушения; повтор 9 — стекло)";
        break; }
    case 11: {  // наночастица золота: металлическая связь, плавление
        int Au = typeOfZ(79); double d = 2 * EL[Au].rmet / 3.405;   // расстояние до ближайшего соседа
        if (d3) {
            double a = d * std::sqrt(2.0), R = 4.6 * a, L = 2 * R + 14;
            worldReset(L, L, L, B_PERIODIC);
            const double bs[4][3] = {{0, 0, 0}, {0.5, 0.5, 0}, {0.5, 0, 0.5}, {0, 0.5, 0.5}};
            int m = (int)(R / a) + 2;
            for (int k = -m; k <= m; k++) for (int j = -m; j <= m; j++) for (int i = -m; i <= m; i++) for (auto& b : bs) {
                double x = (i + b[0]) * a, y = (j + b[1]) * a, z = (k + b[2]) * a;
                if (x * x + y * y + z * z > R * R) continue;
                double vx, vy, vz; thermalVel(Au, 0.05, vx, vy, vz); addAtom(Au, L / 2 + x, L / 2 + y, L / 2 + z, vx, vy, vz);
            }
        } else {
            double R = 17 * d, L = 2 * R + 16;
            worldReset(L, std::max(L * asp, 2 * R + 10), 1, B_PERIODIC);
            int m = (int)(R / d) + 2;
            for (int j = -m; j <= m; j++) for (int i = -m; i <= m; i++) {
                double x = (i + 0.5 * (j & 1)) * d, y = j * d * std::sqrt(3.0) / 2;
                if (x * x + y * y > R * R) continue;
                double vx, vy, vz; thermalVel(Au, 0.05, vx, vy, vz); addAtom(Au, S.Lx / 2 + x, S.Ly / 2 + y, 0, vx, vy, 0);
            }
        }
        P.Tset = 0.05; P.thermostat = TH_POWER; P.heatPower = 0.02; colorMode = 3; P.substeps = 10;
        presetTitle = "Shift+1 · Наночастица золота: металлическая связь (многочастичный потенциал), нагрев → плавление с поверхности"; break; }
    case 12: {  // окисление железа
        int Fe = typeOfZ(26); double d = 2 * EL[Fe].rmet / 3.405;
        if (d3) {
            double a = d * std::sqrt(2.0); int nx = 10, ny = 3, nz = 10;
            worldReset(nx * a + 1.2, 22, nz * a + 1.2, B_WALLS);
            lattice3D(L_FCC, Fe, -1, 0, 0.35, 0.35, 0.35, nx, ny, nz, a, 0.4);
            fillRandom(palette[findPal("O2")], 260, 1, ny * a + 2.5, 1, S.Lx - 1, S.Ly - 1, S.Lz - 1, 0.4);
        } else {   // низкий ящик: газ O2 плотнее у поверхности — окисление заметно уже за первые τ
            worldReset(70, 26, 1, B_WALLS);
            hexLattice2D(Fe, 0.4, 0.35, (int)((S.Lx - 1) / d), 7, d, 0.4);
            fillRandom(palette[findPal("O2")], 240, 1, 7 * d + 3, 0, S.Lx - 1, S.Ly - 1, 0, 0.4);
        }
        P.Tset = 0.6; P.tauT = 1.0; P.wallAttr = 0.8; P.eaScale = 0.3; colorMode = 0;
        presetTitle = "Shift+2 · Окисление железа: Fe + O2 → оксид; окисленные атомы теряют металлическую связь (ржавчина отслаивается)"; break; }
    case 13: {  // натрий в хлоре
        int Na = typeOfZ(11); double d = 2 * EL[Na].rmet / 3.405;
        if (d3) {
            worldReset(26, 26, 26, B_WALLS);
            double a = d * std::sqrt(2.0); int m = 5; double c = m * a;
            lattice3D(L_FCC, Na, -1, 0, S.Lx / 2 - c / 2, 1.0, S.Lz / 2 - c / 2, m, m, m, a, 0.3);
            fillRandom(palette[findPal("Cl2")], 420, 1, c + 3, 1, S.Lx - 1, S.Ly - 1, S.Lz - 1, 1.4);
        } else {
            worldReset(64, 64 * asp, 1, B_WALLS);
            hexLattice2D(Na, S.Lx / 2 - 9 * d, 0.5, 18, 12, d, 0.3);
            fillRandom(palette[findPal("Cl2")], 240, 1, 12 * d + 3, 0, S.Lx - 1, S.Ly - 1, 0, 1.4);
        }
        P.Tset = 1.4; P.tauT = 1.0; colorMode = 0;
        presetTitle = "Shift+3 · Натрий горит в хлоре: 2Na + Cl2 → 2NaCl, ионные пары собираются в кристаллики соли"; break; }
    case 14: {  // горение метана
        if (d3) worldReset(26, 26, 26, B_WALLS); else worldReset(70, 70 * asp, 1, B_WALLS);
        P.Tset = 2.4; P.thermostat = TH_BUSSI; P.tauT = 2.0; colorMode = 0;   // слабая связь с «баней»: пламя не гаснет
        fillBox(palette[findPal("CH4")], 170, P.Tset); fillBox(palette[findPal("O2")], 340, P.Tset);
        script.push_back({1.5, 4, false});
        presetTitle = "Shift+4 · Горение метана: CH4 + 2O2 → CO2 + 2H2O (искра через 1.5τ; L — ещё вспышка)"; break; }
    case 15: {  // электрофорез: ионы в воде в электрическом поле
        int nion;
        if (d3) { worldReset(11, 11, 11, B_PERIODIC); nion = 10; P.substeps = 3; }
        else { worldReset(44, 44 * asp, 1, B_PERIODIC); nion = 14; P.substeps = 5; }
        fillBox(palette[findPal("Na+")], nion, 0.7, 0.0); fillBox(palette[findPal("Cl-")], nion, 0.7, 0.0);
        fillBox(palette[findPal("H2O")], (int)((d3 ? 0.55 : 0.64) * boxVolume()), 0.7, 0.0, 0.9);
        P.Tset = 0.7; P.tauT = 1.0; colorMode = 0;
        script.push_back({2.0, 6, false});
        presetTitle = "Shift+5 · Электрофорез: поле E гонит Na+ по полю, Cl- — против; вода поляризуется, ток I — в верхней строке"; break; }
    case 16: {  // кислота в воде: HCl ионизуется, протон бегает по воде
        waterBox(10, 40);
        const int na = d3 ? 8 : 12;
        fillBox(libTmpl(ML_HCL), na, 0.7, 0.0, 0.9);
        fillBox(palette[findPal("H2O")], waterCount() - 2 * na, 0.7, 0.0, 0.9);
        P.Tset = 0.7; P.tauT = 1.0; colorMode = 0;
        presetTitle = "Кислота в воде: HCl + H2O → H3O+ + Cl−, протон переходит по цепочке водородных связей (Гроттгус); pH — вкладка «Химия»"; break; }
    case 17: {  // нейтрализация / титрование
        waterBox(10.5, 42);
        const int ni = d3 ? 6 : 8;
        const double hx = S.Lx / 2, zt = d3 ? S.Lz : 0;
        (void)hx; (void)zt;
        if (presetVariant == 0) {   // растворы HCl и NaOH только что слиты: ионы перемешаны
            fillBox(libTmpl(ML_H3O), ni, 0.7, 0.0, 0.9); fillBox(palette[findPal("Cl-")], ni, 0.7, 0.0, 0.9);
            fillBox(palette[findPal("Na+")], ni, 0.7, 0.0, 0.9); fillBox(libTmpl(ML_OH), ni, 0.7, 0.0, 0.9);
            presetTitle = "Нейтрализация HCl + NaOH: H3O+ + OH− → 2H2O; теплота нейтрализации греет раствор, pH → 7. Повтор — титрование";
        } else {                    // титрование: раствор HCl, NaOH добавляется порциями
            fillBox(libTmpl(ML_H3O), ni, 0.7, 0.0, 0.9); fillBox(palette[findPal("Cl-")], ni, 0.7, 0.0, 0.9);
            for (int k = 1; k <= 2 * ni; k++) script.push_back({1.5 * k, 7, false});
            presetTitle = "Титрование: к раствору HCl каждые 1.5τ добавляется порция NaOH; pH скачком проходит 7 в точке эквивалентности";
        }
        fillBox(palette[findPal("H2O")], waterCount() - 3 * ni, 0.7, 0.0, 0.9);
        P.Tset = 0.7; P.thermostat = TH_BUSSI; P.tauT = 4.0; colorMode = 0;
        break; }
    case 18: {  // горение этанола
        if (d3) worldReset(26, 26, 26, B_WALLS); else worldReset(70, 70 * asp, 1, B_WALLS);
        P.Tset = 2.4; P.thermostat = TH_BUSSI; P.tauT = 2.0; colorMode = 0;
        const int ne = d3 ? 80 : 60;
        fillBox(libTmpl(ML_C2H5OH), ne, P.Tset); fillBox(palette[findPal("O2")], 3 * ne, P.Tset);
        script.push_back({1.5, 4, false});
        presetTitle = "Горение этанола: C2H5OH + 3O2 → 2CO2 + 3H2O (искра через 1.5τ; L — ещё вспышка)"; break; }
    case 19: {  // гремучая смесь в закрытом сосуде
        if (d3) worldReset(16, 16, 16, B_WALLS); else worldReset(40, 40 * asp, 1, B_WALLS);
        P.Tset = 1.0; P.thermostat = TH_NVE; colorMode = 0;
        const int nh = d3 ? 300 : 240;
        fillBox(palette[findPal("H2")], nh, P.Tset); fillBox(palette[findPal("O2")], nh / 2, P.Tset);
        script.push_back({1.0, 4, false});
        presetTitle = "Гремучая смесь в закрытом сосуде: 2H2 + O2 → 2H2O; искра через 1τ → взрыв: скачок T и давления (NVE)"; break; }
    case 20: {  // гетерогенный катализ: наночастица платины в смеси H2 + O2
        const int Pt = typeOfZ(78);
        if (d3) { worldReset(20, 20, 20, B_PERIODIC); metalBall(Pt, 10, 10, 10, 2.0, 0.8); }
        else { worldReset(56, 56 * asp, 1, B_PERIODIC); metalBall(Pt, S.Lx / 2, S.Ly / 2, 0, 4.0, 0.8); }
        const int nh = d3 ? 240 : 200;
        fillBox(palette[findPal("H2")], nh, 0.9, 0.0); fillBox(palette[findPal("O2")], nh / 2, 0.9, 0.0);
        P.Tset = 0.9; P.thermostat = TH_BUSSI; P.tauT = 1.0; colorMode = 0; P.substeps = d3 ? 5 : 8;
        presetTitle = "Катализ на платине: без искры смесь H2 + O2 инертна, но на поверхности наночастицы Pt идёт 2H2 + O2 → 2H2O"; break; }
    case 21: {  // теплопроводность: левая стенка горячая, правая холодная → стационарный линейный профиль T(x)
        if (d3) { worldReset(34, 13, 13, B_WALLS); P.Thot = 3.0; P.Tcold = 1.2; }
        else { worldReset(50, 50 * asp, 1, B_WALLS); P.Thot = 1.6; P.Tcold = 0.6; }
        P.heatWalls = 1; P.thermostat = TH_NVE; P.Tset = 0.5 * (P.Thot + P.Tcold); P.wallAttr = 0;
        fillBox(palette[findPal("Ar")], (int)((d3 ? 0.40 : 0.50) * boxVolume()), P.Tset, 0.8);
        colorMode = 1;
        presetTitle = "Теплопроводность: левая стенка горячая, правая холодная → линейный профиль T(x), поток тепла q = −κ·dT/dx (закон Фурье)"; break; }
    case 22: {  // ударная труба / расширение Джоуля: перегородка между плотным и разреженным (или пустым) объёмом
        const Tmpl& ar = palette[findPal("Ar")];
        double xm, rhoL, rhoR, TR;
        if (presetVariant == 0) {   // классическая ударная труба: горячий плотный «толкающий» газ и холодный разреженный
            if (d3) worldReset(90, 14, 14, B_WALLS); else worldReset(100, 28, 1, B_WALLS);
            xm = 0.3 * S.Lx; rhoL = d3 ? 0.12 : 0.30; rhoR = d3 ? 0.025 : 0.07; P.Tset = d3 ? 4.0 : 2.5; TR = d3 ? 1.2 : 0.6;
            presetTitle = "Ударная труба: через 1τ мембрана лопнет → горячий плотный газ гонит ударную волну (скачок плотности и T) по холодному разреженному, назад бежит волна разрежения. Повтор — расширение в вакуум";
        } else {
            if (d3) worldReset(36, 18, 18, B_WALLS); else worldReset(70, 70 * asp, 1, B_WALLS);
            xm = 0.5 * S.Lx; rhoL = d3 ? 0.30 : 0.35; rhoR = 0; P.Tset = d3 ? 1.5 : 1.0; TR = P.Tset;
            presetTitle = "Расширение Джоуля: через 1τ плотный газ вырвется в вакуум; энергия сохраняется, но газ остывает — атомы работают против взаимного притяжения";
        }
        const double A = d3 ? (S.Ly - 1.6) * (S.Lz - 1.6) : S.Ly - 1.6;
        partition(xm);
        fillRandom(ar, (int)(rhoL * (xm - 1.7) * A), 0.8, 0.8, 0.8, xm - 0.9, S.Ly - 0.8, S.Lz - 0.8, P.Tset);
        if (rhoR > 0) fillRandom(ar, (int)(rhoR * (S.Lx - xm - 1.7) * A), xm + 0.9, 0.8, 0.8, S.Lx - 0.8, S.Ly - 0.8, S.Lz - 0.8, TR);
        P.thermostat = TH_NVE; P.wallAttr = 0; colorMode = 1;
        script.push_back({1.0, 20, false}); break; }
    case 23: {  // барометрическая формула: смесь Ar и Ne в поле тяжести
        if (d3) worldReset(16, 30, 16, B_WALLS); else worldReset(64, 64 * asp, 1, B_WALLS);
        const double T = 1.5, H = d3 ? 7.0 : 0.24 * S.Ly;   // H — высота однородной атмосферы аргона kT/(mg)
        P.gravity = T / (EL[E_AR].m * H); P.Tset = T; P.thermostat = TH_BUSSI; P.tauT = 1.0; P.wallAttr = 0;
        const int n = d3 ? 300 : 90;
        fillBox(palette[findPal("Ar")], n, T, 0.8); fillBox(palette[findPal("Ne")], n, T, 0.8);
        colorMode = 0; P.substeps = d3 ? 10 : 14;
        presetTitle = "Барометрическая формула: в поле тяжести плотность газа падает как exp(−mgh/kT) — тяжёлый Ar прижат ко дну, лёгкий Ne поднимается выше (ρ(y) — «Графики»)"; break; }
    case 24: {  // эффузия через пористую перегородку: смесь He + Ar, справа вакуум
        if (d3) worldReset(40, 20, 20, B_WALLS); else worldReset(64, 64 * asp, 1, B_WALLS);
        const double xm = 0.5 * S.Lx;
        porousMembrane(xm, d3 ? 4.0 : 4.5, d3 ? 4 : 5);
        const int n = d3 ? 170 : 110, He = typeOfZ(2);
        P.Tset = 1.2; P.thermostat = TH_BUSSI; P.tauT = 2.0; P.wallAttr = 0;
        fillRandom(atomTmpl(He), n, 0.8, 0.8, 0.8, xm - 1.2, S.Ly - 0.8, S.Lz - 0.8, P.Tset);
        fillRandom(palette[findPal("Ar")], n, 0.8, 0.8, 0.8, xm - 1.2, S.Ly - 0.8, S.Lz - 0.8, P.Tset);
        colorMode = 0; P.substeps = d3 ? 10 : 12;
        presetTitle = "Эффузия (закон Грэма): смесь He и Ar утекает через узкие щели в вакуум — лёгкий гелий быстрее в √(mAr/mHe) ≈ 3.2 раза"; break; }
    case 25: {  // спекание: наночастицы золота и серебра касаются и срастаются ниже температуры плавления
        const int Au = typeOfZ(79), Ag = typeOfZ(47);
        const double d = 2 * EL[Au].rmet / 3.405, R = d3 ? 3.4 : 7.0, T0 = d3 ? 0.30 : 0.45;   // в 3D частица плавится уже при ≈0.45
        if (d3) worldReset(4 * R + 12, 2 * R + 10, 2 * R + 10, B_PERIODIC); else worldReset(4 * R + 16, std::max((4 * R + 16) * asp, 2 * R + 10), 1, B_PERIODIC);
        const double cx = S.Lx / 2, cy = S.Ly / 2, cz = d3 ? S.Lz / 2 : 0, h = R + 0.5 * d;
        metalBall(Au, cx - h, cy, cz, R, T0); metalBall(Ag, cx + h, cy, cz, R, T0);
        P.Tset = T0; P.thermostat = TH_BUSSI; P.tauT = 1.0; colorMode = 0; P.substeps = 10;
        presetTitle = "Спекание: наночастицы Au и Ag коснулись ниже температуры плавления — растёт перешеек, частицы сближаются, атомы перемешиваются (твёрдое состояние!)"; break; }
    case 26: {  // адиабатическое сжатие: давление на поршень растёт ступенями, газ теплоизолирован (NVE)
        // низкий широкий сосуд и лёгкий гелий: звук пересекает газ за ~5τ, а сжатие идёт 75τ — процесс квазистатический
        // (при быстром сжатии поршень гонит ударные волны, и газ греется сильнее адиабаты)
        const int n = d3 ? 500 : 220, He = typeOfZ(2);
        if (d3) worldReset(30, 12, 30, B_PISTON); else worldReset(64, 20, 1, B_PISTON);
        P.Tset = 1.5; P.thermostat = TH_NVE; P.wallAttr = 0; S.pistonM = 10; colorMode = 1;
        fillBox(atomTmpl(He), n, P.Tset, 0.8);
        P.pExt = n * P.Tset / boxVolume();   // давление идеального газа: поршень сначала в равновесии
        for (int s = 0; s < 150; s++) script.push_back({2.0 + 0.5 * s, 21, false});   // ×1.018 каждые 0.5τ → ×14.5 за 75τ
        presetTitle = "Адиабатическое сжатие гелия: с 2τ давление на поршень плавно растёт в 14 раз за 75τ; газ без теплообмена нагревается, TV^(γ−1) ≈ const"; break; }
    case 27: {  // смачивание: капля жидкости на притягивающей / отталкивающей стенке
        const bool wet = presetVariant == 0;
        if (d3) {
            worldReset(30, 22, 30, B_WALLS);
            const double a = 1.6, cx = 8 * a, cz = 8 * a;
            lattice3D(L_FCC, E_AR, -1, 0, (S.Lx - cx) / 2, 0.46, (S.Lz - cz) / 2, 8, 4, 8, a, 0.75);
            P.Tset = 0.75;
        } else {
            worldReset(70, 70 * asp, 1, B_WALLS);
            const double a = 1.12; hexLattice2D(E_AR, S.Lx / 2 - 13 * a, 0.3, 26, 12, a, 0.55);
            P.Tset = 0.55;
        }
        P.thermostat = TH_BERENDSEN; P.tauT = 1.0; P.gravity = 0.004; P.wallAttr = wet ? 1.6 : 0.1; colorMode = 0;
        presetTitle = wet ? "Смачивание: стенка притягивает атомы сильнее, чем они друг друга → капля растекается, угол смачивания < 90°. Повтор — несмачивание"
                          : "Несмачивание: стенка почти не притягивает → капля собирается в шар, угол смачивания > 90° (как ртуть на стекле). Повтор — смачивание";
        break; }
    case 28: {  // кристаллизация переохлаждённой жидкости на закреплённой затравке
        double R;
        if (d3) {
            const double a = std::cbrt(4.0 / 0.95); const int m = 8;
            worldReset(m * a, m * a, m * a, B_PERIODIC);
            lattice3D(L_FCC, E_AR, -1, 0, -0.25 * a, -0.25 * a, -0.25 * a, m, m, m, a, 1.8);
            P.Tset = 0.62; R = 2.3;
        } else {
            const double a = std::sqrt(2.0 / (std::sqrt(3.0) * 0.85)); const int nx = 40, ny = 46;
            worldReset(nx * a, ny * a * std::sqrt(3.0) / 2, 1, B_PERIODIC);
            hexLattice2D(E_AR, 0, 0, nx, ny, a, 1.6);
            P.Tset = 0.42; R = 4.5;
        }
        // затравка — шар (круг) решётки в центре: закреплена, пока жидкость вокруг плавится и остывает
        S.pin.resize(S.n, 0);
        for (int i = 0; i < S.n; i++) {
            const double dx = S.x[i] - S.Lx / 2, dy = S.y[i] - S.Ly / 2, dz = d3 ? S.z[i] - S.Lz / 2 : 0;
            if (dx * dx + dy * dy + dz * dz < R * R) { S.pin[i] = 1; S.vx[i] = S.vy[i] = S.vz[i] = 0; }
        }
        P.thermostat = TH_BUSSI; P.tauT = 1.5; colorMode = 3; if (d3) P.substeps = 5;
        presetTitle = "Кристаллизация на затравке: переохлаждённая жидкость нарастает слоями на закреплённом кристаллике (цвет — упорядоченность)"; break; }
    case 29: {  // течение Пуазейля: жидкость в канале между стенками, однородная сила вдоль x
        if (d3) worldReset(20, 14, 12, B_PERIODIC); else worldReset(60, 24, 1, B_PERIODIC);
        for (double x = 0.5; x < S.Lx - 0.2; x += d3 ? 0.9 : 1.0) {   // стенка канала — ряд (плоскость) неподвижных атомов при y ≈ 0.45 (в 2D шаг 1σ — шероховатая, жидкость не скользит)
            if (d3) for (double z = 0.45; z < S.Lz; z += 0.9) addAtom(E_WALL, x, 0.45, z, 0, 0, 0);
            else addAtom(E_WALL, x, 0.45, 0, 0, 0, 0);
        }
        P.Tset = 1.0; P.thermostat = TH_BUSSI; P.tauT = 4.0; P.wallAttr = 0; colorMode = 1;
        fillBox(palette[findPal("Ar")], (int)((d3 ? 0.55 : 0.68) * boxVolume()), P.Tset, 0.0);
        FieldObj w = makeFieldObj(FO_WIND, S.Lx / 2, S.Ly / 2, S.Lz / 2); w.R = 1000; w.strength = d3 ? 0.03 : 0.025; w.ramp = 1.0;
        fieldObjs.push_back(w);   // «ветер» радиусом 1000σ — однородная сила по всему каналу (как перепад давления)
        presetTitle = "Течение Пуазейля: сила гонит жидкость вдоль канала, у стенок она прилипает → профиль скорости — парабола (вязкость)"; break; }
    case 0: {   // химическое равновесие Cl2 ⇌ 2Cl и принцип Ле Шателье
        if (d3) { worldReset(24, 24, 24, B_PISTON); P.pExt = 0.2; fillBox(palette[findPal("Cl2")], 700, 2.6); }
        else { worldReset(60, 60 * asp, 1, B_PISTON); P.pExt = 0.15; fillBox(palette[findPal("Cl2")], 500, 2.6); }
        P.Tset = 2.6; S.pistonM = 60; colorMode = 0;
        presetTitle = "0 · Равновесие Cl2 <=> 2Cl: меняйте T и давление поршня — сдвиг по Ле Шателье"; break; }
    // ---- химические сцены (только из меню): 31–39
    case 31: {  // хлорирование метана на свету: УФ поглощает только Cl2 → радикальная цепь
        if (d3) worldReset(26, 26, 26, B_WALLS); else worldReset(70, 70 * asp, 1, B_WALLS);
        P.Tset = 1.5; P.thermostat = TH_BUSSI; P.tauT = 2.0; colorMode = 0;
        const int nm = d3 ? 200 : 160;
        fillBox(palette[findPal("CH4")], nm, P.Tset); fillBox(palette[findPal("Cl2")], nm, P.Tset);
        for (int k = 0; k < 16; k++) script.push_back({1.0 + 4.0 * k, 30, false});   // УФ-вспышки каждые 4τ
        presetTitle = "Хлорирование метана на свету: CH4 + Cl2 → CH3Cl + HCl; УФ-вспышки рвут только Cl2, дальше идёт радикальная цепь"; break; }
    case 32: {  // равновесие H2 + I2 ⇌ 2HI (Боденштейн)
        const int I = typeOfZ(53);
        if (d3) worldReset(24, 24, 24, B_WALLS); else worldReset(60, 60 * asp, 1, B_WALLS);
        P.Tset = 2.5; P.thermostat = TH_BUSSI; P.tauT = 1.0; colorMode = 0;
        const int nm = d3 ? 250 : 180;
        fillBox(palette[findPal("H2")], nm, P.Tset); fillBox(diTmpl("I2", I, I, 1), nm, P.Tset);
        presetTitle = "Равновесие H2 + I2 <=> 2HI (Боденштейн): прямая и обратная реакции идут одновременно, K_c — вкладка «Химия»"; break; }
    case 33: {  // гидрирование этилена на никеле
        const int Ni = typeOfZ(28);
        if (d3) { worldReset(20, 20, 20, B_PERIODIC); metalBall(Ni, 10, 10, 10, 2.2, 0.8); }
        else { worldReset(50, 50 * asp, 1, B_PERIODIC); metalBall(Ni, S.Lx / 2, S.Ly / 2, 0, 4.5, 0.8); }
        P.Tset = 1.5; P.thermostat = TH_BUSSI; P.tauT = 1.0; colorMode = 0; P.substeps = d3 ? 5 : 8;
        const int n = d3 ? 120 : 90;
        fillBox(libTmpl(ML_C2H4), n, P.Tset, 0.0); fillBox(palette[findPal("H2")], 2 * n, P.Tset, 0.0);
        presetTitle = "Гидрирование на никеле: C2H4 + H2 → C2H6; H2 распадается на поверхности Ni, атомы H присоединяются к этилену"; break; }
    case 34: {  // разложение пероксида водорода на платине
        const int Pt = typeOfZ(78);
        if (d3) { worldReset(20, 20, 20, B_PERIODIC); metalBall(Pt, 10, 10, 10, 2.2, 0.8); }
        else { worldReset(50, 50 * asp, 1, B_PERIODIC); metalBall(Pt, S.Lx / 2, S.Ly / 2, 0, 4.5, 0.8); }
        P.Tset = 1.2; P.thermostat = TH_BUSSI; P.tauT = 1.0; colorMode = 0; P.substeps = d3 ? 5 : 8;
        fillBox(libTmpl(ML_H2O2), d3 ? 150 : 110, P.Tset, 0.0);
        presetTitle = "Разложение пероксида: 2H2O2 → 2H2O + O2; слабая связь O–O рвётся, радикалы OH ведут цепь, Pt связывает OH"; break; }
    case 35: {  // горение ацетилена
        if (d3) worldReset(26, 26, 26, B_WALLS); else worldReset(70, 70 * asp, 1, B_WALLS);
        P.Tset = 2.4; P.thermostat = TH_BUSSI; P.tauT = 2.0; colorMode = 0;
        const int na = d3 ? 100 : 80;
        fillBox(libTmpl(ML_C2H2), na, P.Tset); fillBox(palette[findPal("O2")], na * 5 / 2, P.Tset);
        script.push_back({1.5, 4, false});
        presetTitle = "Горение ацетилена: 2C2H2 + 5O2 → 4CO2 + 2H2O (искра через 1.5τ; L — ещё вспышка)"; break; }
    case 36: {  // H2 + Br2 на свету (Боденштейн–Линд): стадия Br + H2 эндотермична — цепь медленнее хлорной
        if (d3) worldReset(24, 24, 24, B_WALLS); else worldReset(64, 64 * asp, 1, B_WALLS);
        P.Tset = 1.6; P.thermostat = TH_BUSSI; P.tauT = 2.0; colorMode = 0;
        const int nm = d3 ? 220 : 170;
        fillBox(palette[findPal("H2")], nm, P.Tset); fillBox(libTmpl(ML_BR2), nm, P.Tset);
        for (int k = 0; k < 16; k++) script.push_back({1.0 + 3.0 * k, 31, false});   // вспышки видимого света каждые 3τ
        presetTitle = "H2 + Br2 → 2HBr на свету: вспышки рвут Br2, но стадия Br + H2 → HBr + H эндотермична — цепь идёт медленнее, чем с хлором"; break; }
    case 37: {  // хлор вытесняет бром: Cl + HBr → HCl + Br (экзотермично), Br + Br → Br2
        if (d3) worldReset(24, 24, 24, B_WALLS); else worldReset(64, 64 * asp, 1, B_WALLS);
        P.Tset = 1.4; P.thermostat = TH_BUSSI; P.tauT = 2.0; colorMode = 0;
        const int nm = d3 ? 240 : 180;
        fillBox(libTmpl(ML_HBR), nm, P.Tset); fillBox(palette[findPal("Cl2")], nm / 2, P.Tset);
        for (int k = 0; k < 12; k++) script.push_back({1.0 + 4.0 * k, 30, false});
        presetTitle = "Хлор вытесняет бром: Cl + HBr → HCl + Br — связь H–Cl прочнее H–Br; бром собирается в Br2 и BrCl (УФ-вспышки каждые 4τ)"; break; }
    case 38: {  // водород и фтор: связь F–F слабая, H–F — самая прочная; реакция идёт без искры
        if (d3) worldReset(24, 24, 24, B_WALLS); else worldReset(64, 64 * asp, 1, B_WALLS);
        P.Tset = 2.2; P.thermostat = TH_BUSSI; P.tauT = 2.0; P.eaScale = 0.3; colorMode = 0;
        const int nm = d3 ? 200 : 150;
        fillBox(palette[findPal("H2")], nm, P.Tset); fillBox(libTmpl(ML_F2), nm, P.Tset);
        presetTitle = "H2 + F2 → 2HF без искры: при комнатной температуре связь F–F (1.6 эВ) рвётся сама; цепь F + H2 → HF + H, H + F2 → HF + F (H–F — 5.9 эВ)"; break; }
    case 39: {  // распад озона при нагреве
        if (d3) worldReset(22, 22, 22, B_WALLS); else worldReset(56, 56 * asp, 1, B_WALLS);
        P.Tset = 1.0; P.thermostat = TH_POWER; P.heatPower = 0.12; colorMode = 0;
        fillBox(libTmpl(ML_O3), d3 ? 260 : 200, P.Tset);
        presetTitle = "Распад озона при нагреве: O3 → O2 + O, слабая связь O–O⁻ рвётся первой; атомы O соединяются в O2 или отнимают O у озона"; break; }
    }
    for (auto& o : fieldObjs) o.fromPreset = true;   // объекты сцены (сброс пересоздаёт их, объекты пользователя переносятся)
    sceneMeasureReset();
    finishPreset();
    presetLoaded = false; showToast(std::string(T(presetTitle)) + (d3 ? "   [3D]" : ""));
}
// Загрузить сцену; keepUser — перенести объекты поля, поставленные пользователем (сброс сцены, повтор клавиши):
// объекты самого пресета создаются заново, объекты пользователя включаются плавно (ramp с нуля — атомы уже на новых местах)
static void loadPresetKeepObjs(int k, int variant, bool keepUser) {
    std::vector<FieldObj> user;
    if (keepUser) for (const FieldObj& o : fieldObjs) if (!o.fromPreset) { user.push_back(o); user.back().ramp = 0; user.back().acc = 0; }
    loadPreset(k, variant);
    if (user.empty()) return;
    fieldObjs.insert(fieldObjs.end(), user.begin(), user.end());
    computeForces(); resetEnergyRef();
}
// заменить случайную молекулу воды частицей по шаблону m (вставка в плотный раствор: место освобождает вода)
static bool replaceWaterWith(const Tmpl& m) {
    for (int tr = 0; tr < 40; tr++) {
        int i = (int)(urand() * S.n); if (i >= S.n || !isWaterO(i)) continue;
        double x = S.x[i], y = S.y[i], z = S.z[i];
        removeMolecule(i);
        if (placeMol(m, x, y, z, P.Tset, 0.6)) return true;
    }
    return false;
}
static void runScript() {
    for (auto& e : script) {
        if (e.done || S.t < e.t) continue;
        e.done = true;
        switch (e.action) {
        case 1: P.thermostat = TH_NVE; P.heatWalls = 2; resetEnergyRef(); showToast("Дно нагревается → кипение и испарение"); break;
        case 2: P.Tset = DIM == 3 ? 0.75 : 0.35; P.tauT = 3.0; showToast("Охлаждение → пар пересыщен → конденсация"); break;
        case 3: for (int i = S.n - 1; i >= 0; i--) if (S.ty[i] == E_WALL) removeAtom(i);
                updatePresence(); computeForces(); resetEnergyRef(); resetMSD(); showToast("Перегородка убрана"); break;
        case 4: { double c[3] = {S.Lx / 2, S.Ly / 2, S.Lz / 2}; lightFlash(c, nullptr, 5.0); showToast("Искра! Энергия вспышки учтена как внешняя работа"); break; }
        case 5: P.Tset = DIM == 3 ? 0.1 : 0.05; P.tauT = 0.08; showToast("Закалка: T → 0.05–0.1 за доли τ"); break;
        case 6: P.efield = 1.5; showToast("Включено поле E = 1.5 → катионы дрейфуют по полю, анионы — против"); break;
        case 7: {   // титрование: порция NaOH (Na+ и OH− на место двух молекул воды)
            bool ok = replaceWaterWith(palette[findPal("Na+")]); ok = replaceWaterWith(libTmpl(ML_OH)) && ok;
            updatePresence(); computeForces(); resetEnergyRef();
            showToast(ok ? "Добавлена порция NaOH (Na+ + OH−)" : "Не удалось добавить NaOH");
            break; }
        case 20: {  // ударная труба / расширение Джоуля: мембрана лопается
            double xw = 0, kl = 0, kr = 0; int nw = 0, nl = 0, nr = 0;
            for (int i = 0; i < S.n; i++) if (S.ty[i] == E_WALL) { xw += S.x[i]; nw++; }
            if (nw == 0) break;
            xw /= nw;
            for (int i = 0; i < S.n; i++) if (!EL[S.ty[i]].fixed) {
                const double k2 = EL[S.ty[i]].m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]);
                if (S.x[i] < xw) { nl++; kl += k2; } else { nr++; kr += k2; }
            }
            const double T4 = nl ? kl / (DIM * nl) : 1, T1 = nr ? kr / (DIM * nr) : 1;   // температуры до разрыва
            measure();
            SM::tOpen = S.t; SM::xOpen = xw; SM::T0 = EN.T; SM::Ep0 = EN.nmob ? potentialNow() / EN.nmob : 0; SM::front.clear(); SM::speedW = 0;
            for (int i = S.n - 1; i >= 0; i--) if (S.ty[i] == E_WALL) removeAtom(i);
            updatePresence(); computeForces(); resetEnergyRef(); resetMSD();
            // теория ударной трубы для идеального газа (один газ, γ = (d+2)/d, скорости звука a = √(γkT/m)):
            //   p4/p1 = p21·[1 − (γ−1)(a1/a4)(p21 − 1)/√(2γ(2γ + (γ+1)(p21 − 1)))]^(−2γ/(γ−1)),  Ms = √(1 + (γ+1)(p21 − 1)/(2γ))
            const double g = (DIM + 2.0) / DIM, crossV = DIM == 3 ? (S.Ly - 1.6) * (S.Lz - 1.6) : S.Ly - 1.6;
            SM::rhoR0 = nr / std::max(1e-9, (S.Lx - xw - 1.7) * crossV); SM::T1 = T1;
            if (nr > 0 && nl > nr) {
                const double p41 = (nl * T4 / std::max(1e-9, xw - 1.7)) / (nr * T1 / std::max(1e-9, S.Lx - xw - 1.7)), a14 = std::sqrt(T1 / T4);
                auto f = [&](double p) { return p * std::pow(std::max(1e-9, 1 - (g - 1) * a14 * (p - 1) / std::sqrt(2 * g * (2 * g + (g + 1) * (p - 1)))), -2 * g / (g - 1)) - p41; };
                double lo = 1, hi = p41;
                for (int it = 0; it < 80; it++) { double mid = 0.5 * (lo + hi); if (f(mid) > 0) hi = mid; else lo = mid; }
                const double p21 = 0.5 * (lo + hi), Ms = std::sqrt(1 + (g + 1) * (p21 - 1) / (2 * g));
                SM::theoW = Ms * std::sqrt(g * T1 / EL[E_AR].m);
                SM::rho21 = ((g + 1) * p21 + (g - 1)) / ((g - 1) * p21 + (g + 1));
                showToast("Мембрана лопнула → ударная волна вправо, волна разрежения влево");
            } else showToast("Перегородка исчезла → газ расширяется в пустоту");
            break; }
        case 21: {  // адиабатическое сжатие: ступень давления на поршень (первая — запомнить начальное состояние)
            measure();
            if (SM::adV0 <= 0) { SM::adV0 = boxVolume(); SM::adT0 = EN.T; }
            P.pExt *= 1.018; resetEnergyRef(); break; }
        case 30: { int c = photolyze(E_CL, E_CL, 0.15); if (c) showToast("УФ-вспышка: Cl2 → 2Cl·"); break; }
        case 31: { const int Br = typeOfZ(35); int c = photolyze(Br, Br, 0.15); if (c) showToast("Вспышка света: Br2 → 2Br·"); break; }
        }
    }
}
// теплопроводность, ударная волна, барометрическая формула, эффузия, спекание, сжатие — живые измерения
static void sceneMeasure() {
    const int k = currentPreset;
    if (k < 21 || k > 29) return;
    if (SM::tPrev >= 0 && S.t < SM::tPrev - 1e-9) sceneMeasureReset();   // время пошло назад (отмена, загрузка)
    const double dt = SM::tPrev < 0 ? 0 : S.t - SM::tPrev; SM::tPrev = S.t;
    const int B = A::TP_BINS; const bool d3 = DIM == 3;
    std::string& N = A::sceneNote;
    auto fitLine = [](const std::vector<std::pair<double, double>>& p, double& a, double& b) {   // y = a + b·x (МНК)
        double sx = 0, sy = 0, sxx = 0, sxy = 0; int n = (int)p.size(); if (n < 2) return false;
        for (auto& q : p) { sx += q.first; sy += q.second; sxx += q.first * q.first; sxy += q.first * q.second; }
        double den = n * sxx - sx * sx; if (std::fabs(den) < 1e-12) return false;
        b = (n * sxy - sx * sy) / den; a = (sy - b * sx) / n; return true;
    };
    switch (k) {
    case 21: {   // поток тепла от стенок и градиент в средней части: κ = q/|dT/dx|
        if (!SM::qInit || dt <= 0) { SM::q0 = heatWallQ[0]; SM::q1 = heatWallQ[1]; SM::qh = SM::qc = 0; SM::qInit = true; N = "поток тепла: измерение…"; break; }
        const double a = std::min(1.0, dt / 8.0);
        SM::qh += a * ((heatWallQ[0] - SM::q0) / dt - SM::qh); SM::qc += a * ((heatWallQ[1] - SM::q1) / dt - SM::qc);
        SM::q0 = heatWallQ[0]; SM::q1 = heatWallQ[1];
        const double area = d3 ? S.Ly * S.Lz : S.Ly, q = 0.5 * (SM::qh - SM::qc) / area;
        std::vector<std::pair<double, double>> pts;
        for (int b = B / 5; b < B - B / 5; b++) pts.push_back({(b + 0.5) * S.Lx / B, A::tprof[b]});
        double a0, sl; if (!fitLine(pts, a0, sl)) break;
        const double kap = sl < -1e-6 ? q / -sl : 0;
        N = fmt("стенки: приток %.2f, отток %.2f ε/τ · q = %.4f ε/(τσ%s) · dT/dx = %.4f /σ · κ = q/|dT/dx| = %.2f k/(στ) ≈ %.3f Вт/(м·К)",
                SM::qh, -SM::qc, q, d3 ? "²" : "", sl, kap, kap * 0.0406);
        break; }
    case 22: {
        if (SM::tOpen < 0) { N = presetVariant == 0 ? "слева газ плотнее и давление выше; мембрана лопнет через 1τ" : "слева плотный газ, справа вакуум; перегородка исчезнет через 1τ"; break; }
        if (presetVariant == 1) {   // расширение Джоуля: E = const, T падает, потенциальная энергия растёт
            const double ep = EN.nmob ? potentialNow() / EN.nmob : 0;
            N = fmt("T: %.3f → %.3f (%.0f → %.0f K) · Eпот на атом: %.3f → %.3f ε · полная E сохраняется — идеальный газ не остыл бы",
                    SM::T0, EN.T, toKelvin(SM::T0), toKelvin(EN.T), SM::Ep0, ep);
            break;
        }
        // фронт ударной волны: самая правая точка, где плотность пересекает середину скачка
        const double bw = S.Lx / B, thr = SM::rhoR0 * (1 + 0.5 * (SM::rho21 - 1));
        std::vector<double> rs(B);   // сглаживание по трём слоям — шум редкого газа не выдаётся за фронт
        for (int b = 0; b < B; b++) rs[b] = (A::dprof[0][std::max(0, b - 1)] + A::dprof[0][b] + A::dprof[0][std::min(B - 1, b + 1)]) / 3;
        double xs = -1;
        for (int b = B - 2; b >= 1; b--) if (rs[b] > thr && rs[b - 1] > thr) {
            double r0 = rs[b], r1 = rs[b + 1];
            xs = (b + 0.5 + clampv((r0 - thr) / std::max(1e-9, r0 - r1), 0.0, 1.0)) * bw; break;
        }
        if (xs > SM::xOpen + 4 && xs < S.Lx - 8 && S.t > SM::tOpen + 2 && (SM::front.empty() || xs >= SM::front.back().second - 2)) SM::front.push_back({S.t, xs});
        double a0, w; if (SM::front.size() >= 4 && SM::front.back().first - SM::front.front().first > 3 && fitLine(SM::front, a0, w)) SM::speedW = w;
        if (xs < 0) { N = "ударная волна: фронт ещё не сформировался"; break; }
        N = fmt("фронт x = %.0fσ · скорость %s · теория (идеальный газ): %.2f σ/τ, число Маха %.2f, сжатие в %.2f раза",
                xs, SM::speedW > 0 ? fmt("%.2f σ/τ (%.0f м/с)", SM::speedW, SM::speedW * cfg::U_L_NM * 1e3 / cfg::U_T_PS).c_str() : "…",
                SM::theoW, SM::theoW / std::sqrt((DIM + 2.0) / DIM * SM::T1 / EL[E_AR].m), SM::rho21);
        break; }
    case 23: {   // высота однородной атмосферы H = kT/(mg): теория и наклон ln ρ(y)
        if (P.gravity <= 0 || A::dprofN == 0) { N.clear(); break; }
        std::string s;
        const double aw = std::min(1.0, dt / 10.0);   // профили, усреднённые за ~10τ (одиночный снимок редкого газа шумит)
        for (int q = 0; q < A::dprofN; q++) {
            std::vector<double>& av = SM::avg[q];
            if ((int)av.size() != B) av = A::dprof[q]; else for (int b = 0; b < B; b++) av[b] += aw * (A::dprof[q][b] - av[b]);
            const int t = A::dprofType[q];
            // МНК для ln ρ(y) с весом ρ (слою с большим числом атомов — больший вес); крайние слои у стенок не берём
            double sw = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (int b = 1; b < B - 1; b++) if (av[b] > 1e-6) { double w = av[b], x = (b + 0.5) * S.Ly / B, y = std::log(av[b]); sw += w; sx += w * x; sy += w * y; sxx += w * x * x; sxy += w * x * y; }
            const double den = sw * sxx - sx * sx, sl = std::fabs(den) > 1e-12 ? (sw * sxy - sx * sy) / den : 0;
            const double Hth = P.Tset / (EL[t].m * P.gravity);
            s += fmt("%s%s: kT/mg = %.1fσ, по профилю %s", q ? " · " : "", EL[t].sym, Hth, sl < 0 && S.t > 5 ? fmt("%.1fσ", -1 / sl).c_str() : "…");
        }
        N = T("высота однородной атмосферы ") + s +fmt(" · T одинакова по высоте: %.2f", EN.T);
        break; }
    case 24: {   // доли каждого газа за перегородкой; для равных камер N_R/N = (1 − e^{−2kt})/2 → k ∝ −ln(1 − 2f)
        const double xm = 0.5 * S.Lx; const int He = typeOfZ(2);
        int nH = 0, nHr = 0, nA = 0, nAr = 0;
        for (int i = 0; i < S.n; i++) { if (S.ty[i] == He) { nH++; if (S.x[i] > xm) nHr++; } else if (S.ty[i] == E_AR) { nA++; if (S.x[i] > xm) nAr++; } }
        if (!nH || !nA) { N.clear(); break; }
        const double fH = (double)nHr / nH, fA = (double)nAr / nA;
        std::string r = "…";
        if (nAr >= 3 && fH < 0.45 && fA < 0.45) r = fmt("%.1f", std::log(1 - 2 * fH) / std::log(1 - 2 * fA));
        else if (fH >= 0.45) r = "камеры почти выровнялись";
        N = fmt("за перегородкой: He %d из %d (%.0f%%), Ar %d из %d (%.0f%%) · скорость эффузии He/Ar = %s · закон Грэма √(mAr/mHe) = %.2f",
                nHr, nH, 100 * fH, nAr, nA, 100 * fA, r.c_str(), std::sqrt(EL[E_AR].m / EL[He].m));
        break; }
    case 25: {   // центры масс частиц, перешеек в плоскости контакта, взаимная диффузия
        const int Au = typeOfZ(79), Ag = typeOfZ(47);
        double c[2][3] = {{0, 0, 0}, {0, 0, 0}}; int n[2] = {0, 0};
        for (int i = 0; i < S.n; i++) { int s = S.ty[i] == Au ? 0 : S.ty[i] == Ag ? 1 : -1; if (s < 0) continue; c[s][0] += S.x[i]; c[s][1] += S.y[i]; c[s][2] += S.z[i]; n[s]++; }
        if (!n[0] || !n[1]) { N.clear(); break; }
        for (int s = 0; s < 2; s++) for (int q = 0; q < 3; q++) c[s][q] /= n[s];
        const double xc = 0.5 * (c[0][0] + c[1][0]), yc = 0.5 * (c[0][1] + c[1][1]), zc = 0.5 * (c[0][2] + c[1][2]);
        std::vector<double> r; int mixA = 0, mixB = 0;
        for (int i = 0; i < S.n; i++) {
            int s = S.ty[i] == Au ? 0 : S.ty[i] == Ag ? 1 : -1; if (s < 0) continue;
            if (s == 0 && S.x[i] > xc) mixA++; if (s == 1 && S.x[i] < xc) mixB++;
            if (std::fabs(S.x[i] - xc) < 0.6) r.push_back(d3 ? std::hypot(S.y[i] - yc, S.z[i] - zc) : std::fabs(S.y[i] - yc));
        }
        std::sort(r.begin(), r.end());
        const double neck = r.size() >= 3 ? 2 * r[r.size() - 2] + 2 * EL[Au].rmet / 3.405 : 0;   // без самого дальнего (адатом)
        if (SM::V0 <= 0) SM::V0 = c[1][0] - c[0][0];   // начальное расстояние между центрами
        N = fmt("перешеек %.1fσ (%.1f нм) · центры сблизились на %.2fσ · Au в «чужой» половине %.0f%%, Ag %.0f%% · T = %.2f",
                neck, neck * cfg::U_L_NM, SM::V0 - (c[1][0] - c[0][0]), 100.0 * mixA / n[0], 100.0 * mixB / n[1], EN.T);
        break; }
    case 26: {   // адиабата идеального одноатомного газа: T·V^(γ−1) = const, γ = (d+2)/d
        if (SM::adV0 <= 0) { N = fmt("газ в равновесии с поршнем: V = %.0fσ%s, T = %.2f (%.0f K); с 2τ давление начнёт расти", boxVolume(), d3 ? "³" : "²", EN.T, toKelvin(EN.T)); break; }
        const double g = (DIM + 2.0) / DIM, V = boxVolume(), vr = SM::adV0 / std::max(1e-9, V);
        N = fmt("V0/V = %.2f · T/T0 = %.2f (T = %.0f K) · адиабата (V0/V)^(γ−1) = %.2f при γ = %.2f · P внешн. = %.3f",
                vr, EN.T / std::max(1e-9, SM::adT0), toKelvin(EN.T), std::pow(vr, g - 1), g, P.pExt);
        break; }
    case 27: {   // угол смачивания по форме капли-сегмента: tg(θ/2) = 2h/w
        std::vector<double> xs, ys; double ybot = 1e9;
        for (int i = 0; i < S.n; i++) if (!EL[S.ty[i]].fixed) { ys.push_back(S.y[i]); ybot = std::min(ybot, S.y[i]); }
        if (ys.size() < 20) { N.clear(); break; }
        for (int i = 0; i < S.n; i++) if (!EL[S.ty[i]].fixed && S.y[i] < ybot + 1.0) xs.push_back(S.x[i]);
        if (xs.size() < 3) { N.clear(); break; }
        std::sort(xs.begin(), xs.end()); std::sort(ys.begin(), ys.end());
        const double w = xs[(size_t)(0.97 * (xs.size() - 1))] - xs[(size_t)(0.03 * (xs.size() - 1))] + 1.0;
        const double h = ys[(size_t)(0.96 * (ys.size() - 1))] - ybot + 1.0;
        const double th = 2 * std::atan(2 * h / std::max(1e-9, w)) * 180 / PI;
        N = fmt("угол смачивания θ ≈ %.0f° (основание %.1fσ, высота %.1fσ) · притяжение стенки %.2f → %s", th, w, h, P.wallAttr,
                T(th < 90 ? "смачивает (θ < 90°)" : "не смачивает (θ > 90°)"));
        break; }
    case 28: {
        int np = 0; for (int i = 0; i < S.n && i < (int)S.pin.size(); i++) if (S.pin[i]) np++;
        N = fmt("доля кристалла %.0f%% · жидкость %.0f%% · затравка %d атомов (закреплена) · T = %.3f (%.0f K)",
                100 * phaseFrac[2], 100 * phaseFrac[1], np, EN.T, toKelvin(EN.T));
        break; }
    case 29: {   // профиль vx поперёк канала: у параболы средняя скорость = 2/3 максимальной
        const int NB = 12; double yw = 0; int nw = 0;
        for (int i = 0; i < S.n; i++) if (S.ty[i] == E_WALL) { yw += S.y[i]; nw++; }
        if (!nw) { N.clear(); break; }
        yw /= nw;
        std::vector<double> sv(NB, 0.0); std::vector<int> cn(NB, 0);
        for (int i = 0; i < S.n; i++) {
            if (EL[S.ty[i]].fixed) continue;
            double y = S.y[i] - yw; y -= S.Ly * std::floor(y / S.Ly);
            const int b = clampv((int)(y / S.Ly * NB), 0, NB - 1); sv[b] += S.vx[i]; cn[b]++;
        }
        if ((int)SM::vprof.size() != NB) SM::vprof.assign(NB, 0.0);
        const double a = std::min(1.0, dt / 5.0);
        double vmax = -1e9, vsum = 0;
        for (int b = 0; b < NB; b++) { const double v = cn[b] ? sv[b] / cn[b] : 0; SM::vprof[b] += a * (v - SM::vprof[b]); vmax = std::max(vmax, SM::vprof[b]); vsum += SM::vprof[b]; }
        const double vw = 0.5 * (SM::vprof[0] + SM::vprof[NB - 1]);
        N = fmt("скорость потока: в центре %.3f, у стенок %.3f σ/τ (%.0f и %.0f м/с) · средняя / максимальная = %.2f (парабола Пуазейля: 0.67)",
                vmax, vw, vmax * cfg::U_L_NM * 1e3 / cfg::U_T_PS, vw * cfg::U_L_NM * 1e3 / cfg::U_T_PS, vmax > 1e-6 ? vsum / NB / vmax : 0.0);
        break; }
    }
}


// ---- Список сцен для меню (Tab). Новые пресеты добавлять сюда: key = номер пресета для loadPreset.
// group — раздел меню: SG_MATTER — вещество (фазы, перенос, механика), SG_CHEM — химия и растворы.
enum { SG_MATTER, SG_CHEM, SG_N };
struct SceneInfo { int key, var; const char* keyLabel; const char* title; const char* desc; int group; };
static const SceneInfo SCENES[] = {
    {1, 0, "1", "Идеальный газ", "PV = NkT, распределение Максвелла, давление на стенки", SG_MATTER},
    {2, 0, "2", "Плавление кристалла", "нагрев постоянной мощностью: плато T(t); повтор 2 — другие решётки", SG_MATTER},
    {3, 0, "3", "Кипение", "жидкость под гравитацией, горячее дно, испарение с поверхности", SG_MATTER},
    {4, 0, "4", "Конденсация", "пересыщенный пар → зародыши → круглые капли", SG_MATTER},
    {5, 0, "5", "Диффузия", "смешивание двух газов: MSD(t) и коэффициент D", SG_MATTER},
    {6, 0, "6", "NaCl в воде", "растворение соли, гидратные оболочки ионов", SG_CHEM},
    {7, 0, "7", "Горение водорода", "2H2 + O2 → 2H2O от искры; повтор 7 — H2 + Cl2", SG_CHEM},
    {8, 0, "8", "Броуновское движение", "тяжёлая частица среди атомов, MSD ∝ t", SG_MATTER},
    {9, 0, "9", "Закалка", "поликристалл, границы зёрен; повтор 9 — стекло", SG_MATTER},
    {0, 0, "0", "Химическое равновесие", "Cl2 ⇌ 2Cl под поршнем: принцип Ле Шателье", SG_CHEM},
    {11, 0, "Shift+1", "Наночастица золота", "металлическая связь, плавление начинается с поверхности", SG_MATTER},
    {12, 0, "Shift+2", "Окисление железа", "Fe + O2 → оксид: окисленные атомы теряют металлическую связь", SG_CHEM},
    {13, 0, "Shift+3", "Натрий в хлоре", "2Na + Cl2 → 2NaCl: горение и кристаллики соли", SG_CHEM},
    {14, 0, "Shift+4", "Горение метана", "CH4 + 2O2 → CO2 + 2H2O: цепная реакция от искры", SG_CHEM},
    {15, 0, "Shift+5", "Электрофорез", "ионы в воде в электрическом поле, ток I", SG_CHEM},
    {16, 0, "меню", "Кислота в воде", "HCl + H2O → H3O+ + Cl−, прыжки протона по Гроттгусу, pH", SG_CHEM},
    {17, 0, "меню", "Нейтрализация", "H3O+ + OH− → 2H2O, теплота и pH; повтор — титрование", SG_CHEM},
    {18, 0, "меню", "Горение этанола", "C2H5OH + 3O2 → 2CO2 + 3H2O от искры", SG_CHEM},
    {19, 0, "меню", "Гремучая смесь", "взрыв 2H2 + O2 в закрытом сосуде: скачок T и P", SG_CHEM},
    {20, 0, "меню", "Катализ на платине", "H2 + O2 реагируют только на поверхности наночастицы Pt", SG_CHEM},
    {21, 0, "меню", "Теплопроводность", "горячая и холодная стенки: линейный профиль T(x), закон Фурье", SG_MATTER},
    {22, 0, "меню", "Ударная труба", "ударная волна и волна разрежения; повтор — расширение Джоуля", SG_MATTER},
    {23, 0, "меню", "Барометрическая формула", "газ в поле тяжести: ρ ∝ exp(−mgh/kT), Ar ниже Ne", SG_MATTER},
    {24, 0, "меню", "Эффузия", "закон Грэма: He утекает через щели в √10 раз быстрее Ar", SG_MATTER},
    {25, 0, "меню", "Спекание наночастиц", "Au и Ag срастаются в твёрдом состоянии: перешеек, диффузия", SG_MATTER},
    {26, 0, "меню", "Адиабатическое сжатие", "поршень сжимает газ без теплообмена: T·V^(γ−1) = const", SG_MATTER},
    {27, 0, "меню", "Смачивание", "капля растекается или собирается в шар: угол смачивания; повтор — несмачивание", SG_MATTER},
    {28, 0, "меню", "Кристаллизация на затравке", "переохлаждённая жидкость нарастает на закреплённом кристаллике", SG_MATTER},
    {29, 0, "меню", "Течение Пуазейля", "жидкость в канале: у стенок стоит, в центре быстрее всего — парабола", SG_MATTER},
    {31, 0, "меню", "Хлорирование метана", "CH4 + Cl2 → CH3Cl + HCl: УФ-свет запускает радикальную цепь", SG_CHEM},
    {32, 0, "меню", "Равновесие H2 + I2 ⇌ 2HI", "опыт Боденштейна: прямая и обратная реакции, K_c", SG_CHEM},
    {33, 0, "меню", "Гидрирование на никеле", "C2H4 + H2 → C2H6: H2 распадается на поверхности Ni", SG_CHEM},
    {34, 0, "меню", "Разложение пероксида", "2H2O2 → 2H2O + O2: радикалы OH, платина", SG_CHEM},
    {35, 0, "меню", "Горение ацетилена", "2C2H2 + 5O2 → 4CO2 + 2H2O от искры", SG_CHEM},
    {36, 0, "меню", "H2 + Br2 на свету", "цепная реакция медленнее хлорной: стадия Br + H2 эндотермична", SG_CHEM},
    {37, 0, "меню", "Хлор вытесняет бром", "Cl2 + 2HBr → 2HCl + Br2: связь H–Cl прочнее H–Br", SG_CHEM},
    {38, 0, "меню", "Водород и фтор", "H2 + F2 → 2HF без искры: слабая связь F–F рвётся сама", SG_CHEM},
    {39, 0, "меню", "Распад озона", "2O3 → 3O2 при нагреве: O3 → O2 + O, атомы O собираются в O2", SG_CHEM},
};
static const char* SG_NAMES[SG_N] = {"Вещество: фазы, перенос, механика", "Химия и растворы"};

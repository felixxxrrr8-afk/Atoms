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
static void showToast(const std::string& s) { toast = s; toastTime = 3.0; }
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
    pistonGrab = false; nlValid = false; fieldObjs.clear(); selFieldObj = -1;
    buildPairTables();
}
static void finishPreset() {
    wrapAll(); zeroMomentum(); updatePresence(); P.dt = P.dtBase;
    computeForces(); resetEnergyRef(); resetAnalysis(); resetMSD();
}
static std::string presetTitle;
static int presetVariants(int k) {
    if (k == 2) return DIM == 3 ? 6 : 3;
    if (k == 7 || k == 9 || k == 17) return 2;
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
            const char* nm[] = {"гексагональная", "квадратная (неустойчива)", "NaCl (ионная)"};
            presetTitle = std::string("2 · Плавление: ") + nm[v] + " решётка, нагрев P=const → плато T(t). Повтор 2 — другая решётка";
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
            const char* note = kind == L_BCC ? " (для LJ неустойчива → перестраивается в плотную упаковку)" : kind == L_SC ? " (для LJ неустойчива → коллапс)" : "";
            presetTitle = std::string("2 · Плавление: ") + LAT_NAMES[kind] + note + ", нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд";
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
        presetTitle = "Наночастица золота: металлическая связь (многочастичный потенциал), нагрев → плавление с поверхности"; break; }
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
        presetTitle = "Окисление железа: Fe + O2 → оксид; окисленные атомы теряют металлическую связь (ржавчина отслаивается)"; break; }
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
        presetTitle = "Натрий горит в хлоре: 2Na + Cl2 → 2NaCl, ионные пары собираются в кристаллики соли"; break; }
    case 14: {  // горение метана
        if (d3) worldReset(26, 26, 26, B_WALLS); else worldReset(70, 70 * asp, 1, B_WALLS);
        P.Tset = 2.4; P.thermostat = TH_BUSSI; P.tauT = 2.0; colorMode = 0;   // слабая связь с «баней»: пламя не гаснет
        fillBox(palette[findPal("CH4")], 170, P.Tset); fillBox(palette[findPal("O2")], 340, P.Tset);
        script.push_back({1.5, 4, false});
        presetTitle = "Горение метана: CH4 + 2O2 → CO2 + 2H2O (искра через 1.5τ; L — ещё вспышка)"; break; }
    case 15: {  // электрофорез: ионы в воде в электрическом поле
        int nion;
        if (d3) { worldReset(11, 11, 11, B_PERIODIC); nion = 10; P.substeps = 3; }
        else { worldReset(44, 44 * asp, 1, B_PERIODIC); nion = 14; P.substeps = 5; }
        fillBox(palette[findPal("Na+")], nion, 0.7, 0.0); fillBox(palette[findPal("Cl-")], nion, 0.7, 0.0);
        fillBox(palette[findPal("H2O")], (int)((d3 ? 0.55 : 0.64) * boxVolume()), 0.7, 0.0, 0.9);
        P.Tset = 0.7; P.tauT = 1.0; colorMode = 0;
        script.push_back({2.0, 6, false});
        presetTitle = "Электрофорез: поле E гонит Na+ по полю, Cl- — против; вода поляризуется, ток I — в верхней строке"; break; }
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
    }
    if (k >= 11 && k <= 15) presetTitle = fmt("Shift+%d · ", k - 10) + presetTitle;
    finishPreset();
    showToast(presetTitle + (d3 ? "   [3D]" : ""));
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
        case 30: { int c = photolyze(E_CL, E_CL, 0.15); if (c) showToast("УФ-вспышка: Cl2 → 2Cl·"); break; }
        }
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
    {31, 0, "меню", "Хлорирование метана", "CH4 + Cl2 → CH3Cl + HCl: УФ-свет запускает радикальную цепь", SG_CHEM},
    {32, 0, "меню", "Равновесие H2 + I2 ⇌ 2HI", "опыт Боденштейна: прямая и обратная реакции, K_c", SG_CHEM},
    {33, 0, "меню", "Гидрирование на никеле", "C2H4 + H2 → C2H6: H2 распадается на поверхности Ni", SG_CHEM},
    {34, 0, "меню", "Разложение пероксида", "2H2O2 → 2H2O + O2: радикалы OH, платина", SG_CHEM},
    {35, 0, "меню", "Горение ацетилена", "2C2H2 + 5O2 → 4CO2 + 2H2O от искры", SG_CHEM},
};
static const char* SG_NAMES[SG_N] = {"Вещество: фазы, перенос, механика", "Химия и растворы"};

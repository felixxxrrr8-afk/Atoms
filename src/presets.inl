// ===================================== PRESETS / UNDO ==================================
struct Snapshot { Sim s; Params p; std::vector<FieldObj> fo; int selFo = -1; };
static std::vector<Snapshot> undoStack;
static void strainX(double s);   // ui.inl
static void fitView(bool snap);   // render.inl
static void pushUndo() {
    undoStack.push_back({S, P, fieldObjs, selFieldObj});
    while (undoStack.size() > (size_t)std::max(1, opt.undo)) undoStack.erase(undoStack.begin());
}
struct ScriptEv { double t; int action; bool done; };
static std::vector<ScriptEv> script;
static int currentPreset = 1, presetVariant = 0;
static double sceneAspect = 0.66;
static std::string toast; static double toastTime = 0;
static bool trailsOn = false, bondsOn = true;
static double atomVis = 1.0;           // визуальный масштаб шаров
static int lmbTool = 0;                // инструмент ЛКМ: 0 добавить/пинцет, 1 ластик, 2 нагрев, 3 охлаждение, 4 камера
static bool ptOn = false, menuOn = false;   // открыты таблица Менделеева / меню сцен
// открыто окно поверх сцены (таблица, меню сцен, настройки, строение атома) — сцена ввод не получает
static inline bool anyOverlay() { return ptOn || menuOn || settingsOn || atomViewOn; }
// что сейчас на экране: молекулярная динамика или модель другого масштаба (сцены с номерами от 100, worlds.inl)
enum { W_MD, W_NUC, W_QUARK, W_SEMI, W_WAVE, W_N };
static int world = W_MD;
static const char* WORLD_NAMES[W_N] = {"молекулярная динамика", "ядра и нейтроны", "кварки и адроны", "полупроводники", "квантовые волны"};
static void worldLoad(int k, int variant); static void worldLeave(); static void worldStep(double frameDt);   // worlds.inl
static void worldDraw(); static void worldPanel(float x, float y, float w, float h); static void worldStrip(float x, float y, float w, float h);
static std::string worldClock(); static std::string worldSubtitle();
static int colorMode = 0;
static int selPal = 1;
static double sparkR = 5.0, sparkTK = 3000;   // искра сценария: радиус шара (σ) и температура плазмы (K)
static inline double kelvin(double K) { return K / cfg::U_T_K; }   // температура (K) в единицах модели
// режим «штампа» молекулы библиотеки: выбрана структура из вкладки «Химия» (первая ячейка палитры).
// UI: ЛКМ в сцене (инструмент «добавить») → pushUndo(); insertMolecule(chemStampMol, x, y, z) — один раз за щелчок
static inline bool chemStampActive() { return chemStampMol >= 0 && selPal == 0; }
static bool viewFitPending = true;
static bool fullscreen = false; static void toggleFullscreen();
static void showToast(const std::string& s) { toast = LANG == LANG_RU ? s : std::string(Tsv(s, "showToast")); toastTime = opt.toastSec; }   // точный литерал переводится
static void resetMSD();
static void popUndo() {
    if (undoStack.empty() || world != W_MD) return;   // в мирах других масштабов отмена не нужна: R — заново
    S = undoStack.back().s; bool pz = P.paused; P = undoStack.back().p; P.paused = pz;
    fieldObjs = undoStack.back().fo; selFieldObj = clampv(undoStack.back().selFo, -1, (int)fieldObjs.size() - 1); undoStack.pop_back();
    grabbed = -1; buildPairTables(); updatePresence(); computeForces(); resetEnergyRef(); resetMSD();
}

static void worldReset(double Lx, double Ly, double Lz, int boundary) {
    S = Sim(); S.Lx = Lx; S.Ly = Ly; S.Lz = Lz; P.boundary = boundary;
    P.perMask = boundary == B_PERIODIC ? 7 : 0; P.container = CT_BOX; absorbedCount = 0;
    for (int f = 0; f < 6; f++) { P.wallType[f] = WT_SOFT; P.wallTK[f] = kelvin(300); }
    P.npt = false; P.heatWalls = 0; P.gravity = 0; P.catalyst = false; P.chemistry = true;
    P.thermostat = TH_BUSSI; P.tauT = 0.5; P.epsScale = 1.0; P.wallAttr = 0.6; P.heatPower = 0.03; P.efield = 0; P.eaScale = 1.0;
    P.acidBase = true; P.surfCat = true; P.orbRule = true; rxT0 = 0; chemPT = 0;
    grabbed = -1; followAtom = -1; flashes.clear(); CH = ChemStats(); script.clear(); trailsOn = false; P.substeps = 8;
    pistonGrab = false; nlValid = false; fieldObjs.clear(); selFieldObj = -1; heatWallQ[0] = heatWallQ[1] = 0;
    sparkR = 5.0; sparkTK = 3000;
    guardSnap[0].ok = guardSnap[1].ok = false; guardDtScale = 1.0;   // снимки стража и ограничение шага — от прошлой сцены
    dtChanged = 0; dtLimMin = 1e30;
    buildPairTables();
}
static void finishPreset() {
    wrapAll(); zeroMomentum(); updatePresence(); P.dt = P.dtBase;
    computeForces(); dtFromState(); resetEnergyRef(); resetAnalysis(); resetMSD();
}
// presetTitle — всегда целый русский литерал (на экран — через перевод); presetLoaded — состояние загружено из файла
static std::string presetTitle; static bool presetLoaded = false;
static int presetVariants(int k) {
    if (k == 2) return 6;
    if (k == 7 || k == 9 || k == 17 || k == 22 || k == 27 || k == 47) return 2;
    return 1;
}
// шарик металла (ГЦК) радиуса R (σ) с центром (cx, cy, cz)
static void metalBall(int t, double cx, double cy, double cz, double R, double T) {
    const double a = 2 * EL[t].rmet / 3.405 * std::sqrt(2.0); const int m = (int)(R / a) + 2;
    const double bs[4][3] = {{0, 0, 0}, {0.5, 0.5, 0}, {0.5, 0, 0.5}, {0, 0.5, 0.5}};
    const int first = S.n;
    for (int k = -m; k <= m; k++) for (int j = -m; j <= m; j++) for (int i = -m; i <= m; i++) for (auto& b : bs) {
        double x = (i + b[0]) * a, y = (j + b[1]) * a, z = (k + b[2]) * a;
        if (x * x + y * y + z * z > R * R) continue;
        double vx, vy, vz; thermalVel(t, T, vx, vy, vz); addAtom(t, cx + x, cy + y, cz + z, vx, vy, vz);
    }
    // частица целиком покоится: иначе случайная скорость центра масс (при сотнях кельвинов заметная) уносит её
    double px = 0, py = 0, pz = 0; const int cnt = S.n - first; if (cnt <= 0) return;
    for (int i = first; i < S.n; i++) { px += S.vx[i]; py += S.vy[i]; pz += S.vz[i]; }
    for (int i = first; i < S.n; i++) { S.vx[i] -= px / cnt; S.vy[i] -= py / cnt; S.vz[i] -= pz / cnt; }
}
// водный раствор: ящик с периодическими границами (не меньше двух радиусов обрезки кулона), ионы и молекулы
// ставятся до воды, вода — настоящей плотности 1 г/см³ во всё оставшееся место
static void waterBox(double L) { worldReset(L, L, L, B_PERIODIC); P.substeps = 3; P.Tset = kelvin(300); P.tauT = 0.3; P.wallAttr = 0; }
// короткий спуск вдоль сил: снимает случайные тесные контакты после заливки плотной жидкости (скорости не меняются);
// смещение за итерацию ∝ F, но не больше 0.02σ
static void relaxContacts(int iters) {
    updatePresence();
    for (int it = 0; it < iters; it++) {
        computeForces();
        for (int i = 0; i < S.n; i++) {
            if (frozenAt(i)) continue;
            const double fx = S.fx[i] + S.bx[i], fy = S.fy[i] + S.by[i], fz = S.fz[i] + S.bz[i], f = std::sqrt(fx * fx + fy * fy + fz * fz);
            if (f < 1e-9) continue;
            const double s = std::min(0.02, 2e-5 * f) / f;
            S.x[i] += s * fx; S.y[i] += s * fy; S.z[i] += s * fz; S.ux[i] += s * fx; S.uy[i] += s * fy; S.uz[i] += s * fz;
        }
        wrapAll();
    }
}
// вода — 0.96 г/см³: при этой плотности давление модели воды при 300 K близко к атмосферному
static int fillWater(double T) {
    const int n = fillGrid(palette[findPal("H2O")], (int)std::lround(0.96 * waterMolecules(boxVolume())), 0, 0, 0, S.Lx, S.Ly, S.Lz, T);
    relaxContacts(40);
    return n;
}
// горючая смесь при комнатной температуре; до искры термостат держит 300 K, с искрой сосуд теплоизолирован (NVE) —
// теплота реакции остаётся в газе, и пламя само разогревает смесь до тысяч кельвинов.
// Искра (плазма разряда 5000 K в шаре радиуса 7σ) — через 1.5τ
static void combustion(double L) {
    worldReset(L, L, L, B_WALLS); P.Tset = kelvin(300); P.thermostat = TH_BUSSI; P.tauT = 0.3; P.wallAttr = 0; colorMode = 0;
    sparkR = 7; sparkTK = 5000; script.push_back({1.5, 40, false});
}
// стенка-перегородка из неподвижных атомов в плоскости x = x0
static void partition(double x0) {
    for (double y = 0.5; y < S.Ly; y += 0.9) for (double z = 0.5; z < S.Lz; z += 0.9) addAtom(E_WALL, x0, y, z, 0, 0, 0);
}
// шаблон «один атом типа t» (для элементов, которых нет в палитре веществ)
static Tmpl atomTmpl(int t) { Tmpl m; m.label = EL[t].sym; m.a = {{t, 0, 0, 0}}; return m; }
// пористая перегородка в плоскости x = x0: объекты-барьеры (чисто отталкивающие, без адсорбции), между ними
// nSlits щелей шириной slit — полосы через весь ящик по z. Барьеры сразу в полную силу.
static void porousMembrane(double x0, double slit, int nSlits) {
    const double p = S.Ly / nSlits;
    for (int k = 0; k <= nSlits; k++) {
        double y0 = k == 0 ? -1.0 : (k - 0.5) * p + 0.5 * slit, y1 = k == nSlits ? S.Ly + 1.0 : (k + 0.5) * p - 0.5 * slit;
        FieldObj o = makeFieldObj(FO_BARRIER, x0, 0.5 * (y0 + y1), 0.5 * S.Lz);
        o.x = o.x2 = x0; o.y = o.y2 = 0.5 * (y0 + y1); o.z = -1.0; o.z2 = S.Lz + 1.0; o.R = 0.5 * (y1 - y0);
        o.dx = 1; o.dy = 0; o.dz = 0; o.strength = 3.0; o.ramp = 1.0;
        fieldObjs.push_back(o);
    }
}
// ---- Живые измерения сцен (строка A::sceneNote под заголовком; обновляется в analysisTick)
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
static double rhoL = -1, rhoG = -1;                         // жидкость и пар: сглаженные плотности
static double P0 = 1e9;                                     // кавитация: давление в начале
static double Tign = -1;                                    // самовоспламенение: T, при которой пошла реакция
static double wireL0 = 0;                                   // нанопровод: исходная длина
}
static void sceneMeasureReset() { SM::tPrev = -1; SM::qInit = false; SM::tOpen = -1; SM::front.clear(); SM::speedW = 0; SM::V0 = 0; SM::avg[0].clear(); SM::avg[1].clear();
                                  SM::adT0 = SM::adV0 = 0; SM::vprof.clear(); SM::rhoL = SM::rhoG = -1; SM::P0 = 1e9; SM::Tign = -1; SM::wireL0 = 0; }
// двухатомная частица по таблице связей (I2, HI …)
static Tmpl diTmpl(const char* l, int t1, int t2, int o) { Tmpl m; m.label = l; double r = r0of(t1, t2, o); m.a = {{t1, -r / 2, 0, 0}, {t2, r / 2, 0, 0}}; m.b = {{0, 1, o}}; return m; }
static void loadPreset(int k, int variant) {
    if (k >= 100) { worldLoad(k, variant); return; }
    if (world != W_MD) { world = W_MD; worldLeave(); }
    currentPreset = k; presetVariant = variant % std::max(1, presetVariants(k));
    switch (k) {
    case 1: {   // идеальный газ: PV ≈ NkT
        worldReset(30, 30, 30, B_WALLS);
        P.Tset = 2.0; P.thermostat = TH_NVE; colorMode = 1;
        fillBox(palette[findPal("Ar")], 800, P.Tset);
        presetTitle = "1 · Идеальный газ: сравните P с NkT/V (Z = PV/NkT ≈ 1)"; break; }
    case 2: {   // кристалл со свободной поверхностью в ящике с вакуумом, нагрев постоянной мощностью
        const int kind = presetVariant; double T0 = 0.1;
        int nx, ny, nz; double a;
        switch (kind) {
        case L_FCC: a = 1.56; nx = ny = nz = 7; break;
        case L_HCP: a = 1.10; nx = 10; ny = 6; nz = 6; break;
        case L_BCC: a = 1.26; nx = ny = nz = 9; break;
        case L_SC: a = 1.05; nx = ny = nz = 11; break;
        case L_NACL: a = 2.82 / 3.405; nx = ny = nz = 10; T0 = kelvin(300); break;   // Na–Cl 2.82 Å
        default: a = 6.35 / 3.405; nx = ny = nz = 4; T0 = kelvin(100); break;        // лёд Ic: ребро ячейки 6.35 Å;
                                                                                     // у кристаллика 3×3×3 на поверхности 42% молекул
        }
        double cx = kind == L_NACL ? nx * a : nx * latticeCellX(kind, a), cy = kind == L_NACL ? ny * a : ny * latticeCellY(kind, a), cz = kind == L_NACL ? nz * a : nz * latticeCellZ(kind, a);
        double L = std::max({cx, cy, cz}) * (kind == L_ICE ? 2.6 : 2.0);
        worldReset(L, L, L, B_PERIODIC);
        if (kind == L_ICE) {
            iceLattice3D((L - cx) / 2, (L - cy) / 2, (L - cz) / 2, nx, ny, nz, a, T0);
            // идеальная решётка — не минимум энергии модели: угол H–O–H в ней тетраэдрический (у модели 104.5°), молекулы
            // у поверхности не уравновешены соседями. Без спуска вдоль сил эта энергия за доли пикосекунды уходила в тепло,
            // и кристаллик таял, не дойдя до плато. После спуска тепловые скорости раздаются заново
            relaxContacts(300);
            std::vector<int> all(S.n); for (int i = 0; i < S.n; i++) all[i] = i;
            std::vector<std::vector<int>> comps; componentsIn(all, comps);
            for (auto& c : comps) thermalizeMol(c.data(), (int)c.size(), T0);
        }
        else lattice3D(kind, kind == L_NACL ? E_NA : E_AR, kind == L_NACL ? E_CLM : -1, 0, (L - cx) / 2, (L - cy) / 2, (L - cz) / 2, nx, ny, nz, a, T0);
        static const char* const t3[] = {   // по L_FCC, L_HCP, L_BCC, L_SC, L_NACL, L_ICE
            "2 · Плавление: ГЦК, нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд",
            "2 · Плавление: ГПУ, нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд",
            "2 · Плавление: ОЦК (для LJ неустойчива → перестраивается в плотную упаковку), нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд",
            "2 · Плавление: ПК (для LJ неустойчива → коллапс), нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд",
            "2 · Плавление: NaCl, нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд",
            "2 · Плавление: лёд, нагрев P=const → плато T(t). Повтор 2 — ГЦК/ГПУ/ОЦК/ПК/NaCl/лёд"};
        presetTitle = t3[clampv(kind, 0, 5)];
        P.Tset = T0; P.thermostat = TH_POWER; colorMode = 3;
        // мощность на атом: соль плавится при 1074 K, лёд — при 273 K (от 100 K, с теплотой плавления — за ~40τ;
        // поверхность кристаллика разупорядочивается уже к 150–180 K, ядро держится до ~250 K)
        P.heatPower = kind == L_NACL ? 0.75 : (kind == L_ICE ? 0.1 : 0.012);
        if (kind == L_ICE) P.substeps = 4;   // 1536 атомов с кулоном: по 4 шага на кадр модель идёт так же быстро, но вдвое плавнее
        break; }
    case 3: {   // кипение и испарение с поверхности под гравитацией
        // крайние слои — на равновесном расстоянии от стенок (минимум потенциала 9-3 ≈ 0.86σ)
        worldReset(10.5 * 1.6 + 1.72, 30, 10.5 * 1.6 + 1.72, B_WALLS); P.Tset = 0.8;
        lattice3D(L_FCC, E_AR, -1, 0, 0.46, 0.46, 0.46, 11, 4, 11, 1.6, 0.8);
        P.Thot = 1.2;
        P.gravity = 0.012; P.wallAttr = 0.8; P.thermostat = TH_BERENDSEN; colorMode = 1; P.substeps = 6;
        script.push_back({8.0, 1, false});   // через 8τ: горячее дно, термостат выключен
        presetTitle = "3 · Кипение: жидкость под гравитацией, горячее дно, испарение с поверхности"; break; }
    case 4: {   // конденсация пересыщенного пара
        int N = 2000; double rho = 0.05;
        double L = std::cbrt(N / rho); worldReset(L, L, L, B_PERIODIC);
        P.Tset = 1.4; fillBox(palette[findPal("Ar")], N, P.Tset, 0.0);
        script.push_back({3.0, 2, false}); colorMode = 1;
        presetTitle = "4 · Конденсация: пар охлаждается → зародыши → круглые капли (поверхностное натяжение)"; break; }
    case 5: {   // смешивание двух газов
        worldReset(40, 22, 22, B_WALLS);
        P.Tset = 1.5; partition(S.Lx / 2);
        int N = 700;
        fillRandom(palette[findPal("Ar")], N, 1, 1, 1, S.Lx / 2 - 1, S.Ly - 1, S.Lz - 1, 1.5);
        fillRandom(palette[findPal("Ne")], N, S.Lx / 2 + 1, 1, 1, S.Lx - 1, S.Ly - 1, S.Lz - 1, 1.5);
        script.push_back({2.0, 3, false}); colorMode = 0;
        presetTitle = "5 · Диффузия: перегородка исчезнет через 2τ, смотрите MSD(t) и D"; break; }
    case 6: {   // кристаллик NaCl в горячей воде
        waterBox(7.4); P.Tset = kelvin(360);
        const double a = 2.82 / 3.405; const int m = 2, h = 4;   // 2×2×4 = 16 ионов, Na–Cl 2.82 Å
        lattice3D(L_NACL, E_NA, E_CLM, 0, S.Lx / 2 - m * a / 2 - 0.25 * a, S.Ly / 2 - m * a / 2 - 0.25 * a, S.Lz / 2 - h * a / 2 - 0.25 * a, m, m, h, a, P.Tset);
        fillWater(P.Tset); colorMode = 0;
        presetTitle = "6 · NaCl в горячей воде (360 K): ионы с углов и рёбер уходят в раствор, вокруг них гидратные оболочки; весь кристалл растворился бы за наносекунды"; break; }
    case 7: {   // горение водорода / хлороводород
        combustion(20);
        if (presetVariant == 0) {
            fillBox(palette[findPal("H2")], 440, P.Tset); fillBox(palette[findPal("O2")], 220, P.Tset);
            presetTitle = "7 · 2H2 + O2 → 2H2O: смесь при 300 K, искра через 1.5τ — пламя разогревает газ до тысяч кельвинов (L — ещё вспышка, повтор 7 — H2+Cl2)";
        } else {
            fillBox(palette[findPal("H2")], 330, P.Tset); fillBox(palette[findPal("Cl2")], 330, P.Tset);
            presetTitle = "7b · H2 + Cl2 → 2HCl: цепная реакция от искры (L — вспышка света)";
        }
        break; }
    case 8: {   // броуновское движение
        int N = 2000;
        double L = std::cbrt(N / 0.3); worldReset(L, L, L, B_PERIODIC);
        P.Tset = 1.0;
        addAtom(E_BIG, S.Lx / 2, S.Ly / 2, S.Lz / 2, 0, 0, 0);
        fillBox(palette[findPal("Ar")], N, 1.0, 0.0);
        trailsOn = true; colorMode = 1; P.substeps = 6;
        presetTitle = "8 · Броуновское движение: тяжёлая частица среди атомов, MSD ∝ t"; break; }
    case 9: {   // закалка: поликристалл с дефектами / стекло из бинарной смеси
        bool glass = presetVariant == 1;
        double rho = glass ? 1.05 : 0.95, a = std::cbrt(4.0 / rho); int m = 8;
        worldReset(m * a, m * a, m * a, B_PERIODIC);
        lattice3D(L_FCC, E_AR, glass ? E_NE : -1, 0.5, -0.25 * a, -0.25 * a, -0.25 * a, m, m, m, a, 2.0);
        P.Tset = 2.0; script.push_back({3.0, 5, false}); colorMode = glass ? 4 : 3; P.substeps = 5;
        presetTitle = glass ? "9b · Стекло: бинарная смесь Ar/Ne, резкая закалка (повтор 9 — поликристалл)"
                            : "9 · Закалка: поликристалл, границы зёрен, дислокации ([ ] — растянуть до разрушения; повтор 9 — стекло)";
        break; }
    case 11: {  // наночастица золота: металлическая связь, плавление
        const int Au = typeOfZ(79); const double a = 2 * EL[Au].rmet / 3.405 * std::sqrt(2.0), R = 4.6 * a, L = 2 * R + 14;
        worldReset(L, L, L, B_PERIODIC);
        metalBall(Au, L / 2, L / 2, L / 2, R, kelvin(300));
        P.Tset = kelvin(300); P.thermostat = TH_POWER; P.heatPower = 0.8; colorMode = 3; P.substeps = 10;
        presetTitle = "Shift+1 · Наночастица золота (3.6 нм): металлическая связь, нагрев от 300 K → плавление начинается с поверхности, заметно ниже 1337 K массивного золота"; break; }
    case 12: {  // окисление железа
        int Fe = typeOfZ(26); double d = 2 * EL[Fe].rmet / 3.405;
        double a = d * std::sqrt(2.0); int nx = 10, ny = 3, nz = 10;
        worldReset(nx * a + 1.2, 22, nz * a + 1.2, B_WALLS);
        P.Tset = kelvin(600);
        lattice3D(L_FCC, Fe, -1, 0, 0.35, 0.35, 0.35, nx, ny, nz, a, P.Tset);
        fillRandom(palette[findPal("O2")], 260, 1, ny * a + 2.5, 1, S.Lx - 1, S.Ly - 1, S.Lz - 1, P.Tset);
        P.tauT = 0.5; P.wallAttr = 0; colorMode = 0;
        presetTitle = "Shift+2 · Окисление горячего железа (600 K): O2 распадается на поверхности, атомы O садятся на железо — растёт слой оксида, теплота реакции греет металл"; break; }
    case 13: {  // натрий в хлоре
        int Na = typeOfZ(11); double d = 2 * EL[Na].rmet / 3.405;
        worldReset(26, 26, 26, B_WALLS);
        P.Tset = kelvin(300);
        double a = d * std::sqrt(2.0); int m = 5; double c = m * a;
        lattice3D(L_FCC, Na, -1, 0, S.Lx / 2 - c / 2, 1.0, S.Lz / 2 - c / 2, m, m, m, a, P.Tset);
        fillRandom(palette[findPal("Cl2")], 420, 1, c + 3, 1, S.Lx - 1, S.Ly - 1, S.Lz - 1, P.Tset);
        P.tauT = 5.0; P.wallAttr = 0; colorMode = 0;
        presetTitle = "Shift+3 · Натрий горит в хлоре: 2Na + Cl2 → 2NaCl без барьера уже при 300 K; теплота реакции раскаляет металл, пары ионов собираются в кристаллики соли"; break; }
    case 14: {  // горение метана
        combustion(24);
        fillBox(palette[findPal("CH4")], 150, P.Tset); fillBox(palette[findPal("O2")], 300, P.Tset);
        presetTitle = "Shift+4 · Горение метана: CH4 + 2O2 → CO2 + 2H2O — смесь при 300 K, искра через 1.5τ (L — ещё вспышка)"; break; }
    case 15: {  // электрофорез: ионы в воде в электрическом поле
        waterBox(7.4);
        const int nion = 5;
        fillBox(palette[findPal("Na+")], nion, P.Tset, 0.0); fillBox(palette[findPal("Cl-")], nion, P.Tset, 0.0);
        fillWater(P.Tset); colorMode = 0;
        script.push_back({2.0, 6, false});
        presetTitle = "Shift+5 · Электрофорез: поле 0.5 В/нм гонит Na+ по полю, Cl- — против; вода поворачивает диполи, ток I — в верхней строке"; break; }
    case 16: {  // кислота в воде: HCl ионизуется, протон бегает по воде
        waterBox(7.4);
        fillBox(libTmpl(ML_HCL), 5, P.Tset, 0.0, 0.9);
        fillWater(P.Tset); colorMode = 0;
        presetTitle = "Кислота в воде: HCl + H2O → H3O+ + Cl−, протон переходит по цепочке водородных связей (Гроттгус) за пикосекунды; pH — вкладка «Химия»"; break; }
    case 17: {  // нейтрализация / титрование
        waterBox(7.4);
        const int ni = 4;
        if (presetVariant == 0) {   // растворы HCl и NaOH только что слиты: ионы перемешаны
            fillBox(libTmpl(ML_H3O), ni, P.Tset, 0.0, 0.9); fillBox(palette[findPal("Cl-")], ni, P.Tset, 0.0, 0.9);
            fillBox(palette[findPal("Na+")], ni, P.Tset, 0.0, 0.9); fillBox(libTmpl(ML_OH), ni, P.Tset, 0.0, 0.9);
            presetTitle = "Нейтрализация HCl + NaOH: H3O+ + OH− → 2H2O — протон и гидроксид-ион бегут навстречу по цепочкам водородных связей, pH → 7. Повтор — титрование";
        } else {                    // титрование: раствор HCl, NaOH добавляется порциями
            fillBox(libTmpl(ML_H3O), ni, P.Tset, 0.0, 0.9); fillBox(palette[findPal("Cl-")], ni, P.Tset, 0.0, 0.9);
            for (int k = 1; k <= 2 * ni; k++) script.push_back({6.0 * k, 7, false});
            presetTitle = "Титрование: к раствору HCl каждые 6τ добавляется порция NaOH; pH скачком проходит 7 в точке эквивалентности";
        }
        fillWater(P.Tset);
        P.thermostat = TH_BUSSI; P.tauT = 0.5; colorMode = 0;
        break; }
    case 18: {  // горение этанола
        combustion(24);
        const int ne = 90;
        fillBox(libTmpl(ML_C2H5OH), ne, P.Tset); fillBox(palette[findPal("O2")], 3 * ne, P.Tset);
        presetTitle = "Горение этанола: C2H5OH + 3O2 → 2CO2 + 3H2O — пары при 300 K, искра через 1.5τ (L — ещё вспышка)"; break; }
    case 19: {  // гремучая смесь в закрытом сосуде
        combustion(16); sparkR = 5; sparkTK = 4000; script.back().t = 1.0;
        const int nh = 300;
        fillBox(palette[findPal("H2")], nh, P.Tset); fillBox(palette[findPal("O2")], nh / 2, P.Tset);
        presetTitle = "Гремучая смесь в закрытом сосуде: 2H2 + O2 → 2H2O; искра через 1τ → взрыв: газ раскаляется до ~4000 K, давление растёт в десятки раз (NVE)"; break; }
    case 20: {  // гетерогенный катализ: наночастица платины в смеси H2 + O2
        const int Pt = typeOfZ(78);
        worldReset(20, 20, 20, B_PERIODIC); P.Tset = kelvin(600);
        metalBall(Pt, 10, 10, 10, 2.0, P.Tset);
        const int nh = 240;
        fillBox(palette[findPal("H2")], nh, P.Tset, 0.0); fillBox(palette[findPal("O2")], nh / 2, P.Tset, 0.0);
        P.thermostat = TH_BUSSI; P.tauT = 1.0; colorMode = 0; P.substeps = 5;
        presetTitle = "Катализ на платине (600 K): в газе смесь H2 + O2 инертна, а на поверхности Pt молекулы распадаются на атомы, и из них собирается вода"; break; }
    case 21: {  // теплопроводность: левая стенка горячая, правая холодная → стационарный линейный профиль T(x)
        worldReset(34, 13, 13, B_WALLS); P.Thot = 3.0; P.Tcold = 1.2;
        P.heatWalls = 1; P.thermostat = TH_NVE; P.Tset = 0.5 * (P.Thot + P.Tcold); P.wallAttr = 0;
        fillBox(palette[findPal("Ar")], (int)(0.40 * boxVolume()), P.Tset, 0.8);
        colorMode = 1;
        presetTitle = "Теплопроводность: левая стенка горячая, правая холодная → линейный профиль T(x), поток тепла q = −κ·dT/dx (закон Фурье)"; break; }
    case 22: {  // ударная труба / расширение Джоуля: перегородка между плотным и разреженным (или пустым) объёмом
        const Tmpl& ar = palette[findPal("Ar")];
        double xm, rhoL, rhoR, TR;
        if (presetVariant == 0) {   // классическая ударная труба: горячий плотный «толкающий» газ и холодный разреженный
            worldReset(90, 14, 14, B_WALLS);
            xm = 0.3 * S.Lx; rhoL = 0.12; rhoR = 0.025; P.Tset = 4.0; TR = 1.2;
            presetTitle = "Ударная труба: через 1τ мембрана лопнет → горячий плотный газ гонит ударную волну (скачок плотности и T) по холодному разреженному, назад бежит волна разрежения. Повтор — расширение в вакуум";
        } else {
            worldReset(36, 18, 18, B_WALLS);
            xm = 0.5 * S.Lx; rhoL = 0.30; rhoR = 0; P.Tset = 1.5; TR = P.Tset;
            presetTitle = "Расширение Джоуля: через 1τ плотный газ вырвется в вакуум; энергия сохраняется, но газ остывает — атомы работают против взаимного притяжения";
        }
        const double A = (S.Ly - 1.6) * (S.Lz - 1.6);
        partition(xm);
        fillRandom(ar, (int)(rhoL * (xm - 1.7) * A), 0.8, 0.8, 0.8, xm - 0.9, S.Ly - 0.8, S.Lz - 0.8, P.Tset);
        if (rhoR > 0) fillRandom(ar, (int)(rhoR * (S.Lx - xm - 1.7) * A), xm + 0.9, 0.8, 0.8, S.Lx - 0.8, S.Ly - 0.8, S.Lz - 0.8, TR);
        P.thermostat = TH_NVE; P.wallAttr = 0; colorMode = 1;
        script.push_back({1.0, 20, false}); break; }
    case 23: {  // барометрическая формула: смесь Ar и Ne в поле тяжести
        worldReset(16, 30, 16, B_WALLS);
        const double T = 1.5, H = 7.0;   // H — высота однородной атмосферы аргона kT/(mg)
        P.gravity = T / (EL[E_AR].m * H); P.Tset = T; P.thermostat = TH_BUSSI; P.tauT = 1.0; P.wallAttr = 0;
        const int n = 300;
        fillBox(palette[findPal("Ar")], n, T, 0.8); fillBox(palette[findPal("Ne")], n, T, 0.8);
        colorMode = 0; P.substeps = 10;
        presetTitle = "Барометрическая формула: в поле тяжести плотность газа падает как exp(−mgh/kT) — тяжёлый Ar прижат ко дну, лёгкий Ne поднимается выше (ρ(y) — «Графики»)"; break; }
    case 24: {  // эффузия через пористую перегородку: смесь He + Ar, справа вакуум
        worldReset(40, 20, 20, B_WALLS);
        const double xm = 0.5 * S.Lx;
        porousMembrane(xm, 4.0, 4);
        const int n = 170, He = typeOfZ(2);
        P.Tset = 1.2; P.thermostat = TH_BUSSI; P.tauT = 2.0; P.wallAttr = 0;
        fillRandom(atomTmpl(He), n, 0.8, 0.8, 0.8, xm - 1.2, S.Ly - 0.8, S.Lz - 0.8, P.Tset);
        fillRandom(palette[findPal("Ar")], n, 0.8, 0.8, 0.8, xm - 1.2, S.Ly - 0.8, S.Lz - 0.8, P.Tset);
        colorMode = 0; P.substeps = 10;
        presetTitle = "Эффузия (закон Грэма): смесь He и Ar утекает через узкие щели в вакуум — лёгкий гелий быстрее в √(mAr/mHe) ≈ 3.2 раза"; break; }
    case 25: {  // спекание: наночастицы золота и серебра касаются и срастаются ниже температуры плавления
        const int Au = typeOfZ(79), Ag = typeOfZ(47);
        const double d = 2 * EL[Au].rmet / 3.405, R = 3.4, T0 = kelvin(450);   // частицы ~2.3 нм плавятся уже около 600–700 K
        worldReset(4 * R + 12, 2 * R + 10, 2 * R + 10, B_PERIODIC);
        const double cx = S.Lx / 2, cy = S.Ly / 2, cz = S.Lz / 2, h = R + 0.5 * d;
        metalBall(Au, cx - h, cy, cz, R, T0); metalBall(Ag, cx + h, cy, cz, R, T0);
        P.Tset = T0; P.thermostat = TH_BUSSI; P.tauT = 1.0; colorMode = 0; P.substeps = 10;
        presetTitle = "Спекание: наночастицы Au и Ag коснулись ниже температуры плавления — растёт перешеек, частицы сближаются, атомы перемешиваются (твёрдое состояние!)"; break; }
    case 26: {  // адиабатическое сжатие: давление на поршень растёт ступенями, газ теплоизолирован (NVE)
        // низкий широкий сосуд и лёгкий гелий: звук пересекает газ за ~5τ, а сжатие идёт 75τ — процесс квазистатический
        // (при быстром сжатии поршень гонит ударные волны, и газ греется сильнее адиабаты)
        const int n = 500, He = typeOfZ(2);
        worldReset(30, 12, 30, B_PISTON);
        P.Tset = 1.5; P.thermostat = TH_NVE; P.wallAttr = 0; S.pistonM = 10; colorMode = 1;
        fillBox(atomTmpl(He), n, P.Tset, 0.8);
        P.pExt = n * P.Tset / boxVolume();   // давление идеального газа: поршень сначала в равновесии
        for (int s = 0; s < 150; s++) script.push_back({2.0 + 0.5 * s, 21, false});   // ×1.018 каждые 0.5τ → ×14.5 за 75τ
        presetTitle = "Адиабатическое сжатие гелия: с 2τ давление на поршень плавно растёт в 14 раз за 75τ; газ без теплообмена нагревается, TV^(γ−1) ≈ const"; break; }
    case 27: {  // смачивание: капля жидкости на притягивающей / отталкивающей стенке
        const bool wet = presetVariant == 0;
        worldReset(30, 22, 30, B_WALLS);
        const double a = 1.6, cx = 8 * a, cz = 8 * a;
        lattice3D(L_FCC, E_AR, -1, 0, (S.Lx - cx) / 2, 0.46, (S.Lz - cz) / 2, 8, 4, 8, a, 0.75);
        P.Tset = 0.75;
        P.thermostat = TH_BERENDSEN; P.tauT = 1.0; P.gravity = 0.004; P.wallAttr = wet ? 1.6 : 0.1; colorMode = 0;
        presetTitle = wet ? "Смачивание: стенка притягивает атомы сильнее, чем они друг друга → капля растекается, угол смачивания < 90°. Повтор — несмачивание"
                          : "Несмачивание: стенка почти не притягивает → капля собирается в шар, угол смачивания > 90° (как ртуть на стекле). Повтор — смачивание";
        break; }
    case 28: {  // кристаллизация переохлаждённой жидкости на закреплённой затравке
        const double a = std::cbrt(4.0 / 0.95), R = 2.3; const int m = 8;
        worldReset(m * a, m * a, m * a, B_PERIODIC);
        lattice3D(L_FCC, E_AR, -1, 0, -0.25 * a, -0.25 * a, -0.25 * a, m, m, m, a, 1.8);
        P.Tset = 0.62;
        // затравка — шар решётки в центре: закреплена, пока жидкость вокруг плавится и остывает
        S.pin.resize(S.n, 0);
        for (int i = 0; i < S.n; i++) {
            const double dx = S.x[i] - S.Lx / 2, dy = S.y[i] - S.Ly / 2, dz = S.z[i] - S.Lz / 2;
            if (dx * dx + dy * dy + dz * dz < R * R) { S.pin[i] = 1; S.vx[i] = S.vy[i] = S.vz[i] = 0; }
        }
        P.thermostat = TH_BUSSI; P.tauT = 1.5; colorMode = 3; P.substeps = 5;
        presetTitle = "Кристаллизация на затравке: переохлаждённая жидкость нарастает слоями на закреплённом кристаллике (цвет — упорядоченность)"; break; }
    case 29: {  // течение Пуазейля: жидкость в канале между стенками, однородная сила вдоль x
        worldReset(20, 14, 12, B_PERIODIC);
        for (double x = 0.5; x < S.Lx - 0.2; x += 0.9) {   // стенка канала — плоскость неподвижных атомов при y ≈ 0.45
            for (double z = 0.45; z < S.Lz; z += 0.9) addAtom(E_WALL, x, 0.45, z, 0, 0, 0);
        }
        P.Tset = 1.0; P.thermostat = TH_BUSSI; P.tauT = 4.0; P.wallAttr = 0; colorMode = 1;
        fillBox(palette[findPal("Ar")], (int)(0.55 * boxVolume()), P.Tset, 0.0);
        FieldObj w = makeFieldObj(FO_WIND, S.Lx / 2, S.Ly / 2, S.Lz / 2); w.R = 1000; w.strength = 0.03; w.ramp = 1.0;
        fieldObjs.push_back(w);   // «ветер» радиусом 1000σ — однородная сила по всему каналу (как перепад давления)
        presetTitle = "Течение Пуазейля: сила гонит жидкость вдоль канала, у стенок она прилипает → профиль скорости — парабола (вязкость)"; break; }
    case 0: {   // химическое равновесие Cl2 ⇌ 2Cl и принцип Ле Шателье
        worldReset(24, 24, 24, B_PISTON); P.pExt = 0.2; P.Tset = kelvin(5500); P.wallAttr = 0;
        fillBox(palette[findPal("Cl2")], 700, P.Tset);
        S.pistonM = 60; colorMode = 0;
        presetTitle = "0 · Равновесие Cl2 <=> 2Cl при 5500 K (связь Cl–Cl 2.5 эВ за пикосекунды рвётся только в раскалённом газе): меняйте T и давление поршня — сдвиг по Ле Шателье"; break; }
    // ---- химические сцены (только из меню): 31–39
    case 31: {  // хлорирование метана на свету: УФ поглощает только Cl2 → радикальная цепь
        worldReset(24, 24, 24, B_WALLS);
        P.Tset = kelvin(700); P.thermostat = TH_BUSSI; P.tauT = 2.0; P.wallAttr = 0; colorMode = 0;
        const int nm = 180;
        fillBox(palette[findPal("CH4")], nm, P.Tset); fillBox(palette[findPal("Cl2")], nm, P.Tset);
        for (int k = 0; k < 16; k++) script.push_back({1.0 + 4.0 * k, 30, false});   // УФ-вспышки каждые 4τ
        presetTitle = "Хлорирование метана (700 K, как в заводском реакторе): CH4 + Cl2 → CH3Cl + HCl; УФ-вспышки рвут только Cl2, дальше идёт радикальная цепь"; break; }
    case 32: {  // равновесие H2 + I2 ⇌ 2HI (Боденштейн)
        const int I = typeOfZ(53);
        worldReset(24, 24, 24, B_WALLS);
        P.Tset = kelvin(3500); P.thermostat = TH_BUSSI; P.tauT = 1.0; P.wallAttr = 0; colorMode = 0;
        const int nm = 250;
        fillBox(palette[findPal("H2")], nm, P.Tset); fillBox(diTmpl("I2", I, I, 1), nm, P.Tset);
        for (int k = 0; k < 20; k++) script.push_back({1.0 + 3.0 * k, 34, false});   // зелёный свет рвёт I2
        presetTitle = "H2 + I2 <=> 2HI (Боденштейн) при 3500 K и на свету: прямая и обратная реакции идут одновременно — стадия I + H2 → HI + H требует 1.4 эВ; K_c — вкладка «Химия»"; break; }
    case 33: {  // гидрирование этилена на никеле
        const int Ni = typeOfZ(28);
        worldReset(20, 20, 20, B_PERIODIC); P.Tset = kelvin(600);
        metalBall(Ni, 10, 10, 10, 2.2, P.Tset);
        P.thermostat = TH_BUSSI; P.tauT = 1.0; colorMode = 0; P.substeps = 5;
        const int n = 120;
        fillBox(libTmpl(ML_C2H4), n, P.Tset, 0.0); fillBox(palette[findPal("H2")], 2 * n, P.Tset, 0.0);
        presetTitle = "Гидрирование на никеле (600 K): C2H4 + H2 → C2H6; H2 распадается на поверхности Ni, атомы H присоединяются к этилену"; break; }
    case 34: {  // разложение пероксида водорода на платине
        const int Pt = typeOfZ(78);
        worldReset(20, 20, 20, B_PERIODIC); P.Tset = kelvin(500);
        metalBall(Pt, 10, 10, 10, 2.2, P.Tset);
        P.thermostat = TH_BUSSI; P.tauT = 1.0; colorMode = 0; P.substeps = 5;
        fillBox(libTmpl(ML_H2O2), 150, P.Tset, 0.0);
        presetTitle = "Разложение пероксида на платине (500 K): 2H2O2 → 2H2O + O2; слабая связь O–O рвётся на поверхности, атомы собираются в воду и кислород"; break; }
    case 35: {  // горение ацетилена
        combustion(24);
        const int na = 130;
        fillBox(libTmpl(ML_C2H2), na, P.Tset); fillBox(palette[findPal("O2")], na * 5 / 2, P.Tset);
        presetTitle = "Горение ацетилена: 2C2H2 + 5O2 → 4CO2 + 2H2O — смесь при 300 K, искра через 1.5τ (L — ещё вспышка)"; break; }
    case 36: {  // H2 + Br2 на свету (Боденштейн–Линд): стадия Br + H2 эндотермична — цепь медленнее хлорной
        worldReset(24, 24, 24, B_WALLS);
        P.Tset = kelvin(1800); P.thermostat = TH_BUSSI; P.tauT = 2.0; P.wallAttr = 0; colorMode = 0;
        const int nm = 220;
        fillBox(palette[findPal("H2")], nm, P.Tset); fillBox(libTmpl(ML_BR2), nm, P.Tset);
        for (int k = 0; k < 16; k++) script.push_back({1.0 + 3.0 * k, 31, false});   // вспышки видимого света каждые 3τ
        presetTitle = "H2 + Br2 → 2HBr на свету (1800 K): вспышки рвут Br2, но стадия Br + H2 → HBr + H эндотермична (0.7 эВ) — цепь идёт медленнее, чем с хлором"; break; }
    case 37: {  // хлор вытесняет бром: Cl + HBr → HCl + Br (экзотермично), Br + Br → Br2
        worldReset(24, 24, 24, B_WALLS);
        P.Tset = kelvin(300); P.thermostat = TH_BUSSI; P.tauT = 2.0; P.wallAttr = 0; colorMode = 0;
        const int nm = 240;
        fillBox(libTmpl(ML_HBR), nm, P.Tset); fillBox(palette[findPal("Cl2")], nm / 2, P.Tset);
        for (int k = 0; k < 12; k++) script.push_back({1.0 + 4.0 * k, 30, false});
        presetTitle = "Хлор вытесняет бром (300 K): Cl + HBr → HCl + Br почти без барьера — связь H–Cl прочнее H–Br; бром собирается в Br2 и BrCl (УФ-вспышки каждые 4τ)"; break; }
    case 38: {  // водород и фтор: связь F–F слабая, H–F — самая прочная; хватает нескольких атомов F
        worldReset(24, 24, 24, B_WALLS);
        P.Tset = kelvin(300); P.thermostat = TH_NVE; P.wallAttr = 0; colorMode = 0;
        const int nm = 200;
        fillBox(palette[findPal("H2")], nm, P.Tset); fillBox(libTmpl(ML_F2), nm, P.Tset);
        script.push_back({1.0, 33, false});
        presetTitle = "H2 + F2 → 2HF без искры: рассеянный свет рвёт несколько слабых связей F–F (1.6 эВ), а цепь F + H2 → HF + H, H + F2 → HF + F идёт без барьера — взрыв (H–F — 5.9 эВ)"; break; }
    case 39: {  // распад озона при нагреве
        worldReset(22, 22, 22, B_WALLS);
        P.Tset = kelvin(300); P.thermostat = TH_POWER; P.heatPower = 1.2; P.wallAttr = 0; colorMode = 0;
        fillBox(libTmpl(ML_O3), 260, P.Tset);
        presetTitle = "Распад озона при нагреве от 300 K: O3 → O2 + O, слабая связь O–O⁻ рвётся первой; атомы O соединяются в O2 или отнимают O у озона"; break; }
    // ---- ещё сцены (только из меню): вещество 40–43, химия 44–49
    case 40: {  // жидкость и пар: плёнка жидкости в вытянутом ящике испаряется до насыщения
        const double a = std::cbrt(4 / 0.8); const int m = 7;
        worldReset(40, m * a, m * a, B_PERIODIC);
        lattice3D(L_FCC, E_AR, -1, 0, 20 - m * a / 2, 0, 0, m, m, m, a, 0.85);
        P.Tset = 0.85; P.thermostat = TH_BUSSI; P.tauT = 1.0; colorMode = 5; P.substeps = 6;
        presetTitle = "Жидкость и пар: плёнка жидкости в вытянутом ящике испаряется до насыщения; плотности пара и жидкости сравниваются с фазовой диаграммой модели"; break; }
    case 41: {  // адсорбция газа на притягивающих стенках
        worldReset(18, 18, 18, B_WALLS);
        P.Tset = 1.0; P.thermostat = TH_BUSSI; P.tauT = 1.0; P.wallAttr = 1.2; colorMode = 1;
        fillBox(palette[findPal("Ar")], 500, P.Tset, 2.5);
        presetTitle = "Адсорбция: стенки притягивают атомы газа, на них растёт плёнка; покрытие θ и давление газа выходят на равновесие (изотерма Ленгмюра)"; break; }
    case 42: {  // выравнивание температур двух газов с разными массами атомов
        const int Kr = typeOfZ(36);
        worldReset(24, 24, 24, B_PERIODIC);
        P.thermostat = TH_NVE; colorMode = 0; P.substeps = 6;
        fillBox(palette[findPal("Ne")], 450, 3.0, 0.0); fillBox(atomTmpl(Kr), 450, 0.4, 0.0);
        presetTitle = "Выравнивание температур: горячий неон и холодный криптон в одном сосуде; через столкновения средняя кинетическая энергия атомов становится одинаковой, а скорости — нет"; break; }
    case 43: {  // кавитация: растянутая жидкость разрывается пузырьками
        const double a = std::cbrt(4 / 0.55); const int m = 8;
        worldReset(m * a, m * a, m * a, B_PERIODIC);
        lattice3D(L_FCC, E_AR, -1, 0, -0.25 * a, -0.25 * a, -0.25 * a, m, m, m, a, 1.4);
        P.Tset = 0.8; P.thermostat = TH_BUSSI; P.tauT = 0.5; colorMode = 5; P.substeps = 5;
        presetTitle = "Кавитация: растянутая жидкость (давление ниже нуля) разрывается пузырьками пара, давление поднимается к давлению насыщения"; break; }
    case 44: {  // растяжение нанопровода золота до разрыва
        const int Au = typeOfZ(79); const double a = 2 * EL[Au].rmet / 3.405 * std::sqrt(2.0), R = 2.6; const int nx = 14, m = (int)(R / a) + 2;
        worldReset(nx * a, 2 * R + 10, 2 * R + 10, B_PERIODIC);   // провод бесконечный: он замкнут через границу по x
        const double bs[4][3] = {{0, 0, 0}, {0.5, 0.5, 0}, {0.5, 0, 0.5}, {0, 0.5, 0.5}};
        for (int i = 0; i < nx; i++) for (int j = -m; j <= m; j++) for (int k = -m; k <= m; k++) for (auto& b : bs) {
            const double x = (i + b[0] + 0.25) * a, y = (j + b[1]) * a, z = (k + b[2]) * a;
            if (y * y + z * z > R * R) continue;
            double vx, vy, vz; thermalVel(Au, kelvin(300), vx, vy, vz); addAtom(Au, x, S.Ly / 2 + y, S.Lz / 2 + z, vx, vy, vz);
        }
        P.Tset = kelvin(300); P.thermostat = TH_BUSSI; P.tauT = 1.0; colorMode = 0; P.substeps = 8;
        for (int s = 0; s < 110; s++) script.push_back({2.0 + 1.0 * s, 35, false});   // +1.5 % длины каждые 1τ
        presetTitle = "Растяжение нанопровода золота: провод удлиняется на 1.5% каждую τ — атомы скользят, образуется шейка, затем цепочка атомов и разрыв"; break; }
    case 45: {  // горение пропана
        combustion(24);
        const int np = 65;
        fillBox(libTmpl(ML_C3H8), np, P.Tset); fillBox(palette[findPal("O2")], 5 * np, P.Tset);
        presetTitle = "Горение пропана: C3H8 + 5O2 → 3CO2 + 4H2O — смесь при 300 K, искра через 1.5τ (L — ещё вспышка)"; break; }
    case 46: {  // взрывное разложение трихлорида азота
        combustion(20); sparkR = 6; sparkTK = 4000;
        fillBox(libTmpl(ML_NCL3), 180, P.Tset);
        presetTitle = "Взрыв трихлорида азота: 2NCl3 → N2 + 3Cl2 — слабые связи N–Cl (2 эВ) рвутся от искры, выделяется энергия прочнейшей связи N≡N (9.8 эВ)"; break; }
    case 47: {  // самовоспламенение при медленном нагреве без искры
        worldReset(22, 22, 22, B_WALLS);
        P.Tset = kelvin(300); P.thermostat = TH_POWER; P.heatPower = 2.0; P.wallAttr = 0; colorMode = 0;
        fillBox(palette[findPal("H2")], 300, P.Tset);
        if (presetVariant == 0) {
            fillBox(palette[findPal("O2")], 150, P.Tset);
            // цепь здесь начинается только с распада самих H2 (4.5 эВ) или O2 (5.1 эВ): на пикосекундах — около 4500 K
            // (в настоящем сосуде её за секунды запускает медленная реакция H2 + O2 → H + HO2)
            presetTitle = "Самовоспламенение: смесь 2H2 + O2 греется без искры; за пикосекунды цепь начинается лишь около 4500 K, когда молекулы H2 и O2 рвутся сами (в сосуде за секунды — уже около 850 K). Повтор — H2 + Cl2";
        } else {
            fillBox(palette[findPal("Cl2")], 300, P.Tset);
            presetTitle = "Самовоспламенение H2 + Cl2: без света смесь устойчива, но при нагреве связь Cl–Cl (2.5 эВ) рвётся сама и идёт цепная реакция. Повтор — H2 + O2";
        }
        break; }
    }
    for (auto& o : fieldObjs) o.fromPreset = true;   // объекты сцены (сброс пересоздаёт их, объекты пользователя переносятся)
    sceneMeasureReset();
    finishPreset();
    presetLoaded = false; showToast(T(presetTitle));
}
// Загрузить сцену; keepUser — перенести объекты поля, поставленные пользователем (сброс сцены, повтор клавиши):
// объекты самого пресета создаются заново, объекты пользователя включаются плавно (ramp с нуля — атомы уже на новых местах)
static void loadPresetKeepObjs(int k, int variant, bool keepUser) {
    if (k >= 100) { loadPreset(k, variant); return; }
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
        case 2: P.Tset = 0.75; P.tauT = 3.0; showToast("Охлаждение → пар пересыщен → конденсация"); break;
        case 3: for (int i = S.n - 1; i >= 0; i--) if (S.ty[i] == E_WALL) removeAtom(i);
                updatePresence(); computeForces(); resetEnergyRef(); resetMSD(); showToast("Перегородка убрана"); break;
        case 4: case 40: {   // 40 — искра в теплоизолированном сосуде: термостат выключается
            if (e.action == 40) { P.thermostat = TH_NVE; resetEnergyRef(); }
            double c[3] = {S.Lx / 2, S.Ly / 2, S.Lz / 2}; spark(c, sparkR, sparkTK / cfg::U_T_K); showToast("Искра! Энергия разряда учтена как внешняя работа"); break; }
        case 5: P.Tset = 0.1; P.tauT = 0.08; showToast("Закалка: T → 0.05–0.1 за доли τ"); break;
        case 6: P.efield = 0.5 / cfg::U_E_VNM; showToast("Включено поле E = 0.5 В/нм → катионы дрейфуют по полю, анионы — против"); break;
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
            const double T4 = nl ? kl / (3 * nl) : 1, T1 = nr ? kr / (3 * nr) : 1;   // температуры до разрыва
            measure();
            SM::tOpen = S.t; SM::xOpen = xw; SM::T0 = EN.T; SM::Ep0 = EN.nmob ? potentialNow() / EN.nmob : 0; SM::front.clear(); SM::speedW = 0;
            for (int i = S.n - 1; i >= 0; i--) if (S.ty[i] == E_WALL) removeAtom(i);
            updatePresence(); computeForces(); resetEnergyRef(); resetMSD();
            // теория ударной трубы для идеального газа (один газ, γ = 5/3, скорости звука a = √(γkT/m)):
            //   p4/p1 = p21·[1 − (γ−1)(a1/a4)(p21 − 1)/√(2γ(2γ + (γ+1)(p21 − 1)))]^(−2γ/(γ−1)),  Ms = √(1 + (γ+1)(p21 − 1)/(2γ))
            const double g = 5.0 / 3.0, crossV = (S.Ly - 1.6) * (S.Lz - 1.6);
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
        case 32: { int c = photolyze(E_CL, E_CL, 0.5); if (c) showToast("УФ-вспышка: Cl2 → 2Cl·"); break; }   // мало хлора — рвётся половина
        case 33: { int c = photolyze(E_F, E_F, 0.03); if (c) showToast("Рассеянный свет: несколько F2 → 2F·"); break; }
        case 34: { const int I = typeOfZ(53); int c = photolyze(I, I, 0.2); if (c) showToast("Вспышка зелёного света: I2 → 2I·"); break; }
        case 35: strainX(1.015); fitView(false); break;   // растяжение провода на 1.5 %; камера плавно отъезжает
        }
    }
}
// живые измерения сцен вещества: теплопроводность, ударная волна, барометрическая формула, эффузия, спекание,
// сжатие, смачивание, кристаллизация, течение, сосуществование фаз, адсорбция, теплообмен газов, кавитация,
// растяжение провода, самовоспламенение
static void sceneMeasure() {
    const int k = currentPreset;
    if (!((k >= 21 && k <= 29) || (k >= 40 && k <= 44) || k == 47)) return;
    if (SM::tPrev >= 0 && S.t < SM::tPrev - 1e-9) sceneMeasureReset();   // время пошло назад (отмена, загрузка)
    const double dt = SM::tPrev < 0 ? 0 : S.t - SM::tPrev; SM::tPrev = S.t;
    const int B = A::TP_BINS;
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
        const double area = S.Ly * S.Lz, q = 0.5 * (SM::qh - SM::qc) / area;
        std::vector<std::pair<double, double>> pts;
        for (int b = B / 5; b < B - B / 5; b++) pts.push_back({(b + 0.5) * S.Lx / B, A::tprof[b]});
        double a0, sl; if (!fitLine(pts, a0, sl)) break;
        const double kap = sl < -1e-6 ? q / -sl : 0;
        N = fmt("стенки: приток %.2f, отток %.2f ε/τ · q = %.4f ε/(τσ%s) · dT/dx = %.4f /σ · κ = q/|dT/dx| = %.2f k/(στ) ≈ %.3f Вт/(м·К)",
                SM::qh, -SM::qc, q, "²", sl, kap, kap * 0.0406);
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
                SM::theoW, SM::theoW / std::sqrt(5.0 / 3.0 * SM::T1 / EL[E_AR].m), SM::rho21);
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
        N = T("высота однородной атмосферы ") + s + fmt(" · T одинакова по высоте: %.2f", EN.T);
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
            if (std::fabs(S.x[i] - xc) < 0.6) r.push_back(std::hypot(S.y[i] - yc, S.z[i] - zc));
        }
        std::sort(r.begin(), r.end());
        const double neck = r.size() >= 3 ? 2 * r[r.size() - 2] + 2 * EL[Au].rmet / 3.405 : 0;   // без самого дальнего (адатом)
        if (SM::V0 <= 0) SM::V0 = c[1][0] - c[0][0];   // начальное расстояние между центрами
        N = fmt("перешеек %.1fσ (%.1f нм) · центры сблизились на %.2fσ · Au в «чужой» половине %.0f%%, Ag %.0f%% · T = %.0f K",
                neck, neck * cfg::U_L_NM, SM::V0 - (c[1][0] - c[0][0]), 100.0 * mixA / n[0], 100.0 * mixB / n[1], toKelvin(EN.T));
        break; }
    case 26: {   // адиабата идеального одноатомного газа: T·V^(γ−1) = const, γ = 5/3
        if (SM::adV0 <= 0) { N = fmt("газ в равновесии с поршнем: V = %.0fσ%s, T = %.2f (%.0f K); с 2τ давление начнёт расти", boxVolume(), "³", EN.T, toKelvin(EN.T)); break; }
        const double g = 5.0 / 3.0, V = boxVolume(), vr = SM::adV0 / std::max(1e-9, V);
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
    case 40: {   // профиль плотности вдоль x относительно центра плёнки (плёнка может дрейфовать)
        const int NB = 40; double cs = 0, sn = 0; int n = 0;
        for (int i = 0; i < S.n; i++) { const double u = S.x[i] / S.Lx * 2 * PI; cs += std::cos(u); sn += std::sin(u); n++; }
        if (n < 10) { N.clear(); break; }
        const double xc = std::atan2(sn, cs) / (2 * PI) * S.Lx;
        std::vector<double> h(NB, 0.0);
        for (int i = 0; i < S.n; i++) { double u = S.x[i] - xc; u -= S.Lx * std::floor(u / S.Lx + 0.5); h[clampv((int)((u / S.Lx + 0.5) * NB), 0, NB - 1)] += 1; }
        const double binV = S.Ly * S.Lz * S.Lx / NB; double rl = 0, rg = 0; int cl = 0, cg = 0;
        for (int b = 0; b < NB; b++) { const double z = std::fabs((b + 0.5) / NB - 0.5); if (z < 0.08) { rl += h[b]; cl++; } else if (z > 0.36) { rg += h[b]; cg++; } }
        rl /= std::max(1, cl) * binV; rg /= std::max(1, cg) * binV;
        const double aw = SM::rhoL < 0 ? 1.0 : std::min(1.0, dt / 5.0);
        SM::rhoL = SM::rhoL < 0 ? rl : SM::rhoL + aw * (rl - SM::rhoL); SM::rhoG = SM::rhoG < 0 ? rg : SM::rhoG + aw * (rg - SM::rhoG);
        double tl, tg; ljBinodal(EN.T, tl, tg);
        N = fmt("жидкость ρ = %.3f (модель %.3f) · пар ρ = %.4f (модель %.4f) · T = %.3f (%.0f K) · давление пара %.3f бар",
                SM::rhoL, tl, SM::rhoG, tg, EN.T, toKelvin(EN.T), toBar(SM::rhoG * EN.T));
        break; }
    case 41: {   // адсорбированные атомы: ближе 1.6σ к какой-либо стенке
        int ads = 0, gas = 0;
        for (int i = 0; i < S.n; i++) {
            if (frozenAt(i)) continue;
            const double d = std::min({S.x[i], S.Lx - S.x[i], S.y[i], S.Ly - S.y[i], S.z[i], S.Lz - S.z[i]});
            if (d < 1.6) ads++; else gas++;
        }
        const double sites = wallArea() * 2 / std::sqrt(3.0);   // плотная упаковка монослоя: 2/(√3σ²)
        N = fmt("на стенках %d атомов, в газе %d · покрытие θ = %.0f%% монослоя · давление газа %.4f (%.0f бар) · притяжение стенок %.2f",
                ads, gas, 100.0 * ads / sites, EN.P, toBar(EN.P), P.wallAttr);
        break; }
    case 42: {   // температура каждого газа по кинетической энергии: kT = 2K/(3N)
        const int Ne = E_NE, Kr = typeOfZ(36);
        double K[2] = {0, 0}, v2[2] = {0, 0}; int c[2] = {0, 0};
        for (int i = 0; i < S.n; i++) {
            const int s = S.ty[i] == Ne ? 0 : (S.ty[i] == Kr ? 1 : -1); if (s < 0) continue;
            const double w2 = S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i];
            K[s] += 0.5 * EL[S.ty[i]].m * w2; v2[s] += w2; c[s]++;
        }
        if (!c[0] || !c[1]) { N.clear(); break; }
        const double T0 = 2 * K[0] / (3 * c[0]), T1 = 2 * K[1] / (3 * c[1]);
        N = fmt("T неона %.2f (%.0f K) · T криптона %.2f (%.0f K) · v_ср.кв. Ne / Kr = %.2f (при равных T: √(mKr/mNe) = %.2f)",
                T0, toKelvin(T0), T1, toKelvin(T1), std::sqrt(v2[0] / c[0] / std::max(1e-12, v2[1] / c[1])), std::sqrt(EL[Kr].m / EL[Ne].m));
        break; }
    case 43: {   // доля объёма без атомов: ячейки 2σ (в жидкости пустая ячейка почти невероятна)
        if (SM::P0 > 1e8) SM::P0 = EN.P;
        const int nx = std::max(1, (int)(S.Lx / 2)), ny = std::max(1, (int)(S.Ly / 2)), nz = std::max(1, (int)(S.Lz / 2));
        std::vector<unsigned char> occ((size_t)nx * ny * nz, 0);
        for (int i = 0; i < S.n; i++) {
            const int ix = clampv((int)(S.x[i] / S.Lx * nx), 0, nx - 1), iy = clampv((int)(S.y[i] / S.Ly * ny), 0, ny - 1), iz = clampv((int)(S.z[i] / S.Lz * nz), 0, nz - 1);
            occ[((size_t)iz * ny + iy) * nx + ix] = 1;
        }
        int empty = 0; for (unsigned char o : occ) if (!o) empty++;
        N = fmt("пузыри занимают %.0f%% объёма · давление %+.4f (в начале %+.4f — жидкость растянута) · T = %.2f",
                100.0 * empty / occ.size(), EN.P, SM::P0, EN.T);
        break; }
    case 44: {   // удлинение и самое тонкое сечение провода (срезы толщиной ≈ межплоскостного расстояния)
        if (SM::wireL0 <= 0) SM::wireL0 = S.Lx;
        const int NB = std::max(4, (int)(S.Lx / 0.6));
        std::vector<int> cnt(NB, 0);
        for (int i = 0; i < S.n; i++) cnt[clampv((int)(S.x[i] / S.Lx * NB), 0, NB - 1)]++;
        const int mn = *std::min_element(cnt.begin(), cnt.end()), mx = *std::max_element(cnt.begin(), cnt.end());
        N = fmt("удлинение %.0f%% · в самом тонком сечении %d атомов (в самом толстом %d)%s · T = %.0f K",
                100 * (S.Lx / SM::wireL0 - 1), mn, mx, mn == 0 ? T(" — провод разорван") : "", toKelvin(EN.T));
        break; }
    case 47: {   // момент вспышки: первые молекулы продукта
        const char* prod = presetVariant == 0 ? "H2O" : "HCl";
        int w = 0; for (auto& kv : A::mol) if (kv.first == prod) w = kv.second;
        if (SM::Tign < 0 && w >= 5) SM::Tign = EN.T;
        N = fmt("T = %.2f (%.0f K) · молекул %s: %d%s", EN.T, toKelvin(EN.T), prod, w,
                SM::Tign > 0 ? fmt(" · вспышка при T ≈ %.2f (%.0f K)", SM::Tign, toKelvin(SM::Tign)).c_str() : "");
        break; }
    }
}

// ---- Сцены меню (Tab): key — номер пресета для loadPreset.
// group — раздел меню: SG_MATTER — вещество (фазы, перенос, механика), SG_CHEM — химия и растворы,
// SG_NUCL — ядра (сцены 100–104, worlds.inl), SG_QUARK — кварки (110–114, world_quark.inl), SG_SEMI — полупроводники (120–123, world_semi.inl),
// SG_WAVE — волновая функция электрона (130–134, world_wave.inl).
enum { SG_MATTER, SG_CHEM, SG_NUCL, SG_QUARK, SG_SEMI, SG_WAVE, SG_N };
static inline int sceneBtnId(int key) { return key < 100 ? 600 + key : 2000 + key; }   // id карточки в меню сцен
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
    {40, 0, "меню", "Жидкость и пар", "плёнка жидкости испаряется до насыщения; сравнение с фазовой диаграммой", SG_MATTER},
    {41, 0, "меню", "Адсорбция", "газ оседает на притягивающих стенках: покрытие θ и давление", SG_MATTER},
    {42, 0, "меню", "Выравнивание температур", "горячий неон и холодный криптон: равная энергия, разные скорости", SG_MATTER},
    {43, 0, "меню", "Кавитация", "растянутая жидкость разрывается пузырьками пара", SG_MATTER},
    {44, 0, "меню", "Растяжение нанопровода", "провод золота тянется: скольжение, шейка, цепочка атомов, разрыв", SG_MATTER},
    {31, 0, "меню", "Хлорирование метана", "CH4 + Cl2 → CH3Cl + HCl: УФ-свет запускает радикальную цепь", SG_CHEM},
    {32, 0, "меню", "Равновесие H2 + I2 ⇌ 2HI", "опыт Боденштейна: прямая и обратная реакции, K_c", SG_CHEM},
    {33, 0, "меню", "Гидрирование на никеле", "C2H4 + H2 → C2H6: H2 распадается на поверхности Ni", SG_CHEM},
    {34, 0, "меню", "Разложение пероксида", "2H2O2 → 2H2O + O2: радикалы OH, платина", SG_CHEM},
    {35, 0, "меню", "Горение ацетилена", "2C2H2 + 5O2 → 4CO2 + 2H2O от искры", SG_CHEM},
    {36, 0, "меню", "H2 + Br2 на свету", "цепная реакция медленнее хлорной: стадия Br + H2 эндотермична", SG_CHEM},
    {37, 0, "меню", "Хлор вытесняет бром", "Cl2 + 2HBr → 2HCl + Br2: связь H–Cl прочнее H–Br", SG_CHEM},
    {38, 0, "меню", "Водород и фтор", "H2 + F2 → 2HF без искры: слабая связь F–F рвётся сама", SG_CHEM},
    {39, 0, "меню", "Распад озона", "2O3 → 3O2 при нагреве: O3 → O2 + O, атомы O собираются в O2", SG_CHEM},
    {45, 0, "меню", "Горение пропана", "C3H8 + 5O2 → 3CO2 + 4H2O от искры", SG_CHEM},
    {46, 0, "меню", "Взрыв NCl3", "2NCl3 → N2 + 3Cl2: слабые N–Cl, прочная N≡N", SG_CHEM},
    {47, 0, "меню", "Самовоспламенение", "нагрев без искры до температуры вспышки; повтор — H2 + Cl2", SG_CHEM},
    {100, 0, "меню", "Период полураспада", "ядра распадаются случайно, а их число — точно по закону 2^(−t/T½)", SG_NUCL},
    {101, 0, "меню", "Цепочка распадов радона", "Rn-222 → Po-218 → … → Pb-206: α и β, от микросекунд до лет", SG_NUCL},
    {102, 0, "меню", "Критическая масса", "шар урана-235: нейтроны деления вызывают новые деления", SG_NUCL},
    {103, 0, "меню", "Ядерный реактор", "вода замедляет нейтроны, стержни с бором держат k = 1", SG_NUCL},
    {104, 0, "меню", "Ядерный взрыв", "сверхкритическая сборка: лавина делений и разлёт", SG_NUCL},
    {110, 0, "меню", "Протон изнутри", "три цветных кварка и Y-струна глюонного поля", SG_QUARK},
    {111, 0, "меню", "Разрыв струны", "кварк не вырвать: струна рвётся, рождая новую пару", SG_QUARK},
    {112, 0, "меню", "Столкновение протонов", "струны рвутся на цепочки мезонов — струи адронов", SG_QUARK},
    {113, 0, "меню", "Конструктор адронов", "какие сочетания кварков бывают: барионы и мезоны", SG_QUARK},
    {114, 0, "меню", "Распад нейтрона изнутри", "d → u + W⁻: слабое взаимодействие меняет аромат", SG_QUARK},
    {120, 0, "меню", "Диод", "p–n переход: обеднённый слой, ток в одну сторону, формула Шокли", SG_SEMI},
    {121, 0, "меню", "Светодиод и солнечный элемент", "рекомбинация даёт свет; свет рождает пары и ток", SG_SEMI},
    {122, 0, "меню", "МОП-транзистор", "затвор открывает канал между истоком и стоком", SG_SEMI},
    {123, 0, "меню", "Биполярный транзистор", "малый ток базы управляет большим током коллектора", SG_SEMI},
    {130, 0, "меню", "Туннельный эффект", "волна электрона проходит сквозь барьер выше своей энергии", SG_WAVE},
    {131, 0, "меню", "Двойная щель", "интерференция по одному электрону; прибор у щелей её стирает", SG_WAVE},
    {132, 0, "меню", "Квантовый осциллятор", "когерентное состояние качается, как шарик на пружине", SG_WAVE},
    {133, 0, "меню", "Квантовый ковёр", "пакет в ящике: копии, дробные и полное возрождение", SG_WAVE},
    {134, 0, "меню", "Дифракция электронов", "кристалл — решётка для волн де Бройля: d·sin θ = nλ", SG_WAVE},
};
static const char* SG_NAMES[SG_N] = {"Вещество: фазы, перенос, механика", "Химия и растворы", "Ядра: распад, деление, цепная реакция", "Кварки: струны, адроны, распад", "Полупроводники: диод, светодиод, транзисторы", "Квантовый мир: волна электрона"};

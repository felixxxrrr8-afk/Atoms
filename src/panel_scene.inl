// ===================================== ВКЛАДКА «СЦЕНА»: РЕДАКТОР =================================
// Границы по осям (периодическая ось или две стенки), вид каждой стенки, форма сосуда, точная расстановка по сетке
// и заливка области веществом из палитры. Id элементов: 2200–2299.
static bool placeSnap = false;          // точная расстановка: одна молекула за щелчок в узел сетки на рабочей плоскости
static double snapStep = 1.0;           // шаг сетки (σ)
static double planeFrac = 0.5;          // высота рабочей плоскости y = planeFrac·Ly
static int fillRegion = 0;              // область заливки
static double fillDens = 2.0, fillTK = 300;   // молекул на нм³ и температура, K
static const char* FACE_NAMES[6] = {"x− левая", "x+ правая", "y− дно", "y+ верх", "z− задняя", "z+ передняя"};
static const char* REG_NAMES[7] = {"весь сосуд", "левая половина", "правая половина", "нижняя половина", "верхняя половина", "шар в центре", "слой у дна"};
// попадает ли точка в область заливки
static bool inFillRegion(double x, double y, double z) {
    const double cx = S.Lx / 2, cy = S.Ly / 2, cz = S.Lz / 2;
    switch (fillRegion) {
    case 1: return x < cx; case 2: return x >= cx; case 3: return y < cy; case 4: return y >= cy;
    case 5: { const double R = 0.3 * std::min({S.Lx, S.Ly, S.Lz}); return (x - cx) * (x - cx) + (y - cy) * (y - cy) + (z - cz) * (z - cz) < R * R; }
    case 6: return y < 0.25 * S.Ly;
    default: return true;
    }
}
// рабочая точка расстановки: пересечение луча под курсором с плоскостью y = planeFrac·Ly, привязанное к узлам сетки
static bool snapPoint(int mx, int my, double& x, double& y, double& z) {
    double o[3], d[3]; mouseRay(mx, my, o, d);
    if (std::fabs(d[1]) < 1e-6) return false;
    const double py = planeFrac * S.Ly, t = (py - o[1]) / d[1]; if (t <= 0) return false;
    x = o[0] + d[0] * t; z = o[2] + d[2] * t; y = py;
    x = std::round(x / snapStep) * snapStep; z = std::round(z / snapStep) * snapStep;
    return x > 0 && x < S.Lx && z > 0 && z < S.Lz;
}
// шаблон, который ставится сейчас: молекула библиотеки («штамп») или вещество палитры
static const Tmpl& currentTmpl() { return chemStampActive() ? palette[0] : palette[clampv(selPal, 0, (int)palette.size() - 1)]; }
static void fillRegionNow() {
    const Tmpl& m = currentTmpl(); if (m.tool == 1 || m.a.empty()) { showToast("Выберите вещество в палитре"); return; }
    pushUndo();
    // объём области (σ³) — Монте-Карло по ящику: область может быть шаром или пересекаться с сосудом
    int in = 0; const int M = 20000;
    double x0 = 1e9, y0 = 1e9, z0 = 1e9, x1 = -1e9, y1 = -1e9, z1 = -1e9;
    for (int k = 0; k < M; k++) {
        const double x = urand() * S.Lx, y = urand() * S.Ly, z = urand() * S.Lz; double d, nx, ny, nz;
        if (!inFillRegion(x, y, z) || (containerDist(x, y, z, d, nx, ny, nz) && d < 0.6)) continue;
        in++; x0 = std::min(x0, x); y0 = std::min(y0, y); z0 = std::min(z0, z); x1 = std::max(x1, x); y1 = std::max(y1, y); z1 = std::max(z1, z);
    }
    if (!in) { undoStack.pop_back(); return; }
    const double V = boxVolume() * in / M, Vnm = V * std::pow(cfg::U_L_NM, 3);
    const int want = (int)std::lround(fillDens * Vnm);
    const double T = fillTK / cfg::U_T_K;
    // плотная заливка (жидкость) — по узлам сетки, редкая (газ) — случайными точками; лишнее вне области не ставится
    const int before = S.n;
    const double cellV = V / std::max(1, want), a = std::cbrt(cellV);
    int placed = 0;
    if (a < 2.2) {
        const int nx = std::max(1, (int)((x1 - x0) / a)), ny = std::max(1, (int)((y1 - y0) / a)), nz = std::max(1, (int)((z1 - z0) / a));
        for (int k = 0; k < nz && placed < want; k++) for (int j = 0; j < ny && placed < want; j++) for (int i = 0; i < nx && placed < want; i++) {
            const double x = x0 + (i + 0.5) * (x1 - x0) / nx, y = y0 + (j + 0.5) * (y1 - y0) / ny, z = z0 + (k + 0.5) * (z1 - z0) / nz;
            if (!inFillRegion(x, y, z)) continue;
            for (int tr = 0; tr < 6; tr++) if (placeMol(m, x, y, z, T, 0.55)) { placed++; break; }
        }
    } else {
        for (int tr = 0; tr < want * 40 && placed < want; tr++) {
            const double x = x0 + urand() * (x1 - x0), y = y0 + urand() * (y1 - y0), z = z0 + urand() * (z1 - z0);
            if (inFillRegion(x, y, z) && placeMol(m, x, y, z, T, 0.85)) placed++;
        }
    }
    if (S.n != before) { updatePresence(); relaxContacts(20); computeForces(); resetEnergyRef(); }
    showToast(fmt("Залито: %d из %d (%.3g на нм³ в %.1f нм³)", placed, want, fillDens, Vnm));
}
static void clearRegionNow() {
    pushUndo(); int c = 0;
    std::vector<char> gone(S.n, 0); std::vector<int> mol;
    for (int i = 0; i < S.n; i++) if (!gone[i] && !EL[S.ty[i]].fixed && inFillRegion(S.x[i], S.y[i], S.z[i])) { moleculeOf(i, mol); for (int b : mol) gone[b] = 1; }
    for (int i = S.n - 1; i >= 0; i--) if (gone[i]) { removeAtom(i); c++; }
    updatePresence(); nlValid = false; computeForces(); resetEnergyRef();
    showToast(fmt("Убрано атомов: %d", c));
}
// убрать молекулы, задевшие отталкивающий слой стенки (ближе 0.86σ — минимума её потенциала): иначе каждая такая
// молекула получает от стенки сотни ε и разогревает всё вокруг. dist(i) — расстояние атома i до ближайшей новой стенки
static int dropNearWall(const std::function<double(int)>& dist) {
    std::vector<char> gone(S.n, 0); std::vector<int> mol; int c = 0;
    for (int i = 0; i < S.n; i++) {
        if (gone[i] || EL[S.ty[i]].fixed || dist(i) >= 0.86 * EL[S.ty[i]].sig) continue;
        moleculeOf(i, mol); for (int b : mol) gone[b] = 1;
    }
    for (int i = S.n - 1; i >= 0; i--) if (gone[i]) { removeAtom(i); c++; }
    return c;
}
// периодическая ось k становится двумя стенками. Молекулы, разрезанные границей, собираются целиком (обход по связям,
// каждый атом — ближайшим образом к соседу) и центром возвращаются в ящик; у сетки через всю ось (кристалл) связи
// на границе рвутся. Возвращает число убранных атомов
static int splitAxis(int k) {
    const double L = k == 0 ? S.Lx : (k == 1 ? S.Ly : S.Lz);
    std::vector<double>& v = k == 0 ? S.x : (k == 1 ? S.y : S.z);
    std::vector<char> done(S.n, 0); std::vector<int> q;
    for (int i = 0; i < S.n; i++) {
        if (done[i]) continue;
        q.assign(1, i); done[i] = 1;
        for (size_t h = 0; h < q.size(); h++) {
            const int a = q[h];
            for (int p = 0; p < S.nbc[a]; p++) {
                const int b = S.nb[a][p]; if (done[b]) continue;
                double d = v[b] - v[a]; d -= L * std::nearbyint(d / L); v[b] = v[a] + d; done[b] = 1; q.push_back(b);
            }
        }
        double lo = 1e30, hi = -1e30, c = 0;
        for (int a : q) { lo = std::min(lo, v[a]); hi = std::max(hi, v[a]); c += v[a]; }
        if (hi - lo < 0.5 * L) { const double sh = -L * std::floor(c / q.size() / L); for (int a : q) v[a] += sh; continue; }
        for (int a : q) v[a] -= L * std::floor(v[a] / L);
        for (int a : q) for (int p = S.nbc[a] - 1; p >= 0; p--) {
            const int b = S.nb[a][p];
            if (std::fabs(v[b] - v[a]) > 0.5 * L) { while (bonded(a, b)) changeBond(a, b, -1); updateCharge(a); updateCharge(b); }
        }
    }
    return dropNearWall([&](int i) { return std::min(v[i], L - v[i]); });
}
static void setAxisPeriodic(int k, bool per) {
    pushUndo();
    if (P.boundary == B_PISTON) { if (!per) return; P.boundary = B_WALLS; P.perMask = 0; }
    const bool was = perAx(k);
    if (per) P.perMask |= 1 << k; else P.perMask &= ~(1 << k);
    P.boundary = P.perMask == 7 ? B_PERIODIC : B_WALLS;
    const int removed = was && !per ? splitAxis(k) : 0;
    if (P.npt && P.perMask != 7) P.npt = false;
    updatePresence(); nlValid = false; computeForces(); resetEnergyRef();
    if (removed) showToast(fmt("У новых стенок убрано атомов: %d", removed));
}
// смена сосуда: молекулы снаружи или вплотную к его стенке убираются целиком
static void setContainer(int ct) {
    P.container = clampv(ct, 0, CT_N - 1);
    const int removed = dropNearWall([](int i) { double d, nx, ny, nz; return containerDist(S.x[i], S.y[i], S.z[i], d, nx, ny, nz) ? d : 1e30; });
    updatePresence(); nlValid = false; computeForces(); resetEnergyRef();
    if (removed) showToast(fmt("Вне сосуда убрано атомов: %d", removed));
}
static void drawSceneTab(float x, float y, float w, float h) {
    float yy = scrollBegin(7, x, y, w - uiPx(8), h);
    const float W = w - uiPx(8), bh = uiPx(26), sh = uiPx(30), g = uiPx(4);
    auto slider = [&](int id, const char* label, double* v, double lo, double hi, bool logs, const std::string& val, const char* hint) {
        const bool ch = uiSlider(id, x, yy, W, sh - uiPx(4), label, v, lo, hi, logs, val, hint); yy += sh; return ch;
    };
    uiSection(x, yy, W, "Границы по осям");
    {
        const float cw = std::floor((W - 2 * g) / 3);
        static const char* AX[3] = {"x периодична", "y периодична", "z периодична"};
        for (int k = 0; k < 3; k++) {
            bool per = perAx(k);
            if (uiCheck(2200 + k, x + k * (cw + g), yy, cw, uiPx(22), AX[k], &per, "Периодическая ось: вышедший с одной стороны атом входит с другой.\nИначе — две стенки, их вид задаётся ниже")) setAxisPeriodic(k, per);
        }
        yy += uiPx(26);
        if (P.boundary == B_PISTON) yy += drawWrapped(fontXS, x, yy, W, "Сейчас границы «поршень» (вкладка «Управление»): верхняя стенка — поршень.", C_DIM) + g;
    }
    uiSection(x, yy, W, "Стенки");
    {
        bool any = false;
        for (int f = 0; f < 6; f++) {
            if (perAx(f / 2)) continue; any = true;
            if (uiCycle(2210 + f, x, yy, W, bh, FACE_NAMES[f], WT_NAMES[clampv(P.wallType[f], 0, WT_N - 1)], P.wallType[f] != WT_SOFT,
                        "Обычная — притяжение как у ползунка «смачивание стенок»; липкая — сильно притягивает (адсорбция);\nтепловая — отражённый атом получает скорость при её температуре;\nпоглощает — ударившая молекула исчезает (сток); зеркальная — упругое отражение без притяжения")) {
                pushUndo(); P.wallType[f] = (P.wallType[f] + 1) % WT_N; computeForces(); resetEnergyRef();
            }
            yy += bh + g;
            if (P.wallType[f] == WT_THERMAL) {
                double TK = toKelvin(P.wallTK[f]);
                if (slider(2220 + f, "  температура стенки", &TK, 20, 3000, true, fmt("%.0f K", TK), "Температура тепловой стенки")) P.wallTK[f] = TK / cfg::U_T_K;
            }
        }
        if (!any) yy += drawWrapped(fontXS, x, yy, W, "Все оси периодические — стенок нет. Снимите галочку у оси, чтобы появились её две стенки.", C_DIM) + g;
        if (uiCycle(2230, x, yy, W, bh, "сосуд", CT_NAMES[clampv(P.container, 0, CT_N - 1)], P.container != CT_BOX, "Форма сосуда внутри ящика: шар или цилиндр вдоль y (стенка — как обычная).\nС поршнем недоступен: поршень меняет высоту ящика")) {
            if (P.boundary == B_PISTON) showToast("С поршнем сосуд недоступен — смените границы на вкладке «Управление»");
            else { pushUndo(); setContainer((P.container + 1) % CT_N); }
        }
        yy += bh + g;
        if (absorbedCount > 0) { drawText(fontXS, x, yy, fmt("поглощено стенками: %lld атомов", absorbedCount), C_DIM); yy += fontXS.h + g; }
    }
    uiSection(x, yy, W, "Расстановка");
    {
        if (uiCheck(2240, x, yy, W, uiPx(22), "точно по сетке (инструмент «добавить»)", &placeSnap,
                    "Щелчок ставит одну молекулу выбранного вещества в узел сетки на рабочей плоскости;\nпризрак показывает, куда она встанет. Shift+протяжка — ряд молекул")) { if (placeSnap && lmbTool != TOOL_ADD) lmbTool = TOOL_ADD; }
        yy += uiPx(26);
        slider(2241, "шаг сетки", &snapStep, 0.25, 3, true, fmt("%.2fσ · %.2f нм", snapStep, realNm(snapStep)), "Расстояние между узлами сетки расстановки");
        slider(2242, "высота рабочей плоскости", &planeFrac, 0.02, 0.98, false, fmt("%.0f%% ящика", planeFrac * 100), "Горизонтальная плоскость, на которую ставятся молекулы");
    }
    uiSection(x, yy, W, "Заполнить область");
    {
        if (uiCycle(2250, x, yy, W, bh, "область", REG_NAMES[fillRegion], false, "Куда налить выбранное в палитре вещество (или молекулу из библиотеки)")) fillRegion = (fillRegion + 1) % 7;
        yy += bh + g;
        slider(2251, "плотность", &fillDens, 0.02, 40, true, fmt("%.3g на нм³", fillDens), "Число молекул на кубический нанометр.\nГаз при 1 атм — 0.025, жидкая вода — 33, жидкий аргон — 21");
        slider(2252, "температура", &fillTK, 20, 3000, true, fmt("%.0f K", fillTK), "Начальная температура наливаемого вещества");
        const float w2 = std::floor((W - g) / 2);
        if (uiButton(2253, x, yy, w2, bh, "залить", false, false, "Налить вещество из палитры в выбранную область")) fillRegionNow();
        if (uiButton(2254, x + w2 + g, yy, W - w2 - g, bh, "очистить область", false, false, "Убрать из области все молекулы (кроме стенок)")) clearRegionNow();
        yy += bh + g;
        yy += drawWrapped(fontXS, x, yy, W, fmt("вещество: %s", T(currentTmpl().label.c_str())), C_DIM) + uiPx(8);
    }
    scrollEnd(7, yy + uiPx(8));
}
// призрак молекулы и сетка на рабочей плоскости (рисуется поверх сцены, когда включена точная расстановка)
static void drawPlacementGuide() {
    if (!placeSnap || lmbTool != TOOL_ADD || !inScene(mouseX, mouseY) || world != W_MD) return;
    double px, py, pz; if (!snapPoint(mouseX, mouseY, px, py, pz)) return;
    glEnable(GL_LINE_SMOOTH);
    col(withA(C_TEXT, 0.18f)); glBegin(GL_LINES);
    const int R = 6;
    for (int k = -R; k <= R; k++) {
        const double gx = px + k * snapStep, gz = pz + k * snapStep;
        line3(gx, py, pz - R * snapStep, gx, py, pz + R * snapStep); line3(px - R * snapStep, py, gz, px + R * snapStep, py, gz);
    }
    glEnd();
    const Tmpl& m = currentTmpl();
    {
        MonoAtoms colored;
        for (auto& a : m.a) {
            float sx, sy, dd, s; if (!project(px + a.x, py + a.y, pz + a.z, sx, sy, dd, s)) continue;
            const float r = (float)(0.5 * visSig(a.t) * s * atomVis);
            glColor4f(EL[a.t].r, EL[a.t].g, EL[a.t].b, 0.25f); discPx(sx, sy, r, 24);
            glColor4f(EL[a.t].r, EL[a.t].g, EL[a.t].b, 0.8f); circlePx(sx, sy, r, 32);
        }
    }
    glDisable(GL_LINE_SMOOTH);
}

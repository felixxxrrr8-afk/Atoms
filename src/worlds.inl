// ===================================== МИРЫ: ДИСПЕТЧЕР =======================================
// Сцены с номерами от 100 — модели других масштабов: ядра и нейтроны (100–104), кварки (110–114),
// носители заряда в полупроводнике (120–123), волновая функция частицы (130–134). У каждого мира свой шаг,
// рисунок и панель справа; молекулярная динамика в это время стоит (атомов в ящике нет).
// id элементов панелей миров: 1800–1999.
static int worldToolSaved = -1;
static void worldLoad(int k, int variant) {
    (void)variant;
    const int prev = world;
    if (k >= 100 && k < 105) { world = W_NUC; nucReset(k - 100); }
    else if (k >= 120 && k < 124) {   // повтор той же сцены (R) сохраняет напряжения
        if (prev != W_SEMI || currentPreset != k) scDefaults(k - 120);
        world = W_SEMI; scReset(k - 120);
    }
    else if (k >= 130 && k < 135) {   // повтор той же сцены (R) сохраняет настройки ползунков
        if (prev != W_WAVE || currentPreset != k) wvDefaults(k - 130);
        world = W_WAVE; wvReset(k - 130);
    }
    else { world = W_MD; loadPreset(1, 0); return; }
    static const char* TITLES[] = {
        "Радиоактивный распад: каждое ядро распадается случайно, а их число падает по закону N = N0·2^(−t/T½)",
        "Цепочка распадов радона-222: α- и β-распады до стабильного свинца-206, периоды от микросекунд до лет",
        "Критическая масса: в шаре урана-235 нейтроны деления вызывают новые деления — растёт ли их число?",
        "Ядерный реактор: вода замедляет нейтроны, стержни с бором поглощают лишние — цепная реакция идёт ровно",
        "Ядерный взрыв: сверхкритическая сборка — число делений растёт экспоненциально, пока шар не разлетится"};
    presetTitle = world == W_WAVE ? WV_TITLES[wv::scene] : (world == W_SEMI ? SC_TITLES[sc::scene] : TITLES[clampv(nuc::scene, 0, 4)]);
    currentPreset = k; presetVariant = 0; presetLoaded = false;
    if (prev == W_MD) worldToolSaved = lmbTool;
    lmbTool = TOOL_CAMERA; P.paused = false; viewFitPending = true; followAtom = -1; flashes.clear();
    fieldObjs.clear(); selFieldObj = -1; script.clear(); A::sceneNote.clear();
    showToast(T(presetTitle));
}
// выход в молекулярную динамику: вернуть инструмент
static void worldLeave() { if (worldToolSaved >= 0) { lmbTool = worldToolSaved; worldToolSaved = -1; } }
static void worldStep(double frameDt) {
    switch (world) { case W_NUC: nucStep(frameDt); break; case W_WAVE: wvStep(frameDt); break; case W_SEMI: scStep(frameDt); break; }
}
static void worldDraw() {
    switch (world) { case W_NUC: nucDraw(); break; case W_WAVE: wvDraw(); return; case W_SEMI: scDraw(); return; }
    // масштаб и подпись мира в левом нижнем углу
    const float x = sceneX + uiPx(12), y = sceneY + sceneH - uiPx(22);
    const double pps = pxPerSigma();
    const char* unit = world == W_NUC ? (nuc::scene <= 1 ? nullptr : "см") : nullptr;
    if (unit) {
        const double L = niceStep(uiPx(110) / std::max(1e-9, pps)); const float lp = (float)(L * pps);
        rectFill(x, y, lp, 1, withA(C_TEXT, 0.85f)); rectFill(x, y - uiPx(4), 1, uiPx(5), withA(C_TEXT, 0.85f)); rectFill(x + lp - 1, y - uiPx(4), 1, uiPx(5), withA(C_TEXT, 0.85f));
        drawText(fontXS, x + lp / 2 - uiPx(10), y - uiPx(4) - fontXS.h, fmt("%g %s", L, T(unit)), withA(C_TEXT, 0.85f));
    } else if (world == W_NUC) drawText(fontXS, x, y - fontXS.h, "ядра увеличены: на самом деле между ними в 100 000 раз больше места", C_DIM);
}
static std::string worldClock() {
    switch (world) { case W_NUC: return nucWorldTime(); case W_WAVE: return wvTimeStr(); case W_SEMI: return scTimeStr(); }
    return "";
}

// ---- панель мира ядер
static void nucPanel(float x, float y, float w, float h) {
    using namespace nuc;
    rectFill(x, y, w, h, C_PANEL); lineV(x, y, y + h, C_LINE);
    const float pad = uiPx(10), cx = x + pad, cw = w - 2 * pad;
    float yy = scrollBegin(6, cx, y + uiPx(6), cw, h - uiPx(10));
    const float bh = uiPx(26), sh = uiPx(30);
    drawText(fontL, cx, yy, "Ядра и нейтроны", C_TEXT_HI); yy += fontL.h + uiPx(6);
    // показания
    auto row = [&](const char* k, const std::string& v) { drawText(fontU, cx, yy, k, C_DIM); drawTextR(fontM, cx + cw, yy - uiPx(1), v, C_TEXT_HI); yy += fontU.h + uiPx(5); };
    row("время", nucWorldTime());
    if (scene <= 1) {
        int alive = 0; for (auto& n : nuclei) if (n.iso >= 0 && ISO[n.iso].mode != D_STABLE) alive++;
        const int iso = scene == 0 ? isoPick : 7; int left = 0; for (auto& n : nuclei) if (n.iso == iso) left++;
        row("изотоп", fmt("%s · T½ = %s", ISO[iso].name, nucTimeStr(ISO[iso].T12).c_str()));
        row("осталось исходных", fmt("%d из %d", left, N0));
        row("по закону распада", fmt("%.0f", N0 * std::pow(2.0, -t / ISO[iso].T12)));
        row("ещё радиоактивны", fmt("%d", alive));
    } else {
        row("нейтронов", neutronsW < 1e4 ? fmt("%.0f", neutronsW) : fmt("%.2e", neutronsW));
        row("делений", fissions < 1e4 ? fmt("%.0f", fissions) : fmt("%.2e", fissions));
        row("энергия", nucEnergyStr(energyMeV));
        const double k = nucK(); row("k (по поколениям)", k > 0 ? fmt("%.2f", k) : std::string("—"));
        if (scene == 4) row("радиус сборки", fmt("%.2f см", exploding ? expandR : sphereR));
    }
    yy += uiPx(4);
    // управление
    uiSection(cx, yy, cw, "управление");
    if (uiButton(1800, cx, yy, cw / 2 - uiPx(3), bh, "заново (R)", false, false, "Начать сцену заново")) nucReset(scene);
    if (uiButton(1801, cx + cw / 2 + uiPx(3), yy, cw / 2 - uiPx(3), bh, P.paused ? "пуск (пробел)" : "пауза (пробел)", P.paused, false, "Остановить или запустить время")) P.paused = !P.paused;
    yy += bh + uiPx(6);
    {   // скорость времени — логарифмический ползунок
        double lg = std::log10(timeScale);
        const double lo = scene <= 1 ? -6 : -1, hi = scene <= 1 ? 17 : 6;
        const std::string v = scene <= 1 ? fmt("%s за 1 с", nucTimeStr(timeScale).c_str()) : fmt("%s за 1 с", nucTimeStr(timeScale * 1e-9).c_str());
        if (uiSlider(1802, cx, yy, cw, sh - uiPx(4), "скорость времени", &lg, lo, hi, false, v, "Сколько времени мира проходит за секунду на экране")) timeScale = std::pow(10.0, lg);
        yy += sh;
    }
    if (scene == 0) {
        static const int PICK[] = {16, 18, 21, 23, 25, 7, 14, 6, 0};
        int cur = 0; for (int q = 0; q < 9; q++) if (PICK[q] == isoPick) cur = q;
        if (uiCycle(1803, cx, yy, cw, bh, "изотоп", fmt("%s (%s)", ISO[isoPick].name, ISO[isoPick].mode == D_ALPHA ? "α" : ISO[isoPick].mode == D_BETAP ? "β+" : ISO[isoPick].mode == D_GAMMA ? "γ" : "β−"), false,
                    "I-131 (медицина, 8 сут), Cs-137 (30 лет), C-14 (радиоуглерод, 5730 лет), Co-60, F-18 (томография), радон, полоний, радий, уран-238"))
        { isoPick = PICK[(cur + 1) % 9]; nucReset(0); }
        yy += bh + uiPx(6);
    }
    if (scene == 2 || scene == 4) {
        double R = sphereR, e = enrich * 100;
        if (uiSlider(1804, cx, yy, cw, sh - uiPx(4), "радиус шара", &R, 3, 16, false, fmt("%.1f см · %.0f кг", R, 19.05 * 4.0 / 3 * PI * R * R * R / 1000), "Критический радиус голого шара U-235 ≈ 8.7 см (≈52 кг)")) { sphereR = R; nucRestart(); }
        yy += sh;
        if (uiSlider(1805, cx, yy, cw, sh - uiPx(4), "обогащение U-235", &e, 3, 100, false, fmt("%.0f%%", e), "Доля урана-235: остальное — U-238, который почти не делится медленными и средними нейтронами")) { enrich = e / 100; nucRestart(); }
        yy += sh;
    }
    if (scene == 3) {
        double r = rodIns * 100, e = enrich * 100;
        if (uiSlider(1806, cx, yy, cw, sh - uiPx(4), "стержни (бор) введены", &r, 0, 100, false, fmt("%.0f%%", r), "Карбид бора поглощает тепловые нейтроны (B-10: 3840 барн): глубже стержни — меньше k")) { rodIns = r / 100; std::vector<Part> keep = parts; nucReactorBuild(); parts = keep; }
        yy += sh;
        if (uiSlider(1807, cx, yy, cw, sh - uiPx(4), "обогащение U-235", &e, 2, 30, false, fmt("%.1f%%", e),
                     "Маленькой зоне исследовательского реактора нужно топливо обогащением ≈ 20%,\nбольшой зоне АЭС (высота 3.5 м) хватает 3–5%")) { enrich = e / 100; std::vector<Part> keep = parts; nucReactorBuild(); parts = keep; }
        yy += sh;
        if (uiCheck(1808, cx, yy, cw, uiPx(22), "вода-замедлитель", &moderator, "Без воды нейтроны остаются быстрыми, а быстрые почти не делят U-235 — реактор гаснет")) { std::vector<Part> keep = parts; nucReactorBuild(); parts = keep; }
        yy += uiPx(26);
    }
    if (scene >= 2 && uiButton(1809, cx, yy, cw, bh, "впустить нейтроны из источника", false, false, "Несколько нейтронов из центра (как от радий-бериллиевого источника)")) nucSource(scene == 3 ? 30 : 8);
    if (scene >= 2) yy += bh + uiPx(8);
    // график
    uiSection(cx, yy, cw, scene <= 1 ? "число ядер N(t)" : "число нейтронов (логарифм)");
    {
        PR in{cx, yy, cw, uiPx(130)}; boxPanel(in.x, in.y, in.w, in.h, C_PANEL2, C_LINE);
        if (scene <= 1 && !histN.empty()) {
            drawSeries(in, histTheory, 0, std::max(1, N0), withA(C_DIM, 0.9f), 1.2f, LS_DASH);
            drawSeries(in, histN, 0, std::max(1, N0), C_TEXT_HI, 1.5f, LS_SOLID);
            drawText(fontXS, in.x + uiPx(4), in.y + in.h + uiPx(2), "сплошная — модель, пунктир — закон 2^(−t/T½)", C_DIM);
        } else if (!histNeut.empty()) {
            double lo = 0, hi = 1; for (float v : histNeut) hi = std::max(hi, (double)v + 0.5);
            drawSeries(in, histNeut, lo, hi, C_TEXT_HI, 1.5f, LS_SOLID);
            drawText(fontXS, in.x + uiPx(4), in.y + in.h + uiPx(2), fmt("прямая линия вверх — экспоненциальный рост (k > 1); до 10^%.0f", hi), C_DIM);
        }
        yy += in.h + fontXS.h + uiPx(10);
    }
    static const char* NOTES[] = {
        "Ядро не «стареет»: вероятность распада за секунду одна и та же, сколько бы оно ни прожило. Поэтому за каждый период полураспада остаётся половина. "
        "Альфа-распад уносит ядро гелия (2 протона + 2 нейтрона), бета-распад превращает нейтрон в протон с вылетом электрона и антинейтрино, гамма — лишняя энергия ядра в виде фотона.",
        "Радон-222 — газ из урановых руд. Его дочерние ядра живут минуты, полоний-214 — 164 микросекунды, а свинец-210 — 22 года: цепочка «застревает» на нём. "
        "Замедлите или ускорьте время ползунком — видно, как населённость переходит от одного изотопа к другому.",
        "Деление U-235 даёт 2–3 нейтрона и около 200 МэВ — в 50 миллионов раз больше, чем горение одного атома углерода. "
        "Нейтрону нужно встретить ядро U-235, пока он не вылетел наружу: в маленьком шаре большинство улетает (k < 1), в большом — хватает на цепную реакцию (k > 1).",
        "Медленные (тепловые) нейтроны делят U-235 в сотни раз охотнее быстрых — поэтому в реакторе топливо окружено водой: на лёгких ядрах водорода нейтрон теряет энергию за десяток столкновений. "
        "Цвет нейтрона: белый — быстрый, синий — тепловой. Стержни с бором поглощают нейтроны и держат k ≈ 1.",
        "Сборка больше критической: k ≈ 2, число делений удваивается за поколение (около 10 нс). Энергия делений греет и расталкивает металл, "
        "плотность падает — и цепная реакция гаснет раньше, чем разделится весь уран."};
    yy += drawWrapped(fontXS, cx, yy, cw, NOTES[clampv(scene, 0, 4)], C_DIM) + uiPx(12);
    scrollEnd(6, yy);
}
static void worldPanel(float x, float y, float w, float h) {
    switch (world) { case W_NUC: nucPanel(x, y, w, h); break; case W_WAVE: wvPanel(x, y, w, h); break; case W_SEMI: scPanel(x, y, w, h); break; }
}
// ---- полоса под сценой: легенда цветов мира
static void worldStrip(float x, float y, float w, float h) {
    rectFill(x, y, w, h, C_PANEL); lineH(x, x + w, y, C_LINE);
    pushClip(x, y, w, h);
    float bx = x + uiPx(12); const float cy = y + h / 2;
    auto sw = [&](const char* label, RGBA c, bool line = false) {
        { MonoAtoms colored; if (line) rectFill(bx, cy - uiPx(1), uiPx(14), uiPx(2), c); else { col(c); discPx(bx + uiPx(5), cy, uiPx(5), 16); } }
        bx += uiPx(line ? 20.0f : 14.0f); bx += drawText(fontXS, bx, cy - fontXS.h / 2, label, C_TEXT) + uiPx(16);
    };
    if (world == W_NUC) {
        bx += drawText(fontXS, bx, cy - fontXS.h / 2, "цвет:", C_DIM) + uiPx(10);
        sw("протон", {0.95f, 0.22f, 0.20f, 1}); sw("нейтрон ядра", {0.62f, 0.66f, 0.72f, 1});
        sw("быстрый нейтрон", {1.0f, 1.0f, 1.0f, 1}); sw("тепловой нейтрон", {0.35f, 0.55f, 1.0f, 1});
        sw("электрон β−", {0.4f, 0.6f, 1.0f, 1}, true); sw("позитрон β+", {1.0f, 0.45f, 0.8f, 1}, true);
        sw("гамма-квант", {1.0f, 0.95f, 0.4f, 1}, true); sw("антинейтрино", {0.5f, 0.5f, 0.5f, 1}, true);
    }
    if (world == W_SEMI) {
        bx += drawText(fontXS, bx, cy - fontXS.h / 2, "цвет:", C_DIM) + uiPx(10);
        sw("электрон", {0.45f, 0.80f, 1.0f, 1}); sw("дырка", {1.0f, 0.55f, 0.25f, 1});
        sw("n-область, ионы доноров +", {0.25f, 0.4f, 0.85f, 1}); sw("p-область, ионы акцепторов −", {0.85f, 0.35f, 0.25f, 1});
        if (sc::Brad > 0 || (sc::scene == 1 && sc::mode121 == 1)) sw("фотон", {1.0f, 0.9f, 0.3f, 1}, true);
    }
    if (world == W_WAVE) {
        if (wv::scene == 3) bx += drawText(fontXS, bx, cy - fontXS.h / 2, "яркость — плотность вероятности |ψ(x, t)|²: по горизонтали — ящик, сверху вниз — время", C_DIM);
        else {
            bx += drawText(fontXS, bx, cy - fontXS.h / 2, "яркость — вероятность |ψ|², цвет — фаза ψ:", C_DIM) + uiPx(10);
            for (int q = 0; q < 4; q++) { float r, g, b; wvHue(q * PI / 2, r, g, b); static const char* PH[4] = {"0", "π/2", "π", "3π/2"}; sw(PH[q], {r, g, b, 1}); }
            sw("барьер, стенка", {0.6f, 0.6f, 0.6f, 1});
            bx += drawText(fontXS, bx, cy - fontXS.h / 2, "щелчок по волне — измерение положения", C_DIM);
        }
    }
    popClip();
}
static std::string worldSubtitle() {
    switch (world) {
    case W_NUC: return nuc::scene <= 1 ? "мир ядер · время — реальные периоды полураспада, ускорено ползунком справа · мышь: вращать, колесо — масштаб"
                                       : "мир ядер · перенос нейтронов Монте-Карло: реальные сечения и плотности · размеры — в сантиметрах";
    case W_WAVE: return "квантовый мир · уравнение Шрёдингера для электрона на сетке 256×256 · единицы: нм, фс, эВ";
    case W_SEMI: return "полупроводник · дрейф и диффузия носителей, уравнение Пуассона; параметры кремния при 300 K · размеры — микрометры";
    }
    return "";
}
// краткий отчёт для самопроверки (--selftest)
static std::string worldReport() {
    using namespace nuc;
    if (world == W_WAVE) return wvReport();
    if (world == W_SEMI) return scReport();
    if (world != W_NUC) return "";
    if (scene <= 1) {
        const int iso = scene == 0 ? isoPick : 7; int left = 0; for (auto& n : nuclei) if (n.iso == iso) left++;
        return fmt("t = %s · %s осталось %d из %d (закон распада: %.0f)", nucWorldTime().c_str(), ISO[iso].name, left, N0, N0 * std::pow(2.0, -t / ISO[iso].T12));
    }
    return fmt("t = %s · нейтронов %.3g · делений %.3g · k = %.2f · энергия %s", nucWorldTime().c_str(), neutronsW, fissions, nucK(), nucEnergyStr(energyMeV).c_str());
}

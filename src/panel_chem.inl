// ===================================== ПАНЕЛЬ «ХИМИЯ» =====================
// Рисуется внутри вкладки боковой панели; элементы — из ui.inl, цвета — палитра C_* (render.inl).
// id элементов: 1200–1399, кнопки библиотеки — 1400 + номер структуры. Прокручиваемая область № 4.
static std::string chemSelEq;          // выбранная реакция журнала (для кинетики); пусто — самая частая
static bool chemLibAll = false;        // показать всю библиотеку (иначе — первые строки)

// выбрать структуру библиотеки: она становится первой ячейкой палитры, инструмент — «добавить»
static void selectLibMolecule(int idx) {
    Tmpl m;
    if (!buildLibTmpl(idx, m)) return;
    chemStampMol = idx; palette[0] = m; selPal = 0;
    if (lmbTool != TOOL_ADD) lmbTool = TOOL_ADD;
    showToast(fmt("%s: %s. ЛКМ в сцене — вставить", T(MOL_LIB_NAMES[idx]), T(MOL_LIB_DESC[idx])));
}
// скорость реакции → константа скорости: k = r / (V·Π c_i), c — число частиц на σ³
//   real: n = 1 — 1/с; n = 2 — л/(моль·с); n = 3 — л²/(моль²·с)
static bool rxRateConst(const RxStat& s, double rate, double& kSim, double& kReal, int& order) {
    order = (int)s.R.size(); if (order < 1) return false;
    const double V = boxVolume(); double prod = 1;
    for (auto& f : s.R) {
        int n = 0; for (auto& c : chemComp.list) if (c.first == f) { n = c.second; break; }
        if (n <= 0) return false;
        prod *= n / V;
    }
    kSim = rate / V / prod;
    const double sig3L = std::pow(cfg::U_L_NM * 1e-9, 3) * 1e3, tauS = cfg::U_T_PS * 1e-12;
    kReal = kSim * std::pow(sig3L * 6.02214076e23, order - 1) / tauS;
    return true;
}
// концентрационная константа K_c = Π[продукты] / Π[реагенты] (моль/л)
static bool rxKc(const RxStat& s, double& K, int& dn) {
    auto conc = [&](const std::string& f) { for (auto& c : chemComp.list) if (c.first == f) return molPerL(c.second); return 0.0; };
    double num = 1, den = 1;
    for (auto& f : s.Pr) { double c = conc(f); if (c <= 0) return false; num *= c; }
    for (auto& f : s.R) { double c = conc(f); if (c <= 0) return false; den *= c; }
    K = num / den; dn = (int)s.Pr.size() - (int)s.R.size(); return true;
}
static void drawChemPanel(float x, float y, float w, float h) {
    float yy = scrollBegin(4, x, y, w - uiPx(8), h);
    const float W = w - uiPx(8), sh = uiPx(34), bh = uiPx(24), g = uiPx(4), rh = fontS.h + uiPx(3);
    static int tick = 0;
    if (tick++ % 10 == 0 || chemComp.t < 0 || chemComp.t > S.t) chemComposition(chemComp);
    auto kv = [&](const std::string& k, const std::string& v, const std::string& u = "", RGBA vc = C_TEXT_HI) {
        drawText(fontXS, x, yy + uiPx(1), k, C_DIM);
        float vx = x + std::floor(W * 0.36f), vw = drawText(fontS, vx, yy, v, vc);
        if (!u.empty()) { pushClip(vx, yy, W - (vx - x), rh); drawText(fontXS, vx + vw + uiPx(6), yy + uiPx(1), u, C_DIM); popClip(); }
        yy += rh;
    };
    // ---------------- реакции: переключатели и барьеры
    uiSection(x, yy, W, "Реакции");
    {
        const float ch = uiPx(22);
        uiCheck(1200, x, yy, W, ch, "химические реакции (связи рвутся и образуются)", &P.chemistry,
                "Ассоциация радикалов, обмен атомами, диссоциация перерастянутых связей.\nЭнергия каждого события сохраняется точно");
        yy += ch;
        uiCheck(1201, x, yy, W, ch, "кислоты и основания (перенос протона)", &P.acidBase,
                "H3O+ + H2O ⇌ H2O + H3O+ (механизм Гроттгуса), HCl + H2O → H3O+ + Cl−,\nH3O+ + OH− → 2H2O, NH3 + H3O+ → NH4+ + H2O. Самоионизация воды не моделируется");
        yy += ch;
        uiCheck(1202, x, yy, W, ch, "катализ поверхностью металла (Pt, Pd, Ni, Fe…)", &P.surfCat,
                "Молекулы, касающиеся атомов Fe, Co, Ni, Cu, Ru, Rh, Pd, Ag, Re, Os, Ir, Pt,\nреагируют с барьером ×0.3 (только кинетическая часть: не ниже ΔH)");
        yy += ch + g;
        float w1 = std::floor((W - g) / 2);
        if (uiButton(1203, x, yy, w1, bh, P.catalyst ? "зона катализа: вкл" : "зона катализа", P.catalyst, false,
                     "Зона-катализатор в центре ящика: барьеры ×0.25 внутри круга\n(K — поставить под курсор)")) {
            P.catalyst = !P.catalyst; P.catX = S.Lx / 2; P.catY = S.Ly / 2; P.catZ = S.Lz / 2; }
        if (uiButton(1204, x + w1 + g, yy, W - w1 - g, bh, "сбросить журнал", false, false, "Очистить журнал реакций и счётчики скоростей")) {
            rxStats.clear(); rxRecent.clear(); rxT0 = S.t; }
        yy += bh + g;
        if (P.catalyst) { uiSlider(1205, x, yy, W, sh - uiPx(4), "радиус зоны катализа", &P.catR, 1.0, 20.0, true, fmt("%.1fσ · %.2f нм", P.catR, P.catR * cfg::U_L_NM),
                                   "Радиус круга (шара), где барьеры реакций уменьшены в 4 раза"); yy += sh; }
        uiSlider(1206, x, yy, W, sh - uiPx(4), "барьеры реакций ×", &P.eaScale, 0, 3, false, fmt("%.2f", P.eaScale),
                 "Множитель собственных барьеров реакций (1 — как в природе).\nМеняет скорости, но не равновесие; ниже ΔH барьер не опускается");
        yy += sh;
        kv("события", fmt("%lld / %lld / %lld", CH.assoc, CH.exch, CH.diss), "ассоц. / обмен / дисс.");
        if (chemPT) kv("перенос H+", fmt("%lld", chemPT), "включая прыжки по Гроттгусу");
        kv("теплота", fmt("%+.2f эВ", rxEV(CH.heat)), fmt("%+.1f ε выделено всего", CH.heat), CH.heat > 0 ? C_WARN : C_TEXT_HI);
        yy += g;
    }
    // ---------------- свет: длина волны вспышки L
    uiSection(x, yy, W, "Свет (клавиша L)");
    {
        const double E = photonEV(lightNm);
        uiSlider(1207, x, yy, W, sh - uiPx(4), "длина волны λ", &lightNm, 100, 1000, true, fmt("%.0f нм · %s", lightNm, T(lightBand(lightNm))),
                 "L — вспышка вдоль луча под курсором (ширина — как у кисти).\nКвант hν = hc/λ рвёт связь, только если молекула его поглощает и его хватает на разрыв:\nкрасный свет Cl2 не разложит, сколько ни свети, а синий — разложит");
        yy += sh;
        kv("квант hν", fmt("%.2f эВ", E));
        yy += drawWrapped(fontXS, x, yy, W, "Распадаются от света короче: I2 805 нм, Br2 630, Cl2 495, F2 400, H2O2 300, O2 243, H2O 177, CH4 144, H2 112 нм", C_DIM) + g;
    }
    // ---------------- кислотность
    uiSection(x, yy, W, "Кислотность (pH)");
    {
        bool water = false; double pH = chemPH(chemComp, &water);
        const int nH = chemComp.nH3O, nOH = chemComp.nOH;
        if (!water && nH == 0 && nOH == 0) yy += drawWrapped(fontXS, x, yy, W, "pH определяется для водных растворов (в сцене нет воды). Сцены «Кислота в воде», «Нейтрализация» — меню сцен.", C_DIM) + g;
        else {
            // шкала 0…14: от тёмного (кислая) к светлому (щелочная), риски — каждая единица, у 7 — длинная
            const float bw = W, bhh = uiPx(10);
            glDisable(GL_TEXTURE_2D); glBegin(GL_QUADS);
            for (int k = 0; k < 14; k++) {
                auto cc = [](double p) { return grayc((float)(0.18 + 0.72 * p / 14.0), 0.95f); };
                RGBA c0 = cc(k), c1 = cc(k + 1); float x0 = x + bw * k / 14, x1 = x + bw * (k + 1) / 14;
                glColor4f(c0.r, c0.g, c0.b, c0.a); glVertex2f(x0, yy); glColor4f(c1.r, c1.g, c1.b, c1.a); glVertex2f(x1, yy);
                glVertex2f(x1, yy + bhh); glColor4f(c0.r, c0.g, c0.b, c0.a); glVertex2f(x0, yy + bhh);
            }
            glEnd();
            rectLine(x, yy, bw, bhh, C_LINE);
            for (int k = 1; k < 14; k++) lineV(x + bw * k / 14, yy + (k == 7 ? 0 : bhh - uiPx(3)), yy + bhh, withA(C_PANEL, 0.9f));
            float px = x + bw * (float)clampv(pH, 0.0, 14.0) / 14;
            rectFill(px - uiPx(2), yy - uiPx(3), uiPx(5), bhh + uiPx(6), C_SCENE); rectFill(px - uiPx(1), yy - uiPx(3), uiPx(3), bhh + uiPx(6), C_TEXT_HI);
            yy += bhh + uiPx(2);
            drawText(fontXS, x, yy, "0", C_FAINT); drawTextC(fontXS, x + bw / 2, yy, "7", C_FAINT); drawTextR(fontXS, x + bw, yy, "14", C_FAINT);
            yy += fontXS.h + uiPx(3);
            std::string kind = pH < 6.5 ? "кислая среда" : pH > 7.5 ? "щелочная среда" : "нейтральная";
            kv("pH", nH == nOH ? std::string("7.0") : fmt("%.2f", pH), kind, C_TEXT_HI);
            kv("H3O+", fmt("%d", nH), fmt("%.3g моль/л", molPerL(nH)));
            kv("OH−", fmt("%d", nOH), fmt("%.3g моль/л", molPerL(nOH)));
            kv("вода", fmt("%d", chemComp.nWater), fmt("%.1f моль/л · V = %.3g л", molPerL(chemComp.nWater), boxLiters()));
            yy += drawWrapped(fontXS, x, yy, W, "В ящике объёмом ~10⁻²³ л один ион — это ~0.1 моль/л (pH ≈ 1): pH считается по избытку H3O+ или OH−. Вода сама не ионизуется (Kw в такой малой системе — ноль ионов).", C_DIM) + g;
        }
    }
    // ---------------- библиотека молекул
    uiSection(x, yy, W, "Библиотека молекул и структур");
    {
        const int cols = std::max(3, (int)(W / uiPx(66)));
        const float cw = std::floor((W - (cols - 1) * g) / cols), chh = uiPx(22);
        // разделы; в свёрнутом виде — по одной строке из каждого (выбранная структура видна всегда)
        for (int gi = 0; gi < MLG_N; gi++) {
            const int k0 = ML_GROUP_START[gi], k1 = ML_GROUP_START[gi + 1];
            drawText(fontXS, x, yy, fmt("%s (%d)", T(MLG_NAMES[gi]), k1 - k0), C_DIM); yy += fontXS.h + uiPx(2);
            int shown = chemLibAll ? k1 - k0 : std::min(k1 - k0, cols);
            if (!chemLibAll && chemStampActive() && chemStampMol >= k0 + shown && chemStampMol < k1) shown = std::min(k1 - k0, (chemStampMol - k0) / cols * cols + cols);
            for (int q = 0; q < shown; q++) {
                const int k = k0 + q;
                float bx = x + (q % cols) * (cw + g), by = yy + (q / cols) * (chh + g);
                bool on = chemStampActive() && chemStampMol == k;
                bool chemLabel = true; for (const char* c = MOL_LIB_NAMES[k]; *c; c++) if ((unsigned char)*c >= 0x80) chemLabel = false;
                if (uiButton(1400 + k, bx, by, cw, chh, MOL_LIB_NAMES[k], on, chemLabel, MOL_LIB_DESC[k])) selectLibMolecule(k);
            }
            yy += ((shown + cols - 1) / cols) * (chh + g) + uiPx(2);
        }
        float w1 = std::floor((W - g) / 2);
        if (uiButton(1259, x, yy, W, uiPx(20), chemLibAll ? "свернуть" : fmt("вся библиотека (%d)", (int)ML_N), false, false, "Показать все структуры: молекулы, ионы, кристаллы, кластеры металлов")) chemLibAll = !chemLibAll;
        yy += uiPx(20) + g;
        if (chemStampActive()) {
            yy += drawWrapped(fontXS, x, yy, W, fmt("Выбрано: %s — %s. ЛКМ в сцене (инструмент «добавить») вставляет со случайной ориентацией.", T(MOL_LIB_NAMES[chemStampMol]), T(MOL_LIB_DESC[chemStampMol])), C_TEXT) + g;
            if (uiButton(1260, x, yy, w1, bh, "в центр ящика", false, false, "Вставить выбранную структуру в центр ящика")) {
                pushUndo(); if (!insertMolecule(chemStampMol, S.Lx / 2, S.Ly / 2, S.Lz / 2)) { undoStack.pop_back(); showToast("Нет места в центре — освободите место или вставьте ЛКМ"); } }
            if (uiButton(1261, x + w1 + g, yy, W - w1 - g, bh, "5 шт. в случайные места", false, false, "Вставить пять копий в случайные свободные места")) {
                pushUndo(); int c = 0;
                for (int t = 0; t < 60 && c < 5; t++) if (insertMolecule(chemStampMol, S.Lx * urand(), S.Ly * urand(), S.Lz * urand())) c++;
                if (!c) undoStack.pop_back();
                showToast(fmt("Вставлено: %d", c)); }
            yy += bh + g;
        } else yy += drawWrapped(fontXS, x, yy, W, "Щёлкните структуру, затем ЛКМ в сцене. Длины связей и углы — реальные (равновесие модели).", C_DIM) + g;
    }
    // ---------------- состав
    uiSection(x, yy, W, fmt("Состав смеси: %d частиц", chemComp.nMol));
    {
        int tot = 0; for (auto& c : chemComp.list) tot += c.second;
        const int top = std::min(10, (int)chemComp.list.size());
        for (int q = 0; q < top; q++) {
            auto& c = chemComp.list[q];
            RGBA col = C_SERIES[q % 12];
            float frac = tot ? (float)c.second / tot : 0;
            rectFill(x, yy + uiPx(2), std::max(1.0f, W * frac), fontS.h - uiPx(1), withA(col, 0.14f));
            rectFill(x, yy + uiPx(2), uiPx(2), fontS.h - uiPx(1), col);
            pushClip(x, yy, W * 0.45f, rh); drawChem(fontS, x + uiPx(6), yy, c.first, C_TEXT_HI); popClip();
            drawTextR(fontS, x + W * 0.62f, yy, fmt("%d", c.second), C_TEXT);
            drawTextR(fontXS, x + W, yy + uiPx(1), fmt("%.3g моль/л", molPerL(c.second)), C_DIM);
            yy += rh;
        }
        if ((int)chemComp.list.size() > top) { drawText(fontXS, x, yy, fmt("… ещё %d веществ", (int)chemComp.list.size() - top), C_DIM); yy += fontXS.h + uiPx(2); }
        yy += g;
    }
    // ---------------- журнал реакций
    uiSection(x, yy, W, "Журнал реакций");
    std::vector<std::pair<long long, const std::string*>> rs;
    for (auto& kv2 : rxStats) rs.push_back({kv2.second.count, &kv2.first});
    std::sort(rs.begin(), rs.end(), [](const std::pair<long long, const std::string*>& a, const std::pair<long long, const std::string*>& b) { return a.first > b.first; });
    if (!chemSelEq.empty() && rxStats.find(chemSelEq) == rxStats.end()) chemSelEq.clear();
    const std::string selEq = !chemSelEq.empty() ? chemSelEq : (rs.empty() ? std::string() : *rs[0].second);
    const double win = 10.0;
    {
        if (rs.empty()) yy += drawWrapped(fontXS, x, yy, W, anyBondable ? "Реакций пока не было (искра — клавиша L, нагрев — ПКМ)." : "В сцене нет веществ, способных реагировать.", C_DIM) + g;
        const float rowH = fontXS.h * 2 + uiPx(6);
        for (int q = 0; q < (int)rs.size() && q < 10; q++) {
            const std::string& eq = *rs[q].second; const RxStat& st = rxStats[eq];
            const bool sel = eq == selEq;
            if (uiButton(1300 + q, x, yy, W, rowH, "", sel, false, "Выбрать реакцию: её константа скорости и K — ниже")) chemSelEq = eq;
            pushClip(x + uiPx(4), yy, W - uiPx(8), rowH);
            drawEquation(fontXS, x + uiPx(6), yy + uiPx(2), eq, sel ? C_TEXT_HI : C_TEXT);
            RGBA hc = st.dH < 0 ? C_HOT : C_COLD;   // экзо — ярко, эндо — приглушённо
            float lx = x + uiPx(6), ly = yy + uiPx(3) + fontXS.h;
            lx += drawText(fontXS, lx, ly, fmt("×%lld", st.count), C_ACC) + uiPx(10);
            lx += drawText(fontXS, lx, ly, fmt("ΔH %+.0f кДж/моль (%+.2f эВ)", rxKJ(st.dH), rxEV(st.dH)), hc) + uiPx(10);
            drawText(fontXS, lx, ly, fmt("%.3g /τ", rxRate(st, win)), C_DIM);
            popClip();
            yy += rowH + uiPx(2);
        }
        yy += g;
    }
    // ---------------- кинетика выбранной реакции
    if (!selEq.empty()) {
        uiSection(x, yy, W, "Кинетика и равновесие");
        const RxStat& st = rxStats[selEq];
        pushClip(x, yy, W, fontXS.h + uiPx(4)); drawEquation(fontXS, x, yy, selEq, C_TEXT_HI); popClip();
        yy += fontXS.h + uiPx(4);
        double w0 = 0, r = rxRate(st, win, &w0);
        kv("скорость реакции", fmt("%.3g соб./τ", r), fmt("за %.1fτ · %.3g соб./пс", w0, r / cfg::U_T_PS));
        double kS, kR; int ord;
        if (rxRateConst(st, r, kS, kR, ord)) {
            const char* ru = T(ord == 1 ? "1/с" : ord == 2 ? "л/(моль·с)" : "л²/(моль²·с)");
            kv("k", fmt("%.3g", kS), fmt("σ^%d/τ · %.3g %s (порядок %d)", 3 * (ord - 1), kR, ru, ord));
        } else kv("k", "—", "нет реагентов в смеси");
        // обратная реакция и константа равновесия
        std::string rev;
        for (size_t k = 0; k < st.Pr.size(); k++) { if (k) rev += " + "; rev += st.Pr[k]; }
        rev += " → "; for (size_t k = 0; k < st.R.size(); k++) { if (k) rev += " + "; rev += st.R[k]; }
        auto it = rxStats.find(rev);
        if (it != rxStats.end()) {
            double rr = rxRate(it->second, win);
            kv("обратная", fmt("%.3g соб./τ", rr), rr > 0 ? fmt("r₊/r₋ = %.2f%s", r / rr, std::fabs(r / rr - 1) < 0.3 ? T(" — близко к равновесию") : "") : std::string());
        } else kv("обратная", "не наблюдалась");
        double K; int dn;
        if (rxKc(st, K, dn)) kv("K_c", fmt("%.3g", K), dn == 0 ? std::string("по текущим концентрациям") : fmt("(моль/л)^%+d, по текущим концентрациям", dn));
        if (A::arrFit) kv("Ea (Аррениус)", fmt("%.2f эВ", rxEV(A::arrEa)), fmt("%.0f кДж/моль, все реакции", rxKJ(A::arrEa)));
        else yy += drawWrapped(fontXS, x, yy, W, "Энергия активации по Аррениусу: меняйте T (реакции при разных T) — вкладка «Графики».", C_DIM);
        yy += g;
    }
    // ---------------- атом под курсором
    {
        int i = followAtom >= 0 && followAtom < S.n ? followAtom : hoverAtom();
        if (i >= 0 && i < S.n) {
            uiSection(x, yy, W, "Атом: заряды и окисление");
            const Element& e = EL[S.ty[i]];
            kv("атом", fmt("%s #%d", e.sym, i), e.name);
            kv("частичный заряд", fmt("%+.3f e", S.q[i]), S.nbc[i] ? "инкременты связей + EEM иона" : "");
            kv("формальный заряд", fmt("%+d", formalCharge(i)));
            kv("степень окисления", fmt("%+d", oxidationState(i)));
            if (e.Z) kv("χ / η", fmt("%.2f / %.2f", e.chi, ETA[S.ty[i]]), "Полинг / I − A, эВ");
            if (S.nbc[i]) kv("молекула", molFormula(i));
            yy += g;
        }
    }
    scrollEnd(4, yy + uiPx(8));
}

// ===================================== ПРОВЕРКИ ХИМИИ БЕЗ ОКНА (--chemtest) ===================
// atoms.exe --chemtest K N [var] — прогон сцены K (N шагов): состав с зарядами, pH, реакции со средними ΔH,
// баланс энергии каждого события (как --evcheck); отчёт chem_K_vV.log.
// atoms.exe --chemtest lib — вставка каждой структуры библиотеки в пустой ящик и 400 шагов (устойчивость, дрейф).
static int chemTestMain() {
    const wchar_t* cmd = GetCommandLineW();
    const wchar_t* p0 = cmd ? wcsstr(cmd, L"--chemtest") : nullptr;
    if (!p0) return 0;
    wchar_t* p = (wchar_t*)p0 + 10;
    while (*p == L' ') p++;
    if (wcsncmp(p, L"lib", 3) == 0) {
        initBondTable(); initKlm(); initPalette();
        FILE* f = fopen("chem_lib.log", "w"); if (!f) ExitProcess(1);
        for (int k = 0; k < ML_N; k++) {
            worldReset(30, 30, 30, B_PERIODIC); P.thermostat = TH_NVE; P.Tset = 0.3; updatePresence();
            Tmpl m; bool built = buildLibTmpl(k, m);
            bool ok = built && insertMolecule(k, 15, 15, 15);
            if (!ok) { fprintf(f, "%-14s built=%d inserted=0\n", MOL_LIB_NAMES[k], (int)built); continue; }
            computeForces(); measure(); double E0 = EN.total(), Ep0 = EN.ebond + EN.enb; int nb0 = 0; for (int i = 0; i < S.n; i++) nb0 += S.nbc[i];
            double qs = 0; for (int i = 0; i < S.n; i++) qs += S.q[i];
            for (int s = 0; s < 400; s++) mdStep();
            measure(); int nb1 = 0; for (int i = 0; i < S.n; i++) nb1 += S.nbc[i];
            chemComposition(chemComp);
            std::string comp; for (size_t q = 0; q < chemComp.list.size() && q < 4; q++) comp += fmt(" %s:%d", chemComp.list[q].first.c_str(), chemComp.list[q].second);
            fprintf(f, "%-14s N=%3d bonds %d→%d  Q=%+.2f  Epot0/N=%.3f  E drift=%.2e  T=%.3f  events a/e/d=%lld/%lld/%lld  |%s\n", MOL_LIB_NAMES[k], S.n, nb0 / 2, nb1 / 2, qs,
                    Ep0 / std::max(1, S.n), (EN.total() - Wext - E0) / std::max(1.0, std::fabs(E0)), EN.T, CH.assoc, CH.exch, CH.diss, comp.c_str());
            fflush(f);
        }
        fclose(f); ExitProcess(0);
    }
    int K = (int)wcstol(p, &p, 10), N = (int)wcstol(p, &p, 10), var = (int)wcstol(p, &p, 10);
    initBondTable(); initKlm(); initPalette(); loadPreset(K, var);
    FILE* f = fopen(fmt("chem_%d_v%d.log", K, var).c_str(), "w"); if (!f) ExitProcess(1);
    fprintf(f, "%s  N=%d\n", presetTitle.c_str(), S.n);
    gEvLog = f; gEvCheck = true; gEvSum = 0;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int s = 0; s <= N; s++) {
        runScript(); mdStep(); flashes.clear();
        if (s % std::max(1, N / 16) == 0) {
            measure(); chemComposition(chemComp);
            double sec = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
            bool water; double pH = chemPH(chemComp, &water);
            double drift = (EN.total() - Wext - Eref) / std::max(1.0, std::fabs(Eref)) * 100;
            fprintf(f, "t=%6.2f T=%.3f P=%.3f dt=%.4f drift=%.4f%% ΣΔE_событий=%.2e a/e/d=%lld/%lld/%lld PT=%lld H3O+=%d OH-=%d pH=%.2f (%.0f шаг/с)\n   mol:",
                    S.t, EN.T, EN.P, P.dt, drift, gEvSum, CH.assoc, CH.exch, CH.diss, chemPT, chemComp.nH3O, chemComp.nOH, pH, s / std::max(1e-9, sec));
            for (size_t q = 0; q < chemComp.list.size() && q < 12; q++) fprintf(f, " %s:%d", chemComp.list[q].first.c_str(), chemComp.list[q].second);
            std::vector<std::pair<long long, std::string>> rs; for (auto& kv : rxStats) rs.push_back({-kv.second.count, kv.first}); std::sort(rs.begin(), rs.end());
            for (size_t q = 0; q < rs.size() && q < 8; q++) { const RxStat& st = rxStats[rs[q].second]; fprintf(f, "\n   rx ×%lld  %s  ΔH=%+.2f эВ (%+.0f кДж/моль)  r=%.3g/τ", -rs[q].first, rs[q].second.c_str(), rxEV(st.dH), rxKJ(st.dH), rxRate(st, 10)); }
            fprintf(f, "\n   PT gates: class %lld dist %lld vn %lld Ea %lld |", ptGate[0], ptGate[1], ptGate[2], ptGate[3]);
            for (int c = 0; c < 12; c++) if (ptDbgN[c]) fprintf(f, " [d%d b%d n=%lld ok=%lld <ΔU>=%.2f min=%.2f]", c / 3, c % 3, ptDbgN[c], ptDbgOk[c], ptDbgSum[c] / ptDbgN[c], ptDbgMin[c]);
            fprintf(f, "\n"); fflush(f);
        }
    }
    fclose(f); gEvLog = nullptr; gEvCheck = false;
    ExitProcess(0);
    return 0;
}
static const int chemTestHook = chemTestMain();

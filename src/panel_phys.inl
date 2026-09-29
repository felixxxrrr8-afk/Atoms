// ===================================== ПАНЕЛЬ «ФИЗИКА» ===================
// Рисуется внутри вкладки боковой панели; элементы — из ui.inl, цвета — палитра C_* (render.inl).
// id элементов: 1000–1199. Прокручиваемая область № 3.
static bool physShowLog = true;
// мини-диаграмма T–ρ модели: бинодаль, тройная линия, линии плавления, критическая точка, трасса системы
static void drawPhaseDiagram(float x, float y, float w, float h) {
    const LJRef r = ljRef();
    double Tr, rr; reducedState(Tr, rr);
    const int pure = pureLJType();
    double Tmax = std::max(1.45, std::min(Tr * 1.1, 4.0)), Rmax = std::max(1.12, std::min(rr * 1.08, 1.6));
    PR in = plotFrame({x, y, w, h}, pure >= 0 ? fmt("фазовая диаграмма T–ρ (%s, LJ 2.5σ)", EL[pure].sym) : "фазовая диаграмма T–ρ (LJ 2.5σ, аргон)",
                      fmt("T*c %.3g", r.Tc));
    auto X = [&](double rho) { return mapX(in, rho, 0, Rmax); };
    auto Y = [&](double T) { return mapY(in, T, 0, Tmax); };
    pushClip(in.x, in.y, in.w, in.h);
    // область «газ + жидкость» (заливка) и бинодаль
    std::vector<std::array<double, 3>> bn;   // T, ρ_г, ρ_ж
    for (int k = 0; k <= 60; k++) {
        double T = r.Tt + (r.Tc - r.Tt) * (1 - std::pow(1 - k / 60.0, 2.0)); if (k == 60) T = r.Tc - 1e-6;
        double rl, rg; ljBinodal(T, rl, rg); bn.push_back({T, rg, rl});
    }
    // заливки: «газ + жидкость» — 8 %, «твёрдое + жидкость» — 16 %, твёрдое — 4 % белого
    glDisable(GL_TEXTURE_2D); col(withA(C_ACC, 0.08f)); glBegin(GL_TRIANGLE_STRIP);
    for (auto& b : bn) { glVertex2f(X(b[1]), Y(b[0])); glVertex2f(X(b[2]), Y(b[0])); }
    glEnd();
    // твёрдое + жидкость и твёрдое (заливка справа)
    col(withA(C_WARN, 0.16f)); glBegin(GL_TRIANGLE_STRIP);
    for (int k = 0; k <= 20; k++) { double T = r.Tt + (Tmax - r.Tt) * k / 20.0; glVertex2f(X(ljFreeze(T)), Y(T)); glVertex2f(X(ljMelt(T)), Y(T)); }
    glEnd();
    col(withA(C_WARN, 0.04f)); glBegin(GL_TRIANGLE_STRIP);
    for (int k = 0; k <= 20; k++) { double T = Tmax * k / 20.0; glVertex2f(X(ljMelt(T)), Y(T)); glVertex2f(X(Rmax), Y(T)); }
    glEnd();
    glEnable(GL_LINE_SMOOTH); glLineWidth(1.3f);
    col(grayc(0.75f, 0.9f)); glBegin(GL_LINE_STRIP);   // бинодаль — серая сплошная
    for (auto& b : bn) glVertex2f(X(b[1]), Y(b[0]));
    for (int k = (int)bn.size() - 1; k >= 0; k--) glVertex2f(X(bn[k][2]), Y(bn[k][0]));
    glEnd();
    col(withA(C_HOT, 0.85f)); lineStyle(LS_DASH);   // плавление / кристаллизация — белый штрих
    glBegin(GL_LINE_STRIP); for (int k = 0; k <= 12; k++) { double T = r.Tt + (Tmax - r.Tt) * k / 12.0; glVertex2f(X(ljFreeze(T)), Y(T)); } glEnd();
    glBegin(GL_LINE_STRIP); for (int k = 0; k <= 24; k++) { double T = Tmax * k / 24.0; glVertex2f(X(ljMelt(T)), Y(T)); } glEnd();
    lineStyle(LS_SOLID);
    // тройная линия
    { double rl, rg; ljBinodal(r.Tt, rl, rg); col(withA(C_TEXT, 0.6f)); glEnable(GL_LINE_STIPPLE); glLineStipple(2, 0x3333);
      glBegin(GL_LINES); glVertex2f(X(rg), Y(r.Tt)); glVertex2f(X(ljMelt(r.Tt)), Y(r.Tt)); glEnd(); glDisable(GL_LINE_STIPPLE); }
    glLineWidth(1); glDisable(GL_LINE_SMOOTH);
    // критическая точка
    rectFill(X(r.rc) - uiPx(2), Y(r.Tc) - uiPx(2), uiPx(5), uiPx(5), C_TEXT_HI);
    drawText(fontXS, X(r.rc) + uiPx(5), Y(r.Tc) - fontXS.h, "К", C_DIM);
    drawText(fontXS, X(0.02), Y(r.Tt) + uiPx(2), fmt("тройная %.2f", r.Tt), C_DIM);
    // подписи областей
    drawText(fontXS, X(0.03), Y(r.Tt * 0.45), "газ", C_DIM);
    double rl, rg; ljBinodal(0.5 * (r.Tt + r.Tc), rl, rg); drawTextC(fontXS, X(0.5 * (rl + rg)), Y(0.5 * (r.Tt + r.Tc)) - fontXS.h / 2, "г + ж", C_DIM);
    drawTextR(fontXS, X(Rmax) - uiPx(3), Y(Tmax * 0.3), "твёрдое", C_DIM);
    if (Tmax > r.Tc * 1.15) drawTextC(fontXS, X(0.18 * Rmax), Y(0.5 * (r.Tc + Tmax)) - fontXS.h / 2, "флюид", C_DIM);
    // трасса и текущая точка
    const auto& tr = PH::trace;
    if (tr.size() > 1) {
        double sT = 1, sR = 1;
        if (pure >= 0) { sT = 1 / (EL[pure].eps * P.epsScale); double s = EL[pure].sig; sR = s * s * s; }
        glEnable(GL_LINE_SMOOTH); glBegin(GL_LINE_STRIP);
        for (size_t k = 0; k < tr.size(); k++) { float a = 0.15f + 0.75f * k / tr.size(); glColor4f(C_TEXT_HI.r, C_TEXT_HI.g, C_TEXT_HI.b, a); glVertex2f(X(tr[k].first * sR), Y(tr[k].second * sT)); }
        glEnd(); glDisable(GL_LINE_SMOOTH);
    }
    float px = X(rr), py = Y(Tr);
    // текущее состояние: белый квадрат в чёрной обводке с чёрной точкой в центре
    rectFill(px - uiPx(5), py - uiPx(5), uiPx(11), uiPx(11), C_SCENE); rectFill(px - uiPx(4), py - uiPx(4), uiPx(9), uiPx(9), C_TEXT_HI);
    rectFill(px - uiPx(1), py - uiPx(1), uiPx(3), uiPx(3), C_SCENE);
    popClip();
    // оси
    drawText(fontXS, in.x + uiPx(2), in.y + in.h - fontXS.h - uiPx(1), "0", C_FAINT);
    drawTextR(fontXS, in.x + in.w - uiPx(2), in.y + in.h - fontXS.h - uiPx(1), fmt("ρ* %.1f", Rmax), C_FAINT);
    drawText(fontXS, in.x + uiPx(2), in.y + uiPx(1), fmt("T* %.2f", Tmax), C_FAINT);
}
static void drawPhysPanel(float x, float y, float w, float h) {
    float yy = scrollBegin(3, x, y, w - uiPx(8), h);
    const float W = w - uiPx(8), sh = uiPx(34), bh = uiPx(24), g = uiPx(4), rh = fontS.h + uiPx(3);
    const bool per = isPer();
    auto kv = [&](const std::string& k, const std::string& v, const std::string& u = "", RGBA vc = C_TEXT_HI) {
        drawText(fontXS, x, yy + uiPx(1), k, C_DIM);
        float vx = x + std::floor(W * 0.36f), vw = drawText(fontS, vx, yy, v, vc);
        if (!u.empty()) { pushClip(vx, yy, W - (vx - x), rh); drawText(fontXS, vx + vw + uiPx(6), yy + uiPx(1), u, C_DIM); popClip(); }
        yy += rh;
    };
    // ---------------- агрегатное состояние
    uiSection(x, yy, W, "Агрегатное состояние");
    {
        float bw = W, bhh = uiPx(12);
        double tot = phaseFrac[0] + phaseFrac[1] + phaseFrac[2];
        rectFill(x, yy, bw, bhh, C_ELEM);
        if (tot > 0.01) {
            MonoAtoms atomsColored;   // доли фаз — цветами режима раскраски «фаза» (легенда цвета атомов)
            float xx = x;
            for (int k = 0; k < 3; k++) { float ww = (float)(bw * phaseFrac[k] / tot); rectFill(xx, yy, ww, bhh, withA(PHASE_C[k], 0.85f)); xx += ww; }
        }
        rectLine(x, yy, bw, bhh, C_LINE);
        yy += bhh + uiPx(3);
        float cw = W / 3;
        for (int k = 0; k < 3; k++) {
            { MonoAtoms atomsColored; rectFill(x + k * cw, yy + uiPx(4), uiPx(8), uiPx(8), PHASE_C[k]); }
            drawText(fontXS, x + k * cw + uiPx(12), yy + uiPx(1), fmt("%s %.0f%%", T(PH_NAMES[k]), 100 * phaseFrac[k]), C_TEXT);
        }
        yy += rh + uiPx(2);
        kv("состояние", A::phase);
        double Tr, rr; reducedState(Tr, rr);
        const int pure = pureLJType();
        kv("по диаграмме", ljRegion(Tr, rr), pure >= 0 ? fmt("T*=%.3f ρ*=%.3f", Tr, rr) : std::string("(для LJ-вещества)"));
        if (PH::latentS != 0) kv("ΔH плавл.", fmt("%+.2f ε/ат", PH::latentS), fmt("%+.2f кДж/моль", toKJmol(PH::latentS)));
        if (PH::latentG != 0) kv("ΔH испар.", fmt("%+.2f ε/ат", PH::latentG), fmt("%+.2f кДж/моль", toKJmol(PH::latentG)));
        yy += g;
    }
    // ---------------- фазовая диаграмма
    {
        float ph = std::floor(std::min(W * 0.78f, uiPx(230)));
        drawPhaseDiagram(x, yy, W, ph);
        uiRegister(1000, x, yy, W, ph);
        if (uiHover(x, yy, W, ph))
            setHot(1000, "Фазовая диаграмма модели (LJ с обрезкой 2.5σ):\nсерая сплошная — бинодаль жидкость–пар, белые штриховые — плавление/кристаллизация,\n"
                         "мелкий пунктир — тройная линия, К — критическая точка. Белый квадрат с точкой — текущее состояние,\n"
                         "светлая линия — его путь. Для чистого вещества — в его собственных σ и ε");
        yy += ph + g;
        yy += drawWrapped(fontXS, x, yy, W, fmt("Модель аргона: T*c = %.3f (%.0f K, опыт 150.7 K), тройная T* ≈ %.2f (%.0f K, опыт 83.8 K).",
                                                    ljRef().Tc, toKelvin(ljRef().Tc), ljRef().Tt, toKelvin(ljRef().Tt)), C_DIM) + g;
    }
    // ---------------- журнал переходов
    uiSection(x, yy, W, fmt("Журнал переходов: %d", (int)phaseLog.size()));
    if (phaseLog.empty()) yy += drawWrapped(fontXS, x, yy, W, "Переходов пока не было. Нагрейте или охладите вещество (термостат «нагрев P» даёт плато T(t) — теплоту перехода).", C_DIM) + g;
    else {
        int shown = 0;
        for (int k = (int)phaseLog.size() - 1; k >= 0 && shown < (physShowLog ? 8 : 2); k--, shown++) {
            rectFill(x, yy + uiPx(2), uiPx(2), fontXS.h, k == (int)phaseLog.size() - 1 ? C_ACC : C_LINE_H);
            yy += drawWrapped(fontXS, x + uiPx(7), yy, W - uiPx(7), phaseLog[k], k == (int)phaseLog.size() - 1 ? C_TEXT_HI : C_TEXT) + uiPx(3);
        }
        float w2 = std::floor((W - g) / 2);
        if (uiButton(1001, x, yy, w2, bh, physShowLog ? "короче" : "подробнее", false, false, "Показать 2 или 8 последних записей")) physShowLog = !physShowLog;
        if (uiButton(1002, x + w2 + g, yy, W - w2 - g, bh, "очистить журнал", false, false, "Очистить журнал фазовых переходов")) phaseLog.clear();
        yy += bh + g;
    }
    // ---------------- баростат
    uiSection(x, yy, W, "Баростат NPT (C-rescale)");
    {
        bool on = P.npt;
        if (uiCheck(1010, x, yy, W, uiPx(22), "поддерживать давление (стохастический C-rescale)", &on,
                    "Баростат Бернетти–Бусси (2020): объём ящика флуктуирует так,\nчто получается точный NPT-ансамбль при давлении P*.\n"
                    "Работает при периодических границах; «поршень» — механический баростат")) { P.npt = on; baroCount = 0; resetEnergyRef(); }
        yy += uiPx(22) + g;
        if (!per) yy += drawWrapped(fontXS, x, yy, W, P.boundary == B_PISTON ? "Сейчас границы «поршень»: давление задаёт поршень (масса + P внешн.)." : "Нужны периодические границы (вкладка «Управление» → Границы).", C_WARN) + g;
        double pv = std::max(1e-3, P.pExt);
        if (uiSlider(1011, x, yy, W, sh - uiPx(4), "P* внешнее", &pv, 1e-3, 5.0, true, fmt("%.4f · %.1f бар", P.pExt, toBar(P.pExt)),
                     "Целевое давление (ε/σ³).\nПересчёт для аргона: 1 P* = 489 бар")) { P.pExt = pv < 1.05e-3 ? 0.0 : pv; resetEnergyRef(); }
        yy += sh;
        { double t = P.tauP; if (uiSlider(1012, x, yy, W, sh - uiPx(4), "τP отклика", &t, 0.2, 30, true, fmt("%.2fτ · %.1f пс", P.tauP, toPs(P.tauP)),
                                             "Время релаксации объёма: меньше — быстрее подстройка,\nбольше — мягче (распределение от τP не зависит)")) P.tauP = t; }
        yy += sh;
        const double V = boxVolume();
        kv("P сейчас", fmt("%.4f", EN.P), fmt("%.1f бар · %.2f МПа", toBar(EN.P), toMPa(EN.P)), per && P.npt && std::fabs(EN.P - P.pExt) > 0.5 * std::max(0.02, P.pExt) ? C_WARN : C_TEXT_HI);
        kv("объём V", fmt("%.1f", V), fmt("σ³ · %.1f нм³", V * std::pow(cfg::U_L_NM, 3)));
        kv("ящик", fmt("%.2f×%.2f×%.2f", S.Lx, S.Ly, S.Lz), fmt("σ · %.2f нм", toNm(S.Lx)));
        yy += g;
    }
    // ---------------- реальные единицы
    uiSection(x, yy, W, "В реальных единицах (аргон)");
    {
        kv("T", fmt("%.1f K", toKelvin(EN.T)), fmt("%.1f °C", toKelvin(EN.T) - 273.15));
        kv("P", fmt("%.2f бар", toBar(EN.P)), fmt("%.2f атм · %.3f МПа", toAtm(EN.P), toMPa(EN.P)));
        kv("плотность", fmt("%.3f г/см³", massDensityNow()), fmt("%.2f моль/л", molarConc()));
        kv("E на атом", fmt("%.3f кДж/моль", toKJmol(EN.total() / std::max(1, EN.nmob))), fmt("%.4f эВ (хим.)", toEV(EN.total() / std::max(1, EN.nmob))));
        kv("время", fmt("%.2f пс", toPs(S.t)), fmt("шаг %.2f фс", toPs(P.dt) * 1000));
        yy += drawWrapped(fontXS, x, yy, W, "ε/k = 139.8 K подобран по критической точке аргона для LJ с обрезкой 2.5σ; σ = 0.3405 нм, τ = 1.0 пс.", C_DIM) + g;
    }
    // ---------------- объекты поля (сводка; редактирование — вкладка «Объект»)
    if (!fieldObjs.empty()) {
        uiSection(x, yy, W, fmt("Объекты поля: %d", (int)fieldObjs.size()));
        double heat = 0, cool = 0, emit = 0, sink = 0; long long ne = 0, ns = 0;
        for (auto& o : fieldObjs) {
            if (o.kind == FO_HEATER || o.kind == FO_COOLER) { if (o.pw > 0) heat += o.pw; else cool += o.pw; }
            if (o.kind == FO_EMITTER) { emit += o.pw; ne += o.cnt; }
            if (o.kind == FO_SINK) { sink += o.pw; ns += o.cnt; }
        }
        kv("теплоприток", fmt("%+.2f ε/τ", heat), fmt("отвод %+.2f ε/τ", cool));
        if (ne || emit > 0) kv("испущено", fmt("%lld ат.", ne), fmt("%.2f ат/τ", emit));
        if (ns || sink > 0) kv("поглощено", fmt("%lld ат.", ns), fmt("%.2f ат/τ", sink));
        kv("энергия в полях", fmt("%.2f ε", EN.efo), "(работа при их перемещении — в W)");
        yy += g;
    }
    // ---------------- устойчивость
    uiSection(x, yy, W, "Устойчивость интегрирования");
    {
        if (uiCheck(1020, x, yy, W, uiPx(22), "страж: откат при «взрыве» или NaN", &guardEnabled,
                    "Раз в 200 шагов сохраняется снимок. Если появились не-конечные числа\nили T выросла в 50 раз с нарушением баланса энергии —\n"
                    "откат к снимку и уменьшение шага dt (затем dt плавно восстанавливается)")) {}
        yy += uiPx(22) + g;
        kv("шаг dt", fmt("%.5f", P.dt), fmt("базовый %.4f%s", P.dtBase, guardDtScale < 0.999 ? fmt(" · огр. ×%.2f", guardDtScale).c_str() : ""),
           P.dt < P.dtBase * 0.5 ? C_WARN : C_TEXT_HI);
        kv("откатов", fmt("%d", physRollbacks), EN.capped ? fmt("огранич. силы: %d", EN.capped) : std::string(), physRollbacks ? C_WARN : C_TEXT_HI);
        if (!physAlert.empty()) {
            float hh = drawWrapped(fontXS, x + uiPx(8), yy + uiPx(1), W - uiPx(8), physAlert, physAlertActive() ? C_WARN : C_DIM);
            rectFill(x, yy + uiPx(1), uiPx(2), hh, physAlertActive() ? C_WARN : C_LINE_H); yy += hh + g;
        }
    }
    scrollEnd(3, yy + uiPx(8));
}

// ===================================== ПРОВЕРКИ ФИЗИКИ БЕЗ ОКНА (--phystest) ==================
// atoms.exe --phystest РЕЖИМ K N …  — запускается до окна (статическая инициализация), отчёт в phys_*.log
static int physTestMain() {
    const wchar_t* cmd = GetCommandLineW();
    const wchar_t* p0 = cmd ? wcsstr(cmd, L"--phystest") : nullptr;
    if (!p0) return 0;
    wchar_t* p = (wchar_t*)p0 + 10;
    while (*p == L' ') p++;
    std::wstring mode; while (*p && *p != L' ') mode += *p++;
    int K = (int)wcstol(p, &p, 10), N = (int)wcstol(p, &p, 10);
    double A1 = wcstod(p, &p), A2 = wcstod(p, &p);
    initBondTable(); initKlm(); rebuildTables(); loadPreset(K, 0);
    FILE* f = fopen(fmt("phys_%ls_%d_%g.log", mode.c_str(), K, A1).c_str(), "w"); if (!f) ExitProcess(1);
    if (mode == L"coex") {   // сосуществование жидкость–пар (плёнка в вытянутом ящике): ρ_ж, ρ_г при T = A1
        const double T = A1 > 0 ? A1 : 0.8;
        const double a = std::cbrt(4 / 0.8); const int nx = 7, nz = 12;
        worldReset(nx * a, nx * a, 60, B_PERIODIC);
        lattice3D(L_FCC, E_AR, -1, 0, 0, 0, 30 - nz * a / 2, nx, nx, nz, a, T);
        finishPreset(); P.thermostat = TH_BUSSI; P.Tset = T; P.tauT = 0.5; P.chemistry = false;
        const int NBIN = 120; std::vector<double> h(NBIN, 0.0); int samples = 0;
        const double L = S.Lz;
        for (int s = 0; s < N; s++) {
            mdStep();
            if (s > N / 3 && s % 50 == 0) {
                // центр плёнки — круговое среднее координаты по длинной оси
                double cs = 0, sn = 0; for (int i = 0; i < S.n; i++) { double u = S.z[i] / L * 2 * PI; cs += std::cos(u); sn += std::sin(u); }
                double zc = std::atan2(sn, cs) / (2 * PI) * L;
                for (int i = 0; i < S.n; i++) { double u = S.z[i] - zc; u -= L * std::floor(u / L + 0.5); h[clampv((int)((u / L + 0.5) * NBIN), 0, NBIN - 1)] += 1; }
                samples++;
            }
            if (s % 5000 == 0) { measure(); fprintf(f, "s=%d t=%.1f T=%.3f P=%.4f dt=%.4f\n", s, S.t, EN.T, EN.P, P.dt); fflush(f); }
        }
        const double binV = (S.Lx * S.Ly) * L / NBIN;
        for (int b = 0; b < NBIN; b++) fprintf(f, "z=%.2f rho=%.4f\n", (b + 0.5) / NBIN * L - L / 2, h[b] / std::max(1, samples) / binV);
        double rl = 0, rg = 0; int cl = 0, cg = 0;
        for (int b = 0; b < NBIN; b++) { double z = std::fabs((b + 0.5) / NBIN - 0.5); double r = h[b] / std::max(1, samples) / binV;
            if (z < 0.06) { rl += r; cl++; } else if (z > 0.38) { rg += r; cg++; } }
        fprintf(f, "RESULT T=%.3f  rho_liq=%.4f rho_gas=%.4f  (N=%d)\n", T, rl / std::max(1, cl), rg / std::max(1, cg), S.n);
    }
    if (mode == L"vir") {   // вириал Σr·F против −dU/dλ при масштабировании r → λr, L → λL (все многочастичные члены)
        if (!isPer()) fprintf(f, "(границы не периодические: проверяется только внутренняя часть без стенок)\n");
        for (int s = 0; s < N; s++) { runScript(); mdStep(); }
        const bool chem = P.chemistry; P.chemistry = false;
        auto U = [&]() { computeForces(); return EN.enb + EN.ebond + EN.egrav; };
        auto scale = [&](double m) { S.Lx *= m; S.Ly *= m; S.Lz *= m; for (int i = 0; i < S.n; i++) { S.x[i] *= m; S.y[i] *= m; S.z[i] *= m; } };
        U(); double vir = EN.vir; const double h = 1e-6;
        scale(1 + h); double Up = U(); scale(1 / (1 + h)); scale(1 - h); double Um = U(); scale(1 / (1 - h)); U();
        double num = -(Up - Um) / (2 * h);
        fprintf(f, "RESULT preset %d: vir(analytic)=%.6f  -dU/dlambda=%.6f  rel.err=%.2e  (N=%d, P_conf=%.5f)\n", K, vir, num, std::fabs(vir - num) / std::max(1.0, std::fabs(num)), S.n, vir / (DIM * boxVolume()));
        P.chemistry = chem;
    }
    if (mode == L"log") {   // пресет K: доли фаз и журнал переходов (как в окне: анализ каждые ~0.2τ)
        if (A2 > 0) P.heatPower = A2;
        for (int s = 0; s <= N; s++) {
            runScript(); mdStep();
            if (s % 32 == 0) analysisTick();
            if (s % 2000 == 0) { fprintf(f, "s=%d t=%.1f T=%.4f P=%.4f  газ %.2f жидк %.2f тв %.2f  %s\n", s, S.t, EN.T, EN.P, phaseFrac[0], phaseFrac[1], phaseFrac[2], A::phase.c_str()); fflush(f); }
        }
        for (auto& l : phaseLog) fprintf(f, "  log: %s\n", l.c_str());
    }
    if (mode == L"guard") {   // страж: NaN в скорости → откат и продолжение
        for (int s = 0; s <= N; s++) {
            runScript(); mdStep();
            if (s == N / 2) { S.vx[3] = std::numeric_limits<double>::quiet_NaN(); fprintf(f, "s=%d: NaN injected\n", s); }
            if (s % 500 == 0 || s == N / 2 + 1) { measure(); fprintf(f, "s=%d t=%.3f T=%.4f E-W-Eref=%.5f rollbacks=%d dt=%.5f alert=%s\n", s, S.t, EN.T, EN.total() - Wext - Eref, physRollbacks, P.dt, physAlert.c_str()); fflush(f); }
        }
    }
    if (mode == L"npt") {   // баростат C-rescale: LJ при T = A2, P = A1 → средняя плотность и давление
        const double P0 = A1, T = A2 > 0 ? A2 : 0.9;
        double a = std::cbrt(4 / 0.8); worldReset(6 * a, 6 * a, 6 * a, B_PERIODIC); lattice3D(L_FCC, E_AR, -1, 0, 0, 0, 0, 6, 6, 6, a, T);
        finishPreset(); P.thermostat = TH_BUSSI; P.Tset = T; P.tauT = 0.5; P.npt = true; P.pExt = P0; P.tauP = 2.0; P.chemistry = false; resetEnergyRef();
        double sr = 0, sp = 0, sv = 0, sv2 = 0; int c = 0;
        for (int s = 0; s <= N; s++) {
            mdStep();
            if (s > N / 3 && s % 20 == 0) { measure(); double V = boxVolume(); sr += S.n / V; sp += EN.Pvir; sv += V; sv2 += V * V; c++; }
            if (s % 5000 == 0) { measure(); fprintf(f, "s=%d t=%.1f T=%.4f Pvir=%.4f rho=%.4f L=%.3f E-W-Eref=%.4f dt=%.4f\n", s, S.t, EN.T, EN.Pvir, S.n / boxVolume(), S.Lx, EN.total() - Wext - Eref, P.dt); fflush(f); }
        }
        double mv = sv / c, var = sv2 / c - mv * mv;
        fprintf(f, "RESULT NPT P0=%.4f T=%.3f: <rho>=%.4f <Pvir>=%.4f  kappa_T=<dV2>/(kT V)=%.4f  (N=%d)\n", P0, T, sr / c, sp / c, var / (T * mv), S.n);
    }
    if (mode == L"melt") {   // плавление плоской плёнки кристалла (свободные поверхности, P ≈ p_нас) медленным нагревом
        const double T0 = 0.50, pw = A2 > 0 ? A2 : 0.003;
        const double a = std::cbrt(4 / 0.96); const int nx = 7, nz = 16;
        worldReset(nx * a, nx * a, nz * a + 30, B_PERIODIC);
        lattice3D(L_FCC, E_AR, -1, 0, 0, 0, 15, nx, nx, nz, a, T0);
        finishPreset(); P.thermostat = TH_POWER; P.heatPower = pw; P.Tset = T0; P.chemistry = false;
        double prevT = 0, prevF = 1; bool found = false; double Tcross = 0;
        for (int s = 0; s <= N; s++) {
            mdStep();
            if (s % 200 == 0) {
                analysisTick();
                double fsol = phaseFrac[2];
                if (!found && s > 0 && prevF >= 0.5 && fsol < 0.5) { Tcross = prevT; found = true; }
                prevT = EN.T; prevF = fsol;
            }
            if (s % 4000 == 0) { fprintf(f, "s=%d t=%.1f T=%.4f P=%.4f  газ %.2f жидк %.2f тв %.2f  %s\n", s, S.t, EN.T, EN.P, phaseFrac[0], phaseFrac[1], phaseFrac[2], A::phase.c_str()); fflush(f); }
        }
        fprintf(f, "RESULT melting (solid fraction 0.5) at T ≈ %.4f\n", Tcross);
        for (auto& l : phaseLog) fprintf(f, "  log: %s\n", l.c_str());
    }
    if (mode == L"triple") {   // три фазы в NVE (кристалл + расплав + пар) сами приходят к тройной точке
        const double T0 = 0.55, Thot = 1.3;
        const double a = std::cbrt(4 / 0.97); const int nx = 7, nz = 22;
        worldReset(nx * a, nx * a, nz * a + 30, B_PERIODIC);
        lattice3D(L_FCC, E_AR, -1, 0, 0, 0, 15, nx, nx, nz, a, T0);
        const double zc = 15 + nz * a / 2;
        for (int i = 0; i < S.n; i++) if (S.z[i] > zc) { double vx, vy, vz; thermalVel(E_AR, Thot, vx, vy, vz); S.vx[i] = vx; S.vy[i] = vy; S.vz[i] = vz; }
        finishPreset(); P.thermostat = TH_NVE; P.chemistry = false;
        double Tsum = 0; int Tc = 0;
        for (int s = 0; s <= N; s++) {
            mdStep();
            if (s % 250 == 0) { analysisTick(); if (s > N / 2) { Tsum += EN.T; Tc++; } }
            if (s % 5000 == 0) {
                fprintf(f, "s=%d t=%.1f T=%.4f P=%.4f  газ %.2f жидк %.2f тв %.2f  drift=%.2e\n", s, S.t, EN.T, EN.P, phaseFrac[0], phaseFrac[1], phaseFrac[2], (EN.total() - Wext - Eref) / std::fabs(Eref));
                fflush(f);
            }
        }
        fprintf(f, "RESULT T_triple ≈ %.4f (среднее по второй половине)\n", Tsum / std::max(1, Tc));
        for (auto& l : phaseLog) fprintf(f, "  log: %s\n", l.c_str());
    }
    if (mode == L"fo") {   // объекты поля и закрепление: баланс E − W (A1 — набор объектов), NVE
        P.thermostat = TH_NVE; script.clear();
        const double cx = S.Lx / 2, cy = S.Ly / 2, cz = S.Lz / 2;
        int set = (int)A1;
        if (set == 0) {        // консервативные
            fieldObjs.push_back(makeFieldObj(FO_ATTRACT, cx - S.Lx / 4, cy, cz));
            fieldObjs.push_back(makeFieldObj(FO_REPEL, cx + S.Lx / 4, cy, cz));
            fieldObjs.push_back(makeFieldObj(FO_TRAP, cx, cy + S.Ly / 4, cz));
            fieldObjs.push_back(makeFieldObj(FO_BARRIER, cx, cy, cz));
        } else if (set == 1) { // ветер и вихрь
            fieldObjs.push_back(makeFieldObj(FO_WIND, cx - S.Lx / 4, cy, cz));
            fieldObjs.push_back(makeFieldObj(FO_VORTEX, cx + S.Lx / 4, cy, cz));
        } else if (set == 2) { // нагреватель, охладитель
            fieldObjs.push_back(makeFieldObj(FO_HEATER, cx - S.Lx / 4, cy, cz));
            fieldObjs.push_back(makeFieldObj(FO_COOLER, cx + S.Lx / 4, cy, cz));
        } else if (set == 3) { // источник и сток
            FieldObj e = makeFieldObj(FO_EMITTER, 3, cy, cz); e.elem = typeOfZ(10); e.strength = 20; fieldObjs.push_back(e);
            fieldObjs.push_back(makeFieldObj(FO_SINK, S.Lx - 4, cy, cz));
        } else if (set == 4) { // закрепить каждый 5-й атом
            for (int i = 0; i < S.n; i += 5) setPin(i, true);
        }
        computeForces(); resetEnergyRef();
        for (int s = 0; s <= N; s++) {
            runScript(); mdStep();
            if (set == 0 && s % 2000 == 1000) {   // «рука» двигает объекты (учёт работы — как в UI: foEditBegin/End)
                computeForces(); measure(); double E0 = EN.total();
                fieldObjs[0].x += 0.5; fieldObjs[3].y2 += 0.3;
                computeForces(); measure(); Wext += EN.total() - E0;
            }
            if (s % 1000 == 0) {
                measure(); double pinMove = 0;
                if (set == 4) for (int i = 0; i < S.n; i++) if (S.pin[i]) pinMove += std::fabs(S.vx[i]) + std::fabs(S.vy[i]) + std::fabs(S.vz[i]);
                fprintf(f, "s=%d t=%.2f N=%d T=%.4f E=%.4f W=%.4f E-W-Eref=%.5f efo=%.3f dt=%.4f", s, S.t, S.n, EN.T, EN.total(), Wext, EN.total() - Wext - Eref, EN.efo, P.dt);
                for (auto& o : fieldObjs) fprintf(f, "  [%s pw=%.3f cnt=%lld]", FO_NAMES[o.kind], o.pw, o.cnt);
                if (set == 4) fprintf(f, "  pinv=%g", pinMove);
                fprintf(f, "  rollbacks=%d\n", physRollbacks); fflush(f);
            }
        }
    }
    if (mode == L"nan") {   // первый шаг с не-конечной энергией: подробности
        for (int s = 0; s < N; s++) {
            runScript(); mdStep();
            double e = EN.enb + EN.ebond + EN.egrav;
            if (!std::isfinite(e) || !std::isfinite(kinetic())) {
                fprintf(f, "NaN at step %lld t=%.4f: enb=%g ebond=%g egrav=%g K=%g\n", S.step, S.t, EN.enb, EN.ebond, EN.egrav, kinetic());
                int shown = 0;
                for (int i = 0; i < S.n && shown < 20; i++) {
                    bool bad = !std::isfinite(S.ep[i]) || !std::isfinite(S.vx[i]) || !std::isfinite(S.fx[i]);
                    for (int k = 0; k < S.nbc[i]; k++) if (!std::isfinite(S.bc[i][k])) bad = true;
                    if (!bad) continue; shown++;
                    fprintf(f, "  atom %d %s ep=%g v=%g F=%g nbc=%d rho=%.3e w=%g bonds:", i, EL[S.ty[i]].sym, S.ep[i], S.vx[i], S.fx[i], S.nbc[i],
                            i < (int)gRho.size() ? gRho[i] : -1.0, i < (int)gW.size() ? gW[i] : -1.0);
                    for (int k = 0; k < S.nbc[i]; k++) fprintf(f, " %s%d(o%d c=%g)", EL[S.ty[S.nb[i][k]]].sym, S.nb[i][k], S.bo[i][k], S.bc[i][k]);
                    fprintf(f, "\n");
                }
                break;
            }
            if (s % 2000 == 0) { fprintf(f, "t=%.2f T=%.3f E=%.4f\n", S.t, 2 * kinetic() / std::max(1, dofCount(S.n)), e); fflush(f); }
        }
    }
    fclose(f);
    ExitProcess(0);
    return 0;
}
static const int physTestHook = physTestMain();

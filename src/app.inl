// ===================================== ПОЛНЫЙ ЭКРАН / ИНСТРУКЦИЯ ==========================
static WINDOWPLACEMENT wpPrev = {sizeof(WINDOWPLACEMENT)};
static void toggleFullscreen() {   // окно без рамки на весь монитор и обратно
    DWORD style = (DWORD)GetWindowLongW(hwnd, GWL_STYLE);
    if (!fullscreen) {
        MONITORINFO mi = {sizeof(mi)};
        if (GetWindowPlacement(hwnd, &wpPrev) && GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLongW(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            fullscreen = true;
        }
    } else {
        SetWindowLongW(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &wpPrev);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        fullscreen = false;
    }
    showToast(fullscreen ? "Полный экран (F11 или Esc — выйти)" : "Оконный режим");
}
// F1: инструкция на языке интерфейса (MANUAL.html / ИНСТРУКЦИЯ.html) рядом с программой или в docs\;
// нужной нет — открывается другая
static void openManual() {
    const bool en = LANG == LANG_EN;
    auto exists = [](const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; };
    auto find = [&](const wchar_t* name) {
        for (const wchar_t* sub : {L"\\", L"\\docs\\", L"\\..\\docs\\"}) { std::wstring p = exeDir() + sub + name; if (exists(p)) return p; }
        return std::wstring();
    };
    std::wstring path = find(en ? L"MANUAL.html" : L"ИНСТРУКЦИЯ.html");
    const char* msg = "Инструкция открыта в браузере";
    if (path.empty()) {
        path = find(en ? L"ИНСТРУКЦИЯ.html" : L"MANUAL.html");
        if (path.empty()) { showToast(en ? "Файл MANUAL.html не найден рядом с atoms.exe" : "Файл ИНСТРУКЦИЯ.html не найден рядом с atoms.exe"); return; }
        msg = en ? "Файл MANUAL.html не найден — открыта русская инструкция" : "Файл ИНСТРУКЦИЯ.html не найден — открыта английская инструкция (MANUAL.html)";
    }
    if (!uiTestMode) ShellExecuteW(hwnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    showToast(msg);
}
// ---- язык интерфейса: --lang, затем atoms.ini (lang=ru|en), затем язык Windows
static const char* WINDOW_TITLE = "Атомы — молекулярная динамика и химия";
static int langSystemDefault() {   // русский, украинский, белорусский, казахский интерфейс Windows → RU, иначе EN
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) { case LANG_RUSSIAN: case LANG_UKRAINIAN: case LANG_BELARUSIAN: case LANG_KAZAK: return LANG_RU; }
    return LANG_EN;
}
static void setLanguage(int l) {   // переключение на лету: кнопки RU | EN, Ctrl+L
    l = l == LANG_EN ? LANG_EN : LANG_RU;
    if (l == LANG) return;
    langSet(l);
    if (hwnd) SetWindowTextW(hwnd, TW(WINDOW_TITLE).c_str());
    opt.lang = LANG; settingsSave();
    showToast(LANG == LANG_EN ? "Language: English" : "Язык: русский");
}

// ===================================== СОХРАНЕНИЕ / ЗАГРУЗКА / ЭКСПОРТ ======================
// Бинарный файл .atoms: полное состояние (атомы, связи, скорости, параметры, камера).
// После основной части — блоки «метка(4) + размер(4) + данные»: PIN_ — закреплённые атомы, FOBJ — объекты поля,
// VIEW — слои отображения и инструменты. Неизвестные блоки пропускаются.
// Params и FieldObj пишутся с размером: при чтении файла с другим размером берётся общий префикс (новые поля — по умолчанию).
// Версия 4 (5.0): до 6 связей у атома (число записано в файле), реальная шкала энергий. Файлы версии 3 читаются:
// связи переносятся, заряды пересчитываются (ионы там несли ±0.8).
static const char SAVE_MAGIC[8] = {'A', 'T', 'O', 'M', 'S', 'A', 'V', '2'};
static void resetAnalysis();
static bool toastsOff = false;   // сохранение сеанса при выходе — без уведомлений
static bool saveState(const std::wstring& path, bool quiet) {
    FILE* f = _wfopen(path.c_str(), L"wb"); if (!f) { showToast("Не удалось записать файл"); return false; }
    auto w = [&](const void* p, size_t sz) { if (sz) fwrite(p, 1, sz, f); };
    auto wv = [&](const auto& v) { w(v.data(), v.size() * sizeof(v[0])); };
    uint32_t ver = 4, szP = sizeof(Params), szC = sizeof(Cam3);
    w(SAVE_MAGIC, 8); w(&ver, 4); w(&szP, 4); w(&szC, 4);
    int32_t hdr[8] = {3, currentPreset, presetVariant, colorMode, (int)trailsOn, (int)bondsOn, EL[customType].Z, lmbTool}; w(hdr, sizeof(hdr));
    int32_t maxb = cfg::MAXB; w(&maxb, 4);
    w(&P, sizeof(Params));
    int32_t n = S.n; w(&n, 4);
    double sc[9] = {S.Lx, S.Ly, S.Lz, S.pistonV, S.pistonM, S.xi, S.eta, S.t, (double)S.step}; w(sc, sizeof(sc));
    for (auto* v : {&S.x, &S.y, &S.z, &S.vx, &S.vy, &S.vz, &S.ux, &S.uy, &S.uz, &S.q}) wv(*v);
    wv(S.ty); wv(S.nb); wv(S.bo); wv(S.nbc); wv(S.bc); wv(S.gh); wv(S.ghc);
    uint32_t tl = (uint32_t)presetTitle.size(); w(&tl, 4); w(presetTitle.data(), tl);
    w(&cam3, sizeof(Cam3)); double c2[3] = {0, 0, 0}; w(c2, sizeof(c2)); int32_t cm = camMode; w(&cm, 4);   // c2 — место бывшей плоской камеры
    long long ch[3] = {CH.assoc, CH.exch, CH.diss}; w(ch, sizeof(ch));
    // --- блоки версии 3
    auto chunk = [&](const char* tag, uint32_t size) { w(tag, 4); w(&size, 4); };
    { std::vector<unsigned char> pn(S.n, 0); for (int i = 0; i < S.n && i < (int)S.pin.size(); i++) pn[i] = S.pin[i];
      chunk("PIN_", (uint32_t)pn.size()); wv(pn); }
    { uint32_t szF = sizeof(FieldObj), cnt = (uint32_t)fieldObjs.size(); chunk("FOBJ", 8 + szF * cnt); w(&szF, 4); w(&cnt, 4); wv(fieldObjs); }
    { int32_t v[16] = {layerCharges, layerVel, layerForce, layerFO, layerGrid, layerLegend, layerScale, layerPins, foKind, sideTab, graphsOn,
                       (int32_t)std::lround(toolPower * 1000), selPal, 0, 0, 0};
      chunk("VIEW", sizeof(v)); w(v, sizeof(v)); }
    bool ok = ferror(f) == 0; fclose(f);
    if (toastsOff) return ok;
    if (!quiet || !ok) showToast(ok ? "Состояние сохранено" : "Ошибка записи файла");
    else showToast("Быстрое сохранение (F9 — загрузить)");
    return ok;
}
static void clearToolState();
static bool loadState(const std::wstring& path) {
    FILE* f = _wfopen(path.c_str(), L"rb"); if (!f) { showToast("Файл не найден"); return false; }
    bool ok = true;
    auto r = [&](void* p, size_t sz) { if (ok && sz && fread(p, 1, sz, f) != sz) ok = false; };
    auto skip = [&](long sz) { if (ok && sz > 0 && fseek(f, sz, SEEK_CUR) != 0) ok = false; };
    char mg[8]; uint32_t ver = 0, szP = 0, szC = 0; r(mg, 8); r(&ver, 4); r(&szP, 4); r(&szC, 4);
    if (!ok || memcmp(mg, SAVE_MAGIC, 8) != 0 || ver < 2 || ver > 4 || szP < 64 || szP > 65536 || szC != sizeof(Cam3)) { fclose(f); showToast("Это не файл состояния «Атомы» (или другая версия)"); return false; }
    int32_t hdr[8]; r(hdr, sizeof(hdr));
    int32_t maxb = 4; if (ver >= 4) r(&maxb, 4);   // связей на атом в записи файла (до версии 4 — четыре)
    if (maxb < 1 || maxb > 16) { fclose(f); showToast("Файл повреждён"); return false; }
    Params np = P;   // поля, которых нет в файле (более старая версия), остаются текущими
    { size_t m = std::min<size_t>(szP, sizeof(Params)); r(&np, m); skip((long)szP - (long)m); }
    if (szP < sizeof(Params)) {   // файл до редактора граней: границы — по общему режиму, грани обычные
        np.perMask = np.boundary == B_PERIODIC ? 7 : 0; np.container = CT_BOX;
        for (int k = 0; k < 6; k++) { np.wallType[k] = WT_SOFT; np.wallTK[k] = 300 / cfg::U_T_K; }
    }
    int32_t n = 0; r(&n, 4);
    if (!ok || n < 0 || n > 2000000 || (hdr[0] != 2 && hdr[0] != 3)) { fclose(f); showToast("Файл повреждён"); return false; }
    if (hdr[0] == 2) { fclose(f); showToast("Файл сохранён в плоском режиме старой версии — сейчас модель только объёмная"); return false; }
    Sim ns; ns.resize(n); ns.n = n;
    double sc[9]; r(sc, sizeof(sc));
    ns.Lx = sc[0]; ns.Ly = sc[1]; ns.Lz = sc[2]; ns.pistonV = sc[3]; ns.pistonM = sc[4]; ns.xi = sc[5]; ns.eta = sc[6]; ns.t = sc[7]; ns.step = (long long)sc[8];
    auto rv = [&](auto& v) { r(v.data(), v.size() * sizeof(v[0])); };
    for (auto* v : {&ns.x, &ns.y, &ns.z, &ns.vx, &ns.vy, &ns.vz, &ns.ux, &ns.uy, &ns.uz, &ns.q}) rv(*v);
    rv(ns.ty);
    if (maxb == cfg::MAXB) { rv(ns.nb); rv(ns.bo); rv(ns.nbc); rv(ns.bc); }
    else {   // другая ширина записи связей: читаем построчно и переносим, лишние (если их больше, чем помещается) отбрасываем
        std::vector<int> nb((size_t)n * maxb); std::vector<unsigned char> bo((size_t)n * maxb); std::vector<double> bc((size_t)n * maxb);
        rv(nb); rv(bo); rv(ns.nbc); rv(bc);
        for (int i = 0; ok && i < n; i++) {
            if (ns.nbc[i] > cfg::MAXB) { ok = false; break; }
            for (int k = 0; k < ns.nbc[i]; k++) { ns.nb[i][k] = nb[(size_t)i * maxb + k]; ns.bo[i][k] = bo[(size_t)i * maxb + k]; ns.bc[i][k] = bc[(size_t)i * maxb + k]; }
        }
    }
    rv(ns.gh); rv(ns.ghc);
    uint32_t tl = 0; r(&tl, 4); std::string title; if (ok && tl < 4096) { title.resize(tl); if (tl) r(&title[0], tl); } else ok = false;
    Cam3 c3; double c2[3]; int32_t cm = 0; long long ch[3] = {0, 0, 0}; r(&c3, sizeof(Cam3)); r(c2, sizeof(c2)); r(&cm, 4); r(ch, sizeof(ch));
    // блоки версии 3
    std::vector<FieldObj> fo; bool haveFO = false; int32_t view[16]; bool haveView = false;
    while (ok && ver >= 3) {
        char tag[4]; uint32_t size = 0;
        if (fread(tag, 1, 4, f) != 4 || fread(&size, 1, 4, f) != 4) break;   // конец файла
        if (size > 512u * 1024 * 1024) { ok = false; break; }
        if (!memcmp(tag, "PIN_", 4) && size == (uint32_t)n) { std::vector<unsigned char> pn(n); rv(pn); for (int i = 0; i < n; i++) ns.pin[i] = pn[i] ? 1 : 0; }
        else if (!memcmp(tag, "FOBJ", 4) && size >= 8) {
            uint32_t szF = 0, cnt = 0; r(&szF, 4); r(&cnt, 4);
            if (!ok || szF < 16 || szF > 4096 || cnt > 10000 || 8ull + (uint64_t)szF * cnt != size) { ok = false; break; }
            for (uint32_t k = 0; k < cnt && ok; k++) {
                FieldObj o; size_t m = std::min<size_t>(szF, sizeof(FieldObj)); r(&o, m); skip((long)szF - (long)m);
                bool good = o.kind >= 0 && o.kind < FO_N && std::isfinite(o.x) && std::isfinite(o.y) && std::isfinite(o.z) && std::isfinite(o.x2) && std::isfinite(o.y2) &&
                            std::isfinite(o.z2) && std::isfinite(o.R) && std::isfinite(o.strength) && std::isfinite(o.Tset) && o.elem >= 0 && o.elem < NEL;
                if (good) fo.push_back(o);
            }
            haveFO = true;
        }
        else if (!memcmp(tag, "VIEW", 4) && size == sizeof(view)) { r(view, sizeof(view)); haveView = true; }
        else skip((long)size);
    }
    fclose(f);
    // проверка целостности: типы, связи, размеры ящика
    if (ok) ok = ns.Lx > 0 && ns.Ly > 0 && ns.Lz > 0 && std::isfinite(ns.t);
    for (int i = 0; ok && i < n; i++) {
        if (ns.ty[i] < 0 || ns.ty[i] >= NEL || ns.nbc[i] > cfg::MAXB || ns.ghc[i] > 6) { ok = false; break; }
        for (int k = 0; k < ns.nbc[i]; k++) if (ns.nb[i][k] < 0 || ns.nb[i][k] >= n || ns.nb[i][k] == i || ns.bo[i][k] < 1 || ns.bo[i][k] > 3) ok = false;
        for (int k = 0; k < ns.ghc[i]; k++) if (ns.gh[i][k] < 0 || ns.gh[i][k] >= n) ok = false;
        if (!std::isfinite(ns.x[i]) || !std::isfinite(ns.vx[i])) ok = false;
    }
    if (!ok) { showToast("Файл повреждён — состояние не изменено"); return false; }
    if (world != W_MD) { world = W_MD; worldLeave(); }   // файл — всегда сцена с атомами
    pushUndo();
    bool pz = P.paused;
    S = std::move(ns); P = np; P.paused = pz;
    currentPreset = hdr[1]; presetVariant = hdr[2]; colorMode = clampv(hdr[3], 0, COLOR_N - 1); trailsOn = hdr[4] != 0; bondsOn = hdr[5] != 0; lmbTool = clampv(hdr[7], 0, TOOL_N - 1);
    // заголовок — исходный литерал (перевод при выводе); пометка «загружено» — отдельно (старые файлы хранили её в заголовке)
    { const std::string mark = "  [загружено]"; while (title.size() >= mark.size() && title.compare(title.size() - mark.size(), mark.size(), mark) == 0) title.resize(title.size() - mark.size()); }
    presetTitle = title; presetLoaded = true;
    if (typeOfZ(hdr[6]) >= 0) customType = typeOfZ(hdr[6]);
    rebuildTables();
    cam3 = c3; camGoal = c3; camMode = cm == 1 ? 1 : 0;
    CH.assoc = ch[0]; CH.exch = ch[1]; CH.diss = ch[2];
    grabbed = -1; followAtom = -1; pistonGrab = false; flashes.clear(); script.clear();
    fieldObjs = haveFO ? fo : std::vector<FieldObj>(); selFieldObj = -1; clearToolState();
    if (haveView) {
        layerCharges = view[0] != 0; layerVel = view[1] != 0; layerForce = view[2] != 0; layerFO = view[3] != 0; layerGrid = view[4] != 0; layerLegend = view[5] != 0;
        layerScale = view[6] != 0; layerPins = view[7] != 0; foKind = clampv((int)view[8], 0, FO_N - 1); sideTab = clampv((int)view[9], 0, TAB_N - 1); graphsOn = view[10] != 0;
        toolPower = clampv(view[11] / 1000.0, 0.1, 10.0); selPal = clampv((int)view[12], 0, (int)palette.size() - 1);
    }
    if (ver < 4) rechargeAll(0.8);   // файл до перехода на реальную шкалу: однозарядные ионы несли ±0.8
    buildPairTables(); updatePresence(); computeForces(); resetEnergyRef(); resetAnalysis(); resetMSD();
    showToast(ver >= 4 ? "Состояние загружено" : "Состояние загружено (файл прежней версии: заряды пересчитаны)");
    return true;
}
// диалог выбора файла (в автотесте — без диалога)
static std::wstring fileDialog(bool save) {
    if (uiTestMode) return L"uitest_state.atoms";
    wchar_t buf[MAX_PATH] = L"";
    if (save) { SYSTEMTIME st; GetLocalTime(&st); swprintf(buf, MAX_PATH, LANG == LANG_EN ? L"atoms_%04d%02d%02d_%02d%02d%02d.atoms" : L"атомы_%04d%02d%02d_%02d%02d%02d.atoms",
                                                            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond); }
    OPENFILENAMEW of = {}; of.lStructSize = sizeof(of); of.hwndOwner = hwnd;
    std::wstring filter = TW("Состояние «Атомы» (*.atoms)"); filter.push_back(0); filter += L"*.atoms"; filter.push_back(0);   // пары «описание\0маска\0», в конце \0\0
    filter += TW("Все файлы"); filter.push_back(0); filter += L"*.*"; filter.push_back(0); filter.push_back(0);
    of.lpstrFilter = filter.c_str(); of.lpstrFile = buf; of.nMaxFile = MAX_PATH; of.lpstrDefExt = L"atoms";
    of.Flags = save ? (OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR) : (OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR);
    BOOL ok = save ? GetSaveFileNameW(&of) : GetOpenFileNameW(&of);
    in.lDown = in.rDown = in.mDown = false;   // кнопки мыши могли быть отпущены внутри диалога
    return ok ? std::wstring(buf) : std::wstring();
}
static void cmdSave() {
    if (world != W_MD) { showToast("Сохраняются только сцены с атомами; эту сцену R начинает заново"); return; }
    std::wstring p = fileDialog(true); if (!p.empty()) saveState(p, false);
}
static void cmdLoad() { std::wstring p = fileDialog(false); if (!p.empty()) loadState(p); }
static void cmdReset() { pushUndo(); loadPresetKeepObjs(currentPreset, presetVariant, true); viewFitPending = true; followAtom = -1; clearToolState(); }
static void cmdUndo() { popUndo(); clearToolState(); showToast("Отмена"); }
// Экспорт графиков в CSV: RU — разделитель «;» и десятичная запятая (Excel с русской локалью),
// EN — разделитель «,» и десятичная точка; заголовки — на языке интерфейса, текст с разделителем — в кавычках
static void exportCSV() {
    SYSTEMTIME st; GetLocalTime(&st);
    std::string name = fmt("export_%04d%02d%02d_%02d%02d%02d.csv", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    const bool en = LANG == LANG_EN; const char sep = en ? ',' : ';';
    FILE* f = _wfopen((outputDir(en) + L"\\" + widen(name.c_str())).c_str(), L"wb"); if (!f) { showToast("Не удалось создать CSV"); return; }
    auto num = [en](double v) { std::string s = fmt("%.6g", v); if (!en) for (char& c : s) if (c == '.') c = ','; return s; };
    auto cell = [sep](const std::string& s) {
        if (s.find(sep) == std::string::npos && s.find('"') == std::string::npos && s.find('\n') == std::string::npos) return s;
        std::string q = "\""; for (char c : s) { if (c == '"') q += '"'; q += c; } return q + "\"";
    };
    auto row = [&](const std::vector<std::string>& cs) { for (size_t k = 0; k < cs.size(); k++) { if (k) fputc(sep, f); fputs(cell(cs[k]).c_str(), f); } fputc('\n', f); };
    fputs("\xEF\xBB\xBF", f);
    row({T("Атомы — экспорт"), std::string(T(presetTitle)) + (presetLoaded ? T("  [загружено]") : ""), fmt("N=%d", S.n), "t=" + num(S.t)});
    fputc('\n', f);
    row({T("Временные ряды")}); row({"t", "T", "P", T("Eкин"), T("Eпот"), T("Eполн")});
    for (size_t k = 0; k < A::sT.v.size(); k++)
        row({num(k < A::sTime.v.size() ? A::sTime.v[k] : 0), num(A::sT.v[k]), num(k < A::sP.v.size() ? A::sP.v[k] : 0),
             num(k < A::sEk.v.size() ? A::sEk.v[k] : 0), num(k < A::sEp.v.size() ? A::sEp.v[k] : 0), num(k < A::sEt.v.size() ? A::sEt.v[k] : 0)});
    fputc('\n', f); row({"g(r)"}); row({"r", "g"});
    for (int k = 0; k < A::GR_BINS; k++) row({num((k + 0.5) * A::GR_RMAX / A::GR_BINS), num(A::gr[k])});
    fputc('\n', f); row({T("Распределение скоростей")}); row({"v", T("f(v) измер."), T("f(v) Максвелл")});
    for (int k = 0; k < A::VH_BINS; k++) { double v = (k + 0.5) * A::vhMax / A::VH_BINS;
        row({num(v), num(A::vh[k]), num(maxwellF(v, EL[A::vhType].m, std::max(EN.T, 0.005)))}); }
    fputc('\n', f); row({"MSD"}); row({"t", "MSD", T("MSD броун. частицы")});
    for (size_t k = 0; k < A::msdT.size(); k++) row({num(A::msdT[k]), num(A::msdV[k]), num(A::msdBig[k])});
    fputc('\n', f); row({T("Число молекул (отсчёты)")});
    { std::vector<std::string> h = {T("отсчёт")}; for (auto& sp : A::species) h.push_back(sp); row(h); }
    size_t m = 0; for (auto& c : A::conc) m = std::max(m, c.v.size());
    for (size_t k = 0; k < m; k++) { std::vector<std::string> r = {fmt("%zu", k)}; for (auto& c : A::conc) r.push_back(k < c.v.size() ? num(c.v[k]) : std::string()); row(r); }
    fclose(f);
    showToast(std::string(T("Экспорт: ")) + name);
}
// Обращение времени: v → −v (и ξ Нозе–Гувера, скорость поршня). Механика обратима — система идёт «назад»
static void reverseTime() {
    for (int i = 0; i < S.n; i++) { S.vx[i] = -S.vx[i]; S.vy[i] = -S.vy[i]; S.vz[i] = -S.vz[i]; }
    S.xi = -S.xi; S.pistonV = -S.pistonV;
    showToast("Время обращено: v → −v (хаос быстро разрушит обратимость)");
}

// ===================================== PNG (без внешних библиотек) ============================
// deflate с фиксированными кодами Хаффмана и жадным поиском совпадений (хеш по 3 байтам) —
// снимки сцены на чёрном фоне сжимаются в десятки раз; фильтр строк PNG — «Sub».
static uint32_t crcTab[256]; static bool crcReady = false;
static uint32_t crc32(const unsigned char* p, size_t n, uint32_t c = 0xFFFFFFFFu) {
    if (!crcReady) { for (uint32_t k = 0; k < 256; k++) { uint32_t v = k; for (int j = 0; j < 8; j++) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1; crcTab[k] = v; } crcReady = true; }
    for (size_t i = 0; i < n; i++) c = crcTab[(c ^ p[i]) & 255] ^ (c >> 8);
    return c;
}
struct BitWriter {
    std::vector<unsigned char> out; uint32_t buf = 0; int nb = 0;
    void put(uint32_t v, int n) { buf |= v << nb; nb += n; while (nb >= 8) { out.push_back((unsigned char)(buf & 255)); buf >>= 8; nb -= 8; } }
    void huff(uint32_t code, int len) { uint32_t r = 0; for (int k = 0; k < len; k++) r |= ((code >> k) & 1u) << (len - 1 - k); put(r, len); }
    void flush() { if (nb > 0) { out.push_back((unsigned char)(buf & 255)); buf = 0; nb = 0; } }
};
static void deflateFixed(const std::vector<unsigned char>& d, std::vector<unsigned char>& z) {
    static const int LBASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const int LEXT[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const int DBASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const int DEXT[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    BitWriter w; w.out.reserve(d.size() / 8 + 1024);
    w.put(1, 1); w.put(1, 2);   // BFINAL = 1, BTYPE = 01 (фиксированные коды)
    auto lit = [&](int s) {
        if (s < 144) w.huff(0x30 + s, 8); else if (s < 256) w.huff(0x190 + s - 144, 9); else if (s < 280) w.huff(s - 256, 7); else w.huff(0xC0 + s - 280, 8);
    };
    const size_t n = d.size(); const int HB = 16; std::vector<int32_t> head((size_t)1 << HB, -1);
    auto hsh = [&](size_t i) { return (uint32_t)(((d[i] << 11) ^ (d[i + 1] << 5) ^ d[i + 2] ^ (d[i] >> 3)) & ((1u << HB) - 1)); };
    size_t i = 0;
    while (i < n) {
        int best = 0; size_t dist = 0;
        if (i + 2 < n) {
            uint32_t h = hsh(i); int32_t c = head[h]; head[h] = (int32_t)i;
            if (c >= 0 && i - (size_t)c <= 32768) {
                size_t maxL = std::min<size_t>(258, n - i), L = 0;
                while (L < maxL && d[(size_t)c + L] == d[i + L]) L++;
                if (L >= 3) { best = (int)L; dist = i - (size_t)c; }
            }
        }
        if (best >= 3) {
            int k = 28; while (LBASE[k] > best) k--;
            lit(257 + k); if (LEXT[k]) w.put((uint32_t)(best - LBASE[k]), LEXT[k]);
            int q = 29; while (DBASE[q] > (int)dist) q--;
            w.huff((uint32_t)q, 5); if (DEXT[q]) w.put((uint32_t)(dist - DBASE[q]), DEXT[q]);
            size_t e = std::min(i + best, n >= 2 ? n - 2 : 0);
            for (size_t j = i + 1; j < e; j++) head[hsh(j)] = (int32_t)j;
            i += best;
        } else { lit(d[i]); i++; }
    }
    lit(256); w.flush();
    // обёртка zlib: заголовок, данные, контрольная сумма Adler-32
    uint32_t a = 1, b = 0; for (unsigned char c : d) { a = (a + c) % 65521; b = (b + a) % 65521; }
    z.clear(); z.reserve(w.out.size() + 6); z.push_back(0x78); z.push_back(0x01);
    z.insert(z.end(), w.out.begin(), w.out.end());
    uint32_t ad = (b << 16) | a; z.push_back((unsigned char)(ad >> 24)); z.push_back((unsigned char)(ad >> 16)); z.push_back((unsigned char)(ad >> 8)); z.push_back((unsigned char)ad);
}
// rgb — строки сверху вниз, 3 байта на пиксель
static bool writePNG(const std::wstring& path, int w, int h, const std::vector<unsigned char>& rgb) {
    std::vector<unsigned char> raw((size_t)(w * 3 + 1) * h);
    for (int y = 0; y < h; y++) {
        unsigned char* o = &raw[(size_t)y * (w * 3 + 1)]; const unsigned char* s = &rgb[(size_t)y * w * 3];
        o[0] = 1;   // фильтр Sub
        for (int x = 0; x < w * 3; x++) o[1 + x] = (unsigned char)(s[x] - (x >= 3 ? s[x - 3] : 0));
    }
    std::vector<unsigned char> z; deflateFixed(raw, z);
    FILE* f = _wfopen(path.c_str(), L"wb"); if (!f) return false;
    auto be32 = [&](uint32_t v) { unsigned char b[4] = {(unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v}; fwrite(b, 1, 4, f); };
    auto chunk = [&](const char* type, const unsigned char* data, size_t len) {
        be32((uint32_t)len); fwrite(type, 1, 4, f); if (len) fwrite(data, 1, len, f);
        uint32_t c = crc32((const unsigned char*)type, 4); c = crc32(data, len, c); be32(c ^ 0xFFFFFFFFu);
    };
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10}; fwrite(sig, 1, 8, f);
    unsigned char ih[13] = {(unsigned char)(w >> 24), (unsigned char)(w >> 16), (unsigned char)(w >> 8), (unsigned char)w,
                            (unsigned char)(h >> 24), (unsigned char)(h >> 16), (unsigned char)(h >> 8), (unsigned char)h, 8, 2, 0, 0, 0};
    chunk("IHDR", ih, 13); chunk("IDAT", z.data(), z.size()); chunk("IEND", nullptr, 0);
    bool ok = ferror(f) == 0; fclose(f); return ok;
}
// чтение прямоугольника заднего буфера (y — от верхнего края окна), строки сверху вниз
static std::vector<unsigned char> readRGB(int x, int y, int w, int h) {
    std::vector<unsigned char> px((size_t)w * h * 3), out((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1); glReadBuffer(GL_BACK);
    glReadPixels(x, winH - y - h, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    for (int r = 0; r < h; r++) memcpy(&out[(size_t)r * w * 3], &px[(size_t)(h - 1 - r) * w * 3], (size_t)w * 3);
    return out;
}
static void savePPM(const char* name) {
    std::vector<unsigned char> px = readRGB(0, 0, winW, winH);
    FILE* f = fopen(name, "wb");
    if (f) { fprintf(f, "P6 %d %d 255\n", winW, winH); fwrite(px.data(), px.size(), 1, f); fclose(f); }
}
// ---- снимки и запись кадров: захват выполняется в renderFrame (после отрисовки сцены / всего окна)
static int shotPending = 0;   // 1 — только сцена, 2 — всё окно
static std::wstring recDir; static int recTick = 0;
static std::wstring stampName() { SYSTEMTIME st; GetLocalTime(&st); wchar_t b[64]; swprintf(b, 64, L"%04d%02d%02d_%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond); return b; }
static void saveScreenshot(bool sceneOnly) { shotPending = sceneOnly ? 1 : 2; }
static void doCapture(int mode) {
    int x = mode == 1 ? (int)sceneX : 0, y = mode == 1 ? (int)sceneY : 0, w = mode == 1 ? (int)sceneW : winW, h = mode == 1 ? (int)sceneH : winH;
    std::vector<unsigned char> px = readRGB(x, y, w, h);
    const bool en = LANG == LANG_EN;   // имена файлов — на языке интерфейса
    std::wstring name = L"atoms_" + stampName() + (mode == 1 ? (en ? L"_scene.png" : L"_сцена.png") : (en ? L"_window.png" : L"_окно.png")), path = outputDir(en) + L"\\" + name;
    bool ok = writePNG(path, w, h, px);
    showToast(ok ? std::string(T("Снимок: ")) + narrow(name) : std::string("Не удалось записать снимок"));
}
static bool recEN = false;   // язык имён кадров текущей записи (выбирается при старте записи)
static void toggleRecording() {
    if (recording) { recording = false; showToast(fmt("Запись остановлена: %d кадров → ", recFrames) + narrow(recDir.substr(recDir.find_last_of(L"\\/") + 1))); return; }
    recEN = LANG == LANG_EN;
    recDir = outputDir(recEN) + (recEN ? L"\\frames_" : L"\\кадры_") + stampName();
    if (!CreateDirectoryW(recDir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) { showToast("Не удалось создать папку для кадров"); return; }
    recording = true; recFrames = 0; recTick = 0;
    showToast("Запись кадров сцены в PNG. Ctrl+F12 — остановить");
}
static void recordFrame() {
    if (!recording || ++recTick % std::max(1, opt.recStep) != 0) return;
    std::vector<unsigned char> px = readRGB((int)sceneX, (int)sceneY, (int)sceneW, (int)sceneH);
    wchar_t nm[32]; swprintf(nm, 32, recEN ? L"\\frame_%05d.png" : L"\\кадр_%05d.png", recFrames);
    if (writePNG(recDir + nm, (int)sceneW, (int)sceneH, px)) recFrames++;
    if (recFrames >= 20000) toggleRecording();
}

// --shot … rec=ПАПКА every=K from=F: кадры сцены для анимаций в README (каждый K-й кадр, начиная с F-го)
static std::wstring clipDir; static int clipEvery = 2, clipFrom = 0, clipCount = 0, clipTick = 0;
static void clipCapture() {
    if (clipDir.empty() || ++clipTick <= clipFrom || (clipTick - clipFrom) % clipEvery) return;
    wchar_t nm[32]; swprintf(nm, 32, L"\\f_%04d.png", clipCount++);
    writePNG(clipDir + nm, (int)sceneW, (int)sceneH, readRGB((int)sceneX, (int)sceneY, (int)sceneW, (int)sceneH));
}

// ===================================== КЛАВИАТУРА ==========================================
static void selectAll() {
    std::vector<int> v; for (int i = 0; i < S.n; i++) if (!EL[S.ty[i]].fixed) v.push_back(i);
    selSetList(v); showToast(fmt("Выделено атомов: %d", (int)v.size()));
}
static void loadPresetKey(int p) { openScene(p); }   // повтор той же клавиши — следующий вариант, объекты пользователя сохраняются
static void handleKeys() {
    for (int k : in.keys) {
        const bool ctrl = isDown(VK_CONTROL), shift = isDown(VK_SHIFT), fly = camMode == 1;
        if (k >= 0x10000) {   // Alt+клавиша: выбор инструмента
            int vk = k - 0x10000;
            if (vk >= '0' && vk <= '9') setTool(TOOL_BY_DIGIT[vk - '0']);
            else if (vk == 'F') setTool(TOOL_FIELD);
            continue;
        }
        if (ctrl) {
            if (k == 'Z') cmdUndo();
            else if (k == 'S') cmdSave();
            else if (k == 'O') cmdLoad();
            else if (k == 'E') exportCSV();
            else if (k == 'A') selectAll();
            else if (k == 'C') selCopy();
            else if (k == 'X') { selCopy(); selDelete(); }
            else if (k == 'V') { double x, y, z; cursorPoint(x, y, z); pasteAt(x, y, z); }
            else if (k == VK_F12) toggleRecording();
            else if (k == 'L') setLanguage(LANG == LANG_EN ? LANG_RU : LANG_EN);   // язык интерфейса RU/EN
            continue;
        }
        if (k == VK_F8) { settingsOn = !settingsOn; ptOn = menuOn = helpOn = atomViewOn = false; continue; }
        if (settingsOn) { if (k == VK_ESCAPE) settingsOn = false; continue; }   // пока открыты настройки, клавиши сцены не работают
        if (k == VK_F7) { if (!atomViewOn && hoverAtom() >= 0) avZ = EL[S.ty[hoverAtom()]].Z; atomViewOn = !atomViewOn; ptOn = menuOn = helpOn = false; continue; }
        if (atomViewOn) { if (k == VK_ESCAPE) atomViewOn = false; if (k == 'E') { atomViewOn = false; ptOn = true; } continue; }
        if (k >= '0' && k <= '9') {
            int p = k - '0';
            if (shift) { if (p >= 1 && p <= 5) { ptOn = menuOn = false; openScene(10 + p); } continue; }   // Shift+1…5 — новые сцены
            loadPresetKey(p); continue;
        }
        if (k == VK_TAB) { menuOn = !menuOn; ptOn = settingsOn = helpOn = false; continue; }
        if (k == VK_ESCAPE && (ptOn || menuOn)) { ptOn = menuOn = false; continue; }
        if (k == 'E' && !fly) { ptOn = !ptOn; menuOn = false; helpOn = false; continue; }
        if (fly && (k == 'W' || k == 'A' || k == 'S' || k == 'D' || k == 'Q' || k == 'E')) continue;   // эти клавиши двигают камеру
        double o[3], d[3]; mouseRay(mouseX, mouseY, o, d);
        switch (k) {
        case VK_SPACE: P.paused = !P.paused; break;
        case 'S': P.paused = true; if (world != W_MD) worldStep(1.0 / 30); else { mdStep(); analysisTick(); } break;
        case 'R': cmdReset(); break;
        case 'O': cam3.autoRot = !cam3.autoRot; break;
        case 'H': helpOn = !helpOn; break;
        case 'G': graphsOn = !graphsOn; viewFitPending = true; break;
        case 'T': trailsOn = !trailsOn; break;
        case 'B': bondsOn = !bondsOn; break;
        case 'C': colorMode = (colorMode + 1) % COLOR_N; showToast(std::string(T("Цвет: ")) + T(COLOR_NAMES[colorMode])); break;
        case 'P': saveScreenshot(true); break;
        case VK_F12: saveScreenshot(!shift); break;
        case 'F': case VK_HOME: fitView(false); followAtom = -1; break;
        case 'M': resetMSD(); break;
        case 'U': reverseTime(); break;
        case 'I': if (!selList.empty()) selPin(); else showToast("Нечего закреплять: выделите атомы (Alt+2)"); break;
        case 'N': if (selFieldObj >= 0 && selFieldObj < (int)fieldObjs.size()) { foEditBegin(); fieldObjs[selFieldObj].on = !fieldObjs[selFieldObj].on; foEditEnd();
                      showToast(fieldObjs[selFieldObj].on ? "Объект включён" : "Объект выключен"); } break;
        case VK_DELETE: case VK_BACK:
            if (selFieldObj >= 0 && selFieldObj < (int)fieldObjs.size()) foDelete(selFieldObj);
            else if (!selList.empty()) selDelete();
            break;
        case 'V':
            camMode ^= 1; showToast(camMode ? "Полёт: WASD — движение, Q/E — вниз/вверх, Shift — быстрее, Ctrl+ЛКМ — осмотреться" : "Камера: орбита вокруг центра");
            break;
        case 'X':
            sliceOn = !sliceOn; sliceOff = 0; showToast(sliceOn ? "Разрез: видно только то, что за плоскостью. Shift+колесо — сдвиг" : "Разрез выключен");
            break;
        case 'K': {
            double x, y, z; cursorPoint(x, y, z);
            bool same = P.catalyst && std::hypot(std::hypot(P.catX - x, P.catY - y), P.catZ - z) < 1;
            P.catalyst = !same; P.catX = x; P.catY = y; P.catZ = z; P.catR = std::max(3.0, P.brushR * 1.5);
            showToast(P.catalyst ? "Катализатор: Ea×0.25 внутри" : "Катализатор убран"); break; }
        case 'L': {   // вспышка света длины волны lightNm вдоль луча под курсором
            const double E = photonEV(lightNm); int hit = 0; double dMin = 0;
            const int c = lightFlash(o, d, P.brushR, E * cfg::EV, &hit, &dMin);
            if (!hit) showToast(fmt("Свет %.0f нм (%.2f эВ): под курсором нет связей", lightNm, E));
            else if (!c) showToast(fmt("Свет %.0f нм (%.2f эВ) прошёл насквозь: здесь его ничто не поглощает (нужно от %.2f эВ) — ярче светить бесполезно", lightNm, E, dMin / cfg::EV));
            else showToast(fmt("Свет %.0f нм (%.2f эВ): разорвано связей — %d из %d", lightNm, E, c, hit));
            break; }
        case VK_OEM_4: case VK_OEM_6:   // шар и цилиндр вписаны в ящик: одноосная деформация вытолкнула бы атомы за их стенку
            if (P.container != CT_BOX) { showToast("Сжатие и растяжение по x — только без сосуда (вкладка «Сцена»)"); break; }
            pushUndo(); strainX(k == VK_OEM_4 ? 1.0 / 1.02 : 1.02); showToast(k == VK_OEM_4 ? "Сжатие по x −2%" : "Растяжение по x +2%"); break;
        case VK_LEFT: camRotate(-0.12, 0); break;
        case VK_RIGHT: camRotate(0.12, 0); break;
        case VK_UP: camRotate(0, 0.08); break;
        case VK_DOWN: camRotate(0, -0.08); break;
        case VK_PRIOR: camGoal.dist = clampv(camGoal.dist / 1.15, 2.0, 2000.0); break;
        case VK_NEXT: camGoal.dist = clampv(camGoal.dist * 1.15, 2.0, 2000.0); break;
        case VK_F1: openManual(); break;
        case VK_F2: setView(0.0, 0.02); showToast("Вид спереди"); break;
        case VK_F3: setView(PI / 2, 0.02); showToast("Вид сбоку"); break;
        case VK_F4: setView(camGoal.yaw, 1.45); showToast("Вид сверху"); break;
        case VK_F6: setView(0.65, 0.42); showToast("Изометрия"); break;
        case VK_F5: if (world != W_MD) cmdSave(); else saveState(exeDir() + L"\\quicksave.atoms", true); break;
        case VK_F9: loadState(exeDir() + L"\\quicksave.atoms"); break;
        case VK_F11: toggleFullscreen(); break;
        case VK_ESCAPE:   // закрывает то, что открыто, по очереди; программу не закрывает
            if (helpOn) helpOn = false;
            else if (foPlacing || rubberOn || throwDrag) { foPlacing = rubberOn = throwDrag = false; }
            else if (!selList.empty()) selClear();
            else if (selFieldObj >= 0) selFieldObj = -1;
            else if (measN > 0) measN = 0;
            else if (followAtom >= 0) { followAtom = -1; showToast("Слежение за атомом выключено"); }
            else if (fullscreen) toggleFullscreen();
            break;
        }
    }
    in.keys.clear();
}

// ===================================== МЫШЬ =================================================
enum { LM_NONE, LM_CAMERA, LM_TWEEZER, LM_ADD, LM_WALL, LM_PISTON, LM_BRUSH, LM_RUBBER, LM_MOVESEL, LM_THROW, LM_PUSH, LM_CUT, LM_FOMOVE, LM_FOPLACE, LM_SNAP };
static double snapLast[3] = {-1e9, -1e9, -1e9};   // последний узел точной расстановки (ряд Shift+протяжкой)
// точная расстановка: одна молекула в узел сетки (вещество палитры или молекула библиотеки); false — место занято
static bool snapPlace(double x, double y, double z) {
    const bool ok = chemStampActive() ? insertMolecule(chemStampMol, x, y, z) : placeMol(palette[clampv(selPal, 1, (int)palette.size() - 1)], x, y, z, P.Tset, 0.85);
    if (ok) { updatePresence(); computeForces(); resetEnergyRef(); }
    return ok;
}
static int lMode = LM_NONE;
static int lDragTool = 0;   // инструмент штриха ЛКМ (1 ластик, 2 нагрев, 3 холод)
static double moveDepth = 0, lastW[3] = {0, 0, 0}, throwW0[3] = {0, 0, 0};
static int foDragIdx = -1, foDragPart = 0; static float cutX0 = 0, cutY0 = 0;
static bool rubberAdd = false; static int rubberAtom = -1; static bool rDeleting = false;
static PR foFlyRect = {0, 0, 0, 0};
// курсор над элементами интерфейса, лежащими поверх сцены (столбец видов объектов поля)
static bool overSceneUI(int x, int y) {
    if (lmbTool != TOOL_FIELD) return false;
    float bs = uiPx(30), fx = sceneX + uiPx(8), fy = sceneY + uiPx(46), hh = FO_N * (bs + uiPx(2)) + uiPx(24);
    if (fy + hh > sceneY + sceneH - uiPx(8)) { bs = std::max(uiPx(20), (sceneH - uiPx(70)) / FO_N - uiPx(2)); hh = FO_N * (bs + uiPx(2)) + uiPx(24); }
    return x >= fx && x < fx + bs + uiPx(8) && y >= fy && y < fy + hh;
}
static inline bool sceneHit(int x, int y) { return inScene(x, y) && !overSceneUI(x, y) && !helpOn; }
// мировая точка под курсором на глубине depth
static void worldAt(double mx, double my, double depth, double* p) { unprojectAtDepth(mx, my, depth, p[0], p[1], p[2]); }
static void handleMouse(double frameDt) {
    const bool ctrl = isDown(VK_CONTROL), shift = isDown(VK_SHIFT), alt = isDown(VK_MENU);
    const bool cut = sliceOn;
    selValidate();
    if (anyOverlay()) {   // открыто окно поверх сцены — сцена ввод не получает
        in.wheel = 0; in.lPress = in.lRel = in.mPress = in.rPress = in.dbl = false; heatBrush = 0;
        grabbed = -1; panning = rotating = false; wallStroke = false; lInScene = false; pistonGrab = false; lDragTool = 0;
        lMode = LM_NONE; rubberOn = throwDrag = foPlacing = false;
        return;
    }
    if (world == W_WAVE || world == W_SEMI) {   // плоские картины: камеры нет; у волны щелчок — измерение положения электрона
        if (world == W_WAVE && sceneHit(mouseX, mouseY)) wvMouse();
        in.wheel = 0; in.lPress = in.lRel = in.mPress = in.rPress = in.dbl = false; heatBrush = 0; return;
    }
    const bool over = sceneHit(mouseX, mouseY);
    int part = 0;
    foHover = over && lMode != LM_FOMOVE ? foHitTest((float)mouseX, (float)mouseY, part) : (lMode == LM_FOMOVE ? foDragIdx : -1);
    // колесо: Alt — поворот выделения; над объектом поля — радиус (Shift — сила); иначе наезд камеры
    // (в полёте — движение вперёд), Shift+колесо при разрезе — сдвиг плоскости
    if (in.wheel != 0 && inScene(mouseX, mouseY)) {
        double f = std::pow(1.15, in.wheel / 120.0 * opt.zoomSens), notch = in.wheel / 120.0;
        if (alt && !selList.empty()) selRotate(notch * 5 * PI / 180);
        else if (foHover >= 0 && over) foWheel(foHover, notch, shift);
        else if (cut && shift) sliceOff = clampv(sliceOff + 0.6 * notch, -cam3.dist, 2.0 * cam3.dist);
        else if (camMode == 1) { double L = std::max({S.Lx, S.Ly, S.Lz}); camTranslate(camF[0] * 0.04 * L * notch, camF[1] * 0.04 * L * notch, camF[2] * 0.04 * L * notch); }
        else camGoal.dist = clampv(camGoal.dist / f, 2.0, 2000.0);
    }
    in.wheel = 0;
    double o[3], d[3]; mouseRay(mouseX, mouseY, o, d);
    // ПКМ по значку объекта поля — удалить (нагрев при этом не включается до отпускания)
    if (in.rPress && over && foHover >= 0) { foDelete(foHover); rDeleting = true; }
    if (!in.rDown) rDeleting = false;
    // двойной щелчок: по атому — камера следит за ним, по пустому месту — перестаёт
    if (in.dbl) {
        in.dbl = false;
        if (over && lmbTool != TOOL_MEASURE) {
            int h = hoverAtom();
            if (h >= 0 && !EL[S.ty[h]].fixed) { followAtom = h; showToast(fmt("Камера следит за атомом #%d (%s). Esc — стоп", h, EL[S.ty[h]].sym)); }
            else if (followAtom >= 0) { followAtom = -1; showToast("Слежение выключено"); }
        }
    }
    if (in.lPress) {
        lInScene = over;
        lMode = LM_NONE; lDragTool = 0;
        if (lInScene) {
            dragX = mouseX; dragY = mouseY;
            // поршень можно тянуть мышью: курсор на его плоскости и не на атоме
            bool nearPiston = false;
            if (P.boundary == B_PISTON && lmbTool == TOOL_ADD && std::fabs(d[1]) > 1e-9 && hoverAtom() < 0) {
                const double t = (S.Ly - o[1]) / d[1], px = o[0] + d[0] * t, pz = o[2] + d[2] * t;
                nearPiston = t > 0 && px > 0 && px < S.Lx && pz > 0 && pz < S.Lz;
            }
            if (ctrl || lmbTool == TOOL_CAMERA) { if (!shift) rotating = true; else panning = true; lMode = LM_CAMERA; }
            else if (nearPiston) { pistonGrab = true; pistonTarget = S.Ly; moveDepth = viewDepth(S.Lx / 2, S.Ly, S.Lz / 2); lMode = LM_PISTON; }
            else switch (lmbTool) {
            case TOOL_ERASE: case TOOL_HEAT: case TOOL_COOL: lDragTool = lmbTool; if (lmbTool == TOOL_ERASE) pushUndo(); lMode = LM_BRUSH; break;
            case TOOL_ADD:
                if (palette[selPal].tool == 1) { pushUndo(); wallStroke = true; lastWallX = lastWallY = lastWallZ = 1e9; lMode = LM_WALL; }
                else {
                    int h = hoverAtom();
                    if (h >= 0 && !EL[S.ty[h]].fixed && !placeSnap) { grabbed = h; grabDepth = pdep[h]; grabX = S.x[h]; grabY = S.y[h]; grabZ = S.z[h]; lMode = LM_TWEEZER; }
                    else if (placeSnap) {   // точно по сетке: одна молекула за щелчок, Shift+протяжка — ряд
                        double p[3];
                        if (snapPoint(mouseX, mouseY, p[0], p[1], p[2])) { pushUndo(); if (!snapPlace(p[0], p[1], p[2])) { undoStack.pop_back(); showToast("Узел занят — выберите свободное место"); } for (int q = 0; q < 3; q++) snapLast[q] = p[q]; }
                        lMode = LM_SNAP;
                    }
                    else if (chemStampActive()) {   // «штамп» молекулы из библиотеки (вкладка «Химия»): одна структура за щелчок
                        double p[3]; cursorPoint(p[0], p[1], p[2]); pushUndo();
                        if (insertMolecule(chemStampMol, p[0], p[1], p[2])) showToast(std::string(T("Вставлено: ")) + T(palette[0].label));
                        else { undoStack.pop_back(); showToast("Не помещается — выберите свободное место"); }
                    }
                    else { pushUndo(); lastAddT = 1; lMode = LM_ADD; }
                }
                break;
            case TOOL_SELECT: {
                int h = hoverAtom();
                if (h >= 0 && isSel(h)) {
                    double c[3]; selCOM(c[0], c[1], c[2]); moveDepth = viewDepth(c[0], c[1], c[2]);
                    if (alt) { throwDrag = true; throwX0 = (float)mouseX; throwY0 = (float)mouseY; worldAt(mouseX, mouseY, moveDepth, throwW0); lMode = LM_THROW; }
                    else { pushUndo(); worldAt(mouseX, mouseY, moveDepth, lastW); lMode = LM_MOVESEL; }
                } else { rubberOn = true; rubX0 = rubX1 = (float)mouseX; rubY0 = rubY1 = (float)mouseY; rubberAdd = shift; rubberAtom = h; lMode = LM_RUBBER; }
                break; }
            case TOOL_PUSH: lMode = LM_PUSH; break;
            case TOOL_SHOCK: { double c[3]; cursorPoint(c[0], c[1], c[2]); shockWave(c[0], c[1], c[2]); break; }
            case TOOL_CUT: { pushUndo(); cutX0 = (float)mouseX; cutY0 = (float)mouseY; int c = cutAlong(cutX0, cutY0, cutX0, cutY0); if (c) showToast(fmt("Разорвано связей: %d", c)); lMode = LM_CUT; break; }
            case TOOL_MEASURE: {
                int h = hoverAtom();
                if (h < 0) measN = 0;
                else { if (measN >= 4) measN = 0; if (measN == 0 || measIdx[measN - 1] != h) measIdx[measN++] = h; }
                break; }
            case TOOL_FIELD: {
                int ph = 0, fh = foHitTest((float)mouseX, (float)mouseY, ph);
                if (fh >= 0) {
                    selFieldObj = fh; foDragIdx = fh; foDragPart = ph; const FieldObj& ob = fieldObjs[fh];
                    double cx = ph == 2 ? ob.x2 : ob.x, cy = ph == 2 ? ob.y2 : ob.y, cz = ph == 2 ? ob.z2 : ob.z;
                    moveDepth = viewDepth(cx, cy, cz); worldAt(mouseX, mouseY, moveDepth, lastW); lMode = LM_FOMOVE;
                } else {
                    double p[3]; cursorOnMidPlane(mouseX, mouseY, p[0], p[1], p[2]);
                    if (foKind == FO_WIND || foKind == FO_EMITTER || foKind == FO_BARRIER) { foPlacing = true; for (int q = 0; q < 3; q++) foP0[q] = foP1[q] = p[q]; lMode = LM_FOPLACE; }
                    else { foDragIdx = foCreate(foKind, p, p, false); foDragPart = 0; moveDepth = viewDepth(p[0], p[1], p[2]); worldAt(mouseX, mouseY, moveDepth, lastW); lMode = LM_FOMOVE; }
                }
                break; }
            default: break;
            }
        }
    }
    if (in.lDown && lInScene) {
        int mdx = mouseX - dragX, mdy = mouseY - dragY; dragX = mouseX; dragY = mouseY;
        switch (lMode) {
        case LM_CAMERA:
            if (rotating) { const double k = 0.008 * opt.mouseSens; camRotate(-mdx * k, (opt.invertY ? -mdy : mdy) * k); }
            else { double k = cam3.dist / focal; camTranslate((-camR[0] * mdx + camU[0] * mdy) * k, (-camR[1] * mdx + camU[1] * mdy) * k, (-camR[2] * mdx + camU[2] * mdy) * k); followAtom = -1; }
            break;
        case LM_PISTON: { double p[3]; worldAt(mouseX, mouseY, moveDepth, p); pistonTarget = clampv(p[1], 3.0, 400.0); break; }
        case LM_TWEEZER: if (grabbed >= 0) unprojectAtDepth(mouseX, mouseY, grabDepth, grabX, grabY, grabZ); break;
        case LM_WALL: {
            // стена — «занавес» из неподвижных атомов вдоль штриха и вдоль оси, ближайшей к направлению взгляда
            double wx, wy, wz; unprojectAtDepth(mouseX, mouseY, cam3.dist, wx, wy, wz);
            int ax = 2; double a0 = std::fabs(camF[0]), a1 = std::fabs(camF[1]), a2 = std::fabs(camF[2]); ax = a0 > a1 && a0 > a2 ? 0 : (a1 > a2 ? 1 : 2);
            double Lax = ax == 0 ? S.Lx : (ax == 1 ? S.Ly : S.Lz);
            int before = S.n;
            auto put = [&](double px, double py, double pz) {
                for (double s2 = 0.5; s2 < Lax; s2 += 0.9) {
                    double p[3] = {px, py, pz}; p[ax] = s2;
                    if (p[0] > 0 && p[1] > 0 && p[2] > 0 && p[0] < S.Lx && p[1] < S.Ly && p[2] < S.Lz && !overlaps(E_WALL, p[0], p[1], p[2], 0.85)) addAtom(E_WALL, p[0], p[1], p[2], 0, 0, 0);
                }
            };
            if (lastWallX > 1e8) { lastWallX = wx; lastWallY = wy; lastWallZ = wz; put(wx, wy, wz); }
            else {
                int steps = (int)(std::sqrt((wx - lastWallX) * (wx - lastWallX) + (wy - lastWallY) * (wy - lastWallY) + (wz - lastWallZ) * (wz - lastWallZ)) / 0.9);
                for (int k = 1; k <= steps; k++) { double t = (double)k / steps; put(lastWallX + (wx - lastWallX) * t, lastWallY + (wy - lastWallY) * t, lastWallZ + (wz - lastWallZ) * t); }
                if (steps) { lastWallX = wx; lastWallY = wy; lastWallZ = wz; }
            }
            if (S.n != before) { updatePresence(); computeForces(); resetEnergyRef(); }
            break; }
        case LM_ADD:
            if (inScene(mouseX, mouseY)) {
                lastAddT += frameDt;
                if (lastAddT > 0.03) {
                    lastAddT = 0; int added = 0;
                    double t0 = 0, t1 = 0; bool hit = rayBox(o, d, t0, t1);
                    if (hit && cut) { double dd = d[0] * camF[0] + d[1] * camF[1] + d[2] * camF[2]; if (dd > 1e-6) t0 = std::max(t0, sliceDepth() / dd); hit = t1 > t0; }   // только за плоскостью разреза
                    if (hit) for (int k = 0; k < 8 && added < 2; k++) {
                        // случайная точка в круге радиуса R вокруг луча и по глубине внутри ящика
                        const double a = urand() * 2 * PI, r = P.brushR * std::sqrt(urand()), t = std::max(t0, 0.0) + urand() * (t1 - std::max(t0, 0.0));
                        const double ca = std::cos(a) * r, sa = std::sin(a) * r;
                        const double cx = o[0] + d[0] * t + camR[0] * ca + camU[0] * sa, cy = o[1] + d[1] * t + camR[1] * ca + camU[1] * sa, cz = o[2] + d[2] * t + camR[2] * ca + camU[2] * sa;
                        if (placeMol(palette[selPal], cx, cy, cz, P.Tset, 0.85)) added++;
                    }
                    if (added) { updatePresence(); computeForces(); resetEnergyRef(); }
                }
            }
            break;
        case LM_SNAP: {   // Shift+протяжка — молекулы во всех узлах сетки от прошлого узла до узла под курсором
            double p[3];
            if (shift && snapPoint(mouseX, mouseY, p[0], p[1], p[2]) && (std::fabs(p[0] - snapLast[0]) > 1e-9 || std::fabs(p[2] - snapLast[2]) > 1e-9)) {
                const bool from = snapLast[0] > -1e8;
                const int n = from ? std::max(1, (int)std::lround(std::max(std::fabs(p[0] - snapLast[0]), std::fabs(p[2] - snapLast[2])) / snapStep)) : 1;
                for (int k = 1; k <= n; k++) {
                    const double t = (double)k / n;
                    const double x = from ? std::round((snapLast[0] + (p[0] - snapLast[0]) * t) / snapStep) * snapStep : p[0];
                    const double z = from ? std::round((snapLast[2] + (p[2] - snapLast[2]) * t) / snapStep) * snapStep : p[2];
                    snapPlace(x, p[1], z);
                }
                for (int q = 0; q < 3; q++) snapLast[q] = p[q];
            }
            break; }
        case LM_RUBBER: rubX1 = (float)mouseX; rubY1 = (float)mouseY; break;
        case LM_MOVESEL: {   // выделение догоняет курсор шагами ≤ 0.3σ; упёрлось в соседей — стоит (не «взрывается»)
            double p[3]; worldAt(mouseX, mouseY, moveDepth, p);
            double dv[3] = {p[0] - lastW[0], p[1] - lastW[1], p[2] - lastW[2]}, l = std::sqrt(dv[0] * dv[0] + dv[1] * dv[1] + dv[2] * dv[2]);
            if (l > 1e-9) {
                double k = std::min(1.0, 0.3 / l);
                for (int tr = 0; tr < 3; tr++, k *= 0.5)
                    if (selTranslate(dv[0] * k, dv[1] * k, dv[2] * k)) { for (int q = 0; q < 3; q++) lastW[q] += dv[q] * k; break; }
            }
            break; }
        case LM_PUSH: if (inScene(mouseX, mouseY)) pushBrush(o, d, shift ? -1.0 : 1.0, frameDt, cut ? sliceDepth() : -1e30); break;
        case LM_CUT: {
            int c = cutAlong(cutX0, cutY0, (float)mouseX, (float)mouseY); cutX0 = (float)mouseX; cutY0 = (float)mouseY;
            if (c) showToast(fmt("Разорвано связей: %d", c));
            break; }
        case LM_FOMOVE:
            if (foDragIdx >= 0 && foDragIdx < (int)fieldObjs.size()) {
                double p[3]; worldAt(mouseX, mouseY, moveDepth, p);
                double dx = p[0] - lastW[0], dy = p[1] - lastW[1], dz = p[2] - lastW[2];
                if (dx != 0 || dy != 0 || dz != 0) {
                    FieldObj& ob = fieldObjs[foDragIdx];
                    foEditBegin();
                    if (foDragPart != 2) { ob.x += dx; ob.y += dy; ob.z += dz; }
                    if (foDragPart != 1 && ob.kind == FO_BARRIER) { ob.x2 += dx; ob.y2 += dy; ob.z2 += dz; }
                    foEditEnd();
                }
                for (int q = 0; q < 3; q++) lastW[q] = p[q];
            }
            break;
        case LM_FOPLACE: cursorOnMidPlane(mouseX, mouseY, foP1[0], foP1[1], foP1[2]);  break;
        default: break;
        }
    }
    // «ножницы»: связь под курсором (подсветка)
    if (lmbTool == TOOL_CUT && over && !in.lDown) findCutHover((float)mouseX, (float)mouseY); else if (lMode != LM_CUT) cutHoverA = cutHoverB = -1;
    const bool lTool = in.lDown && lInScene && lDragTool != 0;
    if (in.lRel) {
        switch (lMode) {
        case LM_RUBBER: {
            float w = std::fabs(rubX1 - rubX0), h = std::fabs(rubY1 - rubY0);
            std::vector<int> v = rubberAdd ? selList : std::vector<int>();
            if (w < 4 && h < 4) {   // щелчок: вся молекула атома (Shift — добавить); по пустому месту — снять выделение
                if (rubberAtom >= 0 && rubberAtom < S.n) { std::vector<int> m; moleculeOf(rubberAtom, m); if (m.size() > 5000) m = {rubberAtom}; v.insert(v.end(), m.begin(), m.end()); }
            } else {
                float x0 = std::min(rubX0, rubX1), x1 = std::max(rubX0, rubX1), y0 = std::min(rubY0, rubY1), y1 = std::max(rubY0, rubY1);
                for (int i = 0; i < S.n && i < (int)psx.size(); i++) if (pvis[i] && psx[i] >= x0 && psx[i] <= x1 && psy[i] >= y0 && psy[i] <= y1) v.push_back(i);
            }
            selSetList(v);
            if (!selList.empty()) { if (w >= 4 || h >= 4) showToast(fmt("Выделено атомов: %d · Del — удалить, Ctrl+C — копировать, I — закрепить, Alt+тащить — бросок", (int)selList.size())); }
            break; }
        case LM_THROW: {
            double p[3]; worldAt(mouseX, mouseY, moveDepth, p);
            double k = 0.8 * toolPower, V[3] = {(p[0] - throwW0[0]) * k, (p[1] - throwW0[1]) * k, (p[2] - throwW0[2]) * k};
            double v = std::sqrt(V[0] * V[0] + V[1] * V[1] + V[2] * V[2]);
            if (v > 20) { for (double& c : V) c *= 20 / v; v = 20; }   // не быстрее 20 σ/τ
            selSetVelocity(V[0], V[1], V[2]);
            showToast(fmt("Бросок: скорость центра масс %.2f σ/τ = %.0f м/с (энергия — в W)", v, v * cfg::U_L_NM / cfg::U_T_PS * 1000));
            break; }
        case LM_FOPLACE: {
            double dl = std::sqrt((foP1[0] - foP0[0]) * (foP1[0] - foP0[0]) + (foP1[1] - foP0[1]) * (foP1[1] - foP0[1]) + (foP1[2] - foP0[2]) * (foP1[2] - foP0[2]));
            foCreate(foKind, foP0, foP1, dl > 0.6);
            break; }
        default: break;
        }
        if (pistonGrab) { pistonGrab = false; }
        grabbed = -1; panning = rotating = false; wallStroke = false; lInScene = false; lDragTool = 0; lMode = LM_NONE;
        rubberOn = false; throwDrag = false; foPlacing = false; foDragIdx = -1;
    }
    // ПКМ — нагрев, Shift+ПКМ — охлаждение (вдоль луча под курсором); то же — ЛКМ с инструментом «нагрев»/«холод»
    heatBrush = 0;
    if (((in.rDown && !rDeleting) || (lTool && (lDragTool == TOOL_HEAT || lDragTool == TOOL_COOL))) && inScene(mouseX, mouseY)) {
        heatBrush = (in.rDown && !rDeleting) ? (shift ? -1 : 1) : (lDragTool == TOOL_HEAT ? 1 : -1);
        for (int k = 0; k < 3; k++) { brushO[k] = o[k]; brushD[k] = d[k]; brushF[k] = camF[k]; }
        brushCut = cut ? sliceDepth() : -1e30;
    }
    // СКМ (или ЛКМ с инструментом «ластик») — удаляет молекулы целиком
    if (in.mPress && inScene(mouseX, mouseY)) pushUndo();
    if ((in.mDown || (lTool && lDragTool == TOOL_ERASE)) && inScene(mouseX, mouseY)) {
        bool any = false; double R2 = P.brushR * P.brushR;
        for (int i = S.n - 1; i >= 0; i--) {
            if (i >= S.n) continue;
            if (rayDist2(S.x[i], S.y[i], S.z[i], o, d) < R2 && !(cut && viewDepth(S.x[i], S.y[i], S.z[i]) < sliceDepth())) { removeMolecule(i); any = true; }
        }
        if (any) { updatePresence(); computeForces(); resetEnergyRef(); selValidate(); }
    }
    in.lPress = in.lRel = in.mPress = in.rPress = false;
}

// ===================================== ОКНО / ГЛАВНЫЙ ЦИКЛ ================================
static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE: winW = std::max(200, (int)LOWORD(lp)); winH = std::max(200, (int)HIWORD(lp));  return 0;
    case WM_MOUSEMOVE: mouseX = (short)LOWORD(lp); mouseY = (short)HIWORD(lp); return 0;
    case WM_LBUTTONDOWN: SetCapture(h); in.lDown = true; in.lPress = true; ui.down = true; ui.pressed = true; return 0;
    case WM_LBUTTONDBLCLK: SetCapture(h); in.lDown = true; in.dbl = true; ui.down = true; ui.pressed = true; return 0;   // второй щелчок: кнопки UI срабатывают, в сцене — слежение
    case WM_LBUTTONUP: ReleaseCapture(); in.lDown = false; in.lRel = true; ui.down = false; ui.released = true; return 0;
    case WM_RBUTTONDOWN: in.rDown = true; in.rPress = true; return 0;
    case WM_RBUTTONUP: in.rDown = false; return 0;
    case WM_MBUTTONDOWN: in.mDown = true; in.mPress = true; return 0;
    case WM_MBUTTONUP: in.mDown = false; return 0;
    case WM_MOUSEWHEEL: in.wheel += GET_WHEEL_DELTA_WPARAM(wp); return 0;
    case WM_KEYDOWN: if (!(lp & (1 << 30)) || wp == 'S' || (wp >= VK_LEFT && wp <= VK_DOWN) || wp == VK_PRIOR || wp == VK_NEXT) in.keys.push_back((int)wp); return 0;
    case WM_SYSKEYDOWN:
        if (wp == VK_RETURN && !(lp & (1 << 30))) { in.keys.push_back(VK_F11); return 0; }   // Alt+Enter — полный экран
        if (((wp >= '0' && wp <= '9') || wp == 'F') && !(lp & (1 << 30))) { in.keys.push_back(0x10000 + (int)wp); return 0; }   // Alt+цифра, Alt+F — инструменты
        break;
    case WM_SYSCHAR: if (wp != ' ') return 0; break;   // без системного звука на Alt+клавиша
    case WM_KILLFOCUS: in.lDown = in.rDown = in.mDown = false; return 0;
    case WM_CLOSE: PostQuitMessage(0); return 0;
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
// ---- раскладка: сверху — панель команд, слева — инструменты, справа — боковая панель с вкладками,
//      под сценой — палитра веществ, внизу — строка состояния. Всё масштабируется (uiScale) до 4K.
static float uiScaleOverride = 0;
static float L_statusH = 24, L_stripH = 32, L_toolW = 40, L_sideW = 380;
static float autoUiScale() {
    float s = std::floor(std::min(winH / 1000.0f, winW / 1650.0f) * 4) / 4;
    return clampv(s, 1.0f, 3.0f);
}
static float calcUiScale() {
    if (uiScaleOverride > 0) return uiScaleOverride;   // scale= в командной строке
    if (opt.uiScale > 0) return clampv(opt.uiScale / 100.0f, 0.75f, 3.0f);
    return autoUiScale();
}
static void layout() {
    float s = calcUiScale();
    if (s != uiScale) { uiScale = s; if (texFont) buildFonts(); }
    sceneY = uiPx(34); L_statusH = uiPx(24); L_stripH = uiPx(32); L_toolW = uiPx(40);
    L_sideW = graphsOn ? clampv(std::floor(winW * 0.28f), uiPx(344), uiPx(470)) : 0;
    if (winW - L_toolW - L_sideW < 300) L_sideW = std::max(0.0f, winW - L_toolW - 300);
    sceneX = L_toolW; sceneW = std::max(100.0f, winW - L_toolW - L_sideW); sceneH = std::max(100.0f, winH - sceneY - L_stripH - L_statusH);
}
static void advanceFlashes(float dt) {
    for (auto& f : flashes) f.age += dt;
    flashes.erase(std::remove_if(flashes.begin(), flashes.end(), [](const Flash& a) { return a.age > 0.6f; }), flashes.end());
    for (auto& r : ringsFx) r.age += dt;
    ringsFx.erase(std::remove_if(ringsFx.begin(), ringsFx.end(), [](const RingFx& a) { return a.age > 0.5f; }), ringsFx.end());
}
static void renderFrame(double frameDt) {
    layout();
    selValidate();   // источники/стоки могли удалить атомы во время шагов MD
    if (selFieldObj >= (int)fieldObjs.size()) selFieldObj = -1;
    if (viewFitPending) { fitView(); viewFitPending = false; }
    camUpdate(frameDt, uiTestMode || GetForegroundWindow() == hwnd);
    camSetup();
    glViewport(0, 0, winW, winH);
    glClearColor(C_BG.r, C_BG.g, C_BG.b, 1); glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, winW, winH, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    hotId = -1;
    uiModal = anyOverlay();
    uiClock += frameDt;
    monoCtx = "сцена";
    if (world != W_MD) worldDraw();
    else { drawScene(); pushClip(sceneX, sceneY, sceneW, sceneH); drawRingsFx(); drawPlacementGuide(); popClip(); }
    monoCtx = "надписи сцены"; drawSceneOverlay();
    if (shotPending == 1) { doCapture(1); shotPending = 0; }   // снимок сцены: без панелей, подсказок и уведомлений
    recordFrame(); clipCapture();
    monoCtx = "верхняя панель"; drawTopBar();
    monoCtx = "инструменты"; drawToolbar(0, sceneY, L_toolW, winH - sceneY - L_statusH);
    monoCtx = "палитра веществ"; drawPaletteStrip(sceneX, sceneY + sceneH, sceneW, L_stripH);
    monoCtx = "боковая панель";
    if (graphsOn && L_sideW > 0) drawSidePanel(sceneX + sceneW, sceneY, winW - sceneX - sceneW, winH - sceneY - L_statusH);
    monoCtx = "строка состояния"; drawStatusBar(0, winH - L_statusH, (float)winW, L_statusH);
    monoCtx = "объекты поля (выбор)"; drawFoFlyout();
    uiModal = false;
    monoCtx = "уведомление"; drawToast();
    monoCtx = "подсказка атома"; if (opt.atomCard && !anyOverlay() && !overSceneUI(mouseX, mouseY)) drawTooltip();
    monoCtx = "справка"; if (helpOn) drawHelp();
    monoCtx = "таблица Менделеева"; if (ptOn) drawPeriodicTable();
    monoCtx = "меню сцен"; if (menuOn) drawScenesMenu();
    monoCtx = "настройки"; if (settingsOn) drawSettings();
    monoCtx = "строение атома"; if (atomViewOn) drawAtomView(frameDt);
    monoCtx = "всплывающая подсказка"; drawHint(frameDt);
    monoCtx = "";
    if (shotPending == 2) { doCapture(2); shotPending = 0; }
}

// ===================================== SELFTEST (без окна) ===============================
static double driftPct() { double norm = std::max(std::fabs(Eref), std::max(1.0, EN.ek)); return (EN.total() - Wext - Eref) / norm * 100; }
static int selftest() {
    FILE* f = fopen("selftest.log", "w"); if (!f) return 1;
    initBondTable(); initKlm(); rebuildTables();
    fprintf(f, "threads: %d\n", omp_get_max_threads());
    for (const SceneInfo& sc : SCENES) {   // все сцены меню
        const int k = sc.key; loadPreset(k, sc.var);
        if (k >= 100) {   // мир другого масштаба: минута экранного времени
            for (int s = 0; s < 3600; s++) worldStep(1.0 / 60);
            fprintf(f, "\n[%d] %s\n  %s\n", k, presetTitle.c_str(), worldReport().c_str()); fflush(f);
            continue;
        }
        auto t0 = std::chrono::high_resolution_clock::now();
        const int steps = 1500; double Estart = EN.total(); int rb0 = nlRebuilds;
        for (int s = 0; s < steps; s++) { runScript(); mdStep(); if (s % 50 == 0) analysisTick(); advanceFlashes(0.01f); }
        analysisTick(); computeMolecules();
        double sec = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
        fprintf(f, "\n[%d] %s\n  N=%d  steps=%d  %.2f s (%.0f steps/s, %d пересборок списка)  t=%.2f dt=%.4f\n", k, presetTitle.c_str(), S.n, steps, sec, steps / sec, nlRebuilds - rb0, S.t, P.dt);
        fprintf(f, "  T=%.3f P=%.4f Ek=%.2f Enb=%.2f Ebond=%.2f E=%.3f (start %.3f) Wext=%.3f drift=%.4f%% conserving=%d capped=%d\n",
                EN.T, EN.P, EN.ek, EN.enb, EN.ebond, EN.total(), Estart, Wext, driftPct(), (int)conserving, EN.capped);
        fprintf(f, "  phase=%s  coord=%.2f cryst=%.2f (FCC %d HCP %d BCC %d SC %d) D=%.4f  reactions a/e/d=%lld/%lld/%lld ions %d/%d\n  molecules:",
                A::phase.c_str(), A::meanCoord, A::fCryst, A::stCount[ST_FCC], A::stCount[ST_HCP], A::stCount[ST_BCC], A::stCount[ST_SC], A::D, CH.assoc, CH.exch, CH.diss, A::ionsFree, A::ionsTotal);
        int shown = 0; for (auto& kv : A::mol) if (shown++ < 12) fprintf(f, " %s:%d", kv.first.c_str(), kv.second);
        fprintf(f, "\n");
        if (!A::sceneNote.empty()) fprintf(f, "  note: %s\n", A::sceneNote.c_str());
        fflush(f);
    }
    // распознавание решёток: все варианты пресета 2 при низкой T (без нагрева)
    fprintf(f, "\nLATTICES: 400 шагов при T=0.1\n");
    for (int v = 0; v < presetVariants(2); v++) {
        loadPreset(2, v); P.thermostat = TH_BERENDSEN; P.Tset = v == L_ICE ? 0.05 : 0.1;
        for (int s = 0; s < 400; s++) mdStep();
        analysisTick();
        fprintf(f, "  var %d: N=%d phase=%s coord=%.2f cryst=%.2f  FCC %d HCP %d BCC %d SC %d ICE %d other %d  drift(Berendsen)=%.4f%%\n", v, S.n, A::phase.c_str(), A::meanCoord, A::fCryst,
                A::stCount[ST_FCC], A::stCount[ST_HCP], A::stCount[ST_BCC], A::stCount[ST_SC], A::stCount[ST_ICE], A::stCount[ST_OTHER], driftPct());
        if (v == L_ICE) {   // лёд: устойчивость при нагреве
            P.thermostat = TH_BERENDSEN;
            for (double T : {0.1, 0.2, 0.3, 0.45, 0.6}) {
                P.Tset = T; for (int s = 0; s < 1500; s++) mdStep();
                analysisTick(); analysisTick();
                fprintf(f, "     ice T=%.2f -> Tmeas=%.3f ICE %d of %d, D=%.4f phase=%s\n", T, EN.T, A::stCount[ST_ICE], S.n, A::D, A::phase.c_str());
            }
        }
        fflush(f);
    }
    // проверка NVE: сохранение энергии и импульса (жидкость + газ LJ)
    loadPreset(4, 0); P.thermostat = TH_NVE; resetEnergyRef();
    double e0 = EN.total();
    for (int s = 0; s < 4000; s++) mdStep();
    measure(); double px = 0, py = 0, pz = 0;
    for (int i = 0; i < S.n; i++) { double m = EL[S.ty[i]].m; px += m * S.vx[i]; py += m * S.vy[i]; pz += m * S.vz[i]; }
    fprintf(f, "\nNVE check (preset 4, 4000 steps): E0=%.4f E=%.4f rel=%.2e  |p|=%.2e  N=%d\n", e0, EN.total(), (EN.total() - e0) / std::fabs(e0), std::sqrt(px * px + py * py + pz * pz), S.n);
    // кинетика реакций
    auto kin = [&](int preset, int variant, int total, std::function<void(int)> hook) {
        loadPreset(preset, variant);
        fprintf(f, "\nKINETICS %s\n", presetTitle.c_str());
        for (int s = 0; s <= total; s++) {
            runScript(); if (hook) hook(s); mdStep(); advanceFlashes(0.002f);
            if (s % (total / 6) == 0) {
                computeMolecules(); measure();
                fprintf(f, "  t=%6.1f T=%.3f dt=%.4f cap=%d a/e/d=%lld/%lld/%lld drift=%.4f%% :", S.t, EN.T, P.dt, EN.capped, CH.assoc, CH.exch, CH.diss, driftPct());
                int shown = 0; for (auto& kv : A::mol) if (shown++ < 10) fprintf(f, " %s:%d", kv.first.c_str(), kv.second);
                fprintf(f, "\n"); fflush(f);
            }
        }
    };
    const int K = 24000;
    kin(7, 0, K, nullptr);
    kin(7, 1, K, nullptr);
    kin(0, 0, K, [K](int s) { if (s == K / 2) P.Tset = 3.5; });
    kin(6, 0, 12000, nullptr);
    fclose(f);
    return 0;
}
// ===================================== АВТОТЕСТ ИНТЕРФЕЙСА (--uitest) =====================
// Сценарий синтетического ввода: пресеты, все инструменты (включая выделение, линейку, ножницы, толчок,
// удар и объекты поля всех видов), вкладки боковой панели (в том числе все элементы панелей «Физика» и «Химия»),
// кнопки и слайдеры, слои, клавиши, камера, сохранение/загрузка v3, снимки и запись кадров.
// После каждого шага проверяются инварианты (конечные координаты и энергия, корректные индексы связей и выделения).
struct UiStep { std::string name; int frames; std::function<void(int)> f; };
static std::vector<UiStep> uiSteps; static size_t uiStepIdx = 0; static int uiStepFrame = 0; static FILE* uiLog = nullptr; static int uiProblems = 0;
static void tMouse(float x, float y) { mouseX = (int)x; mouseY = (int)y; }
static void tLDown() { in.lDown = true; in.lPress = true; ui.down = true; ui.pressed = true; }
static void tLUp() { in.lDown = false; in.lRel = true; ui.down = false; ui.released = true; }
static PR rectOf(int id) { auto it = uiRects.find(id); return it == uiRects.end() ? PR{sceneX + 10, sceneY + 10, 1, 1} : it->second; }
static int tabOfId(int id) {   // вкладка боковой панели, на которой находится элемент (−1 — не на панели)
    if ((id >= 100 && id <= 119) || (id >= 200 && id <= 208) || (id >= 211 && id <= 214) || (id >= 790 && id <= 799)) return 0;
    if (id >= 1000 && id < 1200) return 1;
    if (id >= 1200 && id < 1400) return 2;
    if (id >= 800 && id < 900) return 4;
    if (id >= 2200 && id < 2300) return 5;
    return -1;
}
static bool checkInvariants(std::string& why) {
    selValidate();   // интерфейс проверяет выделение перед каждым использованием (сток мог удалить атомы)
    for (int i = 0; i < S.n; i++) {
        if (!std::isfinite(S.x[i]) || !std::isfinite(S.y[i]) || !std::isfinite(S.z[i]) || !std::isfinite(S.vx[i])) { why = fmt("NaN у атома %d", i); return false; }
        if (S.ty[i] < 0 || S.ty[i] >= NEL || S.nbc[i] > cfg::MAXB || S.ghc[i] > 6) { why = fmt("плохой атом %d", i); return false; }
        for (int k = 0; k < S.nbc[i]; k++) { int j = S.nb[i][k]; if (j < 0 || j >= S.n || bondSlot(j, i) < 0) { why = fmt("несимметричная связь %d-%d", i, j); return false; } }
        for (int k = 0; k < S.ghc[i]; k++) { int j = S.gh[i][k]; if (j < 0 || j >= S.n || !isGhost(j, i)) { why = fmt("несимметричный призрак %d-%d", i, j); return false; } }
    }
    if (!std::isfinite(EN.total())) { why = "энергия не конечна"; return false; }
    if (!std::isfinite(Wext)) { why = "работа W не конечна"; return false; }
    if (grabbed >= S.n || followAtom >= S.n) { why = "висячий индекс пинцета/слежения"; return false; }
    if (S.pin.size() != (size_t)S.n) { why = fmt("размер pin %d ≠ N %d", (int)S.pin.size(), S.n); return false; }
    for (int i : selList) if (i < 0 || i >= S.n) { why = "индекс выделения вне диапазона"; return false; }
    for (auto& o : fieldObjs) if (!std::isfinite(o.x) || !std::isfinite(o.y) || !std::isfinite(o.z) || !std::isfinite(o.R) || !std::isfinite(o.strength) || o.kind < 0 || o.kind >= FO_N) {
        why = "объект поля с некорректными параметрами"; return false; }
    if (selFieldObj >= (int)fieldObjs.size()) { why = "висячий индекс объекта поля"; return false; }
    return true;
}
static int uiSavedFO = -1;
static void buildUiTest() {
    auto add = [](const std::string& name, int frames, std::function<void(int)> f) { uiSteps.push_back({name, frames, f}); };
    auto key = [&](int vk, int wait = 3) { add(fmt("клавиша %d", vk), wait, [vk](int fr) { if (fr == 0) in.keys.push_back(vk); }); };
    auto keyMod = [&](int mod, int vk) { add(fmt("клавиша %d+%d", mod, vk), 4, [mod, vk](int fr) { if (fr == 0) { testKeys[mod] = 1; in.keys.push_back(vk); } else testKeys[mod] = 0; }); };
    auto altKey = [&](int vk) { add(fmt("Alt+%c", (char)vk), 3, [vk](int fr) { if (fr == 0) in.keys.push_back(0x10000 + vk); }); };
    // щелчок по элементу: сначала открыть нужную вкладку и прокрутить область к элементу, затем нажать
    auto click = [&](int id) { add(fmt("щелчок по элементу %d", id), 4, [id](int fr) {
        if (fr == 0) { int t = tabOfId(id); if (t >= 0) { graphsOn = true; sideTab = t; } uiScrollTo(id); }
        PR r = rectOf(id);
        if (fr == 1) { uiScrollTo(id); }
        if (fr == 2) { tMouse(r.x + r.w / 2, r.y + r.h / 2); tLDown(); } else if (fr == 3) tLUp(); }); };
    auto slide = [&](int id, float u0, float u1) { add(fmt("слайдер %d", id), 10, [id, u0, u1](int fr) {
        if (fr == 0) { int t = tabOfId(id); if (t >= 0) { graphsOn = true; sideTab = t; } uiScrollTo(id); return; }
        if (fr == 1) { uiScrollTo(id); return; }
        PR r = rectOf(id); float u = u0 + (u1 - u0) * (fr - 2) / 6.0f; tMouse(r.x + uiPx(4) + (r.w - uiPx(8)) * std::min(1.0f, u), r.y + r.h / 2);
        if (fr == 2) tLDown(); else if (fr == 9) tLUp(); }); };
    // жест в сцене: кнопка (0 — ЛКМ, 1 — ПКМ, 2 — СКМ), модификаторы, путь от (u0,v0) до (u1,v1) в долях сцены
    auto gesture = [&](const char* name, int button, int mod1, int mod2, float u0, float v0, float u1, float v1, int frames) {
        add(name, frames + 2, [=](int fr) {
            float t = std::min(1.0f, fr / (float)std::max(1, frames - 1));
            tMouse(sceneX + sceneW * (u0 + (u1 - u0) * t), sceneY + sceneH * (v0 + (v1 - v0) * t));
            if (fr == 0) { if (mod1) testKeys[mod1] = 1; if (mod2) testKeys[mod2] = 1;
                if (button == 0) tLDown(); else if (button == 1) { in.rDown = true; in.rPress = true; } else { in.mDown = true; in.mPress = true; } }
            if (fr == frames) { if (button == 0) tLUp(); else if (button == 1) in.rDown = false; else in.mDown = false; testKeys[mod1] = 0; testKeys[mod2] = 0; }
        }); };
    // видимый подвижный атом, k-й по удалённости от центра сцены (only = только выделенные)
    auto pickAtom = [](int k, bool only) {
        std::vector<std::pair<float, int>> c;
        for (int i = 0; i < S.n && i < (int)psx.size(); i++) { if (!pvis[i] || EL[S.ty[i]].fixed || (only && !isSel(i))) continue;
            float dx = psx[i] - (float)scx, dy = psy[i] - (float)scy; c.push_back({dx * dx + dy * dy, i}); }
        if (c.empty()) return -1;
        std::nth_element(c.begin(), c.begin() + std::min((int)c.size() - 1, k), c.end());
        return c[std::min((int)c.size() - 1, k)].second;
    };
    auto onAtom = [&](const char* name, bool dbl) { add(name, 14, [dbl, pickAtom](int fr) {
        if (fr == 0) { int best = pickAtom(0, false); if (best >= 0) tMouse(psx[best], psy[best]); }
        if (fr == 2) { if (dbl) { in.lDown = true; in.dbl = true; ui.pressed = true; } else tLDown(); }
        if (!dbl && fr > 2 && fr < 12) tMouse((float)mouseX + 3, (float)mouseY + 2);
        if (fr == 12) tLUp(); }); };
    // перетащить выделенный атом (mod — Alt для броска)
    auto dragSel = [&](const char* name, int mod) { add(name, 14, [mod, pickAtom](int fr) {
        if (fr == 0) { int a = pickAtom(0, true); if (a >= 0) tMouse(psx[a], psy[a]); if (mod) testKeys[mod] = 1; }
        if (fr == 1) tLDown();
        if (fr > 1 && fr < 12) tMouse((float)mouseX + 4, (float)mouseY - 3);
        if (fr == 12) { tLUp(); } if (fr == 13) testKeys[mod] = 0; }); };
    auto clickAtomK = [&](int k) { add(fmt("щелчок по атому %d", k), 4, [k, pickAtom](int fr) {
        if (fr == 0) { int a = pickAtom(k, false); if (a >= 0) tMouse(psx[a], psy[a]); }
        if (fr == 1) tLDown(); if (fr == 2) tLUp(); }); };
    auto run = [&](int frames) { add(fmt("пауза %d кадров", frames), frames, [](int) {}); };
    auto wheelAt = [&](const char* name, int mod, int delta, std::function<void()> aim) { add(name, 5, [=](int fr) {
        if (fr == 0) { aim(); if (mod) testKeys[mod] = 1; } if (fr == 1) in.wheel += delta; if (fr == 3) testKeys[mod] = 0; }); };
    // щёлкнуть все зарегистрированные элементы из диапазона id (панели «Физика»/«Химия»): шаги вставляются во время теста
    auto clickAllRange = [&](int tab, int lo, int hi) { add(fmt("все элементы %d–%d", lo, hi), 3, [tab, lo, hi](int fr) {
        if (fr == 0) { graphsOn = true; sideTab = tab; return; }
        if (fr != 2) return;
        std::vector<int> ids; for (auto& kv : uiRects) if (kv.first >= lo && kv.first < hi) ids.push_back(kv.first);
        std::vector<UiStep> ins;
        for (int id : ids) ins.push_back({fmt("панель: элемент %d", id), 4, [id, tab](int f2) {
            if (f2 == 0) { graphsOn = true; sideTab = tab; uiScrollTo(id); }
            PR r = rectOf(id); if (f2 == 1) { tMouse(r.x + r.w / 2, r.y + r.h / 2); tLDown(); } else if (f2 == 2) tLUp(); }});
        fprintf(uiLog, "     панель %d: элементов %zu\n", tab, ids.size());
        uiSteps.insert(uiSteps.begin() + uiStepIdx + 1, ins.begin(), ins.end()); }); };

    // язык интерфейса: кнопки RU | EN (773/774) и Ctrl+L; после каждого переключения — язык, заголовок окна и уведомление.
    // Сам сценарий идёт на языке запуска (--lang en — на английском).
    const int lang0 = LANG, other = 1 - lang0;
    auto expectLang = [&](int l, bool switched) { add(fmt("проверка языка %d", l), 1, [l, switched](int) {
        wchar_t buf[256] = L""; GetWindowTextW(hwnd, buf, 256);
        bool ok = LANG == l && TW(WINDOW_TITLE) == buf;
        if (switched) ok = ok && toast == (l == LANG_EN ? std::string("Language: English") : std::string(T("Язык: русский")));
        if (!ok) { uiProblems++; fprintf(uiLog, "     ОШИБКА: язык %d (ожидался %d), заголовок окна или уведомление не совпали\n", LANG, l); }
        else fprintf(uiLog, "     язык %s: заголовок окна и уведомление — ok\n", l == LANG_EN ? "EN" : "RU"); }); };
    auto ctrlL = [&]() { keyMod(VK_CONTROL, 'L'); };
    click(773 + other); expectLang(other, true); click(773 + lang0); expectLang(lang0, true);
    click(773 + lang0); expectLang(lang0, false);   // повторный щелчок по активной кнопке — без изменений
    ctrlL(); expectLang(other, true); ctrlL(); expectLang(lang0, true);
    // пресеты и их варианты
    for (int k : {1, 2, 2, 3, 4, 5, 6, 7, 8, 9, 0}) { key('0' + k, 20); }
    for (int v = 0; v < 3; v++) key('2', 16);
    key('7', 30);                                                 // химия: горение
    // палитра: каждый пункт + мазок кистью
    const int np = (int)palette.size();
    click(230 + TOOL_ADD);
    for (int p = 0; p < np; p++) { click(300 + p); gesture("кисть", 0, 0, 0, 0.3f + 0.02f * (p % 5), 0.4f, 0.5f, 0.5f, 6); }
    // таблица Менделеева: выбрать разные элементы и добавить их в сцену
    for (int z : {1, 9, 26, 79, 118, 6}) { key('E', 3); click(400 + z); gesture("атомы элемента", 0, 0, 0, 0.35f, 0.45f, 0.5f, 0.5f, 6); }
    key('E', 3); gesture("щелчок мимо таблицы", 0, 0, 0, 0.01f, 0.99f, 0.01f, 0.99f, 2); key(VK_ESCAPE);
    click(301);   // Ar
    // все инструменты: кнопкой и жестом в сцене
    for (int t : TOOL_ORDER) { if (t < 0) continue; click(230 + t); gesture("инструмент", 0, 0, t == TOOL_CAMERA ? VK_SHIFT : 0, 0.45f, 0.5f, 0.6f, 0.55f, 8); }
    for (int dg = 0; dg <= 9; dg++) altKey('0' + dg);
    altKey('F');
    // объекты поля: каждый вид — поставить (с перетаскиванием), затем колесо, Shift+колесо, N, перенос, удаление
    click(230 + TOOL_FIELD);
    for (int k = 0; k < FO_N; k++) {
        click(740 + k);
        float u = 0.2f + 0.06f * k;
        gesture("поставить объект", 0, 0, 0, u, 0.35f + 0.03f * (k % 3), u + 0.08f, 0.45f, 6);
    }
    wheelAt("колесо над объектом", 0, 240, [] { if (!fieldObjs.empty()) { float x, y, dd, s; const FieldObj& o = fieldObjs.back(); if (project(o.x, o.y, o.z, x, y, dd, s)) tMouse(x, y); } });
    wheelAt("Shift+колесо над объектом", VK_SHIFT, -120, [] { if (!fieldObjs.empty()) { float x, y, dd, s; const FieldObj& o = fieldObjs[0]; if (project(o.x, o.y, o.z, x, y, dd, s)) tMouse(x, y); } });
    key('N'); key('N');
    add("перенос объекта", 12, [](int fr) {
        if (fieldObjs.empty()) return; const FieldObj& o = fieldObjs[0]; float x, y, dd, s;
        if (fr == 0 && project(o.x, o.y, o.z, x, y, dd, s)) tMouse(x, y);
        if (fr == 1) tLDown(); if (fr > 1 && fr < 10) tMouse((float)mouseX + 5, (float)mouseY + 2); if (fr == 10) tLUp(); });
    // вкладка «Объект» с выбранным объектом поля: все элементы инспектора
    for (int id : {800, 800, 810, 811, 812, 813, 814, 816, 817, 802}) { if (id >= 810 && id <= 814) slide(id, 0.3f, 0.6f); else click(id); }
    add("ПКМ по объекту — удалить", 4, [](int fr) {
        if (fieldObjs.empty()) return; const FieldObj& o = fieldObjs.back(); float x, y, dd, s;
        if (fr == 0 && project(o.x, o.y, o.z, x, y, dd, s)) tMouse(x, y); if (fr == 1) { in.rDown = true; in.rPress = true; } if (fr == 2) in.rDown = false; });
    add("выбрать объект", 1, [](int) { if (!fieldObjs.empty()) selFieldObj = 0; });
    key(VK_DELETE);
    run(40);   // объекты работают (источник, сток, барьер…)
    // выделение: рамка, перенос, бросок, поворот, копия, вставка, закрепление, операции инспектора
    click(230 + TOOL_SELECT);
    gesture("рамка", 0, 0, 0, 0.35f, 0.35f, 0.65f, 0.65f, 8);
    dragSel("перенос выделения", 0);
    dragSel("бросок (Alt)", VK_MENU);
    wheelAt("Alt+колесо — поворот", VK_MENU, 120, [] { tMouse((float)scx, (float)scy); });
    gesture("рамка + Shift", 0, VK_SHIFT, 0, 0.2f, 0.2f, 0.3f, 0.3f, 6);
    keyMod(VK_CONTROL, 'C');
    add("курсор в угол", 1, [](int) { tMouse(sceneX + sceneW * 0.15f, sceneY + sceneH * 0.2f); });
    keyMod(VK_CONTROL, 'V');
    key('I'); key('I'); key('I');
    for (int id = 820; id <= 831; id++) if (id != 825 && id != 830) click(id);
    click(824); click(825);
    keyMod(VK_CONTROL, 'A'); key(VK_ESCAPE);
    gesture("рамка 2", 0, 0, 0, 0.45f, 0.45f, 0.55f, 0.55f, 6); key(VK_DELETE);
    clickAtomK(0); key(VK_ESCAPE);
    // линейка / угломер
    click(230 + TOOL_MEASURE);
    for (int k = 0; k < 5; k++) clickAtomK(k * 3);
    click(840);
    gesture("сброс измерения", 0, 0, 0, 0.02f, 0.02f, 0.02f, 0.02f, 2);
    // толчок, удар, ножницы (в химической сцене — есть связи)
    key('7', 20);
    click(230 + TOOL_PUSH); gesture("толчок", 0, 0, 0, 0.4f, 0.5f, 0.6f, 0.5f, 10); gesture("притяжение", 0, VK_SHIFT, 0, 0.6f, 0.4f, 0.4f, 0.6f, 10);
    click(230 + TOOL_SHOCK); gesture("удар", 0, 0, 0, 0.5f, 0.5f, 0.5f, 0.5f, 2); run(15);
    click(230 + TOOL_CUT); gesture("ножницы", 0, 0, 0, 0.2f, 0.3f, 0.8f, 0.7f, 20); gesture("ножницы 2", 0, 0, 0, 0.8f, 0.3f, 0.2f, 0.7f, 20);
    click(230 + TOOL_ADD); click(301);
    onAtom("пинцет", false);
    onAtom("двойной щелчок — слежение", true);
    run(20); key(VK_ESCAPE);
    gesture("нагрев", 1, 0, 0, 0.5f, 0.5f, 0.55f, 0.5f, 12);
    gesture("охлаждение", 1, VK_SHIFT, 0, 0.5f, 0.5f, 0.45f, 0.5f, 12);
    gesture("ластик", 2, 0, 0, 0.4f, 0.4f, 0.6f, 0.6f, 10);
    gesture("вращение/сдвиг", 0, VK_CONTROL, 0, 0.5f, 0.5f, 0.7f, 0.6f, 12);
    gesture("сдвиг камеры", 0, VK_CONTROL, VK_SHIFT, 0.5f, 0.5f, 0.4f, 0.4f, 12);
    add("колесо", 6, [](int fr) { tMouse((float)scx, (float)scy); in.wheel += fr < 3 ? 240 : -360; });
    // вкладки боковой панели и все их элементы
    for (int t = 0; t < TAB_N; t++) { click(700 + t); run(3); }
    clickAllRange(1, 1000, 1200);
    clickAllRange(2, 1200, 1400);
    // вкладка «Сцена»: оси, грани, сосуд, заливка; расстановка по сетке — щелчок и ряд Shift+протяжкой
    clickAllRange(5, 2200, 2300); run(10);
    for (int id : {2241, 2242, 2251, 2252}) slide(id, 0.3f, 0.6f);
    click(2240); click(230 + TOOL_ADD); click(301);
    gesture("по сетке: щелчок", 0, 0, 0, 0.5f, 0.55f, 0.5f, 0.55f, 2);
    gesture("по сетке: ряд", 0, VK_SHIFT, 0, 0.4f, 0.6f, 0.6f, 0.6f, 10);
    click(2240); for (int k = 0; k < 3; k++) click(2200 + k);   // оси — обратно
    for (int id = 200; id <= 208; id++) { click(id); click(id); }
    for (int id = 211; id <= 214; id++) { click(id); click(id); }
    for (int q = 0; q < 4; q++) click(200);   // полный круг термостатов
    for (int id : {100, 101, 102, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116}) { slide(id, 0.2f, 0.8f); slide(id, 0.8f, 0.45f); }
    add("колесо над слайдером", 5, [](int fr) { if (fr == 0) { graphsOn = true; sideTab = 0; uiScrollTo(100); } PR r = rectOf(100); tMouse(r.x + r.w / 2, r.y + r.h / 2); if (fr > 1) in.wheel += fr < 3 ? 120 : -120; });
    for (int id = 790; id <= 797; id++) { click(id); }
    run(20);   // все слои включены
    for (int id = 790; id <= 797; id++) { click(id); }
    add("прокрутка вкладки", 6, [](int fr) { graphsOn = true; sideTab = 3; PR r = rectOf(700); tMouse(r.x + 40, r.y + 200); in.wheel += fr < 3 ? -360 : 360; });
    // верхняя панель
    for (int id : {209, 209, 763, 209, 771, 771, 772, 772, 220, 220, 221, 221, 768, 769, 769}) click(id);
    run(6);
    // клавиши
    for (int vk : std::initializer_list<int>{'C', 'C', 'C', 'C', 'C', 'C', 'B', 'B', 'T', 'T', 'G', 'G', 'H', 'H', 'O', 'O', 'U', 'K', 'K', 'L', 'M', 'F', VK_HOME, VK_PRIOR, VK_NEXT,
                   VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_OEM_4, VK_OEM_6, VK_F2, VK_F3, VK_F4, VK_F6, VK_F1, VK_SPACE, 'S', 'S', VK_SPACE, 'P', VK_F12}) key(vk);
    keyMod(VK_SHIFT, VK_F12); keyMod(VK_CONTROL, VK_F12); run(8); keyMod(VK_CONTROL, VK_F12);
    // полёт камеры и разрез
    key('V', 3);
    add("полёт WASDQE", 36, [](int fr) { int ks[6] = {'W', 'A', 'S', 'D', 'Q', 'E'}; for (int k : ks) testKeys[k] = 0; testKeys[VK_SHIFT] = fr > 18;
        if (fr < 35) testKeys[ks[(fr / 6) % 6]] = 1; else testKeys[VK_SHIFT] = 0; });
    gesture("осмотр в полёте", 0, VK_CONTROL, 0, 0.5f, 0.5f, 0.6f, 0.45f, 8);
    key('V', 3);
    key('X', 3);
    add("сдвиг разреза", 6, [](int fr) { tMouse((float)scx, (float)scy); testKeys[VK_SHIFT] = fr < 5; in.wheel += fr < 3 ? 240 : -120; });
    gesture("кисть при разрезе", 0, 0, 0, 0.45f, 0.5f, 0.55f, 0.5f, 6);
    key('X', 3);
    // сохранение / загрузка v3: объекты поля и закрепление должны восстановиться
    add("объекты для сохранения", 2, [](int fr) { if (fr) return;
        fieldObjs.clear(); double c[3] = {S.Lx / 2, S.Ly / 2, S.Lz / 2};
        fieldObjs.push_back(makeFieldObj(FO_ATTRACT, c[0], c[1], c[2])); fieldObjs.push_back(makeFieldObj(FO_BARRIER, c[0] * 0.5, c[1], c[2]));
        if (S.n > 0 && S.pin.size() == (size_t)S.n) S.pin[0] = 1; uiSavedFO = (int)fieldObjs.size(); });
    key(VK_F5);
    add("очистить объекты", 1, [](int) { fieldObjs.clear(); selFieldObj = -1; });
    key(VK_F9, 10);
    add("проверка загрузки v3", 1, [](int) {
        bool ok = (int)fieldObjs.size() == uiSavedFO && S.n > 0 && S.pin.size() == (size_t)S.n && S.pin[0] == 1;
        if (!ok) { uiProblems++; fprintf(uiLog, "     ОШИБКА: после F9 объектов %d (ожидалось %d), pin[0]=%d\n", (int)fieldObjs.size(), uiSavedFO, S.n > 0 && !S.pin.empty() ? S.pin[0] : -1); }
        else fprintf(uiLog, "     загрузка v3: объекты поля и закрепление восстановлены\n"); });
    keyMod(VK_CONTROL, 'S'); keyMod(VK_CONTROL, 'O'); keyMod(VK_CONTROL, 'E');
    click(767); click(766); click(764); click(765);
    keyMod(VK_CONTROL, 'Z'); keyMod(VK_CONTROL, 'Z'); keyMod(VK_CONTROL, 'Z');
    key(VK_F11, 10); run(20); key(VK_F11, 10); add("Alt+Enter", 10, [](int fr) { if (fr == 0) in.keys.push_back(VK_F11); }); key(VK_ESCAPE, 10);
    // меню сцен (мышью) и новые сцены с клавиатуры
    for (const SceneInfo& s : SCENES) if (s.key > 10) for (int v = 0; v < presetVariants(s.key); v++) { key(VK_TAB, 3); click(sceneBtnId(s.key)); run(20); }
    key(VK_TAB, 3); key(VK_TAB, 3);
    for (int d = 1; d <= 5; d++) keyMod(VK_SHIFT, '0' + d);
    key('7', 20);
    // стена
    click(230 + TOOL_ADD);
    click(300 + np - 1); gesture("рисуем стену", 0, 0, 0, 0.3f, 0.3f, 0.7f, 0.7f, 10); click(300);
    // число частиц: почти до нуля, затем обратно
    slide(103, 0.5f, 0.0f); run(10); slide(103, 0.0f, 0.1f); run(20);
    // крайний случай: всё стёрто при включённых следах на паузе
    key('T'); key(VK_SPACE); gesture("стереть всё", 2, 0, 0, 0.0f, 0.0f, 1.0f, 1.0f, 30);
    add("большой ластик", 2, [](int fr) { if (fr == 0) P.brushR = 400; });
    gesture("стереть всё 2", 2, 0, 0, 0.5f, 0.5f, 0.5f, 0.5f, 4);
    click(230 + TOOL_SELECT); keyMod(VK_CONTROL, 'A'); keyMod(VK_CONTROL, 'V'); key(VK_DELETE);
    for (int t = 0; t < 5; t++) { click(700 + t); run(2); }
    run(20); key(VK_SPACE); run(20); key('T');
    add("кисть обратно", 1, [](int) { P.brushR = 3; });
    key('1', 30);
    expectLang(lang0, false);
    run(30);
    // окно настроек: каждый элемент (кроме языка и кнопок внизу), язык туда и обратно, «По умолчанию», закрытие
    auto setClick = [&](int id) { add(fmt("настройки: %d", id), 5, [id](int fr) {
        if (fr == 0) { settingsOn = true; uiScrollTo(id); }
        PR r = rectOf(id);
        if (fr == 2) { tMouse(r.x + r.w / 2, r.y + r.h / 2); tLDown(); } else if (fr == 3) tLUp(); }); };
    key(VK_F8, 4);
    add("все элементы настроек", 3, [](int fr) {
        if (fr == 0) { settingsOn = true; return; }
        if (fr != 2) return;
        std::vector<int> ids; for (auto& kv : uiRects) if (kv.first > 1600 && kv.first < 1690) ids.push_back(kv.first);
        std::vector<UiStep> ins;
        for (int id : ids) ins.push_back({fmt("настройки: элемент %d", id), 4, [id](int f2) {
            if (f2 == 0) { settingsOn = true; uiScrollTo(id); }
            PR r = rectOf(id); if (f2 == 1) { tMouse(r.x + r.w / 2, r.y + r.h / 2); tLDown(); } else if (f2 == 2) tLUp(); }});
        fprintf(uiLog, "     настройки: элементов %zu\n", ids.size());
        uiSteps.insert(uiSteps.begin() + uiStepIdx + 1, ins.begin(), ins.end()); });
    run(30);   // сцена идёт с изменёнными настройками
    setClick(1692); setClick(1600); setClick(1600); expectLang(lang0, true);
    setClick(1690); setClick(1691);
    add("настройки закрыты", 1, [](int) { if (settingsOn) { uiProblems++; fprintf(uiLog, "     ОШИБКА: окно настроек не закрылось кнопкой «Готово»\n"); } });
    key(VK_F8, 4); key(VK_ESCAPE, 4);
    key(VK_F8, 4); gesture("щелчок мимо настроек", 0, 0, 0, 0.01f, 0.99f, 0.01f, 0.99f, 2);
    add("настройки по умолчанию", 1, [](int) {
        Settings d = OPT_DEFAULT; d.lang = opt.lang;
        if (opt.uiScale != d.uiScale || opt.threads != d.threads || opt.hints != d.hints || opt.fog != d.fog || opt.start != d.start || settingsOn) {
            uiProblems++; fprintf(uiLog, "     ОШИБКА: «По умолчанию» не вернуло настройки или окно не закрылось\n"); }
        else fprintf(uiLog, "     настройки сброшены, окно закрыто — ok\n"); });
    // переключение языка при открытых окнах (справка, меню сцен, таблица, настройки) и на каждой вкладке — туда и обратно
    for (int vk : {(int)'H', (int)VK_TAB, (int)'E', (int)VK_F8}) { key(vk, 4); ctrlL(); run(4); ctrlL(); run(4); expectLang(lang0, true); key(vk == 'H' ? 'H' : VK_ESCAPE, 4); }
    for (int t = 0; t < 5; t++) { click(700 + t); ctrlL(); run(3); ctrlL(); run(3); expectLang(lang0, true); }
    key('1', 20); expectLang(lang0, false);
}
static bool uiTestTick() {
    if (uiStepIdx >= uiSteps.size()) return false;
    std::function<void(int)> f = uiSteps[uiStepIdx].f; int frames = uiSteps[uiStepIdx].frames;
    f(uiStepFrame);   // шаг может вставить новые шаги после себя (копия функции — ссылка на вектор не нужна)
    if (++uiStepFrame >= frames) {
        std::string why;
        bool ok = checkInvariants(why);
        fprintf(uiLog, "%4zu %-34s N=%5d T=%8.3f E=%12.3f W=%10.2f %s%s\n", uiStepIdx, uiSteps[uiStepIdx].name.c_str(), S.n, EN.T, EN.total(), Wext, ok ? "ok" : "ОШИБКА: ", ok ? "" : why.c_str());
        if (!ok) uiProblems++;
        {   // «взрыв» (перекрытие атомов после действия пользователя): ни одна сцена и ни один инструмент не дают T > 50
            static double lastT = 0;
            if (EN.T > 50 && lastT <= 50) { uiProblems++; fprintf(uiLog, "     ОШИБКА: перегрев после шага «%s»: T %.2f → %.2f\n", uiSteps[uiStepIdx].name.c_str(), lastT, EN.T); }
            lastT = EN.T;
        }
        fflush(uiLog);
        uiStepIdx++; uiStepFrame = 0;
    }
    return true;
}

// ---- демонстрационное состояние для снимков (--shot … demo): объекты поля, выделение, измерение
static void setupDemo() {
    const double cz = S.Lz / 2;
    fieldObjs.clear();
    FieldObj a = makeFieldObj(FO_ATTRACT, S.Lx * 0.25, S.Ly * 0.55, cz); fieldObjs.push_back(a);
    FieldObj h = makeFieldObj(FO_HEATER, S.Lx * 0.72, S.Ly * 0.3, cz); fieldObjs.push_back(h);
    FieldObj w = makeFieldObj(FO_WIND, S.Lx * 0.55, S.Ly * 0.75, cz); w.dx = 0.8; w.dy = -0.6; fieldObjs.push_back(w);
    FieldObj b = makeFieldObj(FO_BARRIER, S.Lx * 0.85, S.Ly * 0.6, cz); fieldObjs.push_back(b);
    selFieldObj = 1;
    std::vector<std::pair<double, int>> c;
    for (int i = 0; i < S.n; i++) { if (EL[S.ty[i]].fixed) continue; double dx = S.x[i] - S.Lx * 0.45, dy = S.y[i] - S.Ly * 0.45, dz = S.z[i] - cz; c.push_back({dx * dx + dy * dy + dz * dz, i}); }
    std::sort(c.begin(), c.end());
    std::vector<int> sel; for (size_t k = 0; k < c.size() && k < 24; k++) sel.push_back(c[k].second);
    selSetList(sel);
    for (size_t k = 0; k < sel.size() && k < 4; k++) if (S.pin.size() == (size_t)S.n) S.pin[sel[k]] = 1;
    // измерение: атом с двумя связями (угол) или три ближайших к центру ящика
    measN = 0;
    for (int i = 0; i < S.n && measN == 0; i++) if (S.nbc[i] >= 2) { measIdx[0] = S.nb[i][0]; measIdx[1] = i; measIdx[2] = S.nb[i][1]; measN = 3; }
    if (measN == 0 && c.size() > 40) { measIdx[0] = c[30].second; measIdx[1] = c[34].second; measIdx[2] = c[38].second; measN = 3; }
}

static int shotHoverZ = 0;   // --shot … hover Z: курсор над элементом таблицы (проверка карточки)
static int argInt(const wchar_t* cmd, const wchar_t* key, int def) { const wchar_t* p = cmd ? wcsstr(cmd, key) : nullptr; return p ? (int)wcstol(p + wcslen(key), nullptr, 10) : def; }
static double argDbl(const wchar_t* cmd, const wchar_t* key, double def) { const wchar_t* p = cmd ? wcsstr(cmd, key) : nullptr; return p ? wcstod(p + wcslen(key), nullptr) : def; }
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR cmd, int) {
    if (cmd && wcsstr(cmd, L"--selftest")) return selftest();
    // --gradcheck K шагов: проверка F = −∇U численным дифференцированием в конфигурации пресета K после N шагов
    if (cmd && wcsstr(cmd, L"--gradcheck")) {
        wchar_t* p = wcsstr(cmd, L"--gradcheck") + 11; int K = (int)wcstol(p, &p, 10), steps = (int)wcstol(p, &p, 10);
        initBondTable(); initKlm(); rebuildTables(); loadPreset(K, 0);
        for (int s = 0; s < steps; s++) { runScript(); mdStep(); }
        FILE* f = fopen(fmt("grad_%d.log", K).c_str(), "w"); if (!f) return 1;
        computeMolecules();
        for (auto& kv : A::mol) fprintf(f, "%s:%d ", kv.first.c_str(), kv.second);
        fprintf(f, "\n");
        auto U = [&]() { computeForces(); return EN.enb + EN.ebond + EN.egrav + EN.efo; };
        U(); std::vector<double> F0x = S.fx, F0y = S.fy, F0z = S.fz;
        for (int i = 0; i < S.n; i++) { F0x[i] += S.bx[i]; F0y[i] += S.by[i]; F0z[i] += S.bz[i]; }   // медленные + быстрые
        const double h = 1e-5; double worst = 0; int bad = 0, tested = 0;
        for (int i = 0; i < S.n; i++) {
            if (EL[S.ty[i]].fixed) continue;
            bool interesting = S.nbc[i] > 0 || S.ghc[i] > 0 || i % 7 == 0; if (!interesting) continue;
            for (int c = 0; c < 3; c++) {
                double* X = c == 0 ? &S.x[i] : (c == 1 ? &S.y[i] : &S.z[i]);
                double x0 = *X; *X = x0 + h; double Ep = U(); *X = x0 - h; double Em = U(); *X = x0;
                double num = -(Ep - Em) / (2 * h), an = c == 0 ? F0x[i] : (c == 1 ? F0y[i] : F0z[i]);
                double err = std::fabs(num - an) / std::max(1.0, std::fabs(an));
                tested++;
                if (err > 1e-3) { bad++; if (bad < 40) fprintf(f, "atom %d (%s nb=%d ghost=%d water=%d donor=%d) comp %d: F=%.6f  -dU/dx=%.6f  err=%.2e\n", i, EL[S.ty[i]].sym, S.nbc[i], S.ghc[i], (int)isWaterO(i), hbDonor(i), c, an, num, err); }
                worst = std::max(worst, err);
            }
        }
        U();
        fprintf(f, "tested %d components, bad %d, worst rel err %.3e\n", tested, bad, worst);
        fclose(f); return 0;
    }
    // --evcheck K шагов: баланс энергии каждого химического события и интегратора
    if (cmd && wcsstr(cmd, L"--evcheck")) {
        wchar_t* p = wcsstr(cmd, L"--evcheck") + 9; int K = (int)wcstol(p, &p, 10), steps = (int)wcstol(p, &p, 10);
        initBondTable(); initKlm(); rebuildTables(); loadPreset(K, 0);
        if (wcsstr(cmd, L"nve")) { P.thermostat = TH_NVE; script.clear(); resetEnergyRef(); }
        gEvLog = fopen(fmt("ev_%d.log", K).c_str(), "w"); if (!gEvLog) return 1; gEvCheck = true;
        double lastE = 0; bool have = false, haveStep = false; double Eprev = 0; int jumps = 0;
        auto dumpAtom = [&](int i) {
            if (i < 0 || i >= S.n) return;
            double v = std::sqrt(S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]);
            fprintf(gEvLog, "    atom %d %s q=%+.2f nbc=%d ghc=%d v=%.2f |F|=%.1f bonds:", i, EL[S.ty[i]].sym, S.q[i], S.nbc[i], S.ghc[i], v, std::sqrt(S.fx[i] * S.fx[i] + S.fy[i] * S.fy[i] + S.fz[i] * S.fz[i]));
            for (int k = 0; k < S.nbc[i]; k++) fprintf(gEvLog, " %s%d(o%d,r=%.3f,c=%.2f)", EL[S.ty[S.nb[i][k]]].sym, S.nb[i][k], S.bo[i][k], std::sqrt(dist2(i, S.nb[i][k])), S.bc[i][k]);
            fprintf(gEvLog, "\n");
            for (int j = 0; j < S.n; j++) { if (j == i) continue; double r2 = dist2(i, j); if (r2 > 0.8 * 0.8) continue;
                fprintf(gEvLog, "      nb %d %s q=%+.2f r=%.3f bonded=%d excl=%d ghost=%d Epair=%.2f\n", j, EL[S.ty[j]].sym, S.q[j], std::sqrt(r2), (int)bonded(i, j), (int)((mayExclude(i, j) && excluded(i, j))), (int)isGhost(i, j), pairEnergy(i, j, r2)); }
        };
        for (int s = 0; s < steps; s++) {
            runScript(); mdStep();
            computeForces(); measure(); { double E = EN.total() - Wext;
                if (haveStep && std::fabs(E - Eprev) > 0.3 && jumps < 30) { jumps++;
                    fprintf(gEvLog, "JUMP step %lld t=%.4f dt=%.6f ΔE=%.3f amax=%.1f\n", S.step, S.t, P.dt, E - Eprev, EN.amax); dumpAtom(EN.amaxI); }
                Eprev = E; haveStep = true; }
            if (s % 500 == 0) {
                measure(); double E = EN.total() - Wext;
                fprintf(gEvLog, "# t=%.2f T=%.3f dt=%.5f  E−W=%.4f (Δ за 500 шагов %.4f)  сумма ΔE событий %.3e  capped=%d\n", S.t, EN.T, P.dt, E, have ? E - lastE : 0.0, gEvSum, EN.capped);
                lastE = E; have = true; fflush(gEvLog);
            }
        }
        fclose(gEvLog); return 0;
    }
    // --ice σO εO T: устойчивость кубического льда при данных параметрах кислорода (подбор модели воды)
    if (cmd && wcsstr(cmd, L"--ice")) {
        wchar_t* p = wcsstr(cmd, L"--ice") + 5; double sO = wcstod(p, &p), eO = wcstod(p, &p), T = wcstod(p, &p);
        initBondTable(); initKlm(); EL[E_O].sig = sO; EL[E_O].eps = eO; rebuildTables(); loadPreset(2, L_ICE);
        P.thermostat = TH_BERENDSEN; P.Tset = T; P.tauT = 0.2;
        FILE* f = fopen(fmt("ice_%.2f_%.2f_%.2f.log", sO, eO, T).c_str(), "w"); if (!f) return 1;
        {   // диагностика исходной решётки: водородные связи (H···O ближе 2.2 Å) и силы
            int nhb = 0; double ehb = 0, emin = 0;
            for (int h = 0; h < S.n; h++) { int d = hbDonor(h); if (d < 0) continue;
                for (int p = nlStart[h]; p < nlStart[h + 1]; p++) { int a = nlIdx[p]; if (a == d || !hbAcceptor(S.ty[a]) || bonded(a, d)) continue;
                    const double r2 = dist2(h, a); if (r2 > (2.2 / 3.405) * (2.2 / 3.405)) continue;
                    double U = pairEnergy(h, a, r2); ehb += U; nhb++; emin = std::min(emin, U); } }
            double fO = 0, fH = 0, fOm = 0, fHm = 0; int nO = 0, nH = 0;
            for (int i = 0; i < S.n; i++) { double F = std::sqrt(S.fx[i] * S.fx[i] + S.fy[i] * S.fy[i] + S.fz[i] * S.fz[i]);
                if (S.ty[i] == E_O) { fO += F; fOm = std::max(fOm, F); nO++; } else { fH += F; fHm = std::max(fHm, F); nH++; } }
            // энергия пар по типам
            double eOO = 0, eOH = 0, eHH = 0;
            for (int i = 0; i < S.n; i++) for (int p = nlStart[i]; p < nlStart[i + 1]; p++) { int j = nlIdx[p]; if (j < i) continue; double e = pairEnergy(i, j, dist2(i, j));
                int k = (S.ty[i] == E_O) + (S.ty[j] == E_O); if (k == 2) eOO += e; else if (k == 1) eOH += e; else eHH += e; }
            fprintf(f, "init: molecules=%d HB(<-0.5)=%d EHB=%.2f minHB=%.2f  pair O-O=%.2f O-H=%.2f H-H=%.2f Ebond=%.2f  |F| O mean %.2f max %.2f, H mean %.2f max %.2f\n",
                    nO, nhb, ehb, emin, eOO, eOH, eHH, EN.ebond, fO / nO, fOm, fH / std::max(1, nH), fHm);
        }
        for (int s = 0; s <= 6000; s++) {
            mdStep();
            if (s % 1000 == 0) { analysisTick(); fprintf(f, "t=%5.1f T=%.3f ICE %d/%d coord=%.2f phase=%s\n", S.t, EN.T, A::stCount[ST_ICE], S.n, A::meanCoord, A::phase.c_str()); fflush(f); }
        }
        fclose(f); return 0;
    }
    // --geom K1,K2…: геометрия структур библиотеки — длины связей и углы у каждого центра сразу после построения и после
    // 2 пс динамики при 300 K в пустом ящике (проверка VSEPR и π-связей), отчёт geom.log
    if (cmd && wcsstr(cmd, L"--geom")) {
        initBondTable(); initKlm(); rebuildTables();
        FILE* f = fopen("geom.log", "w"); if (!f) return 1;
        std::vector<int> ks;
        for (const wchar_t* q = wcsstr(cmd, L"--geom") + 6; *q;) { while (*q == L' ' || *q == L',') q++; if (*q < L'0' || *q > L'9') break; ks.push_back((int)wcstol(q, (wchar_t**)&q, 10)); }
        auto report = [&](const char* when) {
            fprintf(f, "  %s:\n", when);
            for (int c = 0; c < S.n; c++) {
                if (S.nbc[c] == 0) continue;
                fprintf(f, "    %s%d (%d св., своб.вал %d):", EL[S.ty[c]].sym, c, S.nbc[c], freeVal(c));
                for (int a = 0; a < S.nbc[c]; a++) fprintf(f, " %s%d %s%.3fÅ", S.bo[c][a] == 2 ? "=" : S.bo[c][a] == 3 ? "≡" : "–", S.nb[c][a], EL[S.ty[S.nb[c][a]]].sym, std::sqrt(dist2(c, S.nb[c][a])) * 3.405);
                if (S.nbc[c] >= 2) {
                    fprintf(f, " | углы:");
                    for (int a = 0; a < S.nbc[c]; a++) for (int b = a + 1; b < S.nbc[c]; b++) {
                        double u[3], v[3]; dvec(c, S.nb[c][a], u[0], u[1], u[2]); dvec(c, S.nb[c][b], v[0], v[1], v[2]); fprintf(f, " %.0f", angleDeg(u, v)); }
                }
                fprintf(f, "\n");
            }
        };
        for (int k : ks) {
            if (k < 0 || k >= ML_N) continue;
            worldReset(12, 12, 12, B_PERIODIC); P.chemistry = false; P.Tset = 300 / cfg::U_T_K; P.thermostat = TH_BUSSI; P.tauT = 0.2;
            insertMolecule(k, 6, 6, 6);
            for (int i = 0; i < S.n; i++) thermalVel(S.ty[i], P.Tset, S.vx[i], S.vy[i], S.vz[i]);
            finishPreset();
            fprintf(f, "[%d] %s\n", k, MOL_LIB_NAMES[k]);
            report("шаблон");
            {   // силы против численного градиента энергии
                auto U = [&]() { computeForces(); return EN.enb + EN.ebond + EN.egrav + EN.efo; };
                U(); std::vector<double> Fa(3 * S.n);
                for (int i = 0; i < S.n; i++) { Fa[3 * i] = S.fx[i] + S.bx[i]; Fa[3 * i + 1] = S.fy[i] + S.by[i]; Fa[3 * i + 2] = S.fz[i] + S.bz[i]; }
                double worst = 0; int wi = -1;
                for (int i = 0; i < S.n; i++) for (int c = 0; c < 3; c++) {
                    double* X = c == 0 ? &S.x[i] : (c == 1 ? &S.y[i] : &S.z[i]); const double x0 = *X, h = 1e-6;
                    *X = x0 + h; const double Ep = U(); *X = x0 - h; const double Em = U(); *X = x0;
                    const double err = std::fabs(-(Ep - Em) / (2 * h) - Fa[3 * i + c]) / std::max(10.0, std::fabs(Fa[3 * i + c]));
                    if (err > worst) { worst = err; wi = i; }
                }
                U(); fprintf(f, "  градиент: худшая отн. ошибка %.2e (атом %d)\n", worst, wi);
            }
            double Tsum = 0; const int rb0 = physRollbacks; for (int s = 0; s < 1500; s++) { mdStep(); Tsum += EN.T; }
            report(fmt("после %.1f пс: средняя T %.0f K, откатов %d, шаг %.2f фс", toPs(S.t), toKelvin(Tsum / 1500), physRollbacks - rb0, P.dt * 1000 * cfg::U_T_PS).c_str());
            fflush(f);
        }
        fclose(f); return 0;
    }
    // --walltest: стенки из редактора сцены — зеркальные (сохранение энергии), тепловые (перепад T), поглощающая, шар → walltest.log
    if (cmd && wcsstr(cmd, L"--walltest")) {
        initBondTable(); initKlm(); initPalette(); rebuildTables();
        FILE* f = fopen("walltest.log", "w"); if (!f) return 1;
        auto gas = [&](int boundary, int n, double T) { worldReset(20, 20, 20, boundary); P.Tset = T; P.thermostat = TH_NVE; P.wallAttr = 0; fillBox(palette[findPal("Ar")], n, T, 1.0); };
        auto run = [&](int steps) { finishPreset(); for (int s = 0; s < steps; s++) mdStep(); measure(); };
        gas(B_WALLS, 600, kelvin(300)); for (int k = 0; k < 6; k++) P.wallType[k] = WT_MIRROR; run(20000);
        fprintf(f, "зеркальные стенки, NVE: дрейф энергии %.4f%%, атомов %d\n", driftPct(), S.n);
        gas(B_WALLS, 600, kelvin(300)); P.wallType[0] = WT_THERMAL; P.wallTK[0] = kelvin(600); P.wallType[1] = WT_THERMAL; P.wallTK[1] = kelvin(150); run(40000);
        { double tl = 0, tr = 0; int nl = 0, nr = 0; for (int i = 0; i < S.n; i++) { const double k2 = EL[S.ty[i]].m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]) / 3; if (S.x[i] < 5) { tl += k2; nl++; } else if (S.x[i] > 15) { tr += k2; nr++; } }
          fprintf(f, "тепловые стенки 600 K | 150 K: у горячей %.0f K, у холодной %.0f K, средняя %.0f K\n", toKelvin(tl / std::max(1, nl)), toKelvin(tr / std::max(1, nr)), toKelvin(EN.T)); }
        gas(B_WALLS, 600, kelvin(300)); P.wallType[2] = WT_ABSORB; P.gravity = 0.01; run(20000);
        fprintf(f, "поглощающее дно с тяжестью: осталось %d атомов из 600, поглощено %lld\n", S.n, absorbedCount);
        for (int wa : {0, 1}) {   // сосуд-шар с отталкивающей и с притягивающей стенкой
            gas(B_WALLS, 400, kelvin(300)); P.wallAttr = wa ? 0.6 : 0.0; setContainer(CT_SPHERE); measure(); const double T0 = EN.T; run(20000);
            int out = 0; for (int i = 0; i < S.n; i++) { double d, a, b, c; if (containerDist(S.x[i], S.y[i], S.z[i], d, a, b, c) && d < 0) out++; }
            fprintf(f, "сосуд-шар, притяжение стенки %.1f: атомов %d, вне шара %d, T %.0f → %.0f K, дрейф энергии %.4f%%\n", P.wallAttr, S.n, out, toKelvin(T0), toKelvin(EN.T), driftPct());
        }
        gas(B_PERIODIC, 600, kelvin(120)); setAxisPeriodic(1, false); P.wallType[2] = WT_STICKY; run(20000);
        { int low = 0; for (int i = 0; i < S.n; i++) if (S.y[i] < 1.5) low++; fprintf(f, "плёнка: x, z периодичны, липкое дно: у дна %d атомов из %d (дрейф %.4f%%)\n", low, S.n, driftPct()); }
        {   // вода: периодическая ось x становится стенками — молекулы на границе собираются целиком
            waterBox(14); fillWater(kelvin(300)); finishPreset(); for (int s = 0; s < 3000; s++) mdStep();   // уравновесить с термостатом
            measure(); const double T0 = EN.T; const int n0 = S.n;
            setAxisPeriodic(0, false); P.thermostat = TH_NVE; run(4000);
            double worst = 0; for (int i = 0; i < S.n; i++) for (int p = 0; p < S.nbc[i]; p++) worst = std::max(worst, std::sqrt(dist2(i, S.nb[i][p])));
            fprintf(f, "вода, ось x → стенки: убрано %d атомов из %d, T %.0f → %.0f K, дрейф %.4f%%, самая длинная связь %.2f Å\n", n0 - S.n, n0, toKelvin(T0), toKelvin(EN.T), driftPct(), worst * 3.405);
        }
        fclose(f); return 0;
    }
    // --lighttest: порог фотодиссоциации — смеси CH4 + Cl2 и H2 + O2 под вспышками разной длины волны → lighttest.log
    if (cmd && wcsstr(cmd, L"--lighttest")) {
        initBondTable(); initKlm(); initPalette(); rebuildTables();
        FILE* f = fopen("lighttest.log", "w"); if (!f) return 1;
        for (int scene : {31, 7}) for (double nm : {800.0, 650.0, 520.0, 450.0, 300.0, 200.0, 150.0, 122.0, 105.0}) {
            loadPreset(scene, 0);
            const double o[3] = {S.Lx / 2, S.Ly / 2, -5}, d[3] = {0, 0, 1};
            std::map<std::string, int> before; for (int i = 0; i < S.n; i++) for (int k = 0; k < S.nbc[i]; k++) if (S.nb[i][k] > i) before[std::string(EL[S.ty[i]].sym) + "–" + EL[S.ty[S.nb[i][k]]].sym]++;
            int hit = 0; double dMin = 0; const double E = photonEV(nm);
            const int c = lightFlash(o, d, 1e9, E * cfg::EV, &hit, &dMin);
            for (int s = 0; s < 400; s++) mdStep();   // осколки разлетаются, связи рвутся событиями
            std::map<std::string, int> after; for (int i = 0; i < S.n; i++) for (int k = 0; k < S.nbc[i]; k++) if (S.nb[i][k] > i) after[std::string(EL[S.ty[i]].sym) + "–" + EL[S.ty[S.nb[i][k]]].sym]++;
            fprintf(f, "λ = %4.0f нм, hν = %.2f эВ: получили квант %d связей из %d;", nm, E, c, hit);
            for (auto& kv : before) fprintf(f, "  %s %d→%d", kv.first.c_str(), kv.second, after[kv.first]);
            fprintf(f, "\n"); fflush(f);
        }
        fclose(f); return 0;
    }
    // --semi: вольт-амперные характеристики диода и МОП-транзистора (дрейфово-диффузионная модель) → semi.log
    if (cmd && wcsstr(cmd, L"--semi")) {
        FILE* f = fopen("semi.log", "w"); if (!f) return 1;
        const int steps = argInt(cmd, L"steps=", 3000);
        auto t0 = std::chrono::high_resolution_clock::now();
        for (double v : {-1.0, 0.0, 0.3, 0.5, 0.6, 0.65, 0.7, 0.75, 0.8}) {
            sc::V1 = v; scReset(0);
            for (int s = 0; s < steps; s++) scStep1();
            // Шокли для короткого диода: Is = q·A·nᵢ²·(Dn/(N_A·Wp) + Dp/(N_D·Wn)), Wp, Wn ≈ 1 мкм минус обеднённый слой
            const double Is = sc::Q * 1e-8 * 1e20 * (1400 * sc::VT / (1e16 * 0.8e-4) + 450 * sc::VT / (1e16 * 0.8e-4));
            fprintf(f, "диод V=%+.2f В: I(анод)=%.3e А  I(катод)=%.3e А  Шокли %.3e А\n", v, sc::Ic[0], sc::Ic[1], Is * (std::exp(v / sc::VT) - 1)); fflush(f);
        }
        for (double vg : {0.0, 0.5, 1.0, 2.0}) for (double vd : {0.1, 1.0, 2.0}) {
            sc::V1 = vg; sc::V2 = vd; scReset(2);
            for (int s = 0; s < steps; s++) scStep1();
            fprintf(f, "МОП Vg=%.1f Vd=%.1f: Id(сток)=%.3e А  исток %.3e  подложка %.3e\n", vg, vd, sc::Ic[1], sc::Ic[0], sc::Ic[2]); fflush(f);
        }
        for (double v : {1.4, 1.6, 1.8, 1.9, 2.0}) {
            sc::V1 = v; sc::mode121 = 0; sc::ledColor = 0; scReset(1);
            for (int s = 0; s < steps; s++) scStep1();
            fprintf(f, "светодиод V=%.2f: I=%.3e А, фотонов %.3e/с (на электрон %.2f)\n", v, sc::Ic[0], sc::photonsPerS, sc::Ic[0] > 0 ? sc::photonsPerS * sc::Q / sc::Ic[0] : 0.0); fflush(f);
        }
        for (double v : {0.55, 0.6, 0.65, 0.7}) {
            sc::V1 = v; sc::V2 = 2.0; scReset(3);
            for (int s = 0; s < steps; s++) scStep1();
            fprintf(f, "биполярный Vбэ=%.2f Vкэ=2: Iэ=%.3e Iб=%.3e Iк=%.3e β=%.1f\n", v, sc::Ic[0], sc::Ic[1], sc::Ic[2], sc::Ic[1] != 0 ? sc::Ic[2] / sc::Ic[1] : 0.0); fflush(f);
        }
        fprintf(f, "время %.1f с\n", std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count());
        fclose(f); return 0;
    }
    // --nuck: k-эффективный голого шара урана по радиусу и реактора по положению стержней → nuck.log
    if (cmd && wcsstr(cmd, L"--nuck")) {
        initBondTable(); FILE* f = fopen("nuck.log", "w"); if (!f) return 1;
        for (double e : {0.9, 0.2}) for (double R : {5.0, 7.0, 8.0, 8.7, 9.5, 10.5, 12.0, 14.0}) {
            nuc::scene = 2; nuc::enrich = e; nuc::sphereR = R; nucGeometry();
            fprintf(f, "шар U: обогащение %.0f%% R = %.1f см: k = %.3f\n", e * 100, R, nucKeff(3000, 8)); fflush(f);
        }
        for (double e : {0.05, 0.1, 0.2}) for (double rod : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            nuc::scene = 3; nuc::enrich = e; nuc::rodIns = rod; nuc::moderator = true; nucGeometry();
            fprintf(f, "реактор: обогащение %.0f%% стержни %.0f%%: k = %.3f\n", e * 100, rod * 100, nucKeff(2000, 8)); fflush(f);
        }
        nuc::scene = 3; nuc::enrich = 0.04; nuc::rodIns = 0; nuc::moderator = false; nucGeometry();
        fprintf(f, "реактор без воды: k = %.3f\n", nucKeff(2000, 8));
        fclose(f); return 0;
    }
    // --water T P N шагов: проверка модели воды — N молекул при T (K) и давлении P (атм; 0 — объём постоянный),
    // отчёт water_*.log: плотность, энергия взаимодействия на молекулу, коэффициент диффузии, g(r) O–O
    if (cmd && wcsstr(cmd, L"--water")) {
        wchar_t* p = wcsstr(cmd, L"--water") + 7; const double TK = wcstod(p, &p), Pa = wcstod(p, &p);
        const int N = (int)wcstol(p, &p, 10), steps = (int)wcstol(p, &p, 10);
        initBondTable(); initKlm();
        // подбор модели: a= (α DSF, Å⁻¹) kq= (κ зарядов) so= eo= (σ, Å и ε, K кислорода) r0= (O–H, Å) th= (угол H–O–H)
        dsfA = argDbl(cmd, L"a=", cfg::DSF_A / 3.405) * 3.405; kappaQ = argDbl(cmd, L"kq=", cfg::KAPPA_Q);
        EL[E_O].sig = argDbl(cmd, L"so=", EL[E_O].sig * 3.405) / 3.405; EL[E_O].eps = argDbl(cmd, L"eo=", EL[E_O].eps * cfg::U_T_K) / cfg::U_T_K;
        waterAngle = argDbl(cmd, L"th=", waterAngle); waterR0 = argDbl(cmd, L"r0=", waterR0);
        rebuildTables();
        const double L = std::cbrt(N * 29.915 / 39.476);
        worldReset(L, L, L, B_PERIODIC);
        const int placed = fillGrid(palette[findPal("H2O")], N, 0, 0, 0, L, L, L, TK / cfg::U_T_K);
        P.Tset = TK / cfg::U_T_K; P.thermostat = TH_BUSSI; P.tauT = 0.1; P.npt = Pa > 0; P.pExt = Pa / cfg::U_P_ATM; P.tauP = 2.0; P.chemistry = false;
        respaOn = argInt(cmd, L"respa=", 1) != 0; slowStepA = argDbl(cmd, L"al=", slowStepA);
        finishPreset();
        if (wcsstr(cmd, L" nve")) {   // сначала 2000 шагов с термостатом, затем без него — проверка сохранения энергии
            for (int s = 0; s < 2000; s++) mdStep();
            P.thermostat = TH_NVE; P.npt = false; resetEnergyRef();
        }
        std::string tag; if (const wchar_t* tp = wcsstr(cmd, L"tag=")) for (tp += 4; *tp && *tp != L' '; tp++) tag += (char)*tp;
        FILE* f = fopen(fmt("water_%.0f_%.0f_%d%s.log", TK, Pa, N, tag.c_str()).c_str(), "w"); if (!f) return 1;
        fprintf(f, "N=%d (поставлено %d) L=%.3f σ, начальная плотность %.3f г/см³\n", N, placed, L, massDensityNow());
        auto t0 = std::chrono::high_resolution_clock::now(); double rhoSum = 0, eSum = 0; int cnt = 0;
        for (int s = 1; s <= steps; s++) {
            mdStep();
            if (s % 100 == 0) analysisTick();
            if (s > steps / 2 && s % 50 == 0) { measure(); rhoSum += massDensityNow(); eSum += EN.enb / placed * cfg::U_KJMOL; cnt++; }
            if (s % 1000 == 0) {
                measure(); const double sec = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
                fprintf(f, "t=%.2f пс T=%.1f K P=%.0f атм ρ=%.4f г/см³ Eвз=%.2f кДж/моль D=%.3g м²/с dt=%.2f фс drift=%.4f%% (%.0f шагов/с)\n", toPs(S.t), toKelvin(EN.T), toAtm(EN.P),
                        massDensityNow(), EN.enb / placed * cfg::U_KJMOL, A::D * 1.161e-7, P.dt * cfg::U_T_PS * 1000, driftPct(), s / sec);
                fflush(f);
            }
        }
        if (cnt) fprintf(f, "среднее за вторую половину: ρ=%.4f г/см³, Eвз=%.2f кДж/моль (вода: 0.997 и −41.5)\n", rhoSum / cnt, eSum / cnt);
        std::vector<double> g(80, 0.0); int no = 0;   // g(r) O–O до 8 Å
        for (int i = 0; i < S.n; i++) if (S.ty[i] == E_O) { no++; for (int j = 0; j < S.n; j++) if (j != i && S.ty[j] == E_O) { const double r = std::sqrt(dist2(i, j)) * 3.405; if (r < 8) g[(int)(r * 10)] += 1; } }
        const double rhoO = no / boxVolume() / 39.476;
        fprintf(f, "g(r) O-O:");
        for (int k = 20; k < 80; k += 2) { const double r1 = k * 0.1, r2 = (k + 2) * 0.1, shell = 4.0 / 3 * PI * (r2 * r2 * r2 - r1 * r1 * r1); fprintf(f, " %.1f:%.2f", r1, (g[k] + g[k + 1]) / (no * rhoO * shell)); }
        fprintf(f, "\n"); fclose(f); return 0;
    }
    // --kin K шагов T вариант: длинный прогон одного пресета без окна (T ≤ 0 — как в пресете), отчёт в kin_*.log
    if (cmd && wcsstr(cmd, L"--kin")) {
        wchar_t* p = wcsstr(cmd, L"--kin") + 5;
        int K = (int)wcstol(p, &p, 10), steps = (int)wcstol(p, &p, 10); double T = wcstod(p, &p); int var = (int)wcstol(p, &p, 10);
        initBondTable(); initKlm(); rebuildTables(); loadPreset(K, var);
        if (T > 0) { P.Tset = T; measure(); if (EN.T > 1e-9) scaleVel(std::sqrt(T / EN.T)); resetEnergyRef(); }   // и начальные скорости — под эту T
        P.eaScale = argDbl(cmd, L"ea=", P.eaScale); sparkR = argDbl(cmd, L"sr=", sparkR); sparkTK = argDbl(cmd, L"st=", sparkTK);
        std::string tag; if (const wchar_t* tp = wcsstr(cmd, L"tag=")) for (tp += 4; *tp && *tp != L' '; tp++) tag += (char)*tp;
        FILE* f = fopen(fmt("kin_%d_%.2f_v%d%s.log", K, T, var, tag.c_str()).c_str(), "w"); if (!f) return 1;
        fprintf(f, "%s  N=%d\n", presetTitle.c_str(), S.n);
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int s = 0; s <= steps; s++) {
            runScript(); mdStep(); advanceFlashes(0.002f);
            const bool rep = s % std::max(1, steps / 12) == 0;
            if (s % 32 == 0 && !rep) analysisTick();   // как в окне: анализ каждые ~4 кадра (сглаженные профили, живые измерения)
            if (rep) {
                analysisTick(); computeMolecules();
                double sec = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
                fprintf(f, "t=%6.1f T=%.3f P=%.3f %s cryst=%.2f ICE %d FCC %d HCP %d BCC %d SC %d ions %d/%d coord=%.2f D=%.4f drift=%.4f%% a/e/d=%lld/%lld/%lld dt=%.4f (%.0f steps/s)\n   mol:", S.t, EN.T, EN.P, A::phase.c_str(), A::fCryst,
                        A::stCount[ST_ICE], A::stCount[ST_FCC], A::stCount[ST_HCP], A::stCount[ST_BCC], A::stCount[ST_SC], A::ionsFree, A::ionsTotal, A::meanCoord, A::D, driftPct(), CH.assoc, CH.exch, CH.diss, P.dt, s / std::max(1e-9, sec));
                std::vector<std::pair<int, std::string>> ms; for (auto& kv : A::mol) ms.push_back({-kv.second, kv.first}); std::sort(ms.begin(), ms.end());
                for (size_t q = 0; q < ms.size() && q < 14; q++) fprintf(f, " %s:%d", ms[q].second.c_str(), -ms[q].first);
                fprintf(f, "\n   rx:");
                std::vector<std::pair<long long, std::string>> rs; for (auto& kv : rxStats) rs.push_back({-kv.second.count, kv.first}); std::sort(rs.begin(), rs.end());
                for (size_t q = 0; q < rs.size() && q < 6; q++) fprintf(f, "  [%s ×%lld]", rs[q].second.c_str(), -rs[q].first);
                if (P.efield != 0) {   // электрофорез: средняя скорость дрейфа катионов и анионов вдоль поля
                    double vp = 0, vm = 0; int np = 0, nm2 = 0;
                    for (int i = 0; i < S.n; i++) { if (S.ty[i] == E_NA) { vp += S.vx[i]; np++; } else if (S.ty[i] == E_CLM) { vm += S.vx[i]; nm2++; } }
                    fprintf(f, "\n   E=%.2f  <vx> Na+ %+.4f  Cl- %+.4f", P.efield, np ? vp / np : 0.0, nm2 ? vm / nm2 : 0.0);
                }
                if (!A::sceneNote.empty()) fprintf(f, "\n   note: %s", A::sceneNote.c_str());
                fprintf(f, "\n");
                fflush(f);
            }
        }
        // перенос протона: сколько попыток дошло до каждого фильтра и ΔU по классам донор×основание (эВ)
        if (ptGate[0]) {
            fprintf(f, "PT: кандидатов %lld, по расстоянию %lld, по сближению %lld, по барьеру %lld\n", ptGate[0], ptGate[1], ptGate[2], ptGate[3]);
            for (int c = 0; c < 12; c++) if (ptDbgN[c]) fprintf(f, "   донор %d × основание %d: попыток %lld, принято %lld, ΔU средн. %.2f мин. %.2f эВ\n", c / 3, c % 3, ptDbgN[c], ptDbgOk[c], rxEV(ptDbgSum[c] / ptDbgN[c]), rxEV(ptDbgMin[c]));
        }
        fclose(f); return 0;
    }
    // снимки и автотест работают с настройками по умолчанию и не пишут atoms.ini
    const bool automation = cmd && (wcsstr(cmd, L"--shot") || wcsstr(cmd, L"--uitest"));
    if (automation) settingsNoSave = true; else settingsLoad();
    ompAllThreads = std::max(1, omp_get_num_procs()); applyThreads();
    // язык окна: --lang (разобран в lang.inl) → atoms.ini → язык Windows (режимы без окна выше — русский, если нет --lang)
    if (langCmd < 0) langSet(opt.lang >= 0 ? opt.lang : langSystemDefault());
    SetProcessDPIAware();
    // --monocheck: записывать цвета с оттенком вне белого списка атомов в mono_violations.log; grayatoms — атомы тоже серые (отладка)
    monoCheck = cmd && wcsstr(cmd, L"--monocheck"); monoGrayAtoms = cmd && wcsstr(cmd, L"grayatoms");
    if (monoCheck) if (FILE* fp = fopen("mono_violations.log", "w")) fclose(fp);
    initBondTable(); initKlm(); initPalette();
    WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc); wc.style = CS_OWNDC | CS_DBLCLKS; wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = L"AtomsSim";
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    RegisterClassExW(&wc);
    RECT wr = {0, 0, 1600, 960};
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    if (sw > 0 && sh > 0) { wr.right = std::min(1760, sw - 60); wr.bottom = std::min(1000, sh - 100); }
    // --shot … size=WxH: размер клиентской области (проверка раскладки 1280×720, 1920×1080 …)
    { int sx = argInt(cmd, L"size=", 0); const wchar_t* xp = cmd ? wcsstr(cmd, L"size=") : nullptr; if (sx > 0 && xp && wcschr(xp, L'x')) { int sy = (int)wcstol(wcschr(xp, L'x') + 1, nullptr, 10); if (sy > 0) { wr.right = sx; wr.bottom = sy; } } }
    uiScaleOverride = (float)argDbl(cmd, L"scale=", 0);
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    int wx = CW_USEDEFAULT, wy = CW_USEDEFAULT, ww = wr.right - wr.left, wh = wr.bottom - wr.top;
    bool maximize = false;
    if (!automation && opt.keepWindow && opt.ww >= 400 && opt.wh >= 300) {   // окно там же, где было при выходе (если этот монитор ещё есть)
        RECT r = {opt.wx, opt.wy, opt.wx + opt.ww, opt.wy + opt.wh};
        if (MonitorFromRect(&r, MONITOR_DEFAULTTONULL)) { wx = opt.wx; wy = opt.wy; ww = opt.ww; wh = opt.wh; maximize = opt.wmax; }
    }
    hwnd = CreateWindowW(L"AtomsSim", TW(WINDOW_TITLE).c_str(), WS_OVERLAPPEDWINDOW | WS_VISIBLE, wx, wy, ww, wh, nullptr, nullptr, hInst, nullptr);
    if (cmd && wcsstr(cmd, L"size=")) SetWindowPos(hwnd, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top, SWP_NOZORDER);
    if (maximize) ShowWindow(hwnd, SW_MAXIMIZE);
    hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {}; pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER; pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cAlphaBits = 8;
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    hglrc = wglCreateContext(hdc); wglMakeCurrent(hdc, hglrc);
    wglSwapInterval = (PFNSWAP)wglGetProcAddress("wglSwapIntervalEXT");
    applyVsync();
    RECT cr; GetClientRect(hwnd, &cr); winW = cr.right; winH = cr.bottom;
    uiScale = calcUiScale();
    buildFonts(); buildTextures();
    layout(); sceneAspect = clampv(sceneH / sceneW, 0.4f, 1.2f);
    // --shot K N [vV] [rot] [help] [table] [menu] [settings] [hoverZ] [tab=T] [tool=T] [fo=K] [demo] [layers] [nopanel] [liball] [cut]
    //        [color=C] [zoom=K] [scrollto=ID] [lib=K1,K2…] [rec=ПАПКА every=K from=F] [png] [out=имя] [size=WxH] [scale=S]
    //        [faces=ABCDEF] [cont=C] [per=M]:
    //   пресет K, N кадров, сохранить снимок окна и выйти (проверка графики и раскладки, картинки для README);
    //   faces — вид каждой грани цифрой (−x +x −y +y −z +z), cont — сосуд, per — маска периодичных осей
    int shotPreset = -1, shotFrames = 400, shotVar = 0; bool shotPng = false, shotDemo = false; std::wstring shotOut;
    if (cmd && wcsstr(cmd, L"--shot")) {
        const wchar_t* p = wcsstr(cmd, L"--shot") + 6; shotPreset = (int)wcstol(p, (wchar_t**)&p, 10); int fr = (int)wcstol(p, nullptr, 10); if (fr > 0) shotFrames = fr;
        const wchar_t* v = wcsstr(cmd, L" v"); if (v) shotVar = (int)wcstol(v + 2, nullptr, 10);
        if (wcsstr(cmd, L" rot")) cam3.autoRot = true;
        if (wcsstr(cmd, L" help")) helpOn = true;
        if (wcsstr(cmd, L" table")) ptOn = true;      // таблица Менделеева
        if (wcsstr(cmd, L" menu")) menuOn = true;     // меню сцен
        if (wcsstr(cmd, L" settings")) settingsOn = true;
        if (wcsstr(cmd, L" atomview")) { atomViewOn = true; avZ = argInt(cmd, L"avz=", 6); avMode = clampv(argInt(cmd, L"avmode=", 0), 0, 2); avN = argInt(cmd, L"avn=", 2); avL = argInt(cmd, L"avl=", 1); avM = argInt(cmd, L"avm=", 0); }
        const wchar_t* hz = wcsstr(cmd, L" hover"); if (hz) shotHoverZ = (int)wcstol(hz + 6, nullptr, 10);   // навести курсор на элемент Z
        sideTab = clampv(argInt(cmd, L"tab=", 0), 0, TAB_N - 1); lmbTool = clampv(argInt(cmd, L"tool=", TOOL_ADD), 0, TOOL_N - 1); foKind = clampv(argInt(cmd, L"fo=", 0), 0, FO_N - 1);
        shotPng = wcsstr(cmd, L" png") != nullptr; shotDemo = wcsstr(cmd, L" demo") != nullptr;
        if (wcsstr(cmd, L" layers")) { layerVel = true; layerGrid = true; layerCharges = true; }
        if (wcsstr(cmd, L" nopanel")) graphsOn = false;
        if (wcsstr(cmd, L" liball")) chemLibAll = true;   // вся библиотека во вкладке «Химия»
        if (wcsstr(cmd, L" cut")) sliceOn = true;
        const wchar_t* op = wcsstr(cmd, L"out="); if (op) { op += 4; while (*op && *op != L' ') shotOut += *op++; }
        const wchar_t* rp = wcsstr(cmd, L"rec="); if (rp) { rp += 4; while (*rp && *rp != L' ') clipDir += *rp++; CreateDirectoryW(clipDir.c_str(), nullptr); }
        clipEvery = std::max(1, argInt(cmd, L"every=", 2)); clipFrom = argInt(cmd, L"from=", 0);
        fitZoom = clampv(argDbl(cmd, L"zoom=", 1.0), 0.05, 3.0);
    }
    loadPreset(shotPreset >= 0 ? shotPreset : 1, shotVar);
    if (shotDemo) setupDemo();
    if (cmd && shotPreset >= 0 && shotPreset < 100) {   // редактор сцены
        if (wcsstr(cmd, L"per=") && P.boundary != B_PISTON) { const int m = argInt(cmd, L"per=", 7); for (int k = 0; k < 3; k++) setAxisPeriodic(k, (m >> k) & 1); }
        if (const wchar_t* fp = wcsstr(cmd, L"faces=")) for (int k = 0; k < 6 && fp[6 + k] >= L'0' && fp[6 + k] <= L'9'; k++) P.wallType[k] = clampv(fp[6 + k] - L'0', 0, WT_N - 1);
        if (wcsstr(cmd, L"cont=") && P.boundary != B_PISTON) setContainer(argInt(cmd, L"cont=", 0));
        undoStack.clear();
    }
    if (const wchar_t* lp = cmd ? wcsstr(cmd, L"lib=") : nullptr) {   // галерея структур библиотеки: lib=K1,K2,…
        std::vector<int> ks;
        for (const wchar_t* q = lp + 4; *q >= L'0' && *q <= L'9';) { ks.push_back((int)wcstol(q, (wchar_t**)&q, 10)); if (*q == L',') q++; }
        const int n = (int)ks.size(), cols = n == 1 ? 1 : std::max(1, (int)std::ceil(std::sqrt(n * 1.7))), rows = (n + cols - 1) / std::max(1, cols);
        const double cell = 8;
        worldReset(cols * cell, rows * cell, cell, B_PERIODIC);
        P.thermostat = TH_BERENDSEN; P.Tset = 0.02; P.chemistry = false; opt.box = false; colorMode = 0;
        for (int q = 0; q < n; q++) if (ks[q] >= 0 && ks[q] < ML_N) insertMolecule(ks[q], (q % cols + 0.5) * cell, (rows - 1 - q / cols + 0.5) * cell, cell / 2);
        presetTitle = "Библиотека молекул и структур"; presetLoaded = false;
        cam3.yaw = camGoal.yaw = 0.18; cam3.pitch = camGoal.pitch = 0.16;
        fitView(true); cam3.dist *= 0.62; camGoal = cam3; viewFitPending = false;   // сетка структур занимает не весь шар обзора
    }
    if (cmd && wcsstr(cmd, L"color=")) colorMode = clampv(argInt(cmd, L"color=", 0), 0, COLOR_N - 1);
    const int scrollTo = argInt(cmd, L"scrollto=", 0);
    if (cmd && wcsstr(cmd, L"--fullscreen")) toggleFullscreen();
    if (cmd && wcsstr(cmd, L"--uitest")) {
        uiTestMode = true; uiLog = fopen("uitest.log", "w"); buildUiTest();
        if (wglSwapInterval) wglSwapInterval(0);   // без вертикальной синхронизации — быстрее
        fprintf(uiLog, "шагов сценария: %zu\n", uiSteps.size());
    }
    const std::wstring sessionPath = exeDir() + L"\\session.atoms";
    bool restored = false;
    if (!automation) {
        if (opt.start == START_LAST_SCENE && opt.lastScene != 1) {
            bool known = opt.lastScene >= 0 && opt.lastScene <= 9;
            for (const SceneInfo& s : SCENES) known = known || s.key == opt.lastScene;
            if (known) loadPreset(opt.lastScene, opt.lastVar % std::max(1, presetVariants(opt.lastScene)));
        }
        if (opt.start == START_SESSION && GetFileAttributesW(sessionPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            restored = loadState(sessionPath); undoStack.clear();
            if (restored) viewFitPending = false;   // камера — как была при выходе
        }
        if (opt.startFull && !fullscreen) toggleFullscreen();
    }
    if (shotPreset < 0) showToast(restored ? "Прошлый сеанс восстановлен (F8 — настройки запуска)" : "H — справка · Tab — сцены · F8 — настройки · Alt+1…0 — инструменты");
    auto last = std::chrono::high_resolution_clock::now(); double fpsAcc = 0; int fpsCnt = 0; int frame = 0;
    MSG msg; bool running = true;
    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { if (msg.message == WM_QUIT) running = false; TranslateMessage(&msg); DispatchMessageW(&msg); }
        if (!running) break;
        auto now = std::chrono::high_resolution_clock::now();
        double frameDt = std::min(0.1, std::chrono::duration<double>(now - last).count()); last = now;
        fpsAcc += frameDt; fpsCnt++; if (fpsAcc > 0.5) { fps = fpsCnt / fpsAcc; fpsAcc = 0; fpsCnt = 0; }
        if (uiTestMode && !uiTestTick()) { running = false; break; }
        if (scrollTo > 0) uiScrollTo(scrollTo);
        if (shotPreset >= 0 && !shotHoverZ) { mouseX = -1000; mouseY = -1000; }   // снимок без кисти и подсказок под курсором
        if (shotHoverZ > 0 && ptOn) { auto it = uiRects.find(400 + shotHoverZ); if (it != uiRects.end()) { mouseX = (int)(it->second.x + it->second.w / 2); mouseY = (int)(it->second.y + it->second.h / 2); } }
        ui.mx = mouseX; ui.my = mouseY;
        layout(); camSetup();
        const bool overlay = anyOverlay();
        ui.wheel = (inScene(mouseX, mouseY) && !overlay) ? 0 : in.wheel;   // колесо вне сцены — панелям
        if (!inScene(mouseX, mouseY) || overlay) in.wheel = 0;
        handleKeys();
        handleMouse(frameDt);
        const bool md = world == W_MD;
        if (md) boxTick();
        const bool minimized = !uiTestMode && IsIconic(hwnd);
        const bool idle = !uiTestMode && opt.bgPause && (minimized || GetForegroundWindow() != hwnd);
        if (!P.paused && !idle) {
            if (!md) worldStep(automation ? 1.0 / 60 : frameDt);   // снимки и автотест — ровный шаг, как 60 к/с
            else if (automation) { runScript(); for (int s = 0; s < P.substeps; s++) mdStep(); }
            else {
                // «скорость» — время модели за кадр (substeps обычных шагов): если шаг dt укоротился (раскалённые лёгкие
                // атомы в пламени), шагов за кадр больше — пока расчёт кадра укладывается в ~14 мс
                runScript();
                const double tGoal = S.t + P.substeps * P.dtBase - 1e-9; const auto c0 = std::chrono::high_resolution_clock::now();
                for (int s = 1; s <= 400; s++) {
                    mdStep();
                    if (S.t >= tGoal) break;
                    if (s >= P.substeps && std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - c0).count() > 0.014) break;
                }
            }
        }
        if (minimized || idle) Sleep(15);   // свёрнутое или фоновое окно не грузит процессор отрисовкой
        { static bool wasOpen = false; if (wasOpen && !settingsOn) settingsSave(); wasOpen = settingsOn; }
        frame++;
        if (md && ((!P.paused && frame % 4 == 0) || (P.paused && frame % 20 == 0))) analysisTick();
        if (md && trailsOn && !P.paused && frame % 2 == 0) trailsRecord();
        advanceFlashes((float)frameDt);
        toastTime -= frameDt;
        renderFrame(frameDt);
        ui.pressed = false; ui.released = false; ui.wheel = 0;
        if (shotPreset >= 0 && frame == shotFrames) {
            if (shotPng || !shotOut.empty()) {
                std::wstring nm = !shotOut.empty() ? shotOut : L"shot_" + std::to_wstring(shotPreset) + L".png";
                writePNG(nm, winW, winH, readRGB(0, 0, winW, winH));
            } else savePPM(fmt("shot_%d.ppm", shotPreset).c_str());
            running = false;
        }
        SwapBuffers(hdc);
    }
    if (uiLog) { fprintf(uiLog, "\nИТОГ: шагов %zu из %zu, проблем %d\n", uiStepIdx, uiSteps.size(), uiProblems); fclose(uiLog); }
    if (!automation) {   // положение окна, последняя сцена и (по выбору) весь сеанс — к следующему запуску
        WINDOWPLACEMENT wp = wpPrev;
        if (fullscreen || GetWindowPlacement(hwnd, &wp)) {
            const RECT& r = wp.rcNormalPosition;
            if (r.right - r.left >= 400 && r.bottom - r.top >= 300) { opt.wx = r.left; opt.wy = r.top; opt.ww = r.right - r.left; opt.wh = r.bottom - r.top; }
            opt.wmax = wp.showCmd == SW_SHOWMAXIMIZED;
        }
        opt.lastScene = currentPreset; opt.lastVar = presetVariant;
        if (opt.start == START_SESSION && world == W_MD) { toastsOff = true; saveState(sessionPath, true); }
        settingsSave();
    }
    wglMakeCurrent(nullptr, nullptr); wglDeleteContext(hglrc); ReleaseDC(hwnd, hdc);
    return uiProblems ? 2 : 0;
}

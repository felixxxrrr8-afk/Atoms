// ===================================== ОКНО НАСТРОЕК (F8) ====================================
// Две колонки разделов; изменения действуют сразу, в atoms.ini записываются при закрытии окна и при выходе.
// id элементов: 1600–1699, прокручиваемая область № 5.
typedef BOOL(WINAPI* PFNSWAP)(int);
static PFNSWAP wglSwapInterval = nullptr;
static int ompAllThreads = 1;    // число логических ядер (при запуске)
static float autoUiScale();      // app.inl: масштаб по размеру окна

static void applyVsync() { if (wglSwapInterval && !uiTestMode) wglSwapInterval(opt.vsync ? 1 : 0); }
static void applyThreads() { omp_set_num_threads(opt.threads > 0 ? std::min(opt.threads, ompAllThreads) : ompAllThreads); }
static void applySettings() { applyVsync(); applyThreads(); }
static void resetSettings() {
    Settings d = OPT_DEFAULT;   // язык, последняя сцена и положение окна не сбрасываются
    d.lang = opt.lang; d.lastScene = opt.lastScene; d.lastVar = opt.lastVar;
    d.wx = opt.wx; d.wy = opt.wy; d.ww = opt.ww; d.wh = opt.wh; d.wmax = opt.wmax;
    opt = d; applySettings();
    showToast("Настройки по умолчанию");
}
template <class V> static V nextIn(const std::vector<V>& list, V cur) {   // следующее значение списка по кругу
    for (size_t k = 0; k < list.size(); k++) if (list[k] == cur) return list[(k + 1) % list.size()];
    return list[0];
}
static std::vector<int> threadChoices() {
    std::vector<int> v = {0, 1};
    for (int t = 2; t < ompAllThreads; t *= 2) v.push_back(t);
    if (ompAllThreads > 1) v.push_back(ompAllThreads);
    return v;
}
static void openOutputDir() {
    std::wstring d = outputDir(LANG == LANG_EN);
    if (!uiTestMode) ShellExecuteW(hwnd, L"open", d.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    showToast(std::string(T("Папка: ")) + narrow(d));
}

static PR setRect;
static float setContentH = 0;   // высота содержимого прошлого кадра (окно подстраивается под неё)
static void drawSettings() {
    const float pad = uiPx(18), gap = uiPx(22), bh = uiPx(24), sh = uiPx(30), g = uiPx(5), headH = uiPx(50), footH = uiPx(46);
    float W = std::min((float)winW - uiPx(24), uiPx(820));
    float colW = std::floor((W - 2 * pad - gap) / 2);
    const bool two = colW >= uiPx(290);
    if (!two) colW = W - 2 * pad;
    const float want = setContentH > 0 ? setContentH : uiPx(560);
    float H = std::min((float)winH - uiPx(16), headH + want + footH);
    float x0 = std::floor((winW - W) / 2), y0 = std::floor(std::max(uiPx(8), (winH - H) / 2));
    setRect = {x0, y0, W, H};
    rectFill(0, 0, (float)winW, (float)winH, {0, 0, 0, 0.6f});
    boxPanel(x0, y0, W, H, C_PANEL, C_LINE_H); rectFill(x0, y0, W, uiPx(2), C_ACC);
    drawText(fontL, x0 + pad, y0 + uiPx(12), "Настройки", C_TEXT_HI);
    drawTextR(fontXS, x0 + W - pad, y0 + uiPx(17), "изменения действуют сразу · Esc / F8 — закрыть", C_DIM);

    const float top = scrollBegin(5, x0 + pad, y0 + headH, W - 2 * pad, H - headH - footH);
    float cx = x0 + pad, yy = top, colTop = top, bottom = top;
    auto section = [&](const char* title) { uiSection(cx, yy, colW, title); };
    auto cyc = [&](int id, const char* label, const std::string& value, const char* hint) {
        bool c = uiCycle(id, cx, yy, colW, bh, label, value, false, hint); yy += bh + g; return c; };
    auto chk = [&](int id, const char* label, bool* v, const char* hint) {
        bool c = uiCheck(id, cx, yy, colW, uiPx(22), label, v, hint); yy += uiPx(22) + g - uiPx(2); return c; };
    auto sld = [&](int id, const char* label, double* v, double lo, double hi, const std::string& val, const char* hint) {
        bool c = uiSlider(id, cx, yy, colW, sh - uiPx(4), label, v, lo, hi, false, val, hint); yy += sh; return c; };
    auto nextColumn = [&]() {
        bottom = std::max(bottom, yy);
        if (two) { cx += colW + gap; yy = colTop; }
        else yy += uiPx(6);
    };

    // ---- левая колонка
    section("Интерфейс");
    if (cyc(1600, "Язык", LANG == LANG_EN ? "EN — English" : "RU — русский", "Язык интерфейса (Ctrl+L)"))
        setLanguage(LANG == LANG_EN ? LANG_RU : LANG_EN);
    {
        const std::string v = opt.uiScale > 0 ? fmt("%d%%", opt.uiScale) : fmt("авто · %d%%", (int)std::lround(autoUiScale() * 100));
        if (cyc(1601, "Масштаб интерфейса", v, "Размер кнопок и шрифтов. «Авто» — по размеру окна"))
            opt.uiScale = nextIn<int>({0, 100, 125, 150, 175, 200, 250, 300}, opt.uiScale);
    }
    chk(1602, "Подсказки к кнопкам", &opt.hints, "Всплывающие подсказки при наведении на кнопки и слайдеры");
    chk(1603, "Карточка атома под курсором", &opt.atomCard, "Элемент, координаты, скорость и связи атома под курсором");
    chk(1604, "Время модели и к/с вверху", &opt.clock, "Время модели, номер шага и частота кадров в правой части верхней панели");
    sld(1605, "Уведомления", &opt.toastSec, 1, 8, fmt("%.1f с", opt.toastSec), "Сколько секунд видно уведомление внизу сцены");
    section("Графика");
    chk(1610, "Вертикальная синхронизация", &opt.vsync, "Кадры в такт с монитором: ровная картинка без разрывов.\nВыключите, чтобы расчёт шёл быстрее на слабом мониторе");
    {
        static const char* BGN[3] = {"чёрный", "графит", "тёмно-серый"};
        if (cyc(1611, "Фон сцены", T(BGN[clampv(opt.sceneBg, 0, 2)]), "Цвет фона за атомами")) opt.sceneBg = (opt.sceneBg + 1) % 3;
    }
    sld(1612, "Затемнение дальних атомов", &opt.fog, 0, 1, fmt("%.0f%%", opt.fog * 100), "Глубина: дальние атомы темнее ближних. 0 — все одинаково яркие");
    chk(1613, "Блики на атомах", &opt.gloss, "Световой блик на шарах — объём без затрат на освещение");
    chk(1614, "Контур ящика", &opt.box, "Рёбра расчётного ящика; периодические границы — пунктиром");
    sld(1615, "Толщина связей", &opt.bondW, 0.4, 2.5, fmt("%.2f×", opt.bondW), "Толщина палочек химических связей");

    // ---- правая колонка
    nextColumn();
    section("Камера и мышь");
    sld(1620, "Чувствительность вращения", &opt.mouseSens, 0.25, 3, fmt("%.2f×", opt.mouseSens), "Скорость поворота камеры мышью");
    chk(1621, "Инвертировать вертикаль", &opt.invertY, "Мышь вверх — камера смотрит сверху (как в авиасимуляторе)");
    sld(1622, "Шаг колеса", &opt.zoomSens, 0.25, 3, fmt("%.2f×", opt.zoomSens), "Насколько приближает один щелчок колеса");
    sld(1623, "Плавность камеры", &opt.camLag, 0, 0.4, opt.camLag > 0.005 ? fmt("%.2f с", opt.camLag) : std::string(T("выкл")),
        "Время, за которое камера догоняет мышь. 0 — без сглаживания");
    sld(1624, "Скорость автовращения", &opt.spin, 0.05, 1.5, fmt("%.0f °/с", opt.spin * 180 / PI), "Автовращение включается клавишей O");
    section("Расчёт");
    {
        const std::string v = opt.threads > 0 ? fmt("%d из %d", std::min(opt.threads, ompAllThreads), ompAllThreads) : fmt("все (%d)", ompAllThreads);
        if (cyc(1630, "Потоки расчёта", v, "Сколько ядер процессора считает силы.\nМеньше потоков — компьютер свободнее для других программ")) {
            opt.threads = nextIn(threadChoices(), opt.threads); applyThreads(); }
    }
    if (cyc(1631, "Глубина отмены", fmt("%d действий", opt.undo), "Сколько последних действий можно отменить (Ctrl+Z).\nКаждый шаг хранит копию всей сцены"))
        opt.undo = nextIn<int>({5, 10, 20, 40, 80}, opt.undo);
    chk(1632, "Пауза, когда окно неактивно", &opt.bgPause, "Расчёт останавливается, пока вы работаете в другой программе");
    {
        static const char* SN[3] = {"«Идеальный газ»", "последняя сцена", "прошлый сеанс"};
        if (cyc(1633, "При запуске", T(SN[clampv(opt.start, 0, 2)]), "Что открывать при запуске программы.\n«Прошлый сеанс» — состояние на момент выхода (session.atoms)"))
            opt.start = (opt.start + 1) % 3;
    }
    section("Файлы и окно");
    if (cyc(1640, "Снимки, кадры и CSV", opt.shotsTo == SHOTS_PICTURES ? "Изображения\\Атомы" : "рядом с atoms.exe",
            "Куда сохранять снимки (F12), запись кадров (Ctrl+F12) и экспорт графиков (Ctrl+E)"))
        opt.shotsTo = 1 - opt.shotsTo;
    {
        static const char* RN[4] = {"каждый кадр", "каждый 2-й", "каждый 3-й", "каждый 4-й"};
        if (cyc(1641, "Запись кадров", T(RN[clampv(opt.recStep, 1, 4) - 1]), "Какие кадры сохранять при записи (Ctrl+F12).\nРеже — меньше файлов, движение быстрее в готовом ролике"))
            opt.recStep = opt.recStep % 4 + 1;
    }
    chk(1642, "Запоминать размер и положение окна", &opt.keepWindow, "При следующем запуске окно откроется там же и того же размера");
    chk(1643, "Запускать на весь экран", &opt.startFull, "Полный экран сразу после запуска (F11 — выйти)");
    bottom = std::max(bottom, yy);
    scrollEnd(5, bottom + uiPx(6));
    setContentH = bottom + uiPx(6) - top;

    // ---- низ окна
    const float fy = y0 + H - footH + uiPx(10);
    lineH(x0 + pad, x0 + W - pad, fy - uiPx(6), C_LINE);
    drawText(fontXS, x0 + pad, fy + (bh - fontXS.h) / 2, "Хранится в atoms.ini рядом с atoms.exe", C_DIM);
    float bx = x0 + W - pad;
    auto btn = [&](int id, const char* label, bool accent, const char* hint) {
        float w = textW(fontU, label) + uiPx(24); bx -= w;
        bool c = uiButton(id, bx, fy, w, bh, label, false, false, hint, nullptr, accent); bx -= uiPx(6); return c; };
    if (btn(1691, "Готово", true, "Закрыть настройки (Esc, F8)")) settingsOn = false;
    if (btn(1690, "По умолчанию", false, "Вернуть все настройки, кроме языка и положения окна")) resetSettings();
    if (btn(1692, "Открыть папку снимков", false, "Папка, куда сохраняются снимки, кадры и CSV")) openOutputDir();

    if (ui.pressed && !inPR(setRect)) settingsOn = false;   // щелчок мимо окна — закрыть
}

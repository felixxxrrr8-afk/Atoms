// ===================================== RENDER: базовое ==================================
static HWND hwnd; static HDC hdc; static HGLRC hglrc;
static int winW = 1600, winH = 960;
static GLuint texGlow = 0, texCore = 0, texFont = 0;
struct RGBA { float r, g, b, a; };
static constexpr RGBA hexc(unsigned h, float a = 1) { return {((h >> 16) & 255) / 255.0f, ((h >> 8) & 255) / 255.0f, (h & 255) / 255.0f, a}; }
static constexpr RGBA grayc(float v, float a = 1) { return {v, v, v, a}; }   // оттенок серого: 0 — чёрный, 1 — белый

// ---- Интерфейс серый, цветные только атомы и всё, что поясняет их цвет (легенда, образцы в палитре, следы…) —
//  такие места рисуются внутри MonoAtoms. Любой другой цветной glColor* приводится к яркости (Rec.601), а с ключом
//  --monocheck ещё и записывается в mono_violations.log, чтобы найти, откуда он взялся. grayatoms — серые и атомы.
static bool monoCheck = false;             // --monocheck
static bool monoGrayAtoms = false;         // grayatoms: атомы и их легенда тоже серые (отладка)
static int monoAtomScope = 0;              // > 0 — рисуется цвет атома (белый список)
struct MonoAtoms { MonoAtoms() { monoAtomScope++; } ~MonoAtoms() { monoAtomScope--; } };
static const char* monoCtx = "";           // раздел кадра (renderFrame): подсказка, где искать нарушение
static void monoReport(const char* where, float r, float g, float b) {
    static std::map<std::string, int> seen;
    const char* f = where; for (const char* p = where; *p; p++) if (*p == '\\' || *p == '/') f = p + 1;
    char key[256]; snprintf(key, sizeof(key), "%s | %s | rgb %.3f %.3f %.3f", monoCtx, f, r, g, b);
    if (seen[key]++) return;
    if (FILE* fp = fopen("mono_violations.log", "a")) { fprintf(fp, "%s\n", key); fclose(fp); }
}
static inline void monoFix(float& r, float& g, float& b, const char* where) {
    if (std::fabs(r - g) <= 1e-4f && std::fabs(g - b) <= 1e-4f) return;
    if (monoAtomScope > 0) { if (monoGrayAtoms) r = g = b = 0.299f * r + 0.587f * g + 0.114f * b; return; }   // цвет атома — разрешён
    if (monoCheck) monoReport(where, r, g, b);
    r = g = b = 0.299f * r + 0.587f * g + 0.114f * b;
}
static inline void monoColor4f(float r, float g, float b, float a, const char* where) { monoFix(r, g, b, where); glColor4f(r, g, b, a); }
static inline void monoColor3f(float r, float g, float b, const char* where) { monoFix(r, g, b, where); glColor3f(r, g, b); }
static inline void monoClearColor(float r, float g, float b, float a, const char* where) { monoFix(r, g, b, where); glClearColor(r, g, b, a); }
#define MONO_STR2(x) #x
#define MONO_STR(x) MONO_STR2(x)
#define MONO_AT __FILE__ ":" MONO_STR(__LINE__)
#define glColor4f(r, g, b, a) monoColor4f((float)(r), (float)(g), (float)(b), (float)(a), MONO_AT)
#define glColor3f(r, g, b) monoColor3f((float)(r), (float)(g), (float)(b), MONO_AT)
#define glClearColor(r, g, b, a) monoClearColor((float)(r), (float)(g), (float)(b), (float)(a), MONO_AT)
// прочие способы задать цвет запрещены (ошибка компиляции): только glColor4f / glColor3f / col()
#define glColor3ub(...) MONO_only_glColor4f_is_allowed
#define glColor4ub(...) MONO_only_glColor4f_is_allowed
#define glColor3fv(...) MONO_only_glColor4f_is_allowed
#define glColor4fv(...) MONO_only_glColor4f_is_allowed
#define glColor3d(...) MONO_only_glColor4f_is_allowed
#define glColor4d(...) MONO_only_glColor4f_is_allowed

// ---- Палитра интерфейса: все цвета панелей берутся отсюда. Сцена чёрная, панели графитовые, акцент белый.
//  Раз цвета нет, различаем яркостью и рисунком: активное — белая рамка, предупреждение — светлая плашка, ошибка — инверсия;
//  тепло и «+» — ярко и сплошной линией, холод и «−» — серым пунктиром; ряды графиков — яркость × тип линии.
static const RGBA
    C_BG      = hexc(0x0A0A0A),          // фон окна
    C_SCENE   = hexc(0x000000),          // фон сцены — чистый чёрный
    C_PANEL   = hexc(0x131313),          // панели (графит)
    C_PANEL2  = hexc(0x0D0D0D),          // полосы и заголовки внутри панелей (темнее панели), нажатая кнопка
    C_ELEM    = hexc(0x1D1D1D),          // элементы: кнопки, поля, дорожки слайдеров
    C_ELEM_H  = hexc(0x292929),          // элемент под курсором
    C_LINE    = hexc(0x2C2C2C),          // тонкие линии и рамки
    C_LINE_H  = hexc(0x4B4B4B),          // рамка под курсором
    C_TEXT    = hexc(0xC4C4C4),          // основной текст
    C_TEXT_HI = hexc(0xF2F2F2),          // значения, активный текст
    C_DIM     = hexc(0x828282),          // подписи, единицы измерения
    C_FAINT   = hexc(0x4C4C4C),          // неактивное / недоступное
    C_ACC     = hexc(0xFFFFFF),          // акцент (белый): активное состояние, выделение — всегда вместе с формой (рамка, линия)
    C_ACC_BG  = hexc(0xFFFFFF, 0.17f),   // подложка активного элемента (≈ #3D3D3D на панели)
    C_WARN    = hexc(0xFFFFFF),          // предупреждение: белый текст / метка (на плашке C_WARN_BG)
    C_WARN_BG = hexc(0xFFFFFF, 0.17f),   // плашка предупреждения
    C_ERR     = hexc(0xFFFFFF),          // ошибка: белая плашка (инверсия), текст на ней — C_ERR_INK
    C_ERR_INK = hexc(0x000000),          // текст ошибки на плашке
    C_HOT     = hexc(0xFFFFFF),          // тепло, выделение энергии, «+»: ярко, сплошной линией
    C_COLD    = hexc(0x8C8C8C),          // холод, поглощение энергии, «−»: приглушённо, пунктиром / контуром
    C_GRID    = {1, 1, 1, 0.055f},       // сетка графиков
    C_BOX     = hexc(0x999999, 0.5f),    // контур ящика на сцене
    C_MEAS    = hexc(0xEBEBEB);          // линейка / угломер
// ряды графиков и списки веществ: 12 сочетаний «уровень серого × тип линии» (3 уровня × 4 типа, все различны;
// соседние номера отличаются и яркостью, и рисунком). Тип линии — LS_SERIES[k], образец — styleSample().
enum { LS_SOLID, LS_DASH, LS_DOT, LS_DASHDOT };
static const RGBA C_SERIES[12] = {grayc(1.00f), grayc(0.70f), grayc(1.00f), grayc(0.70f), grayc(0.50f), grayc(1.00f),
                                  grayc(0.50f), grayc(0.70f), grayc(0.50f), grayc(1.00f), grayc(0.70f), grayc(0.50f)};
static const int LS_SERIES[12] = {LS_SOLID, LS_DASH, LS_DOT, LS_DASHDOT, LS_SOLID, LS_DASHDOT, LS_DASH, LS_SOLID, LS_DOT, LS_DASH, LS_DOT, LS_DASHDOT};
static inline RGBA withA(RGBA c, float a) { c.a = a; return c; }
static inline RGBA mulA(RGBA c, float k) { c.a *= k; return c; }

// ---- масштаб интерфейса: 1 — окна до ≈1200 px по высоте, 1.25 — 1440p, 2 — 4K (шрифты перепекаются)
static float uiScale = 1.0f;
static inline float uiPx(float v) { return std::floor(v * uiScale + 0.5f); }

// ---- пересчёт в реальные единицы (аргон, модель LJTS: σ = 0.3405 нм, ε/k = 139.8 K, τ = 0.999 пс, масса 10 а.е.м.; см. cfg в core.inl)
static inline double realK(double T) { return T * cfg::U_T_K; }
static inline double realBar(double P) { return P * cfg::U_P_ATM * 1.01325; }
static inline double realNm(double L) { return L * cfg::U_L_NM; }
static inline double realPs(double t) { return t * cfg::U_T_PS; }
// плотность массы, г/см³: 10 а.е.м./σ³ = 0.4206 г/см³
static inline double massDensity(double mass10, double vol) { return vol > 0 ? 0.42063 * mass10 / vol : 0; }

struct Glyph { float u0, v0, u1, v1, w, h, adv; };
struct Font { std::unordered_map<uint32_t, Glyph> g; float h = 12; };
// fontS/fontM — моноширинные (числа, таблицы), fontU/fontUB — подписи интерфейса (шрифт из настроек),
// fontL — заголовки, fontXS — мелкие подписи, fontXL — крупный символ элемента
static Font fontS, fontM, fontL, fontXS, fontXL, fontU, fontUB;

static std::vector<uint32_t> utf8(std::string_view s) {
    std::vector<uint32_t> out; size_t i = 0;
    while (i < s.size()) {
        unsigned char c = (unsigned char)s[i]; uint32_t cp; int len;
        if (c < 0x80) { cp = c; len = 1; } else if ((c >> 5) == 6) { cp = c & 31; len = 2; } else if ((c >> 4) == 14) { cp = c & 15; len = 3; } else { cp = c & 7; len = 4; }
        for (int k = 1; k < len && i + k < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i + k] & 63);
        out.push_back(cp); i += len;
    }
    return out;
}
// установлен ли шрифт с таким именем
static bool hasFace(HDC dc, const wchar_t* face) {
    LOGFONTW lf = {}; lf.lfCharSet = DEFAULT_CHARSET; wcsncpy_s(lf.lfFaceName, face, _TRUNCATE);
    bool found = false;
    EnumFontFamiliesExW(dc, &lf, [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM p) -> int { *(bool*)p = true; return 0; }, (LPARAM)&found, 0);
    return found;
}
// Атлас шрифтов: GDI рисует сглаженные глифы в DIB → текстура яркость/альфа. Пересобирается при смене масштаба и шрифта.
static void buildFonts() {
    const int AW = uiScale > 1.6f ? 2048 : 1024, AH = AW;
    BITMAPINFO bmi = {}; bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = AW; bmi.bmiHeader.biHeight = -AH; bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32; bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr; HDC mdc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateDIBSection(mdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) { if (bmp) DeleteObject(bmp); DeleteDC(mdc); return; }   // нет памяти под атлас — остаётся прежний шрифт
    HGDIOBJ oldb = SelectObject(mdc, bmp);
    memset(bits, 0, (size_t)AW * AH * 4);
    SetBkMode(mdc, TRANSPARENT); SetTextColor(mdc, RGB(255, 255, 255));
    std::vector<uint32_t> cps;
    auto range = [&](uint32_t a, uint32_t b) { for (uint32_t c = a; c <= b; c++) cps.push_back(c); };
    range(32, 126); range(0xA0, 0xFF); range(0x391, 0x3A9); range(0x3B1, 0x3C9); cps.push_back(0x401); range(0x410, 0x44F); cps.push_back(0x451);
    range(0x2010, 0x2027); cps.push_back(0x2030); cps.push_back(0x2032); cps.push_back(0x2033); range(0x2070, 0x2079); range(0x207A, 0x207B); range(0x2080, 0x2089);
    range(0x2190, 0x2195); cps.push_back(0x21C4); cps.push_back(0x21CC);
    for (uint32_t c : utf8("∂∆∑−√∞∝∠≈≠≡≤≥⋅⊕⊗⟨⟩▲▶▼◀●○■□✓✕")) cps.push_back(c);
    int px = 1, py = 1, rowH = 0;
    auto bake = [&](Font& f, float size, int weight, const wchar_t* face, bool asciiOnly = false) {
        f.g.clear();
        const int hpx = -(int)std::lround(size * uiScale);
        HFONT hf = CreateFontW(hpx, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, face);
        // запасной шрифт для символов, которых нет в основном (⇌, ∝, ⟨⟩ …)
        HFONT hfb = CreateFontW(hpx, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, L"Segoe UI Symbol");
        HGDIOBJ of = SelectObject(mdc, hfb); TEXTMETRICW tmb; GetTextMetricsW(mdc, &tmb);
        SelectObject(mdc, hf);
        TEXTMETRICW tm; GetTextMetricsW(mdc, &tm); f.h = (float)tm.tmHeight;
        for (uint32_t cp : cps) {
            if (asciiOnly && cp >= 127) continue;
            wchar_t wc = (wchar_t)cp; WORD gi = 0; bool fb = false;
            if (cp != 32 && GetGlyphIndicesW(mdc, &wc, 1, &gi, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR && gi == 0xFFFF) {   // нет в шрифте
                if (asciiOnly) continue;
                SelectObject(mdc, hfb);
                if (GetGlyphIndicesW(mdc, &wc, 1, &gi, GGI_MARK_NONEXISTING_GLYPHS) == GDI_ERROR || gi == 0xFFFF) { SelectObject(mdc, hf); continue; }
                fb = true;
            }
            SIZE sz; GetTextExtentPoint32W(mdc, &wc, 1, &sz);
            int w = sz.cx + 3;
            if (px + w >= AW) { px = 1; py += rowH + 2; rowH = 0; }
            if (py + tm.tmHeight >= AH) { if (fb) SelectObject(mdc, hf); break; }   // атлас заполнен
            TextOutW(mdc, px + 1, py + (fb ? (int)(tm.tmAscent - tmb.tmAscent) : 0), &wc, 1);   // выравнивание по базовой линии
            if (fb) SelectObject(mdc, hf);
            Glyph g; g.u0 = (float)px / AW; g.v0 = (float)py / AH; g.u1 = (float)(px + w) / AW; g.v1 = (float)(py + tm.tmHeight) / AH;
            g.w = (float)w; g.h = (float)tm.tmHeight; g.adv = (float)sz.cx;
            f.g[cp] = g; px += w + 2; rowH = std::max(rowH, (int)tm.tmHeight);
        }
        SelectObject(mdc, of); DeleteObject(hf); DeleteObject(hfb);
        px = 1; py += rowH + 4; rowH = 0;
    };
    // подписи — шрифтом из настроек (по умолчанию Bahnschrift: строгий гротеск с кириллицей, есть в Windows 10 с 2017 г.),
    // числа и таблицы — моноширинным (Cascadia Mono из Windows 11 и Терминала, иначе Consolas)
    static const wchar_t* UIF[4] = {L"Bahnschrift", L"Segoe UI", L"Calibri", L"Verdana"};
    const wchar_t* face = UIF[clampv(opt.font, 0, 3)];
    if (!hasFace(mdc, face)) face = L"Segoe UI";
    const bool bahn = !wcscmp(face, L"Bahnschrift");
    const wchar_t* bold = bahn && hasFace(mdc, L"Bahnschrift SemiBold") ? L"Bahnschrift SemiBold" : face;
    const int bw = bold != face ? 400 : 600;   // у Bahnschrift полужирный — отдельное начертание
    const wchar_t* mono = hasFace(mdc, L"Cascadia Mono") ? L"Cascadia Mono" : L"Consolas";
    const float k = bahn ? 1.04f : 1.0f;       // Bahnschrift чуть мельче при том же кегле
    bake(fontS, 13, 400, mono); bake(fontM, 15, 400, mono); bake(fontL, 18 * k, bw, bold);
    bake(fontXS, 12 * k, 400, face); bake(fontU, 13 * k, 400, face); bake(fontUB, 13 * k, bw, bold);
    bake(fontXL, 34, bw + 100, bold, true);
    GdiFlush();
    std::vector<unsigned char> la((size_t)AW * AH * 2);
    const unsigned char* p = (const unsigned char*)bits;
    for (int i = 0; i < AW * AH; i++) { la[2 * i] = 255; la[2 * i + 1] = std::max(p[4 * i], std::max(p[4 * i + 1], p[4 * i + 2])); }
    if (texFont) glDeleteTextures(1, &texFont);
    glGenTextures(1, &texFont); glBindTexture(GL_TEXTURE_2D, texFont);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, AW, AH, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, la.data());
    SelectObject(mdc, oldb); DeleteObject(bmp); DeleteDC(mdc);
}
static GLuint makeTex(int N, std::function<void(double, double, unsigned char&, unsigned char&)> f) {
    std::vector<unsigned char> d((size_t)N * N * 2);
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
        double x = (i + 0.5) / N * 2 - 1, y = (j + 0.5) / N * 2 - 1;
        f(x, y, d[2 * (j * N + i)], d[2 * (j * N + i) + 1]);
    }
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, N, N, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, d.data());
    return t;
}
static void buildTextures() {
    // мягкое пятно (вспышки реакций): гауссов профиль
    texGlow = makeTex(128, [](double x, double y, unsigned char& l, unsigned char& a) {
        double r2 = x * x + y * y; l = 255; a = (unsigned char)(255 * std::exp(-r2 * 4.5) * (r2 < 1 ? 1 : 0)); });
    // Атлас шара 384×128, три квадрата, яркость уже умножена на покрытие (смешивание с предумноженной альфой):
    //   [0] тело: рассеянный свет (Ламберт) от источника сверху-слева-спереди + немного окружающего, к краю темнеет
    //       почти до чёрного — объём как у отрендеренной модели;
    //   [1] блик Блинна–Фонга (складывается поверх, альфа 0) — белый, даже на красном кислороде;
    //   [2] ровный диск (маска): туман к цвету фона, заливки.
    // Цилиндры связей берут тот же атлас вдоль диаметра, перпендикулярного связи на экране, — освещение
    // шаров и связей согласовано, как на фотографии шаростержневой модели.
    {
        const int N = 128; std::vector<unsigned char> d((size_t)3 * N * N * 2);
        const double Lx = -0.42, Ly = -0.58, Lz = 0.70, ll = std::sqrt(Lx * Lx + Ly * Ly + Lz * Lz);
        const double lx = Lx / ll, ly = Ly / ll, lz = Lz / ll, hx0 = lx, hy0 = ly, hz0 = lz + 1, hl = std::sqrt(hx0 * hx0 + hy0 * hy0 + hz0 * hz0);
        for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
            double x = (i + 0.5) / N * 2 - 1, y = (j + 0.5) / N * 2 - 1, r2 = x * x + y * y, r = std::sqrt(r2);
            double edge = clampv((1.0 - r) / 0.035, 0.0, 1.0), nz = std::sqrt(std::max(0.0, 1 - r2));
            double diff = std::max(0.0, x * lx + y * ly + nz * lz);
            double shade = 0.10 + 0.84 * std::pow(diff, 1.15) + 0.10 * nz * nz;
            double spec = std::pow(std::max(0.0, (x * hx0 + y * hy0 + nz * hz0) / hl), 48.0);
            double soft = std::pow(std::max(0.0, (x * hx0 + y * hy0 + nz * hz0) / hl), 6.0);   // широкий мягкий ореол вокруг блика
            size_t row = (size_t)j * 3 * N, c0 = (row + i) * 2, c1 = (row + N + i) * 2, c2 = (row + 2 * N + i) * 2;
            d[c0] = (unsigned char)(255 * clampv(shade * edge, 0.0, 1.0)); d[c0 + 1] = (unsigned char)(255 * edge);
            d[c1] = (unsigned char)(255 * clampv((0.95 * spec + 0.10 * soft) * edge, 0.0, 1.0)); d[c1 + 1] = 0;
            d[c2] = d[c2 + 1] = (unsigned char)(255 * edge);
        }
        glGenTextures(1, &texCore); glBindTexture(GL_TEXTURE_2D, texCore);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 3 * N, N, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, d.data());
    }
}
static inline void col(const RGBA& c) { glColor4f(c.r, c.g, c.b, c.a); }
static void rectFill(float x, float y, float w, float h, RGBA c) {
    glDisable(GL_TEXTURE_2D); col(c);
    glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}
static void rectLine(float x, float y, float w, float h, RGBA c) {
    glDisable(GL_TEXTURE_2D); col(c);
    glBegin(GL_LINE_LOOP); glVertex2f(x + 0.5f, y + 0.5f); glVertex2f(x + w - 0.5f, y + 0.5f); glVertex2f(x + w - 0.5f, y + h - 0.5f); glVertex2f(x + 0.5f, y + h - 0.5f); glEnd();
}
// горизонтальная / вертикальная линия толщиной 1 px (по пиксельной сетке)
static void lineH(float x0, float x1, float y, RGBA c) { rectFill(std::floor(x0), std::floor(y), std::floor(x1) - std::floor(x0), 1, c); }
static void lineV(float x, float y0, float y1, RGBA c) { rectFill(std::floor(x), std::floor(y0), 1, std::floor(y1) - std::floor(y0), c); }
// прямоугольник со скруглением (строгий стиль: радиус не больше 3 px) и его контур
static void roundRect(float x, float y, float w, float h, float r, RGBA c) {
    r = std::min(r, 3.0f * uiScale);
    r = std::min(r, std::min(w, h) * 0.5f); if (r < 1.5f) { rectFill(x, y, w, h, c); return; }
    glDisable(GL_TEXTURE_2D); col(c);
    glBegin(GL_TRIANGLE_FAN); glVertex2f(x + w / 2, y + h / 2);
    const float cx[4] = {x + w - r, x + w - r, x + r, x + r}, cy[4] = {y + r, y + h - r, y + h - r, y + r};
    for (int q = 0; q < 4; q++) for (int k = 0; k <= 4; k++) {
        double a = (q - 1) * PI / 2 + k * PI / 8;   // −90°…0° (верх-право), 0…90 (низ-право), 90…180, 180…270
        glVertex2f(cx[q] + r * (float)std::cos(a), cy[q] + r * (float)std::sin(a));
    }
    glVertex2f(x + w - r, y);
    glEnd();
}
static void roundLine(float x, float y, float w, float h, float r, RGBA c) {
    r = std::min(r, 3.0f * uiScale);
    r = std::min(r, std::min(w, h) * 0.5f);
    if (r < 1.5f) { rectLine(x, y, w, h, c); return; }
    glDisable(GL_TEXTURE_2D); col(c);
    glBegin(GL_LINE_LOOP);
    const float cx[4] = {x + w - r - 0.5f, x + w - r - 0.5f, x + r + 0.5f, x + r + 0.5f}, cy[4] = {y + r + 0.5f, y + h - r - 0.5f, y + h - r - 0.5f, y + r + 0.5f};
    for (int q = 0; q < 4; q++) for (int k = 0; k <= 4; k++) { double a = (q - 1) * PI / 2 + k * PI / 8; glVertex2f(cx[q] + r * (float)std::cos(a), cy[q] + r * (float)std::sin(a)); }
    glEnd();
}
// панель/рамка: заливка + контур 1 px
static void boxPanel(float x, float y, float w, float h, RGBA fill, RGBA line) { rectFill(x, y, w, h, fill); rectLine(x, y, w, h, line); }
// ---- тип линии (монохромная замена цвета ряда): сплошная, штрих 8/8, точки 2/2, штрих-пунктир 7/3/2/4 (масштаб — uiScale)
static void lineStyle(int s) {
    if (s == LS_SOLID) { glDisable(GL_LINE_STIPPLE); return; }
    static const GLushort pat[4] = {0xFFFF, 0x00FF, 0x3333, 0x0C7F};
    glEnable(GL_LINE_STIPPLE); glLineStipple(std::max(1, (int)std::lround(uiScale)), pat[s & 3]);
}
// образец линии для легенды: отрезок длиной w на высоте y
static void styleSample(float x, float y, float w, RGBA c, int s, float lw = 1.3f) {
    glDisable(GL_TEXTURE_2D); lineStyle(s); glLineWidth(lw); col(c);
    glBegin(GL_LINES); glVertex2f(x, std::floor(y) + 0.5f); glVertex2f(x + w, std::floor(y) + 0.5f); glEnd();
    glLineWidth(1); lineStyle(LS_SOLID);
}
// ---- состояние значения (вместо янтарного / красного текста): VS_WARN — светлая плашка под белым текстом,
//      VS_ERR — инверсия (белая плашка, чёрный текст). Плашка рисуется до текста, цвет текста — stateInk().
enum { VS_OK, VS_WARN, VS_ERR };
static void statePlate(float x, float y, float w, float h, int st) {
    if (st == VS_WARN) { rectFill(x, y, w, h, C_WARN_BG); rectFill(x, y, uiPx(2), h, C_WARN); }
    else if (st == VS_ERR) rectFill(x, y, w, h, C_ERR);
}
static inline RGBA stateInk(int st, RGBA normal) { return st == VS_ERR ? C_ERR_INK : st == VS_WARN ? C_WARN : normal; }
// Весь текст выводится через эти функции; они переводят строку (lang.inl) — ширина и рисование — по одному тексту.
// …Raw — без перевода (текст уже переведён: перенос строк, подсказки, формулы).
static float textWRaw(const Font& f, std::string_view s) {
    float w = 0; for (uint32_t c : utf8(s)) { auto it = f.g.find(c); w += it == f.g.end() ? f.h * 0.5f : it->second.adv; } return w;
}
static float drawTextRaw(const Font& f, float x, float y, std::string_view s, RGBA c) {
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texFont); col(c);
    x = std::floor(x); y = std::floor(y);
    float x0 = x;
    glBegin(GL_QUADS);
    for (uint32_t cp : utf8(s)) {
        auto it = f.g.find(cp);
        if (it == f.g.end()) { x += f.h * 0.5f; continue; }
        const Glyph& g = it->second;
        glTexCoord2f(g.u0, g.v0); glVertex2f(x, y); glTexCoord2f(g.u1, g.v0); glVertex2f(x + g.w, y);
        glTexCoord2f(g.u1, g.v1); glVertex2f(x + g.w, y + g.h); glTexCoord2f(g.u0, g.v1); glVertex2f(x, y + g.h);
        x += g.adv;
    }
    glEnd(); glDisable(GL_TEXTURE_2D);
    return x - x0;
}
static float textW(const Font& f, std::string_view s) { return textWRaw(f, Tsv(s, "textW")); }
static float drawText(const Font& f, float x, float y, std::string_view s, RGBA c) { return drawTextRaw(f, x, y, Tsv(s, "drawText"), c); }
static float drawTextR(const Font& f, float xr, float y, std::string_view s, RGBA c) { s = Tsv(s, "drawText"); float w = textWRaw(f, s); drawTextRaw(f, xr - w, y, s, c); return w; }
static float drawTextC(const Font& f, float xc, float y, std::string_view s, RGBA c) { s = Tsv(s, "drawText"); float w = textWRaw(f, s); drawTextRaw(f, xc - w / 2, y, s, c); return w; }
// формула ли строка (s — до перевода): только ASCII; в английском режиме — ещё и без пробелов и «%»
// (подписи вроде «газ 35%» после перевода становятся ASCII, но остаются текстом, а не формулой)
static inline bool chemFormula(std::string_view s) {
    for (char ch : s) if ((unsigned char)ch >= 0x80 || (LANG != LANG_RU && (ch == ' ' || ch == '%'))) return false;
    return true;
}
// химическая формула: цифры после букв — нижний индекс, +/- в конце — верхний
// индекс — цифры после символа элемента или «)» (H2O, (CH3)2CO); заряд — последний «+»/«−» (H3O+, NH4+, OH-)
static inline bool chemSub(std::string_view s, size_t k, char prev) {
    const char ch = s[k];
    return ch >= '0' && ch <= '9' && (std::isalpha((unsigned char)prev) || prev == ')' || (prev >= '0' && prev <= '9' && k > 1 && !(s[k - 2] == ' ')));
}
static inline bool chemSup(std::string_view s, size_t k, char prev) {
    const char ch = s[k];
    return (ch == '+' || ch == '-') && k + 1 == s.size() && (std::isalpha((unsigned char)prev) || (prev >= '0' && prev <= '9'));
}
static float drawChem(const Font& f, float x, float y, std::string_view s, RGBA c) {
    if (!chemFormula(s)) return drawTextRaw(f, x, y, Tsv(s, "drawChem"), c);   // не формула (кириллица → перевод)
    float x0 = x; char prev = 0;
    for (size_t k = 0; k < s.size(); k++) {
        char ch = s[k]; std::string_view one = s.substr(k, 1);
        bool sub = chemSub(s, k, prev), sup = chemSup(s, k, prev);
        if (sub) x += drawTextRaw(fontXS, x, y + f.h * 0.34f, one, c);
        else if (sup) x += drawTextRaw(fontXS, x, y - f.h * 0.22f, ch == '-' ? "−" : "+", c);
        else x += drawTextRaw(f, x, y, one, c);
        prev = ch;
    }
    return x - x0;
}
static float chemW(const Font& f, std::string_view s) {
    if (!chemFormula(s)) return textWRaw(f, Tsv(s, "chemW"));
    float w = 0; char prev = 0;
    for (size_t k = 0; k < s.size(); k++) {
        char ch = s[k]; bool index = chemSub(s, k, prev) || chemSup(s, k, prev);
        w += textWRaw(index ? fontXS : f, s.substr(k, 1)); prev = ch;
    }
    return w;
}
static void hsv(float h, float s, float v, float& r, float& g, float& b) {
    h = h - std::floor(h); float i = std::floor(h * 6), f = h * 6 - i, p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch ((int)i % 6) { case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break;
                          case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; }
}
// цвет излучения абсолютно чёрного тела (sRGB, нормирован по самой яркой компоненте) при температуре T, K —
// таблица по расчёту М. Чарити (цветовые координаты CIE 1931, 2°), между узлами линейно
static void blackbody(float T, float& r, float& g, float& b) {
    static const float TT[] = {800, 1000, 1500, 2000, 2500, 3000, 4000, 5000, 6500, 8000, 10000, 15000};
    static const float C[][3] = {{1.00f, 0.10f, 0.00f}, {1.00f, 0.22f, 0.00f}, {1.00f, 0.43f, 0.00f}, {1.00f, 0.54f, 0.07f}, {1.00f, 0.63f, 0.28f},
                                 {1.00f, 0.71f, 0.42f}, {1.00f, 0.82f, 0.64f}, {1.00f, 0.89f, 0.81f}, {1.00f, 0.99f, 0.98f}, {0.89f, 0.91f, 1.00f},
                                 {0.80f, 0.86f, 1.00f}, {0.71f, 0.80f, 1.00f}};
    const int N = sizeof(TT) / sizeof(*TT);
    if (T <= TT[0]) { r = C[0][0]; g = C[0][1]; b = C[0][2]; return; }
    for (int k = 1; k < N; k++) if (T <= TT[k]) {
        const float t = (T - TT[k - 1]) / (TT[k] - TT[k - 1]);
        r = C[k - 1][0] + t * (C[k][0] - C[k - 1][0]); g = C[k - 1][1] + t * (C[k][1] - C[k - 1][1]); b = C[k - 1][2] + t * (C[k][2] - C[k - 1][2]);
        return;
    }
    r = C[N - 1][0]; g = C[N - 1][1]; b = C[N - 1][2];
}
// ---- векторные примитивы для значков и разметки сцены
static void segPx(float x0, float y0, float x1, float y1) { glVertex2f(x0, y0); glVertex2f(x1, y1); }
static void arrowPx(float x0, float y0, float x1, float y1, float head) {   // стрелка (внутри glBegin(GL_LINES))
    float dx = x1 - x0, dy = y1 - y0, l = std::sqrt(dx * dx + dy * dy); if (l < 0.5f) return;
    dx /= l; dy /= l; float hs = std::min(head, l * 0.5f);
    segPx(x0, y0, x1, y1);
    segPx(x1, y1, x1 - hs * dx + hs * 0.45f * dy, y1 - hs * dy - hs * 0.45f * dx);
    segPx(x1, y1, x1 - hs * dx - hs * 0.45f * dy, y1 - hs * dy + hs * 0.45f * dx);
}
static void circlePx(float cx, float cy, float R, int seg = 64) {
    glBegin(GL_LINE_LOOP); for (int k = 0; k < seg; k++) { double a = 2 * PI * k / seg; glVertex2d(cx + R * std::cos(a), cy + R * std::sin(a)); } glEnd();
}
static void discPx(float cx, float cy, float R, int seg = 48) {
    glBegin(GL_TRIANGLE_FAN); glVertex2f(cx, cy); for (int k = 0; k <= seg; k++) { double a = 2 * PI * k / seg; glVertex2d(cx + R * std::cos(a), cy + R * std::sin(a)); } glEnd();
}
static void arcPx(float cx, float cy, float R, float a0, float a1, int seg = 24) {   // дуга (внутри glBegin(GL_LINES))
    for (int k = 0; k < seg; k++) { float t0 = a0 + (a1 - a0) * k / seg, t1 = a0 + (a1 - a0) * (k + 1) / seg;
        segPx(cx + R * std::cos(t0), cy + R * std::sin(t0), cx + R * std::cos(t1), cy + R * std::sin(t1)); }
}
static void dashedCircle(float cx, float cy, float R) {
    int seg = std::max(24, std::min(160, (int)(R * 0.8f))); if (seg & 1) seg++;
    glBegin(GL_LINES); for (int k = 0; k < seg; k += 2) { double a = 2 * PI * k / seg, b = 2 * PI * (k + 1) / seg;
        glVertex2d(cx + R * std::cos(a), cy + R * std::sin(a)); glVertex2d(cx + R * std::cos(b), cy + R * std::sin(b)); } glEnd();
}

// ---- значки (векторные, монохромные): инструменты, объекты поля, кнопки верхней панели
enum { IC_CAMERA, IC_SELECT, IC_MEASURE, IC_ADD, IC_ERASE, IC_HEAT, IC_COOL, IC_PUSH, IC_SHOCK, IC_CUT, IC_FIELD,
       IC_FO0,   // + FO_* — значки видов объектов поля
       IC_PLAY = IC_FO0 + FO_N, IC_PAUSE, IC_STEP, IC_RESET, IC_UNDO, IC_OPEN, IC_SAVE, IC_SHOT, IC_REC, IC_HELP, IC_PANEL, IC_GEAR, IC_N };
static void drawIcon(int ic, float cx, float cy, float s, RGBA c) {
    glDisable(GL_TEXTURE_2D); col(c); glEnable(GL_LINE_SMOOTH); glLineWidth(std::max(1.0f, 1.25f * uiScale));
    const float h = s * 0.5f;
    auto thermo = [&](float ox) {   // термометр
        glBegin(GL_LINES); segPx(ox, cy - h * 0.85f, ox, cy + h * 0.35f); glEnd(); circlePx(ox, cy + h * 0.55f, h * 0.24f, 16);
    };
    switch (ic) {
    case IC_CAMERA: {   // глаз
        glBegin(GL_LINES); arcPx(cx, cy + h * 0.9f, h * 1.25f, -2.3f, -0.84f, 12); arcPx(cx, cy - h * 0.9f, h * 1.25f, 0.84f, 2.3f, 12); glEnd();
        circlePx(cx, cy, h * 0.3f, 16); discPx(cx, cy, h * 0.12f, 10); break; }
    case IC_SELECT: {   // пунктирная рамка + стрелка курсора
        glBegin(GL_LINES);
        for (int k = 0; k < 4; k++) { float a = -h * 0.85f + k * h * 0.45f; segPx(cx + a, cy - h * 0.8f, cx + a + h * 0.25f, cy - h * 0.8f); segPx(cx - h * 0.85f, cy + a + h * 0.05f, cx - h * 0.85f, cy + a + h * 0.3f); }
        segPx(cx - h * 0.1f, cy - h * 0.1f, cx + h * 0.85f, cy + h * 0.85f); segPx(cx - h * 0.1f, cy - h * 0.1f, cx + h * 0.45f, cy - h * 0.05f);
        segPx(cx - h * 0.1f, cy - h * 0.1f, cx - h * 0.05f, cy + h * 0.45f); glEnd(); break; }
    case IC_MEASURE: {  // угломер: два луча из вершины и дуга
        glBegin(GL_LINES); segPx(cx - h * 0.8f, cy + h * 0.7f, cx + h * 0.85f, cy + h * 0.7f); segPx(cx - h * 0.8f, cy + h * 0.7f, cx + h * 0.5f, cy - h * 0.8f);
        arcPx(cx - h * 0.8f, cy + h * 0.7f, h * 0.9f, -0.86f, 0.0f, 8); glEnd();
        discPx(cx + h * 0.85f, cy + h * 0.7f, h * 0.13f, 8); discPx(cx + h * 0.5f, cy - h * 0.8f, h * 0.13f, 8); break; }
    case IC_ADD: { circlePx(cx, cy, h * 0.8f, 24); glBegin(GL_LINES); segPx(cx - h * 0.4f, cy, cx + h * 0.4f, cy); segPx(cx, cy - h * 0.4f, cx, cy + h * 0.4f); glEnd(); break; }
    case IC_ERASE: {    // ластик: повёрнутый прямоугольник
        float a = 0.7854f, ca = std::cos(a), sa = std::sin(a); float pts[4][2] = {{-0.85f, -0.35f}, {0.85f, -0.35f}, {0.85f, 0.35f}, {-0.85f, 0.35f}};
        glBegin(GL_LINE_LOOP); for (auto& p : pts) glVertex2f(cx + h * (p[0] * ca - p[1] * sa), cy + h * (p[0] * sa + p[1] * ca)); glEnd();
        glBegin(GL_LINES); segPx(cx + h * (-0.1f * ca + 0.35f * sa), cy + h * (-0.1f * sa - 0.35f * ca), cx + h * (-0.1f * ca - 0.35f * sa), cy + h * (-0.1f * sa + 0.35f * ca));
        segPx(cx - h * 0.9f, cy + h * 0.9f, cx + h * 0.2f, cy + h * 0.9f); glEnd(); break; }
    case IC_HEAT: case IC_FO0 + FO_HEATER: { thermo(cx - h * 0.25f); glBegin(GL_LINES); segPx(cx + h * 0.3f, cy - h * 0.35f, cx + h * 0.9f, cy - h * 0.35f); segPx(cx + h * 0.6f, cy - h * 0.65f, cx + h * 0.6f, cy - h * 0.05f); glEnd(); break; }
    case IC_COOL: case IC_FO0 + FO_COOLER: { thermo(cx - h * 0.25f); glBegin(GL_LINES); segPx(cx + h * 0.3f, cy - h * 0.35f, cx + h * 0.9f, cy - h * 0.35f); glEnd(); break; }
    case IC_PUSH: case IC_FO0 + FO_REPEL: case IC_FO0 + FO_ATTRACT: {   // точка и четыре залитых треугольника: наружу (отталкивание) или внутрь (притяжение)
        const bool inward = ic == IC_FO0 + FO_ATTRACT;
        discPx(cx, cy, h * 0.22f, 12);
        if (inward) circlePx(cx, cy, h * 0.92f, 24);
        else { glBegin(GL_LINES); for (int k = 0; k < 4; k++) { float a = k * 1.5708f + 0.7854f; segPx(cx + std::cos(a) * h * 0.3f, cy + std::sin(a) * h * 0.3f, cx + std::cos(a) * h * 0.7f, cy + std::sin(a) * h * 0.7f); } glEnd(); }
        glBegin(GL_TRIANGLES);
        for (int k = 0; k < 4; k++) {
            float a = k * 1.5708f + 0.7854f, c0 = std::cos(a), s0 = std::sin(a), ta = -s0, tb = c0;
            float tip = inward ? h * 0.38f : h * 1.08f, base = inward ? h * 0.78f : h * 0.62f, wdt = h * 0.3f;
            glVertex2f(cx + c0 * tip, cy + s0 * tip); glVertex2f(cx + c0 * base + ta * wdt, cy + s0 * base + tb * wdt); glVertex2f(cx + c0 * base - ta * wdt, cy + s0 * base - tb * wdt);
        }
        glEnd(); break; }
    case IC_SHOCK: { discPx(cx - h * 0.55f, cy, h * 0.15f, 10); glBegin(GL_LINES); for (int k = 1; k <= 3; k++) arcPx(cx - h * 0.55f, cy, h * 0.45f * k, -0.9f, 0.9f, 10); glEnd(); break; }
    case IC_CUT: {      // ножницы
        circlePx(cx - h * 0.45f, cy + h * 0.55f, h * 0.28f, 14); circlePx(cx + h * 0.45f, cy + h * 0.55f, h * 0.28f, 14);
        glBegin(GL_LINES); segPx(cx - h * 0.25f, cy + h * 0.33f, cx + h * 0.55f, cy - h * 0.9f); segPx(cx + h * 0.25f, cy + h * 0.33f, cx - h * 0.55f, cy - h * 0.9f); glEnd(); break; }
    case IC_FIELD: { discPx(cx, cy, h * 0.18f, 10); circlePx(cx, cy, h * 0.45f, 20); dashedCircle(cx, cy, h * 0.9f); break; }
    case IC_FO0 + FO_WIND: { glBegin(GL_LINES); for (int k = -1; k <= 1; k++) arrowPx(cx - h * 0.85f + (k == 0 ? h * 0.2f : 0), cy + k * h * 0.55f, cx + h * 0.85f, cy + k * h * 0.55f, h * 0.35f); glEnd(); break; }
    case IC_FO0 + FO_VORTEX: { glBegin(GL_LINES); arcPx(cx, cy, h * 0.75f, 0.3f, 5.6f, 20);
        float a = 5.6f, ex = cx + h * 0.75f * std::cos(a), ey = cy + h * 0.75f * std::sin(a);
        segPx(ex, ey, ex - h * 0.4f, ey - h * 0.05f); segPx(ex, ey, ex + h * 0.05f, ey + h * 0.4f); glEnd(); discPx(cx, cy, h * 0.13f, 8); break; }
    case IC_FO0 + FO_TRAP: { circlePx(cx, cy, h * 0.55f, 20); glBegin(GL_LINES); segPx(cx - h, cy, cx - h * 0.3f, cy); segPx(cx + h * 0.3f, cy, cx + h, cy);
        segPx(cx, cy - h, cx, cy - h * 0.3f); segPx(cx, cy + h * 0.3f, cx, cy + h); glEnd(); discPx(cx, cy, h * 0.12f, 8); break; }
    case IC_FO0 + FO_EMITTER: { circlePx(cx - h * 0.5f, cy, h * 0.35f, 14); glBegin(GL_LINES); arrowPx(cx - h * 0.1f, cy, cx + h * 0.95f, cy, h * 0.4f); glEnd();
        discPx(cx + h * 0.35f, cy - h * 0.55f, h * 0.1f, 6); discPx(cx + h * 0.6f, cy + h * 0.55f, h * 0.1f, 6); break; }
    case IC_FO0 + FO_SINK: { circlePx(cx, cy, h * 0.85f, 22); circlePx(cx, cy, h * 0.5f, 18); discPx(cx, cy, h * 0.2f, 10); break; }
    case IC_FO0 + FO_BARRIER: { glLineWidth(std::max(2.0f, 2.5f * uiScale)); glBegin(GL_LINES); segPx(cx - h * 0.7f, cy + h * 0.8f, cx + h * 0.7f, cy - h * 0.8f); glEnd();
        glLineWidth(std::max(1.0f, 1.25f * uiScale)); discPx(cx - h * 0.7f, cy + h * 0.8f, h * 0.16f, 8); discPx(cx + h * 0.7f, cy - h * 0.8f, h * 0.16f, 8); break; }
    case IC_PLAY: { glBegin(GL_TRIANGLES); glVertex2f(cx - h * 0.45f, cy - h * 0.6f); glVertex2f(cx + h * 0.6f, cy); glVertex2f(cx - h * 0.45f, cy + h * 0.6f); glEnd(); break; }
    case IC_PAUSE: { rectFill(cx - h * 0.5f, cy - h * 0.6f, h * 0.32f, h * 1.2f, c); rectFill(cx + h * 0.18f, cy - h * 0.6f, h * 0.32f, h * 1.2f, c); break; }
    case IC_STEP: { glBegin(GL_TRIANGLES); glVertex2f(cx - h * 0.6f, cy - h * 0.6f); glVertex2f(cx + h * 0.3f, cy); glVertex2f(cx - h * 0.6f, cy + h * 0.6f); glEnd();
        rectFill(cx + h * 0.35f, cy - h * 0.6f, h * 0.28f, h * 1.2f, c); break; }
    case IC_RESET: case IC_UNDO: {
        glBegin(GL_LINES); arcPx(cx, cy, h * 0.7f, ic == IC_RESET ? -2.6f : -2.9f, ic == IC_RESET ? 2.2f : 1.3f, 18);
        float a = ic == IC_RESET ? -2.6f : -2.9f, ex = cx + h * 0.7f * std::cos(a), ey = cy + h * 0.7f * std::sin(a);
        segPx(ex, ey, ex + h * 0.45f, ey - h * 0.05f); segPx(ex, ey, ex + h * 0.05f, ey + h * 0.45f); glEnd(); break; }
    case IC_OPEN: { glBegin(GL_LINE_LOOP); glVertex2f(cx - h * 0.9f, cy - h * 0.6f); glVertex2f(cx - h * 0.3f, cy - h * 0.6f); glVertex2f(cx - h * 0.1f, cy - h * 0.35f);
        glVertex2f(cx + h * 0.9f, cy - h * 0.35f); glVertex2f(cx + h * 0.9f, cy + h * 0.7f); glVertex2f(cx - h * 0.9f, cy + h * 0.7f); glEnd(); break; }
    case IC_SAVE: { rectLine(cx - h * 0.8f, cy - h * 0.8f, h * 1.6f, h * 1.6f, c); rectLine(cx - h * 0.45f, cy - h * 0.8f, h * 0.9f, h * 0.55f, c);
        rectFill(cx - h * 0.45f, cy + h * 0.2f, h * 0.9f, h * 0.5f, withA(c, c.a * 0.6f)); break; }
    case IC_SHOT: { rectLine(cx - h * 0.9f, cy - h * 0.5f, h * 1.8f, h * 1.3f, c); rectLine(cx - h * 0.35f, cy - h * 0.8f, h * 0.7f, h * 0.32f, c); circlePx(cx, cy + h * 0.15f, h * 0.38f, 16); break; }
    case IC_REC: { circlePx(cx, cy, h * 0.75f, 20); discPx(cx, cy, h * 0.42f, 16); break; }
    case IC_HELP: { circlePx(cx, cy, h * 0.85f, 22); break; }
    case IC_PANEL: { rectLine(cx - h * 0.9f, cy - h * 0.7f, h * 1.8f, h * 1.4f, c); rectFill(cx + h * 0.25f, cy - h * 0.7f, h * 0.65f, h * 1.4f, withA(c, c.a * 0.5f)); break; }
    case IC_GEAR: {     // шестерёнка: кольцо и восемь зубцов
        circlePx(cx, cy, h * 0.55f, 24); circlePx(cx, cy, h * 0.22f, 14);
        glLineWidth(std::max(2.0f, 2.6f * uiScale)); glBegin(GL_LINES);
        for (int k = 0; k < 8; k++) { float a = k * 0.7854f, ca = std::cos(a), sa = std::sin(a); segPx(cx + ca * h * 0.58f, cy + sa * h * 0.58f, cx + ca * h * 0.9f, cy + sa * h * 0.9f); }
        glEnd(); break; }
    default: break;
    }
    glLineWidth(1); glDisable(GL_LINE_SMOOTH);
}

// ===================================== КАМЕРА ==========================================
// Перспективная камера на сфере вокруг цели (yaw, pitch, dist).
// Любой ввод меняет «цель» camGoal, а сама камера cam3 плавно догоняет её (экспоненциальное сглаживание) —
// повороты, наезды, готовые ракурсы и слежение за атомом выглядят плавно.
static float sceneX = 0, sceneY = 46, sceneW = 1000, sceneH = 760;
struct Cam3 { double yaw = 0.65, pitch = 0.42, dist = 50, tx = 0, ty = 0, tz = 0, fov = 38; bool autoRot = false; } cam3, camGoal;
static int camMode = 0;                 // 0 — орбита вокруг цели, 1 — полёт (WASD/QE)
static bool sliceOn = false; static double sliceOff = 0;   // разрез: скрыть всё ближе плоскости (dist + sliceOff)
static double eyeP[3], camR[3], camU[3], camF[3], focal = 1000, scx = 0, scy = 0;
static bool graphsOn = true, helpOn = false;   // graphsOn — видна правая боковая панель (клавиша G)
static int mouseX = 0, mouseY = 0;
// состояние клавиш (в режиме автотеста интерфейса подменяется синтетическим)
static bool uiTestMode = false; static unsigned char testKeys[256];
static inline bool isDown(int vk) { return uiTestMode ? testKeys[vk & 255] != 0 : (GetKeyState(vk) & 0x8000) != 0; }

static inline void camOffset(const Cam3& c, double* off) {   // вектор от цели к глазу
    double cp = std::cos(c.pitch), sp = std::sin(c.pitch), cy = std::cos(c.yaw), sy = std::sin(c.yaw);
    off[0] = c.dist * cp * sy; off[1] = c.dist * sp; off[2] = c.dist * cp * cy;
}
static void camSetup() {
    scx = sceneX + sceneW / 2; scy = sceneY + sceneH / 2;
    double off[3]; camOffset(cam3, off);
    eyeP[0] = cam3.tx + off[0]; eyeP[1] = cam3.ty + off[1]; eyeP[2] = cam3.tz + off[2];
    for (int k = 0; k < 3; k++) camF[k] = -off[k] / cam3.dist;
    // R = F × (0,1,0), U = R × F
    camR[0] = -camF[2]; camR[1] = 0; camR[2] = camF[0];
    double rl = std::sqrt(camR[0] * camR[0] + camR[2] * camR[2]); if (rl < 1e-9) rl = 1e-9;
    camR[0] /= rl; camR[2] /= rl;
    camU[0] = camR[1] * camF[2] - camR[2] * camF[1]; camU[1] = camR[2] * camF[0] - camR[0] * camF[2]; camU[2] = camR[0] * camF[1] - camR[1] * camF[0];
    focal = (sceneH / 2) / std::tan(cam3.fov * PI / 360);
}
// глубина плоскости разреза (расстояние от глаза вдоль взгляда)
static inline double sliceDepth() { return cam3.dist + sliceOff; }
// мировые координаты → экран; depth — расстояние вдоль взгляда, scale — пикселей на σ в этой точке
static inline bool project(double x, double y, double z, float& sx, float& sy, float& depth, float& scale) {
    double dx = x - eyeP[0], dy = y - eyeP[1], dz = z - eyeP[2];
    double zv = dx * camF[0] + dy * camF[1] + dz * camF[2];
    if (zv < 0.3) return false;
    double xv = dx * camR[0] + dy * camR[1] + dz * camR[2], yv = dx * camU[0] + dy * camU[1] + dz * camU[2];
    double s = focal / zv;
    sx = (float)(scx + xv * s); sy = (float)(scy - yv * s); depth = (float)zv; scale = (float)s;
    return true;
}
static inline double viewDepth(double x, double y, double z) { return (x - eyeP[0]) * camF[0] + (y - eyeP[1]) * camF[1] + (z - eyeP[2]) * camF[2]; }
// луч взгляда через точку экрана
static void mouseRay(double mx, double my, double* o, double* d) {
    double a = (mx - scx) / focal, b = -(my - scy) / focal;
    for (int k = 0; k < 3; k++) { o[k] = eyeP[k]; d[k] = camR[k] * a + camU[k] * b + camF[k]; }
    double l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]); for (int k = 0; k < 3; k++) d[k] /= l;
}
// точка на луче на заданной глубине (вдоль направления взгляда)
static void unprojectAtDepth(double mx, double my, double depth, double& x, double& y, double& z) {
    double a = (mx - scx) / focal * depth, b = -(my - scy) / focal * depth;
    x = eyeP[0] + camR[0] * a + camU[0] * b + camF[0] * depth;
    y = eyeP[1] + camR[1] * a + camU[1] * b + camF[1] * depth;
    z = eyeP[2] + camR[2] * a + camU[2] * b + camF[2] * depth;
}
// точка под курсором на плоскости через центр ящика, перпендикулярной взгляду (для объектов поля)
static void cursorOnMidPlane(double mx, double my, double& x, double& y, double& z) {
    unprojectAtDepth(mx, my, viewDepth(S.Lx / 2, S.Ly / 2, S.Lz / 2), x, y, z);
}
// пересечение луча с ящиком (метод пластин): отрезок [t0, t1]
static bool rayBox(const double* o, const double* d, double& t0, double& t1) {
    double L[3] = {S.Lx, S.Ly, S.Lz};
    t0 = -1e30; t1 = 1e30;
    for (int k = 0; k < 3; k++) {
        if (std::fabs(d[k]) < 1e-12) { if (o[k] < 0 || o[k] > L[k]) return false; continue; }
        double a = (0 - o[k]) / d[k], b = (L[k] - o[k]) / d[k]; if (a > b) std::swap(a, b);
        t0 = std::max(t0, a); t1 = std::min(t1, b);
    }
    return t1 > std::max(t0, 0.0);
}
// вписать ящик в окно; snap — мгновенно (при смене пресета), иначе плавный наезд
static double fitZoom = 1.0;   // --shot … zoom=K: ближе (K < 1) или дальше, чем «вписать»
static void fitView(bool snap = true) {
    camSetup();
    camGoal.tx = S.Lx / 2; camGoal.ty = S.Ly / 2; camGoal.tz = S.Lz / 2; camGoal.fov = cam3.fov;
    double r = 0.5 * std::sqrt(S.Lx * S.Lx + S.Ly * S.Ly + S.Lz * S.Lz);
    double fh = cam3.fov * PI / 360, fw = std::atan(std::tan(fh) * sceneW / sceneH);
    camGoal.dist = r / std::sin(std::min(fh, fw)) * 1.02 * fitZoom;
    if (snap) { camGoal.autoRot = cam3.autoRot; cam3 = camGoal; }
    camSetup();
}
// готовый ракурс: поворот по кратчайшему пути
static void setView(double yaw, double pitch) {
    while (yaw - camGoal.yaw > PI) yaw -= 2 * PI;
    while (yaw - camGoal.yaw < -PI) yaw += 2 * PI;
    camGoal.yaw = yaw; camGoal.pitch = pitch;
}
// поворот камеры: в режиме орбиты — вокруг цели, в режиме полёта — вокруг глаза (цель смещается)
static void camRotate(double dyaw, double dpitch) {
    double off0[3]; camOffset(camGoal, off0);
    double eye[3] = {camGoal.tx + off0[0], camGoal.ty + off0[1], camGoal.tz + off0[2]};
    camGoal.yaw += dyaw; camGoal.pitch = clampv(camGoal.pitch + dpitch, -1.5, 1.5);
    if (camMode == 1) { double off[3]; camOffset(camGoal, off); camGoal.tx = eye[0] - off[0]; camGoal.ty = eye[1] - off[1]; camGoal.tz = eye[2] - off[2]; }
}
static void camTranslate(double dx, double dy, double dz) { camGoal.tx += dx; camGoal.ty += dy; camGoal.tz += dz; }
// шаг камеры за кадр: автоповорот, полёт, слежение, сглаживание
static void camUpdate(double dt, bool focused) {
    if (cam3.autoRot) camGoal.yaw += opt.spin * dt;
    if (camMode == 1 && focused) {   // полёт: W/S — вперёд/назад, A/D — влево/вправо, Q/E — вниз/вверх, Shift — быстрее
        double L = std::max({S.Lx, S.Ly, S.Lz}), v = 0.45 * L * dt * (isDown(VK_SHIFT) ? 3.0 : 1.0);
        double off[3]; camOffset(camGoal, off); double f[3] = {-off[0] / camGoal.dist, -off[1] / camGoal.dist, -off[2] / camGoal.dist};
        double r[3] = {-f[2], 0, f[0]}; double rl = std::sqrt(r[0] * r[0] + r[2] * r[2]); if (rl > 1e-9) { r[0] /= rl; r[2] /= rl; }
        double m[3] = {0, 0, 0};
        auto add = [&](const double* a, double s) { for (int k = 0; k < 3; k++) m[k] += a[k] * s; };
        if (isDown('W')) add(f, v); if (isDown('S')) add(f, -v);
        if (isDown('D')) add(r, v); if (isDown('A')) add(r, -v);
        if (isDown('E')) m[1] += v; if (isDown('Q')) m[1] -= v;
        camTranslate(m[0], m[1], m[2]);
    }
    if (followAtom >= 0 && followAtom < S.n) {
        // при переходе атома через периодическую границу камера переносится вместе с ним, без «пролёта» через ящик
        double p[3] = {S.x[followAtom], S.y[followAtom], S.z[followAtom]}, L[3] = {S.Lx, S.Ly, S.Lz}, g[3] = {camGoal.tx, camGoal.ty, camGoal.tz};
        for (int k = 0; k < 3; k++) {
            double d = p[k] - g[k];
            if (isPer() && std::fabs(d) > L[k] / 2) { double sh = L[k] * std::nearbyint(d / L[k]); if (k == 0) cam3.tx += sh; else if (k == 1) cam3.ty += sh; else cam3.tz += sh; }
        }
        camGoal.tx = p[0]; camGoal.ty = p[1]; camGoal.tz = p[2];
    }
    double k = opt.camLag > 0.005 ? 1 - std::exp(-dt / opt.camLag) : 1.0;
    cam3.yaw += (camGoal.yaw - cam3.yaw) * k; cam3.pitch += (camGoal.pitch - cam3.pitch) * k;
    cam3.tx += (camGoal.tx - cam3.tx) * k; cam3.ty += (camGoal.ty - cam3.ty) * k; cam3.tz += (camGoal.tz - cam3.tz) * k;
    cam3.dist *= std::exp(std::log(camGoal.dist / cam3.dist) * k);   // масштаб — в логарифме (равномерный наезд)
}
static bool inScene(int x, int y) { return x >= sceneX && x < sceneX + sceneW && y >= sceneY && y < sceneY + sceneH; }
// пикселей на σ в плоскости цели камеры (для линейки масштаба и кистей)
static inline double pxPerSigma() { return focal / cam3.dist; }

// ===================================== СОСТОЯНИЕ ИНСТРУМЕНТОВ (для отрисовки) =================
// Инструменты левой кнопки мыши (lmbTool). Порядок значений сохраняется в файлах — новые только в конец.
enum { TOOL_ADD, TOOL_ERASE, TOOL_HEAT, TOOL_COOL, TOOL_CAMERA, TOOL_SELECT, TOOL_PUSH, TOOL_SHOCK, TOOL_CUT, TOOL_MEASURE, TOOL_FIELD, TOOL_N };
static std::vector<int> selList;                  // выделенные атомы
static std::vector<unsigned char> selMask;        // та же информация маской (размер S.n)
static bool rubberOn = false; static float rubX0 = 0, rubY0 = 0, rubX1 = 0, rubY1 = 0;   // рамка выделения (экран)
static int measIdx[4] = {-1, -1, -1, -1}, measN = 0;   // линейка/угломер: выбранные атомы
static int foKind = FO_ATTRACT;                   // вид объекта поля для инструмента «объекты поля»
static int foHover = -1;                          // объект поля под курсором
static bool foPlacing = false; static double foP0[3] = {0, 0, 0}, foP1[3] = {0, 0, 0};   // установка с перетаскиванием
static int cutHoverA = -1, cutHoverB = -1;        // связь под «ножницами»
// слои отображения
static bool layerCharges = false, layerVel = false, layerForce = false, layerFO = true, layerGrid = false, layerLegend = true, layerScale = true, layerPins = true;
static double toolPower = 1.0;                    // сила толчка / удара / броска
static bool throwDrag = false; static float throwX0 = 0, throwY0 = 0;   // «бросок» выделения (Alt+перетаскивание)
static inline bool isSel(int i) { return i >= 0 && i < (int)selMask.size() && selMask[i]; }
static inline bool isPinned(int i) { return i >= 0 && i < (int)S.pin.size() && S.pin[i]; }

// ===================================== RENDER: сцена ====================================
// следы траекторий
static const int TRAIL = 48;
static std::vector<float> trailX, trailY, trailZ; static int trailHead = 0, trailCount = 0, trailN = -1; static std::vector<int> trailIdx;
static void trailsRecord() {
    std::vector<int> idx;
    for (int i = 0; i < S.n; i++) if (S.ty[i] == E_BIG) idx.push_back(i);
    if (idx.empty()) for (int i = 0; i < S.n; i++) if (!EL[S.ty[i]].fixed) idx.push_back(i);
    if (idx.size() > 3000) idx.resize(3000);
    if (trailN != S.n || idx != trailIdx) {
        trailIdx = idx; trailN = S.n; size_t sz = TRAIL * idx.size();
        trailX.assign(sz, 0); trailY.assign(sz, 0); trailZ.assign(sz, 0); trailHead = 0; trailCount = 0;
    }
    size_t m = trailIdx.size();
    for (size_t k = 0; k < m; k++) { int i = trailIdx[k]; trailX[trailHead * m + k] = (float)S.x[i]; trailY[trailHead * m + k] = (float)S.y[i]; trailZ[trailHead * m + k] = (float)S.z[i]; }
    trailHead = (trailHead + 1) % TRAIL; trailCount = std::min(trailCount + 1, TRAIL);
}

static std::vector<float> vb;   // x y u v r g b a
static inline void quadUV(float x, float y, float R, float r, float g, float b, float a, float u0 = 0, float u1 = 1) {
    monoFix(r, g, b, "quadUV (цвет вершин)");
    float d[4][4] = {{x - R, y - R, u0, 0}, {x + R, y - R, u1, 0}, {x + R, y + R, u1, 1}, {x - R, y + R, u0, 1}};
    for (auto& v : d) { vb.push_back(v[0]); vb.push_back(v[1]); vb.push_back(v[2]); vb.push_back(v[3]); vb.push_back(r); vb.push_back(g); vb.push_back(b); vb.push_back(a); }
}
// шар атома: тело, блик и ровный диск — трети атласа texCore (яркость предумножена на покрытие).
// Блик и диск с альфой 0 складываются с тем, что под ними, тело — закрывает.
static inline void quadAtom(float x, float y, float R, float r, float g, float b) { quadUV(x, y, R, r, g, b, 1.0f, 0.0f, 1.0f / 3); }
static inline void quadSpec(float x, float y, float R, float k) { quadUV(x, y, R, k, k, k, 0.0f, 1.0f / 3, 2.0f / 3); }
static inline void quadDisc(float x, float y, float R, float r, float g, float b, float a) { quadUV(x, y, R, r, g, b, a, 2.0f / 3, 1.0f); }
// Цилиндр (часть связи) от (x1,y1) до (x2,y2), полутолщина w1 и w2 на концах (перспектива).
// Поперёк цилиндра текстура идёт по диаметру шара в направлении нормали к связи: нормали боковой поверхности
// цилиндра те же, что у шара на этом диаметре, поэтому свет и блик ложатся так же, как на шарах.
static void cylQuad(float x1, float y1, float w1, float x2, float y2, float w2, float r, float g, float b, float spec) {
    float dx = x2 - x1, dy = y2 - y1, l = std::sqrt(dx * dx + dy * dy); if (l < 0.5f) return;
    monoFix(r, g, b, "cylQuad (цвет вершин)");
    const float nx = -dy / l, ny = dx / l, du = nx * 0.46f / 3, dv = ny * 0.46f, cu = 1.0f / 6, cv = 0.5f;
    auto put = [](float px, float py, float u, float v, float cr, float cg, float cb, float ca) {
        vb.push_back(px); vb.push_back(py); vb.push_back(u); vb.push_back(v); vb.push_back(cr); vb.push_back(cg); vb.push_back(cb); vb.push_back(ca);
    };
    auto quad = [&](float u0, float cr, float cg, float cb, float ca) {
        put(x1 + nx * w1, y1 + ny * w1, u0 + du, cv + dv, cr, cg, cb, ca); put(x2 + nx * w2, y2 + ny * w2, u0 + du, cv + dv, cr, cg, cb, ca);
        put(x2 - nx * w2, y2 - ny * w2, u0 - du, cv - dv, cr, cg, cb, ca); put(x1 - nx * w1, y1 - ny * w1, u0 - du, cv - dv, cr, cg, cb, ca);
    };
    quad(cu, r, g, b, 1.0f);
    if (spec > 0) quad(cu + 1.0f / 3, spec, spec, spec, 0.0f);
}
static void flushQuads(GLuint tex) {
    if (vb.empty()) return;
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, tex);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, 32, vb.data()); glTexCoordPointer(2, GL_FLOAT, 32, vb.data() + 2); glColorPointer(4, GL_FLOAT, 32, vb.data() + 4);
    glDrawArrays(GL_QUADS, 0, (GLsizei)(vb.size() / 8));
    glDisableClientState(GL_VERTEX_ARRAY); glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisableClientState(GL_COLOR_ARRAY);
    glDisable(GL_TEXTURE_2D); vb.clear();
}
static const char* COLOR_NAMES[] = {"элемент", "скорость", "энергия", "порядок", "координация", "фаза"};
static const int COLOR_N = 6;
// цвета фаз: газ, жидкость, твёрдое, стенка/закреплён
static const RGBA PHASE_C[4] = {hexc(0x8FA3B8), hexc(0x4F9BE0), hexc(0xE3A53C), hexc(0x5A606A)};
static void velMap(float u, float& r, float& g, float& b) {   // скорость: тёмно-синий → белый → янтарный
    if (u < 0.5f) { float t = u * 2; r = 0.20f + 0.72f * t; g = 0.38f + 0.55f * t; b = 0.85f + 0.08f * t; }
    else { float t = (u - 0.5f) * 2; r = 0.92f + 0.05f * t; g = 0.93f - 0.33f * t; b = 0.93f - 0.72f * t; }
}
static void atomColor(int i, float& r, float& g, float& b, double emin, double emax) {
    const Element& e = EL[S.ty[i]];
    r = e.r; g = e.g; b = e.b;
    if (e.fixed) return;
    if (colorMode == 1) {   // по скорости
        double vT = std::sqrt(3 * std::max(EN.T, 0.02) / e.m), v = std::sqrt(S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]);
        velMap((float)clampv(v / vT / 2.0, 0.0, 1.0), r, g, b);
    } else if (colorMode == 2) {   // по потенциальной энергии
        float u = (float)clampv((S.ep[i] - emin) / std::max(1e-6, emax - emin), 0.0, 1.0);
        hsv(0.66f - 0.66f * u, 0.62f, 0.92f, r, g, b);
    } else if (colorMode == 3 && i < (int)A::stype.size()) {   // тип локальной структуры (как в OVITO): ГЦК зелёный, ГПУ красный, ОЦК синий, ПК жёлтый
        switch (A::stype[i]) {
        case ST_FCC: r = 0.40f; g = 0.82f; b = 0.45f; break;
        case ST_HCP: r = 0.92f; g = 0.40f; b = 0.34f; break;
        case ST_BCC: r = 0.38f; g = 0.56f; b = 0.95f; break;
        case ST_SC: r = 0.92f; g = 0.80f; b = 0.34f; break;
        case ST_ICE: r = 0.65f; g = 0.88f; b = 0.98f; break;
        default: r = g = b = 0.35f + 0.45f * std::min(1.0f, A::ordMag[i] * 2.0f); b += 0.04f;
        }
    } else if (colorMode == 4 && i < (int)A::coord.size()) {   // координационное число: норма — 12
        int c = A::coord[i], n0 = 12;
        if (c == n0) { r = 0.55f; g = 0.72f; b = 0.86f; } else if (c == n0 - 1) { r = 0.92f; g = 0.40f; b = 0.34f; }
        else if (c == n0 + 1) { r = 0.35f; g = 0.52f; b = 0.95f; } else if (c < n0 - 1 && c > n0 / 2) { r = 0.92f; g = 0.76f; b = 0.34f; }
        else if (c <= n0 / 2) { r = 0.42f; g = 0.44f; b = 0.48f; } else { r = 0.76f; g = 0.45f; b = 0.92f; }
    } else if (colorMode == 5 && atomPhase.size() == (size_t)S.n) {   // агрегатное состояние
        const RGBA& c = PHASE_C[std::min(3, (int)atomPhase[i])]; r = c.r; g = c.g; b = c.b;
    }
}
// ---- стиль модели. Шаростержневая — как привычные изображения молекул: небольшие глянцевые шары цвета CPK
//  (≈0.3 ван-дер-ваальсова радиуса) и связи-цилиндры; ван-дер-ваальсова — плотные шары; палочки — только связи.
//  «Авто»: где возможна химия — шаростержневая, вещество без ковалентных связей (инертный газ, металл) — плотные шары.
enum { MS_AUTO, MS_BALL, MS_VDW, MS_STICK, MS_N };
static const char* MS_NAMES[MS_N] = {"авто", "шаростержневая", "ван-дер-ваальсова", "палочки"};
static inline int modelStyle() { const int s = clampv(opt.style, 0, MS_N - 1); return s != MS_AUTO ? s : (anyBondable ? MS_BALL : MS_VDW); }
static inline double bondRadius(int style) { return style == MS_STICK ? 0.044 : 0.029; }   // σ: 0.15 и 0.10 Å
// радиус шара атома i на экране (σ)
static double atomDrawR(int i, int style) {
    const int t = S.ty[i]; const Element& e = EL[t];
    if (e.fixed) return 0.5 * visSig(t);
    if (style == MS_VDW) return 0.42 * visSig(t);
    if (e.metal) return 0.40 * visSig(t);
    if (style == MS_STICK) return S.nbc[i] ? bondRadius(style) : 0.12 * visSig(t);
    if (S.nbc[i] == 0 && std::fabs(S.q[i]) > 0.4) return 0.28 * visSig(t);   // одноатомный ион — крупнее атома в молекуле
    return 0.145 * visSig(t);
}
// кэш проекций атомов (для отрисовки и выбора мышью)
static std::vector<float> psx, psy, pdep, pscl; static std::vector<char> pvis;
static void projectAll() {
    int n = S.n; psx.resize(n); psy.resize(n); pdep.resize(n); pscl.resize(n); pvis.resize(n);
    const bool cut = sliceOn; const float cd = (float)sliceDepth();   // разрез: ближе плоскости — не видно
#pragma omp parallel for
    for (int i = 0; i < n; i++) pvis[i] = project(S.x[i], S.y[i], S.z[i], psx[i], psy[i], pdep[i], pscl[i]) && !(cut && pdep[i] < cd) ? 1 : 0;
}
static void line3(double x1, double y1, double z1, double x2, double y2, double z2) {
    float a, b, c, d, e, f, g, h;
    if (!project(x1, y1, z1, a, b, e, g) || !project(x2, y2, z2, c, d, f, h)) return;
    glVertex2f(a, b); glVertex2f(c, d);
}
static void boxEdges(double y1) {   // 12 рёбер ящика (верх на высоте y1)
    double X = S.Lx, Z = S.Lz;
    double c[8][3] = {{0, 0, 0}, {X, 0, 0}, {X, 0, Z}, {0, 0, Z}, {0, y1, 0}, {X, y1, 0}, {X, y1, Z}, {0, y1, Z}};
    int e[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    glBegin(GL_LINES); for (auto& k : e) line3(c[k[0]][0], c[k[0]][1], c[k[0]][2], c[k[1]][0], c[k[1]][1], c[k[1]][2]); glEnd();
}
// «красивый» шаг сетки/линейки: 1, 2, 5 × 10^k
static double niceStep(double raw) {
    double p = std::pow(10.0, std::floor(std::log10(std::max(raw, 1e-9)))), m = raw / p;
    return (m <= 1 ? 1 : m <= 2 ? 2 : m <= 5 ? 5 : 10) * p;
}
struct DrawItem { float depth; int a, b; };   // b < 0 — атом, иначе связь a–b
static int hoverAtom();
static void drawLegendAndScale(double emin, double emax);

// ---- объекты поля на сцене: зона (под атомами) и значок/ручки (поверх атомов)
static RGBA foColor(const FieldObj& o, int idx) {
    RGBA c = o.kind == FO_HEATER ? C_HOT : o.kind == FO_COOLER ? C_COLD : hexc(0xD6D6D6);   // нагреватель — белый, охладитель — серый
    if (idx == selFieldObj) c = C_ACC;
    if (!o.on) c.a *= 0.45f;
    return c;
}
// контур барьера — «стадион»: все точки пластины на расстоянии ≤ R от отрезка
static void barrierOutline(const FieldObj& o, std::vector<std::array<float, 2>>& pts) {
    pts.clear();
    double n[3] = {o.dx, o.dy, o.dz}, nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]); if (nl < 1e-9) { n[0] = 1; n[1] = n[2] = 0; nl = 1; }
    for (double& v : n) v /= nl;
    double u[3] = {o.x2 - o.x, o.y2 - o.y, o.z2 - o.z}, ul = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    double un = u[0] * n[0] + u[1] * n[1] + u[2] * n[2]; for (int k = 0; k < 3; k++) u[k] -= un * n[k];
    ul = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    double len = ul;
    if (ul < 1e-6) { double t[3] = {std::fabs(n[0]) < 0.9 ? 1.0 : 0.0, std::fabs(n[0]) < 0.9 ? 0.0 : 1.0, 0}; double tn = t[0] * n[0] + t[1] * n[1];
                     for (int k = 0; k < 3; k++) u[k] = t[k] - tn * n[k]; ul = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]); len = 0; }
    for (double& v : u) v /= ul;
    double w[3] = {n[1] * u[2] - n[2] * u[1], n[2] * u[0] - n[0] * u[2], n[0] * u[1] - n[1] * u[0]};
    double R = std::min(o.R, 2.0 * std::max({S.Lx, S.Ly, S.Lz}));
    double p0[3] = {o.x, o.y, o.z};
    auto emit = [&](double s, double a) {   // точка контура: вдоль отрезка s, угол a вокруг конца
        double c = std::cos(a), sn = std::sin(a), q[3];
        for (int k = 0; k < 3; k++) q[k] = p0[k] + u[k] * s + (u[k] * c + w[k] * sn) * R;
        float x, y, dd, sc; if (project(q[0], q[1], q[2], x, y, dd, sc)) pts.push_back({x, y});
    };
    for (int k = 0; k <= 16; k++) emit(len, -PI / 2 + PI * k / 16);
    for (int k = 0; k <= 16; k++) emit(0, PI / 2 + PI * k / 16);
}
static void drawFieldObjs(bool over) {
    if (!layerFO) return;
    std::vector<std::array<float, 2>> pts;
    for (int k = 0; k < (int)fieldObjs.size(); k++) {
        const FieldObj& o = fieldObjs[k];
        RGBA c = foColor(o, k); const bool hot = k == selFieldObj || k == foHover;
        float cx, cy, dd, sc;
        if (o.kind == FO_BARRIER) {
            barrierOutline(o, pts); if (pts.size() < 2) continue;
            if (!over) {
                { glColor4f(c.r, c.g, c.b, 0.07f * c.a); glBegin(GL_POLYGON); for (auto& p : pts) glVertex2f(p[0], p[1]); glEnd();
                                col(withA(c, 0.55f * c.a)); glEnable(GL_LINE_SMOOTH); glBegin(GL_LINE_LOOP); for (auto& p : pts) glVertex2f(p[0], p[1]); glEnd(); glDisable(GL_LINE_SMOOTH); }
            } else {
                float ax, ay, bx, by;
                if (!project(o.x, o.y, o.z, ax, ay, dd, sc) || !project(o.x2, o.y2, o.z2, bx, by, dd, sc)) continue;
                col(withA(c, hot ? 1.0f : 0.7f * c.a)); glBegin(GL_LINES); segPx(ax, ay, bx, by); glEnd();
                float hs = uiPx(hot ? 4.0f : 3.0f); rectFill(ax - hs, ay - hs, 2 * hs, 2 * hs, withA(c, 0.9f * c.a)); rectFill(bx - hs, by - hs, 2 * hs, 2 * hs, withA(c, 0.9f * c.a));
                if (hot) drawText(fontXS, std::max(ax, bx) + uiPx(8), std::min(ay, by) - uiPx(4), fmt("%s%s", T(FO_NAMES[o.kind]), o.on ? "" : T(" (выкл.)")), withA(c, 1));
            }
            continue;
        }
        if (!project(o.x, o.y, o.z, cx, cy, dd, sc)) continue;
        float R = (float)(o.R * sc);
        if (!over) {   // зона действия: едва заметная заливка + тонкий пунктир
            if (R > 3) { glColor4f(c.r, c.g, c.b, (o.kind == FO_HEATER || o.kind == FO_COOLER ? 0.07f : 0.035f) * c.a); discPx(cx, cy, R, 64);
                         col(withA(c, (hot ? 0.75f : 0.4f) * c.a)); glEnable(GL_LINE_SMOOTH); if (o.on) circlePx(cx, cy, R, 96); else dashedCircle(cx, cy, R); glDisable(GL_LINE_SMOOTH); }
            // направление (ветер / источник / ось вихря)
            if (o.kind == FO_WIND || o.kind == FO_EMITTER || o.kind == FO_VORTEX) {
                float ex, ey, d2, s2; double L = std::max(o.R, 1.5);
                if (project(o.x + o.dx * L, o.y + o.dy * L, o.z + o.dz * L, ex, ey, d2, s2)) {
                    float dx = ex - cx, dy = ey - cy, l = std::sqrt(dx * dx + dy * dy);
                    glEnable(GL_LINE_SMOOTH); col(withA(c, 0.55f * c.a)); glBegin(GL_LINES);
                    if (o.kind == FO_WIND && l > 4) {   // три параллельные стрелки поперёк зоны
                        float nx = -dy / l, ny = dx / l;
                        for (int q = -1; q <= 1; q++) { float ox = nx * R * 0.45f * q, oy = ny * R * 0.45f * q; arrowPx(cx - dx * 0.7f + ox, cy - dy * 0.7f + oy, cx + dx * 0.7f + ox, cy + dy * 0.7f + oy, uiPx(8)); }
                    } else arrowPx(cx, cy, ex, ey, uiPx(8));
                    glEnd(); glDisable(GL_LINE_SMOOTH);
                }
            }
        } else {       // центр: строгий значок, подпись у выбранного/наведённого
            float is = uiPx(16);
            rectFill(cx - is * 0.7f, cy - is * 0.7f, is * 1.4f, is * 1.4f, withA(C_SCENE, 0.55f));
            drawIcon(IC_FO0 + o.kind, cx, cy, is, withA(c, hot ? 1.0f : 0.8f * c.a));
            if (hot) rectLine(cx - is * 0.7f, cy - is * 0.7f, is * 1.4f, is * 1.4f, withA(c, 0.9f));
            if (o.kind == FO_EMITTER) drawText(fontXS, cx + is * 0.8f, cy - is * 0.9f, EL[clampv(o.elem, 0, NEL - 1)].sym, withA(c, 0.9f * c.a));
            if (hot) {
                std::string s = fmt("%s  R %.1fσ  A %.2f %s", T(FO_NAMES[o.kind]), o.R, o.strength, T(FO_UNITS[o.kind]));
                if (foUsesT(o.kind)) s += fmt("  T %.2f", o.Tset);
                if (!o.on) s += T("  (выкл.)");
                float tw = textW(fontXS, s), lx = cx + is;
                if (lx + tw + uiPx(10) > sceneX + sceneW) lx = cx - is - tw - uiPx(8);   // у правого края — подпись слева
                rectFill(lx, cy + is * 0.6f, tw + uiPx(8), fontXS.h + uiPx(2), withA(C_SCENE, 0.7f));
                drawText(fontXS, lx + uiPx(4), cy + is * 0.6f, s, withA(c, 1));
            }
        }
    }
    // предпросмотр установки (ветер/источник — направление, барьер — отрезок)
    if (over && foPlacing) {
        float ax, ay, bx, by, d0, s0;
        if (project(foP0[0], foP0[1], foP0[2], ax, ay, d0, s0) && project(foP1[0], foP1[1], foP1[2], bx, by, d0, s0)) {
            glEnable(GL_LINE_SMOOTH); col(withA(C_ACC, 0.9f)); glLineWidth(foKind == FO_BARRIER ? 2.0f : 1.0f);
            glBegin(GL_LINES); if (foKind == FO_BARRIER) segPx(ax, ay, bx, by); else arrowPx(ax, ay, bx, by, uiPx(9)); glEnd();
            glLineWidth(1); glDisable(GL_LINE_SMOOTH);
        }
    }
}
// угол между векторами, градусы
static double angleDeg(const double* a, const double* b) {
    double d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2], la = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]), lb = std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
    if (la < 1e-12 || lb < 1e-12) return 0;
    return std::acos(clampv(d / (la * lb), -1.0, 1.0)) * 180 / PI;
}
// результаты линейки/угломера (для сцены и инспектора): расстояние, угол, двугранный угол
static bool measResult(double& d12, double& ang, double& dih) {
    d12 = ang = dih = -1;
    if (measN < 2) return false;
    for (int k = 0; k < measN; k++) if (measIdx[k] < 0 || measIdx[k] >= S.n) return false;
    double v[3][3];   // векторы между последовательными атомами (минимальный образ)
    for (int k = 0; k + 1 < measN; k++) dvec(measIdx[k], measIdx[k + 1], v[k][0], v[k][1], v[k][2]);
    d12 = std::sqrt(v[0][0] * v[0][0] + v[0][1] * v[0][1] + v[0][2] * v[0][2]);
    if (measN >= 3) { double a[3] = {-v[0][0], -v[0][1], -v[0][2]}; ang = angleDeg(a, v[1]); }
    if (measN >= 4) {   // двугранный угол 1-2-3-4
        const double *b1 = v[0], *b2 = v[1], *b3 = v[2];
        double n1[3] = {b1[1] * b2[2] - b1[2] * b2[1], b1[2] * b2[0] - b1[0] * b2[2], b1[0] * b2[1] - b1[1] * b2[0]};
        double n2[3] = {b2[1] * b3[2] - b2[2] * b3[1], b2[2] * b3[0] - b2[0] * b3[2], b2[0] * b3[1] - b2[1] * b3[0]};
        double l2 = std::sqrt(b2[0] * b2[0] + b2[1] * b2[1] + b2[2] * b2[2]);
        double m1[3] = {n1[1] * b2[2] / l2 - n1[2] * b2[1] / l2, n1[2] * b2[0] / l2 - n1[0] * b2[2] / l2, n1[0] * b2[1] / l2 - n1[1] * b2[0] / l2};
        double x = n1[0] * n2[0] + n1[1] * n2[1] + n1[2] * n2[2], y = m1[0] * n2[0] + m1[1] * n2[1] + m1[2] * n2[2];
        dih = -std::atan2(y, x) * 180 / PI;
    }
    return true;
}
static void drawMeasure() {
    if (measN <= 0) return;
    for (int k = 0; k < measN; k++) if (measIdx[k] < 0 || measIdx[k] >= S.n) return;
    float px[4], py[4];
    // атомы цепочки «разворачиваем» по минимальному образу от первого
    double cx = S.x[measIdx[0]], cy = S.y[measIdx[0]], cz = S.z[measIdx[0]];
    for (int k = 0; k < measN; k++) {
        if (k > 0) { double dx, dy, dz; dvec(measIdx[k - 1], measIdx[k], dx, dy, dz); cx += dx; cy += dy; cz += dz; }
        float dd, s; if (!project(cx, cy, cz, px[k], py[k], dd, s)) return;
    }
    glEnable(GL_LINE_SMOOTH); glEnable(GL_LINE_STIPPLE); glLineStipple(1, 0x0F0F); col(withA(C_MEAS, 0.85f));
    glBegin(GL_LINES); for (int k = 0; k + 1 < measN; k++) segPx(px[k], py[k], px[k + 1], py[k + 1]); glEnd();
    glDisable(GL_LINE_STIPPLE);
    for (int k = 0; k < measN; k++) { col(withA(C_MEAS, 0.95f)); circlePx(px[k], py[k], uiPx(7), 24);
        drawText(fontXS, px[k] + uiPx(8), py[k] - uiPx(18), std::to_string(k + 1), C_MEAS); }
    glDisable(GL_LINE_SMOOTH);
    // результаты — одной табличкой справа от группы атомов (не перекрывает атомы и не наезжает сама на себя)
    double d12, ang, dih; measResult(d12, ang, dih);
    std::vector<std::string> L;
    auto dist = [&](int a, int b) { double dx, dy, dz; dvec(measIdx[a], measIdx[b], dx, dy, dz); return std::sqrt(dx * dx + dy * dy + dz * dz); };
    for (int k = 0; k + 1 < measN; k++) L.push_back(fmt("%d–%d  %.3f Å", k + 1, k + 2, dist(k, k + 1) * 3.405));
    if (measN >= 3) L.push_back(fmt("угол  %.1f°", ang));
    if (measN >= 4) L.push_back(fmt("двугр.  %.1f°", dih));
    if (L.empty()) return;
    float bx = px[0], by = py[0];
    for (int k = 0; k < measN; k++) { bx = std::max(bx, px[k]); by = std::min(by, py[k]); }
    float w = 0; for (auto& s : L) w = std::max(w, textW(fontS, s));
    w += uiPx(14); float lh = fontS.h + uiPx(1), h = L.size() * lh + uiPx(8);
    bx = std::min(bx + uiPx(18), sceneX + sceneW - w - uiPx(6)); by = clampv(by - uiPx(6), sceneY + uiPx(44), sceneY + sceneH - h - uiPx(6));
    boxPanel(bx, by, w, h, withA(C_PANEL, 0.92f), withA(C_MEAS, 0.45f));
    for (size_t k = 0; k < L.size(); k++) drawText(fontS, bx + uiPx(7), by + uiPx(4) + k * lh, L[k], C_MEAS);
}
static void drawScene() {
    glEnable(GL_SCISSOR_TEST); glScissor((int)sceneX, (int)(winH - sceneY - sceneH), (int)sceneW, (int)sceneH);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    static const RGBA BG[3] = {C_SCENE, hexc(0x0E0E0E), hexc(0x1A1A1A)};
    rectFill(sceneX, sceneY, sceneW, sceneH, BG[clampv(opt.sceneBg, 0, 2)]);
    projectAll();
    // --- сетка (слой) на «полу» (y = 0); шаг — «красивое» число нанометров
    if (layerGrid) {
        double pps = pxPerSigma(), stepNm = niceStep(realNm(uiPx(46) / std::max(1e-6, pps))), st = stepNm / cfg::U_L_NM;
        glEnable(GL_LINE_SMOOTH); glBegin(GL_LINES);
        int nx = (int)(S.Lx / st), nz = (int)(S.Lz / st);
        if (nx < 400 && nz < 400) {
            for (int k = 0; k <= nx; k++) { double x = k * st; glColor4f(1, 1, 1, k % 5 == 0 ? 0.11f : 0.05f); line3(x, 0, 0, x, 0, S.Lz); }
            for (int k = 0; k <= nz; k++) { double z = k * st; glColor4f(1, 1, 1, k % 5 == 0 ? 0.11f : 0.05f); line3(0, 0, z, S.Lx, 0, z); }
        }
        glEnd(); glDisable(GL_LINE_SMOOTH);
    }
    // --- ящик: тонкий серый контур; периодические границы — пунктир
    glLineWidth(1.0f);
    if (opt.box) {
        if (isPer()) { glEnable(GL_LINE_STIPPLE); glLineStipple(1, 0x3333); }
        col(isPer() ? withA(C_BOX, 0.4f) : C_BOX);
        glEnable(GL_LINE_SMOOTH); boxEdges(S.Ly); glDisable(GL_LINE_SMOOTH);
        glDisable(GL_LINE_STIPPLE);
    }
    // тепловые стенки: горячая — белая сплошная толстая линия, холодная — серый пунктир
    if (P.heatWalls && !isPer()) {
        double Z = S.Lz;
        glLineWidth(2.5f); col(withA(C_HOT, 0.9f)); glBegin(GL_LINES);
        if (P.heatWalls == 1) { line3(0, 0, 0, 0, S.Ly, 0); line3(0, 0, Z, 0, S.Ly, Z); line3(0, 0, 0, 0, 0, Z); line3(0, S.Ly, 0, 0, S.Ly, Z); }
        else { line3(0, 0, 0, S.Lx, 0, 0); line3(0, 0, Z, S.Lx, 0, Z); line3(0, 0, 0, 0, 0, Z); line3(S.Lx, 0, 0, S.Lx, 0, Z); }
        glEnd();
        if (P.heatWalls == 1) {
            glLineWidth(2.0f); lineStyle(LS_DASH); col(withA(C_COLD, 0.9f)); glBegin(GL_LINES);
            line3(S.Lx, 0, 0, S.Lx, S.Ly, 0); line3(S.Lx, 0, Z, S.Lx, S.Ly, Z); line3(S.Lx, 0, 0, S.Lx, 0, Z); line3(S.Lx, S.Ly, 0, S.Lx, S.Ly, Z);
            glEnd(); lineStyle(LS_SOLID);
        }
        glLineWidth(1.0f);
    }
    // поршень
    if (P.boundary == B_PISTON) {
        float p[4][2]; float dd, s; double Z = S.Lz;
        double c[4][3] = {{0, S.Ly, 0}, {S.Lx, S.Ly, 0}, {S.Lx, S.Ly, Z}, {0, S.Ly, Z}};
        bool ok = true; for (int k = 0; k < 4; k++) ok &= project(c[k][0], c[k][1], c[k][2], p[k][0], p[k][1], dd, s);
        if (ok) { col(grayc(0.84f, 0.1f)); glBegin(GL_QUADS); for (auto& q : p) glVertex2f(q[0], q[1]); glEnd();
                  col(grayc(0.84f, 0.75f)); glBegin(GL_LINE_LOOP); for (auto& q : p) glVertex2f(q[0], q[1]); glEnd(); }
    }
    // катализатор: пунктирная окружность
    if (P.catalyst) {
        float cx, cy, dd, s;
        if (project(P.catX, P.catY, P.catZ, cx, cy, dd, s)) {
            float R = (float)(P.catR * s);
            col(grayc(0.8f, 0.05f)); discPx(cx, cy, R, 48);
            col(grayc(0.8f, 0.6f)); glEnable(GL_LINE_SMOOTH); dashedCircle(cx, cy, R); circlePx(cx, cy, R - uiPx(3), 96); glDisable(GL_LINE_SMOOTH);
            drawText(fontXS, cx + R * 0.72f, cy - R * 0.72f - fontXS.h, "катализатор", grayc(0.8f, 0.85f));
        }
    }
    // электрическое поле: полупрозрачные стрелки вдоль x через среднюю плоскость ящика
    if (P.efield != 0) {
        float a = (float)clampv(0.1 + 0.08 * std::fabs(P.efield), 0.12, 0.3), sgn = P.efield > 0 ? 1.0f : -1.0f;
        col(grayc(0.88f, a)); glEnable(GL_LINE_SMOOTH);
        const int rows = 5, colsA = 6; double zm = S.Lz / 2;
        glBegin(GL_LINES);
        for (int r = 1; r <= rows; r++) for (int c = 0; c < colsA; c++) {
            double y = S.Ly * r / (rows + 1), x1 = S.Lx * (c + 0.2) / colsA, x2 = S.Lx * (c + 0.8) / colsA;
            if (sgn < 0) std::swap(x1, x2);
            float ax, ay, bx, by, dd, s1, s2;
            if (!project(x1, y, zm, ax, ay, dd, s1) || !project(x2, y, zm, bx, by, dd, s2)) continue;
            arrowPx(ax, ay, bx, by, uiPx(8));
        }
        glEnd(); glDisable(GL_LINE_SMOOTH);
    }
    // зоны объектов поля (под атомами)
    drawFieldObjs(false);
    // --- цвета атомов, диапазон глубин для глубинного затемнения
    double emin = 1e30, emax = -1e30;
    if (colorMode == 2) for (int i = 0; i < S.n; i++) if (!EL[S.ty[i]].fixed) { emin = std::min(emin, S.ep[i]); emax = std::max(emax, S.ep[i]); }
    int n = S.n;
    std::vector<float> cr(n), cg(n), cb(n);
    float dmin = 1e30f, dmax = -1e30f;
    for (int i = 0; i < n; i++) { atomColor(i, cr[i], cg[i], cb[i], emin, emax); if (pvis[i]) { dmin = std::min(dmin, pdep[i]); dmax = std::max(dmax, pdep[i]); } }
    // глубинное затемнение к чёрному (depth cue): ближние атомы яркие, дальние — тусклее
    // (глубина нормируется не меньше чем на диагональ ящика: одиночная молекула не темнеет с одного бока)
    const float fogK = (float)opt.fog, fogL = std::max(dmax - dmin, 0.7f * (float)std::sqrt(S.Lx * S.Lx + S.Ly * S.Ly + S.Lz * S.Lz));
    auto fog = [&](float dep) { if (fogL <= 0) return 1.0f; float t = clampv((dep - dmin) / fogL, 0.0f, 1.0f); return 1.0f - fogK * std::pow(t, 0.85f); };
    // --- следы
    if (trailsOn && trailCount > 1 && n > 0 && trailN == n) {
        size_t m = trailIdx.size(); glLineWidth(1.0f);
        MonoAtoms atomsColored;   // следы — цветом атома
        glBegin(GL_LINES);
        for (size_t k = 0; k < m; k++) {
            int ai = trailIdx[k]; if (ai >= n) continue;
            for (int s = 1; s < trailCount; s++) {
                int a = (trailHead - s + TRAIL) % TRAIL, b = (trailHead - s - 1 + TRAIL) % TRAIL;
                float x1 = trailX[a * m + k], y1 = trailY[a * m + k], z1 = trailZ[a * m + k], x2 = trailX[b * m + k], y2 = trailY[b * m + k], z2 = trailZ[b * m + k];
                if (std::fabs(x1 - x2) > S.Lx / 2 || std::fabs(y1 - y2) > S.Ly / 2 || std::fabs(z1 - z2) > S.Lz / 2) continue;
                glColor4f(cr[ai], cg[ai], cb[ai], 0.4f * (1.0f - (float)s / trailCount));
                line3(x1, y1, z1, x2, y2, z2);
            }
        }
        glEnd();
    }
    // --- вспышки реакций (аддитивно, приглушённо)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (auto& fl : flashes) {
        float sx, sy, dd, s; if (!project(fl.x, fl.y, fl.z, sx, sy, dd, s)) continue;
        float t = fl.age / 0.6f, R = (float)(0.4 + 2.2 * t) * fl.str * s;
        quadUV(sx, sy, R, fl.r, fl.g, fl.b, (1 - t) * 0.32f * std::min(1.0f, fl.str));
    }
    flushQuads(texGlow);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // поглощение энергии (эндотермическое событие): вместо яркого пятна — сжимающееся серое кольцо
    glEnable(GL_LINE_SMOOTH);
    for (auto& fl : flashes) {
        if (fl.r > 0.9f) continue;
        float sx, sy, dd, s; if (!project(fl.x, fl.y, fl.z, sx, sy, dd, s)) continue;
        float t = fl.age / 0.6f, R = (float)(2.6 - 2.2 * t) * fl.str * s;
        if (R > 2) { col(withA(C_COLD, (1 - t) * 0.5f)); circlePx(sx, sy, R, 32); }
    }
    glDisable(GL_LINE_SMOOTH);
    // --- атомы и связи от дальних к ближним (алгоритм художника). Связь — два цилиндра цветом своих атомов
    //     (как у шаростержневой модели), каждый рисуется прямо перед своим шаром: шар закрывает начало цилиндра,
    //     и связь выходит из его поверхности. Кратная связь — две или три тонкие параллельные трубки.
    const int style = modelStyle();
    const float coreK = (float)atomVis;   // множитель радиусов (слайдер «размер атомов»)
    std::vector<DrawItem> items; items.reserve(n * 3);
    for (int i = 0; i < n; i++) if (pvis[i]) items.push_back({pdep[i], i, -1});
    if (bondsOn && style != MS_VDW)
        for (int i = 0; i < n; i++) if (pvis[i]) for (int k = 0; k < S.nbc[i]; k++) { int j = S.nb[i][k]; if (pvis[j]) items.push_back({pdep[i] + 1e-3f, i, j}); }
    std::sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) { return a.depth > b.depth; });
    vb.reserve((size_t)items.size() * 72);
    const float rb0 = (float)(bondRadius(style) * opt.bondW);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);   // предумноженная альфа: тела, блики и связи — одним пакетом
    for (auto& it : items) {
        const int i = it.a; const Element& e = EL[S.ty[i]];
        float f = fog(pdep[i]); if (e.fixed) f *= 0.8f;
        if (it.b < 0) {
            const float R = (float)(atomDrawR(i, style) * coreK) * pscl[i];
            { MonoAtoms atomsColored; quadAtom(psx[i], psy[i], R, cr[i] * f, cg[i] * f, cb[i] * f); }
            if (opt.gloss && R > 2.0f && !e.fixed) quadSpec(psx[i], psy[i], R, 0.85f * f);
            continue;
        }
        const int j = it.b; double dx, dy, dz; dvec(i, j, dx, dy, dz);
        float x2, y2, dep2, s2; if (!project(S.x[i] + dx, S.y[i] + dy, S.z[i] + dz, x2, y2, dep2, s2)) continue;
        if (std::fabs(x2 - psx[j]) > 2 || std::fabs(y2 - psy[j]) > 2) continue;   // связь через периодическую границу не рисуем
        // граница цветов — посередине видимой части связи (между поверхностями шаров)
        const double d = std::sqrt(dx * dx + dy * dy + dz * dz); if (d < 1e-6) continue;
        const double Ri = atomDrawR(i, style) * coreK, Rj = atomDrawR(j, style) * coreK, gap = d - Ri - Rj;
        const double s = gap > 0 ? (Ri + 0.5 * gap) / d : Ri / std::max(1e-9, Ri + Rj);
        float xm, ym, dm, sm; if (!project(S.x[i] + dx * s, S.y[i] + dy * s, S.z[i] + dz * s, xm, ym, dm, sm)) continue;
        const int o = style == MS_STICK ? 1 : std::max(1, bondOrder(i, j));
        const float wk = o == 1 ? 1.0f : o == 2 ? 0.66f : 0.55f, sep = rb0 * (o == 2 ? 1.35f : 2.1f);
        const float sx = xm - psx[i], sy = ym - psy[i], sl = std::sqrt(sx * sx + sy * sy); if (sl < 0.5f) continue;
        const float nx = -sy / sl, ny = sx / sl, spec = opt.gloss ? 0.7f * f : 0.0f;
        MonoAtoms atomsColored;
        for (int q = 0; q < o; q++) {
            const float off = (q - 0.5f * (o - 1)) * sep;
            cylQuad(psx[i] + nx * off * pscl[i], psy[i] + ny * off * pscl[i], rb0 * wk * pscl[i], xm + nx * off * sm, ym + ny * off * sm, rb0 * wk * sm,
                    cr[i] * f, cg[i] * f, cb[i] * f, spec);
        }
    }
    flushQuads(texCore);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // --- свечение раскалённых атомов: цвет абсолютно чёрного тела при «температуре» атома (его кинетическая энергия,
    //     сглаженная по времени), яркость растёт как T⁴ — тёмно-красное каление около 1000 K, белое выше 5000 K
    if (opt.glow && n > 0) {
        static std::vector<float> Tsm; if ((int)Tsm.size() != n) Tsm.assign(n, 0.0f);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        MonoAtoms atomsColored;
        for (int i = 0; i < n; i++) {
            const Element& e = EL[S.ty[i]]; if (e.fixed) continue;
            const float Ti = (float)realK(e.m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]) / 3);
            Tsm[i] += (Ti - Tsm[i]) * 0.12f;
            if (Tsm[i] < 900 || !pvis[i]) continue;
            float r, g, b; blackbody(Tsm[i], r, g, b);
            const float u = std::min(1.0f, (Tsm[i] - 900) / 2600), a = 0.75f * u * u * (0.4f + 0.6f * u);
            const float R = (float)std::max(0.5, 2.6 * atomDrawR(i, style) * coreK) * pscl[i];
            quadUV(psx[i], psy[i], R, r, g, b, a * fog(pdep[i]));
        }
        flushQuads(texGlow);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    // --- слои: заряды, скорости, силы, закреплённые атомы, выделение
    glEnable(GL_LINE_SMOOTH);
    if (layerCharges && anyCharge) {
        for (int i = 0; i < n; i++) {
            if (!pvis[i] || std::fabs(S.q[i]) < 0.05) continue;
            float R = (float)(atomDrawR(i, style) * coreK) * pscl[i] + uiPx(2.5f);
            float a = (float)clampv(0.35 + 0.6 * std::fabs(S.q[i]), 0.35, 0.95);
            // «+» — яркое сплошное кольцо, «−» — серое пунктирное (и знак внутри)
            if (S.q[i] > 0) { col(withA(C_HOT, a)); circlePx(psx[i], psy[i], R, 20); } else { col(withA(C_COLD, a)); dashedCircle(psx[i], psy[i], R); }
            if (R > uiPx(7)) { float h = std::min(R * 0.4f, uiPx(4)); glBegin(GL_LINES); segPx(psx[i] - h, psy[i], psx[i] + h, psy[i]); if (S.q[i] > 0) segPx(psx[i], psy[i] - h, psx[i], psy[i] + h); glEnd(); }
        }
    }
    if ((layerVel || layerForce) && n <= 6000) {
        glBegin(GL_LINES);
        for (int i = 0; i < n; i++) {
            if (!pvis[i] || EL[S.ty[i]].fixed) continue;
            if (layerVel) {   // скорость: 0.5σ на единицу скорости, не длиннее 3σ
                double vx = S.vx[i], vy = S.vy[i], vz = S.vz[i], v = std::sqrt(vx * vx + vy * vy + vz * vz), L = std::min(0.5 * v, 3.0);
                if (v > 1e-6 && L * pscl[i] > 2) { float ex, ey, dd, s; if (project(S.x[i] + vx / v * L, S.y[i] + vy / v * L, S.z[i] + vz / v * L, ex, ey, dd, s)) {
                    col(withA(C_ACC, 0.75f)); arrowPx(psx[i], psy[i], ex, ey, uiPx(5)); } }
            }
            if (layerForce) { // сила: длина ∝ log(1 + |F|), не длиннее 3σ
                double fx = S.fx[i], fy = S.fy[i], fz = S.fz[i], F = std::sqrt(fx * fx + fy * fy + fz * fz), L = std::min(0.6 * std::log1p(F / 5.0), 3.0);
                if (F > 1e-6 && L * pscl[i] > 2) { float ex, ey, dd, s; if (project(S.x[i] + fx / F * L, S.y[i] + fy / F * L, S.z[i] + fz / F * L, ex, ey, dd, s)) {
                    col(withA(C_COLD, 0.9f)); arrowPx(psx[i], psy[i], ex, ey, uiPx(5)); } }   // сила — серая, скорость — белая
            }
        }
        glEnd();
    }
    if (layerPins && !S.pin.empty()) {   // закреплённые атомы: перекрестье в квадрате
        col(withA(C_TEXT_HI, 0.85f)); glBegin(GL_LINES);
        for (int i = 0; i < n && i < (int)S.pin.size(); i++) {
            if (!S.pin[i] || !pvis[i]) continue;
            float h = std::max(uiPx(3.5f), (float)(atomDrawR(i, style) * coreK) * pscl[i] * 0.55f);
            segPx(psx[i] - h, psy[i] - h, psx[i] + h, psy[i] - h); segPx(psx[i] + h, psy[i] - h, psx[i] + h, psy[i] + h);
            segPx(psx[i] + h, psy[i] + h, psx[i] - h, psy[i] + h); segPx(psx[i] - h, psy[i] + h, psx[i] - h, psy[i] - h);
            segPx(psx[i] - h * 0.5f, psy[i], psx[i] + h * 0.5f, psy[i]); segPx(psx[i], psy[i] - h * 0.5f, psx[i], psy[i] + h * 0.5f);
        }
        glEnd();
    }
    if (!selList.empty()) {   // выделение: тонкое акцентное кольцо
        col(withA(C_ACC, 0.95f));
        for (int i : selList) {
            if (i < 0 || i >= n || !pvis[i]) continue;
            float R = (float)(atomDrawR(i, style) * coreK) * pscl[i] + uiPx(2);
            circlePx(psx[i], psy[i], std::max(R, uiPx(4)), 20);
        }
    }
    glDisable(GL_LINE_SMOOTH);
    // --- «ножницы»: связь под курсором
    if (cutHoverA >= 0 && cutHoverA < n && cutHoverB >= 0 && cutHoverB < n && pvis[cutHoverA]) {
        double dx, dy, dz; dvec(cutHoverA, cutHoverB, dx, dy, dz); float bx, by, dd, s;
        if (project(S.x[cutHoverA] + dx, S.y[cutHoverA] + dy, S.z[cutHoverA] + dz, bx, by, dd, s)) {
            glEnable(GL_LINE_SMOOTH); glLineWidth(2.5f); col(withA(C_ACC, 0.95f)); glBegin(GL_LINES); segPx(psx[cutHoverA], psy[cutHoverA], bx, by); glEnd(); glLineWidth(1); glDisable(GL_LINE_SMOOTH);
        }
    }
    // значки объектов поля (поверх атомов)
    drawFieldObjs(true);
    // линейка / угломер
    drawMeasure();
    // --- пинцет
    if (grabbed >= 0 && grabbed < n) {
        col(grayc(1, 0.6f)); glEnable(GL_LINE_STIPPLE); glLineStipple(2, 0x0F0F);
        glBegin(GL_LINES); line3(S.x[grabbed], S.y[grabbed], S.z[grabbed], grabX, grabY, grabZ); glEnd(); glDisable(GL_LINE_STIPPLE);
    }
    // --- «бросок» выделения: стрелка от точки захвата к курсору
    if (throwDrag) {
        glEnable(GL_LINE_SMOOTH); col(withA(C_ACC, 0.9f)); glLineWidth(1.5f); glBegin(GL_LINES); arrowPx(throwX0, throwY0, (float)mouseX, (float)mouseY, uiPx(10)); glEnd();
        glLineWidth(1); glDisable(GL_LINE_SMOOTH);
    }
    // --- рамка выделения
    if (rubberOn) {
        float x0 = std::min(rubX0, rubX1), y0 = std::min(rubY0, rubY1), w = std::fabs(rubX1 - rubX0), h = std::fabs(rubY1 - rubY0);
        rectFill(x0, y0, w, h, withA(C_ACC, 0.07f));
        glEnable(GL_LINE_STIPPLE); glLineStipple(1, 0x3333); rectLine(x0, y0, w, h, withA(C_ACC, 0.9f)); glDisable(GL_LINE_STIPPLE);
    }
    // --- кисть инструмента: окружность радиуса R в плоскости цели камеры
    if (inScene(mouseX, mouseY) && !helpOn && !ptOn && !menuOn && !settingsOn) {
        const int tl = lmbTool; const bool brushTool = tl == TOOL_ADD || tl == TOOL_ERASE || tl == TOOL_HEAT || tl == TOOL_COOL || tl == TOOL_PUSH || tl == TOOL_SHOCK;
        if (brushTool || heatBrush) {
            float Rpx = (float)(P.brushR * (tl == TOOL_SHOCK ? 2.5 : 1.0) * pxPerSigma());
            // нагрев — яркая сплошная окружность, охлаждение — серый пунктир, ластик — двойная окружность, толчок/удар — пунктир
            const bool hot = heatBrush > 0 || (!heatBrush && tl == TOOL_HEAT), cold = heatBrush < 0 || (!heatBrush && tl == TOOL_COOL);
            RGBA bc = hot ? C_HOT : cold ? C_COLD : hexc(0xD6D6D6);
            glEnable(GL_LINE_SMOOTH); col(withA(bc, heatBrush ? 0.75f : hot ? 0.55f : 0.4f));
            if (cold || tl == TOOL_SHOCK || tl == TOOL_PUSH) dashedCircle((float)mouseX, (float)mouseY, Rpx); else circlePx((float)mouseX, (float)mouseY, Rpx, 64);
            if (!hot && !cold && tl == TOOL_ERASE && Rpx > uiPx(6)) circlePx((float)mouseX, (float)mouseY, Rpx - uiPx(3), 64);
            glDisable(GL_LINE_SMOOTH);
        }
    }
    // --- подсветка: молекула под курсором и атом, за которым следит камера
    {
        int h = (grabbed < 0 && inScene(mouseX, mouseY) && !helpOn && !ptOn && !menuOn && !settingsOn && lmbTool != TOOL_CAMERA) ? hoverAtom() : -1;
        glEnable(GL_LINE_SMOOTH);
        if (h >= 0) {
            std::vector<int> mol; moleculeOf(h, mol);
            if (mol.size() <= 400) for (int a : mol) {
                if (a >= n || !pvis[a]) continue;
                float R = (float)(atomDrawR(a, style) * coreK) * pscl[a] + uiPx(2);
                col(grayc(1, a == h ? 0.8f : 0.35f)); circlePx(psx[a], psy[a], R, 28);
            }
        }
        if (followAtom >= 0 && followAtom < n && pvis[followAtom]) {
            float pulse = 0.5f + 0.5f * std::sin((float)S.t * 6.0f);
            float R = (float)(visSig(S.ty[followAtom]) * 0.6) * pscl[followAtom] + 5 + 2 * pulse;
            col(C_ACC); circlePx(psx[followAtom], psy[followAtom], R, 40);
        }
        glDisable(GL_LINE_SMOOTH);
    }
    drawLegendAndScale(emin, emax);
    glDisable(GL_SCISSOR_TEST);
}
// ---- легенда цветов (правый нижний угол сцены), линейка масштаба и оси (левый нижний угол)
static void drawLegendAndScale(double emin, double emax) {
    const float pad = uiPx(10), by = sceneY + sceneH - pad;   // нижняя граница подписей
    float lx0 = sceneX + pad;
    {   // оси-триада
        float L = uiPx(22), ox = lx0 + L + uiPx(4), oy = by - L - uiPx(4); const char* nm[3] = {"x", "y", "z"};
        RGBA cc[3] = {grayc(1.0f), grayc(0.72f), grayc(0.48f)};   // оси различаются яркостью и подписями
        glEnable(GL_LINE_SMOOTH);
        for (int k = 0; k < 3; k++) {
            double v[3] = {0, 0, 0}; v[k] = 1;
            float ax = (float)(v[0] * camR[0] + v[1] * camR[1] + v[2] * camR[2]) * L, ay = -(float)(v[0] * camU[0] + v[1] * camU[1] + v[2] * camU[2]) * L;
            col(cc[k]); glBegin(GL_LINES); glVertex2f(ox, oy); glVertex2f(ox + ax, oy + ay); glEnd();
            drawText(fontXS, ox + ax * 1.3f - uiPx(3), oy + ay * 1.3f - fontXS.h * 0.5f, nm[k], cc[k]);
        }
        glDisable(GL_LINE_SMOOTH);
        lx0 = ox + L + uiPx(18);
    }
    // линейка масштаба (нм / Å) в плоскости цели камеры
    if (layerScale) {
        double pps = pxPerSigma(), nmPerPx = cfg::U_L_NM / std::max(1e-9, pps);
        double lenNm = niceStep(uiPx(110) * nmPerPx); float lpx = (float)(lenNm / nmPerPx);
        if (lpx > uiPx(160)) { lenNm /= 2; lpx /= 2; }
        std::string lab = lenNm < 1 ? fmt("%g Å", lenNm * 10) : fmt("%g нм", lenNm);
        float y = by - uiPx(6);
        col(withA(C_TEXT, 0.85f));
        rectFill(lx0, y, lpx, 1, withA(C_TEXT, 0.85f)); rectFill(lx0, y - uiPx(4), 1, uiPx(5), withA(C_TEXT, 0.85f)); rectFill(lx0 + lpx - 1, y - uiPx(4), 1, uiPx(5), withA(C_TEXT, 0.85f));
        drawText(fontXS, lx0 + lpx / 2 - textW(fontXS, lab) / 2, y - uiPx(4) - fontXS.h, lab, withA(C_TEXT, 0.85f));
        drawText(fontXS, lx0 + lpx + uiPx(8), y - fontXS.h * 0.6f, "(в центре вида)", withA(C_DIM, 0.8f));
    }
    if (!layerLegend) return;
    float lw = uiPx(180), lx = sceneX + sceneW - lw - pad, ly = by - uiPx(12);
    auto frame = [&](float x, float w, const char* title) {
        float h = uiPx(44); boxPanel(x - uiPx(8), ly - h + uiPx(16), w + uiPx(16), h, withA(C_PANEL, 0.82f), C_LINE);
        drawText(fontXS, x, ly - h + uiPx(19), title, C_DIM);
    };
    auto bar = [&](const char* title, const std::string& lo, const std::string& hi, std::function<void(float, float&, float&, float&)> cmap) {
        lw = std::max(lw, textW(fontXS, title)); lx = sceneX + sceneW - lw - pad - uiPx(8);
        frame(lx, lw, title);
        MonoAtoms atomsColored;   // шкала объясняет цвет атомов — цветная
        glDisable(GL_TEXTURE_2D); glBegin(GL_QUADS);
        float y0 = ly - uiPx(9), y1 = ly - uiPx(3);
        for (int k = 0; k < 48; k++) { float r, g, b; cmap(k / 47.0f, r, g, b); float x0 = lx + lw * k / 48, x1 = lx + lw * (k + 1) / 48;
            glColor4f(r, g, b, 1); glVertex2f(x0, y0); glVertex2f(x1, y0); glVertex2f(x1, y1); glVertex2f(x0, y1); }
        glEnd();
        drawText(fontXS, lx, ly - uiPx(2), lo, C_DIM); drawTextR(fontXS, lx + lw, ly - uiPx(2), hi, C_DIM);
    };
    auto swatches = [&](const char* title, const std::vector<std::pair<std::string, RGBA>>& items) {
        float w = 0; for (auto& it : items) w += chemW(fontXS, it.first) + uiPx(20);
        w = std::max(w, textW(fontXS, title));
        float x = sceneX + sceneW - pad - w;
        frame(x, w, title);
        float yy = ly - uiPx(8);
        for (auto& it : items) { { MonoAtoms atomsColored; rectFill(x, yy + uiPx(2), uiPx(8), uiPx(8), it.second); } x += uiPx(12); x += drawChem(fontXS, x, yy - uiPx(2), it.first, C_TEXT) + uiPx(8); }
    };
    if (colorMode == 1) bar("цвет — скорость атома", "медленно", "быстро", velMap);
    else if (colorMode == 2) bar("цвет — потенциальная энергия, ε", fmt("%.1f", emin > 1e29 ? 0.0 : emin), fmt("%.1f", emax < -1e29 ? 0.0 : emax),
                                 [](float u, float& r, float& g, float& b) { hsv(0.66f - 0.66f * u, 0.62f, 0.92f, r, g, b); });
    else if (colorMode == 3) swatches("цвет — локальная структура", {{"ГЦК", {0.40f, 0.82f, 0.45f, 1}}, {"ГПУ", {0.92f, 0.40f, 0.34f, 1}}, {"ОЦК", {0.38f, 0.56f, 0.95f, 1}},
                                                                     {"ПК/NaCl", {0.92f, 0.80f, 0.34f, 1}}, {"лёд", {0.65f, 0.88f, 0.98f, 1}}, {"прочее", {0.55f, 0.56f, 0.6f, 1}}});
    else if (colorMode == 4) { const int n0 = 12;
        swatches("цвет — число соседей", {{fmt("%d норма", n0), {0.55f, 0.72f, 0.86f, 1}}, {fmt("%d", n0 - 1), {0.92f, 0.40f, 0.34f, 1}}, {fmt("%d", n0 + 1), {0.35f, 0.52f, 0.95f, 1}},
                                         {"мало", {0.92f, 0.76f, 0.34f, 1}}, {"газ", {0.42f, 0.44f, 0.48f, 1}}}); }
    else if (colorMode == 5) {
        if (atomPhase.size() == (size_t)S.n) swatches("цвет — агрегатное состояние", {{fmt("газ %.0f%%", phaseFrac[0] * 100), PHASE_C[0]}, {fmt("жидк. %.0f%%", phaseFrac[1] * 100), PHASE_C[1]},
                                                                                   {fmt("тв. %.0f%%", phaseFrac[2] * 100), PHASE_C[2]}, {"закрепл.", PHASE_C[3]}});
        else swatches("цвет — агрегатное состояние", {{"анализ ещё не готов", C_FAINT}});
    }
    else {   // по элементам — только присутствующие
        std::vector<std::pair<std::string, RGBA>> it;
        for (int t = 0; t < NEL; t++) if (present[t] && !EL[t].fixed) it.push_back({EL[t].sym, {EL[t].r, EL[t].g, EL[t].b, 1}});
        if (!it.empty() && it.size() <= 8) swatches("цвет — элемент", it);
    }
}

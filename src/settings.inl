// ===================================== НАСТРОЙКИ ============================================
// Хранятся в atoms.ini рядом с atoms.exe: по строке «ключ=значение». При записи незнакомые строки сохраняются,
// испорченные значения при чтении заменяются значениями по умолчанию. Окно настроек — F8 или шестерёнка сверху.
enum { START_GAS, START_LAST_SCENE, START_SESSION };
enum { SHOTS_EXE_DIR, SHOTS_PICTURES };
struct Settings {
    int lang = -1;               // −1 — не выбран (берётся язык Windows)
    int uiScale = 0;             // масштаб интерфейса в процентах; 0 — по размеру окна
    bool hints = true;           // всплывающие подсказки к кнопкам
    bool atomCard = true;        // карточка атома под курсором
    bool clock = true;           // время модели и к/с в верхней панели
    double toastSec = 3.0;       // сколько секунд видно уведомление
    bool vsync = true;
    int sceneBg = 0;             // фон сцены: 0 чёрный, 1 графит, 2 тёмно-серый
    double fog = 0.62;           // затемнение дальних атомов: 0 — нет, 1 — до чёрного
    bool gloss = true;           // блики на шарах
    bool box = true;             // контур ящика
    double bondW = 1.0;          // толщина связей
    int style = 0;               // модель молекул: 0 авто, 1 шаростержневая, 2 ван-дер-ваальсова, 3 палочки (render.inl, MS_*)
    int font = 0;                // шрифт интерфейса: 0 Bahnschrift, 1 Segoe UI, 2 Calibri, 3 Verdana
    bool glow = true;            // свечение раскалённых атомов (пламя, взрывы)
    bool smoothVib = true;       // средняя форма молекул вместо дрожи быстрых колебаний (smooth.inl)
    double mouseSens = 1.0;      // чувствительность вращения мышью
    bool invertY = false;
    double zoomSens = 1.0;       // шаг наезда колесом
    double camLag = 0.09;        // сглаживание камеры, с (0 — камера сразу там, куда её повернули)
    double spin = 0.25;          // скорость автовращения, рад/с
    int threads = 0;             // потоков расчёта; 0 — все ядра
    int undo = 20;               // сколько действий можно отменить
    bool bgPause = false;        // останавливать расчёт, пока окно не активно
    int start = START_GAS;       // что открывать при запуске
    int lastScene = 1, lastVar = 0;
    int recStep = 2;             // запись кадров: каждый N-й кадр
    int shotsTo = SHOTS_EXE_DIR; // куда складывать снимки, кадры и CSV
    bool keepWindow = true;      // запоминать размер и положение окна
    bool startFull = false;      // запускать на весь экран
    int wx = 0, wy = 0, ww = 0, wh = 0; bool wmax = false;   // окно при последнем выходе
};
static Settings opt;
static const Settings OPT_DEFAULT;
static bool settingsOn = false;   // открыто окно настроек
static bool atomViewOn = false;   // открыто окно «Строение атома» (orbitals.inl, F7)

struct OptKey { const char* key; char type; void* p; double lo, hi; };
static const OptKey OPT_KEYS[] = {
    {"ui_scale", 'i', &opt.uiScale, 0, 300},     {"hints", 'b', &opt.hints, 0, 1},        {"atom_card", 'b', &opt.atomCard, 0, 1},
    {"clock", 'b', &opt.clock, 0, 1},             {"toast_sec", 'd', &opt.toastSec, 1, 8}, {"vsync", 'b', &opt.vsync, 0, 1},
    {"scene_bg", 'i', &opt.sceneBg, 0, 2},        {"fog", 'd', &opt.fog, 0, 1},            {"gloss", 'b', &opt.gloss, 0, 1},
    {"box", 'b', &opt.box, 0, 1},                 {"bond_width", 'd', &opt.bondW, 0.4, 2.5},
    {"model", 'i', &opt.style, 0, 3},             {"font", 'i', &opt.font, 0, 3},          {"glow", 'b', &opt.glow, 0, 1},
    {"smooth_vibrations", 'b', &opt.smoothVib, 0, 1},
    {"mouse_sens", 'd', &opt.mouseSens, 0.25, 3}, {"invert_y", 'b', &opt.invertY, 0, 1},  {"zoom_sens", 'd', &opt.zoomSens, 0.25, 3},
    {"camera_lag", 'd', &opt.camLag, 0, 0.4},     {"spin", 'd', &opt.spin, 0.05, 1.5},
    {"threads", 'i', &opt.threads, 0, 256},       {"undo", 'i', &opt.undo, 1, 100},       {"bg_pause", 'b', &opt.bgPause, 0, 1},
    {"start", 'i', &opt.start, 0, 2},             {"last_scene", 'i', &opt.lastScene, 0, 99}, {"last_variant", 'i', &opt.lastVar, 0, 9},
    {"rec_step", 'i', &opt.recStep, 1, 4},        {"shots_to", 'i', &opt.shotsTo, 0, 1},
    {"keep_window", 'b', &opt.keepWindow, 0, 1},  {"fullscreen", 'b', &opt.startFull, 0, 1},
    {"window_x", 'i', &opt.wx, -32000, 32000},    {"window_y", 'i', &opt.wy, -32000, 32000},
    {"window_w", 'i', &opt.ww, 0, 16000},         {"window_h", 'i', &opt.wh, 0, 16000},   {"window_max", 'b', &opt.wmax, 0, 1},
};

static std::wstring exeDir() {
    wchar_t p[MAX_PATH]; DWORD n = GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring s(p, n); size_t k = s.find_last_of(L"\\/"); return k == std::wstring::npos ? L"." : s.substr(0, k);
}
static std::string narrow(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr); return s;
}
static std::wstring iniPath() { return exeDir() + L"\\atoms.ini"; }

static std::string trimStr(std::string s) {
    size_t a = 0, b = s.size();
    if (s.compare(0, 3, "\xEF\xBB\xBF") == 0) a = 3;
    while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
    return s.substr(a, b - a);
}
// «ключ = значение»; false — комментарий, пустая строка или строка без «=»
static bool iniSplit(const std::string& line, std::string& key, std::string& val) {
    std::string s = trimStr(line);
    if (s.empty() || s[0] == ';' || s[0] == '#' || s[0] == '[') return false;
    size_t e = s.find('='); if (e == std::string::npos) return false;
    key = trimStr(s.substr(0, e)); val = trimStr(s.substr(e + 1));
    for (char& c : key) c = (char)std::tolower((unsigned char)c);
    return !key.empty();
}
static const OptKey* optFind(const std::string& key) {
    for (const OptKey& k : OPT_KEYS) if (key == k.key) return &k;
    return nullptr;
}
static std::vector<std::string> iniLines() {
    std::vector<std::string> v;
    if (FILE* f = _wfopen(iniPath().c_str(), L"rb")) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) v.push_back(line);
        fclose(f);
    }
    return v;
}
static void settingsLoad() {
    for (const std::string& line : iniLines()) {
        std::string key, val;
        if (!iniSplit(line, key, val)) continue;
        if (key == "lang") { opt.lang = !_strnicmp(val.c_str(), "en", 2) ? 1 : !_strnicmp(val.c_str(), "ru", 2) ? 0 : -1; continue; }
        const OptKey* k = optFind(key); if (!k) continue;
        char* end = nullptr; double v = strtod(val.c_str(), &end);
        if (end == val.c_str() || !std::isfinite(v)) continue;   // мусор — остаётся значение по умолчанию
        v = clampv(v, k->lo, k->hi);
        if (k->type == 'b') *(bool*)k->p = v != 0;
        else if (k->type == 'i') *(int*)k->p = (int)std::lround(v);
        else *(double*)k->p = v;
    }
}
static bool settingsNoSave = false;   // автотест и снимки не трогают atoms.ini
static void settingsSave() {
    if (settingsNoSave) return;
    std::vector<std::string> keep;   // строки, которых программа не знает (например, оставленные вручную)
    for (const std::string& line : iniLines()) {
        std::string key, val, t = trimStr(line);
        if (t.empty() || t[0] == ';') continue;
        if (iniSplit(line, key, val) && (key == "lang" || optFind(key))) continue;
        keep.push_back(t);
    }
    FILE* f = _wfopen(iniPath().c_str(), L"wb"); if (!f) return;   // нет прав на запись — настройки живут до выхода
    fputs("; Atoms settings. Edit in the program: F8\r\n", f);
    if (opt.lang >= 0) fprintf(f, "lang=%s\r\n", opt.lang == 1 ? "en" : "ru");
    for (const OptKey& k : OPT_KEYS) {
        if (k.type == 'b') fprintf(f, "%s=%d\r\n", k.key, *(bool*)k.p ? 1 : 0);
        else if (k.type == 'i') fprintf(f, "%s=%d\r\n", k.key, *(int*)k.p);
        else fprintf(f, "%s=%.3f\r\n", k.key, *(double*)k.p);
    }
    for (auto& s : keep) fprintf(f, "%s\r\n", s.c_str());
    fclose(f);
}
// папка для снимков, кадров и CSV: рядом с программой или «Изображения\Атомы»
static std::wstring outputDir(bool en) {
    if (opt.shotsTo == SHOTS_PICTURES) {
        wchar_t p[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_MYPICTURES | CSIDL_FLAG_CREATE, nullptr, 0, p))) {
            std::wstring d = std::wstring(p) + (en ? L"\\Atoms" : L"\\Атомы");
            if (CreateDirectoryW(d.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) return d;
        }
    }
    return exeDir();
}

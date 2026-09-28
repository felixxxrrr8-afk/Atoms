// ===================================== ЯЗЫК ИНТЕРФЕЙСА: русский / English =======================
// Исходный язык — русский: все строки интерфейса в коде русские. Английский текст берётся из таблицы LANG_TABLE
// (src/lang_table.inl: ключ — точная русская строка, значение — перевод; таблицу ведёт скрипт tools/i18n.py).
// Перевод выполняется там, где текст выводится: drawText / textW (и drawTextR / drawTextC), drawChem / chemW,
// drawWrapped, всплывающие подсказки (drawHint), fmt() (переводится строка формата — спецификаторы в переводе те же)
// и showToast — поэтому обычные литералы правок не требуют. Составные строки (склейка кусков, русские массивы и
// переменные в %s) переводятся по частям через T() в месте склейки: fmt("%s — %s", e.sym, T(e.name)).
// Язык переключается на лету (Ctrl+L, «RU | EN» в верхней панели): всё, что рисуется каждый кадр, меняется сразу;
// строки, сохранённые при создании (уведомления, журнал переходов, тревога физики), остаются на языке создания.
// Порядок выбора языка: --lang ru|en → atoms.ini рядом с atoms.exe → язык Windows; режимы без окна — русский.
#include "lang_table.inl"
enum { LANG_RU, LANG_EN };
static int LANG = LANG_RU;              // текущий язык интерфейса
static int langCmd = -1;                // --lang ru|en в командной строке (−1 — не задан); не запоминается
static bool langCheck = false;          // --langcheck: в режиме EN собирать строки без перевода → lang_missing.log

// кириллица U+0400–U+04FF в UTF-8 — ведущие байты 0xD0–0xD3 (строка без них не переводится)
static inline bool langHasCyr(const char* p, size_t n) {
    for (size_t i = 0; i < n; i++) { unsigned char c = (unsigned char)p[i]; if (c >= 0xD0 && c <= 0xD3) return true; }
    return false;
}
// словарь «русская строка → перевод»: строится один раз (при первом переходе на английский)
static const std::unordered_map<std::string_view, const char*>& langMap() {
    static const std::unordered_map<std::string_view, const char*> m = [] {
        std::unordered_map<std::string_view, const char*> r; r.reserve(2 * (sizeof(LANG_TABLE) / sizeof(LANG_TABLE[0])));
        for (const auto& row : LANG_TABLE) if (row[0] && row[1] && row[1][0]) r.emplace(row[0], row[1]);   // пустой перевод — остаётся русский
        return r;
    }();
    return m;
}
// --langcheck: строки без перевода — текст → число обращений и точки перевода (где строка встретилась)
struct LangMiss { long long n = 0; std::string hooks; };
static std::map<std::string, LangMiss>* langMisses = nullptr;
static void langMiss(std::string_view s, const char* hook) {
#pragma omp critical(langMissLock)
    {
        if (!langMisses) langMisses = new std::map<std::string, LangMiss>();
        LangMiss& e = (*langMisses)[std::string(s)];
        e.n++;
        if (("," + e.hooks + ",").find(std::string(",") + hook + ",") == std::string::npos) { if (!e.hooks.empty()) e.hooks += ","; e.hooks += hook; }
    }
}
// lang_missing.log (текущая папка): «число обращений <Tab> точки перевода <Tab> строка»; пустой файл — всё переведено
static void langWriteMissing() {
    FILE* f = fopen("lang_missing.log", "wb"); if (!f) return;
    if (langMisses) for (auto& kv : *langMisses) {
        std::string s; for (char c : kv.first) { if (c == '\n') s += "\\n"; else if (c == '\t') s += "\\t"; else s += c; }
        fprintf(f, "%lld\t%s\t%s\n", kv.second.n, kv.second.hooks.c_str(), s.c_str());
    }
    fclose(f);
}
// перевод строки; nullptr — переводить нечего (русский режим, нет кириллицы) или строки нет в таблице
static const char* langFind(const char* p, size_t n, const char* hook) {
    if (LANG == LANG_RU || !langHasCyr(p, n)) return nullptr;
    const auto& m = langMap(); auto it = m.find(std::string_view(p, n));
    if (it != m.end()) return it->second;
    if (langCheck) langMiss(std::string_view(p, n), hook);
    return nullptr;   // нет перевода — показываем русский текст
}
// T() — перевод строки интерфейса (кусков составных строк). Возвращает перевод из таблицы или сам аргумент;
// для std::string — s.c_str(), если перевода нет: результат живёт, пока жив аргумент (не сохранять от временной строки)
static inline const char* T(const char* s) { if (LANG == LANG_RU || !s) return s; const char* r = langFind(s, strlen(s), "T"); return r ? r : s; }
static inline const char* T(const std::string& s) { if (LANG == LANG_RU) return s.c_str(); const char* r = langFind(s.data(), s.size(), "T"); return r ? r : s.c_str(); }
// перевод в точках вывода текста (hook — имя точки для lang_missing.log)
static inline std::string_view Tsv(std::string_view s, const char* hook) {
    if (LANG == LANG_RU) return s;
    const char* r = langFind(s.data(), s.size(), hook); return r ? std::string_view(r) : s;
}
// UTF-8 → UTF-16 (заголовок окна, диалоги Windows) и перевод для «широких» строк
static std::wstring widen(const char* s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0); if (n <= 1) return std::wstring();
    std::wstring w((size_t)n - 1, L'\0'); MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n); return w;
}
static inline std::wstring TW(const char* s) { return widen(T(s)); }
static void langSet(int l) { LANG = l == LANG_EN ? LANG_EN : LANG_RU; if (LANG == LANG_EN) langMap(); }
// командная строка разбирается при статической инициализации — раньше проверок без окна (--phystest, --chemtest),
// которые запускаются из статических инициализаторов в panel_phys.inl / panel_chem.inl
static int langParseCmd() {
    const wchar_t* cmd = GetCommandLineW();
    for (const wchar_t* p = cmd ? wcsstr(cmd, L"--lang") : nullptr; p; p = wcsstr(p + 6, L"--lang")) {
        if (p[6] != L' ' && p[6] != L'=') { if (!wcsncmp(p + 6, L"check", 5)) langCheck = true; continue; }
        const wchar_t* v = p + 7; while (*v == L' ') v++;
        if (!_wcsnicmp(v, L"en", 2)) langCmd = LANG_EN; else if (!_wcsnicmp(v, L"ru", 2)) langCmd = LANG_RU;
    }
    langSet(langCmd >= 0 ? langCmd : LANG_RU);   // окно выбирает язык по умолчанию в wWinMain (atoms.ini / язык Windows)
    if (langCheck) atexit(langWriteMissing);
    return 0;
}
static const int langCmdHook = langParseCmd();

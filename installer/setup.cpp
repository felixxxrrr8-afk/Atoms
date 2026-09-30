// Установщик «Атомов» одним файлом: внутри — atoms.exe, библиотека OpenMP, инструкции, лицензия и uninstall.exe.
// По умолчанию ставит программу для текущего пользователя (%LOCALAPPDATA%\Programs\Atoms) — права администратора
// не нужны; если выбрана папка, куда без них не записать, перезапускается с запросом прав. Делает ярлыки на рабочем
// столе и в меню «Пуск» и записывает программу в список установленных приложений Windows.
//
// Тот же исходник с /DUNINSTALLER — uninstall.exe: убирает установленные файлы, настройки, ярлыки и запись
// в списке программ. Файлы пользователя (снимки, быстрое сохранение, CSV) остаются — и тогда остаётся папка.
//
// Сборка — build.bat setup. Ключи командной строки:
//   установщик: /S — без окна, /D=папка, /nodesktop, /nostartmenu, /norun
//   uninstall.exe: /S — без вопросов
//   оба: /lang=ru или /lang=en — язык окон (иначе — язык Windows)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellscalingapi.h>
#include <commctrl.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>
#include "ver.h"   // APP_VER — версия из res\atoms.rc (пишет build.bat)

static bool RU = true;   // язык окон: русский, если Windows на русском, иначе английский
static const wchar_t* tr(const wchar_t* ru, const wchar_t* en) { return RU ? ru : en; }

static const wchar_t* UNINST_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Atoms";
static const wchar_t* HOME_URL = L"https://github.com/felixxxrrr8-afk/atoms-md";

// файлы программы: id ресурса в установщике и имя в папке установки
struct Item { int id; const wchar_t* name; };
static const Item ITEMS[] = {
    {101, L"atoms.exe"}, {102, L"vcomp140.dll"}, {103, L"MANUAL.html"}, {104, L"ИНСТРУКЦИЯ.html"},
    {105, L"LICENSE.txt"}, {106, L"uninstall.exe"},
};
// что программа сама пишет рядом с собой и что удаляется вместе с ней (быстрое сохранение и снимки — нет)
static const wchar_t* STATE_FILES[] = {L"atoms.ini", L"session.atoms", L"lang_missing.log"};

// ===================================== общее ================================================
static std::wstring selfPath() {
    std::vector<wchar_t> b(1024);
    for (;;) {
        DWORD n = GetModuleFileNameW(nullptr, b.data(), (DWORD)b.size());
        if (n < b.size()) return std::wstring(b.data(), n);
        b.resize(b.size() * 2);
    }
}
static std::wstring dirOf(const std::wstring& p) { size_t k = p.find_last_of(L"\\/"); return k == std::wstring::npos ? L"." : p.substr(0, k); }
static std::wstring nameOf(const std::wstring& p) { size_t k = p.find_last_of(L"\\/"); return k == std::wstring::npos ? p : p.substr(k + 1); }
static bool exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
static bool isDir(const std::wstring& p) { DWORD a = GetFileAttributesW(p.c_str()); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }
static std::wstring knownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr; std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) r = p;
    CoTaskMemFree(p); return r;
}
static std::wstring sysError(DWORD e) {
    wchar_t* buf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, e, 0, (LPWSTR)&buf, 0, nullptr);
    std::wstring s = buf ? buf : L""; LocalFree(buf);
    while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r' || s.back() == L' ' || s.back() == L'.')) s.pop_back();
    return s;
}
static bool elevated() {
    HANDLE t = nullptr; TOKEN_ELEVATION te = {}; DWORD n = 0; bool r = false;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) { if (GetTokenInformation(t, TokenElevation, &te, sizeof te, &n)) r = te.TokenIsElevated != 0; CloseHandle(t); }
    return r;
}
// перезапуск себя с запросом прав администратора; false — пользователь отказал
static bool relaunchElevated(const std::wstring& args) {
    const std::wstring self = selfPath();
    SHELLEXECUTEINFOW si = {sizeof si};
    si.lpVerb = L"runas"; si.lpFile = self.c_str(); si.lpParameters = args.c_str(); si.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&si) != FALSE;
}
// можно ли писать в папку (создаёт её при необходимости). ERROR_ACCESS_DENIED — нужны права администратора
static DWORD probeWritable(const std::wstring& dir) {
    int r = SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    if (r != ERROR_SUCCESS && r != ERROR_ALREADY_EXISTS) return (DWORD)r;
    if (!isDir(dir)) return ERROR_DIRECTORY;
    const std::wstring probe = dir + L"\\~atoms-setup.tmp";
    HANDLE f = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (f == INVALID_HANDLE_VALUE) return GetLastError();
    CloseHandle(f); return ERROR_SUCCESS;
}
static std::wstring regRead(HKEY root, const wchar_t* name) {
    wchar_t buf[2048]; DWORD n = sizeof buf;
    return RegGetValueW(root, UNINST_KEY, name, RRF_RT_REG_SZ, nullptr, buf, &n) == ERROR_SUCCESS ? std::wstring(buf) : std::wstring();
}
// ярлык: имя (без .lnk) на рабочем столе и в «Программах» меню «Пуск»
static std::wstring linkName() { return tr(L"Атомы", L"Atoms"); }
static std::wstring desktopLink(const std::wstring& name) { return knownFolder(FOLDERID_Desktop) + L"\\" + name + L".lnk"; }
static std::wstring startLink(const std::wstring& name) { return knownFolder(FOLDERID_Programs) + L"\\" + name + L".lnk"; }
// путь, на который указывает ярлык (пусто — не ярлык или не открылся)
static std::wstring linkTarget(const std::wstring& lnk) {
    std::wstring r; IShellLinkW* sl = nullptr; IPersistFile* pf = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&sl)))) return r;
    if (SUCCEEDED(sl->QueryInterface(IID_PPV_ARGS(&pf)))) {
        wchar_t buf[MAX_PATH * 2] = L"";
        if (SUCCEEDED(pf->Load(lnk.c_str(), STGM_READ)) && SUCCEEDED(sl->GetPath(buf, (int)std::size(buf), nullptr, SLGP_RAWPATH))) r = buf;
        pf->Release();
    }
    sl->Release(); return r;
}

#ifdef UNINSTALLER
// ===================================== удаление ==============================================
static int uninstall(bool silent) {
    const std::wstring self = selfPath(), dir = dirOf(self), exe = dir + L"\\atoms.exe";
    const wchar_t* title = tr(L"Удаление «Атомов»", L"Uninstall Atoms");
    if (!silent) {
        const std::wstring q = std::wstring(tr(L"Удалить программу «Атомы» из папки\n", L"Remove Atoms from\n")) + dir +
            tr(L"?\n\nЯрлыки и запись в списке программ тоже будут удалены. Снимки, быстрое сохранение и другие ваши файлы останутся.",
               L"?\n\nThe shortcuts and the entry in the list of apps will be removed too. Snapshots, the quick save and your other files stay.");
        if (MessageBoxW(nullptr, q.c_str(), title, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return 1;
    }
    if (probeWritable(dir) == ERROR_ACCESS_DENIED && !elevated()) {   // программа стоит в папке администратора
        if (!relaunchElevated(RU ? L"/S /lang=ru" : L"/S /lang=en")) return 1;
        return 0;
    }
    // atoms.exe открыт — его файл не удалить
    while (exists(exe) && !DeleteFileW(exe.c_str())) {
        const DWORD e = GetLastError();
        if (silent) return 2;
        const std::wstring m = e == ERROR_SHARING_VIOLATION || e == ERROR_ACCESS_DENIED
            ? std::wstring(tr(L"Программа «Атомы» сейчас запущена. Закройте её и нажмите «Повторить».", L"Atoms is running. Close it and press Retry."))
            : sysError(e);
        if (MessageBoxW(nullptr, m.c_str(), title, MB_RETRYCANCEL | MB_ICONWARNING) != IDRETRY) return 2;
    }
    for (const Item& it : ITEMS) if (std::wstring(it.name) != L"uninstall.exe") DeleteFileW((dir + L"\\" + it.name).c_str());
    for (const wchar_t* f : STATE_FILES) DeleteFileW((dir + L"\\" + f).c_str());
    // ярлыки — только те, что ведут на эту программу (имя на обоих языках: ставили могли с любым)
    std::vector<std::wstring> names = {L"Атомы", L"Atoms"};
    const std::wstring saved = regRead(HKEY_CURRENT_USER, L"ShortcutName");
    if (!saved.empty()) names.insert(names.begin(), saved);
    for (const std::wstring& n : names) for (const std::wstring& lnk : {desktopLink(n), startLink(n)})
        if (exists(lnk) && CompareStringOrdinal(linkTarget(lnk).c_str(), -1, exe.c_str(), -1, TRUE) == CSTR_EQUAL) DeleteFileW(lnk.c_str());
    // запись в списке программ — если она об этой папке (могли переставить в другую)
    const std::wstring loc = regRead(HKEY_CURRENT_USER, L"InstallLocation");
    if (loc.empty() || CompareStringOrdinal(loc.c_str(), -1, dir.c_str(), -1, TRUE) == CSTR_EQUAL) RegDeleteTreeW(HKEY_CURRENT_USER, UNINST_KEY);
    // остались ли файлы пользователя
    bool userFiles = false;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { const std::wstring n = fd.cFileName; if (n != L"." && n != L".." && CompareStringOrdinal(n.c_str(), -1, nameOf(self).c_str(), -1, TRUE) != CSTR_EQUAL) userFiles = true; } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    // сам uninstall.exe и пустая папка удаляются после выхода: через пару секунд командой cmd (из временной папки,
    // чтобы не держать удаляемую папку текущей)
    wchar_t tmp[MAX_PATH + 1] = L""; GetTempPathW(MAX_PATH, tmp);
    std::wstring cmd = L"cmd.exe /d /c ping -n 3 127.0.0.1 >nul & del /f /q \"" + self + L"\" & rd \"" + dir + L"\"";
    STARTUPINFOW si = {sizeof si}; PROCESS_INFORMATION pi = {};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, tmp, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    if (!silent) {
        const std::wstring m = userFiles
            ? std::wstring(tr(L"«Атомы» удалены. Папка осталась — в ней ваши файлы:\n", L"Atoms has been removed. The folder stays — it holds your files:\n")) + dir
            : std::wstring(tr(L"«Атомы» удалены.", L"Atoms has been removed."));
        MessageBoxW(nullptr, m.c_str(), title, MB_OK | MB_ICONINFORMATION);
    }
    return 0;
}
#else
// ===================================== установка =============================================
struct Options { std::wstring dir; bool desktop = true, startMenu = true, run = true; };

static const void* resData(int id, DWORD& size) {
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!r) { size = 0; return nullptr; }
    size = SizeofResource(nullptr, r);
    return LockResource(LoadResource(nullptr, r));
}
static unsigned long long payloadSize() { unsigned long long s = 0; for (const Item& it : ITEMS) { DWORD n; resData(it.id, n); s += n; } return s; }
static bool makeLink(const std::wstring& lnk, const std::wstring& target, const std::wstring& dir, const std::wstring& desc) {
    IShellLinkW* sl = nullptr; IPersistFile* pf = nullptr; bool ok = false;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&sl)))) return false;
    sl->SetPath(target.c_str()); sl->SetWorkingDirectory(dir.c_str()); sl->SetDescription(desc.c_str()); sl->SetIconLocation(target.c_str(), 0);
    if (SUCCEEDED(sl->QueryInterface(IID_PPV_ARGS(&pf)))) { ok = SUCCEEDED(pf->Save(lnk.c_str(), TRUE)); pf->Release(); }
    sl->Release(); return ok;
}
static void regStr(HKEY k, const wchar_t* name, const std::wstring& v) { RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)v.c_str(), DWORD((v.size() + 1) * sizeof(wchar_t))); }
static void regDword(HKEY k, const wchar_t* name, DWORD v) { RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof v); }

// установка: пустая строка — успех, иначе текст ошибки. progress(k, n, имя файла) — для полосы в окне
template <class F> static std::wstring install(const Options& o, F progress) {
    const int n = (int)std::size(ITEMS);
    for (int k = 0; k < n; k++) {
        progress(k, n, ITEMS[k].name);
        DWORD size = 0; const void* data = resData(ITEMS[k].id, size);
        if (!data) return std::wstring(tr(L"Установщик повреждён: нет файла ", L"The installer is damaged: missing ")) + ITEMS[k].name;
        const std::wstring path = o.dir + L"\\" + ITEMS[k].name;
        HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) {
            const DWORD e = GetLastError();
            if (k == 0 && (e == ERROR_SHARING_VIOLATION || e == ERROR_ACCESS_DENIED) && exists(path))
                return tr(L"Программа «Атомы» в этой папке сейчас запущена. Закройте её и нажмите «Установить» ещё раз.",
                          L"Atoms is running from this folder. Close it and press Install again.");
            return std::wstring(ITEMS[k].name) + L": " + sysError(e);
        }
        DWORD w = 0; const BOOL ok = WriteFile(f, data, size, &w, nullptr) && w == size; const DWORD e = GetLastError();
        CloseHandle(f);
        if (!ok) return std::wstring(ITEMS[k].name) + L": " + sysError(e);
    }
    progress(n, n, L"");
    const std::wstring exe = o.dir + L"\\atoms.exe", name = linkName();
    const std::wstring desc = tr(L"Молекулярная динамика и химия в реальном времени", L"Real-time molecular dynamics and chemistry");
    if (o.desktop) makeLink(desktopLink(name), exe, o.dir, desc);
    if (o.startMenu) makeLink(startLink(name), exe, o.dir, desc);
    // запись в «Установленных приложениях»: удаление — через uninstall.exe
    HKEY k = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, UNINST_KEY, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        const std::wstring un = L"\"" + o.dir + L"\\uninstall.exe\"";
        regStr(k, L"DisplayName", name); regStr(k, L"DisplayVersion", APP_VER); regStr(k, L"Publisher", L"felinebut");
        regStr(k, L"DisplayIcon", exe); regStr(k, L"InstallLocation", o.dir); regStr(k, L"UninstallString", un);
        regStr(k, L"QuietUninstallString", un + L" /S"); regStr(k, L"URLInfoAbout", HOME_URL); regStr(k, L"ShortcutName", name);
        regStr(k, L"Language", RU ? L"ru" : L"en");   // удаление говорит на том же языке, что и установка
        regDword(k, L"NoModify", 1); regDword(k, L"NoRepair", 1); regDword(k, L"EstimatedSize", DWORD(payloadSize() / 1024));
        RegCloseKey(k);
    }
    return L"";
}
static void runProgram(const std::wstring& dir) {
    const std::wstring exe = dir + L"\\atoms.exe";
    // из установщика с правами администратора программу запускает проводник — она получает обычные права
    if (elevated()) ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"\"" + exe + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
    else ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, dir.c_str(), SW_SHOWNORMAL);
}
static std::wstring defaultDir() {
    const std::wstring prev = regRead(HKEY_CURRENT_USER, L"InstallLocation");
    if (!prev.empty()) return prev;
    return knownFolder(FOLDERID_UserProgramFiles) + L"\\Atoms";   // %LOCALAPPDATA%\Programs
}
static std::wstring cleanDir(std::wstring d) {
    while (!d.empty() && (d.back() == L' ' || d.back() == L'\t')) d.pop_back();
    while (!d.empty() && (d.front() == L' ' || d.front() == L'\t' || d.front() == L'"')) d.erase(d.begin());
    while (!d.empty() && d.back() == L'"') d.pop_back();
    while (d.size() > 3 && (d.back() == L'\\' || d.back() == L'/')) d.pop_back();
    for (auto& c : d) if (c == L'/') c = L'\\';
    return d;
}
static bool absolutePath(const std::wstring& d) { return (d.size() >= 3 && d[1] == L':' && d[2] == L'\\') || (d.size() > 2 && d[0] == L'\\' && d[1] == L'\\'); }

// ---- окно
enum { ID_DIR = 10, ID_BROWSE, ID_DESKTOP, ID_START, ID_RUN, ID_INSTALL, ID_CANCEL };
static HWND hWnd, hLblDir, hDir, hBrowse, hSpace, hDesk, hStart, hRun, hStatus, hProgress, hInstall, hCancel;
static HFONT fontUi, fontTitle; static HICON bigIcon; static HBRUSH bandBrush; static UINT dpi = 96;
static Options opt; static bool finished = false, autoGo = false;
static int S(int v) { return MulDiv(v, (int)dpi, 96); }
static const int CW = 540, CH = 372, BAND = 58;   // размер клиентской области при 96 dpi и высота нижней полосы

static void makeFonts() {
    if (fontUi) DeleteObject(fontUi);
    if (fontTitle) DeleteObject(fontTitle);
    if (bigIcon) DestroyIcon(bigIcon);
    NONCLIENTMETRICSW m = {sizeof m}; SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof m, &m, 0, dpi);
    fontUi = CreateFontIndirectW(&m.lfMessageFont);
    LOGFONTW t = m.lfMessageFont; t.lfHeight = -S(22); t.lfWeight = FW_SEMIBOLD; fontTitle = CreateFontIndirectW(&t);
    bigIcon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, S(48), S(48), 0);
}
static void layout() {
    const int W = S(CW), H = S(CH), m = S(24), bw = S(100), bh = S(28);
    MoveWindow(hLblDir, m, S(98), W - 2 * m, S(20), FALSE);
    MoveWindow(hDir, m, S(120), W - 2 * m - bw - S(8), bh, FALSE);
    MoveWindow(hBrowse, W - m - bw, S(120), bw, bh, FALSE);
    MoveWindow(hSpace, m, S(152), W - 2 * m, S(20), FALSE);
    MoveWindow(hDesk, m, S(186), W - 2 * m, S(22), FALSE);
    MoveWindow(hStart, m, S(212), W - 2 * m, S(22), FALSE);
    MoveWindow(hRun, m, S(238), W - 2 * m, S(22), FALSE);
    MoveWindow(hStatus, m, S(274), W - 2 * m, S(20), FALSE);
    MoveWindow(hProgress, m, S(298), W - 2 * m, S(6), FALSE);
    const int by = H - S(BAND) + (S(BAND) - bh) / 2;
    MoveWindow(hCancel, W - m - bw, by, bw, bh, FALSE);
    MoveWindow(hInstall, W - m - 2 * bw - S(10), by, bw + S(4), bh, FALSE);
    for (HWND c : {hLblDir, hDir, hBrowse, hSpace, hDesk, hStart, hRun, hStatus, hInstall, hCancel}) SendMessageW(c, WM_SETFONT, (WPARAM)fontUi, FALSE);
    InvalidateRect(hWnd, nullptr, TRUE);
}
static void setStatus(const std::wstring& s) { SetWindowTextW(hStatus, s.c_str()); UpdateWindow(hStatus); }
static void readForm() {
    wchar_t buf[2048] = L""; GetWindowTextW(hDir, buf, (int)std::size(buf)); opt.dir = cleanDir(buf);
    opt.desktop = SendMessageW(hDesk, BM_GETCHECK, 0, 0) == BST_CHECKED;
    opt.startMenu = SendMessageW(hStart, BM_GETCHECK, 0, 0) == BST_CHECKED;
    opt.run = SendMessageW(hRun, BM_GETCHECK, 0, 0) == BST_CHECKED;
}
static void browse() {
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return;
    DWORD f = 0; d->GetOptions(&f); d->SetOptions(f | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    d->SetTitle(tr(L"Папка для программы «Атомы»", L"Folder for Atoms"));
    readForm();
    std::wstring start = opt.dir; while (!start.empty() && !isDir(start)) { const std::wstring up = dirOf(start); if (up == start) break; start = up; }
    IShellItem* si = nullptr;
    if (!start.empty() && SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&si)))) { d->SetFolder(si); si->Release(); }
    if (SUCCEEDED(d->Show(hWnd)) && SUCCEEDED(d->GetResult(&si))) {
        PWSTR p = nullptr;
        if (SUCCEEDED(si->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
            // в выбранной папке — своя подпапка Atoms (если это уже не она и не прежняя установка)
            std::wstring d2 = cleanDir(p); const std::wstring last = nameOf(d2);
            if (CompareStringOrdinal(last.c_str(), -1, L"Atoms", -1, TRUE) != CSTR_EQUAL && last != L"Атомы" && !exists(d2 + L"\\atoms.exe"))
                d2 += (d2.back() == L'\\' ? L"" : L"\\") + std::wstring(L"Atoms");
            SetWindowTextW(hDir, d2.c_str());
            CoTaskMemFree(p);
        }
        si->Release();
    }
    d->Release();
}
static void doInstall() {
    readForm();
    const wchar_t* title = tr(L"Установка «Атомов»", L"Atoms Setup");
    if (!absolutePath(opt.dir)) { MessageBoxW(hWnd, tr(L"Укажите полный путь к папке, например C:\\Atoms.", L"Enter a full folder path, for example C:\\Atoms."), title, MB_ICONWARNING); return; }
    const DWORD w = probeWritable(opt.dir);
    if (w == ERROR_ACCESS_DENIED && !elevated()) {
        // программа хранит настройки и быстрое сохранение рядом с собой — в папке администратора они не запишутся
        const int a = MessageBoxW(hWnd, tr(L"В эту папку можно писать только с правами администратора.\n\nПрограмма хранит настройки и быстрое сохранение рядом с собой, а в такой папке они сохраняться не будут. Лучше выбрать папку пользователя (как предложено по умолчанию).\n\nВсё равно установить сюда с правами администратора?",
                                           L"Only an administrator can write to this folder.\n\nThe program keeps its settings and quick save next to itself, and they will not be saved in such a folder. A user folder (the default) is a better choice.\n\nInstall here with administrator rights anyway?"), title, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (a != IDYES) return;
        std::wstring args = std::wstring(RU ? L"/lang=ru" : L"/lang=en") + L" /go /D=\"" + opt.dir + L"\"";
        if (!opt.desktop) args += L" /nodesktop";
        if (!opt.startMenu) args += L" /nostartmenu";
        if (!opt.run) args += L" /norun";
        if (relaunchElevated(args)) DestroyWindow(hWnd);
        return;
    }
    if (w != ERROR_SUCCESS) { MessageBoxW(hWnd, (opt.dir + L"\n\n" + sysError(w)).c_str(), title, MB_ICONERROR); return; }
    for (HWND c : {hDir, hBrowse, hDesk, hStart, hRun, hInstall}) EnableWindow(c, FALSE);
    ShowWindow(hProgress, SW_SHOW);
    const std::wstring err = install(opt, [](int k, int n, const wchar_t* name) {
        SendMessageW(hProgress, PBM_SETRANGE32, 0, n); SendMessageW(hProgress, PBM_SETPOS, k, 0);
        if (*name) setStatus(std::wstring(tr(L"Копирование: ", L"Copying: ")) + name);
    });
    if (!err.empty()) {
        setStatus(L""); ShowWindow(hProgress, SW_HIDE);
        for (HWND c : {hDir, hBrowse, hDesk, hStart, hRun, hInstall}) EnableWindow(c, TRUE);
        MessageBoxW(hWnd, err.c_str(), title, MB_ICONERROR);
        return;
    }
    if (opt.run) { runProgram(opt.dir); DestroyWindow(hWnd); return; }
    finished = true;
    setStatus(std::wstring(tr(L"Готово: программа установлена в ", L"Done: installed to ")) + opt.dir);
    SetWindowTextW(hCancel, tr(L"Закрыть", L"Close"));
    SendMessageW(hInstall, BM_SETSTYLE, BS_PUSHBUTTON, TRUE); SendMessageW(hCancel, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE); SetFocus(hCancel);
}
static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        hWnd = h; dpi = GetDpiForWindow(h); makeFonts();
        const HINSTANCE hi = GetModuleHandleW(nullptr);
        auto ctl = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex = 0) {
            return CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, h, (HMENU)(INT_PTR)id, hi, nullptr);
        };
        hLblDir = ctl(L"STATIC", tr(L"Папка установки:", L"Install folder:"), 0, 0);
        hDir = ctl(L"EDIT", opt.dir.c_str(), WS_TABSTOP | ES_AUTOHSCROLL, ID_DIR, WS_EX_CLIENTEDGE);
        hBrowse = ctl(L"BUTTON", tr(L"Обзор…", L"Browse…"), WS_TABSTOP | BS_PUSHBUTTON, ID_BROWSE);
        wchar_t mb[64]; swprintf(mb, 64, L"%.1f", payloadSize() / 1048576.0);
        hSpace = ctl(L"STATIC", (std::wstring(tr(L"Нужно на диске: ", L"Disk space needed: ")) + mb + tr(L" МБ", L" MB")).c_str(), 0, 0);
        hDesk = ctl(L"BUTTON", tr(L"Ярлык на рабочем столе", L"Desktop shortcut"), WS_TABSTOP | BS_AUTOCHECKBOX, ID_DESKTOP);
        hStart = ctl(L"BUTTON", tr(L"Ярлык в меню «Пуск»", L"Start menu shortcut"), WS_TABSTOP | BS_AUTOCHECKBOX, ID_START);
        hRun = ctl(L"BUTTON", tr(L"Запустить после установки", L"Run Atoms when done"), WS_TABSTOP | BS_AUTOCHECKBOX, ID_RUN);
        hStatus = ctl(L"STATIC", L"", SS_ENDELLIPSIS | SS_NOPREFIX, 0);
        hProgress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD, 0, 0, 10, 10, h, nullptr, hi, nullptr);
        hInstall = ctl(L"BUTTON", tr(L"Установить", L"Install"), WS_TABSTOP | BS_DEFPUSHBUTTON, ID_INSTALL);
        hCancel = ctl(L"BUTTON", tr(L"Отмена", L"Cancel"), WS_TABSTOP | BS_PUSHBUTTON, ID_CANCEL);
        SendMessageW(hDesk, BM_SETCHECK, opt.desktop ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(hStart, BM_SETCHECK, opt.startMenu ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(hRun, BM_SETCHECK, opt.run ? BST_CHECKED : BST_UNCHECKED, 0);
        layout();
        return 0; }
    case WM_DPICHANGED: {
        dpi = HIWORD(wp); makeFonts();
        const RECT* r = (const RECT*)lp; SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        layout(); return 0; }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        RECT band = rc; band.top = rc.bottom - S(BAND); FillRect(dc, &band, bandBrush);
        RECT line = band; line.bottom = line.top + std::max(1, S(1)); HBRUSH lb = CreateSolidBrush(RGB(223, 223, 223)); FillRect(dc, &line, lb); DeleteObject(lb);
        if (bigIcon) DrawIconEx(dc, S(24), S(22), bigIcon, S(48), S(48), 0, nullptr, DI_NORMAL);
        SetBkMode(dc, TRANSPARENT);
        HGDIOBJ old = SelectObject(dc, fontTitle); SetTextColor(dc, RGB(20, 20, 20));
        const std::wstring t = std::wstring(tr(L"Атомы ", L"Atoms ")) + APP_VER;
        TextOutW(dc, S(88), S(20), t.c_str(), (int)t.size());
        SelectObject(dc, fontUi); SetTextColor(dc, RGB(96, 96, 96));
        const wchar_t* sub = tr(L"Молекулярная динамика и химия в реальном времени", L"Real-time molecular dynamics and chemistry");
        TextOutW(dc, S(89), S(52), sub, (int)wcslen(sub));
        SelectObject(dc, old); EndPaint(h, &ps);
        return 0; }
    case WM_CTLCOLORSTATIC: {   // подписи и флажки — на белом фоне окна
        HDC dc = (HDC)wp; SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, (HWND)lp == hSpace ? RGB(96, 96, 96) : RGB(20, 20, 20));
        return (LRESULT)GetStockObject(WHITE_BRUSH); }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_BROWSE: browse(); return 0;
        case ID_INSTALL: doInstall(); return 0;
        case ID_CANCEL: case IDCANCEL: DestroyWindow(h); return 0;
        }
        break;
    case DM_GETDEFID: return MAKELRESULT(finished ? ID_CANCEL : ID_INSTALL, DC_HASDEFID);   // Enter — кнопка по умолчанию
    case WM_APP: doInstall(); return 0;   // /go: установка сразу после перезапуска с правами администратора
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
static int runWindow(HINSTANCE hi) {
    INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES}; InitCommonControlsEx(&icc);
    bandBrush = CreateSolidBrush(RGB(243, 243, 243));
    WNDCLASSEXW wc = {sizeof wc};
    wc.lpfnWndProc = wndProc; wc.hInstance = hi; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = L"AtomsSetup"; wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1)); wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);
    // размер окна — под dpi монитора, на котором оно откроется (основного)
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    const POINT p0 = {0, 0}; HMONITOR mon = MonitorFromPoint(p0, MONITOR_DEFAULTTOPRIMARY);
    UINT dx = 96, dy = 96; GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy); dpi = dx;
    RECT r = {0, 0, S(CW), S(CH)}; AdjustWindowRectExForDpi(&r, style, FALSE, 0, dpi);
    MONITORINFO mi = {sizeof mi}; GetMonitorInfoW(mon, &mi);
    const int w = r.right - r.left, hgt = r.bottom - r.top;
    const int x = (mi.rcWork.left + mi.rcWork.right - w) / 2, y = (mi.rcWork.top + mi.rcWork.bottom - hgt) / 2;
    HWND h = CreateWindowExW(0, wc.lpszClassName, tr(L"Установка «Атомов»", L"Atoms Setup"), style, x, y, w, hgt, nullptr, nullptr, hi, nullptr);
    if (!h) return 1;
    ShowWindow(h, SW_SHOWNORMAL); UpdateWindow(h);
    if (autoGo) PostMessageW(h, WM_APP, 0, 0);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) if (!IsDialogMessageW(h, &m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    return 0;
}
#endif

// ===================================== запуск ================================================
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int) {
    RU = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool silent = false;
    int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
#ifdef UNINSTALLER
    { const std::wstring l = regRead(HKEY_CURRENT_USER, L"Language"); if (!l.empty()) RU = l == L"ru"; }
#endif
    for (int i = 1; i < argc; i++) {   // /lang=ru или /lang=en — язык окон вместо языка Windows
        if (!lstrcmpiW(argv[i], L"/lang=ru")) RU = true;
        else if (!lstrcmpiW(argv[i], L"/lang=en")) RU = false;
    }
#ifdef UNINSTALLER
    (void)hi;
    for (int i = 1; i < argc; i++) if (!lstrcmpiW(argv[i], L"/S")) silent = true;
    LocalFree(argv);
    const int r = uninstall(silent);
#else
    opt.dir = defaultDir();
    for (int i = 1; i < argc; i++) {
        const std::wstring a = argv[i];
        if (!lstrcmpiW(a.c_str(), L"/S")) silent = true;
        else if (a.size() > 3 && (a.compare(0, 3, L"/D=") == 0 || a.compare(0, 3, L"/d=") == 0)) opt.dir = cleanDir(a.substr(3));
        else if (!lstrcmpiW(a.c_str(), L"/nodesktop")) opt.desktop = false;
        else if (!lstrcmpiW(a.c_str(), L"/nostartmenu")) opt.startMenu = false;
        else if (!lstrcmpiW(a.c_str(), L"/norun")) opt.run = false;
        else if (!lstrcmpiW(a.c_str(), L"/go")) autoGo = true;
    }
    LocalFree(argv);
    int r = 0;
    if (silent) {   // без окна: код возврата 0 — установлено
        opt.run = false;
        const DWORD w = absolutePath(opt.dir) ? probeWritable(opt.dir) : ERROR_BAD_PATHNAME;
        r = w != ERROR_SUCCESS ? 2 : (install(opt, [](int, int, const wchar_t*) {}).empty() ? 0 : 3);
    } else r = runWindow(hi);
#endif
    CoUninitialize();
    return r;
}

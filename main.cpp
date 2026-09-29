// Атомы — молекулярная динамика вещества и простой химии в реальном времени.
// Газ, жидкость, кристалл, фазовые переходы и реакции не прописаны заранее: они получаются из потенциалов
// взаимодействия и законов сохранения.
//
// C++17, Win32, OpenGL 1.x, OpenMP; внешних библиотек нет. Сборка — build.bat или вручную:
//     rc /fo res\atoms.res res\atoms.rc
//     cl /O2 /openmp /utf-8 /EHsc /std:c++17 /fp:fast main.cpp res\atoms.res /Fe:atoms.exe user32.lib gdi32.lib opengl32.lib comdlg32.lib shell32.lib
//
// Запуск без окна (проверки):
//     --selftest            все сцены меню, решётки, NVE, кинетика → selftest.log
//     --uitest              сценарий ввода по всему интерфейсу → uitest.log (--langcheck: строки без перевода)
//     --shot K N [vV] …     сцена K, N кадров, снимок окна (все ключи — в app.inl; ими пользуется tools\media.ps1)
//     --gradcheck K N       силы против −∇U (численная производная) после N шагов сцены K
//     --evcheck K N         баланс энергии каждого химического события
//     --kin K N T V         длинный прогон сцены с отчётом о фазах, реакциях и живых измерениях
//     --chemtest lib        устойчивость каждой структуры библиотеки молекул
//     --lang ru|en          язык интерфейса (иначе atoms.ini, затем язык Windows); Ctrl+L — на лету
//
// Единицы — приведённые LJ (σ, ε аргона, масса 10 а.е.м.). Для аргона: T[K] = 139.8·T*, L[нм] = 0.3405·L*,
// t[пс] = 0.999·t*, P[атм] = 482.5·P* (ε/k подобран по критической точке аргона для LJ с обрезкой 2.5σ).
// Химический масштаб: 1 эВ = 4ε — энергии связей уменьшены, чтобы реакции шли при доступных температурах.
// Вода: заряды как в SPC/E, направленная водородная связь (DREIDING) и трёхчастичный член тетраэдричности (mW).
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <GL/gl.h>
#include <omp.h>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <algorithm>
#include <random>
#include <map>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <cctype>
#include <cstdarg>
#include <complex>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

static const double PI = 3.14159265358979323846;

// одна единица трансляции: модули включаются по порядку
static void drawPhysPanel(float x, float y, float w, float h);   // src/panel_phys.inl
static void drawChemPanel(float x, float y, float w, float h);   // src/panel_chem.inl
#include "src/lang.inl"      // перевод интерфейса (нужен fmt() из core.inl)
#include "src/core.inl"
#include "src/settings.inl"   // atoms.ini
#include "src/physics.inl"
#include "src/chemistry.inl"
#include "src/analysis.inl"
#include "src/presets.inl"
#include "src/render.inl"
#include "src/ui.inl"
#include "src/panel_phys.inl"
#include "src/panel_chem.inl"
#include "src/panel_settings.inl"   // окно настроек (F8)
#include "src/orbitals.inl"   // строение атома: электронные облака и орбитали (F7)
#include "src/world_nuclear.inl"   // ядра: распад, деление, цепная реакция
#include "src/worlds.inl"     // миры других масштабов: переключение, панели
#include "src/app.inl"

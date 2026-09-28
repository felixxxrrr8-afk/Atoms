// =====================================================================================
//  АТОМЫ — интерактивная молекулярная динамика вещества и простой химии в реальном времени (2D и 3D)
//  Газ, жидкость, кристалл, фазовые переходы, реакции — всё возникает из потенциалов и законов сохранения.
//
//  Стек: C++17 + Win32 + OpenGL 1.x + OpenMP. Внешних зависимостей нет (всё входит в Windows SDK / MSVC).
//  Сборка (x64 Native Tools Command Prompt, или просто запустите build.bat):
//     rc /fo atoms.res atoms.rc
//     cl /O2 /openmp /utf-8 /EHsc /std:c++17 /fp:fast main.cpp atoms.res /Fe:atoms.exe user32.lib gdi32.lib opengl32.lib comdlg32.lib shell32.lib
//  Запуск:   atoms.exe                 (D — переключение 2D/3D)
//            atoms.exe --selftest      — прогон всех пресетов в 2D и 3D без окна, отчёт в selftest.log
//            atoms.exe --shot K N [3d] [vV] — открыть пресет K (вариант V), отрисовать N кадров, сохранить shot_K.ppm
//            atoms.exe --lang en       — язык интерфейса ru|en (иначе atoms.ini, затем язык Windows); Ctrl+L — на лету;
//                                        --langcheck — строки без перевода в режиме EN → lang_missing.log
//  Инструменты проверки (без окна):
//            --gradcheck D K N  — сравнить силы с −∇U численным дифференцированием (пресет K, после N шагов)
//            --evcheck D K N    — баланс энергии каждого химического события и интегратора
//            --kin K D N T V    — длинный прогон пресета с отчётом о фазе, реакциях, растворении
//            --ice σO εO T      — устойчивость кубического льда при заданных параметрах кислорода
//
//  Единицы: безразмерные LJ (σ_Ar, ε_Ar, масса = 10 а.е.м.). Пересчёт для аргона:
//     T[K] = 139.8·T*,  L[нм] = 0.3405·L*,  t[пс] = 0.999·t*,  P[атм] = 482.5·P*
//     (ε/k откалиброван по критической точке аргона для LJ с обрезкой 2.5σ: T*c = 1.078 → 150.7 K)
//     (в 2D давление — сила на единицу длины, пересчитано на слой толщиной σ)
//  Химический масштаб: 1 эВ = 4 ε (энергии связей уменьшены, чтобы реакции шли при доступных T*).
//  Вода: заряды из электроотрицательностей (O −0.87, H +0.43, как SPC/E), направленная водородная связь
//  (DREIDING) и трёхчастичный член тетраэдричности (как в модели mW) — лёд плавится при T* ≈ 0.2–0.3.
// =====================================================================================
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

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

static const double PI = 3.14159265358979323846;

// Исходник разбит на модули (единая единица трансляции — порядок включения важен):
static void drawPhysPanel(float x, float y, float w, float h);   // src/panel_phys.inl
static void drawChemPanel(float x, float y, float w, float h);   // src/panel_chem.inl
#include "src/lang.inl"      // язык интерфейса RU/EN: T(), таблица перевода (до core.inl — ею пользуется fmt)
#include "src/core.inl"
#include "src/physics.inl"
#include "src/chemistry.inl"
#include "src/analysis.inl"
#include "src/presets.inl"
#include "src/render.inl"
#include "src/ui.inl"
#include "src/panel_phys.inl"
#include "src/panel_chem.inl"
#include "src/app.inl"

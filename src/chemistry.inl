// ===================================== CHEMISTRY =======================================
// Реакции — дискретные события на фоне непрерывной динамики:
//   • ассоциация   X· + Y· → X–Y          (оба атома имеют свободную валентность)
//   • обмен        X· + A–B → X–A + B·    (перенос одной единицы валентности A от B к X)
//   • диссоциация  A–B → A· + B·          (связь растянута дальше r0 + R_BREAK)
//   • перенос протона  D–H + :A → D⁻ + H–A⁺  (кислоты и основания: H3O+, OH−, NH4+, HCl; механизм Гроттгуса)
// Барьер: энергия удара по линии центров ½μv_n² ≥ Ea, Ea — по модели пересекающихся парабол (rxBarrier).
// Закон сохранения энергии точный: ΔU события либо поглощается сдвигом новой связи (и затем за ~τ_heat
// переходит в кинетическую энергию пары — это и есть выделение теплоты реакции), либо сразу компенсируется
// кинетической энергией участников в системе их центра масс (импульс сохраняется).
//
// Ионы. Отдельного «формального заряда» у атома не хранится: заряд молекулы Q = Σq (в зарядах электрона, у Na+ и Cl−
// полный ±1, как в модели Джунга — Чиэтэма), а места зарядов восстанавливаются по топологии:
// O с тремя связями — оксоний (H3O+), N с четырьмя — аммоний (NH4+), отрицательный заряд — на самом
// электроотрицательном атоме со свободной валентностью (OH−). Заряд иона распределяется по атомам пропорционально
// мягкости 1/η, η = I − A (упрощённое выравнивание электроотрицательностей, EEM); нейтральная часть — инкременты
// связей κ·Δχ (на них откалибрована вода). Радикальные реакции идут только у нейтральных молекул,
// заряженные частицы реагируют переносом протона. Энергия иона (без сольватации) хранится в сдвигах его связей
// (bc стремится не к нулю, а к «энергии иона») — поэтому нейтрализация H3O+ + OH− → 2H2O экзотермична.
struct Flash { double x, y, z; float age, str, r, g, b; };
static std::vector<Flash> flashes;
static std::vector<char> usedFlag;

namespace chem {
constexpr double ZION = 1.0;          // заряд однозарядного иона (Na+, Cl−, H3O+, OH−) — заряд электрона
constexpr double KJ_PER_EV = 96.485;  // 1 эВ на частицу = 96.485 кДж/моль
constexpr double EA_PT = 0.10 * cfg::EV;   // барьер прыжка протона по водородной связи ≈ 0.1 эВ: в воде — раз в 1–2 пс
constexpr double R_PT = 0.40;         // перенос протона возможен при r(H···A) < r0(A–H) + R_PT
constexpr double CAT_SURF = 0.3;      // поверхность металла-катализатора (Pt, Pd, Ni, Fe…): множитель кинетической части барьера
// Энергия ионов в газе относительно нейтральных частиц (эВ). Связи в модели рвутся «пополам» (гомолитически), поэтому
// цена разделения зарядов — энергия ионизации H минус сродство к электрону и сродство к протону — добавляется к ионам
// отдельно. Из цикла Борна — Габера по газофазным данным (NIST): H3O+ + OH− → 2H2O −9.8 эВ, HCl + H2O → H3O+ + Cl− +7.3 эВ,
// HCl + NH3 → NH4+ + Cl− +5.6 эВ; у Cl− своей доли нет (она вошла в остальные). В воде эту цену почти целиком
// возвращает гидратация ионов — её даёт кулоновское притяжение к молекулам воды, поэтому HCl в воде диссоциирует.
constexpr double E_OXONIUM = 7.63 * cfg::EV;
constexpr double E_HYDROXIDE = 2.17 * cfg::EV;
constexpr double E_AMMONIUM = 5.19 * cfg::EV;
constexpr double Q_LOC = 0.7;         // доля заряда иона на заряженном центре (остальное — по мягкости атомов)
constexpr int MOL_CAP = 64;           // молекулы крупнее — «сетки»: заряд по ним не перераспределяется
}
// теплота/энтальпия реакции (ε) → эВ и кДж/моль
static inline double rxEV(double e) { return e / cfg::EV; }
static inline double rxKJ(double e) { return e / cfg::EV * chem::KJ_PER_EV; }
// Барьер стадии с теплотой ΔH и собственным барьером E0 (Маркус): Ea = E0·(1 + ΔH/4E0)²; при ΔH ≤ −4E0 барьера нет,
// при ΔH ≥ 4E0 он равен ΔH. Прямой и обратный барьеры всегда отличаются ровно на ΔH, поэтому катализатор и множитель
// «барьеры реакций ×» (они умножают E0) ускоряют реакции, но не сдвигают равновесие
static inline double rxBarrier(double E0, double dH) {
    if (E0 <= 1e-12) return std::max(0.0, dH);
    const double x = dH / (4 * E0);
    if (x <= -1) return 0;
    if (x >= 1) return dH;
    return E0 * (1 + x) * (1 + x);
}
// собственный барьер переноса атома от B к радикалу i — среднее «половинок» концов (перекрёстное правило Маркуса):
// у атомов галогенов вдвое меньше, чем у H, C, N, O. Так Cl + CH4 → HCl + CH3 получает 0.17 эВ (опыт 0.11),
// Cl + H2 — 0.27 (0.2), а Br + H2 по-прежнему 0.74 (0.78)
static inline double rxIntrinsic(int i, int B) {
    auto g = [](int a) { const int z = EL[S.ty[a] == E_CLM ? E_CL : S.ty[a]].Z; return (z == 9 || z == 17 || z == 35 || z == 53) ? 0.5 : 1.0; };
    return cfg::EA_EXCH * 0.5 * (g(i) + g(B));
}

// ---- первые энергии ионизации I (эВ) для Z = 1…118 и сродство к электрону A (эВ): жёсткость η = I − A
static const float IE_Z[119] = {0,
    13.598f, 24.587f, 5.392f, 9.323f, 8.298f, 11.260f, 14.534f, 13.618f, 17.423f, 21.565f,     // H … Ne
    5.139f, 7.646f, 5.986f, 8.152f, 10.487f, 10.360f, 12.968f, 15.760f, 4.341f, 6.113f,        // Na … Ca
    6.561f, 6.828f, 6.746f, 6.767f, 7.434f, 7.902f, 7.881f, 7.640f, 7.726f, 9.394f,            // Sc … Zn
    5.999f, 7.900f, 9.815f, 9.752f, 11.814f, 14.000f, 4.177f, 5.695f, 6.217f, 6.634f,          // Ga … Zr
    6.759f, 7.092f, 7.28f, 7.361f, 7.459f, 8.337f, 7.576f, 8.994f, 5.786f, 7.344f,             // Nb … Sn
    8.608f, 9.010f, 10.451f, 12.130f, 3.894f, 5.212f, 5.577f, 5.539f, 5.473f, 5.525f,          // Sb … Nd
    5.582f, 5.644f, 5.670f, 6.150f, 5.864f, 5.939f, 6.022f, 6.108f, 6.184f, 6.254f,            // Pm … Yb
    5.426f, 6.825f, 7.550f, 7.864f, 7.834f, 8.438f, 8.967f, 8.959f, 9.226f, 10.438f,           // Lu … Hg
    6.108f, 7.417f, 7.286f, 8.414f, 9.318f, 10.749f, 4.073f, 5.278f, 5.17f, 6.307f,            // Tl … Th
    5.89f, 6.194f, 6.266f, 6.026f, 5.974f, 5.991f, 6.198f, 6.282f, 6.42f, 6.50f,               // Pa … Fm
    6.58f, 6.65f, 4.9f, 6.0f, 6.8f, 7.8f, 7.7f, 7.6f, 8.7f, 9.6f,                              // Md … Ds
    10.6f, 11.97f, 7.3f, 8.5f, 5.6f, 6.6f, 7.7f, 8.9f};                                        // Rg … Og
static double eaOfZ(int z) {
    switch (z) {
    case 1: return 0.754; case 3: return 0.618; case 5: return 0.277; case 6: return 1.262; case 8: return 1.461; case 9: return 3.401;
    case 11: return 0.548; case 13: return 0.433; case 14: return 1.390; case 15: return 0.746; case 16: return 2.077; case 17: return 3.613;
    case 19: return 0.501; case 22: return 0.079; case 23: return 0.525; case 24: return 0.666; case 26: return 0.151; case 27: return 0.662;
    case 28: return 1.156; case 29: return 1.235; case 31: return 0.43; case 32: return 1.233; case 33: return 0.804; case 34: return 2.021;
    case 35: return 3.364; case 37: return 0.486; case 41: return 0.893; case 42: return 0.746; case 44: return 1.05; case 45: return 1.137;
    case 46: return 0.562; case 47: return 1.302; case 50: return 1.112; case 51: return 1.047; case 52: return 1.971; case 53: return 3.059;
    case 55: return 0.472; case 74: return 0.816; case 76: return 1.1; case 77: return 1.564; case 78: return 2.128; case 79: return 2.309;
    case 82: return 0.364; case 83: return 0.942; case 84: return 1.9; case 85: return 2.4;
    case 2: case 4: case 7: case 10: case 12: case 18: case 25: case 30: case 36: case 48: case 54: case 80: case 86: return 0;
    }
    return 0.3;
}
static double waterR0 = 0.96;      // длина связи O–H, Å (меняется только в проверочном режиме --water)
static double ETA[NEL];            // химическая жёсткость η = I − A (эВ) по типу частицы
static bool CATMETAL[NEL];         // каталитически активные металлы (Fe, Co, Ni, Cu, Ru, Rh, Pd, Ag, Re, Os, Ir, Pt)
// Реальные энергии диссоциации (эВ), длины связей (Å) и ширины ям Морзе a = ω_e·√(μ/2D_e) (Å⁻¹; по колебательным
// постоянным двухатомных молекул и частотам валентных колебаний многоатомных) для распространённых пар —
// приоритет над общими правилами. Вызывается из initBondTable() после общего правила и повторно при сборке палитры.
static void initChemTables() {
    auto T = [](int z) { return typeOfZ(z); };
    auto sb = [](int a, int b, std::initializer_list<double> D, std::initializer_list<double> r, std::initializer_list<double> w = {}) {
        if (a >= 0 && b >= 0) setBond(a, b, D, r, w); };
    // водород
    sb(E_H, E_H, {4.52}, {0.74}, {1.94});              // H2: ω_e = 4401 см⁻¹
    sb(E_C, E_H, {4.30}, {1.09}, {1.90});              // C–H (метан 4.3 эВ на связь, 3000 см⁻¹)
    sb(E_N, E_H, {4.05}, {1.01}, {2.20});              // N–H (аммиак, среднее 4.0)
    sb(E_O, E_H, {4.80}, {waterR0}, {2.20});           // O–H (вода: среднее двух связей 4.8; первая 5.1; 3700 см⁻¹)
    sb(T(9), E_H, {5.87}, {0.92}, {2.22});             // H–F
    sb(E_CL, E_H, {4.43}, {1.27}, {1.87});             // H–Cl
    sb(T(35), E_H, {3.79}, {1.41}, {1.81});            // H–Br
    sb(T(53), E_H, {3.09}, {1.61}, {1.75});            // H–I
    sb(T(16), E_H, {3.78}, {1.34}, {1.87});            // S–H
    sb(T(15), E_H, {3.30}, {1.42}, {1.71});            // P–H
    sb(T(14), E_H, {3.30}, {1.48}, {1.62});            // Si–H
    sb(T(5), E_H, {3.40}, {1.19}, {1.76});             // B–H
    sb(T(11), E_H, {1.97}, {1.89}, {1.11});            // NaH
    sb(T(3), E_H, {2.43}, {1.60}, {1.13});             // LiH
    // углерод
    sb(E_C, E_C, {3.61, 6.36, 8.70}, {1.54, 1.34, 1.20}, {1.70, 2.13, 2.22});
    sb(E_C, E_N, {3.17, 6.30, 9.20}, {1.47, 1.28, 1.16}, {1.94, 2.27, 2.55});
    // C=O: 8.0 эВ — между карбонилом (7.5) и CO2 (8.3 на связь): одна кривая описывает и альдегиды, и CO2
    sb(E_C, E_O, {3.71, 8.00, 11.1}, {1.43, 1.20, 1.13}, {1.90, 2.19, 2.30});
    sb(E_C, T(9), {5.03}, {1.35}, {1.72});
    sb(E_C, E_CL, {3.40}, {1.77}, {1.61});
    sb(E_C, T(35), {2.90}, {1.94}, {1.55});
    sb(E_C, T(53), {2.40}, {2.14}, {1.44});
    sb(E_C, T(16), {2.80, 5.90}, {1.82, 1.56}, {1.67, 2.0});
    sb(E_C, T(14), {3.30}, {1.87}, {1.6});
    sb(E_C, T(15), {2.70}, {1.84}, {1.6});
    // азот, кислород, галогены
    sb(E_N, E_N, {1.70, 4.30, 9.79}, {1.45, 1.25, 1.10}, {2.9, 2.59, 2.69});   // N≡N: 2359 см⁻¹
    sb(E_N, E_O, {2.10, 6.50}, {1.40, 1.15}, {2.3, 2.75});   // N=O (NO: 6.5 эВ)
    sb(E_N, E_CL, {2.00}, {1.75}, {1.9});
    sb(E_O, E_O, {1.50, 5.12}, {1.48, 1.21}, {2.75, 2.65});  // O–O в пероксидах, O=O (O2: 5.12 эВ, 1580 см⁻¹)
    sb(E_O, E_CL, {2.40, 4.70}, {1.70, 1.44}, {2.44, 2.5});   // Cl=O — только у хлора с расширенным октетом (HClO4, ClO4−)
    sb(E_O, T(9), {1.90}, {1.42}, {2.4});
    sb(E_CL, E_CL, {2.48}, {1.99}, {2.02});            // Cl2: 560 см⁻¹
    sb(T(9), T(9), {1.60}, {1.42}, {2.97});
    sb(T(35), T(35), {1.99}, {2.28}, {1.98});
    sb(T(53), T(53), {1.54}, {2.67}, {1.86});
    // сера, кремний, фосфор
    sb(T(16), E_O, {2.80, 5.36}, {1.57, 1.48}, {2.12, 2.20});   // S=O (SO: 5.36 эВ)
    sb(T(16), T(16), {2.75, 4.37}, {2.05, 1.89}, {1.63, 1.9});
    sb(T(14), E_O, {4.70, 8.26}, {1.63, 1.51}, {2.0, 1.87});    // Si–O (кремнезём 4.7), Si=O (SiO: 8.26)
    sb(T(14), T(14), {2.35}, {2.35}, {1.5});
    sb(T(14), E_CL, {4.00}, {2.02}, {1.6});
    sb(T(15), E_O, {3.70, 7.80}, {1.63, 1.48}, {2.0, 2.2});   // P=O: подобрано по теплотам образования H3PO4 и POCl3
    sb(T(15), E_CL, {3.40}, {2.04}, {1.6});
    sb(T(15), T(9), {5.08}, {1.56}, {1.8});            // PF3
    sb(T(16), T(9), {3.39}, {1.56}, {1.9});
    sb(T(16), E_CL, {2.81}, {2.01}, {1.7});            // SCl2
    sb(T(14), T(9), {5.86}, {1.56}, {1.7});            // SiF4
    sb(T(34), E_H, {2.86}, {1.46}, {1.8});             // H2Se
    sb(T(32), E_H, {2.99}, {1.53}, {1.65});            // GeH4
    sb(T(33), E_H, {3.10}, {1.52}, {1.7});             // AsH3
    sb(E_N, T(9), {2.93}, {1.37}, {2.2});              // NF3
    // бор
    sb(T(5), T(9), {6.35}, {1.31}, {1.9});             // BF3
    sb(T(5), E_CL, {4.73}, {1.75}, {1.7});             // BCl3
    sb(T(5), E_O, {5.56}, {1.36}, {2.0});
    sb(T(5), E_C, {3.69}, {1.56}, {1.8});
    // межгалогенные
    sb(E_CL, T(9), {2.58}, {1.63}, {2.32});
    sb(T(35), E_CL, {2.26}, {2.14}, {1.99});
    sb(T(53), E_CL, {2.16}, {2.32}, {1.86});
    sb(T(53), T(35), {1.82}, {2.47}, {1.89});
    // благородные газы связываются только с F и O; здесь D — до вычета промотирования (у ксенона 1.3 эВ на связь)
    sb(T(54), T(9), {2.66}, {1.97}, {2.0});                        // XeF2: Xe–F 1.36 эВ (теплоты образования XeF2, XeF4)
    sb(T(54), E_O, {1.90, 3.79}, {1.90, 1.76}, {2.0, 2.2});         // XeO3 эндотермичен (+402 кДж/моль) — взрывчат
    sb(T(36), T(9), {2.49}, {1.89}, {2.0});                        // KrF2 эндотермичен: Kr–F всего 0.5 эВ
    // металл–неметалл (двухатомные молекулы): ионная связь — яма шире и мягче
    sb(T(11), E_CL, {4.23}, {2.36}, {0.90});           // NaCl (газ): 366 см⁻¹
    sb(T(19), E_CL, {4.43}, {2.67}, {0.78});           // KCl
    sb(T(11), T(9), {4.95}, {1.93}, {1.05});           // NaF
    sb(T(12), E_O, {3.70}, {1.75}, {1.7});             // MgO
    sb(T(20), E_O, {4.00}, {1.82}, {1.5});             // CaO
    sb(T(13), E_O, {5.30}, {1.62}, {1.8});             // AlO
    sb(T(22), E_O, {6.90}, {1.62}, {1.8});             // TiO
    sb(T(26), E_O, {4.17}, {1.62}, {1.8});             // FeO
    sb(T(29), E_O, {2.90}, {1.72}, {1.8});             // CuO
    sb(T(30), E_O, {2.80}, {1.70}, {1.8});             // ZnO
    // атомы на поверхности катализаторов: за вычетом ослабления металлической связи (W_BOND) дают справочные
    // энергии адсорбции — O на Pt(111) ≈ 4.3 эВ (мостик из двух связей), этилен на Ni(111) ≈ 0.8 эВ
    sb(T(78), E_O, {3.10}, {1.80}, {1.8});             // Pt–O
    sb(T(28), E_C, {2.50}, {1.90}, {1.7});             // Ni–C
    for (int t = 0; t < NEL; t++) {
        const int z = EL[t].Z;
        ETA[t] = z >= 1 && z <= 118 ? std::max(2.0, IE_Z[z] - eaOfZ(z)) : 10.0;
        CATMETAL[t] = false;
    }
    for (int z : {26, 27, 28, 29, 44, 45, 46, 47, 75, 76, 77, 78}) if (T(z) >= 0) CATMETAL[T(z)] = true;
}

static double catalystFactor(double x, double y, double z) {
    if (!P.catalyst) return 1.0;
    double dx = x - P.catX, dy = y - P.catY, dz = z - P.catZ;
    return (dx * dx + dy * dy + dz * dz < P.catR * P.catR) ? cfg::CAT_FACTOR : 1.0;
}
static inline bool inSet(const int* a, int n, int v) { for (int k = 0; k < n; k++) if (a[k] == v) return true; return false; }

// ===================================== МОЛЕКУЛЫ И ЗАРЯДЫ =================================
static std::vector<int> molMark; static int molStamp = 0;
static int newMolStamp() {
    if ((int)molMark.size() < S.n) molMark.resize(S.n, 0);
    if (++molStamp > 2000000000) { std::fill(molMark.begin(), molMark.end(), 0); molStamp = 1; }
    return molStamp;
}
// атомы молекул, содержащих затравки seeds (обход по связям); false — атомов больше cap («сетка»)
static bool gatherMol(const int* seeds, int ns, std::vector<int>& out, int cap) {
    const int st = newMolStamp(); out.clear();
    for (int s = 0; s < ns; s++) {
        int a0 = seeds[s]; if (a0 < 0 || molMark[a0] == st) continue;
        size_t k0 = out.size(); molMark[a0] = st; out.push_back(a0);
        for (size_t k = k0; k < out.size(); k++) {
            int a = out[k];
            for (int m = 0; m < S.nbc[a]; m++) { int b = S.nb[a][m]; if (molMark[b] != st) { molMark[b] = st; out.push_back(b); if ((int)out.size() > cap) return false; } }
        }
    }
    return true;
}
static inline int chargeUnits(double q) { return (int)std::lround(q / chem::ZION); }
static inline int typeFc(int t) { return EL[t].fq == 0 ? 0 : chargeUnits(EL[t].fq); }   // Na+ → +1, Cl− → −1
static int netCharge(const std::vector<int>& m) { double s = 0; for (int a : m) s += S.q[a]; return chargeUnits(s); }
// «обычная» молекула: нейтральна и без ионных центров — только такие участвуют в радикальных реакциях
static bool plainMol(int i) {
    static std::vector<int> m; int s[1] = {i};
    if (!gatherMol(s, 1, m, chem::MOL_CAP)) return true;   // большая сетка считается нейтральной
    if (netCharge(m) != 0) return false;
    for (int a : m) { const int t = S.ty[a]; if (EL[t].val > 0 && usedVal(a) > valMax(t)) return false; }
    return true;
}
// Формальные заряды атомов молекулы m с суммарным зарядом Q (в единицах иона): катионные центры — атомы
// с превышенной валентностью (O с тремя связями, N с четырьмя), анионные — самые электроотрицательные атомы
// со свободной валентностью; остаток положительного заряда — на наименее электроотрицательный атом («голый» протон)
static void siteFc(const std::vector<int>& m, int Q, std::vector<int>& fc) {
    fc.assign(m.size(), 0); int sum = 0;
    for (size_t k = 0; k < m.size(); k++) {
        int a = m[k], t = S.ty[a], tf = typeFc(t);
        if (tf) { fc[k] = tf; sum += tf; continue; }
        if (EL[t].val > 0) { int over = usedVal(a) - valMax(t); if (over > 0) { fc[k] = over; sum += over; } }
    }
    int rem = Q - sum;
    for (int guard = 0; rem < 0 && guard < 8; guard++) {
        int best = -1; double key = -1e9;
        for (size_t k = 0; k < m.size(); k++) {
            int a = m[k]; if (typeFc(S.ty[a]) || fc[k] > 0) continue;
            double v = EL[S.ty[a]].chi + (freeVal(a) + fc[k] > 0 ? 10 : 0); if (v > key) { key = v; best = (int)k; }
        }
        if (best < 0) break; fc[best]--; rem++;
    }
    for (int guard = 0; rem > 0 && guard < 8; guard++) {
        int best = -1; double key = 1e9;
        for (size_t k = 0; k < m.size(); k++) {
            int a = m[k]; if (typeFc(S.ty[a]) || fc[k] < 0) continue;
            double v = EL[S.ty[a]].chi; if (v < key) { key = v; best = (int)k; }
        }
        if (best < 0) break; fc[best]++; rem--;
    }
}
// Заряды атомов фрагмента f с суммарным зарядом Q (единицы иона): инкременты связей κ·Δχ + заряд иона:
// доля Q_LOC — на заряженном центре (O в H3O+, OH−; N в NH4+), остальное — по мягкости 1/η всех атомов (EEM).
// Так H3O+: O −0.68, H +0.49; OH−: O −1.12, H +0.32; NH4+: N −0.52, H +0.33 (в единицах модели, ион ±0.8).
static void assignCharges(const std::vector<int>& f, int Q) {
    int tsum = 0; double ssum = 0;
    for (int a : f) { updateCharge(a); int tf = typeFc(S.ty[a]); tsum += tf; if (!tf) ssum += 1.0 / ETA[S.ty[a]]; }
    const int ex = Q - tsum;
    if (ex == 0 || ssum <= 0) return;
    static std::vector<int> fc; siteFc(f, Q, fc);
    for (size_t k = 0; k < f.size(); k++) {
        int a = f[k]; if (typeFc(S.ty[a])) continue;
        S.q[a] += fc[k] * chem::ZION * chem::Q_LOC + ex * chem::ZION * (1 - chem::Q_LOC) / ETA[S.ty[a]] / ssum;
    }
}
static inline double bondTarget(int i, int j);
// пересчитать заряды всех молекул: заряд молекулы — Σq в единицах oldUnit (старые файлы), распределение — по правилам модели;
// сдвиги связей — к «энергии ионов» (теплота прошлых реакций сбрасывается)
static void rechargeAll(double oldUnit) {
    std::vector<char> seen(S.n, 0); std::vector<int> m;
    for (int i = 0; i < S.n; i++) {
        if (seen[i]) continue;
        int s[1] = {i}; gatherMol(s, 1, m, 1 << 30);
        double qs = 0; for (int a : m) { seen[a] = 1; qs += S.q[a]; }
        assignCharges(m, (int)std::lround(qs / oldUnit));
        for (int a : m) for (int k = 0; k < S.nbc[a]; k++) S.bc[a][k] = bondTarget(a, S.nb[a][k]);
    }
}
// гидроксид-ион: O–H с суммарным зарядом −1
static inline bool hydroxideO(int a) {
    if (S.ty[a] != E_O || S.nbc[a] != 1) return false;
    int h = S.nb[a][0]; return S.ty[h] == E_H && S.nbc[h] == 1 && S.q[a] + S.q[h] < -0.5 * chem::ZION;
}
// анион кислорода (OH−, O−, алкоголят): O с ≤ 1 связью в отрицательно заряженной молекуле
static bool anionO(int a) {
    if (S.ty[a] != E_O || S.nbc[a] > 1 || S.q[a] > -0.3) return false;
    static std::vector<int> m; int s[1] = {a};
    if (!gatherMol(s, 1, m, 16)) return false;
    return netCharge(m) < 0;
}
// энергия «особого состояния» атома a, приходящаяся на одну его связь: ион (H3O+, OH−, NH4+) или расширенный октет
// (промотирование электронов у SF6, PCl5…); сдвиги связей релаксируют не к нулю, а к ней
static inline double siteOffset(int a) {
    const int t = S.ty[a], nb = S.nbc[a];
    if (t == E_O) { if (nb == 3) return chem::E_OXONIUM / 3; if (nb == 1 && hydroxideO(a)) return chem::E_HYDROXIDE; return 0; }
    if (t == E_N) return nb == 4 ? chem::E_AMMONIUM / 4 : 0;
    if (HYPER[t] && nb > 0) { const int o = hyperOver(a); if (o > 0) return EPROM[t] * o / nb; }
    return 0;
}
static inline double bondTarget(int i, int j) { return siteOffset(i) + siteOffset(j); }
// степень окисления: электроны каждой связи отдаются более электроотрицательному атому (+ заряд иона)
static int oxidationState(int i) {
    const Element& e = EL[S.ty[i]]; int ox = typeFc(S.ty[i]);
    for (int k = 0; k < S.nbc[i]; k++) {
        const Element& f = EL[S.ty[S.nb[i][k]]]; int o = S.bo[i][k];
        if (f.chi > e.chi + 1e-9) ox += o; else if (f.chi < e.chi - 1e-9) ox -= o;
    }
    if (!typeFc(S.ty[i]) && S.nbc[i] == 0) ox += chargeUnits(S.q[i]);   // одноатомный ион
    return ox;
}
// формальный заряд атома (по молекуле)
static int formalCharge(int i) {
    static std::vector<int> m, fc; int s[1] = {i};
    if (!gatherMol(s, 1, m, chem::MOL_CAP)) return 0;
    siteFc(m, netCharge(m), fc);
    for (size_t k = 0; k < m.size(); k++) if (m[k] == i) return fc[k];
    return 0;
}

// ===================================== ЭНЕРГИЯ СОБЫТИЯ ===================================
static std::vector<int> lsMark; static int lsStamp = 0;
// Энергия, зависящая от топологии и зарядов атомов множества Pset (пары через список Верле, связи со сдвигами),
// углы центров, металлическая связь
static double localEnergy(const std::vector<int>& Pset, const std::vector<int>& centers) {
    if ((int)lsMark.size() < S.n) lsMark.resize(S.n, 0);
    if (++lsStamp > 2000000000) { std::fill(lsMark.begin(), lsMark.end(), 0); lsStamp = 1; }
    const int st = lsStamp;
    for (int a : Pset) lsMark[a] = st;
    double E = 0;
    for (int a : Pset) {
        for (int p = nlStart[a]; p < nlStart[a + 1]; p++) {
            int j = nlIdx[p];
            if (lsMark[j] == st && j < a) continue;
            E += pairEnergy(a, j, dist2(a, j));
        }
        for (int k = 0; k < S.nbc[a]; k++) {
            int b = S.nb[a][k]; if (lsMark[b] == st && b < a) continue;
            E += morseU(BT[S.ty[a]][S.ty[b]], S.bo[a][k], std::sqrt(dist2(a, b))) + S.bc[a][k];
        }
    }
    // валентные углы центров из множества C = Sa ∪ соседи Sa и π-члены связей атомов Pset
    for (int c : centers) E += angleEnergyAt(c);
    E += piEnergyOf(Pset);
    // металлическая связь: доля металличности меняется у атомов металла, получивших/потерявших связь
    E += metalEmbedLocal(centers);
    return E;
}
// собственная энергия ионов (ionSelfK, physics.inl) среди молекул, содержащих атомы P; «сетки» крупнее 64 атомов — нейтральны
static void componentsIn(const std::vector<int>& M, std::vector<std::vector<int>>& comps);
static double ionSelfOf(const std::vector<int>& P) {
    if (!anyCharge || P.empty()) return 0;
    static std::vector<int> M; static std::vector<std::vector<int>> comps;
    if (!gatherMol(P.data(), (int)P.size(), M, 4 * chem::MOL_CAP)) return 0;
    componentsIn(M, comps);
    double E = 0;
    for (auto& c : comps) {
        if ((int)c.size() > 64) continue;
        double q = 0; for (int a : c) q += S.q[a];
        const long Q = std::lround(q); if (Q) E -= ionSelfK() * Q * Q;
    }
    return E;
}
// Состояние атома (для отката отклонённого события)
struct TopoSave {
    int a; std::array<int, cfg::MAXB> nb; std::array<unsigned char, cfg::MAXB> bo; std::array<double, cfg::MAXB> bc;
    unsigned char nbc; std::array<int, 6> gh; unsigned char ghc; double q; int ty;
};
// Событие меняет топологию атомов Sa. ΔU = U_после − U_до считается точно (Морзе+углы+LJ+Кулон).
//  absA ≥ 0: ΔU поглощается сдвигом новой связи absA–absB (потенциальная энергия непрерывна);
//  absA < 0: ΔU немедленно компенсируется кинетической энергией участников.
//  qset — атомы, у которых apply() сам меняет заряды/типы/сдвиги связей (ионные события; иначе заряды Sa
//  пересчитываются по инкрементам связей); cextra — дополнительные центры для углов и водородных связей.
//  ghosts = false — разорванные пары сразу взаимодействуют (перенос протона: протон переставлен на равновесное расстояние);
//  extE — энергия во внешних полях (стенки, тяжесть, объекты поля) атомов, которые apply() перемещает.
template <class F>
static bool tryEvent(const int* Sa, int ns, F apply, int absA, int absB, double& dEout, double maxUp = 1e30,
                     const std::vector<int>* qset = nullptr, const std::vector<int>* cextra = nullptr,
                     bool ghosts = true, const std::function<double()>* extE = nullptr) {
    std::vector<int> C;
    for (int s = 0; s < ns; s++) { C.push_back(Sa[s]); for (int k = 0; k < S.nbc[Sa[s]]; k++) C.push_back(S.nb[Sa[s]][k]); }
    if (cextra) C.insert(C.end(), cextra->begin(), cextra->end());
    std::sort(C.begin(), C.end()); C.erase(std::unique(C.begin(), C.end()), C.end());
    std::vector<int> Pset(Sa, Sa + ns);
    if (qset) Pset.insert(Pset.end(), qset->begin(), qset->end());
    std::sort(Pset.begin(), Pset.end()); Pset.erase(std::unique(Pset.begin(), Pset.end()), Pset.end());
    // пары, исключённые до события: если исключение пропадёт на малом расстоянии — станут «призраками»
    std::vector<std::pair<int, int>> exb;
    for (int s = 0; s < ns; s++) {
        int a = Sa[s];
        for (int p = nlStart[a]; p < nlStart[a + 1]; p++) { int j = nlIdx[p]; if (mayExclude(a, j) && excluded(a, j)) exb.push_back({a, j}); }
    }
    std::vector<int> V = C; V.insert(V.end(), Pset.begin(), Pset.end()); for (auto& pr : exb) V.push_back(pr.second);
    std::sort(V.begin(), V.end()); V.erase(std::unique(V.begin(), V.end()), V.end());
    std::vector<TopoSave> sv(V.size());
    for (size_t k = 0; k < V.size(); k++) { int a = V[k]; sv[k] = {a, S.nb[a], S.bo[a], S.bc[a], S.nbc[a], S.gh[a], S.ghc[a], S.q[a], S.ty[a]}; }
    auto rollback = [&] { for (auto& t : sv) { S.nb[t.a] = t.nb; S.bo[t.a] = t.bo; S.bc[t.a] = t.bc; S.nbc[t.a] = t.nbc; S.gh[t.a] = t.gh; S.ghc[t.a] = t.ghc; S.q[t.a] = t.q; S.ty[t.a] = t.ty; } };

    double E0 = localEnergy(Pset, C) + ionSelfOf(Pset) + (extE ? (*extE)() : 0.0);
    // энергия «особых состояний» (расширенный октет) меняется вместе со связями: сдвиги связей центров
    // сразу переводятся к новым целям, и ΔU события её учитывает (у ионных событий это делает ionicCore)
    struct TRec { int a, b; double tg; };   // связь (a < b) и её цель
    auto bondsOfC = [&](std::vector<TRec>& out) {
        out.clear();
        for (int a : C) for (int k = 0; k < S.nbc[a]; k++) { const int b = S.nb[a][k]; out.push_back({std::min(a, b), std::max(a, b), 0.0}); }
        std::sort(out.begin(), out.end(), [](const TRec& p, const TRec& q) { return p.a != q.a ? p.a < q.a : p.b < q.b; });
        out.erase(std::unique(out.begin(), out.end(), [](const TRec& p, const TRec& q) { return p.a == q.a && p.b == q.b; }), out.end());
        for (auto& r : out) r.tg = bondTarget(r.a, r.b);
    };
    std::vector<TRec> oldT, newT;
    if (!qset) bondsOfC(oldT);
    apply();
    if (!qset) {
        for (int s = 0; s < ns; s++) updateCharge(Sa[s]);
        bondsOfC(newT);
        for (auto& r : newT) {
            double old = 0; for (auto& o : oldT) if (o.a == r.a && o.b == r.b) { old = o.tg; break; }
            if (r.tg == old) continue;
            const int ka = bondSlot(r.a, r.b), kb = bondSlot(r.b, r.a);
            if (ka >= 0) S.bc[r.a][ka] += r.tg - old; if (kb >= 0) S.bc[r.b][kb] += r.tg - old;
        }
    }
    if (ghosts) for (auto& pr : exb) if (!excluded(pr.first, pr.second)) {
        double s = 1.1225 * 0.5 * (EL[S.ty[pr.first]].sig + EL[S.ty[pr.second]].sig);
        if (dist2(pr.first, pr.second) < s * s) addGhost(pr.first, pr.second);
    }
    double E1 = localEnergy(Pset, C) + ionSelfOf(Pset) + (extE ? (*extE)() : 0.0);
    double dE = E1 - E0;
    dEout = dE;
    if (!std::isfinite(dE)) { rollback(); return false; }   // не-конечная энергия (перекрытие, NaN) — событие отклоняется
    if (absA >= 0) {
        if (dE > maxUp) { rollback(); return false; }   // энергии удара не хватает на такой рост потенциальной энергии
        int k1 = bondSlot(absA, absB), k2 = bondSlot(absB, absA);
        if (k1 >= 0 && k2 >= 0) { S.bc[absA][k1] -= dE; S.bc[absB][k2] -= dE; return true; }
    }
    double M = 0, px = 0, py = 0, pz = 0, K = 0;
    for (int s = 0; s < ns; s++) {
        int a = Sa[s]; double m = EL[S.ty[a]].m; M += m; px += m * S.vx[a]; py += m * S.vy[a]; pz += m * S.vz[a];
        K += 0.5 * m * (S.vx[a] * S.vx[a] + S.vy[a] * S.vy[a] + S.vz[a] * S.vz[a]);
    }
    double vcx = px / M, vcy = py / M, vcz = pz / M, Krel = K - 0.5 * M * (vcx * vcx + vcy * vcy + vcz * vcz);
    if (Krel - dE < 1e-4 || Krel < 1e-9) { rollback(); return false; }   // энергии не хватает — событие невозможно
    double sc = std::sqrt((Krel - dE) / Krel);
    for (int s = 0; s < ns; s++) { int a = Sa[s]; S.vx[a] = vcx + sc * (S.vx[a] - vcx); S.vy[a] = vcy + sc * (S.vy[a] - vcy); S.vz[a] = vcz + sc * (S.vz[a] - vcz); }
    return true;
}
// Теплота реакции: сдвиг связи c → c* (0 или энергия иона) с временем τ, энергия (c − c*)·(1−e^{−dt/τ})
// уходит в относительное движение пары
static void relaxBondOffsets(double dt) {
    const double f = 1 - std::exp(-dt / cfg::TAU_HEAT);
    for (int i = 0; i < S.n; i++) for (int k = 0; k < S.nbc[i]; k++) {
        int j = S.nb[i][k]; if (j < i) continue;
        const double tg = bondTarget(i, j);
        double c = S.bc[i][k] - tg; if (c == 0) continue;
        double delta = std::fabs(c) < 1e-6 ? c : c * f;
        double mi = EL[S.ty[i]].m, mj = EL[S.ty[j]].m, M = mi + mj, mu = mi * mj / M;
        double vrx = S.vx[j] - S.vx[i], vry = S.vy[j] - S.vy[i], vrz = S.vz[j] - S.vz[i];
        double Krel = 0.5 * mu * (vrx * vrx + vry * vry + vrz * vrz);
        if (delta < 0) delta = std::max(delta, -0.3 * Krel);   // эндотермика: берём энергию понемногу
        if (delta == 0) continue;
        double nx, ny, nz;
        if (Krel > 1e-12) { double s = std::sqrt((Krel + delta) / Krel); nx = s * vrx; ny = s * vry; nz = s * vrz; }
        else {
            if (delta < 0) continue;
            double dx, dy, dz; dvec(i, j, dx, dy, dz); double r = std::sqrt(dx * dx + dy * dy + dz * dz); if (r < 1e-9) continue;
            double v = std::sqrt(2 * delta / mu); nx = v * dx / r; ny = v * dy / r; nz = v * dz / r;
        }
        double vcx = (mi * S.vx[i] + mj * S.vx[j]) / M, vcy = (mi * S.vy[i] + mj * S.vy[j]) / M, vcz = (mi * S.vz[i] + mj * S.vz[j]) / M;
        S.vx[i] = vcx - mj / M * nx; S.vy[i] = vcy - mj / M * ny; S.vz[i] = vcz - mj / M * nz;
        S.vx[j] = vcx + mi / M * nx; S.vy[j] = vcy + mi / M * ny; S.vz[j] = vcz + mi / M * nz;
        int kj = bondSlot(j, i);
        S.bc[i][k] = tg + (c - delta); if (kj >= 0) S.bc[j][kj] = tg + (c - delta);
    }
}
static void addFlash(double x, double y, double z, double dE) {
    Flash f; f.x = x; f.y = y; f.z = z; f.age = 0; f.str = (float)clampv(std::fabs(dE) / cfg::EV, 0.25, 1.5);
    // выделение энергии — яркое белое пятно, поглощение — тусклое серое (и сжимающееся кольцо, см. drawScene)
    if (dE < 0) { f.r = f.g = f.b = 1.0f; } else { f.r = f.g = f.b = 0.5f; }
    if (flashes.size() < 400) flashes.push_back(f);
}
static void midpoint(int i, int j, double& mx, double& my, double& mz) {
    double dx, dy, dz; dvec(i, j, dx, dy, dz); mx = S.x[i] + 0.5 * dx; my = S.y[i] + 0.5 * dy; mz = S.z[i] + 0.5 * dz;
}

// ===================================== ФОРМУЛЫ И ЖУРНАЛ ==================================
// Брутто-формула: органика — система Хилла (C, H, затем по алфавиту), неорганика — по возрастанию χ (NaCl, H2O, SiO2)
static std::string formulaOf(const int* cnt) {
    std::vector<int> ts; bool hasC = cnt[E_C] > 0;
    for (int t = 0; t < NEL; t++) if (cnt[t] && !EL[t].fixed) ts.push_back(t);
    std::sort(ts.begin(), ts.end(), [hasC](int a, int b) {
        if (hasC) {
            int ka = a == E_C ? 0 : (a == E_H ? 1 : 2), kb = b == E_C ? 0 : (b == E_H ? 1 : 2);
            if (ka != kb) return ka < kb;
            return strcmp(EL[a].sym, EL[b].sym) < 0;
        }
        double ca = EL[a].chi > 0 ? EL[a].chi : 9.0, cb = EL[b].chi > 0 ? EL[b].chi : 9.0;   // благородные газы — в конце
        if (ca != cb) return ca < cb;
        return strcmp(EL[a].sym, EL[b].sym) < 0;
    });
    std::string s;
    for (int t : ts) { s += EL[t].sym; if (cnt[t] > 1) s += std::to_string(cnt[t]); }
    if (s == "HO") s = "OH";
    else if (s == "H3O") s = "H3O+";      // O с тремя связями — всегда оксоний
    else if (s == "H3N") s = "NH3";
    else if (s == "H2N") s = "NH2";
    else if (s == "HN") s = "NH";
    else if (s == "H4N") s = "NH4+";      // N с четырьмя связями — всегда аммоний
    else if (s == "NaHO") s = "NaOH";
    else if (s == "HKO") s = "KOH";
    return s;
}
// заряд иона в формуле: «+», «-», а двух- и более зарядный — через «^» (SO4^2-): цифра заряда не сливается с индексом
static std::string chargeSuffix(int Q) {
    if (Q == 0) return ""; if (Q == 1) return "+"; if (Q == -1) return "-";
    return Q > 0 ? fmt("^%d+", Q) : fmt("^%d-", -Q);
}
static std::string withCharge(std::string f, int Q) {
    if (Q == 0 || f.empty() || f.back() == '+' || f.back() == '-') return f;
    return f + chargeSuffix(Q);
}
// формула молекулы, содержащей атом a (с зарядом иона); большие сетки — «(сетка)»
static std::string molFormula(int a) {
    static std::vector<int> m; int s[1] = {a};
    if (!gatherMol(s, 1, m, 47)) return "(сетка)";
    int cnt[NEL] = {0}; bool ionType = false; double qs = 0;
    for (int k : m) { cnt[S.ty[k]]++; qs += S.q[k]; if (EL[S.ty[k]].fq != 0) ionType = true; }
    std::string f = formulaOf(cnt);
    return ionType ? f : withCharge(f, chargeUnits(qs));
}
static bool sameMolecule(int a, int b) {
    static std::vector<int> m; int s[1] = {a};
    gatherMol(s, 1, m, 400);
    return std::find(m.begin(), m.end(), b) != m.end();
}
// Журнал реакций: уравнение «реагенты → продукты», число событий, средняя теплота, времена событий (для скоростей)
struct RxStat { long long count = 0; double dH = 0, dHsum = 0; std::vector<std::string> R, Pr; std::vector<double> times; };
static std::map<std::string, RxStat> rxStats;
struct RxRecent { double t; std::string eq; double dH; };
static std::vector<RxRecent> rxRecent;
static double rxT0 = 0;               // начало учёта скоростей (сброс журнала)
static void logReaction(std::vector<std::string> R, std::vector<std::string> Pr, double dH) {
    std::sort(R.begin(), R.end()); std::sort(Pr.begin(), Pr.end());
    if (R == Pr) return;   // перестановка без изменения состава (Cl + Cl2 → Cl2 + Cl, H3O+ + H2O → H2O + H3O+) — не показываем
    std::string eq;
    for (size_t k = 0; k < R.size(); k++) { if (k) eq += " + "; eq += R[k]; }
    eq += " → ";
    for (size_t k = 0; k < Pr.size(); k++) { if (k) eq += " + "; eq += Pr[k]; }
    if (S.t < rxT0) rxT0 = S.t;
    if (rxStats.size() > 400) { rxStats.clear(); rxT0 = S.t; }   // защита от разрастания в «сетках»
    RxStat& s = rxStats[eq]; s.count++; s.dHsum += dH; s.dH = s.dHsum / s.count;
    if (s.R.empty()) { s.R = R; s.Pr = Pr; }
    s.times.push_back(S.t); if (s.times.size() > 3000) s.times.erase(s.times.begin(), s.times.begin() + 1000);
    rxRecent.push_back({S.t, eq, dH}); if (rxRecent.size() > 12) rxRecent.erase(rxRecent.begin());
}
// число событий реакции за последнее окно window (τ) → скорость, событий/τ
static double rxRate(const RxStat& s, double window, double* wOut = nullptr) {
    double w = std::max(1e-9, std::min(window, S.t - rxT0));
    int c = 0; for (auto it = s.times.rbegin(); it != s.times.rend() && *it >= S.t - w; ++it) c++;
    if (wOut) *wOut = w;
    return c / w;
}
static long long chemPT = 0;          // счётчик переносов протона (включая «невидимые» прыжки по Гроттгусу)

// ===================================== ИОННЫЕ СОБЫТИЯ ====================================
static inline int idxIn(const std::vector<int>& M, int a) { for (size_t k = 0; k < M.size(); k++) if (M[k] == a) return (int)k; return -1; }
// компоненты связности внутри множества M (M — объединение целых молекул)
static void componentsIn(const std::vector<int>& M, std::vector<std::vector<int>>& comps) {
    comps.clear(); std::vector<char> vis(M.size(), 0);
    for (size_t k0 = 0; k0 < M.size(); k0++) {
        if (vis[k0]) continue;
        std::vector<int> c{M[k0]}; vis[k0] = 1;
        for (size_t q = 0; q < c.size(); q++) {
            int a = c[q];
            for (int m = 0; m < S.nbc[a]; m++) { int k = idxIn(M, S.nb[a][m]); if (k >= 0 && !vis[k]) { vis[k] = 1; c.push_back(M[k]); } }
        }
        comps.push_back(c);
    }
}
// Общая часть ионных событий: формальные заряды до события → изменение топологии topo() → перенос формального
// заряда fcAdj(fc, M) → заряды фрагментов (EEM), превращения Cl ↔ Cl−, сдвиги связей ионов; ΔU точно через tryEvent
template <class Topo, class FcAdj>
static bool ionicCore(const int* Sa, int ns, Topo topo, FcAdj fcAdj, int absA, int absB, double maxUp, double& dE,
                      bool ghosts = true, const std::function<double()>* extE = nullptr) {
    std::vector<int> M;
    if (!gatherMol(Sa, ns, M, chem::MOL_CAP)) return false;
    std::vector<int> fc(M.size(), 0);
    {
        std::vector<std::vector<int>> comps; componentsIn(M, comps);
        std::vector<int> f;
        for (auto& c : comps) { siteFc(c, netCharge(c), f); for (size_t k = 0; k < c.size(); k++) fc[idxIn(M, c[k])] = f[k]; }
    }
    // сдвиги связей до события и их «ионные» цели
    struct BRec { int a, b; double tg; };
    std::vector<BRec> before;
    for (int a : M) for (int k = 0; k < S.nbc[a]; k++) { int b = S.nb[a][k]; if (b > a) before.push_back({a, b, bondTarget(a, b)}); }
    // водороды-доноры вокруг участников: у них меняются водородные связи при смене типа акцептора
    std::vector<int> Cx;
    for (int s = 0; s < ns; s++) for (int p = nlStart[Sa[s]]; p < nlStart[Sa[s] + 1]; p++) { int j = nlIdx[p]; if (S.ty[j] == E_H && hbDonor(j) >= 0) Cx.push_back(j); }
    auto apply = [&] {
        topo();
        fcAdj(fc, M);
        std::vector<std::vector<int>> comps; componentsIn(M, comps);
        for (auto& c : comps) {
            int Q = 0; for (int a : c) Q += fc[idxIn(M, a)];
            if (c.size() == 1) {   // одноатомные ионы хлора — отдельный тип частиц (Cl−)
                int a = c[0];
                if (S.ty[a] == E_CL && Q == -1) S.ty[a] = E_CLM;
                else if (S.ty[a] == E_CLM && Q == 0) S.ty[a] = E_CL;
            }
            assignCharges(c, Q);
        }
        for (int a : M) for (int k = 0; k < S.nbc[a]; k++) {
            int b = S.nb[a][k]; if (b < a) continue;
            double tg = bondTarget(a, b), old = 0;
            for (auto& r : before) if (r.a == a && r.b == b) { old = r.tg; break; }
            if (tg != old) { S.bc[a][k] += tg - old; int kb = bondSlot(b, a); if (kb >= 0) S.bc[b][kb] += tg - old; }
        }
    };
    bool ok = tryEvent(Sa, ns, apply, absA, absB, dE, maxUp, &M, &Cx, ghosts, extE);
    if (ok) {
        // выделившаяся теплота (избыток сдвигов связей над «энергией ионов») сразу распределяется по окрестности события
        // (≈ 1.5σ): иначе десятки ε ушли бы в колебание одной связи O–H и разорвали бы её. Энергия переходит точно,
        // импульс сохраняется (масштабирование скоростей относительно центра масс группы).
        double X = 0;
        for (int a : M) for (int k = 0; k < S.nbc[a]; k++) {
            int b = S.nb[a][k]; if (b < a) continue;
            double c = S.bc[a][k] - bondTarget(a, b); if (c <= 0) continue;
            X += c; S.bc[a][k] -= c; int kb = bondSlot(b, a); if (kb >= 0) S.bc[b][kb] -= c;
        }
        if (X > 0) {
            std::vector<int> G = M;
            for (int s = 0; s < ns; s++) for (int p = nlStart[Sa[s]]; p < nlStart[Sa[s] + 1]; p++) { int j = nlIdx[p]; if (dist2(Sa[s], j) < 2.25) G.push_back(j); }
            std::sort(G.begin(), G.end()); G.erase(std::unique(G.begin(), G.end()), G.end());
            double Mt = 0, px = 0, py = 0, pz = 0, K = 0;
            for (int a : G) { if (frozenAt(a)) continue; double m = EL[S.ty[a]].m; Mt += m; px += m * S.vx[a]; py += m * S.vy[a]; pz += m * S.vz[a]; K += 0.5 * m * (S.vx[a] * S.vx[a] + S.vy[a] * S.vy[a] + S.vz[a] * S.vz[a]); }
            double vcx = Mt > 0 ? px / Mt : 0, vcy = Mt > 0 ? py / Mt : 0, vcz = Mt > 0 ? pz / Mt : 0, Kr = K - 0.5 * Mt * (vcx * vcx + vcy * vcy + vcz * vcz);
            if (Kr > 1e-9) {
                double sc = std::sqrt((Kr + X) / Kr);
                for (int a : G) { if (frozenAt(a)) continue; S.vx[a] = vcx + sc * (S.vx[a] - vcx); S.vy[a] = vcy + sc * (S.vy[a] - vcy); S.vz[a] = vcz + sc * (S.vz[a] - vcz); }
            } else {   // покоящаяся группа: вернуть теплоту в сдвиги (отдадутся обычной релаксацией)
                for (int a : M) for (int k = 0; k < S.nbc[a]; k++) { int b = S.nb[a][k]; if (b > a) { S.bc[a][k] += X; int kb = bondSlot(b, a); if (kb >= 0) S.bc[b][kb] += X; X = 0; } }
            }
        }
        bool newType = false; for (int s = 0; s < ns; s++) if (!present[S.ty[Sa[s]]]) newType = true;
        if (newType) updatePresence();
        for (int a : M) if (a < (int)usedFlag.size()) usedFlag[a] = 1;
    }
    return ok;
}
// класс основания (акцептора протона): 2 — сильное (OH−, O−), 1 — вода, спирты, аммиак, 0 — слабое (Cl−), −1 — нет
static int baseClass(int a) {
    const int t = S.ty[a];
    if (t == E_O) {
        if (S.nbc[a] <= 1) return anionO(a) ? 2 : -1;
        return (S.nbc[a] == 2 && usedVal(a) == 2) ? 1 : -1;
    }
    if (t == E_N) return (S.nbc[a] == 3 && usedVal(a) == 3) ? 1 : -1;
    if (t == E_CLM) return 0;
    return -1;
}
// класс донора протона h, связанного с D: 3 — катион (H3O+, NH4+) или «голый» протон (D < 0), 2 — сильная кислота (HCl),
// 1 — вода/спирт (отдаёт протон только сильному основанию), 0 — нет
static int donorClass(int D) {
    if (D < 0) return 3;
    const int t = S.ty[D];
    if (t == E_O) { if (S.nbc[D] == 3) return 3; return (S.nbc[D] == 2 && usedVal(D) == 2) ? 1 : 0; }
    if (t == E_N) return S.nbc[D] == 4 ? 3 : 0;
    if (t == E_CL) return S.nbc[D] == 1 ? 2 : 0;
    return 0;
}
// число полярных соседей (O, N) — признак раствора: HCl ионизуется только в воде
static int polarNeighbors(int a, double rc) {
    int c = 0;
    for (int p = nlStart[a]; p < nlStart[a + 1]; p++) { int j = nlIdx[p]; if ((S.ty[j] == E_O || S.ty[j] == E_N) && !bonded(a, j) && dist2(a, j) < rc * rc) c++; }
    return c;
}
// энергия атома во внешних потенциалах (стенки 9-3, тяжесть, консервативные объекты поля) — для событий, двигающих атом
static double extEnergyOf(int i) {
    double E = 0; const Element& e = EL[S.ty[i]];
    double U, F, s = e.sig;
    if (!perAx(0)) { wallTerm(0, S.x[i], s, U, F); E += U; wallTerm(1, S.Lx - S.x[i], s, U, F); E += U; }
    if (!perAx(1)) { wallTerm(2, S.y[i], s, U, F); E += U; wallTerm(3, S.Ly - S.y[i], s, U, F); E += U; if (P.gravity != 0) E += e.m * P.gravity * S.y[i]; }
    if (!perAx(2)) { wallTerm(4, S.z[i], s, U, F); E += U; wallTerm(5, S.Lz - S.z[i], s, U, F); E += U; }
    { double d, nx, ny, nz; if (containerDist(S.x[i], S.y[i], S.z[i], d, nx, ny, nz)) { wallTerm9(d, s, P.wallAttr, U, F); E += U; } }
    for (auto& o : fieldObjs) if (o.on && foConservative(o.kind)) E += foConsAtom(o, i, nullptr);
    return E;
}
static bool ptListFresh = false;
// табличная энтальпия кислотно-основной стадии в водном растворе (ε) — для журнала. Сама динамика идёт с точным ΔU:
// энергия ионов в модели (кулон + «энергия иона» в сдвигах связей) подобрана так, чтобы HCl диссоциировал, а нейтрализация шла с выделением тепла
//   H3O+ + OH− → 2H2O: −56 кДж/моль; HCl + H2O → H3O+ + Cl−: −75; H3O+ + NH3 → NH4+ + H2O: −52; прыжки Гроттгуса: 0
static double ptRefDH(int D, int A, int dc, int bc, double dHbond) {
    const double k = cfg::EV / chem::KJ_PER_EV;   // кДж/моль → ε
    const bool dO = D >= 0 && S.ty[D] == E_O, dN = D >= 0 && S.ty[D] == E_N, aO = S.ty[A] == E_O, aN = S.ty[A] == E_N;
    if (dc == 3 && D >= 0) {
        if (bc == 2) return (dO ? -56 : -4) * k;
        if (bc == 0) return 75 * k;
        if (dO && aO) return 0; if (dO && aN) return -52 * k; if (dN && aO) return 52 * k; if (dN && aN) return 0;
    }
    if (dc == 2) { if (bc == 2) return -131 * k; if (aO) return -75 * k; if (aN) return -176 * k; }
    if (dc == 1 && bc == 2) return 0;
    return dHbond;
}
// отладка: попытки переноса протона по классам донор×основание (число, средняя и минимальная ΔU, принятые)
static long long ptDbgN[12], ptDbgOk[12], ptGate[6]; static double ptDbgSum[12], ptDbgMin[12] = {1e9, 1e9, 1e9, 1e9, 1e9, 1e9, 1e9, 1e9, 1e9, 1e9, 1e9, 1e9};
// Перенос протона h от донора D (D < 0 — «голый» протон) к основанию A. Самообмен (H3O+ + H2O → H2O + H3O+) —
// механизм Гроттгуса. Самоионизация воды запрещена правилами (в малом ящике даже один ион — это pH ≈ 1).
static bool tryProton(int D, int h, int A) {
    if (A == D || A == h || usedFlag[A] || bonded(A, h)) return false;
    const int dc = donorClass(D), bc = baseClass(A);
    if (dc == 0 || bc < 0 || (dc == 2 && bc < 1) || (dc == 1 && bc < 2)) return false;
    if (D >= 0 && bonded(A, D)) return false;
    ptGate[0]++;
    const int tA = S.ty[A] == E_CLM ? E_CL : S.ty[A];
    const BondT& bt = BT[tA][E_H]; if (bt.maxOrder == 0) return false;
    double dx, dy, dz; dvec(h, A, dx, dy, dz); double r2 = dx * dx + dy * dy + dz * dz;
    const double rf = bt.r0[1] + chem::R_PT; if (r2 > rf * rf) return false;
    const double r = std::sqrt(r2); if (r < 0.6 * bt.r0[1]) return false;
    ptGate[1]++;
    if (D >= 0) {   // D–H···A почти на одной линии
        double ux, uy, uz; dvec(h, D, ux, uy, uz); double lu = std::sqrt(ux * ux + uy * uy + uz * uz);
        if (lu < 1e-9 || (ux * dx + uy * dy + uz * dz) / (lu * r) > -0.7) return false;
    }
    const double vn = -((S.vx[A] - S.vx[h]) * dx + (S.vy[A] - S.vy[h]) * dy + (S.vz[A] - S.vz[h]) * dz) / r;
    ptGate[2]++;
    if (vn <= 0) return false;
    const double mh = EL[E_H].m, ma = EL[S.ty[A]].m, mu = mh * ma / (mh + ma), Eapp = 0.5 * mu * vn * vn;
    const double dH = (D >= 0 ? BT[S.ty[D]][E_H].D[1] : 0.0) - bt.D[1];   // ΔH по энергиям связей (без сольватации)
    double mx = S.x[h] + 0.5 * dx, my = S.y[h] + 0.5 * dy, mz = S.z[h] + 0.5 * dz;
    // барьер — по теплоте стадии в растворе (ptRefDH); к сильному основанию (OH−) — почти без барьера:
    // нейтрализация лимитирована диффузией (k ≈ 1.4·10¹¹ л/(моль·с))
    const double Ea = rxBarrier(chem::EA_PT * (bc == 2 ? 0.2 : 1.0) * catalystFactor(mx, my, mz) * P.eaScale, ptRefDH(D, A, dc, bc, dH));
    if (Eapp < Ea) return false;
    ptGate[3]++;
    if (dc == 2 && bc == 1 && S.ty[A] == E_O && (polarNeighbors(A, 1.3) < 2 || polarNeighbors(D, 1.35) < 2)) return false;   // HCl + H2O — только в растворе
    std::string rD = D >= 0 ? molFormula(D) : std::string("H+"), rA = molFormula(A);
    int Sa[3]; int ns = 0; if (D >= 0) Sa[ns++] = D; Sa[ns++] = h; Sa[ns++] = A;
    // протон переставляется на равновесное расстояние r0(A–H) по линии A→H (прыжок вдоль водородной связи);
    // энергия во внешних полях (стенки, тяжесть, объекты поля) входит в ΔU события
    const double hx = S.x[h], hy = S.y[h], hz = S.z[h], hux = S.ux[h], huy = S.uy[h], huz = S.uz[h];
    const double sh = bt.r0[1] / r - 1;   // смещение протона: (r0/r − 1)·(h − A)
    auto topo = [&] {
        if (D >= 0) changeBond(D, h, -1); if (S.ty[A] == E_CLM) S.ty[A] = E_CL; changeBond(A, h, +1);
        const double ddx = -dx * sh, ddy = -dy * sh, ddz = -dz * sh;   // (h − A) = −(dx, dy, dz)
        S.x[h] += ddx; S.y[h] += ddy; S.z[h] += ddz; S.ux[h] += ddx; S.uy[h] += ddy; S.uz[h] += ddz;
    };
    auto fcAdj = [&](std::vector<int>& fc, const std::vector<int>& M) { int kd = idxIn(M, D >= 0 ? D : h), ka = idxIn(M, A); if (kd >= 0) fc[kd]--; if (ka >= 0) fc[ka]++; };
    const std::function<double()> ext = [h] { return extEnergyOf(h); };
    // протон сдвигается до 0.3σ: «кожи» списка соседей (0.4σ) хватает, если атомы с его сборки сместились не больше чем на 0.05σ
    if (!ptListFresh) { if (nlMaxDisp2() > 0.05 * 0.05) buildNeighborList(); ptListFresh = true; }
    // Протон перескакивает мгновенно, а вода вокруг ещё повёрнута «под старые» заряды и геометрия ионов не подстроилась:
    // энергия сразу после прыжка выше, чем после перестройки (~0.1 пс). Этот временный избыток (до 5 эВ) не запрещает
    // прыжок — он записывается в сдвиг новой связи O–H и возвращается, когда вода и ионы подстроятся (энергия сохраняется)
    double dE = NAN;
    const bool ok = ionicCore(Sa, ns, topo, fcAdj, A, h, Eapp + 5.0 * cfg::EV, dE, false, &ext);
    if (std::isfinite(dE)) { int c = std::min(3, dc) * 3 + std::min(2, bc); ptDbgN[c]++; ptDbgSum[c] += dE; ptDbgMin[c] = std::min(ptDbgMin[c], dE); if (ok) ptDbgOk[c]++; }
    if (!ok) { S.x[h] = hx; S.y[h] = hy; S.z[h] = hz; S.ux[h] = hux; S.uy[h] = huy; S.uz[h] = huz; return false; }
    chemPT++; CH.exch++;
    const double dHr = ptRefDH(D, A, dc, bc, dH); CH.heat -= dHr;
    if (std::fabs(dHr) > 0.1 * cfg::EV) addFlash(mx, my, mz, dHr);
    std::vector<std::string> Pr; if (D >= 0) Pr.push_back(molFormula(D)); Pr.push_back(molFormula(A));
    logReaction({rD, rA}, Pr, dHr);
    return true;
}
// разрыв перерастянутой связи в заряженной молекуле: у катионного центра — гетеролитически
// (уходящая группа уносит положительный заряд: H3O+ → H2O + H+), у анионного заряд остаётся на месте
static bool ionicBreak(int i, int j, double Dij) {
    std::string before = molFormula(i);
    int Sa[2] = {i, j};
    auto topo = [&] { while (bonded(i, j)) changeBond(i, j, -1); };
    auto fcAdj = [&](std::vector<int>& fc, const std::vector<int>& M) {
        int ki = idxIn(M, i), kj = idxIn(M, j);
        for (int s = 0; s < 2; s++) {
            int a = s ? j : i, ka = s ? kj : ki, kb = s ? ki : kj;
            if (ka < 0 || kb < 0) continue;
            if (fc[ka] > 0 && !typeFc(S.ty[a]) && usedVal(a) <= EL[S.ty[a]].val) { fc[ka]--; fc[kb]++; }
        }
    };
    double dE;
    if (!ionicCore(Sa, 2, topo, fcAdj, -1, -1, 1e30, dE)) return false;
    CH.diss++; CH.heat -= Dij;
    double mx, my, mz; midpoint(i, j, mx, my, mz); addFlash(mx, my, mz, Dij);
    if (sameMolecule(i, j)) logReaction({before}, {molFormula(i)}, Dij);
    else logReaction({before}, {molFormula(i), molFormula(j)}, Dij);
    return true;
}
// кислотно-основные события: от катионов (H3O+, NH4+), кислот (HCl) и «голых» протонов к основаниям;
// сильные основания (OH−) забирают протон у соседней воды
static bool protonStep() {
    bool any = false; const int n = S.n; ptListFresh = false;
    for (int i = 0; i < n; i++) {
        if (usedFlag[i]) continue;
        const int t = S.ty[i];
        const bool cat = (t == E_O && S.nbc[i] == 3) || (t == E_N && S.nbc[i] == 4);
        const bool acid = t == E_CL && S.nbc[i] == 1 && S.ty[S.nb[i][0]] == E_H;
        if (cat || acid) {
            bool done = false;
            for (int k = 0; k < S.nbc[i] && !done; k++) {
                int h = S.nb[i][k]; if (S.ty[h] != E_H || S.nbc[h] != 1 || usedFlag[h]) continue;
                for (int p = nlStart[h]; p < nlStart[h + 1] && !done; p++) done = tryProton(i, h, nlIdx[p]);
            }
            any |= done; continue;
        }
        if (t == E_H && S.nbc[i] == 0 && S.q[i] > 0.5 * chem::ZION) {
            for (int p = nlStart[i]; p < nlStart[i + 1]; p++) if (tryProton(-1, i, nlIdx[p])) { any = true; break; }
            continue;
        }
        if (t == E_O && S.nbc[i] <= 1 && S.q[i] < -0.5 * chem::ZION && anionO(i)) {
            for (int p = nlStart[i]; p < nlStart[i + 1]; p++) {
                int h = nlIdx[p]; if (S.ty[h] != E_H || S.nbc[h] != 1 || usedFlag[h]) continue;
                int D = S.nb[h][0]; if (D == i || usedFlag[D]) continue;
                if (tryProton(D, h, i)) { any = true; break; }
            }
        }
    }
    return any;
}
// изменение энергии промотирования атома i, если сумма порядков его связей изменится на d
static inline double promDelta(int i, int d) {
    const int t = S.ty[i]; if (!HYPER[t]) return 0;
    const int u = usedVal(i), v = EL[t].val;
    return EPROM[t] * (std::max(0, u + d - v) - std::max(0, u - v));
}
// атом на поверхности металла-катализатора (сам металл, связан с ним или касается его)
static bool onCatSurface(int a) {
    const int ta = S.ty[a];
    if (CATMETAL[ta]) return true;
    for (int k = 0; k < S.nbc[a]; k++) if (CATMETAL[S.ty[S.nb[a][k]]]) return true;
    for (int p = nlStart[a]; p < nlStart[a + 1]; p++) {
        int j = nlIdx[p]; if (!CATMETAL[S.ty[j]]) continue;
        double s = 1.3 * 0.5 * (EL[ta].sig + EL[S.ty[j]].sig); if (dist2(a, j) < s * s) return true;
    }
    return false;
}

// ---- Поверхность металла-катализатора (механизм Ленгмюра — Хиншелвуда): молекула X–Y, коснувшись поверхности
// обоими концами, распадается на атомы, связанные с металлом (H2 и O2 на Pt, Ni), а два атома на поверхности
// соединяются друг с другом и уходят (H + O → OH, OH + H → H2O, этилен + H → этан). Каждая такая стадия —
// четыре атома сразу; барьер — собственный барьер переноса атома, сниженный поверхностью (CAT_SURF)
static int catPartner(int a, int ex) {   // ближайший касающийся атома a металл со свободной валентностью, кроме ex
    const int ta = S.ty[a]; int best = -1; double bd = 1e30;
    for (int p = nlStart[a]; p < nlStart[a + 1]; p++) {
        const int m = nlIdx[p]; if (m == ex || usedFlag[m] || !CATMETAL[S.ty[m]] || freeVal(m) <= 0 || bonded(a, m)) continue;
        const BondT& bt = BT[ta][S.ty[m]]; if (bt.maxOrder == 0 || S.nbc[m] >= cfg::MAXB) continue;
        const double r2 = dist2(a, m), rf = bt.r0[1] + cfg::R_FORM;
        if (r2 > rf * rf || r2 < 0.49 * bt.r0[1] * bt.r0[1]) continue;
        if (r2 < bd) { bd = r2; best = m; }
    }
    return best;
}
// «напряжение» связи a–b порядка o в текущей геометрии: насколько её энергия Морзе выше дна ямы (0 — нет связи).
// Событие меняет порядки связей мгновенно, а атомы ещё стоят по-старому; этот избыток вернётся, когда геометрия
// подстроится, поэтому в допустимый рост энергии события он не входит
static inline double morseStrain(int a, int b, int o) {
    if (o <= 0) return 0;
    const BondT& bt = BT[S.ty[a]][S.ty[b]]; if (o > bt.maxOrder) return 0;
    return morseU(bt, o, std::sqrt(dist2(a, b))) + bt.D[o];
}
static inline double approachE(int a, int b) {   // энергия сближения пары по линии центров (0, если расходятся)
    double dx, dy, dz; dvec(a, b, dx, dy, dz); const double r = std::sqrt(dx * dx + dy * dy + dz * dz); if (r < 1e-9) return 0;
    const double vn = -((S.vx[b] - S.vx[a]) * dx + (S.vy[b] - S.vy[a]) * dy + (S.vz[b] - S.vz[a]) * dz) / r;
    const double ma = EL[S.ty[a]].m, mb = EL[S.ty[b]].m;
    return vn > 0 ? 0.5 * ma * mb / (ma + mb) * vn * vn : 0;
}
// во сколько обходится металлу одна связь с неметаллом (для оценки теплоты поверхностной стадии): доля W_BOND/val
// его зонной энергии, а она у потенциала Гупты около 1.6 энергии когезии
static inline double metalBondCost(int m) { const Element& e = EL[S.ty[m]]; return e.val > 0 ? W_BOND / e.val * 1.6 * e.ecoh * cfg::EV : 0; }
static bool surfaceStep() {
    bool any = false; const int n = S.n;
    const double E0 = cfg::EA_EXCH * chem::CAT_SURF * P.eaScale;
    for (int x = 0; x < n; x++) {
        if (usedFlag[x] || CATMETAL[S.ty[x]] || EL[S.ty[x]].val <= 0) continue;
        // а) диссоциативная адсорбция: связь x–y рвётся, оба атома садятся на соседние атомы металла
        for (int k = 0; k < S.nbc[x] && !usedFlag[x]; k++) {
            const int y = S.nb[x][k]; if (y < x || usedFlag[y] || CATMETAL[S.ty[y]]) continue;
            const int m1 = catPartner(x, -1); if (m1 < 0) continue;
            const int m2 = catPartner(y, m1); if (m2 < 0) continue;
            const int o = S.bo[x][k]; const BondT& b = BT[S.ty[x]][S.ty[y]];
            const double dH = (b.D[o] - b.D[o - 1]) - BT[S.ty[x]][S.ty[m1]].D[1] - BT[S.ty[y]][S.ty[m2]].D[1] + metalBondCost(m1) + metalBondCost(m2);
            const double ea = approachE(x, m1) + approachE(y, m2);
            if (ea < rxBarrier(E0, dH)) continue;
            if (!plainMol(x)) continue;
            const std::string before = molFormula(x);
            const double strain = morseStrain(x, y, o - 1) - morseStrain(x, y, o) + morseStrain(x, m1, 1) + morseStrain(y, m2, 1);
            int Sa[4] = {x, y, m1, m2}; double dE;
            if (tryEvent(Sa, 4, [&] { changeBond(x, y, -1); changeBond(x, m1, +1); changeBond(y, m2, +1); }, x, m1, dE, ea + 0.2 * cfg::EV + std::max(0.0, strain))) {
                CH.exch++; CH.heat -= dH; usedFlag[x] = usedFlag[y] = usedFlag[m1] = usedFlag[m2] = 1; any = true;
                double mx, my, mz; midpoint(x, y, mx, my, mz); addFlash(mx, my, mz, dH);
                logReaction({before, EL[S.ty[m1]].sym}, {molFormula(x), molFormula(y)}, dH);
            }
        }
        if (usedFlag[x]) continue;
        // б) рекомбинация на поверхности: x и y сидят на металле, сближаются и связываются друг с другом
        int m1 = -1; for (int k = 0; k < S.nbc[x]; k++) if (CATMETAL[S.ty[S.nb[x][k]]]) { m1 = S.nb[x][k]; break; }
        if (m1 < 0 || usedFlag[m1]) continue;
        for (int p = nlStart[x]; p < nlStart[x + 1]; p++) {
            const int y = nlIdx[p]; if (y == m1 || usedFlag[y] || CATMETAL[S.ty[y]] || EL[S.ty[y]].val <= 0) continue;
            const BondT& b = BT[S.ty[x]][S.ty[y]]; const int o = bondOrder(x, y);
            if (b.maxOrder == 0 || o + 1 > b.maxOrder || (o == 0 && (S.nbc[x] > cfg::MAXB - 1 || S.nbc[y] > cfg::MAXB - 1))) continue;
            int m2 = -1; for (int k = 0; k < S.nbc[y]; k++) if (CATMETAL[S.ty[S.nb[y][k]]] && !usedFlag[S.nb[y][k]]) { m2 = S.nb[y][k]; break; }
            if (m2 < 0) continue;
            const double rf = b.r0[o + 1] + cfg::R_FORM, r2 = dist2(x, y); if (r2 > rf * rf || r2 < 0.49 * b.r0[1] * b.r0[1]) continue;
            const double dH = -(b.D[o + 1] - b.D[o]) + BT[S.ty[x]][S.ty[m1]].D[1] + BT[S.ty[y]][S.ty[m2]].D[1] - metalBondCost(m1) - metalBondCost(m2);
            const double ea = approachE(x, y); if (ea < rxBarrier(E0, dH)) continue;
            if (!plainMol(x) || !plainMol(y)) continue;
            const double strain = morseStrain(x, y, o + 1) - morseStrain(x, y, o) - morseStrain(x, m1, 1) - morseStrain(y, m2, 1);
            int Sa[4] = {x, y, m1, m2}; double dE;
            if (tryEvent(Sa, m1 == m2 ? 3 : 4, [&] { changeBond(x, m1, -1); changeBond(y, m2, -1); changeBond(x, y, +1); }, x, y, dE, ea + 0.2 * cfg::EV + std::max(0.0, strain))) {
                CH.assoc++; CH.heat -= dH; usedFlag[x] = usedFlag[y] = usedFlag[m1] = usedFlag[m2] = 1; any = true;
                double mx, my, mz; midpoint(x, y, mx, my, mz); addFlash(mx, my, mz, dH);
                logReaction({EL[S.ty[x]].sym, EL[S.ty[y]].sym}, {molFormula(x)}, dH);
                break;
            }
        }
    }
    return any;
}

// ===================================== ШАГ ХИМИИ =========================================
static bool chemistryStep() {
    const int n = S.n; bool any = false;
    usedFlag.assign(n, 0);
    if (rxStats.empty()) rxT0 = S.t;   // скорости реакций считаются с момента первого события после сброса журнала
    bool catMetal = false;
    if (P.surfCat) for (int t = 0; t < NEL; t++) if (present[t] && CATMETAL[t]) { catMetal = true; break; }
    // 0) «призраки» разошлись до минимума LJ → включить обычное взаимодействие (ΔU — в кинетическую энергию)
    for (int i = 0; i < n; i++) {
        if (!S.ghc[i] || usedFlag[i]) continue;
        for (int k = 0; k < S.ghc[i]; k++) {
            int j = S.gh[i][k]; if (j < i || usedFlag[j]) continue;
            double s = 1.1225 * 0.5 * (EL[S.ty[i]].sig + EL[S.ty[j]].sig);
            if (dist2(i, j) < s * s) continue;
            int Sa[2] = {i, j}; double dE;
            if (tryEvent(Sa, 2, [&] { removeGhost(i, j); }, -1, -1, dE)) { usedFlag[i] = usedFlag[j] = 1; any = true; break; }
        }
    }
    // 1) диссоциация перерастянутых связей
    for (int i = 0; i < n; i++) {
        if (usedFlag[i]) continue;
        for (int k = 0; k < S.nbc[i]; k++) {
            int j = S.nb[i][k]; if (j < i || usedFlag[j]) continue;
            const BondT& bt = BT[S.ty[i]][S.ty[j]]; int o = S.bo[i][k];
            double r = std::sqrt(dist2(i, j));
            if (r <= bt.r0[o] + cfg::R_BREAK) continue;
            const double Dij = bt.D[o];
            if (!plainMol(i)) { if (ionicBreak(i, j, Dij)) { any = true; break; } continue; }
            int Sa[2] = {i, j}; double dE;
            std::string before = molFormula(i);
            if (tryEvent(Sa, 2, [&] { while (bonded(i, j)) changeBond(i, j, -1); }, -1, -1, dE)) {
                CH.diss++; CH.heat -= Dij; usedFlag[i] = usedFlag[j] = 1; any = true;
                double mx, my, mz; midpoint(i, j, mx, my, mz); addFlash(mx, my, mz, Dij);
                if (sameMolecule(i, j)) logReaction({before}, {molFormula(i)}, Dij);
                else logReaction({before}, {molFormula(i), molFormula(j)}, Dij);
                break;
            }
        }
    }
    // 2) ассоциация и обмен (инициатор — атом со свободной валентностью, «радикал»)
    for (int i = 0; i < n; i++) {
        if (usedFlag[i] || freeVal(i) <= 0) continue;
        const int ti = S.ty[i];
        for (int p = nlStart[i]; p < nlStart[i + 1]; p++) {
            int j = nlIdx[p]; if (usedFlag[j]) continue;
            const int tj = S.ty[j]; const BondT& bt = BT[ti][tj];
            if (bt.maxOrder == 0) continue;
            double dx, dy, dz; dvec(i, j, dx, dy, dz); double r2 = dx * dx + dy * dy + dz * dz;
            double rf = bt.r0[1] + cfg::R_FORM; if (r2 > rf * rf) continue;
            double r = std::sqrt(r2);
            if (r < 0.7 * bt.r0[1]) continue;   // «призраки» могут перекрываться: связь не рождается внутри отталкивательного ядра
            double vn = -((S.vx[j] - S.vx[i]) * dx + (S.vy[j] - S.vy[i]) * dy + (S.vz[j] - S.vz[i]) * dz) / r;   // скорость сближения
            if (vn <= 0) continue;
            double mi = EL[ti].m, mj = EL[tj].m, mu = mi * mj / (mi + mj);
            double Eapp = 0.5 * mu * vn * vn;                                          // энергия удара по линии центров
            double mx = S.x[i] + 0.5 * dx, my = S.y[i] + 0.5 * dy, mz = S.z[i] + 0.5 * dz, cat = catalystFactor(mx, my, mz);
            int o = bondOrder(i, j);
            if (o + 1 > bt.maxOrder) continue;
            if (o == 0 && (S.nbc[i] >= cfg::MAXB || S.nbc[j] >= cfg::MAXB)) continue;
            // металлическая поверхность снижает собственный барьер (проверяется, только если это что-то решает)
            auto barrier = [&](double E0, double dH, double c) { return rxBarrier(E0 * c * P.eaScale, dH); };
            auto passes = [&](double E0, double dH) {
                if (Eapp >= barrier(E0, dH, cat)) return true;
                return catMetal && Eapp >= barrier(E0, dH, cat * chem::CAT_SURF) && (onCatSurface(i) || onCatSurface(j));
            };
            bool done = false;
            if (freeVal(j) > 0 || (o == 0 && canExpand(j, i))) {
                // ΔH по энергиям связей + промотирование электронов, если атом уходит за обычную валентность
                double dH = -(bt.D[o + 1] - bt.D[o]) + promDelta(i, +1) + promDelta(j, +1);
                if (!passes(cfg::EA_ASSOC, dH)) continue;
                if (!plainMol(i) || !plainMol(j)) continue;   // заряженные частицы реагируют переносом протона
                int Sa[2] = {i, j}; double dE;
                bool one = o > 0 || sameMolecule(i, j);
                std::string ri = molFormula(i), rj = one ? std::string() : molFormula(j);
                if (tryEvent(Sa, 2, [&] { changeBond(i, j, +1); }, i, j, dE, Eapp + 0.6 * (bt.D[o + 1] - bt.D[o]) + 0.2 * cfg::EV)) {
                    CH.assoc++; CH.heat -= dH; usedFlag[i] = usedFlag[j] = 1; any = true; done = true; addFlash(mx, my, mz, dH);
                    if (one) logReaction({ri}, {molFormula(i)}, dH); else logReaction({ri, rj}, {molFormula(i)}, dH);
                }
            } else {
                // из связей атома j уступает ту, где барьер ниже: кратная связь (присоединение) или простая (перенос атома)
                int best = -1; double bestH = 0, bestE0 = 0, bestEa = 1e30;
                for (int q = 0; q < S.nbc[j]; q++) {
                    int B = S.nb[j][q]; if (B == i || usedFlag[B]) continue;
                    int oB = S.bo[j][q]; const BondT& b2 = BT[tj][S.ty[B]];
                    double dH = (b2.D[oB] - b2.D[oB - 1]) - (bt.D[o + 1] - bt.D[o]) + promDelta(i, +1) + promDelta(B, -1);
                    const double E0 = oB >= 2 ? cfg::EA_ADD : rxIntrinsic(i, B), ea = barrier(E0, dH, cat);
                    if (ea < bestEa) { bestEa = ea; bestH = dH; bestE0 = E0; best = B; }
                }
                if (best < 0) continue;
                if (!passes(bestE0, bestH)) continue;
                if (!plainMol(i) || !plainMol(j)) continue;
                int B = best; int Sa[3] = {i, j, B}; double dE;
                bool one = sameMolecule(i, j);
                std::string ri = molFormula(i), rj = one ? std::string() : molFormula(j);
                if (tryEvent(Sa, 3, [&] { changeBond(j, B, -1); changeBond(i, j, +1); }, i, j, dE, Eapp + 0.6 * (bt.D[o + 1] - bt.D[o]) + 0.2 * cfg::EV)) {
                    CH.exch++; CH.heat -= bestH; usedFlag[i] = usedFlag[j] = usedFlag[B] = 1; any = true; done = true; addFlash(mx, my, mz, bestH);
                    std::vector<std::string> R = one ? std::vector<std::string>{ri} : std::vector<std::string>{ri, rj};
                    if (sameMolecule(i, B)) logReaction(R, {molFormula(i)}, bestH); else logReaction(R, {molFormula(i), molFormula(B)}, bestH);
                }
            }
            if (done) break;
        }
    }
    // 3) поверхность металла-катализатора: распад молекул на атомы и сборка атомов в молекулы
    if (catMetal && surfaceStep()) any = true;
    // 4) кислоты и основания: перенос протона
    if (P.acidBase && protonStep()) any = true;
    return any;
}
// «Вспышка света / искра»: каждая связь вблизи луча (d != nullptr) или точки o (d == nullptr)
// получает энергию D + 0.3 эВ вдоль оси связи (фотодиссоциация); энергия учитывается как внешняя работа
static void photoKick(int i, int k) {   // связь i–(k-й сосед) получает энергию D + 0.3 эВ вдоль своей оси
    const int j = S.nb[i][k];
    const BondT& bt = BT[S.ty[i]][S.ty[j]]; double Eph = bt.D[S.bo[i][k]] + 0.3 * cfg::EV;
    double dx, dy, dz; dvec(i, j, dx, dy, dz); double r = std::sqrt(dx * dx + dy * dy + dz * dz); if (r < 1e-9) return;
    double nx = dx / r, ny = dy / r, nz = dz / r;
    double mi = EL[S.ty[i]].m, mj = EL[S.ty[j]].m, mu = mi * mj / (mi + mj);
    double vn = (S.vx[j] - S.vx[i]) * nx + (S.vy[j] - S.vy[i]) * ny + (S.vz[j] - S.vz[i]) * nz;
    double vnew = std::sqrt(vn * vn + 2 * Eph / mu), dv = vnew - vn;   // ½μ(v'² − v²) = E_фотона
    S.vx[i] -= mu / mi * dv * nx; S.vy[i] -= mu / mi * dv * ny; S.vz[i] -= mu / mi * dv * nz;
    S.vx[j] += mu / mj * dv * nx; S.vy[j] += mu / mj * dv * ny; S.vz[j] += mu / mj * dv * nz;
    Wext += Eph;
    double mx, my, mz; midpoint(i, j, mx, my, mz); addFlash(mx, my, mz, -Eph);
}
static void lightFlash(const double* o, const double* d, double R) {
    for (int i = 0; i < S.n; i++) for (int k = 0; k < S.nbc[i]; k++) {
        int j = S.nb[i][k]; if (j < i) continue;
        double mx, my, mz; midpoint(i, j, mx, my, mz);
        double r2 = d ? rayDist2(mx, my, mz, o, d) : (mx - o[0]) * (mx - o[0]) + (my - o[1]) * (my - o[1]) + (mz - o[2]) * (mz - o[2]);
        if (r2 > R * R) continue;
        photoKick(i, k);
    }
}
// Избирательный фотолиз: свет поглощают только связи ta–tb (Cl2 — в ближнем УФ, а CH4 и HCl для него прозрачны;
// пероксид-инициатор — по слабой связи O–O). Каждая такая связь возбуждается с вероятностью frac; возвращает их число
static int photolyze(int ta, int tb, double frac) {
    int c = 0;
    for (int i = 0; i < S.n; i++) for (int k = 0; k < S.nbc[i]; k++) {
        int j = S.nb[i][k]; if (j < i) continue;
        if (!((S.ty[i] == ta && S.ty[j] == tb) || (S.ty[i] == tb && S.ty[j] == ta)) || urand() >= frac) continue;
        photoKick(i, k); c++;
    }
    return c;
}

// ===================================== СОСТАВ, pH =========================================
// Состав смеси: формулы молекул (с зарядами ионов) и их число; заодно число ионов H3O+/OH− и молекул воды
struct ChemComp { std::vector<std::pair<std::string, int>> list; int nH3O = 0, nOH = 0, nWater = 0, nMol = 0; double t = -1; };
static ChemComp chemComp;
static void chemComposition(ChemComp& out) {
    const int n = S.n; std::vector<int> par(n); for (int i = 0; i < n; i++) par[i] = i;
    auto find = [&](int a) { while (par[a] != a) { par[a] = par[par[a]]; a = par[a]; } return a; };
    for (int i = 0; i < n; i++) for (int k = 0; k < S.nbc[i]; k++) { int a = find(i), b = find(S.nb[i][k]); if (a != b) par[a] = b; }
    std::vector<std::pair<int, int>> ra; ra.reserve(n);
    for (int i = 0; i < n; i++) if (!EL[S.ty[i]].fixed) ra.push_back({find(i), i});
    std::sort(ra.begin(), ra.end());
    std::map<std::string, int> cnt; out = ChemComp(); out.t = S.t;
    int c[NEL];
    for (size_t k = 0; k < ra.size();) {
        size_t e = k; std::fill(c, c + NEL, 0); double qs = 0; bool ionType = false;
        while (e < ra.size() && ra[e].first == ra[k].first) { int a = ra[e].second; c[S.ty[a]]++; qs += S.q[a]; if (EL[S.ty[a]].fq != 0) ionType = true; e++; }
        std::string f = formulaOf(c); if (!ionType) f = withCharge(f, chargeUnits(qs));
        cnt[f]++; out.nMol++; k = e;
    }
    for (auto& kv : cnt) out.list.push_back({kv.first, kv.second});
    std::sort(out.list.begin(), out.list.end(), [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
    auto get = [&](const char* s) { auto it = cnt.find(s); return it == cnt.end() ? 0 : it->second; };
    out.nH3O = get("H3O+") + get("H+"); out.nOH = get("OH-"); out.nWater = get("H2O");
}
// объём ящика в литрах (σ = 0.3405 нм)
static double boxLiters() { return boxVolume() * std::pow(cfg::U_L_NM, 3) * 1e-24; }
static inline double molPerL(double N) { return N / (6.02214076e23 * std::max(1e-40, boxLiters())); }
// pH по числу ионов H3O+ и OH− в объёме ящика (избыток — остальное нейтрализуется); без ионов — 7
static double chemPH(const ChemComp& c, bool* valid = nullptr) {
    if (valid) *valid = c.nWater > 0;
    int ex = c.nH3O - c.nOH;
    if (ex > 0) return -std::log10(molPerL(ex));
    if (ex < 0) return 14 + std::log10(molPerL(-ex));
    return 7.0;
}

// ===================================== MOLECULES / LATTICES ============================
struct TAtom { int t; double x, y, z; int fc = 0; };   // fc — формальный заряд атома шаблона (ионы)
struct Tmpl { std::string label; std::vector<TAtom> a; std::vector<std::array<int, 3>> b; int tool = 0; };  // tool: 0 атомы, 1 стенка
static std::vector<Tmpl> palette;
static int customType = -1;            // элемент, выбранный в таблице Менделеева (первая ячейка палитры)
static int chemStampMol = -1;          // выбранная молекула библиотеки (≥ 0: первая ячейка палитры — она)
static bool buildLibTmpl(int idx, Tmpl& m);
static double r0of(int a, int b, int o) { return BT[a][b].r0[o]; }
static void initPalette() {
    initChemTables();
    palette.clear();
    auto atom = [](const char* l, int t) { Tmpl m; m.label = l; m.a = {{t, 0, 0, 0}}; return m; };
    if (customType < 0) customType = typeOfZ(26);   // по умолчанию — железо
    palette.push_back(atom(EL[customType].sym, customType));
    palette.push_back(atom("Ar", E_AR)); palette.push_back(atom("Ne", E_NE)); palette.push_back(atom("H", E_H));
    palette.push_back(atom("O", E_O)); palette.push_back(atom("C", E_C)); palette.push_back(atom("N", E_N));
    palette.push_back(atom("Na+", E_NA)); palette.push_back(atom("Cl", E_CL)); palette.push_back(atom("Cl-", E_CLM));
    auto di = [](const char* l, int t1, int t2, int o) { Tmpl m; m.label = l; double r = r0of(t1, t2, o); m.a = {{t1, -r / 2, 0, 0}, {t2, r / 2, 0, 0}}; m.b = {{0, 1, o}}; return m; };
    palette.push_back(di("H2", E_H, E_H, 1)); palette.push_back(di("O2", E_O, E_O, 2)); palette.push_back(di("N2", E_N, E_N, 3));
    palette.push_back(di("Cl2", E_CL, E_CL, 1)); palette.push_back(di("HCl", E_H, E_CL, 1));
    { Tmpl m; m.label = "H2O"; double r = r0of(E_O, E_H, 1), h = 104.5 / 2 * PI / 180;
      m.a = {{E_O, 0, 0, 0}, {E_H, r * std::cos(h), r * std::sin(h), 0}, {E_H, r * std::cos(h), -r * std::sin(h), 0}}; m.b = {{0, 1, 1}, {0, 2, 1}}; palette.push_back(m); }
    { Tmpl m; m.label = "CO2"; double r = r0of(E_C, E_O, 2); m.a = {{E_C, 0, 0, 0}, {E_O, -r, 0, 0}, {E_O, r, 0, 0}}; m.b = {{0, 1, 2}, {0, 2, 2}}; palette.push_back(m); }
    { Tmpl m; m.label = "CH4"; double s = r0of(E_C, E_H, 1) / std::sqrt(3.0);   // тетраэдр
      m.a = {{E_C, 0, 0, 0}, {E_H, s, s, s}, {E_H, s, -s, -s}, {E_H, -s, s, -s}, {E_H, -s, -s, s}};
      m.b = {{0, 1, 1}, {0, 2, 1}, {0, 3, 1}, {0, 4, 1}}; palette.push_back(m); }
    { Tmpl m; m.label = "NH3"; double r = r0of(E_N, E_H, 1), h = r * 0.37, q = std::sqrt(r * r - h * h);   // пирамида
      m.a = {{E_N, 0, 0, 0}, {E_H, q, -h, 0}, {E_H, -q / 2, -h, q * 0.866}, {E_H, -q / 2, -h, -q * 0.866}};
      m.b = {{0, 1, 1}, {0, 2, 1}, {0, 3, 1}}; palette.push_back(m); }
    { Tmpl m; m.label = "NaCl"; m.a = {{E_NA, -0.46, 0, 0}, {E_CLM, 0.46, 0, 0}}; palette.push_back(m); }
    palette.push_back(atom("Big", E_BIG));
    { Tmpl m; m.label = "Стена"; m.a = {{E_WALL, 0, 0, 0}}; m.tool = 1; palette.push_back(m); }
    // выбранная молекула библиотеки занимает первую ячейку
    if (chemStampMol >= 0) { Tmpl m; if (buildLibTmpl(chemStampMol, m)) palette[0] = m; else chemStampMol = -1; }
}
static int findPal(const char* l) { for (size_t k = 1; k < palette.size(); k++) if (palette[k].label == l) return (int)k; return 1; }
// выбрать элемент таблицы Менделеева как «кисть»
static void setCustomElement(int t) {
    customType = t; chemStampMol = -1;
    palette[0].label = EL[t].sym; palette[0].a = {{t, 0, 0, 0}}; palette[0].b.clear(); palette[0].tool = 0;
}

// случайный поворот, равномерный по сфере (кватернион Шумейка)
static void randomRotation(double R[9]) {
    double u1 = urand(), u2 = urand(), u3 = urand();
    double qx = std::sqrt(1 - u1) * std::sin(2 * PI * u2), qy = std::sqrt(1 - u1) * std::cos(2 * PI * u2);
    double qz = std::sqrt(u1) * std::sin(2 * PI * u3), qw = std::sqrt(u1) * std::cos(2 * PI * u3);
    double M[9] = {1 - 2 * (qy * qy + qz * qz), 2 * (qx * qy - qz * qw), 2 * (qx * qz + qy * qw),
                   2 * (qx * qy + qz * qw), 1 - 2 * (qx * qx + qz * qz), 2 * (qy * qz - qx * qw),
                   2 * (qx * qz - qy * qw), 2 * (qy * qz + qx * qw), 1 - 2 * (qx * qx + qy * qy)};
    memcpy(R, M, sizeof(M));
}
static bool overlaps(int t, double x, double y, double z, double fac) {
    const bool per = !(P.perMask == 0 || P.boundary == B_PISTON);
    for (int j = 0; j < S.n; j++) {
        double dx = S.x[j] - x, dy = S.y[j] - y, dz = S.z[j] - z;
        if (per) minImage3(dx, dy, dz);
        // σ_ij с учётом NBFIX, но не ближе 1.5 Å: у водорода и ядра WCA σ мало, а вплотную к чужому атому — огромная энергия
        double s2 = std::max(PT[t][S.ty[j]].sig2 * fac * fac, 0.44 * 0.44);
        if (dx * dx + dy * dy + dz * dz < s2) return true;
    }
    return false;
}
static void thermalVel(int t, double T, double& vx, double& vy, double& vz);
// Поставить молекулу по шаблону; возвращает false при перекрытии или выходе за стенки.
// Заряды: инкременты связей + заряд иона (формальные заряды шаблона) по мягкости атомов; сдвиги связей ионов — их энергия.
static bool placeMol(const Tmpl& m, double cx, double cy, double cz, double T, double fac = 0.8) {
    double R[9]; randomRotation(R);
    std::vector<std::array<double, 3>> pos;
    for (auto& a : m.a) {
        double x = cx + R[0] * a.x + R[1] * a.y + R[2] * a.z, y = cy + R[3] * a.x + R[4] * a.y + R[5] * a.z, z = cz + R[6] * a.x + R[7] * a.y + R[8] * a.z;
        wrapPoint(x, y, z);
        {   // у стенок (оси без периодичности) — отступ, в сосуде-шаре или цилиндре — внутри него
            const double mg = 0.6 * EL[a.t].sig;
            if ((!perAx(0) && (x < mg || x > S.Lx - mg)) || (!perAx(1) && (y < mg || y > S.Ly - mg)) || (!perAx(2) && (z < mg || z > S.Lz - mg))) return false;
            double d, nx, ny, nz; if (containerDist(x, y, z, d, nx, ny, nz) && d < mg) return false;
        }
        if (overlaps(a.t, x, y, z, fac)) return false;
        pos.push_back({x, y, z});
    }
    // скорости — тепловые у каждого атома: поступательное, вращательное и колебательное движение сразу при температуре T
    int base = S.n;
    for (size_t k = 0; k < m.a.size(); k++) {
        double vx, vy, vz; thermalVel(m.a[k].t, T, vx, vy, vz);
        addAtom(m.a[k].t, pos[k][0], pos[k][1], pos[k][2], vx, vy, vz);
    }
    for (auto& b : m.b) for (int o = 0; o < b[2]; o++) changeBond(base + b[0], base + b[1], +1);
    bool ionic = false; for (auto& a : m.a) if (a.fc) ionic = true;
    if (!ionic) { for (size_t k = 0; k < m.a.size(); k++) updateCharge(base + (int)k); return true; }
    std::vector<int> M2; for (size_t k = 0; k < m.a.size(); k++) M2.push_back(base + (int)k);
    std::vector<std::vector<int>> comps; componentsIn(M2, comps);
    for (auto& c : comps) {
        int Q = 0; for (int a : c) Q += m.a[a - base].fc + typeFc(S.ty[a]);
        assignCharges(c, Q);
    }
    for (int a : M2) for (int k = 0; k < S.nbc[a]; k++) { int b = S.nb[a][k]; S.bc[a][k] = bondTarget(a, b); }
    return true;
}
static int fillRandom(const Tmpl& m, int count, double x0, double y0, double z0, double x1, double y1, double z1, double T, double fac = 0.9) {
    int placed = 0, tries = 0;
    while (placed < count && tries < count * 60) {
        tries++;
        double z = z0 + urand() * (z1 - z0);
        if (placeMol(m, x0 + urand() * (x1 - x0), y0 + urand() * (y1 - y0), z, T, fac)) placed++;
    }
    return placed;
}
static int fillBox(const Tmpl& m, int count, double T, double margin = 1.0, double fac = 0.9) {
    return fillRandom(m, count, margin, margin, margin, S.Lx - margin, S.Ly - margin, S.Lz - margin, T, fac);
}
// Жидкость настоящей плотности: случайной вставкой до неё не добраться (у воды — одна молекула на 30 Å³),
// поэтому молекулы ставятся в узлы кубической сетки в случайном порядке и со случайным поворотом;
// сетка расплывается за доли пикосекунды. Возвращает число поставленных молекул.
static int fillGrid(const Tmpl& m, int count, double x0, double y0, double z0, double x1, double y1, double z1, double T, double fac = 0.55) {
    if (count <= 0) return 0;
    const double Lx = x1 - x0, Ly = y1 - y0, Lz = z1 - z0;
    double a = std::cbrt(Lx * Ly * Lz / count);
    int nx, ny, nz;
    for (;;) {
        nx = std::max(1, (int)std::floor(Lx / a)); ny = std::max(1, (int)std::floor(Ly / a)); nz = std::max(1, (int)std::floor(Lz / a));
        if ((long long)nx * ny * nz >= count) break;
        a *= 0.98;
    }
    std::vector<int> sites(nx * ny * nz);
    for (int k = 0; k < (int)sites.size(); k++) sites[k] = k;
    std::shuffle(sites.begin(), sites.end(), rng);
    const double ax = Lx / nx, ay = Ly / ny, az = Lz / nz;
    int placed = 0;
    for (int s : sites) {
        if (placed >= count) break;
        const int i = s % nx, j = (s / nx) % ny, k = s / (nx * ny);
        for (int tr = 0; tr < 6; tr++)   // поворот, при котором молекула не задевает соседей
            if (placeMol(m, x0 + (i + 0.5) * ax, y0 + (j + 0.5) * ay, z0 + (k + 0.5) * az, T, fac)) { placed++; break; }
    }
    return placed;
}
// число молекул воды в объёме V (σ³) при плотности 1 г/см³: одна молекула на 29.9 Å³
static int waterMolecules(double V) { return (int)std::lround(V * 39.476 / 29.915); }
static void thermalVel(int t, double T, double& vx, double& vy, double& vz) {
    double s = std::sqrt(T / EL[t].m); vx = grand() * s; vy = grand() * s; vz = grand() * s;
}
// Искра разряда: в шаре радиуса R газ превращается в плазму — связи рвутся (каждая получает D + 0.3 эВ, как при
// фотодиссоциации), атомы получают добавочные тепловые скорости при температуре Tk. Вся энергия — внешняя работа
static void spark(const double* c, double R, double Tk) {
    lightFlash(c, nullptr, R);
    double v2max = 0;
    for (int i = 0; i < S.n; i++) {
        if (frozenAt(i)) continue;
        const double dx = S.x[i] - c[0], dy = S.y[i] - c[1], dz = S.z[i] - c[2];
        if (dx * dx + dy * dy + dz * dz > R * R) continue;
        const double m = EL[S.ty[i]].m, k0 = S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i];
        double vx, vy, vz; thermalVel(S.ty[i], Tk, vx, vy, vz);
        S.vx[i] += vx; S.vy[i] += vy; S.vz[i] += vz;
        const double k1 = S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i];
        Wext += 0.5 * m * (k1 - k0); v2max = std::max(v2max, k1);
    }
    // шаг — сразу под раскалённые атомы (как в mdStep): иначе первые шаги после искры идут с «холодным» dt и теряют энергию
    if (v2max > 0) P.dt = std::min(P.dt, std::max(P.dtBase / 64, 0.85 * (respaOn && anyBondable ? 0.05 : 0.02) / std::sqrt(v2max)));
}
// решётки Браве и ГПУ: элементарная ячейка + базис (дробные координаты)
enum { L_FCC, L_HCP, L_BCC, L_SC, L_NACL, L_ICE };
static const char* LAT_NAMES[] = {"ГЦК", "ГПУ", "ОЦК", "ПК", "NaCl", "лёд"};
static void lattice3D(int kind, int t, int t2, double frac2, double x0, double y0, double z0, int nx, int ny, int nz, double a, double T) {
    std::vector<std::array<double, 3>> basis; double cx = a, cy = a, cz = a;
    switch (kind) {
    case L_SC: basis = {{0, 0, 0}}; break;
    case L_BCC: basis = {{0, 0, 0}, {0.5, 0.5, 0.5}}; break;
    case L_FCC: basis = {{0, 0, 0}, {0.5, 0.5, 0}, {0.5, 0, 0.5}, {0, 0.5, 0.5}}; break;
    case L_HCP: cy = std::sqrt(3.0) * a; cz = std::sqrt(8.0 / 3.0) * a;   // орторомбическая ячейка ГПУ, укладка ABAB вдоль z
        basis = {{0, 0, 0}, {0.5, 0.5, 0}, {0.5, 1.0 / 6, 0.5}, {0, 2.0 / 3, 0.5}}; break;
    case L_NACL: basis = {{0, 0, 0}}; break;   // простая кубическая с чередованием зарядов (a — расстояние Na–Cl)
    }
    for (int k = 0; k < nz; k++) for (int j = 0; j < ny; j++) for (int i = 0; i < nx; i++) for (auto& b : basis) {
        double x = x0 + (i + b[0] + 0.25) * cx, y = y0 + (j + b[1] + 0.25) * cy, z = z0 + (k + b[2] + 0.25) * cz;
        int tt = t;
        if (kind == L_NACL) tt = ((i + j + k) & 1) ? t2 : t;
        else if (t2 >= 0 && urand() < frac2) tt = t2;
        double vx, vy, vz; thermalVel(tt, T, vx, vy, vz);
        addAtom(tt, x, y, z, vx, vy, vz);
    }
}
static double latticeCellX(int kind, double a) { (void)kind; return a; }
static double latticeCellY(int kind, double a) { return kind == L_HCP ? std::sqrt(3.0) * a : a; }
static double latticeCellZ(int kind, double a) { return kind == L_HCP ? std::sqrt(8.0 / 3.0) * a : a; }
// Кубический лёд Ic: кислород в решётке алмаза, водороды по правилам Бернала–Фаулера
// (на каждой связи O···O ровно один H, у каждого O два своих H). Подрешётка A отдаёт H по d1,d2, B — по −d3,−d4.
static void iceLattice3D(double x0, double y0, double z0, int nx, int ny, int nz, double a, double T) {
    const double fcc[4][3] = {{0, 0, 0}, {0, 0.5, 0.5}, {0.5, 0, 0.5}, {0.5, 0.5, 0}};
    const double d[4][3] = {{1, 1, 1}, {1, -1, -1}, {-1, 1, -1}, {-1, -1, 1}};
    const double rOH = r0of(E_O, E_H, 1), s3 = 1.0 / std::sqrt(3.0);
    for (int k = 0; k < nz; k++) for (int j = 0; j < ny; j++) for (int i = 0; i < nx; i++) for (int f = 0; f < 4; f++) for (int sub = 0; sub < 2; sub++) {
        double ox = x0 + (i + fcc[f][0] + 0.125 + 0.25 * sub) * a, oy = y0 + (j + fcc[f][1] + 0.125 + 0.25 * sub) * a, oz = z0 + (k + fcc[f][2] + 0.125 + 0.25 * sub) * a;
        int h1 = sub == 0 ? 0 : 2, h2 = sub == 0 ? 1 : 3; double sg = sub == 0 ? 1 : -1;
        double vx, vy, vz; thermalVel(E_O, T, vx, vy, vz);
        int o = addAtom(E_O, ox, oy, oz, vx, vy, vz);
        for (int h : {h1, h2}) {
            int hi = addAtom(E_H, ox + sg * rOH * s3 * d[h][0], oy + sg * rOH * s3 * d[h][1], oz + sg * rOH * s3 * d[h][2], vx, vy, vz);
            changeBond(o, hi, +1); updateCharge(hi);
        }
        updateCharge(o);
    }
}
static void wrapAll() {
    if (P.perMask == 0 || P.boundary == B_PISTON) return;
    for (int i = 0; i < S.n; i++) {
        wrapPoint(S.x[i], S.y[i], S.z[i]);
        S.ux[i] = S.x[i]; S.uy[i] = S.y[i]; S.uz[i] = S.z[i];
    }
}
static void zeroMomentum() {
    double px = 0, py = 0, pz = 0, M = 0;
    for (int i = 0; i < S.n; i++) { if (EL[S.ty[i]].fixed) continue; double m = EL[S.ty[i]].m; px += m * S.vx[i]; py += m * S.vy[i]; pz += m * S.vz[i]; M += m; }
    if (M <= 0) return;
    for (int i = 0; i < S.n; i++) if (!EL[S.ty[i]].fixed) { S.vx[i] -= px / M; S.vy[i] -= py / M; S.vz[i] -= pz / M; }
}

// ===================================== БИБЛИОТЕКА МОЛЕКУЛ И СТРУКТУР ======================
// Геометрия: длины связей — r0 модели (реальные длины из таблицы), углы — равновесные углы модели (VSEPR),
// поэтому вставленная молекула сразу находится в минимуме энергии. Кольца и кристаллы строятся явно.
// Порядок — по разделам (ML_GROUP_START): неорганические молекулы, органические, ионы, кристаллы и кластеры.
enum { ML_H2O, ML_H2O2, ML_CO2, ML_CO, ML_NH3, ML_HCL, ML_HF, ML_HBR, ML_HI, ML_H2S, ML_PH3, ML_SIH4, ML_HCN, ML_N2H4, ML_O3,
       ML_NO, ML_NO2, ML_HNO2, ML_NH2OH, ML_NH2CL, ML_HOCL, ML_CL2O, ML_OF2, ML_NF3, ML_NCL3, ML_PF3, ML_PCL3, ML_ASH3, ML_H2SE,
       ML_SCL2, ML_CS2, ML_COS, ML_SIF4, ML_SICL4, ML_GEH4, ML_BF3, ML_BCL3,
       ML_N2, ML_O2, ML_H2, ML_F2, ML_CL2, ML_BR2, ML_I2, ML_CLF, ML_ICL,
       ML_SO2, ML_SO3, ML_H2SO4, ML_H3PO4, ML_HNO3, ML_HCLO4, ML_POCL3, ML_SOCL2, ML_SF4, ML_SF6, ML_PF5, ML_PCL5, ML_CLF3, ML_BRF5,
       ML_IF5, ML_XEF2, ML_XEF4, ML_XEO3,
       ML_CH4, ML_C2H6, ML_C3H8, ML_C4H10, ML_ISOBUTANE, ML_C5H12, ML_C6H14, ML_C2H4, ML_C3H6, ML_C4H6, ML_C2H2, ML_C3H4,
       ML_CYCLOPENTANE, ML_CYCLOHEXANE, ML_C6H6, ML_PHENOL, ML_TOLUENE, ML_STYRENE, ML_ANILINE, ML_BENZOIC, ML_NAPHTHALENE, ML_PYRIDINE,
       ML_CH3OH, ML_C2H5OH, ML_IPA, ML_GLYCOL, ML_GLYCEROL, ML_DME, ML_DEE, ML_HCHO, ML_CH3CHO, ML_ACETONE, ML_HCOOH, ML_CH3COOH,
       ML_OXALIC, ML_LACTIC, ML_ETOAC, ML_CH3CN, ML_HCONH2, ML_CH3CL, ML_CH2CL2, ML_CHCL3, ML_CCL4, ML_C2H5CL, ML_C2H3CL, ML_CF4,
       ML_CCL2F2, ML_CH3SH, ML_CH3NH2, ML_UREA, ML_GLYCINE, ML_ALANINE,
       ML_H3O, ML_OH, ML_NH4, ML_CN, ML_HS, ML_CLO, ML_NO2M, ML_HCOO, ML_CH3COO, ML_HCO3, ML_SO4, ML_PO4, ML_NO3, ML_CLO4, ML_NAOH, ML_NACL,
       ML_NACL_CRYST, ML_DIAMOND, ML_GRAPHENE, ML_C60, ML_ICE, ML_SI_CRYST, ML_SIO2,
       ML_PT, ML_AU, ML_FE, ML_NI, ML_CU, ML_AG, ML_AL, ML_PD, ML_RH, ML_PB, ML_N };
enum { MLG_INORG, MLG_ORG, MLG_ION, MLG_SOLID, MLG_N };
static const int ML_GROUP_START[MLG_N + 1] = {ML_H2O, ML_CH4, ML_H3O, ML_NACL_CRYST, ML_N};
static const char* MLG_NAMES[MLG_N] = {"неорганические молекулы", "органические молекулы", "ионы", "кристаллы и кластеры металлов"};
static const char* MOL_LIB_NAMES[] = {
    "H2O", "H2O2", "CO2", "CO", "NH3", "HCl", "HF", "HBr", "HI", "H2S", "PH3", "SiH4", "HCN", "N2H4", "O3",
    "NO", "NO2", "HNO2", "NH2OH", "NH2Cl", "HOCl", "Cl2O", "OF2", "NF3", "NCl3", "PF3", "PCl3", "AsH3", "H2Se",
    "SCl2", "CS2", "COS", "SiF4", "SiCl4", "GeH4", "BF3", "BCl3",
    "N2", "O2", "H2", "F2", "Cl2", "Br2", "I2", "ClF", "ICl",
    "SO2", "SO3", "H2SO4", "H3PO4", "HNO3", "HClO4", "POCl3", "SOCl2", "SF4", "SF6", "PF5", "PCl5", "ClF3", "BrF5",
    "IF5", "XeF2", "XeF4", "XeO3",
    "CH4", "C2H6", "C3H8", "C4H10", "i-C4H10", "C5H12", "C6H14", "C2H4", "C3H6", "C4H6", "C2H2", "C3H4",
    "C5H10", "C6H12", "C6H6", "C6H5OH", "C6H5CH3", "C8H8", "C6H5NH2", "C6H5COOH", "C10H8", "C5H5N",
    "CH3OH", "C2H5OH", "C3H7OH", "C2H4(OH)2", "C3H5(OH)3", "CH3OCH3", "(C2H5)2O", "HCHO", "CH3CHO", "(CH3)2CO", "HCOOH", "CH3COOH",
    "H2C2O4", "C3H6O3", "C4H8O2", "CH3CN", "HCONH2", "CH3Cl", "CH2Cl2", "CHCl3", "CCl4", "C2H5Cl", "C2H3Cl", "CF4",
    "CCl2F2", "CH3SH", "CH3NH2", "CO(NH2)2", "глицин", "аланин",
    "H3O+", "OH-", "NH4+", "CN-", "HS-", "ClO-", "NO2-", "HCOO-", "CH3COO-", "HCO3-", "SO4^2-", "PO4^3-", "NO3-", "ClO4-", "NaOH", "NaCl",
    "кристалл NaCl", "алмаз", "графен", "C60", "лёд", "кремний", "SiO2",
    "Pt", "Au", "Fe", "Ni", "Cu", "Ag", "Al", "Pd", "Rh", "Pb"};
static const char* MOL_LIB_DESC[] = {
    // неорганические
    "вода: угол H–O–H 104.5°, O–H 0.96 Å, заряды как SPC/E", "пероксид водорода: O–O 1.48 Å, двугранный угол 111°", "углекислый газ: линейная, C=O 1.20 Å",
    "угарный газ (в модели C=O; в природе тройная связь C≡O)", "аммиак: пирамида, угол 107°", "хлороводород: H–Cl 1.27 Å, 4.43 эВ; в воде — сильная кислота",
    "фтороводород: H–F 0.92 Å, 5.87 эВ — самая прочная связь с водородом", "бромоводород: H–Br 1.41 Å, 3.76 эВ",
    "иодоводород: H–I 1.61 Å, 3.06 эВ — легко распадается на H2 и I2", "сероводород: угол H–S–H 92°, S–H 1.34 Å (запах тухлых яиц)",
    "фосфин: пирамида, P–H 1.42 Å, угол ≈ 94° (в модели 98°)", "силан: тетраэдр, Si–H 1.48 Å; на воздухе самовоспламеняется",
    "циановодород: линейная H–C≡N, C≡N 1.16 Å", "гидразин N2H4: N–N 1.45 Å, ракетное топливо",
    "озон O=O⁺–O⁻ (биполярный ион): неустойчив, распадается при нагреве",
    "оксид азота(II): радикал с неспаренным электроном, N=O 1.15 Å", "диоксид азота: бурый газ-радикал, при охлаждении димеризуется в N2O4",
    "азотистая кислота H–O–N=O: слабая и неустойчивая", "гидроксиламин NH2OH: производное аммиака, восстановитель",
    "хлорамин NH2Cl: образуется при хлорировании воды, где есть аммиак", "хлорноватистая кислота H–O–Cl: действующее начало отбеливателей",
    "оксид хлора(I) Cl2O: угловая молекула, ангидрид HOCl", "дифторид кислорода OF2: угловая молекула, сильный окислитель",
    "трифторид азота NF3: пирамида, газ для травления микросхем", "трихлорид азота NCl3: маслянистая взрывчатая жидкость",
    "трифторид фосфора PF3: пирамида, P–F 1.56 Å", "трихлорид фосфора PCl3: пирамида, дымит на воздухе",
    "арсин AsH3: пирамида, очень ядовит", "селеноводород H2Se: угол 91°, тяжёлый аналог H2S",
    "дихлорид серы SCl2: угловая молекула, вишнёво-красная жидкость", "сероуглерод S=C=S: линейный, легко воспламеняется",
    "карбонилсульфид O=C=S: самый распространённый серосодержащий газ атмосферы",
    "тетрафторид кремния: тетраэдр, Si–F 1.56 Å — одна из самых прочных одинарных связей", "тетрахлорид кремния: тетраэдр, сырьё для чистого кремния",
    "герман GeH4: тетраэдр, аналог силана", "трифторид бора: плоский треугольник, кислота Льюиса", "трихлорид бора: плоский треугольник",
    "азот: N≡N 1.10 Å, 9.79 эВ — самая прочная связь", "кислород: O=O 1.21 Å, 5.12 эВ", "водород: H–H 0.74 Å, 4.52 эВ",
    "фтор: F–F 1.42 Å, всего 1.6 эВ — самый активный неметалл", "хлор: Cl–Cl 1.99 Å, 2.48 эВ",
    "бром: Br–Br 2.28 Å, 1.97 эВ; при комнатной температуре — жидкость", "иод: I–I 2.67 Å, 1.54 эВ; возгоняется фиолетовым паром",
    "монофторид хлора: межгалогенное соединение, Cl–F 1.63 Å", "монохлорид иода: межгалогенное соединение, I–Cl 2.32 Å",
    // переменная валентность: у элементов 3-го периода и ниже октет расширяется
    "диоксид серы O=S=O: сера четырёхвалентна, неподелённая пара сгибает молекулу (119°)",
    "триоксид серы SO3: сера шестивалентна, плоский треугольник", "серная кислота H2SO4: тетраэдр S(=O)2(OH)2, сера шестивалентна",
    "ортофосфорная кислота H3PO4: фосфор пятивалентен, тетраэдр", "азотная кислота HNO3: у азота четыре связи — на нём заряд +, на одном O заряд −",
    "хлорная кислота HClO4: хлор семивалентен — самая сильная из обычных кислот",
    "хлорокись фосфора POCl3: тетраэдр, P=O 1.45 Å", "тионилхлорид SOCl2: пирамида, S=O 1.45 Å, реагент для хлорирования",
    "тетрафторид серы SF4: «качели» — неподелённая пара в экваторе бипирамиды", "гексафторид серы SF6: октаэдр, очень инертный газ-изолятор",
    "пентафторид фосфора PF5: тригональная бипирамида, связи в экваторе короче осевых", "пентахлорид фосфора PCl5: бипирамида; при нагреве распадается на PCl3 и Cl2",
    "трифторид хлора ClF3: T-образная молекула, две неподелённые пары в экваторе; поджигает даже стекло",
    "пентафторид брома BrF5: квадратная пирамида, неподелённая пара снизу", "пентафторид иода IF5: квадратная пирамида",
    "дифторид ксенона XeF2: благородный газ со связями! Линейная, три пары в экваторе",
    "тетрафторид ксенона XeF4: квадрат, две неподелённые пары над и под плоскостью", "триоксид ксенона XeO3: пирамида, взрывается от удара",
    // органические
    "метан: тетраэдр, C–H 1.09 Å, 4.3 эВ на связь", "этан: C–C 1.54 Å, 3.6 эВ", "пропан C3H8: бытовой баллонный газ", "н-бутан: цепь из четырёх атомов C (газ для зажигалок)",
    "изобутан (CH3)3CH: разветвлённый изомер бутана", "н-пентан: жидкость, кипит при 36 °C", "н-гексан: неполярный растворитель",
    "этилен: плоская, C=C 1.34 Å, 6.4 эВ", "пропилен CH2=CH–CH3: сырьё для полипропилена",
    "бутадиен-1,3 CH2=CH–CH=CH2: сопряжённые двойные связи, сырьё для каучука", "ацетилен: линейная, C≡C 1.20 Å, 8.7 эВ", "пропин CH≡C–CH3: гомолог ацетилена",
    "циклопентан: пятичленное кольцо, сложенное «конвертом»", "циклогексан: кольцо в форме «кресла», все углы близки к 109.5°",
    "бензол: кольцо Кекуле (чередование C–C и C=C)", "фенол: бензольное кольцо с группой OH (слабая кислота, антисептик)",
    "толуол: бензольное кольцо с группой CH3 (растворитель)", "стирол C6H5–CH=CH2: мономер полистирола", "анилин C6H5NH2: ароматический амин, основа красителей",
    "бензойная кислота C6H5COOH: консервант E210", "нафталин: два сконденсированных бензольных кольца", "пиридин: бензольное кольцо, где один CH заменён на N; основание",
    "метанол CH3OH", "этанол C2H5OH (горит: C2H5OH + 3O2 → 2CO2 + 3H2O)", "изопропанол (CH3)2CHOH: антисептик и растворитель",
    "этиленгликоль HO–CH2–CH2–OH: основа антифриза", "глицерин: трёхатомный спирт, основа жиров", "диметиловый эфир CH3–O–CH3: изомер этанола",
    "диэтиловый эфир C2H5–O–C2H5: летучий растворитель, первый наркоз", "формальдегид HCHO: плоская, C=O 1.20 Å",
    "ацетальдегид CH3CHO: продукт окисления этанола", "ацетон: группа C=O между двумя CH3 (растворитель)",
    "муравьиная кислота HCOOH: простейшая карбоновая кислота", "уксусная кислота CH3COOH: группа COOH (уксус)",
    "щавелевая кислота HOOC–COOH: простейшая двухосновная кислота", "молочная кислота CH3–CH(OH)–COOH: накапливается в работающих мышцах",
    "этилацетат CH3COOC2H5: сложный эфир с запахом груши", "ацетонитрил CH3–C≡N: полярный растворитель",
    "формамид HCONH2: простейший амид — пептидная связь в миниатюре",
    "хлорметан CH3Cl: первый продукт хлорирования метана, C–Cl 1.77 Å", "дихлорметан CH2Cl2: растворитель", "хлороформ CHCl3: трихлорметан",
    "тетрахлорметан CCl4: тетраэдр из четырёх связей C–Cl", "хлорэтан C2H5Cl: местная «заморозка» в спорте", "винилхлорид CH2=CHCl: мономер ПВХ",
    "тетрафторметан CF4: очень инертный газ", "фреон-12 CCl2F2: хладагент, разрушает озоновый слой",
    "метантиол CH3SH: запах, который добавляют в бытовой газ", "метиламин CH3NH2: простейший амин, основание",
    "мочевина CO(NH2)2: первое органическое вещество, полученное из неорганического (Вёлер, 1828)", "глицин NH2–CH2–COOH: простейшая аминокислота",
    "аланин CH3–CH(NH2)–COOH: одна из аминокислот белков",
    // ионы
    "ион гидроксония H3O+ (кислота): протон переходит к соседней воде", "гидроксид-ион OH− (основание): нейтрализует H3O+", "ион аммония NH4+: тетраэдр",
    "цианид-ион C≡N⁻: сильный лиганд и яд", "гидросульфид-ион HS⁻: основание", "гипохлорит-ион ClO⁻: окислитель в хлорных отбеливателях",
    "нитрит-ион O=N–O⁻: угловой", "формиат-ион HCOO⁻", "ацетат-ион CH3COO⁻: сопряжённое основание уксусной кислоты",
    "гидрокарбонат-ион HCO3⁻: основа питьевой соды", "сульфат-ион SO4²⁻: тетраэдр, заряд −1 на двух атомах O",
    "фосфат-ион PO4³⁻: тетраэдр", "нитрат-ион NO3⁻: плоский треугольник", "перхлорат-ион ClO4⁻: тетраэдр, окислитель ракетного топлива",
    "гидроксид натрия: Na+ и OH− (щёлочь)", "ионная пара Na+ Cl−",
    // кристаллы и кластеры
    "кристаллик NaCl (решётка каменной соли)", "кластер алмаза, поверхность закрыта водородом (C–C 1.54 Å, sp3)",
    "лист графена: соты sp2, структура Кекуле, край закрыт водородом",
    "фуллерен C60: «футбольный мяч» из 12 пятиугольников и 20 шестиугольников", "кубический лёд Ic: 64 молекулы, правила Бернала–Фаулера",
    "кристалл кремния: решётка алмаза, Si–Si 2.35 Å, поверхность закрыта водородом",
    "кремнезём: сетка SiO4 (кристобалит), поверхность — группы Si–OH", "кластер платины (ГЦК): катализатор окисления водорода",
    "кластер золота (ГЦК)", "кластер железа: катализатор, ржавеет в O2", "кластер никеля: катализатор гидрирования",
    "кластер меди (ГЦК)", "кластер серебра (ГЦК)", "кластер алюминия (ГЦК): лёгкий металл", "кластер палладия (ГЦК): поглощает водород, катализатор",
    "кластер родия (ГЦК): катализатор дожига выхлопных газов", "кластер свинца (ГЦК): тяжёлый мягкий металл"};
static_assert(sizeof(MOL_LIB_NAMES) / sizeof(*MOL_LIB_NAMES) == ML_N && sizeof(MOL_LIB_DESC) / sizeof(*MOL_LIB_DESC) == ML_N, "MOL_LIB_NAMES / MOL_LIB_DESC: size must match ML_N");
struct V3 { double x, y, z; };
static inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static inline V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
static inline double vdot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 vcross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static inline V3 vunit(V3 a) { double l = std::sqrt(vdot(a, a)); return l > 1e-12 ? a * (1 / l) : V3{1, 0, 0}; }
static inline double tmplTheta(int t, int k) { return vseprAngle(t, k); }
// направления k связей атома: первое — u (к родителю), остальные по VSEPR (плоские молекулы — в одной плоскости)
static std::vector<V3> bondDirs(int t, int k, V3 u, int depth) {
    std::vector<V3> d{u};
    V3 g{0, 1, 0}; if (std::fabs(vdot(g, u)) > 0.9) g = {0, 0, 1};
    const V3 w = vunit(g - u * vdot(g, u)), v = vcross(u, w);
    auto at = [&](double th, double ph) { double a = th * PI / 180, p = ph * PI / 180; return u * std::cos(a) + (w * std::cos(p) + v * std::sin(p)) * std::sin(a); };
    double th = tmplTheta(t, k);
    if (k == 2) { if (th <= 0) th = 180; d.push_back(at(th, (depth & 1) ? 180 : 0)); }
    else if (k == 3) {
        if (th <= 0) th = 109.47;
        if (th >= 119.9) { d.push_back(at(th, 0)); d.push_back(at(th, 180)); }
        else { double ct = std::cos(th * PI / 180), c2 = clampv((ct - ct * ct) / (1 - ct * ct), -1.0, 1.0), ph = 0.5 * std::acos(c2) * 180 / PI;
               d.push_back(at(th, ph)); d.push_back(at(th, -ph)); }
    } else if (k == 4) { d.push_back(at(109.47, 0)); d.push_back(at(109.47, 120)); d.push_back(at(109.47, 240)); }
    else if (k == 5) { d.push_back(u * -1.0); d.push_back(at(90, 0)); d.push_back(at(90, 120)); d.push_back(at(90, 240)); }   // тригональная бипирамида
    else if (k >= 6) { d.push_back(u * -1.0); for (int q = 0; q < 4; q++) d.push_back(at(90, 90.0 * q)); }                    // октаэдр
    return d;
}
// Координаты атомов по графу связей, обход в ширину. Атомы с fixedN первых номеров уже стоят (кольцо),
// остальные растут от них по VSEPR; без готовых атомов обход начинается с атома 0.
static void vseprPlace(Tmpl& m, int fixedN = 0) {
    const int n = (int)m.a.size(); if (!n) return;
    std::vector<std::vector<std::pair<int, int>>> adj(n);
    for (auto& b : m.b) { adj[b[0]].push_back({b[1], b[2]}); adj[b[1]].push_back({b[0], b[2]}); }
    std::vector<char> placed(n, 0); std::vector<int> par(n, -1), dep(n, 0); std::vector<V3> Pp(n, V3{0, 0, 0});
    std::vector<int> q;
    for (int i = 0; i < fixedN && i < n; i++) { placed[i] = 1; Pp[i] = {m.a[i].x, m.a[i].y, m.a[i].z}; q.push_back(i); }
    if (q.empty()) { q.push_back(0); placed[0] = 1; }
    for (size_t qi = 0; qi < q.size(); qi++) {
        int a = q[qi], k = (int)adj[a].size(); if (!k) continue;
        std::vector<V3> cd;
        int nPlaced = 0; V3 sum{0, 0, 0}, u1{0, 0, 0}, u2{0, 0, 0};
        for (auto& nb : adj[a]) if (placed[nb.first] && nb.first != par[a] && a < fixedN) {
            V3 e = vunit(Pp[nb.first] - Pp[a]); sum = sum + e; (nPlaced ? u2 : u1) = e; nPlaced++;
        }
        if (nPlaced >= 2) {   // атом кольца: заместители — наружу, по биссектрисе (sp2) или парой над и под плоскостью (sp3)
            const V3 b = vunit(sum * -1.0); const int nNew = k - nPlaced;
            if (nNew == 1) cd = {b};
            else if (nNew >= 2) {
                const V3 nn = vunit(vcross(u1, u2)); const double h = 0.5 * std::acos(-1.0 / 3.0);
                cd = {b * std::cos(h) + nn * std::sin(h), b * std::cos(h) - nn * std::sin(h)};
            }
        } else {
            V3 u = par[a] >= 0 ? vunit(Pp[par[a]] - Pp[a]) : V3{1, 0, 0};
            const int t = m.a[a].t; int used = 0; for (auto& nb : adj[a]) used += nb.second;
            if (HYPER[t] && used > EL[t].val && k >= 2 && k <= cfg::MAXB) {
                // расширенный октет: связи — по идеальной фигуре облаков VSEPR (с неподелёнными парами), первая — к родителю
                double w[cfg::MAXB], wl[4], dd[cfg::MAXB + 4][3]; int q = 0;
                if (par[a] >= 0) for (auto& nb : adj[a]) if (nb.first == par[a]) w[q++] = vsBondW(nb.second);
                for (auto& nb : adj[a]) if (nb.first != par[a]) w[q++] = vsBondW(nb.second);
                const int nbE = std::max(0, VE[t] - used), nl = std::min(3, nbE / 2), nd = nl + (nbE & 1);
                for (int p = 0; p < nd; p++) wl[p] = p < nl ? vs::W_LP : vs::W_ONE;
                vsIdeal(k, w, nd, wl, dd, vsTransPair(k, nl));
                // поворот, переводящий первую идеальную связь в u (формула Родрига)
                const V3 p0 = vunit(V3{dd[0][0], dd[0][1], dd[0][2]}); V3 ax = vcross(p0, u); const double s = std::sqrt(vdot(ax, ax)), c = vdot(p0, u);
                auto rot = [&](V3 v) {
                    if (s < 1e-9) return c > 0 ? v : v * -1.0;
                    const V3 e = ax * (1 / s); return v * c + vcross(e, v) * s + e * (vdot(e, v) * (1 - c));
                };
                for (int p = par[a] >= 0 ? 1 : 0; p < k; p++) cd.push_back(vunit(rot(V3{dd[p][0], dd[p][1], dd[p][2]})));
            } else {
                std::vector<V3> dirs = bondDirs(t, k, u, dep[a]);
                cd.assign(dirs.begin() + (par[a] >= 0 ? 1 : 0), dirs.end());
            }
        }
        if (par[a] >= 0 && !cd.empty()) {   // поворот заместителей вокруг связи с родителем: подальше от уже поставленных атомов
            const V3 u = vunit(Pp[par[a]] - Pp[a]);
            double bestS = -1; std::vector<V3> best = cd; const int nr = 12;
            for (int rr = 0; rr < nr; rr++) {
                double ps = rr * 2 * PI / nr, c = std::cos(ps), s = std::sin(ps), sc = 1e9;
                std::vector<V3> rd; for (auto& d : cd) rd.push_back(d * c + vcross(u, d) * s + u * (vdot(u, d) * (1 - c)));
                for (auto& d : rd) { V3 pc = Pp[a] + d * 0.3; for (int p = 0; p < n; p++) if (placed[p] && p != a) { V3 dd = pc - Pp[p]; sc = std::min(sc, vdot(dd, dd)); } }
                if (sc > bestS + 1e-9) { bestS = sc; best = rd; }
            }
            cd = best;
        }
        int di = 0;
        for (auto& nb : adj[a]) {
            int b = nb.first; if (placed[b] || di >= (int)cd.size()) continue;
            double r = BT[m.a[a].t][m.a[b].t].r0[nb.second]; if (r <= 0) r = 0.35;
            Pp[b] = Pp[a] + cd[di++] * r; placed[b] = 1; par[b] = a; dep[b] = dep[a] + 1; q.push_back(b);
        }
    }
    for (int i = 0; i < n; i++) { m.a[i].x = Pp[i].x; m.a[i].y = Pp[i].y; m.a[i].z = Pp[i].z; }
}
// Релаксация шаблона (наискорейший спуск): связи Морзе, валентные углы модели, отталкивание несвязанных атомов
// (WCA по параметрам LJ, пары 1-2 и 1-3 исключены) — вставленная структура сразу в минимуме энергии, без «нагрева»
static void relaxTmpl(Tmpl& m, int iters = 3000) {
    const int n = (int)m.a.size(); if (n < 2 || m.b.empty()) return;
    std::vector<std::vector<std::pair<int, int>>> adj(n);
    for (auto& b : m.b) { adj[b[0]].push_back({b[1], b[2]}); adj[b[1]].push_back({b[0], b[2]}); }
    std::vector<char> ex((size_t)n * n, 0);
    for (int a = 0; a < n; a++) { ex[(size_t)a * n + a] = 1;
        for (auto& p : adj[a]) { ex[(size_t)a * n + p.first] = 1; for (auto& q : adj[p.first]) ex[(size_t)a * n + q.first] = 1; } }
    std::vector<int> comp(n, -1);   // отталкивание — только внутри одной молекулы (межмолекулярные контакты задаёт сама сцена)
    for (int s = 0, id = 0; s < n; s++) if (comp[s] < 0) {
        std::vector<int> st{s}; comp[s] = id;
        while (!st.empty()) { int a = st.back(); st.pop_back(); for (auto& p : adj[a]) if (comp[p.first] < 0) { comp[p.first] = id; st.push_back(p.first); } }
        id++;
    }
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) if (comp[i] != comp[j]) ex[(size_t)i * n + j] = 1;
    std::vector<double> X(3 * n), F(3 * n);
    for (int i = 0; i < n; i++) { X[3 * i] = m.a[i].x + 0.01 * (urand() - 0.5); X[3 * i + 1] = m.a[i].y + 0.01 * (urand() - 0.5); X[3 * i + 2] = m.a[i].z + 0.01 * (urand() - 0.5); }
    // гипервалентные центры шаблона (связей больше обычной валентности): их геометрия — облака VSEPR с неподелёнными парами
    std::vector<char> hyperT(n, 0), tlpInit(n, 0); std::vector<std::array<double, 12>> tlp(n);
    for (int i = 0; i < n; i++) { int used = 0; for (auto& p : adj[i]) used += p.second; const int t = m.a[i].t; hyperT[i] = HYPER[t] && used > EL[t].val; }
    auto forces = [&]() {
        std::fill(F.begin(), F.end(), 0.0);
        for (auto& b : m.b) {   // связи
            int i = b[0], j = b[1]; const BondT& bt = BT[m.a[i].t][m.a[j].t]; int o = std::min(b[2], std::max(1, bt.maxOrder));
            double d[3] = {X[3 * j] - X[3 * i], X[3 * j + 1] - X[3 * i + 1], X[3 * j + 2] - X[3 * i + 2]}, r = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (r < 1e-9 || bt.D[o] <= 0) continue;
            double dU; bondPotT(bt, o, r, dU);
            for (int c = 0; c < 3; c++) { F[3 * i + c] += dU * d[c] / r; F[3 * j + c] -= dU * d[c] / r; }
        }
        for (int c0 = 0; c0 < n; c0++) {   // углы: U = K(cosθ − cosθ0)², у гипервалентных центров — облака VSEPR
            int k = (int)adj[c0].size();
            if (k >= 2 && k <= cfg::MAXB && hyperT[c0]) {
                const int t = m.a[c0].t; int used = 0; double r[cfg::MAXB][3], w[cfg::MAXB], wl[4], g[cfg::MAXB][3];
                for (int p = 0; p < k; p++) { const int b = adj[c0][p].first; used += adj[c0][p].second; w[p] = vsBondW(adj[c0][p].second);
                                              for (int c = 0; c < 3; c++) r[p][c] = X[3 * b + c] - X[3 * c0 + c]; }
                const int nbE = std::max(0, VE[t] - used), nl = std::min(3, nbE / 2), unp = nbE & 1, nd = nl + unp;
                for (int q = 0; q < nd; q++) wl[q] = q < nl ? vs::W_LP : vs::W_ONE;
                double (*L)[3] = reinterpret_cast<double (*)[3]>(tlp[c0].data());
                const bool trans = vsTransPair(k, nl);
                if (!tlpInit[c0]) { if (nd) vsInitLP(k, r, w, nd, wl, L, trans); tlpInit[c0] = 1; }
                vsEnergy(k, r, w, nd, wl, L, 2, g, trans);
                for (int p = 0; p < k; p++) { const int b = adj[c0][p].first; for (int c = 0; c < 3; c++) { F[3 * b + c] -= g[p][c]; F[3 * c0 + c] += g[p][c]; } }
                continue;
            }
            double th = tmplTheta(m.a[c0].t, k); if (th <= 0) continue;
            double cs0 = std::cos(th * PI / 180);
            for (int p = 0; p < k; p++) for (int q = p + 1; q < k; q++) {
                int a = adj[c0][p].first, b = adj[c0][q].first; double A[3], B[3];
                for (int c = 0; c < 3; c++) { A[c] = X[3 * a + c] - X[3 * c0 + c]; B[c] = X[3 * b + c] - X[3 * c0 + c]; }
                double ra = std::sqrt(A[0] * A[0] + A[1] * A[1] + A[2] * A[2]), rb = std::sqrt(B[0] * B[0] + B[1] * B[1] + B[2] * B[2]); if (ra < 1e-9 || rb < 1e-9) continue;
                double cs = (A[0] * B[0] + A[1] * B[1] + A[2] * B[2]) / (ra * rb), k2 = 2 * cfg::K_ANGLE * (cs - cs0), iab = 1 / (ra * rb);
                for (int c = 0; c < 3; c++) {
                    double ga = B[c] * iab - cs * A[c] / (ra * ra), gb = A[c] * iab - cs * B[c] / (rb * rb);
                    F[3 * a + c] -= k2 * ga; F[3 * b + c] -= k2 * gb; F[3 * c0 + c] += k2 * (ga + gb);
                }
            }
        }
        for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) {   // отталкивание несвязанных (WCA)
            if (ex[(size_t)i * n + j]) continue;
            const PairP& pp = PT[m.a[i].t][m.a[j].t]; if (pp.eps4 <= 0) continue;
            double d[3] = {X[3 * j] - X[3 * i], X[3 * j + 1] - X[3 * i + 1], X[3 * j + 2] - X[3 * i + 2]}, r2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
            if (r2 > 1.2599 * pp.sig2 || r2 < 1e-12) continue;
            double sr2 = pp.sig2 / r2, sr6 = sr2 * sr2 * sr2, ff = pp.eps4 * (12 * sr6 * sr6 - 6 * sr6) / r2;
            for (int c = 0; c < 3; c++) { F[3 * i + c] -= ff * d[c]; F[3 * j + c] += ff * d[c]; }
        }
    };
    for (int it = 0; it < iters; it++) {
        forces();
        double fm = 0; for (double f : F) fm = std::max(fm, std::fabs(f));
        if (fm < 0.5) break;
        // смещение ≤ 0.01σ за итерацию; шаг 2·10⁻⁵ < 2/k самой жёсткой связи (O–H: k = 2Da² ≈ 4.5·10⁴ ε/σ²)
        const double alpha = std::min(2e-5, 0.01 / fm);
        for (int k = 0; k < 3 * n; k++) X[k] += alpha * F[k];
    }
    for (int i = 0; i < n; i++) { m.a[i].x = X[3 * i]; m.a[i].y = X[3 * i + 1]; m.a[i].z = X[3 * i + 2]; }
}
static void centerTmpl(Tmpl& m) {
    double cx = 0, cy = 0, cz = 0; for (auto& a : m.a) { cx += a.x; cy += a.y; cz += a.z; }
    const double k = m.a.empty() ? 0 : 1.0 / m.a.size();
    for (auto& a : m.a) { a.x -= cx * k; a.y -= cy * k; a.z -= cz * k; }
}
static int tA(Tmpl& m, int t, V3 p = {0, 0, 0}, int fc = 0) { m.a.push_back({t, p.x, p.y, p.z, fc}); return (int)m.a.size() - 1; }
static void tB(Tmpl& m, int a, int b, int o = 1) { m.b.push_back({a, b, o}); }
static int bondCount(const Tmpl& m, int a) { int c = 0; for (auto& b : m.b) if (b[0] == a || b[1] == a) c++; return c; }
// водороды на недостающих направлениях атома сетки (dirs — идеальные направления связей); H ближе 1.5 Å друг к другу не ставятся
static void capWithH(Tmpl& m, int a, const std::vector<V3>& dirs, int Hto = E_H) {
    const double rH = r0of(m.a[a].t, Hto, 1);
    V3 pa{m.a[a].x, m.a[a].y, m.a[a].z};
    for (auto& d : dirs) {
        bool occ = false;
        for (auto& b : m.b) { int o = b[0] == a ? b[1] : (b[1] == a ? b[0] : -1); if (o < 0) continue;
            V3 po{m.a[o].x, m.a[o].y, m.a[o].z}; if (vdot(vunit(po - pa), d) > 0.9) occ = true; }
        if (occ) continue;
        V3 ph = pa + d * rH; bool clash = false;
        for (size_t q = 0; q < m.a.size(); q++) { if ((int)q == a) continue; V3 px{m.a[q].x, m.a[q].y, m.a[q].z}; V3 dd = px - ph; if (vdot(dd, dd) < std::pow(1.5 / 3.405, 2)) { clash = true; break; } }
        if (clash) continue;
        int h = tA(m, Hto, ph); tB(m, a, h, 1);
    }
}
// кластер металла типа t: ГЦК в шаре радиусом Rn межатомных расстояний
static void metalCluster(Tmpl& m, int t, double Rn) {
    const double d = 2 * EL[t].rmet / 3.405, a = d * std::sqrt(2.0), R = Rn * d; int k = (int)(R / a) + 2;
    const double bs[4][3] = {{0, 0, 0}, {0.5, 0.5, 0}, {0.5, 0, 0.5}, {0, 0.5, 0.5}};
    for (int z = -k; z <= k; z++) for (int y = -k; y <= k; y++) for (int x = -k; x <= k; x++) for (auto& b : bs) {
        V3 p{(x + b[0]) * a, (y + b[1]) * a, (z + b[2]) * a}; if (vdot(p, p) <= R * R + 1e-9) tA(m, t, p); }
}
// правильный многоугольник из атомов types (в плоскости xy), связь k–(k+1) имеет порядок orders[k].
// Кольцо должно быть первым в шаблоне: заместители потом растит vseprPlace(m, n)
static void ringTmpl(Tmpl& m, const std::vector<int>& types, const std::vector<int>& orders) {
    const int n = (int)types.size(); double L = 0;
    for (int k = 0; k < n; k++) L += r0of(types[k], types[(k + 1) % n], orders[k]);
    const double R = L / n / (2 * std::sin(PI / n));
    for (int k = 0; k < n; k++) tA(m, types[k], V3{std::cos(2 * PI * k / n), std::sin(2 * PI * k / n), 0} * R);
    for (int k = 0; k < n; k++) tB(m, k, (k + 1) % n, orders[k]);
}
// фуллерен C60: вершины усечённого икосаэдра — чётные перестановки (0, ±1, ±3φ), (±1, ±(2+φ), ±2φ), (±φ, ±2, ±(2φ+1)),
// длина ребра 2. Рёбра пятиугольников — одинарные связи, рёбра между шестиугольниками — двойные (структура Кекуле)
static void fullereneTmpl(Tmpl& m) {
    const double f = (1 + std::sqrt(5.0)) / 2, sc = 1.44 / 3.405 / 2;
    const double base[3][3] = {{0, 1, 3 * f}, {1, 2 + f, 2 * f}, {f, 2, 2 * f + 1}};
    std::vector<V3> P;
    for (auto& b : base) for (int s = 0; s < 8; s++) {
        double v[3] = {b[0] * (s & 1 ? -1 : 1), b[1] * (s & 2 ? -1 : 1), b[2] * (s & 4 ? -1 : 1)};
        for (int c = 0; c < 3; c++) {
            V3 p{v[c], v[(c + 1) % 3], v[(c + 2) % 3]}; bool dup = false;
            for (auto& q : P) { V3 d = q - p; if (vdot(d, d) < 1e-6) { dup = true; break; } }
            if (!dup) P.push_back(p);
        }
    }
    const int n = (int)P.size();
    std::vector<std::vector<int>> adj(n);
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) { V3 d = P[i] - P[j]; if (std::fabs(vdot(d, d) - 4) < 0.01) { adj[i].push_back(j); adj[j].push_back(i); } }
    auto linked = [&](int a, int b) { return std::find(adj[a].begin(), adj[a].end(), b) != adj[a].end(); };
    auto inPentagon = [&](int i, int j) {   // есть путь i–a–b–c–j из четырёх рёбер без повторов
        for (int a : adj[i]) if (a != j) for (int b : adj[a]) if (b != i && b != j) for (int c : adj[b]) if (c != a && c != i && linked(c, j)) return true;
        return false;
    };
    for (auto& p : P) tA(m, E_C, p * sc);
    for (int i = 0; i < n; i++) for (int j : adj[i]) if (j > i) tB(m, i, j, inPentagon(i, j) ? 1 : 2);
}
static bool buildLibTmpl(int idx, Tmpl& m) {
    m = Tmpl(); if (idx < 0 || idx >= ML_N) return false;
    m.label = MOL_LIB_NAMES[idx];
    auto CH = [&](int c, int nH) { for (int k = 0; k < nH; k++) tB(m, c, tA(m, E_H)); };
    auto CX = [&](int c, int t, int n) { for (int k = 0; k < n; k++) tB(m, c, tA(m, t)); };   // n заместителей типа t
    auto di = [&](int t1, int t2, int o) { tB(m, tA(m, t1), tA(m, t2), o); };
    auto chain = [&](int n) {   // н-алкан CnH2n+2
        int prev = -1;
        for (int k = 0; k < n; k++) { int c = tA(m, E_C); if (prev >= 0) tB(m, prev, c); CH(c, (k == 0 || k == n - 1) ? 3 : 2); prev = c; }
    };
    auto OH = [&](int c) { int o = tA(m, E_O); tB(m, c, o); CH(o, 1); return o; };
    auto COOH = [&](int c) { int o = tA(m, E_O); tB(m, c, o, 2); OH(c); };   // c — углерод карбоксила
    const int tF = typeOfZ(9), tB5 = typeOfZ(5), tSi = typeOfZ(14), tP = typeOfZ(15), tS = typeOfZ(16), tGe = typeOfZ(32), tAs = typeOfZ(33),
              tSe = typeOfZ(34), tBr = typeOfZ(35), tI = typeOfZ(53);
    int fixedN = 0;   // > 0: первые fixedN атомов (кольцо) уже расставлены
    bool tree = true;
    switch (idx) {
    case ML_H2O: { int o = tA(m, E_O); CH(o, 2); break; }
    case ML_H2O2: { int o1 = tA(m, E_O), o2 = tA(m, E_O); tB(m, o1, o2); CH(o1, 1); CH(o2, 1); break; }
    case ML_CO2: { int c = tA(m, E_C); tB(m, c, tA(m, E_O), 2); tB(m, c, tA(m, E_O), 2); break; }
    case ML_CO: di(E_C, E_O, 2); break;
    case ML_NH3: { int n = tA(m, E_N); CH(n, 3); break; }
    case ML_HCL: di(E_H, E_CL, 1); break;
    case ML_HF: di(E_H, tF, 1); break;
    case ML_HBR: di(E_H, tBr, 1); break;
    case ML_HI: di(E_H, tI, 1); break;
    case ML_H2S: { int s = tA(m, tS); CH(s, 2); break; }
    case ML_PH3: { int p = tA(m, tP); CH(p, 3); break; }
    case ML_SIH4: { int s = tA(m, tSi); CH(s, 4); break; }
    case ML_HCN: { int c = tA(m, E_C), n = tA(m, E_N); tB(m, c, n, 3); CH(c, 1); break; }
    case ML_N2H4: { int n1 = tA(m, E_N), n2 = tA(m, E_N); tB(m, n1, n2); CH(n1, 2); CH(n2, 2); break; }
    case ML_O3: { int a = tA(m, E_O), b = tA(m, E_O), c = tA(m, E_O, {0, 0, 0}, -1); tB(m, b, a, 2); tB(m, b, c, 1); m.a[b].fc = 1; break; }   // O=O⁺–O⁻
    case ML_NO: di(E_N, E_O, 2); break;
    case ML_NO2: { int n = tA(m, E_N); tB(m, n, tA(m, E_O), 2); tB(m, n, tA(m, E_O), 1); break; }
    case ML_HNO2: { int n = tA(m, E_N); tB(m, n, tA(m, E_O), 2); OH(n); break; }
    case ML_NH2OH: { int n = tA(m, E_N); OH(n); CH(n, 2); break; }
    case ML_NH2CL: { int n = tA(m, E_N); CX(n, E_CL, 1); CH(n, 2); break; }
    case ML_HOCL: { int o = tA(m, E_O); CX(o, E_CL, 1); CH(o, 1); break; }
    case ML_CL2O: { int o = tA(m, E_O); CX(o, E_CL, 2); break; }
    case ML_OF2: { int o = tA(m, E_O); CX(o, tF, 2); break; }
    case ML_NF3: { int n = tA(m, E_N); CX(n, tF, 3); break; }
    case ML_NCL3: { int n = tA(m, E_N); CX(n, E_CL, 3); break; }
    case ML_PF3: { int p = tA(m, tP); CX(p, tF, 3); break; }
    case ML_PCL3: { int p = tA(m, tP); CX(p, E_CL, 3); break; }
    case ML_ASH3: { int a = tA(m, tAs); CH(a, 3); break; }
    case ML_H2SE: { int s = tA(m, tSe); CH(s, 2); break; }
    case ML_SCL2: { int s = tA(m, tS); CX(s, E_CL, 2); break; }
    case ML_CS2: { int c = tA(m, E_C); tB(m, c, tA(m, tS), 2); tB(m, c, tA(m, tS), 2); break; }
    case ML_COS: { int c = tA(m, E_C); tB(m, c, tA(m, E_O), 2); tB(m, c, tA(m, tS), 2); break; }
    case ML_SIF4: { int s = tA(m, tSi); CX(s, tF, 4); break; }
    case ML_SICL4: { int s = tA(m, tSi); CX(s, E_CL, 4); break; }
    case ML_GEH4: { int g = tA(m, tGe); CH(g, 4); break; }
    case ML_BF3: { int b = tA(m, tB5); CX(b, tF, 3); break; }
    case ML_BCL3: { int b = tA(m, tB5); CX(b, E_CL, 3); break; }
    case ML_N2: di(E_N, E_N, 3); break;
    case ML_O2: di(E_O, E_O, 2); break;
    case ML_H2: di(E_H, E_H, 1); break;
    case ML_F2: di(tF, tF, 1); break;
    case ML_CL2: di(E_CL, E_CL, 1); break;
    case ML_BR2: di(tBr, tBr, 1); break;
    case ML_I2: di(tI, tI, 1); break;
    case ML_CLF: di(E_CL, tF, 1); break;
    case ML_ICL: di(tI, E_CL, 1); break;
    // переменная валентность (облака VSEPR задают форму сами)
    case ML_SO2: { int s = tA(m, tS); tB(m, s, tA(m, E_O), 2); tB(m, s, tA(m, E_O), 2); break; }
    case ML_SO3: { int s = tA(m, tS); for (int k = 0; k < 3; k++) tB(m, s, tA(m, E_O), 2); break; }
    case ML_H2SO4: { int s = tA(m, tS); tB(m, s, tA(m, E_O), 2); tB(m, s, tA(m, E_O), 2); OH(s); OH(s); break; }
    case ML_H3PO4: { int p = tA(m, tP); tB(m, p, tA(m, E_O), 2); OH(p); OH(p); OH(p); break; }
    case ML_HNO3: { int n = tA(m, E_N, {0, 0, 0}, 1); tB(m, n, tA(m, E_O), 2); tB(m, n, tA(m, E_O, {0, 0, 0}, -1)); OH(n); break; }
    case ML_HCLO4: { int c = tA(m, E_CL); for (int k = 0; k < 3; k++) tB(m, c, tA(m, E_O), 2); OH(c); break; }
    case ML_POCL3: { int p = tA(m, tP); tB(m, p, tA(m, E_O), 2); CX(p, E_CL, 3); break; }
    case ML_SOCL2: { int s = tA(m, tS); tB(m, s, tA(m, E_O), 2); CX(s, E_CL, 2); break; }
    case ML_SF4: { int s = tA(m, tS); CX(s, tF, 4); break; }
    case ML_SF6: { int s = tA(m, tS); CX(s, tF, 6); break; }
    case ML_PF5: { int p = tA(m, tP); CX(p, tF, 5); break; }
    case ML_PCL5: { int p = tA(m, tP); CX(p, E_CL, 5); break; }
    case ML_CLF3: { int c = tA(m, E_CL); CX(c, tF, 3); break; }
    case ML_BRF5: { int b = tA(m, tBr); CX(b, tF, 5); break; }
    case ML_IF5: { int i5 = tA(m, tI); CX(i5, tF, 5); break; }
    case ML_XEF2: { int x = tA(m, typeOfZ(54)); CX(x, tF, 2); break; }
    case ML_XEF4: { int x = tA(m, typeOfZ(54)); CX(x, tF, 4); break; }
    case ML_XEO3: { int x = tA(m, typeOfZ(54)); for (int k = 0; k < 3; k++) tB(m, x, tA(m, E_O), 2); break; }
    // углеводороды
    case ML_CH4: { int c = tA(m, E_C); CH(c, 4); break; }
    case ML_C2H6: chain(2); break;
    case ML_C3H8: chain(3); break;
    case ML_C4H10: chain(4); break;
    case ML_C5H12: chain(5); break;
    case ML_C6H14: chain(6); break;
    case ML_ISOBUTANE: { int c = tA(m, E_C); for (int k = 0; k < 3; k++) { int r = tA(m, E_C); tB(m, c, r); CH(r, 3); } CH(c, 1); break; }
    case ML_C2H4: { int c1 = tA(m, E_C), c2 = tA(m, E_C); tB(m, c1, c2, 2); CH(c1, 2); CH(c2, 2); break; }
    case ML_C3H6: { int c1 = tA(m, E_C), c2 = tA(m, E_C), c3 = tA(m, E_C); tB(m, c1, c2, 2); tB(m, c2, c3); CH(c1, 2); CH(c2, 1); CH(c3, 3); break; }
    case ML_C4H6: { int c[4]; for (int& ci : c) ci = tA(m, E_C);
        tB(m, c[0], c[1], 2); tB(m, c[1], c[2]); tB(m, c[2], c[3], 2); CH(c[0], 2); CH(c[1], 1); CH(c[2], 1); CH(c[3], 2); break; }
    case ML_C2H2: { int c1 = tA(m, E_C), c2 = tA(m, E_C); tB(m, c1, c2, 3); CH(c1, 1); CH(c2, 1); break; }
    case ML_C3H4: { int c1 = tA(m, E_C), c2 = tA(m, E_C), c3 = tA(m, E_C); tB(m, c1, c2, 3); tB(m, c2, c3); CH(c1, 1); CH(c3, 3); break; }
    case ML_CYCLOPENTANE: case ML_CYCLOHEXANE: {
        const int n = idx == ML_CYCLOPENTANE ? 5 : 6;
        ringTmpl(m, std::vector<int>(n, E_C), std::vector<int>(n, 1)); for (int k = 0; k < n; k++) CH(k, 2);
        fixedN = n; break; }
    case ML_C6H6: case ML_PHENOL: case ML_TOLUENE: case ML_STYRENE: case ML_ANILINE: case ML_BENZOIC: case ML_PYRIDINE: {
        // шестиугольник Кекуле; у пиридина атом 0 — азот (без водорода), у остальных на нём заместитель
        std::vector<int> ty(6, E_C); if (idx == ML_PYRIDINE) ty[0] = E_N;
        ringTmpl(m, ty, {2, 1, 2, 1, 2, 1});
        switch (idx) {
        case ML_C6H6: CH(0, 1); break;
        case ML_PHENOL: OH(0); break;
        case ML_TOLUENE: { int c = tA(m, E_C); tB(m, 0, c); CH(c, 3); break; }
        case ML_STYRENE: { int c1 = tA(m, E_C), c2 = tA(m, E_C); tB(m, 0, c1); tB(m, c1, c2, 2); CH(c1, 1); CH(c2, 2); break; }
        case ML_ANILINE: { int n = tA(m, E_N); tB(m, 0, n); CH(n, 2); break; }
        case ML_BENZOIC: { int c = tA(m, E_C); tB(m, 0, c); COOH(c); break; }
        }
        for (int k = 1; k < 6; k++) CH(k, 1);
        fixedN = 6; break; }
    case ML_NAPHTHALENE: {   // два шестиугольника с общим ребром 0–5
        const double L = r0of(E_C, E_C, 1) * 0.935, h = L * std::sqrt(3.0) / 2;
        const V3 p[10] = {{0, L / 2, 0}, {-h, L, 0}, {-2 * h, L / 2, 0}, {-2 * h, -L / 2, 0}, {-h, -L, 0}, {0, -L / 2, 0},
                          {h, -L, 0}, {2 * h, -L / 2, 0}, {2 * h, L / 2, 0}, {h, L, 0}};
        for (auto& q : p) tA(m, E_C, q);
        const int bonds[11][3] = {{0, 1, 1}, {1, 2, 2}, {2, 3, 1}, {3, 4, 2}, {4, 5, 1}, {5, 0, 1}, {5, 6, 2}, {6, 7, 1}, {7, 8, 2}, {8, 9, 1}, {9, 0, 2}};
        for (auto& b : bonds) tB(m, b[0], b[1], b[2]);
        for (int k : {1, 2, 3, 4, 6, 7, 8, 9}) CH(k, 1);
        fixedN = 10; break; }
    // кислородные, азотные и галогенпроизводные
    case ML_CH3OH: { int c = tA(m, E_C); OH(c); CH(c, 3); break; }
    case ML_C2H5OH: { int c1 = tA(m, E_C), c2 = tA(m, E_C); tB(m, c1, c2); OH(c2); CH(c1, 3); CH(c2, 2); break; }
    case ML_IPA: { int c = tA(m, E_C); for (int k = 0; k < 2; k++) { int r = tA(m, E_C); tB(m, c, r); CH(r, 3); } OH(c); CH(c, 1); break; }
    case ML_GLYCOL: { int c1 = tA(m, E_C), c2 = tA(m, E_C); tB(m, c1, c2); OH(c1); OH(c2); CH(c1, 2); CH(c2, 2); break; }
    case ML_GLYCEROL: { int c1 = tA(m, E_C), c2 = tA(m, E_C), c3 = tA(m, E_C); tB(m, c1, c2); tB(m, c2, c3);
        OH(c1); OH(c2); OH(c3); CH(c1, 2); CH(c2, 1); CH(c3, 2); break; }
    case ML_DME: { int c1 = tA(m, E_C), o = tA(m, E_O), c2 = tA(m, E_C); tB(m, c1, o); tB(m, o, c2); CH(c1, 3); CH(c2, 3); break; }
    case ML_DEE: { int o = tA(m, E_O);
        for (int k = 0; k < 2; k++) { int c1 = tA(m, E_C), c2 = tA(m, E_C); tB(m, o, c1); tB(m, c1, c2); CH(c1, 2); CH(c2, 3); }
        break; }
    case ML_HCHO: { int c = tA(m, E_C); tB(m, c, tA(m, E_O), 2); CH(c, 2); break; }
    case ML_CH3CHO: { int c1 = tA(m, E_C), c2 = tA(m, E_C); tB(m, c1, c2); tB(m, c2, tA(m, E_O), 2); CH(c1, 3); CH(c2, 1); break; }
    case ML_ACETONE: { int c = tA(m, E_C); tB(m, c, tA(m, E_O), 2); for (int k = 0; k < 2; k++) { int r = tA(m, E_C); tB(m, c, r); CH(r, 3); } break; }
    case ML_HCOOH: { int c = tA(m, E_C); COOH(c); CH(c, 1); break; }
    case ML_CH3COOH: { int c1 = tA(m, E_C), c2 = tA(m, E_C); tB(m, c1, c2); COOH(c2); CH(c1, 3); break; }
    case ML_OXALIC: { int c1 = tA(m, E_C), c2 = tA(m, E_C); tB(m, c1, c2); COOH(c1); COOH(c2); break; }
    case ML_LACTIC: { int c1 = tA(m, E_C), c2 = tA(m, E_C), c3 = tA(m, E_C); tB(m, c1, c2); tB(m, c2, c3); CH(c1, 3); OH(c2); CH(c2, 1); COOH(c3); break; }
    case ML_ETOAC: { int c1 = tA(m, E_C), c2 = tA(m, E_C), o = tA(m, E_O), c3 = tA(m, E_C), c4 = tA(m, E_C);
        tB(m, c1, c2); tB(m, c2, tA(m, E_O), 2); tB(m, c2, o); tB(m, o, c3); tB(m, c3, c4); CH(c1, 3); CH(c3, 2); CH(c4, 3); break; }
    case ML_CH3CN: { int c1 = tA(m, E_C), c2 = tA(m, E_C); tB(m, c1, c2); tB(m, c2, tA(m, E_N), 3); CH(c1, 3); break; }
    case ML_HCONH2: { int c = tA(m, E_C), n = tA(m, E_N); tB(m, c, tA(m, E_O), 2); tB(m, c, n); CH(c, 1); CH(n, 2); break; }
    case ML_CH3CL: { int c = tA(m, E_C); CX(c, E_CL, 1); CH(c, 3); break; }
    case ML_CH2CL2: { int c = tA(m, E_C); CX(c, E_CL, 2); CH(c, 2); break; }
    case ML_CHCL3: { int c = tA(m, E_C); CX(c, E_CL, 3); CH(c, 1); break; }
    case ML_CCL4: { int c = tA(m, E_C); CX(c, E_CL, 4); break; }
    case ML_C2H5CL: { int c1 = tA(m, E_C), c2 = tA(m, E_C); tB(m, c1, c2); CX(c2, E_CL, 1); CH(c1, 3); CH(c2, 2); break; }
    case ML_C2H3CL: { int c1 = tA(m, E_C), c2 = tA(m, E_C); tB(m, c1, c2, 2); CX(c2, E_CL, 1); CH(c1, 2); CH(c2, 1); break; }
    case ML_CF4: { int c = tA(m, E_C); CX(c, tF, 4); break; }
    case ML_CCL2F2: { int c = tA(m, E_C); CX(c, E_CL, 2); CX(c, tF, 2); break; }
    case ML_CH3SH: { int c = tA(m, E_C), s = tA(m, tS); tB(m, c, s); CH(c, 3); CH(s, 1); break; }
    case ML_CH3NH2: { int c = tA(m, E_C), n = tA(m, E_N); tB(m, c, n); CH(c, 3); CH(n, 2); break; }
    case ML_UREA: { int c = tA(m, E_C); tB(m, c, tA(m, E_O), 2); for (int k = 0; k < 2; k++) { int n = tA(m, E_N); tB(m, c, n); CH(n, 2); } break; }
    case ML_GLYCINE: case ML_ALANINE: { int n = tA(m, E_N), ca = tA(m, E_C), c = tA(m, E_C); tB(m, n, ca); tB(m, ca, c); CH(n, 2); COOH(c);
        if (idx == ML_ALANINE) { int r = tA(m, E_C); tB(m, ca, r); CH(r, 3); CH(ca, 1); } else CH(ca, 2);
        break; }
    // ионы (формальный заряд — в fc атома шаблона)
    case ML_H3O: { int o = tA(m, E_O, {0, 0, 0}, 1); CH(o, 3); break; }
    case ML_OH: { int o = tA(m, E_O, {0, 0, 0}, -1); CH(o, 1); break; }
    case ML_NH4: { int n = tA(m, E_N, {0, 0, 0}, 1); CH(n, 4); break; }
    case ML_CN: { int c = tA(m, E_C, {0, 0, 0}, -1); tB(m, c, tA(m, E_N), 3); break; }
    case ML_HS: { int s = tA(m, tS, {0, 0, 0}, -1); CH(s, 1); break; }
    case ML_CLO: { int o = tA(m, E_O, {0, 0, 0}, -1); CX(o, E_CL, 1); break; }
    case ML_NO2M: { int n = tA(m, E_N); tB(m, n, tA(m, E_O), 2); tB(m, n, tA(m, E_O, {0, 0, 0}, -1)); break; }
    case ML_HCOO: case ML_CH3COO: { int c = tA(m, E_C); tB(m, c, tA(m, E_O), 2); tB(m, c, tA(m, E_O, {0, 0, 0}, -1));
        if (idx == ML_HCOO) CH(c, 1); else { int r = tA(m, E_C); tB(m, c, r); CH(r, 3); }
        break; }
    case ML_HCO3: { int c = tA(m, E_C); tB(m, c, tA(m, E_O), 2); tB(m, c, tA(m, E_O, {0, 0, 0}, -1)); OH(c); break; }
    case ML_SO4: { int s = tA(m, tS); tB(m, s, tA(m, E_O), 2); tB(m, s, tA(m, E_O), 2); tB(m, s, tA(m, E_O, {0, 0, 0}, -1)); tB(m, s, tA(m, E_O, {0, 0, 0}, -1)); break; }
    case ML_PO4: { int p = tA(m, tP); tB(m, p, tA(m, E_O), 2); for (int k = 0; k < 3; k++) tB(m, p, tA(m, E_O, {0, 0, 0}, -1)); break; }
    case ML_NO3: { int n = tA(m, E_N, {0, 0, 0}, 1); tB(m, n, tA(m, E_O), 2); tB(m, n, tA(m, E_O, {0, 0, 0}, -1)); tB(m, n, tA(m, E_O, {0, 0, 0}, -1)); break; }
    case ML_CLO4: { int c = tA(m, E_CL); for (int k = 0; k < 3; k++) tB(m, c, tA(m, E_O), 2); tB(m, c, tA(m, E_O, {0, 0, 0}, -1)); break; }
    default: tree = false; break;
    }
    if (tree) { vseprPlace(m, fixedN); relaxTmpl(m); centerTmpl(m); return true; }
    switch (idx) {
    case ML_NAOH: { const double r = r0of(E_O, E_H, 1); tA(m, E_NA, {-0.88, 0, 0}); int o = tA(m, E_O, {0, 0, 0}, -1); int h = tA(m, E_H, {r, 0, 0}); tB(m, o, h); break; }
    case ML_NACL: { tA(m, E_NA, {-0.46, 0, 0}); tA(m, E_CLM, {0.46, 0, 0}); break; }
    case ML_NACL_CRYST: {
        const double a = 0.92;
        for (int k = 0; k < 4; k++) for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) tA(m, ((i + j + k) & 1) ? E_CLM : E_NA, {i * a, j * a, k * a});
        break; }
    case ML_DIAMOND: case ML_SI_CRYST: {   // решётка алмаза в шаре, поверхность закрыта водородом
        const int tc = idx == ML_DIAMOND ? E_C : tSi;
        const double r = r0of(tc, tc, 1), a = 4 * r / std::sqrt(3.0), R = 2.43 * r;   // у алмаза R ≈ 1.1σ
        const double fcc[4][3] = {{0, 0, 0}, {0, 0.5, 0.5}, {0.5, 0, 0.5}, {0.5, 0.5, 0}};
        std::vector<int> isB;
        for (int z = -2; z <= 2; z++) for (int y = -2; y <= 2; y++) for (int x = -2; x <= 2; x++) for (int f = 0; f < 4; f++) for (int s = 0; s < 2; s++) {
            V3 p{(x + fcc[f][0] + 0.25 * s) * a, (y + fcc[f][1] + 0.25 * s) * a, (z + fcc[f][2] + 0.25 * s) * a};
            p = p - V3{0.125 * a, 0.125 * a, 0.125 * a};
            if (vdot(p, p) <= R * R) { tA(m, tc, p); isB.push_back(s); }
        }
        // связи между соседями; атомы с одной связью убираются (повторять, пока есть)
        for (int it = 0; it < 4; it++) {
            std::vector<int> deg(m.a.size(), 0);
            for (size_t i = 0; i < m.a.size(); i++) for (size_t j = i + 1; j < m.a.size(); j++) {
                V3 dd{m.a[i].x - m.a[j].x, m.a[i].y - m.a[j].y, m.a[i].z - m.a[j].z}; if (std::fabs(std::sqrt(vdot(dd, dd)) - r) < 0.1 * r) { deg[i]++; deg[j]++; } }
            Tmpl n2; std::vector<int> nb2;
            for (size_t i = 0; i < m.a.size(); i++) if (deg[i] >= 2) { n2.a.push_back(m.a[i]); nb2.push_back(isB[i]); }
            bool same = n2.a.size() == m.a.size(); m.a = n2.a; isB = nb2; if (same) break;
        }
        for (size_t i = 0; i < m.a.size(); i++) for (size_t j = i + 1; j < m.a.size(); j++) {
            V3 dd{m.a[i].x - m.a[j].x, m.a[i].y - m.a[j].y, m.a[i].z - m.a[j].z}; if (std::fabs(std::sqrt(vdot(dd, dd)) - r) < 0.1 * r) tB(m, (int)i, (int)j); }
        const int nC = (int)m.a.size();
        const double s3 = 1 / std::sqrt(3.0);
        for (int i = 0; i < nC; i++) {
            const double sg = isB[i] ? -1 : 1;
            capWithH(m, i, {V3{s3, s3, s3} * sg, V3{s3, -s3, -s3} * sg, V3{-s3, s3, -s3} * sg, V3{-s3, -s3, s3} * sg});
        }
        break; }
    case ML_GRAPHENE: {   // соты sp2: двойные связи по паросочетанию (структура Кекуле), край — C–H
        const double r = 1.44 / 3.405, R = 1.9;
        std::vector<int> sub;
        const double ax = r * std::sqrt(3.0);
        for (int j = -8; j <= 8; j++) for (int i = -8; i <= 8; i++) for (int s = 0; s < 2; s++) {
            V3 p{(i + 0.5 * j) * ax, j * 1.5 * r + (s ? r : 0), 0};
            p = p - V3{0, 0.5 * r, 0};
            if (vdot(p, p) <= R * R) { tA(m, E_C, p); sub.push_back(s); }
        }
        for (int it = 0; it < 4; it++) {
            std::vector<int> deg(m.a.size(), 0);
            for (size_t i = 0; i < m.a.size(); i++) for (size_t j = i + 1; j < m.a.size(); j++) {
                V3 dd{m.a[i].x - m.a[j].x, m.a[i].y - m.a[j].y, 0}; if (std::fabs(std::sqrt(vdot(dd, dd)) - r) < 0.1 * r) { deg[i]++; deg[j]++; } }
            Tmpl n2; std::vector<int> s2;
            for (size_t i = 0; i < m.a.size(); i++) if (deg[i] >= 2) { n2.a.push_back(m.a[i]); s2.push_back(sub[i]); }
            bool same = n2.a.size() == m.a.size(); m.a = n2.a; sub = s2; if (same) break;
        }
        const int nC = (int)m.a.size();
        std::vector<std::vector<int>> adj(nC);
        for (int i = 0; i < nC; i++) for (int j = i + 1; j < nC; j++) {
            V3 dd{m.a[i].x - m.a[j].x, m.a[i].y - m.a[j].y, 0}; if (std::fabs(std::sqrt(vdot(dd, dd)) - r) < 0.1 * r) { adj[i].push_back(j); adj[j].push_back(i); } }
        // максимальное паросочетание (подрешётки A/B — двудольный граф): чередующиеся пути
        std::vector<int> mate(nC, -1);
        std::function<bool(int, std::vector<char>&)> aug = [&](int u, std::vector<char>& vis) {
            for (int v : adj[u]) { if (vis[v]) continue; vis[v] = 1; if (mate[v] < 0 || aug(mate[v], vis)) { mate[v] = u; mate[u] = v; return true; } }
            return false;
        };
        for (int u = 0; u < nC; u++) if (sub[u] == 0 && mate[u] < 0) { std::vector<char> vis(nC, 0); aug(u, vis); }
        for (int i = 0; i < nC; i++) for (int j : adj[i]) if (j > i) tB(m, i, j, mate[i] == j ? 2 : 1);
        for (int i = 0; i < nC; i++) if (adj[i].size() == 2) {   // край: H по биссектрисе наружу
            V3 pi{m.a[i].x, m.a[i].y, 0}, a1{m.a[adj[i][0]].x, m.a[adj[i][0]].y, 0}, a2{m.a[adj[i][1]].x, m.a[adj[i][1]].y, 0};
            V3 d = vunit((pi - a1) + (pi - a2)); capWithH(m, i, {d});
        }
        break; }
    case ML_C60: fullereneTmpl(m); break;
    case ML_ICE: {
        const double a = 2.08, rOH = r0of(E_O, E_H, 1), s3 = 1.0 / std::sqrt(3.0);
        const double fcc[4][3] = {{0, 0, 0}, {0, 0.5, 0.5}, {0.5, 0, 0.5}, {0.5, 0.5, 0}};
        const double dd[4][3] = {{1, 1, 1}, {1, -1, -1}, {-1, 1, -1}, {-1, -1, 1}};
        for (int k = 0; k < 2; k++) for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) for (int f = 0; f < 4; f++) for (int sub = 0; sub < 2; sub++) {
            V3 po{(i + fcc[f][0] + 0.125 + 0.25 * sub) * a, (j + fcc[f][1] + 0.125 + 0.25 * sub) * a, (k + fcc[f][2] + 0.125 + 0.25 * sub) * a};
            int o = tA(m, E_O, po); double sg = sub == 0 ? 1 : -1;
            for (int h : {sub == 0 ? 0 : 2, sub == 0 ? 1 : 3}) { int hi = tA(m, E_H, po + V3{dd[h][0], dd[h][1], dd[h][2]} * (sg * rOH * s3)); tB(m, o, hi); }
        }
        break; }
    case ML_SIO2: {   // кристобалит: Si в узлах алмаза, O посередине связей Si–Si; свободные O — группы OH
        const double rSO = r0of(tSi, E_O, 1), rOH = r0of(E_O, E_H, 1), a = 8 * rSO / std::sqrt(3.0), R = 1.8;
        const double fcc[4][3] = {{0, 0, 0}, {0, 0.5, 0.5}, {0.5, 0, 0.5}, {0.5, 0.5, 0}};
        std::vector<V3> si; std::vector<int> sb2;
        for (int z = -2; z <= 2; z++) for (int y = -2; y <= 2; y++) for (int x = -2; x <= 2; x++) for (int f = 0; f < 4; f++) for (int s = 0; s < 2; s++) {
            V3 p{(x + fcc[f][0] + 0.25 * s - 0.125) * a, (y + fcc[f][1] + 0.25 * s - 0.125) * a, (z + fcc[f][2] + 0.25 * s - 0.125) * a};
            if (vdot(p, p) <= R * R) { si.push_back(p); sb2.push_back(s); }
        }
        std::vector<int> siIdx; for (auto& p : si) siIdx.push_back(tA(m, tSi, p));
        const double s3 = 1 / std::sqrt(3.0);
        std::vector<std::pair<V3, int>> oxy;   // позиции O и индекс
        for (size_t k = 0; k < si.size(); k++) {
            const double sg = sb2[k] ? -1 : 1;
            for (V3 d : {V3{s3, s3, s3}, V3{s3, -s3, -s3}, V3{-s3, s3, -s3}, V3{-s3, -s3, s3}}) {
                V3 po = si[k] + d * (sg * rSO); int oi = -1;
                for (auto& q : oxy) { V3 dd = q.first - po; if (vdot(dd, dd) < 1e-4) { oi = q.second; break; } }
                if (oi < 0) { oi = tA(m, E_O, po); oxy.push_back({po, oi}); }
                tB(m, siIdx[k], oi);
            }
        }
        for (auto& q : oxy) if (bondCount(m, q.second) == 1) {   // O–H на поверхности (угол Si–O–H ≈ 104.5°)
            int s = -1; for (auto& b : m.b) { if (b[1] == q.second) s = b[0]; }
            V3 u = vunit(V3{m.a[s].x, m.a[s].y, m.a[s].z} - q.first);
            V3 g = std::fabs(u.z) < 0.9 ? V3{0, 0, 1} : V3{0, 1, 0};
            V3 w = vunit(g - u * vdot(g, u)); double th = 104.5 * PI / 180;
            int h = tA(m, E_H, q.first + (u * std::cos(th) + w * std::sin(th)) * rOH); tB(m, q.second, h);
        }
        break; }
    case ML_PT: metalCluster(m, typeOfZ(78), 2.05); break;
    case ML_AU: metalCluster(m, typeOfZ(79), 2.05); break;
    case ML_FE: metalCluster(m, typeOfZ(26), 2.05); break;
    case ML_NI: metalCluster(m, typeOfZ(28), 2.05); break;
    case ML_CU: metalCluster(m, typeOfZ(29), 2.05); break;
    case ML_AG: metalCluster(m, typeOfZ(47), 2.05); break;
    case ML_AL: metalCluster(m, typeOfZ(13), 2.05); break;
    case ML_PD: metalCluster(m, typeOfZ(46), 2.05); break;
    case ML_RH: metalCluster(m, typeOfZ(45), 2.05); break;
    case ML_PB: metalCluster(m, typeOfZ(82), 2.05); break;
    default: return false;
    }
    relaxTmpl(m);
    centerTmpl(m);
    return !m.a.empty();
}
static Tmpl libTmpl(int idx) { Tmpl m; buildLibTmpl(idx, m); return m; }
// Вставить молекулу/структуру библиотеки в точку (x, y, z) со случайной ориентацией и тепловой скоростью P.Tset.
// Сама пересчитывает присутствие типов, силы и опорную энергию. false — не поместилась (перекрытие, стенки).
// Отмену (pushUndo) делает вызывающий.
static bool insertMolecule(int idx, double x, double y, double z) {
    Tmpl m; if (!buildLibTmpl(idx, m)) return false;
    bool ok = false;
    for (int tr = 0; tr < 16 && !ok; tr++) {
        double j = 0.35 * tr;
        ok = placeMol(m, x + j * (urand() - 0.5), y + j * (urand() - 0.5), z + j * (urand() - 0.5), P.Tset, 0.8);
    }
    if (ok) { updatePresence(); computeForces(); resetEnergyRef(); }
    return ok;
}

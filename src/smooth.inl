// ===================================== ПОКАЗ: СРЕДНЯЯ ФОРМА МОЛЕКУЛ =========================
// Связи с водородом колеблются с периодом 9–11 фс, валентные углы — 20–30 фс, а за один кадр модель проходит 4–16 фс:
// снятые с такой выдержкой атомы H мечутся от кадра к кадру. В природе при комнатной температуре этой дрожи нет:
// квант колебания ħω в 7–17 раз больше kT, молекула сидит в основном колебательном состоянии, и её средняя форма
// неподвижна. Поэтому на экране форма каждой молекулы сглажена по времени (τ = 60 фс), поворот — меньше
// (30 фс: гаснут частые качания молекул воды в клетке из соседей), а перенос молекулы как целого показывается без задержки.
// Расчёт это не меняет: сглаживаются только координаты для рисования.
namespace vsm {
constexpr double TAU_SHAPE = 0.060;   // τ формы (в единицах времени модели ≈ пс)
constexpr double TAU_TURN = 0.030;    // τ поворота
constexpr double MAX_OFF = 0.25;      // рисуемый атом отходит от настоящего не дальше 0.25σ (0.85 Å)
}
static std::vector<double> gX, gY, gZ;                  // координаты для рисования (в ящике, как S.x)
static std::vector<double> smBx, smBy, smBz;            // сглаженная форма: координаты атома в системе своей молекулы
static std::vector<double> smOx, smOy, smOz;            // сдвиг рисуемого атома от настоящего в прошлом кадре
static std::vector<std::array<double, 4>> smQ;          // сглаженный поворот молекулы (кватернион), у каждого её атома
static std::vector<unsigned> smKey;                     // подпись молекулы атома в прошлом кадре: другая — форма заново
static std::vector<double> smRx, smRy, smRz;            // настоящие координаты в прошлом кадре (правки на паузе)
static std::vector<int> smPar, smStart, smList, smSeen;
static std::vector<double> smPx, smPy, smPz;            // координаты молекулы без разрыва на периодической границе
static int smN = -1, smStamp = 0; static long long smStep = -1; static double smT = 0;

static inline void quatMat(const double* q, double R[3][3]) {
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    R[0][0] = 1 - 2 * (y * y + z * z); R[0][1] = 2 * (x * y - w * z); R[0][2] = 2 * (x * z + w * y);
    R[1][0] = 2 * (x * y + w * z); R[1][1] = 1 - 2 * (x * x + z * z); R[1][2] = 2 * (y * z - w * x);
    R[2][0] = 2 * (x * z - w * y); R[2][1] = 2 * (y * z + w * x); R[2][2] = 1 - 2 * (x * x + y * y);
}
// Поворот, лучше всего переводящий форму b в текущие координаты r (метод Хорна, 1987): кватернион — собственный вектор
// наибольшего собственного значения матрицы 4×4 из сумм Σm·b·rᵀ. У линейной молекулы поворот вокруг её оси не определён —
// из равноправных берётся ближайший к прошлому (qPrev), иначе рисунок крутился бы вокруг оси
static void fitTurn(const double S3[3][3], const double* qPrev, double* q) {
    const double Sxx = S3[0][0], Sxy = S3[0][1], Sxz = S3[0][2], Syx = S3[1][0], Syy = S3[1][1], Syz = S3[1][2], Szx = S3[2][0], Szy = S3[2][1], Szz = S3[2][2];
    double N[4][4] = {{Sxx + Syy + Szz, Syz - Szy, Szx - Sxz, Sxy - Syx},
                      {Syz - Szy, Sxx - Syy - Szz, Sxy + Syx, Szx + Sxz},
                      {Szx - Sxz, Sxy + Syx, -Sxx + Syy - Szz, Syz + Szy},
                      {Sxy - Syx, Szx + Sxz, Syz + Szy, -Sxx - Syy + Szz}};
    double V[4][4], d[4]; jacobiEigen<4>(N, V, d);
    int o[4] = {0, 1, 2, 3}; std::sort(o, o + 4, [&](int a, int b) { return d[a] > d[b]; });
    const double scale = std::fabs(d[o[0]]) + std::fabs(d[o[3]]) + 1e-300;
    double v[4] = {0, 0, 0, 0};
    for (int m = 0; m < 4; m++) {   // векторы с (почти) тем же наибольшим значением: проекция прошлого поворота на них
        if (m > 0 && d[o[0]] - d[o[m]] > 1e-3 * scale) break;
        double c = 0; for (int k = 0; k < 4; k++) c += qPrev[k] * V[k][o[m]];
        for (int k = 0; k < 4; k++) v[k] += c * V[k][o[m]];
    }
    double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
    if (l < 1e-9) { for (int k = 0; k < 4; k++) v[k] = V[k][o[0]]; l = 1; }
    const double sg = (v[0] * qPrev[0] + v[1] * qPrev[1] + v[2] * qPrev[2] + v[3] * qPrev[3]) < 0 ? -1.0 : 1.0;
    for (int k = 0; k < 4; k++) q[k] = sg * v[k] / l;
}

static void displayRaw() {
    const int n = S.n;
    gX.assign(S.x.begin(), S.x.begin() + n); gY.assign(S.y.begin(), S.y.begin() + n); gZ.assign(S.z.begin(), S.z.begin() + n);
}
// координаты для рисования; вызывается раз в кадр после шагов расчёта
static void displayUpdate() {
    const int n = S.n;
    if (!opt.smoothVib || n == 0) { displayRaw(); smN = -1; return; }
    bool reset = smN < 0 || n < smN || S.t < smT - 1e-12;   // атомы удалены (номера сдвинулись) или время пошло назад (отмена, загрузка)
    if (!reset && n == smN && S.step == smStep) {   // модель стоит: рисунок тот же, пока атомы не передвинули руками
        bool moved = false;
        for (int i = 0; i < n && !moved; i++) moved = S.x[i] != smRx[i] || S.y[i] != smRy[i] || S.z[i] != smRz[i];
        if (!moved) return;
        reset = true;
    }
    if (reset) {
        smKey.assign(n, 0u); smOx.assign(n, 0.0); smOy.assign(n, 0.0); smOz.assign(n, 0.0);
        smBx.assign(n, 0.0); smBy.assign(n, 0.0); smBz.assign(n, 0.0); smQ.assign(n, {1, 0, 0, 0});
    } else if (n > smN) {   // новые атомы — в конце списка, у старых всё сохраняется
        smKey.resize(n, 0u); smOx.resize(n, 0.0); smOy.resize(n, 0.0); smOz.resize(n, 0.0);
        smBx.resize(n, 0.0); smBy.resize(n, 0.0); smBz.resize(n, 0.0); smQ.resize(n, {1, 0, 0, 0});
    }
    const double dtm = reset ? 1e9 : std::max(0.0, S.t - smT);
    const double aShape = 1 - std::exp(-dtm / vsm::TAU_SHAPE), aTurn = 1 - std::exp(-dtm / vsm::TAU_TURN);
    smN = n; smStep = S.step; smT = S.t;
    smRx.assign(S.x.begin(), S.x.begin() + n); smRy.assign(S.y.begin(), S.y.begin() + n); smRz.assign(S.z.begin(), S.z.begin() + n);
    gX.resize(n); gY.resize(n); gZ.resize(n);
    // молекулы — компоненты связности по химическим связям; атомы каждой — подряд в smList
    smPar.resize(n); for (int i = 0; i < n; i++) smPar[i] = i;
    auto root = [&](int a) { while (smPar[a] != a) { smPar[a] = smPar[smPar[a]]; a = smPar[a]; } return a; };
    for (int i = 0; i < n; i++) for (int k = 0; k < S.nbc[i]; k++) { const int a = root(i), b = root(S.nb[i][k]); if (a != b) smPar[a] = b; }
    smStart.assign(n + 1, 0);
    for (int i = 0; i < n; i++) { smPar[i] = root(i); smStart[smPar[i] + 1]++; }
    for (int i = 0; i < n; i++) smStart[i + 1] += smStart[i];
    smList.resize(n);
    { std::vector<int> fill(smStart.begin(), smStart.end() - 1); for (int i = 0; i < n; i++) smList[fill[smPar[i]]++] = i; }
    smPx.resize(n); smPy.resize(n); smPz.resize(n);
    if ((int)smSeen.size() < n) smSeen.assign(n, 0);
    if (++smStamp > 2000000000) { std::fill(smSeen.begin(), smSeen.end(), 0); smStamp = 1; }
    std::vector<int> queue;
    for (int r = 0; r < n; r++) {
        const int a0 = smStart[r], k = smStart[r + 1] - a0; if (k == 0) continue;
        const int* L = &smList[a0];
        if (k == 1) {   // одиночный атом: колебаний формы нет; сдвиг, оставшийся от молекулы, плавно уходит
            const int i = L[0]; const double s = 1 - aShape;
            smOx[i] *= s; smOy[i] *= s; smOz[i] *= s; smKey[i] = 0;
            gX[i] = S.x[i] + smOx[i]; gY[i] = S.y[i] + smOy[i]; gZ[i] = S.z[i] + smOz[i];
            continue;
        }
        // координаты без разрыва: обход по связям от первого атома
        queue.clear(); queue.push_back(L[0]); smSeen[L[0]] = smStamp;
        smPx[L[0]] = S.x[L[0]]; smPy[L[0]] = S.y[L[0]]; smPz[L[0]] = S.z[L[0]];
        for (size_t h = 0; h < queue.size(); h++) {
            const int a = queue[h];
            for (int m = 0; m < S.nbc[a]; m++) {
                const int b = S.nb[a][m]; if (smSeen[b] == smStamp) continue;
                smSeen[b] = smStamp; double dx, dy, dz; dvec(a, b, dx, dy, dz);
                smPx[b] = smPx[a] + dx; smPy[b] = smPy[a] + dy; smPz[b] = smPz[a] + dz; queue.push_back(b);
            }
        }
        unsigned key = 2166136261u; for (int q = 0; q < k; q++) key = (key ^ (unsigned)L[q]) * 16777619u; key ^= (unsigned)k;
        bool known = true; for (int q = 0; q < k && known; q++) known = smKey[L[q]] == key;
        double M = 0, cx = 0, cy = 0, cz = 0;
        for (int q = 0; q < k; q++) { const int i = L[q]; const double m = EL[S.ty[i]].m; M += m; cx += m * smPx[i]; cy += m * smPy[i]; cz += m * smPz[i]; }
        cx /= M; cy /= M; cz /= M;
        if (!known) {
            // новая молекула (реакция, вставка): форма — из того, что было нарисовано в прошлом кадре, чтобы рисунок не прыгал
            double bx = 0, by = 0, bz = 0;
            for (int q = 0; q < k; q++) { const int i = L[q]; const double m = EL[S.ty[i]].m; bx += m * (smPx[i] + smOx[i]); by += m * (smPy[i] + smOy[i]); bz += m * (smPz[i] + smOz[i]); }
            bx /= M; by /= M; bz /= M;
            for (int q = 0; q < k; q++) {
                const int i = L[q];
                smBx[i] = smPx[i] + smOx[i] - bx; smBy[i] = smPy[i] + smOy[i] - by; smBz[i] = smPz[i] + smOz[i] - bz;
                smQ[i] = {1, 0, 0, 0}; smKey[i] = key;
            }
        }
        // поворот формы к текущим координатам
        double C[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
        for (int q = 0; q < k; q++) {
            const int i = L[q]; const double m = EL[S.ty[i]].m;
            const double b[3] = {smBx[i], smBy[i], smBz[i]}, rr[3] = {smPx[i] - cx, smPy[i] - cy, smPz[i] - cz};
            for (int u = 0; u < 3; u++) for (int v = 0; v < 3; v++) C[u][v] += m * b[u] * rr[v];
        }
        const std::array<double, 4> qs0 = smQ[L[0]];
        double qt[4]; fitTurn(C, qs0.data(), qt);
        double Rt[3][3]; quatMat(qt, Rt);
        // форма тянется к текущей (в системе молекулы), затем центр формы — в ноль
        double mb[3] = {0, 0, 0};
        for (int q = 0; q < k; q++) {
            const int i = L[q]; const double rr[3] = {smPx[i] - cx, smPy[i] - cy, smPz[i] - cz};
            const double tx = Rt[0][0] * rr[0] + Rt[1][0] * rr[1] + Rt[2][0] * rr[2];
            const double ty = Rt[0][1] * rr[0] + Rt[1][1] * rr[1] + Rt[2][1] * rr[2];
            const double tz = Rt[0][2] * rr[0] + Rt[1][2] * rr[1] + Rt[2][2] * rr[2];
            smBx[i] += aShape * (tx - smBx[i]); smBy[i] += aShape * (ty - smBy[i]); smBz[i] += aShape * (tz - smBz[i]);
            const double m = EL[S.ty[i]].m; mb[0] += m * smBx[i]; mb[1] += m * smBy[i]; mb[2] += m * smBz[i];
        }
        for (int q = 0; q < k; q++) { const int i = L[q]; smBx[i] -= mb[0] / M; smBy[i] -= mb[1] / M; smBz[i] -= mb[2] / M; }
        // поворот рисунка догоняет настоящий
        double qs[4] = {qs0[0], qs0[1], qs0[2], qs0[3]}, l = 0;
        for (int c = 0; c < 4; c++) { qs[c] += aTurn * (qt[c] - qs[c]); l += qs[c] * qs[c]; }
        l = 1 / std::sqrt(std::max(l, 1e-24)); for (int c = 0; c < 4; c++) qs[c] *= l;
        double Rs[3][3]; quatMat(qs, Rs);
        for (int q = 0; q < k; q++) {
            const int i = L[q];
            double ox = cx + Rs[0][0] * smBx[i] + Rs[0][1] * smBy[i] + Rs[0][2] * smBz[i] - smPx[i];
            double oy = cy + Rs[1][0] * smBx[i] + Rs[1][1] * smBy[i] + Rs[1][2] * smBz[i] - smPy[i];
            double oz = cz + Rs[2][0] * smBx[i] + Rs[2][1] * smBy[i] + Rs[2][2] * smBz[i] - smPz[i];
            const double o2 = ox * ox + oy * oy + oz * oz;
            if (o2 > vsm::MAX_OFF * vsm::MAX_OFF) { const double s = vsm::MAX_OFF / std::sqrt(o2); ox *= s; oy *= s; oz *= s; }
            smOx[i] = ox; smOy[i] = oy; smOz[i] = oz; smQ[i] = {qs[0], qs[1], qs[2], qs[3]};
            gX[i] = S.x[i] + ox; gY[i] = S.y[i] + oy; gZ[i] = S.z[i] + oz;
        }
    }
}
// рисуемые координаты есть у всех атомов (после вставки или удаления — до ближайшего кадра их нет)
static inline bool gValid() { return (int)gX.size() == S.n && (int)gY.size() == S.n && (int)gZ.size() == S.n; }
// вектор между рисуемыми положениями атомов (минимальный образ, как dvec)
static inline void dvecG(int i, int j, double& dx, double& dy, double& dz) {
    if (!gValid()) { dvec(i, j, dx, dy, dz); return; }
    dx = gX[j] - gX[i]; dy = gY[j] - gY[i]; dz = gZ[j] - gZ[i];
    if (perAx(0)) dx = minImg(dx, S.Lx, 1.0 / S.Lx);
    if (perAx(1)) dy = minImg(dy, S.Ly, 1.0 / S.Ly);
    if (perAx(2)) dz = minImg(dz, S.Lz, 1.0 / S.Lz);
}

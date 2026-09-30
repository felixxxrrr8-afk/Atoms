// ===================================== КВАНТОВЫЙ МИР: ВОЛНОВАЯ ФУНКЦИЯ ======================================
// Сцены 130–134: электрон — волна ψ(x, y, t), нестационарное уравнение Шрёдингера
//   iħ·∂ψ/∂t = −ħ²/(2m)·∇²ψ + V·ψ
// на сетке 256×256 методом расщепления оператора (split-step Фурье): полшага потенциала, шаг кинетической энергии
// в импульсном пространстве (двумерное БПФ), ещё полшага потенциала. Метод унитарен — вероятность сохраняется точно;
// теряется она только в поглощающих краях, куда волна уходит из кадра. Единицы: нм, фс, эВ; масса — электрона.
// Яркость — плотность вероятности |ψ|², цвет — фаза ψ (как на картинках орбиталей).
namespace wv {
constexpr int N = 256;
constexpr double HBAR = 0.6582119569;   // ħ, эВ·фс
constexpr double H2M = 0.0380998;       // ħ²/(2mₑ), эВ·нм²
using cf = std::complex<float>;
static int scene = 0;                   // 0 туннель, 1 две щели, 2 осциллятор, 3 квантовый ковёр, 4 дифракция на кристалле
static double L = 24, dx = L / N, t = 0, dt = 0.01, timeScale = 4;   // сторона (нм), шаг сетки, время и шаг (фс), фс за секунду
static std::vector<cf> psi, expV, expK;
static std::vector<float> V, lossL, lossR;
static int rev[N]; static cf tw[N / 2];
static GLuint tex = 0; static std::vector<unsigned char> img;
static double rhoMax = 1e-9;            // яркость: сглаженный максимум |ψ|²
// параметры сцен
static double E0 = 2.0;                              // средняя энергия электрона, эВ
static double barV = 3.0, barW = 0.30;               // туннель: высота (эВ) и ширина барьера (нм)
static double slitD = 1.8, slitW = 0.40;             // две щели: расстояние между центрами и ширина (нм)
static bool detector = false;                        // прибор, отмечающий, через какую щель прошёл электрон
static double hw = 0.5, shift0 = 3.0; static int oscMode = 0;   // осциллятор: ħω (эВ), смещение (нм); 0 колебание, 1 вращение, 2 сжатое
static double latD = 0.8;                            // кристалл: период решётки (нм)
static double boxA = 10;                             // ковёр: ширина ящика (нм)
static bool showPhase = true;
// показания
static double norm0 = 1, lostL = 0, lostR = 0;       // исходная норма и вероятность, ушедшая в левый и правый края
static std::vector<float> dots; static int electrons = 0; static int measuredSide = 0; static bool measureDone = false;
static std::vector<float> histX, histXc;              // осциллятор: ⟨x⟩ и классическая траектория
static std::vector<float> angI;                       // дифракция: интенсивность по углу −60…60°
static std::vector<double> cn; static std::vector<float> carpet; static int carpetN = 64;   // ковёр: коэффициенты и картинка
static double carpetT = 1;                            // время возрождения, фс
static float lastClickX = 0, lastClickY = 0, lastClickAge = 99; static bool lastClickFound = false;
static inline double xOf(int i) { return (i - N / 2 + 0.5) * dx; }
static inline double kOf(int i) { return 2 * PI / L * (i < N / 2 ? i : i - N); }
static inline double k0() { return std::sqrt(E0 / H2M); }   // волновое число пакета, 1/нм
}
static std::vector<double> wvRun;   // две щели: поток через экран за текущий пролёт, по строкам

// ---- БПФ по основанию 2 (N = 256), строки и столбцы — параллельно
static void wvFftInit() {
    using namespace wv;
    int lg = 0; while ((1 << lg) < N) lg++;
    for (int i = 0; i < N; i++) { int r = 0; for (int b = 0; b < lg; b++) if (i & (1 << b)) r |= 1 << (lg - 1 - b); rev[i] = r; }
    for (int k = 0; k < N / 2; k++) tw[k] = cf((float)std::cos(-2 * PI * k / N), (float)std::sin(-2 * PI * k / N));
}
static void wvFft1(wv::cf* a, bool inv) {
    using namespace wv;
    for (int i = 0; i < N; i++) { const int j = rev[i]; if (i < j) std::swap(a[i], a[j]); }
    for (int len = 2; len <= N; len <<= 1) {
        const int half = len / 2, step = N / len;
        for (int i = 0; i < N; i += len) for (int k = 0; k < half; k++) {
            cf w = tw[k * step]; if (inv) w = std::conj(w);
            const cf u = a[i + k], v = a[i + k + half] * w;
            a[i + k] = u + v; a[i + k + half] = u - v;
        }
    }
}
static void wvFft2(std::vector<wv::cf>& a, bool inv) {
    using namespace wv;
#pragma omp parallel for schedule(static)
    for (int y = 0; y < N; y++) wvFft1(&a[(size_t)y * N], inv);
#pragma omp parallel
    {
        cf col[N];
#pragma omp for schedule(static)
        for (int x = 0; x < N; x++) {
            for (int y = 0; y < N; y++) col[y] = a[(size_t)y * N + x];
            wvFft1(col, inv);
            for (int y = 0; y < N; y++) a[(size_t)y * N + x] = col[y];
        }
    }
}
// множители шага: потенциал (полшага) с поглощающей рамкой и кинетическая энергия (с нормировкой обратного БПФ)
static void wvFactors() {
    using namespace wv;
    const size_t NN = (size_t)N * N; expV.resize(NN); expK.resize(NN);
    const int edge = scene == 2 ? 10 : 22;   // поглощающий слой по краям, точек
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
        const size_t p = (size_t)j * N + i;
        const int e = std::min(std::min(i, N - 1 - i), std::min(j, N - 1 - j));
        const double s = e < edge ? (double)(edge - e) / edge : 0, damp = std::exp(-16.0 * s * s * s * dt);
        const double ph = -V[p] * dt / (2 * HBAR);
        expV[p] = cf((float)(damp * std::cos(ph)), (float)(damp * std::sin(ph)));
        const double kx = kOf(i), ky = kOf(j), pk = -H2M * (kx * kx + ky * ky) * dt / HBAR;
        expK[p] = cf((float)(std::cos(pk) / NN), (float)(std::sin(pk) / NN));
    }
}
// гауссов пакет: центр (x0, y0), ширины σx, σy (по |ψ|²), волновой вектор (kx, ky)
static void wvPacket(double x0, double y0, double sx, double sy, double kx, double ky) {
    using namespace wv;
    double nrm = 0;
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
        const double x = xOf(i), y = xOf(j), a = -(x - x0) * (x - x0) / (4 * sx * sx) - (y - y0) * (y - y0) / (4 * sy * sy), ph = kx * x + ky * y;
        const double m = std::exp(a); psi[(size_t)j * N + i] = cf((float)(m * std::cos(ph)), (float)(m * std::sin(ph))); nrm += m * m;
    }
    const float s = (float)(1 / std::sqrt(nrm)); for (auto& c : psi) c *= s;
    norm0 = 1; lostL = lostR = 0; rhoMax = 1e-9; measureDone = false; measuredSide = 0;
}
static double wvNorm(int i0 = 0, int i1 = wv::N) {   // вероятность в полосе столбцов [i0, i1)
    using namespace wv; double s = 0;
    for (int j = 0; j < N; j++) for (int i = i0; i < i1; i++) s += std::norm(psi[(size_t)j * N + i]);
    return s;
}
// потенциал сцены
static void wvPotential() {
    using namespace wv;
    V.assign((size_t)N * N, 0.0f);
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
        const double x = xOf(i), y = xOf(j); float& v = V[(size_t)j * N + i];
        switch (scene) {
        case 0: {   // доля ячейки сетки, занятая барьером: ширина барьера не «округляется» до целого числа точек
            const double lo = std::max(x - dx / 2, -barW / 2), hi = std::min(x + dx / 2, barW / 2);
            if (hi > lo) v = (float)(barV * (hi - lo) / dx); break; }
        case 1: {   // стенка толщиной 0.3 нм с двумя щелями
            const double wx = -2.0;
            if (std::fabs(x - wx) < 0.15 && std::fabs(std::fabs(y) - slitD / 2) > slitW / 2) v = 40; break; }
        case 2: v = (float)std::min(60.0, hw * hw * (x * x + y * y) / (4 * H2M)); break;
        case 4: {   // три ряда атомов-рассеивателей: гауссовы бугры 6 эВ радиусом 0.1 нм
            for (int r = 0; r < 3; r++) {
                const double ax = -0.5 * latD + r * 0.5 * latD, dxa = x - ax; if (std::fabs(dxa) > 0.4) continue;
                const double ay = std::round(y / latD - (r & 1) * 0.5) * latD + (r & 1) * 0.5 * latD, dya = y - ay;
                v += (float)(6.0 * std::exp(-(dxa * dxa + dya * dya) / (2 * 0.1 * 0.1)));
            }
            break; }
        }
    }
}
// квантовый ковёр: ψ(x, t) = Σ cₙ·√(2/a)·sin(nπx/a)·e^{−iEₙt/ħ}, Eₙ = n²·E₁ — картинка «пространство × время» за один период
static void wvCarpet() {
    using namespace wv;
    const double a = boxA, s = 0.04 * a, x0 = 0.5 * a;
    cn.assign(carpetN + 1, 0.0);
    const int M = 2048; double nrm = 0;
    std::vector<double> g(M); for (int q = 0; q < M; q++) { const double x = (q + 0.5) * a / M; g[q] = std::exp(-(x - x0) * (x - x0) / (4 * s * s)); nrm += g[q] * g[q] * a / M; }
    for (int n = 1; n <= carpetN; n++) { double c = 0; for (int q = 0; q < M; q++) c += g[q] * std::sqrt(2 / a) * std::sin(n * PI * (q + 0.5) / M) * a / M; cn[n] = c / std::sqrt(nrm); }
    const double E1 = H2M * (PI / a) * (PI / a); carpetT = 2 * PI * HBAR / E1;
    carpet.assign((size_t)N * N, 0.0f);
    std::vector<double> sn((size_t)(carpetN + 1) * N);
    for (int n = 1; n <= carpetN; n++) for (int i = 0; i < N; i++) sn[(size_t)n * N + i] = std::sqrt(2 / a) * std::sin(n * PI * (i + 0.5) / N);
#pragma omp parallel for schedule(static)
    for (int r = 0; r < N; r++) {
        const double tt = carpetT * r / (N - 1);
        std::vector<double> cr(carpetN + 1), ci(carpetN + 1);
        for (int n = 1; n <= carpetN; n++) { const double ph = -n * n * E1 * tt / HBAR; cr[n] = cn[n] * std::cos(ph); ci[n] = cn[n] * std::sin(ph); }
        for (int i = 0; i < N; i++) {
            double re = 0, im = 0; for (int n = 1; n <= carpetN; n++) { const double b = sn[(size_t)n * N + i]; re += cr[n] * b; im += ci[n] * b; }
            carpet[(size_t)r * N + i] = (float)(re * re + im * im);
        }
    }
    const float mx = std::max(1e-12f, *std::max_element(carpet.begin(), carpet.end()));
    for (auto& v : carpet) v /= mx;
}
static void wvReset(int sc) {
    using namespace wv;
    static bool fftReady = false; if (!fftReady) { wvFftInit(); fftReady = true; }
    scene = clampv(sc, 0, 4); t = 0;
    psi.assign((size_t)N * N, cf(0, 0)); lossL.assign(N, 0); dots.clear(); electrons = 0; histX.clear(); histXc.clear(); angI.assign(121, 0);
    lastClickAge = 99;
    switch (scene) {
    case 0: L = 24; timeScale = 3; break;
    case 1: L = 24; timeScale = 8; wvRun.assign(N, 0.0); break;
    case 2: L = 16; timeScale = 1.5; break;
    case 3: L = boxA; break;   // масштабная линейка — по ширине ящика
    case 4: L = 16; timeScale = 1.5; break;
    }
    dx = L / N;
    // шаг: фаза кинетической энергии пакета за шаг ≈ 0.25 рад
    const double Emax = scene == 2 ? hw * hw * shift0 * shift0 / (4 * H2M) + 2 * hw : (scene == 4 ? E0 * 1.3 : std::max(E0, barV) * 1.3);
    dt = clampv(0.25 * HBAR / std::max(0.5, Emax), 0.002, 0.03);
    wvPotential(); wvFactors();
    switch (scene) {
    case 0: wvPacket(-6.0, 0, 1.4, 1.4, k0(), 0); break;
    case 1: wvPacket(-7.5, 0, 1.0, 2.6, k0(), 0); break;
    case 2: {
        const double s0 = std::sqrt(H2M / hw), kY = oscMode == 1 ? shift0 * hw / (2 * H2M) : 0;
        wvPacket(shift0, 0, s0 * (oscMode == 2 ? 0.5 : 1.0), s0 * (oscMode == 2 ? 0.5 : 1.0), 0, kY); break; }
    case 3: wvCarpet(); timeScale = carpetT / 20; break;
    case 4: wvPacket(-4.5, 0, 1.0, 3.2, k0(), 0); break;
    }
    norm0 = wvNorm();
}
// один шаг: потенциал/2 — кинетика — потенциал/2; вероятность, съеденная краями, делится на «левую» и «правую»
static void wvStep1() {
    using namespace wv;
    const size_t NN = (size_t)N * N;
    auto half = [&](bool count) {
        double l = 0, r = 0;
#pragma omp parallel for schedule(static) reduction(+ : l, r)
        for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
            const size_t p = (size_t)j * N + i; const float before = count ? std::norm(psi[p]) : 0.0f;
            psi[p] *= expV[p];
            if (count) { const double d = before - std::norm(psi[p]); if (i < N / 2) l += d; else r += d; }
        }
        lostL += l; lostR += r;
    };
    half(true);
    wvFft2(psi, false);
    if (scene == 4) {   // угловое распределение прошедшей волны: |ψ(k)|² по направлениям с kx > 0
        std::vector<float> a(121, 0.0f);
        for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
            const double kx = kOf(i), ky = kOf(j); if (kx <= 0) continue;
            const double th = std::atan2(ky, kx) * 180 / PI; if (std::fabs(th) > 60) continue;
            a[(int)std::lround(th + 60)] += std::norm(psi[(size_t)j * N + i]);
        }
        for (int q = 0; q < 121; q++) angI[q] += 0.05f * (a[q] - angI[q]);
    }
#pragma omp parallel for schedule(static)
    for (int p = 0; p < (int)NN; p++) psi[p] *= expK[p];
    wvFft2(psi, true);
    half(true);
    t += dt;
}
// две щели: попадания электронов в экран и «прибор у щелей». За каждый пролёт волны копится распределение потока
// через экран; когда волна ушла из кадра, на экране появляются точки попаданий — каждая в одном месте, с вероятностью |ψ|²
// (пролёт изображает серию из 12 электронов, летящих по одному), и летит следующая волна
static void wvSlits() {
    using namespace wv;
    const int is = (int)std::lround((9.3 / dx) + N / 2 - 0.5);   // столбец экрана
    if ((int)wvRun.size() != N) wvRun.assign(N, 0.0);
    for (int j = 0; j < N; j++) wvRun[j] += std::norm(psi[(size_t)j * N + is]);
    // наблюдение пути: когда волна прошла стенку, прибор «видит» электрон в одной из щелей — вторая половина исчезает
    const double wx = -2.0, tMeas = (wx + 1.2 + 7.5) / (2 * H2M * k0() / HBAR);
    if (detector && !measureDone && t >= tMeas) {
        const int iw = (int)std::lround((wx + 0.2) / dx + N / 2 - 0.5);
        double up = 0, dn = 0;
        for (int j = 0; j < N; j++) for (int i = iw; i < N; i++) (xOf(j) > 0 ? up : dn) += std::norm(psi[(size_t)j * N + i]);
        measuredSide = urand() * (up + dn) < up ? 1 : -1;
        for (int j = 0; j < N; j++) if ((xOf(j) > 0 ? 1 : -1) != measuredSide) for (int i = iw; i < N; i++) psi[(size_t)j * N + i] = cf(0, 0);
        measureDone = true;
    }
    if (wvNorm() > 0.03 * norm0 && t < 45) return;
    double tot = 0; for (double v : wvRun) tot += v;
    if (tot > 0) for (int e = 0; e < 12; e++) {
        double r = urand() * tot, s = 0; int jj = N / 2;
        for (int j = 0; j < N; j++) { s += wvRun[j]; if (s >= r) { jj = j; break; } }
        dots.push_back((float)xOf(jj));
    }
    if (dots.size() > 6000) dots.erase(dots.begin(), dots.begin() + (dots.size() - 6000));
    const int e = electrons + 12; const std::vector<float> d = dots;
    t = 0; wvPacket(-7.5, 0, 1.0, 2.6, k0(), 0); norm0 = wvNorm(); electrons = e; dots = d; wvRun.assign(N, 0.0);
}
static void wvStep(double frameDt) {
    using namespace wv;
    lastClickAge += (float)frameDt;
    if (scene == 3) { t += timeScale * frameDt; if (t > carpetT) t = 0; return; }
    const double want = timeScale * frameDt; int nst = (int)std::ceil(want / dt - 1e-9);
    nst = clampv(nst, 1, 40);
    const auto c0 = std::chrono::high_resolution_clock::now();
    for (int s = 0; s < nst; s++) {
        wvStep1();
        if (scene == 1) wvSlits();
        if (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - c0).count() > 0.03) break;
    }
    if (scene == 2) {   // ⟨x⟩ и классическое движение x0·cos ωt
        double sx = 0, sn = 0;
        for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) { const double r = std::norm(psi[(size_t)j * N + i]); sx += r * xOf(i); sn += r; }
        histX.push_back((float)(sn > 0 ? sx / sn : 0)); histXc.push_back((float)(shift0 * std::cos(hw / HBAR * t)));
        if (histX.size() > 400) { histX.erase(histX.begin()); histXc.erase(histXc.begin()); }
    }
}
// щелчок по волне — измерение положения: с вероятностью, равной доле |ψ|² в кружке радиусом 0.6 нм, частица найдена там
// (волна «схлопывается» в этот кружок), иначе в кружке её нет — и волна там обнуляется (измерение с отрицательным итогом)
static void wvMeasure(double mx, double my) {
    using namespace wv;
    if (scene == 3 || psi.empty()) return;
    const double R = 0.6; double in = 0, tot = 0;
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
        const double r = std::norm(psi[(size_t)j * N + i]); tot += r;
        const double ddx = xOf(i) - mx, ddy = xOf(j) - my; if (ddx * ddx + ddy * ddy < R * R) in += r;
    }
    if (tot <= 0) return;
    const bool found = urand() < in / tot;
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
        const double ddx = xOf(i) - mx, ddy = xOf(j) - my; const bool inside = ddx * ddx + ddy * ddy < R * R;
        if (inside != found) psi[(size_t)j * N + i] = cf(0, 0);
    }
    const double left = found ? in : tot - in;
    if (left > 0) { const float s = (float)std::sqrt(tot / left); for (auto& c : psi) c *= s; }
    lastClickX = (float)mx; lastClickY = (float)my; lastClickAge = 0; lastClickFound = found;
    showToast(found ? fmt("Измерение: электрон найден здесь (вероятность была %.0f%%) — волна сжалась в точку и снова расплывается", 100 * in / tot)
                    : fmt("Измерение: здесь электрона нет (вероятность была %.0f%%) — волна в этом месте исчезла", 100 * in / tot));
}
// теория туннелирования: прозрачность прямоугольного барьера для плоской волны с энергией E
static double wvTunnelT(double E, double U, double a) {
    using namespace wv;
    if (U <= 0) return 1;
    if (E < U) { const double k = std::sqrt((U - E) / H2M), s = std::sinh(k * a); return 1 / (1 + U * U * s * s / (4 * E * (U - E))); }
    if (E > U) { const double k = std::sqrt((E - U) / H2M), s = std::sin(k * a); return 1 / (1 + U * U * s * s / (4 * E * (E - U))); }
    return 1 / (1 + a * a * U / (4 * H2M));
}

// ---- рисунок: плотность вероятности яркостью, фаза цветом; потенциал — серым
static inline void wvHue(double ph, float& r, float& g, float& b) {   // фаза → цвет по кругу (красный — 0, голубой — π)
    const double h = (ph / (2 * PI) + 1.0) * 6.0; const int k = (int)std::floor(h) % 6; const float f = (float)(h - std::floor(h));
    switch (k) { case 0: r = 1; g = f; b = 0; break; case 1: r = 1 - f; g = 1; b = 0; break; case 2: r = 0; g = 1; b = f; break;
                 case 3: r = 0; g = 1 - f; b = 1; break; case 4: r = f; g = 0; b = 1; break; default: r = 1; g = 0; b = 1 - f; }
}
static void wvUpload() {
    using namespace wv;
    img.resize((size_t)N * N * 4);
    if (scene == 3) {   // ковёр: строки — время (сверху вниз), видна часть до текущего момента
        const int rows = clampv((int)(t / carpetT * (N - 1)), 0, N - 1);
        for (int r = 0; r < N; r++) for (int i = 0; i < N; i++) {
            const float v = r <= rows ? std::sqrt(carpet[(size_t)r * N + i]) : 0.0f; unsigned char* p = &img[((size_t)r * N + i) * 4];
            p[0] = (unsigned char)(255 * std::min(1.0f, 1.1f * v)); p[1] = (unsigned char)(255 * std::min(1.0f, 0.75f * v + 0.15f * v * v)); p[2] = (unsigned char)(255 * std::min(1.0f, 0.35f * v)); p[3] = 255;
        }
    } else {
        double mx = 0; for (auto& c : psi) mx = std::max(mx, (double)std::norm(c));
        rhoMax = std::max(mx, rhoMax * 0.97);   // медленно следует за максимумом: расплывание видно как потемнение
        float vmax = 1e-6f; for (float v : V) vmax = std::max(vmax, v);
        for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
            const size_t p = (size_t)j * N + i; const cf c = psi[p];
            // яркость ~ |ψ|^0.7: слабые прошедшие и рассеянные волны тоже видны
            const float b = (float)std::min(1.0, std::pow(std::norm(c) / rhoMax, 0.35));
            float r, g, bl;
            if (showPhase) { wvHue(std::arg(c), r, g, bl); r *= b; g *= b; bl *= b; } else { r = g = bl = b; }
            const float u = V[p] / vmax, w = scene == 2 ? 0.10f * u : (u > 0.12f ? 0.15f + 0.4f * u : 0.0f);   // стенки, барьеры, атомы
            unsigned char* q = &img[((size_t)(N - 1 - j) * N + i) * 4];
            q[0] = (unsigned char)(255 * std::min(1.0f, r + w)); q[1] = (unsigned char)(255 * std::min(1.0f, g + w)); q[2] = (unsigned char)(255 * std::min(1.0f, bl + w)); q[3] = 255;
        }
    }
    if (!tex) { glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data()); }
    else { glBindTexture(GL_TEXTURE_2D, tex); glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, N, N, GL_RGBA, GL_UNSIGNED_BYTE, img.data()); }
}
static PR wvArea;   // где на экране нарисована область (для мыши)
static void wvDraw() {
    using namespace wv;
    glEnable(GL_SCISSOR_TEST); glScissor((int)sceneX, (int)(winH - sceneY - sceneH), (int)sceneW, (int)sceneH);
    rectFill(sceneX, sceneY, sceneW, sceneH, C_SCENE);
    const float extra = scene == 1 ? uiPx(90) : 0.0f, top = uiPx(44);   // сверху — заголовок сцены
    const float bottom = scene == 3 ? uiPx(120) : uiPx(20);
    const float side = std::floor(std::min(sceneW - extra - uiPx(40), sceneH - bottom - top));
    const float ox = std::floor(sceneX + (sceneW - extra - side) / 2), oy = std::floor(sceneY + top);
    wvArea = {ox, oy, side, side};
    wvUpload();
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, tex); glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS); glTexCoord2f(0, 0); glVertex2f(ox, oy); glTexCoord2f(1, 0); glVertex2f(ox + side, oy); glTexCoord2f(1, 1); glVertex2f(ox + side, oy + side); glTexCoord2f(0, 1); glVertex2f(ox, oy + side); glEnd();
    glDisable(GL_TEXTURE_2D);
    rectLine(ox, oy, side, side, C_LINE);
    auto toPx = [&](double x, double y, float& px, float& py) { px = (float)(ox + (x / L + 0.5) * side); py = (float)(oy + (0.5 - y / L) * side); };
    if (scene == 1) {   // экран справа: точки попаданий электронов
        const float sx0 = ox + side + uiPx(14), sw = uiPx(60);
        rectFill(sx0, oy, sw, side, hexc(0x101010)); rectLine(sx0, oy, sw, side, C_LINE);
        std::mt19937 jit(5);
        MonoAtoms colored; glColor4f(0.55f, 1.0f, 0.65f, 0.85f);
        glPointSize(std::max(2.0f, uiPx(2.5f))); glBegin(GL_POINTS);
        for (float y : dots) { float px, py; toPx(0, y, px, py); glVertex2f(sx0 + uiPx(4) + (float)(jit() % 1000) / 1000.0f * (sw - uiPx(8)), py); }
        glEnd(); glPointSize(1);
        drawText(fontXS, sx0, oy + side + uiPx(4), "экран", C_DIM);
        if (detector && measureDone) { float px, py; toPx(-2.0, measuredSide * slitD / 2, px, py); rectLine(px - uiPx(9), py - uiPx(9), uiPx(18), uiPx(18), C_ACC); }
    }
    if (scene == 3) {   // ковёр: подписи осей и текущая плотность под картинкой
        float px0, py0; (void)px0; (void)py0;
        const float yNow = oy + (float)(t / carpetT) * side;
        lineH(ox, ox + side, yNow, withA(C_ACC, 0.7f));
        drawText(fontXS, ox + side + uiPx(6), oy - uiPx(2), "t = 0", C_DIM);
        drawText(fontXS, ox + side + uiPx(6), oy + side / 2 - fontXS.h / 2, "T/2", C_DIM);
        drawText(fontXS, ox + side + uiPx(6), oy + side - fontXS.h, "T", C_DIM);
        drawText(fontXS, ox, oy + side + uiPx(4), fmt("стенки ящика шириной %.0f нм · время вниз, период возрождения T = %.0f фс", boxA, carpetT), C_DIM);
        // плотность вероятности сейчас
        const double a = boxA, E1 = H2M * (PI / a) * (PI / a);
        std::vector<float> rho(N); float mx = 1e-9f;
        for (int i = 0; i < N; i++) {
            double re = 0, im = 0;
            for (int n = 1; n <= carpetN; n++) { const double ph = -n * n * E1 * t / HBAR, b = std::sqrt(2 / a) * std::sin(n * PI * (i + 0.5) / N); re += cn[n] * std::cos(ph) * b; im += cn[n] * std::sin(ph) * b; }
            rho[i] = (float)(re * re + im * im); mx = std::max(mx, rho[i]);
        }
        PR g{ox, oy + side + fontXS.h + uiPx(10), side, bottom - fontXS.h - uiPx(20)};
        boxPanel(g.x, g.y, g.w, g.h, C_PANEL2, C_LINE);
        drawSeries(g, rho, 0, mx * 1.05, C_TEXT_HI, 1.5f, LS_SOLID, -1);
    }
    if (scene == 0) {   // подпись барьера
        float px, py; toPx(barW / 2, 0, px, py);
        drawText(fontXS, px + uiPx(6), oy + side - uiPx(8) - fontXS.h, fmt("барьер %.1f эВ × %.2f нм", barV, barW), C_TEXT);
    }
    if (lastClickAge < 1.5f) {   // место последнего измерения
        float px, py; toPx(lastClickX, lastClickY, px, py); const float r = (float)(0.6 / L * side);
        glEnable(GL_LINE_SMOOTH); col(withA(C_ACC, 1.0f - lastClickAge / 1.5f)); circlePx(px, py, r, 48); glDisable(GL_LINE_SMOOTH);
    }
    // масштаб
    {
        const double sL = niceStep(uiPx(110) / (side / L)); const float lp = (float)(sL / L * side), x = ox + uiPx(8), y = oy + side - uiPx(10);
        rectFill(x, y, lp, 1, withA(C_TEXT, 0.85f)); rectFill(x, y - uiPx(4), 1, uiPx(5), withA(C_TEXT, 0.85f)); rectFill(x + lp - 1, y - uiPx(4), 1, uiPx(5), withA(C_TEXT, 0.85f));
        drawText(fontXS, x + lp / 2 - uiPx(10), y - uiPx(4) - fontXS.h, fmt("%g нм", sL), withA(C_TEXT, 0.85f));
    }
    glDisable(GL_SCISSOR_TEST);
}
static void wvMouse() {   // щелчок внутри картинки волны (вызывается, только когда мышь над сценой и не над её кнопками)
    using namespace wv;
    if (!in.lPress) return;
    const PR& a = wvArea; if (mouseX < a.x || mouseX > a.x + a.w || mouseY < a.y || mouseY > a.y + a.h) return;
    wvMeasure((mouseX - a.x) / a.w * L - L / 2, L / 2 - (mouseY - a.y) / a.h * L);
}
static std::string wvTimeStr() { return fmt("%.1f фс", wv::t); }
// значения по умолчанию при входе в сцену (R и кнопка «заново» их не сбрасывают)
static void wvDefaults(int sc) {
    using namespace wv;
    switch (sc) {
    case 0: E0 = 2.0; barV = 3.0; barW = 0.30; break;
    case 1: E0 = 3.0; slitD = 2.4; slitW = 0.40; detector = false; break;
    case 2: hw = 0.5; shift0 = 3.0; oscMode = 0; break;
    case 3: boxA = 10; break;
    case 4: E0 = 10.0; latD = 0.8; break;
    }
    showPhase = true;
}
static const char* WV_TITLES[5] = {
    "Туннельный эффект: электрон проходит сквозь барьер выше своей энергии — классическая частица отразилась бы вся",
    "Двойная щель: каждый электрон попадает в одну точку экрана, а вместе они рисуют интерференцию; прибор у щелей её стирает",
    "Квантовый осциллятор: когерентное состояние качается, как шарик на пружине, и не расплывается",
    "Квантовый ковёр: пакет в ящике расплывается, собирается в копии и через период T возрождается целиком",
    "Дифракция электронов на кристалле (Дэвиссон и Джермер, 1927): пучок выходит лучами под углами d·sin θ = nλ"};
static void wvPanel(float x, float y, float w, float h) {
    using namespace wv;
    rectFill(x, y, w, h, C_PANEL); lineV(x, y, y + h, C_LINE);
    const float pad = uiPx(10), cx = x + pad, cw = w - 2 * pad;
    float yy = scrollBegin(6, cx, y + uiPx(6), cw, h - uiPx(10));
    const float bh = uiPx(26), sh = uiPx(30);
    drawText(fontL, cx, yy, "Волновая функция", C_TEXT_HI); yy += fontL.h + uiPx(6);
    auto row = [&](const char* k, const std::string& v) { drawText(fontU, cx, yy, k, C_DIM); drawTextR(fontM, cx + cw, yy - uiPx(1), v, C_TEXT_HI); yy += fontU.h + uiPx(5); };
    row("время", wvTimeStr());
    const double lam = 2 * PI / k0();
    if (scene != 2 && scene != 3) row("длина волны λ", fmt("%.2f нм (E = %.1f эВ)", lam, E0));
    if (scene == 0) {
        const double here = wvNorm(), right = wvNorm(N / 2, N);
        row("прошло сквозь барьер", fmt("%.1f%%", 100 * (right + lostR) / norm0));
        row("отразилось", fmt("%.1f%%", 100 * (here - right + lostL) / norm0));
        row("теория (плоская волна)", fmt("%.1f%%", 100 * wvTunnelT(E0, barV, barW)));
    } else if (scene == 1) {
        row("электронов на экране", fmt("%d", (int)dots.size()));
        row("полосы (теория λD/d)", fmt("%.2f нм", lam * (9.3 + 2.0) / slitD));
        if (detector) row("прибор увидел", measureDone ? (measuredSide > 0 ? std::string(T("верхнюю щель")) : std::string(T("нижнюю щель"))) : std::string("…"));
    } else if (scene == 2) {
        const double T0 = 2 * PI * HBAR / hw;
        row("период 2πħ/ħω", fmt("%.2f фс", T0));
        row("⟨x⟩ сейчас", histX.empty() ? std::string("—") : fmt("%+.2f нм", histX.back()));
        row("классически", histXc.empty() ? std::string("—") : fmt("%+.2f нм", histXc.back()));
    } else if (scene == 3) {
        row("период возрождения T", fmt("%.0f фс", carpetT));
        row("сейчас", fmt("t = %.3f·T", t / carpetT));
    } else {
        const double s1 = lam / latD;
        row("первый луч (теория)", s1 < 1 ? fmt("θ = %.1f°", std::asin(s1) * 180 / PI) : std::string(T("нет: λ > d")));
    }
    yy += uiPx(4);
    uiSection(cx, yy, cw, "управление");
    if (uiButton(1850, cx, yy, cw / 2 - uiPx(3), bh, "заново (R)", false, false, "Начать сцену заново")) wvReset(scene);
    if (uiButton(1851, cx + cw / 2 + uiPx(3), yy, cw / 2 - uiPx(3), bh, P.paused ? "пуск (пробел)" : "пауза (пробел)", P.paused, false, "Остановить или запустить время")) P.paused = !P.paused;
    yy += bh + uiPx(6);
    {
        double lg = std::log10(timeScale); const double lo = scene == 3 ? 0 : -1.5, hi = scene == 3 ? 3 : 1.5;
        if (uiSlider(1852, cx, yy, cw, sh - uiPx(4), "скорость времени", &lg, lo, hi, false, fmt("%.3g фс за 1 с", timeScale), "Сколько фемтосекунд проходит за секунду на экране")) timeScale = std::pow(10.0, lg);
        yy += sh;
    }
    if (scene != 3) { uiCheck(1853, cx, yy, cw, uiPx(22), "фаза — цветом", &showPhase, "Цвет — фаза ψ (волна «вращается» по кругу цветов), яркость — |ψ|²"); yy += uiPx(26); }
    if (scene == 0 || scene == 1 || scene == 4) {
        double e = E0;
        if (uiSlider(1854, cx, yy, cw, sh - uiPx(4), "энергия электрона", &e, scene == 4 ? 3 : 0.5, scene == 4 ? 20 : 6, false, fmt("%.2f эВ", e), "Кинетическая энергия: больше энергия — короче волна де Бройля λ = h/p")) { E0 = e; wvReset(scene); }
        yy += sh;
    }
    if (scene == 0) {
        double v = barV, a = barW;
        if (uiSlider(1855, cx, yy, cw, sh - uiPx(4), "высота барьера", &v, 0, 8, false, fmt("%.2f эВ", v), "Классически электрон с энергией меньше барьера отражается всегда")) { barV = v; wvReset(0); }
        yy += sh;
        if (uiSlider(1856, cx, yy, cw, sh - uiPx(4), "ширина барьера", &a, 0.05, 1.2, false, fmt("%.2f нм", a), "Прозрачность падает с шириной экспоненциально: T ≈ e^(−2κa)")) { barW = a; wvReset(0); }
        yy += sh;
    }
    if (scene == 1) {
        double d = slitD;
        if (uiSlider(1857, cx, yy, cw, sh - uiPx(4), "расстояние между щелями", &d, 0.8, 4, false, fmt("%.2f нм", d), "Чем ближе щели, тем шире интерференционные полосы")) { slitD = d; wvReset(1); }
        yy += sh;
        if (uiCheck(1858, cx, yy, cw, uiPx(22), "прибор у щелей", &detector, "Прибор отмечает, через какую щель прошёл электрон. Узнав путь,\nмы разрушаем интерференцию: полосы на экране исчезают")) { dots.clear(); electrons = 0; wvReset(1); }
        yy += uiPx(26);
        if (uiButton(1859, cx, yy, cw, bh, "очистить экран", false, false, "Стереть точки попаданий")) { dots.clear(); electrons = 0; }
        yy += bh + uiPx(6);
    }
    if (scene == 2) {
        double v = hw, s = shift0;
        if (uiSlider(1860, cx, yy, cw, sh - uiPx(4), "квант энергии ħω", &v, 0.2, 1.0, false, fmt("%.2f эВ", v), "Жёсткость «пружины»: уровни энергии идут через ħω")) { hw = v; wvReset(2); }
        yy += sh;
        if (uiSlider(1861, cx, yy, cw, sh - uiPx(4), "начальное смещение", &s, 0, 4, false, fmt("%.1f нм", s), "Насколько пакет отведён от центра")) { shift0 = s; wvReset(2); }
        yy += sh;
        static const char* MODES[3] = {"колебание", "вращение", "сжатое состояние"};
        if (uiCycle(1862, cx, yy, cw, bh, "начальное состояние", MODES[clampv(oscMode, 0, 2)], false, "Колебание — отведённый пакет; вращение — с толчком вбок;\nсжатое — вдвое уже основного: его ширина «дышит» с частотой 2ω")) { oscMode = (oscMode + 1) % 3; wvReset(2); }
        yy += bh + uiPx(6);
    }
    if (scene == 3) {
        double a = boxA;
        if (uiSlider(1863, cx, yy, cw, sh - uiPx(4), "ширина ящика", &a, 4, 20, false, fmt("%.0f нм", a), "Период возрождения растёт как квадрат ширины: T = 4mL²/(πħ)")) { boxA = a; wvReset(3); }
        yy += sh;
    }
    if (scene == 4) {
        double d = latD;
        if (uiSlider(1864, cx, yy, cw, sh - uiPx(4), "период решётки d", &d, 0.5, 1.5, false, fmt("%.2f нм", d), "Расстояние между рядами атомов кристалла")) { latD = d; wvReset(4); }
        yy += sh;
    }
    // графики
    if (scene == 2 && histX.size() > 2) {
        uiSection(cx, yy, cw, "⟨x⟩(t): квантовое среднее и классика");
        PR in{cx, yy, cw, uiPx(110)}; boxPanel(in.x, in.y, in.w, in.h, C_PANEL2, C_LINE);
        const double m = std::max(0.5, shift0 * 1.1);
        drawSeries(in, histXc, -m, m, withA(C_DIM, 0.9f), 1.2f, LS_DASH, 400); drawSeries(in, histX, -m, m, C_TEXT_HI, 1.5f, LS_SOLID, 400);
        yy += in.h + uiPx(4); drawText(fontXS, cx, yy, "сплошная — ⟨x⟩ волны, пунктир — шарик на пружине", C_DIM); yy += fontXS.h + uiPx(8);
    }
    if (scene == 4) {
        uiSection(cx, yy, cw, "интенсивность по углу −60…60°");
        PR in{cx, yy, cw, uiPx(110)}; boxPanel(in.x, in.y, in.w, in.h, C_PANEL2, C_LINE);
        float mx = 1e-12f; for (int q = 0; q < 121; q++) if (std::abs(q - 60) > 3) mx = std::max(mx, angI[q]);
        std::vector<float> a(angI.begin(), angI.end()); for (auto& v : a) v = std::min(v, mx * 1.2f);
        drawSeries(in, a, 0, mx * 1.25, C_TEXT_HI, 1.4f, LS_SOLID, -1);
        for (int n = -2; n <= 2; n++) {   // теория: d·sin θ = nλ
            if (!n) continue; const double s = n * lam / latD; if (std::fabs(s) >= std::sin(60 * PI / 180)) continue;
            const float px = in.x + (float)((std::asin(s) * 180 / PI + 60) / 120 * in.w); lineV(px, in.y, in.y + in.h, withA(C_DIM, 0.8f));
        }
        yy += in.h + uiPx(4); drawText(fontXS, cx, yy, "вертикальные линии — углы d·sin θ = nλ", C_DIM); yy += fontXS.h + uiPx(8);
    }
    static const char* NOTES[5] = {
        "Энергии электрона не хватает, чтобы перелезть через барьер, но волна проникает под него и затухает экспоненциально. "
        "Если барьер тонкий, «хвост» волны выходит с другой стороны — часть вероятности проходит. На туннельном эффекте работают "
        "туннельный микроскоп, флеш-память и альфа-распад ядер. Щёлкните по волне — это измерение положения.",
        "Волна проходит через обе щели сразу, и за ними две волны складываются: где гребень встречает гребень — светлая полоса, "
        "где гребень встречает впадину — тёмная. Но каждый электрон щёлкает по экрану в одной точке — картина складывается из отдельных попаданий. "
        "Включите прибор у щелей: узнав, через какую щель прошёл электрон, мы превращаем волну в одну из двух половин — и полосы исчезают.",
        "В параболической яме (как атом в кристалле или молекула на пружинке связи) уровни энергии равноотстоят: Eₙ = ħω(n + ½). "
        "Гауссов пакет ширины основного состояния качается как классический шарик и не расплывается. Сжатое состояние «дышит»: "
        "неопределённость переходит из координаты в импульс и обратно.",
        "В ящике энергии уровней растут как n², поэтому фазы всех волн снова совпадают через время T = 4mL²/(πħ): пакет возрождается. "
        "В доли периода (T/2, T/3, T/4…) он собирается в несколько своих копий — отсюда узор, похожий на ковёр.",
        "Электрон с энергией 10 эВ имеет длину волны около 0.4 нм — как расстояние между атомами. Кристалл работает как дифракционная решётка: "
        "волны, рассеянные рядами атомов, усиливают друг друга только под углами d·sin θ = nλ. Так в 1927 году доказали, что электрон — волна."};
    yy += drawWrapped(fontXS, cx, yy, cw, NOTES[clampv(scene, 0, 4)], C_DIM) + uiPx(12);
    scrollEnd(6, yy);
}
static std::string wvReport() {
    using namespace wv;
    switch (scene) {
    case 0: return fmt("t = %.1f фс · прошло %.1f%%, отразилось %.1f%% · теория %.1f%%", t, 100 * (wvNorm(N / 2, N) + lostR) / norm0, 100 * (wvNorm(0, N / 2) + lostL) / norm0, 100 * wvTunnelT(E0, barV, barW));
    case 1: return fmt("t = %.1f фс · электронов на экране %d", t, (int)dots.size());
    case 2: return fmt("t = %.1f фс · ⟨x⟩ = %+.2f нм, классически %+.2f нм", t, histX.empty() ? 0.0 : histX.back(), histXc.empty() ? 0.0 : histXc.back());
    case 3: return fmt("t = %.3f·T, T = %.0f фс", t / carpetT, carpetT);
    default: {
        int best = 60; float bv = 0; for (int q = 80; q < 121; q++) if (angI[q] > bv) { bv = angI[q]; best = q; }   // луч первого порядка — дальше 20° от прямого
        const double s = 2 * PI / k0() / latD;
        return fmt("t = %.1f фс · ярче всего луч под %d° (теория первого луча %.1f°)", t, best - 60, s < 1 ? std::asin(s) * 180 / PI : 0.0); }
    }
}

// ===================================== UI (immediate mode): основа ==========================
struct PR { float x, y, w, h; };
struct UIState { int mx = 0, my = 0; bool down = false, pressed = false, released = false; int active = -1; int wheel = 0; } ui;
static int sliderReleased = -1;
static bool uiModal = false;           // открыто модальное окно: элементы под ним не реагируют
static double uiClock = 0;             // секунды с запуска (медленное вращение миниатюр атомов)
static int avZ = 6;                    // элемент окна «Строение атома» (orbitals.inl)
static void drawAtomMini(int Z, float x, float y, float s, float t);   // orbitals.inl
// всплывающие подсказки: элемент под курсором и его текст (показывается после короткой задержки)
static int hotId = -1, hintShownId = -1; static std::string hotHint; static double hintTime = 0;
static inline void setHot(int id, const char* hint) { hotId = id; hotHint = hint ? hint : ""; }
// прямоугольники элементов интерфейса (для автотеста) и отсечение рисования (стек областей)
static std::map<int, PR> uiRects;
static std::vector<std::array<int, 4>> clipStack;
static void applyClip() {
    if (clipStack.empty()) { glDisable(GL_SCISSOR_TEST); return; }
    auto& c = clipStack.back(); glEnable(GL_SCISSOR_TEST); glScissor(c[0], winH - c[1] - c[3], std::max(0, c[2]), std::max(0, c[3]));
}
static void pushClip(float x, float y, float w, float h) {
    int x0 = (int)x, y0 = (int)y, x1 = (int)(x + w), y1 = (int)(y + h);
    if (!clipStack.empty()) { auto& c = clipStack.back(); x0 = std::max(x0, c[0]); y0 = std::max(y0, c[1]); x1 = std::min(x1, c[0] + c[2]); y1 = std::min(y1, c[1] + c[3]); }
    clipStack.push_back({x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)}); applyClip();
}
static void popClip() { if (!clipStack.empty()) clipStack.pop_back(); applyClip(); }
static bool inPR(const PR& r) { return ui.mx >= r.x && ui.mx < r.x + r.w && ui.my >= r.y && ui.my < r.y + r.h; }
// курсор внутри текущей области отсечения (прокрученные за край элементы не реагируют)
static bool inClipNow(float x, float y) {
    if (clipStack.empty()) return true;
    auto& c = clipStack.back(); return x >= c[0] && x < c[0] + c[2] && y >= c[1] && y < c[1] + c[3];
}
// ---- прокручиваемые области (боковая панель): колесо над областью, полоса прокрутки справа
struct ScrollArea { float off = 0, content = 0, x = 0, y = 0, w = 0, h = 0; };
static ScrollArea scrollAreas[8]; static int curScroll = -1;   // 6 — панели миров (worlds.inl)
static std::map<int, std::pair<int, float>> uiScrollPos;   // элемент → (область, положение в содержимом) — для автотеста
static float scrollBegin(int k, float x, float y, float w, float h) {
    ScrollArea& a = scrollAreas[k]; a.x = x; a.y = y; a.w = w; a.h = h;
    a.off = clampv(a.off, 0.0f, std::max(0.0f, a.content - h));
    pushClip(x, y, w, h); curScroll = k;
    return y - a.off;
}
static void scrollEnd(int k, float contentBottom) {
    ScrollArea& a = scrollAreas[k];
    a.content = contentBottom - (a.y - a.off);
    popClip(); curScroll = -1;
    const bool over = !uiModal && ui.mx >= a.x && ui.mx < a.x + a.w + uiPx(8) && ui.my >= a.y && ui.my < a.y + a.h;
    if (over && ui.wheel != 0) { a.off -= ui.wheel / 120.0f * uiPx(56); ui.wheel = 0; }
    a.off = clampv(a.off, 0.0f, std::max(0.0f, a.content - a.h));
    if (a.content > a.h + 1) {   // полоса прокрутки (перетаскивается)
        float tx = a.x + a.w + uiPx(2), tw = uiPx(4), th = std::max(uiPx(24), a.h * a.h / a.content), ty = a.y + (a.h - th) * a.off / (a.content - a.h);
        int id = 690 + k; bool hov = !uiModal && ui.mx >= tx - uiPx(3) && ui.mx < tx + tw + uiPx(3) && ui.my >= a.y && ui.my < a.y + a.h;
        if (hov && ui.pressed) ui.active = id;
        if (ui.active == id) { a.off = clampv((ui.my - a.y - th / 2) / std::max(1.0f, a.h - th) * (a.content - a.h), 0.0f, a.content - a.h); if (ui.released) ui.active = -1; }
        rectFill(tx, a.y, tw, a.h, C_PANEL2); rectFill(tx, ty, tw, th, hov || ui.active == id ? C_LINE_H : C_LINE);
    }
}
static void uiScrollTo(int id) {   // прокрутить так, чтобы элемент был виден (автотест)
    auto it = uiScrollPos.find(id); if (it == uiScrollPos.end()) return;
    ScrollArea& a = scrollAreas[it->second.first]; a.off = clampv(it->second.second - a.h * 0.4f, 0.0f, std::max(0.0f, a.content - a.h));
}
static void uiRegister(int id, float x, float y, float w, float h) {
    uiRects[id] = {x, y, w, h};
    if (curScroll >= 0) { const ScrollArea& a = scrollAreas[curScroll]; uiScrollPos[id] = {curScroll, y - (a.y - a.off)}; }
    else uiScrollPos.erase(id);
}
static bool uiHover(float x, float y, float w, float h) { return !uiModal && ui.mx >= x && ui.mx < x + w && ui.my >= y && ui.my < y + h && inClipNow((float)ui.mx, (float)ui.my); }

// ---- элементы управления. Стиль: плоские прямоугольники (радиус ≤ 3 px), рамка 1 px, активное — акцентная подсветка.
// кнопка; accent — выделенная рамкой (важные действия); swatch — цветная метка слева (элемент палитры)
static bool uiButton(int id, float x, float y, float w, float h, const std::string& label, bool on, bool chem = false, const char* hint = nullptr,
                     const RGBA* swatch = nullptr, bool accent = false) {
    uiRegister(id, x, y, w, h);
    bool hover = uiHover(x, y, w, h);
    bool clicked = false;
    if (hover) setHot(id, hint);
    if (hover && ui.pressed) ui.active = id;
    if (ui.active == id && ui.released) { if (hover) clicked = true; ui.active = -1; }
    RGBA bg = on ? C_ACC_BG : (hover ? C_ELEM_H : C_ELEM);
    if (ui.active == id && hover) bg = C_PANEL2;
    RGBA ln = on ? withA(C_ACC, 0.8f) : accent ? withA(C_ACC, 0.5f) : (hover ? C_LINE_H : C_LINE);
    roundRect(x, y, w, h, 2, bg); roundLine(x, y, w, h, 2, ln);
    float sw = 0;
    if (swatch) { sw = uiPx(11); MonoAtoms atomsColored; rectFill(x + uiPx(5), y + std::floor((h - uiPx(8)) / 2), uiPx(8), uiPx(8), *swatch); }
    const Font* fp = &fontU;
    float tw = chem ? chemW(*fp, label) : textW(*fp, label);
    if (tw > w - uiPx(6) - sw) { fp = &fontXS; tw = chem ? chemW(*fp, label) : textW(*fp, label); }   // не влезает — мельче
    const Font& f = *fp;
    RGBA tc = on || accent ? C_TEXT_HI : (hover ? C_TEXT_HI : C_TEXT);
    float tx = tw > w - uiPx(8) - sw ? x + uiPx(4) + sw : x + sw + (w - sw - tw) / 2;   // не влезает — по левому краю с обрезкой
    pushClip(x + 1, y, w - 2, h);
    if (chem) drawChem(f, tx, y + (h - f.h) / 2, label, tc);
    else drawText(f, tx, y + (h - f.h) / 2, label, tc);
    popClip();
    return clicked;
}
// слайдер: подпись и значение сверху, тонкая дорожка снизу; перетаскивание, колесо мыши над ним — точная подстройка
static bool uiSlider(int id, float x, float y, float w, float h, const std::string& label, double* v, double lo, double hi, bool logs, const std::string& val,
                     const char* hint = nullptr) {
    uiRegister(id, x, y, w, h);
    bool hover = uiHover(x, y, w, h);
    if (hover) setHot(id, hint);
    if (hover && ui.pressed) ui.active = id;
    bool changed = false;
    float tx = x + uiPx(4), tw = w - uiPx(8), ty = y + h - uiPx(5);
    auto toU = [&](double a) { return logs ? std::log(a / lo) / std::log(hi / lo) : (a - lo) / (hi - lo); };
    auto fromU = [&](double u) { return logs ? lo * std::pow(hi / lo, u) : lo + u * (hi - lo); };
    if (ui.active == id) {
        double nv = fromU(clampv((ui.mx - tx) / (double)tw, 0.0, 1.0));
        if (nv != *v) { *v = nv; changed = true; }
        if (ui.released) { ui.active = -1; sliderReleased = id; }
    } else if (hover && ui.wheel != 0) {
        double nv = fromU(clampv(toU(*v) + 0.02 * ui.wheel / 120.0, 0.0, 1.0));
        if (nv != *v) { *v = nv; changed = true; sliderReleased = id; }
        ui.wheel = 0;
    }
    float u = (float)clampv(toU(*v), 0.0, 1.0);
    const bool act = hover || ui.active == id;
    if (act) rectFill(x, y, w, h, withA(C_ELEM, 0.6f));
    float vw = textW(fontS, val);
    pushClip(x + 2, y, w - vw - uiPx(12), h); drawText(fontU, x + uiPx(4), y + uiPx(1), label, act ? C_TEXT_HI : C_TEXT); popClip();
    drawText(fontS, x + w - uiPx(4) - vw, y + uiPx(2), val, act ? C_TEXT_HI : C_TEXT);
    rectFill(tx, ty, tw, uiPx(2), C_LINE);
    rectFill(tx, ty, std::max(1.0f, std::floor(tw * u)), uiPx(2), withA(C_ACC, act ? 1.0f : 0.8f));
    float kx = std::floor(tx + tw * u), kw = uiPx(5), kh = uiPx(10);
    rectFill(kx - kw / 2, ty + uiPx(1) - kh / 2, kw, kh, ui.active == id ? C_ACC : (hover ? C_TEXT_HI : C_TEXT));
    return changed;
}
// флажок: квадрат 1 px + галочка
static bool uiCheck(int id, float x, float y, float w, float h, const std::string& label, bool* v, const char* hint = nullptr) {
    uiRegister(id, x, y, w, h);
    bool hover = uiHover(x, y, w, h), clicked = false;
    if (hover) setHot(id, hint);
    if (hover && ui.pressed) ui.active = id;
    if (ui.active == id && ui.released) { if (hover) { clicked = true; *v = !*v; } ui.active = -1; }
    float bs = uiPx(12), bx = x + uiPx(2), by = y + std::floor((h - bs) / 2);
    rectFill(bx, by, bs, bs, *v ? C_ACC_BG : C_ELEM); rectLine(bx, by, bs, bs, *v ? C_ACC : (hover ? C_LINE_H : C_LINE));
    if (*v) { glEnable(GL_LINE_SMOOTH); glLineWidth(std::max(1.0f, 1.5f * uiScale)); col(C_ACC); glBegin(GL_LINE_STRIP);
              glVertex2f(bx + bs * 0.22f, by + bs * 0.52f); glVertex2f(bx + bs * 0.43f, by + bs * 0.74f); glVertex2f(bx + bs * 0.8f, by + bs * 0.28f); glEnd();
              glLineWidth(1); glDisable(GL_LINE_SMOOTH); }
    pushClip(x, y, w, h); drawText(fontU, bx + bs + uiPx(7), y + (h - fontU.h) / 2, label, hover ? C_TEXT_HI : C_TEXT); popClip();
    return clicked;
}
// переключатель режима по кругу: «подпись ........ значение ▸»
static bool uiCycle(int id, float x, float y, float w, float h, const std::string& label, const std::string& value, bool on, const char* hint = nullptr) {
    uiRegister(id, x, y, w, h);
    bool hover = uiHover(x, y, w, h), clicked = false;
    if (hover) setHot(id, hint);
    if (hover && ui.pressed) ui.active = id;
    if (ui.active == id && ui.released) { if (hover) clicked = true; ui.active = -1; }
    rectFill(x, y, w, h, ui.active == id && hover ? C_PANEL2 : (hover ? C_ELEM_H : C_ELEM)); rectLine(x, y, w, h, hover ? C_LINE_H : C_LINE);
    if (on) rectFill(x, y, uiPx(2), h, C_ACC);
    float ty = y + (h - fontU.h) / 2, aw = uiPx(10);
    float vw = textW(fontU, value);
    pushClip(x, y, w - vw - aw - uiPx(12), h); drawText(fontU, x + uiPx(8), ty, label, C_DIM); popClip();
    drawText(fontU, x + w - aw - uiPx(6) - vw, ty, value, on ? C_TEXT_HI : C_TEXT);
    float ax = x + w - aw, ay = y + h / 2; col(hover ? C_TEXT_HI : C_DIM);
    glBegin(GL_TRIANGLES); glVertex2f(ax - uiPx(1), ay - uiPx(3.5f)); glVertex2f(ax + uiPx(3), ay); glVertex2f(ax - uiPx(1), ay + uiPx(3.5f)); glEnd();
    return clicked;
}
// квадратная кнопка со значком (панель инструментов, верхняя панель)
static bool uiIconBtn(int id, float x, float y, float w, float h, int icon, bool on, const char* hint, RGBA tint = C_TEXT) {
    uiRegister(id, x, y, w, h);
    bool hover = uiHover(x, y, w, h), clicked = false;
    if (hover) setHot(id, hint);
    if (hover && ui.pressed) ui.active = id;
    if (ui.active == id && ui.released) { if (hover) clicked = true; ui.active = -1; }
    if (on) { rectFill(x, y, w, h, C_ACC_BG); rectLine(x, y, w, h, withA(C_ACC, 0.8f)); }
    else if (hover) { rectFill(x, y, w, h, ui.active == id ? C_PANEL2 : C_ELEM_H); rectLine(x, y, w, h, C_LINE_H); }
    drawIcon(icon, x + w / 2, y + h / 2, std::min(w, h) * 0.52f, on ? C_ACC : (hover ? C_TEXT_HI : tint));
    return clicked;
}
// вкладка: текст, активная — светлый текст и акцентная линия снизу
static bool uiTab(int id, float x, float y, float w, float h, const std::string& label, bool on, const char* hint = nullptr) {
    uiRegister(id, x, y, w, h);
    bool hover = uiHover(x, y, w, h), clicked = false;
    if (hover) setHot(id, hint);
    if (hover && ui.pressed) ui.active = id;
    if (ui.active == id && ui.released) { if (hover) clicked = true; ui.active = -1; }
    if (on) rectFill(x, y, w, h, C_PANEL); else if (hover) rectFill(x, y, w, h, C_ELEM);
    if (on) rectFill(x, y + h - uiPx(2), w, uiPx(2), C_ACC);
    const Font& f = on ? fontUB : fontU;
    pushClip(x, y, w, h); drawTextC(f, x + w / 2, y + (h - f.h) / 2, label, on ? C_TEXT_HI : (hover ? C_TEXT : C_DIM)); popClip();
    return clicked;
}
// заголовок раздела: текст + тонкая линия до правого края
static void uiSection(float x, float& y, float w, const std::string& title) {
    y += uiPx(6);
    float tw = drawText(fontUB, x, y, title, C_TEXT_HI);
    lineH(x + tw + uiPx(8), x + w, y + fontUB.h * 0.55f, C_LINE);
    y += fontUB.h + uiPx(4);
}
// многострочный текст с переносом по словам (текст переводится целиком до переноса); возвращает высоту
static float drawWrapped(const Font& f, float x, float y, float w, std::string_view s0, RGBA c, float lineH_ = 0) {
    const std::string_view s = Tsv(s0, "drawWrapped");
    float lh = lineH_ > 0 ? lineH_ : f.h + uiPx(1), yy = y;
    size_t p = 0;
    while (p <= s.size()) {
        size_t nl = s.find('\n', p); if (nl == std::string_view::npos) nl = s.size();
        std::string_view para = s.substr(p, nl - p); std::string line;
        size_t q = 0;
        while (q <= para.size()) {
            size_t sp = para.find(' ', q); if (sp == std::string_view::npos) sp = para.size();
            std::string word(para.substr(q, sp - q)), cand = line.empty() ? word : line + " " + word;
            if (!line.empty() && textWRaw(f, cand) > w) { drawTextRaw(f, x, yy, line, c); yy += lh; line = word; } else line = cand;
            q = sp + 1;
        }
        drawTextRaw(f, x, yy, line, c); yy += lh;
        p = nl + 1;
    }
    return yy - y;
}
// всплывающая подсказка (рисуется последней; текст переводится целиком, затем делится на строки)
static void drawHint(double frameDt) {
    if (hotId != hintShownId) { hintShownId = hotId; hintTime = 0; }
    else hintTime += frameDt;
    if (!opt.hints || hotId < 0 || hotHint.empty() || hintTime < 0.45 || ui.down) return;
    const std::string_view hh = Tsv(hotHint, "drawHint");
    std::vector<std::string_view> L; size_t s = 0;
    for (size_t k = 0; k <= hh.size(); k++) if (k == hh.size() || hh[k] == '\n') { L.push_back(hh.substr(s, k - s)); s = k + 1; }
    float w = 0; for (size_t k = 0; k < L.size(); k++) w = std::max(w, textWRaw(k == 0 && L.size() > 1 ? fontUB : fontU, L[k]));
    float lh = fontU.h + uiPx(2), h = L.size() * lh + uiPx(10), x = (float)ui.mx + uiPx(14), y = (float)ui.my + uiPx(20);
    if (x + w + uiPx(20) > winW) x = winW - w - uiPx(24);
    if (y + h > winH - uiPx(4)) y = (float)ui.my - h - uiPx(10);
    float a = (float)std::min(1.0, (hintTime - 0.45) * 6);
    boxPanel(x, y, w + uiPx(18), h, withA(C_PANEL2, 0.97f * a), withA(C_LINE_H, a));
    rectFill(x, y, uiPx(2), h, withA(C_ACC, 0.8f * a));
    for (size_t k = 0; k < L.size(); k++) drawTextRaw(k == 0 && L.size() > 1 ? fontUB : fontU, x + uiPx(10), y + uiPx(5) + k * lh, L[k], withA(k == 0 ? C_TEXT_HI : C_TEXT, a));
}

// ===================================== ГРАФИКИ ===========================================
static PR plotFrame(PR r, const std::string& title, const std::string& info) {
    boxPanel(r.x, r.y, r.w, r.h, C_PANEL2, C_LINE);
    // заголовок слева целиком; справа — значение (если не помещается, обрезается слева)
    float tw = std::min(textW(fontXS, title), r.w - uiPx(12));
    pushClip(r.x + uiPx(6), r.y, tw + 1, uiPx(20)); drawText(fontXS, r.x + uiPx(6), r.y + uiPx(3), title, C_DIM); popClip();
    if (!info.empty()) { float ix = r.x + uiPx(6) + tw + uiPx(10); pushClip(ix, r.y, std::max(0.0f, r.x + r.w - uiPx(4) - ix), uiPx(20)); drawTextR(fontXS, r.x + r.w - uiPx(6), r.y + uiPx(3), info, C_TEXT); popClip(); }
    PR in = {r.x + uiPx(6), r.y + uiPx(21), r.w - uiPx(12), r.h - uiPx(27)};
    glDisable(GL_TEXTURE_2D); col(C_GRID); glBegin(GL_LINES);
    for (int k = 0; k <= 4; k++) { float x = std::floor(in.x + in.w * k / 4) + 0.5f; glVertex2f(x, in.y); glVertex2f(x, in.y + in.h); }
    for (int k = 0; k <= 3; k++) { float y = std::floor(in.y + in.h * k / 3) + 0.5f; glVertex2f(in.x, y); glVertex2f(in.x + in.w, y); }
    glEnd();
    return in;
}
static inline float mapX(const PR& in, double x, double x0, double x1) { return in.x + (float)((x - x0) / (x1 - x0)) * in.w; }
static inline float mapY(const PR& in, double y, double y0, double y1) { return in.y + in.h - (float)clampv((y - y0) / (y1 - y0), -0.02, 1.02) * in.h; }
static void seriesRange(const std::vector<float>& v, double& lo, double& hi) {
    for (float f : v) { if (!std::isfinite(f)) continue; lo = std::min(lo, (double)f); hi = std::max(hi, (double)f); }
}
static void niceRange(double& lo, double& hi) { if (hi - lo < 1e-6) { hi += 0.5; lo -= 0.5; } double m = 0.08 * (hi - lo); lo -= m; hi += m; }
// ряд значений; span — сколько точек занимает вся ширина (0 — окно истории cfg::HIST, −1 — ровно размер ряда)
static void drawSeries(const PR& in, const std::vector<float>& v, double lo, double hi, RGBA c, float w = 1.2f, int style = LS_SOLID, int span = 0) {
    if (v.size() < 2) return;
    const double last = span > 0 ? span - 1 : (span < 0 ? (double)v.size() - 1 : cfg::HIST - 1);
    glLineWidth(w); glEnable(GL_LINE_SMOOTH); lineStyle(style); col(c); glBegin(GL_LINE_STRIP);
    for (size_t k = 0; k < v.size(); k++) glVertex2f(mapX(in, (double)k, 0, last), mapY(in, v[k], lo, hi));
    glEnd(); lineStyle(LS_SOLID); glDisable(GL_LINE_SMOOTH); glLineWidth(1);
}
static void hline(const PR& in, double y, double lo, double hi, RGBA c) {
    glEnable(GL_LINE_STIPPLE); glLineStipple(2, 0x3333); col(c);
    glBegin(GL_LINES); glVertex2f(in.x, mapY(in, y, lo, hi)); glVertex2f(in.x + in.w, mapY(in, y, lo, hi)); glEnd(); glDisable(GL_LINE_STIPPLE);
}
static void axisLabels(const PR& in, double lo, double hi, const char* f = "%.3g") {
    std::string a = fmt(f, hi), b = fmt(f, lo);
    rectFill(in.x + uiPx(1), in.y, textW(fontXS, a) + uiPx(3), fontXS.h - uiPx(2), withA(C_PANEL2, 0.85f));
    rectFill(in.x + uiPx(1), in.y + in.h - fontXS.h + uiPx(1), textW(fontXS, b) + uiPx(3), fontXS.h - uiPx(1), withA(C_PANEL2, 0.85f));
    drawText(fontXS, in.x + uiPx(2), in.y - uiPx(1), a, withA(C_DIM, 0.95f));
    drawText(fontXS, in.x + uiPx(2), in.y + in.h - fontXS.h, b, withA(C_DIM, 0.95f));
}
static const RGBA* SPC = C_SERIES;   // цвета веществ на графике концентраций
// уравнение реакции: формулы с индексами, стрелки и плюсы — обычным текстом
static float drawEquation(const Font& f, float x, float y, const std::string& eq, RGBA c) {
    float x0 = x; size_t s = 0;
    while (s <= eq.size()) {
        size_t e = eq.find(' ', s); if (e == std::string::npos) e = eq.size();
        std::string tok = eq.substr(s, e - s);
        if (!tok.empty()) x += drawChem(f, x, y, tok, c) + textW(f, " ");
        s = e + 1;
    }
    return x - x0;
}
// Все графики сеткой 2 × 6 (журнал реакций — на всю ширину); высота ячейки не меньше minH. Возвращает нижнюю границу.
static float drawPlots(float x0, float y0, float w, float h, float minH = 0) {
    int cols = 2, rows = 6; float gap = uiPx(6);
    float pw = (w - gap * (cols - 1)) / cols, ph = std::max(minH, (h - gap * (rows - 1)) / rows);
    auto cell = [&](int k) { int c = k % cols, r = k / cols; return PR{std::floor(x0 + c * (pw + gap)), std::floor(y0 + r * (ph + gap)), std::floor(pw), std::floor(ph)}; };
    // ряды: T и Eк — белые сплошные, P, g(r) и Eп — серые штриховые, Eполн — светлая толстая, идеальный газ — пунктир
    const RGBA cT = C_HOT, cB = grayc(0.62f), cW = grayc(0.85f), cG = grayc(0.8f);
    const double V = boxVolume();
    // 1. распределение скоростей + Максвелл–Больцман
    {
        double m = EL[A::vhType].m, T = std::max(EN.T, 0.005), vmax = A::vhMax;
        double ymax = 0; for (double v : A::vh) ymax = std::max(ymax, v);
        double vp = std::sqrt(2 * T / m); ymax = std::max(ymax, maxwellF(vp, m, T)) * 1.15;
        PR in = plotFrame(cell(0), "f(v) Максвелл", fmt("%s  T=%.2f", EL[A::vhType].sym, EN.T));
        float bw = in.w / A::VH_BINS;
        col(withA(C_ACC, 0.3f)); glBegin(GL_QUADS);
        for (int k = 0; k < A::VH_BINS; k++) { float x = in.x + k * bw, y = mapY(in, A::vh[k], 0, ymax); glVertex2f(x + 1, y); glVertex2f(x + bw - 1, y); glVertex2f(x + bw - 1, in.y + in.h); glVertex2f(x + 1, in.y + in.h); }
        glEnd();
        glLineWidth(1.3f); glEnable(GL_LINE_SMOOTH); col(cT); glBegin(GL_LINE_STRIP);
        for (int k = 0; k <= 100; k++) { double v = vmax * k / 100; glVertex2f(mapX(in, v, 0, vmax), mapY(in, maxwellF(v, m, T), 0, ymax)); }
        glEnd(); glDisable(GL_LINE_SMOOTH); glLineWidth(1);
        drawTextR(fontXS, in.x + in.w, in.y + in.h - fontXS.h, fmt("v → %.2g σ/τ", vmax), C_DIM);
    }
    // 2. g(r)
    {
        double gmax = 3; for (double g : A::gr) gmax = std::max(gmax, g * 1.1);
        double pk = 0, pr = 0; for (int k = 0; k < A::GR_BINS; k++) if (A::gr[k] > pk) { pk = A::gr[k]; pr = (k + 0.5) * A::GR_RMAX / A::GR_BINS; }
        PR in = plotFrame(cell(1), "g(r)", fmt("пик %.2f при %.2f Å", pk, pr * 3.405));
        hline(in, 1, 0, gmax, withA(C_TEXT, 0.25f));
        glLineWidth(1.3f); glEnable(GL_LINE_SMOOTH); col(cB); glBegin(GL_LINE_STRIP);
        for (int k = 0; k < A::GR_BINS; k++) glVertex2f(mapX(in, (k + 0.5) * A::GR_RMAX / A::GR_BINS, 0, A::GR_RMAX), mapY(in, A::gr[k], 0, gmax));
        glEnd(); glDisable(GL_LINE_SMOOTH); glLineWidth(1);
        axisLabels(in, 0, gmax, "%.1f"); drawTextR(fontXS, in.x + in.w, in.y + in.h - fontXS.h, fmt("r → %.1f Å", A::GR_RMAX * 3.405), C_DIM);
    }
    // 3. T(t)
    {
        double lo = 1e30, hi = -1e30; seriesRange(A::sT.v, lo, hi);
        bool th = P.thermostat != TH_NVE && P.thermostat != TH_POWER;
        if (th) { lo = std::min(lo, P.Tset); hi = std::max(hi, P.Tset); }
        if (lo > hi) { lo = 0; hi = 1; } niceRange(lo, hi);
        PR in = plotFrame(cell(2), "T(t)", fmt("%.3f · %.0f K", EN.T, realK(EN.T)));
        if (th) hline(in, P.Tset, lo, hi, withA(cT, 0.45f));
        drawSeries(in, A::sT.v, lo, hi, cT); axisLabels(in, lo, hi);
    }
    // 4. P(t) и идеальный газ NkT/V
    {
        double Pid = EN.nmob * EN.T / V;
        double lo = 1e30, hi = -1e30; seriesRange(A::sP.v, lo, hi); lo = std::min(lo, Pid); hi = std::max(hi, Pid);
        if (lo > hi) { lo = 0; hi = 1; } niceRange(lo, hi);
        double Z = Pid > 1e-9 ? EN.P / Pid : 0;
        PR in = plotFrame(cell(3), "P(t)", fmt("%.1f бар · Z=%.2f", realBar(EN.P), Z));
        hline(in, Pid, lo, hi, withA(cG, 0.6f));
        drawSeries(in, A::sP.v, lo, hi, cT); axisLabels(in, lo, hi);
        drawTextR(fontXS, in.x + in.w, in.y + in.h - fontXS.h, "- - NkT/V", withA(cG, 0.9f));
    }
    // 5. энергии
    {
        double lo = 1e30, hi = -1e30; seriesRange(A::sEk.v, lo, hi); seriesRange(A::sEp.v, lo, hi); seriesRange(A::sEt.v, lo, hi);
        if (lo > hi) { lo = 0; hi = 1; } niceRange(lo, hi);
        PR in = plotFrame(cell(4), "E(t), ε", fmt("E = %.1f", EN.total()));
        drawSeries(in, A::sEk.v, lo, hi, cT); drawSeries(in, A::sEp.v, lo, hi, cB, 1.3f, LS_DASH); drawSeries(in, A::sEt.v, lo, hi, cW, 2.2f, LS_DOT);
        axisLabels(in, lo, hi, "%.0f");
        // легенда: образец линии + подпись
        const float sw = uiPx(14), ly2 = in.y + in.h - fontXS.h, sy = ly2 + fontXS.h * 0.55f;
        float lx = in.x + in.w; lx -= drawTextR(fontXS, lx, ly2, "Eполн", C_TEXT) + uiPx(3) + sw; styleSample(lx, sy, sw, cW, LS_DOT, 2.2f); lx -= uiPx(8);
        lx -= drawTextR(fontXS, lx, ly2, "Eп", C_TEXT) + uiPx(3) + sw; styleSample(lx, sy, sw, cB, LS_DASH); lx -= uiPx(8);
        lx -= drawTextR(fontXS, lx, ly2, "Eк", C_TEXT) + uiPx(3) + sw; styleSample(lx, sy, sw, cT, LS_SOLID);
    }
    // 6. MSD(t)
    {
        bool big = present[E_BIG];
        double hi = 1e-6; for (float f : A::msdV) hi = std::max(hi, (double)f); if (big) for (float f : A::msdBig) hi = std::max(hi, (double)f);
        double tmax = A::msdT.empty() ? 1 : std::max(1e-3, (double)A::msdT.back());
        PR in = plotFrame(cell(5), big ? "MSD броун. частицы" : "MSD(t)", fmt("D = %.4f σ²/τ", std::max(0.0, big ? A::Dbig : A::D)));
        glLineWidth(1.3f); glEnable(GL_LINE_SMOOTH);
        auto line = [&](const std::vector<float>& s, RGBA c) { col(c); glBegin(GL_LINE_STRIP); for (size_t k = 0; k < s.size() && k < A::msdT.size(); k++) glVertex2f(mapX(in, A::msdT[k], 0, tmax), mapY(in, s[k], 0, hi * 1.05)); glEnd(); };
        if (big) { line(A::msdV, withA(cB, 0.6f)); line(A::msdBig, cT); } else line(A::msdV, cB);
        glDisable(GL_LINE_SMOOTH); glLineWidth(1);
        axisLabels(in, 0, hi * 1.05); drawTextR(fontXS, in.x + in.w, in.y + in.h - fontXS.h, fmt("t → %.3g τ", tmax), C_DIM);
    }
    // 7. концентрации веществ (кинетика)
    {
        std::vector<int> idx; for (size_t k = 0; k < A::species.size(); k++) idx.push_back((int)k);
        std::sort(idx.begin(), idx.end(), [](int a, int b) { float fa = A::conc[a].v.empty() ? 0 : A::conc[a].v.back(), fb = A::conc[b].v.empty() ? 0 : A::conc[b].v.back(); return fa > fb; });
        double hi = 1; for (auto& s : A::conc) for (float f : s.v) hi = std::max(hi, (double)f);
        std::string info = A::progressLabel.empty() ? "" : fmt("израсх. %s: %.0f%%", A::progressLabel.c_str(), A::progress * 100);
        PR in = plotFrame(cell(6), "Молекулы", info);
        for (size_t q = 0; q < idx.size() && q < 7; q++) drawSeries(in, A::conc[idx[q]].v, 0, hi * 1.05, SPC[idx[q] % 12], 1.3f, LS_SERIES[idx[q] % 12]);
        float lx = in.x + uiPx(2), ly = in.y + uiPx(1);
        pushClip(in.x, in.y, in.w, in.h);
        for (size_t q = 0; q < idx.size() && q < 7; q++) {
            int k = idx[q]; std::string s = A::species[k]; int c = A::conc[k].v.empty() ? 0 : (int)A::conc[k].v.back();
            const float sw = uiPx(12);   // образец линии ряда перед формулой
            float ww = sw + uiPx(3) + chemW(fontXS, s) + textW(fontXS, std::to_string(c)) + uiPx(3);
            if (lx + ww > in.x + in.w && lx > in.x + uiPx(4)) { lx = in.x + uiPx(2); ly += fontXS.h; }
            styleSample(lx, ly + fontXS.h * 0.55f, sw, SPC[k % 12], LS_SERIES[k % 12]);
            float dw = drawChem(fontXS, lx + sw + uiPx(3), ly, s, SPC[k % 12]); drawText(fontXS, lx + sw + uiPx(3) + dw + uiPx(3), ly, std::to_string(c), C_DIM);
            lx += ww + uiPx(10);
        }
        popClip();
    }
    // 8. фазовая диаграмма ρ–T для LJ (rc=2.5σ со сдвигом, приближённо)
    {
        double rho = EN.nmob / V;
        PR in = plotFrame(cell(7), "ρ–T (LJ, ≈)", fmt("ρ=%.3f T=%.2f", rho, EN.T));
        // Tc ≈ 1.08, ρc ≈ 0.32, T_тр ≈ 0.69
        const double Tc = 1.08, rc = 0.32, Tt = 0.69, TM = 2.2, RM = 1.25;
        const double rl_t = 0.84, rf0 = 0.845, rfk = 0.16, rmelt = 0.10;
        const double tt = (Tc - Tt) / Tc;
        auto rl = [&](double T) { double t = (Tc - T) / Tc; return rc + (rl_t - rc) * std::pow(t / tt, 0.325); };
        auto rg = [&](double T) { double t = (Tc - T) / Tc; double u = 1 - std::pow(t / tt, 0.325) * 0.95; return rc * u * u; };
        glLineWidth(1.2f); glEnable(GL_LINE_SMOOTH);
        col(withA(cB, 0.85f)); glBegin(GL_LINE_STRIP);   // бинодаль жидкость–газ
        for (int k = 0; k <= 40; k++) { double T = Tt + (Tc - Tt) * k / 40; glVertex2f(mapX(in, rg(T), 0, RM), mapY(in, T, 0, TM)); }
        for (int k = 40; k >= 0; k--) { double T = Tt + (Tc - Tt) * k / 40; glVertex2f(mapX(in, rl(T), 0, RM), mapY(in, T, 0, TM)); }
        glEnd();
        col(withA(cT, 0.85f)); lineStyle(LS_DASH);   // линии затвердевания и плавления — белый штрих (бинодаль — серая сплошная)
        glBegin(GL_LINE_STRIP); for (int k = 0; k <= 20; k++) { double T = Tt + (TM - Tt) * k / 20; glVertex2f(mapX(in, rf0 + rfk * (T - Tt), 0, RM), mapY(in, T, 0, TM)); } glEnd();
        glBegin(GL_LINE_STRIP); for (int k = 0; k <= 20; k++) { double T = Tt + (TM - Tt) * k / 20; glVertex2f(mapX(in, rf0 + rmelt + rfk * (T - Tt), 0, RM), mapY(in, T, 0, TM)); } glEnd();
        lineStyle(LS_SOLID);
        col(withA(C_TEXT, 0.35f)); glBegin(GL_LINES); glVertex2f(mapX(in, 0.02, 0, RM), mapY(in, Tt, 0, TM)); glVertex2f(mapX(in, rf0 + rmelt, 0, RM), mapY(in, Tt, 0, TM)); glEnd();
        glDisable(GL_LINE_SMOOTH); glLineWidth(1);
        drawText(fontXS, mapX(in, 0.08 * RM, 0, RM), mapY(in, 0.8 * TM, 0, TM), "газ", C_DIM);
        drawText(fontXS, mapX(in, 0.45 * RM, 0, RM), mapY(in, 0.62 * TM, 0, TM), "флюид", C_DIM);
        drawText(fontXS, mapX(in, 0.86 * RM, 0, RM), mapY(in, 0.35 * TM, 0, TM), "тв.", C_DIM);
        drawText(fontXS, mapX(in, 0.3 * RM, 0, RM), mapY(in, 0.17 * TM, 0, TM), "газ+тв.", C_DIM);
        drawText(fontXS, mapX(in, 0.3 * RM, 0, RM), mapY(in, Tt + 0.3 * (Tc - Tt), 0, TM), "ж+г", C_DIM);
        float px = mapX(in, clampv(rho, 0.0, RM), 0, RM), py = mapY(in, clampv(EN.T, 0.0, TM), 0, TM);
        col(C_TEXT_HI); glEnable(GL_LINE_SMOOTH); circlePx(px, py, uiPx(4), 16); glDisable(GL_LINE_SMOOTH); rectFill(px - 1, py - 1, 2, 2, C_TEXT_HI);
        drawTextR(fontXS, in.x + in.w, in.y + in.h - fontXS.h, fmt("ρ → %.2g", RM), C_DIM); drawText(fontXS, in.x + uiPx(2), in.y - uiPx(1), fmt("T %.1f", TM), C_DIM);
    }
    // 9. Аррениус: ln k против 1/T → Ea = −наклон
    {
        PR in = plotFrame(cell(8), "ln k(1/T)", A::arrFit ? fmt("Ea ≈ %.2f эВ", A::arrEa / cfg::EV) : "мало данных");
        if (!A::arr.empty()) {
            double x0 = 1e30, x1 = -1e30, y0 = 1e30, y1 = -1e30;
            for (auto& p : A::arr) { x0 = std::min(x0, (double)p.first); x1 = std::max(x1, (double)p.first); y0 = std::min(y0, (double)p.second); y1 = std::max(y1, (double)p.second); }
            niceRange(x0, x1); niceRange(y0, y1);
            col(cT); for (auto& p : A::arr) { float qx = mapX(in, p.first, x0, x1), qy = mapY(in, p.second, y0, y1); rectFill(qx - uiPx(2), qy - uiPx(2), uiPx(4), uiPx(4), cT); }
            if (A::arrFit) {
                double sx = 0, sy = 0; for (auto& p : A::arr) { sx += p.first; sy += p.second; } sx /= A::arr.size(); sy /= A::arr.size();
                col(withA(C_TEXT, 0.5f)); glBegin(GL_LINES);
                glVertex2f(mapX(in, x0, x0, x1), mapY(in, sy - A::arrEa * (x0 - sx), y0, y1)); glVertex2f(mapX(in, x1, x0, x1), mapY(in, sy - A::arrEa * (x1 - sx), y0, y1)); glEnd();
            }
            drawTextR(fontXS, in.x + in.w, in.y + in.h - fontXS.h, "1/T →", C_DIM);
        }
    }
    // 10. профиль температуры (теплопроводность / кипение)
    {
        double hi = 0.1; for (double t : A::tprof) hi = std::max(hi, t * 1.1);
        PR in = plotFrame(cell(9), A::tprofAxis ? "T(y) снизу вверх" : "T(x)", P.heatWalls ? fmt("Tгор=%.2f Tхол=%.2f", P.Thot, P.Tcold) : "");
        float bw = in.w / A::TP_BINS;
        glBegin(GL_QUADS);
        for (int k = 0; k < A::TP_BINS; k++) {
            float u = (float)clampv(A::tprof[k] / hi, 0.0, 1.0);
            RGBA c = grayc(0.3f + 0.7f * u, 0.8f);   // холоднее — темнее, горячее — светлее
            col(c); float x = in.x + k * bw, y = mapY(in, A::tprof[k], 0, hi);
            glVertex2f(x + 1, y); glVertex2f(x + bw - 1, y); glVertex2f(x + bw - 1, in.y + in.h); glVertex2f(x + 1, in.y + in.h);
        }
        glEnd();
        // плотность веществ: линии поверх столбиков T (своя шкала; второе вещество — пунктир)
        double dh = 1e-9; for (int s = 0; s < A::dprofN; s++) for (double d : A::dprof[s]) dh = std::max(dh, d * 1.1);
        glLineWidth(1.4f); glEnable(GL_LINE_SMOOTH);
        for (int s = 0; s < A::dprofN; s++) {
            if (s) { glEnable(GL_LINE_STIPPLE); glLineStipple(2, 0x3333); }
            col(s ? cG : cW); glBegin(GL_LINE_STRIP);
            for (int k = 0; k < A::TP_BINS; k++) glVertex2f(in.x + (k + 0.5f) * bw, mapY(in, A::dprof[s][k], 0, dh));
            glEnd(); glDisable(GL_LINE_STIPPLE);
        }
        glDisable(GL_LINE_SMOOTH); glLineWidth(1);
        axisLabels(in, 0, hi, "%.2f");
        float lx = in.x + in.w;
        for (int s = A::dprofN - 1; s >= 0; s--) lx -= drawTextR(fontXS, lx, in.y + in.h - fontXS.h, fmt("ρ %s", EL[A::dprofType[s]].sym), s ? cG : cW) + uiPx(8);
    }
    // 11. журнал реакций: самые частые уравнения, число событий, ΔH по энергиям связей
    {
        PR c = cell(10); c.w = std::floor(w);
        PR in = plotFrame(c, "Реакции (самые частые)", fmt("теплота %+.2f эВ", CH.heat / cfg::EV));
        std::vector<std::pair<long long, const std::string*>> rs;
        for (auto& kv : rxStats) rs.push_back({kv.second.count, &kv.first});
        std::sort(rs.begin(), rs.end(), [](const std::pair<long long, const std::string*>& a, const std::pair<long long, const std::string*>& b) { return a.first > b.first; });
        float lh = fontXS.h + uiPx(1), ly = in.y - uiPx(2); int lines = std::max(1, (int)((in.h + uiPx(4)) / lh));
        if (rs.empty()) drawText(fontXS, in.x + uiPx(4), ly + uiPx(2), anyBondable ? "реакций пока не было" : "в этой сцене нет веществ, способных реагировать", C_DIM);
        pushClip(in.x, in.y - uiPx(4), in.w, in.h + uiPx(8));
        for (int q = 0; q < (int)rs.size() && q < lines; q++) {
            const RxStat& st = rxStats[*rs[q].second];
            float x = in.x + uiPx(4);
            drawText(fontXS, x, ly, fmt("×%lld", rs[q].first), C_ACC); x += uiPx(52);
            float ew = drawEquation(fontXS, x, ly, *rs[q].second, C_TEXT);
            RGBA hc = st.dH < 0 ? C_HOT : C_COLD;   // экзо — ярко, эндо — приглушённо (знак ΔH — в тексте)
            std::string hs = fmt("ΔH %+.2f эВ", st.dH / cfg::EV);
            drawText(fontXS, std::max(x + ew + uiPx(10), in.x + in.w - textW(fontXS, hs) - uiPx(4)), ly, hs, hc);
            ly += lh;
        }
        popClip();
        return c.y + c.h;
    }
}

// ===================================== ИЗМЕНЕНИЕ СИСТЕМЫ ИЗ ИНТЕРФЕЙСА ======================
static const char* HW_NAMES[] = {"нет", "лев./прав.", "горячее дно"};
static double sliderN = 0;
static void userBegin(); static void userEnd();
static void rescaleBox(double s) {   // работа сжатия/расширения — внешняя (W)
    userBegin();
    S.Lx *= s; S.Ly *= s; S.Lz *= s;
    for (int i = 0; i < S.n; i++) { S.x[i] *= s; S.y[i] *= s; S.z[i] *= s; }
    for (auto& o : fieldObjs) { o.x *= s; o.y *= s; o.z *= s; o.x2 *= s; o.y2 *= s; o.z2 *= s; }
    nlValid = false; userEnd();
}
// размер ящика со слайдера меняется плавно (≤ 2 % за кадр), чтобы резкое сжатие не «взорвало» вещество
static double boxTarget = -1;
static void boxTick() {
    if (boxTarget <= 0) return;
    // не сжимать сильнее плотной упаковки: ρ ≤ 1.2 σ⁻³
    double rhoMax = 1.2, Vmin = std::max(1, S.n) / rhoMax, V = boxVolume();
    double sMin = std::cbrt(Vmin / V) * S.Lx;
    if (boxTarget < sMin) { boxTarget = sMin; showToast("Сжатие ограничено плотной упаковкой атомов"); }
    double s = boxTarget / S.Lx;
    if (std::fabs(s - 1) < 1e-4 || !std::isfinite(s)) { boxTarget = -1; return; }
    rescaleBox(clampv(s, 0.98, 1.02));
}
static void strainX(double s) {   // одноосная деформация: растяжение/сжатие ящика и атомов по x
    S.Lx *= s; for (int i = 0; i < S.n; i++) S.x[i] *= s;
    for (auto& o : fieldObjs) { o.x *= s; o.x2 *= s; }
    nlValid = false; computeForces(); resetEnergyRef();
}
// текущее вещество палитры как тип одного атома (для смены элемента и источника)
static int currentElemType() {
    const Tmpl& m = palette[clampv(selPal, 0, (int)palette.size() - 1)];
    if (!m.tool && m.a.size() == 1) return m.a[0].t;
    return customType >= 0 ? customType : E_AR;
}
static void adjustCount(int target) {
    pushUndo();
    const Tmpl& m = palette[selPal].tool ? palette[findPal("Ar")] : palette[selPal];
    if (target > S.n) {
        int per = (int)m.a.size(); int k = (target - S.n + per - 1) / per;
        fillBox(m, k, P.Tset, 0.5);
    } else {
        int guard = 0;
        while (S.n > target && guard++ < 100000) { int i = (int)(urand() * S.n); if (!EL[S.ty[i]].fixed) removeMolecule(i); }
    }
    updatePresence(); computeForces(); resetEnergyRef();
}
// ---- Внешняя работа пользователя: всё, что меняет энергию системы (перемещение, разрыв связей, вставка,
//  удаление, смена элемента, изменение объектов поля), записывается в W — баланс E − W («дрейф») остаётся честным.
static double userE0 = 0; static bool userOpen = false;
static void userBegin() { computeForces(); measure(); userE0 = EN.total(); userOpen = true; }
static void userEnd() { computeForces(); measure(); if (userOpen) Wext += EN.total() - userE0; userOpen = false; }
static inline double kinOf(int i) { double m = EL[S.ty[i]].m; return 0.5 * m * (S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]); }
// объекты поля: изменения, влияющие на энергию (консервативные объекты), тоже идут в W
static void foEditBegin() { userBegin(); }
static void foEditEnd() { userEnd(); }
static inline void wrapPos(double& x, double& y, double& z) {
    if (isPer()) { x -= S.Lx * std::floor(x / S.Lx); y -= S.Ly * std::floor(y / S.Ly); z -= S.Lz * std::floor(z / S.Lz); }
    else { x = clampv(x, 0.3, S.Lx - 0.3); y = clampv(y, 0.3, S.Ly - 0.3); z = clampv(z, 0.3, S.Lz - 0.3); }
}

// ---- выделение атомов
static int selSeenN = -1;
static void selRebuildMask() { selMask.assign(S.n, 0); for (int i : selList) if (i >= 0 && i < S.n) selMask[i] = 1; selSeenN = S.n; }
static void selClear() { selList.clear(); selRebuildMask(); }
static void selSetList(std::vector<int> v) { std::sort(v.begin(), v.end()); v.erase(std::unique(v.begin(), v.end()), v.end()); selList = v; selRebuildMask(); }
// атомы могли удалиться (индексы сдвигаются: последний встаёт на место удалённого) — оставляем существующие
static void selValidate() {
    if (selSeenN == S.n && selMask.size() == (size_t)S.n) return;
    std::vector<int> v; for (int i : selList) if (i >= 0 && i < S.n) v.push_back(i);
    selList.swap(v); selRebuildMask();
    for (int k = 0; k < measN; k++) if (measIdx[k] >= S.n) { measN = 0; break; }
    if (cutHoverA >= S.n || cutHoverB >= S.n) cutHoverA = cutHoverB = -1;
    if (selFieldObj >= (int)fieldObjs.size()) selFieldObj = -1;
}
// центр масс выделения (с учётом периодических образов — относительно первого атома)
static void selCOM(double& cx, double& cy, double& cz) {
    cx = cy = cz = 0; if (selList.empty()) return;
    int a = selList[0]; double M = 0, sx = 0, sy = 0, sz = 0;
    for (int i : selList) { double dx, dy, dz; dvec(a, i, dx, dy, dz); double m = EL[S.ty[i]].m; sx += m * dx; sy += m * dy; sz += m * dz; M += m; }
    cx = S.x[a] + sx / M; cy = S.y[a] + sy / M; cz = S.z[a] + sz / M;
}
// новые положения выделенных атомов не должны сильно перекрываться с остальными атомами (r < 0.7·σ_ij):
// иначе потенциал LJ ~ r⁻¹² даёт «взрыв». Проверка O(M·(N−M)); для очень больших систем пропускается.
static bool selPlacementOK(const std::vector<std::array<double, 3>>& np) {
    if (selList.empty()) return true;
    std::vector<int> others; others.reserve(S.n); for (int j = 0; j < S.n; j++) if (!isSel(j)) others.push_back(j);
    if ((double)others.size() * selList.size() > 6e7) return true;
    const bool per = isPer();
    for (size_t k = 0; k < selList.size(); k++) {
        int i = selList[k]; const double* p = np[k].data(), mg = EL[S.ty[i]].fixed ? 0.2 : 0.6 * EL[S.ty[i]].sig;
        if (!per && (p[0] < mg || p[1] < mg || p[0] > S.Lx - mg || p[1] > S.Ly - mg || p[2] < mg || p[2] > S.Lz - mg)) return false;
        for (int j : others) {
            double dx = S.x[j] - p[0], dy = S.y[j] - p[1], dz = S.z[j] - p[2];
            if (per) { dx -= S.Lx * std::nearbyint(dx / S.Lx); dy -= S.Ly * std::nearbyint(dy / S.Ly); dz -= S.Lz * std::nearbyint(dz / S.Lz); }
            double r2 = dx * dx + dy * dy + dz * dz; if (r2 > 4.0) continue;
            if (r2 < PT[S.ty[i]][S.ty[j]].sig2 * 0.49 && !isGhost(i, j)) return false;
        }
    }
    return true;
}
// сдвиг выделения; false — сдвиг отклонён (атомы влезли бы в соседей или за стенку)
static bool selTranslate(double dx, double dy, double dz) {
    if (selList.empty() || (dx == 0 && dy == 0 && dz == 0)) return true;
    std::vector<std::array<double, 3>> np(selList.size());
    for (size_t k = 0; k < selList.size(); k++) { int i = selList[k]; double x = S.x[i] + dx, y = S.y[i] + dy, z = S.z[i] + dz;
        if (isPer()) wrapPos(x, y, z); np[k] = {x, y, z}; }
    if (!selPlacementOK(np)) return false;
    userBegin();
    for (size_t k = 0; k < selList.size(); k++) { int i = selList[k]; S.x[i] = np[k][0]; S.y[i] = np[k][1]; S.z[i] = np[k][2]; S.ux[i] += dx; S.uy[i] += dy; S.uz[i] += dz; }
    nlValid = false; userEnd();
    return true;
}
// поворот выделения вокруг центра масс и оси взгляда (скорости поворачиваются тоже)
static void selRotate(double ang) {
    if (selList.size() < 2) return;
    double cx, cy, cz; selCOM(cx, cy, cz);
    const double ax[3] = {-camF[0], -camF[1], -camF[2]};
    double c = std::cos(ang), s = std::sin(ang);
    auto rot = [&](double& x, double& y, double& z) {   // формула Родрига
        double d = ax[0] * x + ax[1] * y + ax[2] * z, cr[3] = {ax[1] * z - ax[2] * y, ax[2] * x - ax[0] * z, ax[0] * y - ax[1] * x};
        double nx = x * c + cr[0] * s + ax[0] * d * (1 - c), ny = y * c + cr[1] * s + ax[1] * d * (1 - c), nz = z * c + cr[2] * s + ax[2] * d * (1 - c);
        x = nx; y = ny; z = nz;
    };
    int a = selList[0]; double ox = S.x[a], oy = S.y[a], oz = S.z[a];
    std::vector<std::array<double, 3>> rel(selList.size()), np(selList.size());
    for (size_t k = 0; k < selList.size(); k++) { int i = selList[k]; double dx, dy, dz; dvec(a, i, dx, dy, dz); rel[k] = {ox + dx - cx, oy + dy - cy, oz + dz - cz}; }
    for (size_t k = 0; k < selList.size(); k++) {
        double x = rel[k][0], y = rel[k][1], z = rel[k][2]; rot(x, y, z);
        double nx = cx + x, ny = cy + y, nz = cz + z; if (isPer()) wrapPos(nx, ny, nz); np[k] = {nx, ny, nz};
    }
    if (!selPlacementOK(np)) { showToast("Поворот невозможен: атомы перекрылись бы с соседями"); return; }
    userBegin();
    for (size_t k = 0; k < selList.size(); k++) {
        int i = selList[k]; double x = rel[k][0], y = rel[k][1], z = rel[k][2]; rot(x, y, z);
        S.ux[i] += x - rel[k][0]; S.uy[i] += y - rel[k][1]; S.uz[i] += z - rel[k][2];
        S.x[i] = np[k][0]; S.y[i] = np[k][1]; S.z[i] = np[k][2];
        rot(S.vx[i], S.vy[i], S.vz[i]);
    }
    nlValid = false; userEnd();
}
// скорость центра масс выделения := V (внутреннее движение сохраняется); ΔK — внешняя работа
static void selSetVelocity(double Vx, double Vy, double Vz) {
    if (selList.empty()) return;
    double M = 0, px = 0, py = 0, pz = 0, K0 = 0, K1 = 0;
    for (int i : selList) { if (EL[S.ty[i]].fixed || isPinned(i)) continue; double m = EL[S.ty[i]].m; M += m; px += m * S.vx[i]; py += m * S.vy[i]; pz += m * S.vz[i]; K0 += kinOf(i); }
    if (M <= 0) return;
    double dvx = Vx - px / M, dvy = Vy - py / M, dvz = Vz - pz / M;
    for (int i : selList) { if (EL[S.ty[i]].fixed || isPinned(i)) continue; S.vx[i] += dvx; S.vy[i] += dvy; S.vz[i] += dvz; K1 += kinOf(i); }
    Wext += K1 - K0;
}
// масштаб тепловых скоростей выделения (нагреть/охладить): k² — во сколько раз меняется T
static void selScaleThermal(double k) {
    if (selList.empty()) return;
    double M = 0, px = 0, py = 0, pz = 0, K0 = 0, K1 = 0;
    for (int i : selList) { if (EL[S.ty[i]].fixed || isPinned(i)) continue; double m = EL[S.ty[i]].m; M += m; px += m * S.vx[i]; py += m * S.vy[i]; pz += m * S.vz[i]; K0 += kinOf(i); }
    if (M <= 0) return;
    double vx = px / M, vy = py / M, vz = pz / M;
    for (int i : selList) {
        if (EL[S.ty[i]].fixed || isPinned(i)) continue;
        double wx = S.vx[i] - vx, wy = S.vy[i] - vy, wz = S.vz[i] - vz;
        if (k > 1 && wx * wx + wy * wy + wz * wz < 1e-6) { double s = std::sqrt(0.05 / EL[S.ty[i]].m); wx = grand() * s; wy = grand() * s; wz = grand() * s; }
        S.vx[i] = vx + wx * k; S.vy[i] = vy + wy * k; S.vz[i] = vz + wz * k; K1 += kinOf(i);
    }
    Wext += K1 - K0;
}
static void selStop() { double K0 = 0; for (int i : selList) { K0 += kinOf(i); S.vx[i] = S.vy[i] = S.vz[i] = 0; } Wext -= K0; }
static void selPin() {   // закрепить (скорость → 0, ΔK — в W) или открепить, если всё уже закреплено
    if (selList.empty() || S.pin.size() != (size_t)S.n) return;
    bool all = true; for (int i : selList) if (!S.pin[i]) all = false;
    if (all) { for (int i : selList) S.pin[i] = 0; showToast(fmt("Откреплено атомов: %d", (int)selList.size())); return; }
    double K0 = 0; for (int i : selList) { if (!S.pin[i]) { K0 += kinOf(i); S.vx[i] = S.vy[i] = S.vz[i] = 0; } S.pin[i] = 1; }
    Wext -= K0;
    showToast(fmt("Закреплено атомов: %d (I — открепить)", (int)selList.size()));
}
static void selDelete() {
    if (selList.empty()) return;
    pushUndo(); userBegin();
    std::vector<int> v = selList; std::sort(v.begin(), v.end(), std::greater<int>());
    int n0 = (int)v.size();
    for (int i : v) if (i < S.n) removeAtom(i);
    selClear(); measN = 0; updatePresence(); userEnd();
    showToast(fmt("Удалено атомов: %d", n0));
}
// разорвать все связи атома i; бывшие соседи на малом расстоянии становятся «призраками» (не отталкиваются до разлёта)
static void breakAllBondsGhost(int i) {
    std::vector<int> nbs; for (int k = 0; k < S.nbc[i]; k++) nbs.push_back(S.nb[i][k]);
    removeAllBonds(i);
    for (int j : nbs) { double s = 1.1225 * 0.5 * (EL[S.ty[i]].sig + EL[S.ty[j]].sig); if (dist2(i, j) < s * s) addGhost(i, j); }
}
static void selChangeElement(int t) {
    if (selList.empty() || t < 0 || t >= NEL) return;
    // атом, который с новым размером влез бы в соседа (не связанного с ним), не меняется — иначе «взрыв»
    std::vector<int> ok; int skipped = 0; const bool per = isPer();
    for (int i : selList) {
        bool bad = false;
        if (!per) {   // стенки (потенциал 9-3 зависит от размера атома)
            double mg = 0.75 * EL[t].sig;
            if (S.x[i] < mg || S.y[i] < mg || S.x[i] > S.Lx - mg || S.y[i] > S.Ly - mg || S.z[i] < mg || S.z[i] > S.Lz - mg) bad = true;
        }
        for (int j = 0; j < S.n && !bad; j++) {
            if (j == i || bonded(i, j) || isGhost(i, j)) continue;
            double dx, dy, dz; dx = S.x[j] - S.x[i]; dy = S.y[j] - S.y[i]; dz = S.z[j] - S.z[i];
            if (per) { dx -= S.Lx * std::nearbyint(dx / S.Lx); dy -= S.Ly * std::nearbyint(dy / S.Ly); dz -= S.Lz * std::nearbyint(dz / S.Lz); }
            double r2 = dx * dx + dy * dy + dz * dz; if (r2 > 9.0) continue;
            int tj = isSel(j) ? t : S.ty[j];
            if (r2 < PT[t][tj].sig2 * 0.49) bad = true;
        }
        if (bad) skipped++; else ok.push_back(i);
    }
    if (ok.empty()) { showToast("Смена элемента невозможна: новые атомы не помещаются среди соседей"); return; }
    // допустимый прирост энергии: энергия разрываемых связей + 25ε на атом (перестройка окружения)
    double allowE = 50.0 + 25.0 * ok.size();
    for (int i : ok) for (int k = 0; k < S.nbc[i]; k++) { int j = S.nb[i][k]; allowE += BT[S.ty[i]][S.ty[j]].D[std::min<int>(S.bo[i][k], 3)]; }
    pushUndo(); userBegin();
    for (int i : ok) { S.ty[i] = t; S.q[i] = EL[t].fq; if (EL[t].fixed) S.vx[i] = S.vy[i] = S.vz[i] = 0; }
    for (int i : ok) { if (S.nbc[i]) breakAllBondsGhost(i); }   // бывшие соседи по связи — «призраки» (с новым размером)
    for (int i : selList) updateCharge(i);
    updatePresence(); nlValid = false;
    // проверка по факту: если новые атомы всё же сильно перекрылись (плотное окружение, связанные соседи) — отмена
    computeForces(); measure();
    if (const double dE = EN.total() - userE0; dE > allowE) {
        userOpen = false; popUndo();
        showToast(fmt("Смена элемента отменена: %s не помещается среди соседей (скачок энергии %.0f ε)", EL[t].sym, dE));
        return;
    }
    userEnd();
    showToast(skipped ? fmt("Элемент %s: заменено %d, пропущено %d (не помещаются)", EL[t].sym, (int)ok.size(), skipped)
                      : fmt("Элемент выделенных атомов: %s — %s", EL[t].sym, T(EL[t].name)));
}
// буфер обмена атомов: относительные координаты, скорости, связи, закрепление
struct ClipAtom { int t; double x, y, z, vx, vy, vz; unsigned char pin; };
static std::vector<ClipAtom> clipAtoms; static std::vector<std::array<int, 3>> clipBonds;
static void selCopy() {
    if (selList.empty()) return;
    double cx, cy, cz; selCOM(cx, cy, cz);
    clipAtoms.clear(); clipBonds.clear();
    std::map<int, int> loc; int a = selList[0];
    for (int i : selList) {
        double dx, dy, dz; dvec(a, i, dx, dy, dz);
        loc[i] = (int)clipAtoms.size();
        clipAtoms.push_back({S.ty[i], S.x[a] + dx - cx, S.y[a] + dy - cy, S.z[a] + dz - cz, S.vx[i], S.vy[i], S.vz[i], (unsigned char)(isPinned(i) ? 1 : 0)});
    }
    for (int i : selList) for (int k = 0; k < S.nbc[i]; k++) { int j = S.nb[i][k]; if (j > i && loc.count(j)) clipBonds.push_back({loc[i], loc[j], (int)S.bo[i][k]}); }
    showToast(fmt("Скопировано атомов: %d, связей: %d (Ctrl+V — вставить у курсора)", (int)clipAtoms.size(), (int)clipBonds.size()));
}
static bool pasteAt(double px, double py, double pz) {
    if (clipAtoms.empty()) { showToast("Буфер пуст: выделите атомы и нажмите Ctrl+C"); return false; }
    std::vector<std::array<double, 3>> pos;
    for (auto& c : clipAtoms) {
        double x = px + c.x, y = py + c.y, z = pz + c.z;
        const double mg = EL[c.t].fixed ? 0.3 : 0.6 * EL[c.t].sig;
        if (!isPer() && (x < mg || y < mg || x > S.Lx - mg || y > S.Ly - mg || z < mg || z > S.Lz - mg)) { showToast("Вставка не помещается в ящик — сдвиньте курсор"); return false; }
        if (isPer()) wrapPos(x, y, z);
        if (overlaps(c.t, x, y, z, 0.7)) { showToast("Место занято — вставка отменена (сдвиньте курсор)"); return false; }
        pos.push_back({x, y, z});
    }
    pushUndo(); userBegin();
    int base = S.n; std::vector<int> added;
    for (size_t k = 0; k < clipAtoms.size(); k++) { const ClipAtom& c = clipAtoms[k]; int i = addAtom(c.t, pos[k][0], pos[k][1], pos[k][2], c.vx, c.vy, c.vz); added.push_back(i);
        if ((size_t)i < S.pin.size()) S.pin[i] = c.pin; }
    for (auto& b : clipBonds) for (int o = 0; o < b[2]; o++) changeBond(base + b[0], base + b[1], +1);
    for (int i : added) updateCharge(i);
    updatePresence(); userEnd();
    selSetList(added);
    showToast(fmt("Вставлено атомов: %d", (int)added.size()));
    return true;
}
// ---- «ножницы»: разрыв связи i–j. Бывшие исключённые пары на малом расстоянии — «призраки»; лёгкий разлёт пары,
//  чтобы атомы сразу не рекомбинировали. Энергия разрыва и толчка учитывается в W вызывающим (userBegin/End).
static bool cutBond(int i, int j) {
    if (i < 0 || j < 0 || i >= S.n || j >= S.n || !bonded(i, j)) return false;
    std::vector<std::pair<int, int>> exb;
    for (int a : {i, j}) for (int p = nlStart[a]; p < nlStart[a + 1]; p++) { int b = nlIdx[p]; if (mayExclude(a, b) && excluded(a, b)) exb.push_back({a, b}); }
    exb.push_back({i, j});
    while (bonded(i, j)) changeBond(i, j, -1);
    updateCharge(i); updateCharge(j);
    for (auto& pr : exb) if (!excluded(pr.first, pr.second)) {
        double s = 1.1225 * 0.5 * (EL[S.ty[pr.first]].sig + EL[S.ty[pr.second]].sig);
        if (dist2(pr.first, pr.second) < s * s) addGhost(pr.first, pr.second);
    }
    double dx, dy, dz; dvec(i, j, dx, dy, dz); double r = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (r > 1e-9 && !isPinned(i) && !isPinned(j) && !EL[S.ty[i]].fixed && !EL[S.ty[j]].fixed) {
        double mi = EL[S.ty[i]].m, mj = EL[S.ty[j]].m, mu = mi * mj / (mi + mj), nx = dx / r, ny = dy / r, nz = dz / r;
        double vn = (S.vx[j] - S.vx[i]) * nx + (S.vy[j] - S.vy[i]) * ny + (S.vz[j] - S.vz[i]) * nz, vt = std::sqrt(2 * 1.0 / mu);   // разлёт с энергией ≈ 1 ε
        if (vn < vt) { double dv = vt - vn;
            S.vx[i] -= mu / mi * dv * nx; S.vy[i] -= mu / mi * dv * ny; S.vz[i] -= mu / mi * dv * nz;
            S.vx[j] += mu / mj * dv * nx; S.vy[j] += mu / mj * dv * ny; S.vz[j] += mu / mj * dv * nz; }
    }
    double mx, my, mz; midpoint(i, j, mx, my, mz); addFlash(mx, my, mz, 3.0);
    return true;
}
// пересечение отрезков на экране
static bool segCross(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
    auto cr = [](float ux, float uy, float vx, float vy) { return ux * vy - uy * vx; };
    float d1 = cr(bx - ax, by - ay, cx - ax, cy - ay), d2 = cr(bx - ax, by - ay, dx - ax, dy - ay), d3 = cr(dx - cx, dy - cy, ax - cx, ay - cy), d4 = cr(dx - cx, dy - cy, bx - cx, by - cy);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}
static float segDistPx(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy, t = l2 > 0 ? clampv(((px - ax) * vx + (py - ay) * vy) / l2, 0.0f, 1.0f) : 0;
    float dx = ax + t * vx - px, dy = ay + t * vy - py; return std::sqrt(dx * dx + dy * dy);
}
// экранные концы связи i–j (с учётом минимального образа); false — связь не видна или идёт через границу
static bool bondScreen(int i, int j, float& ax, float& ay, float& bx, float& by) {
    if (i >= (int)pvis.size() || j >= (int)pvis.size() || !pvis[i] || !pvis[j]) return false;
    double dx, dy, dz; dvec(i, j, dx, dy, dz); float dd, s;
    if (!project(S.x[i] + dx, S.y[i] + dy, S.z[i] + dz, bx, by, dd, s)) return false;
    if (std::fabs(bx - psx[j]) > 2 || std::fabs(by - psy[j]) > 2) return false;
    ax = psx[i]; ay = psy[i]; return true;
}
static void findCutHover(float mx, float my) {
    cutHoverA = cutHoverB = -1; float best = uiPx(7);
    for (int i = 0; i < S.n; i++) for (int k = 0; k < S.nbc[i]; k++) {
        int j = S.nb[i][k]; if (j < i) continue; float ax, ay, bx, by;
        if (!bondScreen(i, j, ax, ay, bx, by)) continue;
        float d = segDistPx(mx, my, ax, ay, bx, by); if (d < best) { best = d; cutHoverA = i; cutHoverB = j; }
    }
}
// разрезать все связи, которые пересекает путь курсора (x0,y0)→(x1,y1); возвращает число разрывов
static int cutAlong(float x0, float y0, float x1, float y1) {
    std::vector<std::pair<int, int>> todo;
    for (int i = 0; i < S.n; i++) for (int k = 0; k < S.nbc[i]; k++) {
        int j = S.nb[i][k]; if (j < i) continue; float ax, ay, bx, by;
        if (!bondScreen(i, j, ax, ay, bx, by)) continue;
        if (segCross(x0, y0, x1, y1, ax, ay, bx, by) || segDistPx(x1, y1, ax, ay, bx, by) < uiPx(3)) todo.push_back({i, j});
    }
    if (todo.empty()) return 0;
    userBegin();
    int c = 0; for (auto& p : todo) if (cutBond(p.first, p.second)) c++;
    nlValid = false; userEnd();
    return c;
}
// ---- толчок / притяжение: радиальный импульс от оси луча под курсором (ΔK — в W)
static void pushBrush(const double* o, const double* d, double sign, double dt, double cutDepth) {
    const double R = P.brushR, R2 = R * R, a = 5.0 * toolPower;
    double dK = 0;
    for (int i = 0; i < S.n; i++) {
        if (EL[S.ty[i]].fixed || isPinned(i)) continue;
        double r2 = rayDist2(S.x[i], S.y[i], S.z[i], o, d); if (r2 > R2) continue;
        if (viewDepth(S.x[i], S.y[i], S.z[i]) < cutDepth) continue;
        double wx = S.x[i] - o[0], wy = S.y[i] - o[1], wz = S.z[i] - o[2], t = wx * d[0] + wy * d[1] + wz * d[2];
        double px = wx - t * d[0], py = wy - t * d[1], pz = wz - t * d[2], r = std::sqrt(r2);
        if (r < 1e-6) continue;
        double k = sign * a * dt * (1 - r / R) / r, K0 = kinOf(i);
        S.vx[i] += k * px; S.vy[i] += k * py; S.vz[i] += k * pz; dK += kinOf(i) - K0;
    }
    Wext += dK;
}
// ---- ударная волна: радиальный импульс из точки c в радиусе 2.5·R (ΔK — в W)
struct RingFx { double x, y, z, R; float age; };
static std::vector<RingFx> ringsFx;
static void shockWave(double cx, double cy, double cz) {
    const double Rs = 2.5 * P.brushR, A = 3.0 * toolPower;
    double dK = 0; int hit = 0;
    for (int i = 0; i < S.n; i++) {
        if (EL[S.ty[i]].fixed || isPinned(i)) continue;
        double dx = S.x[i] - cx, dy = S.y[i] - cy, dz = S.z[i] - cz;
        if (isPer()) { dx -= S.Lx * std::nearbyint(dx / S.Lx); dy -= S.Ly * std::nearbyint(dy / S.Ly); dz -= S.Lz * std::nearbyint(dz / S.Lz); }
        double r = std::sqrt(dx * dx + dy * dy + dz * dz); if (r > Rs || r < 1e-6) continue;
        double k = A * (1 - r / Rs) / r, K0 = kinOf(i);
        S.vx[i] += k * dx; S.vy[i] += k * dy; S.vz[i] += k * dz; dK += kinOf(i) - K0; hit++;
    }
    Wext += dK;
    ringsFx.push_back({cx, cy, cz, Rs, 0});
    if (ringsFx.size() > 16) ringsFx.erase(ringsFx.begin());
}
static void drawRingsFx() {
    glEnable(GL_LINE_SMOOTH);
    for (auto& r : ringsFx) {
        float sx, sy, dd, s; if (!project(r.x, r.y, r.z, sx, sy, dd, s)) continue;
        float t = r.age / 0.5f; col(withA(C_TEXT_HI, 0.6f * (1 - t))); circlePx(sx, sy, (float)(r.R * s * (0.25f + 0.75f * t)), 64);
    }
    glDisable(GL_LINE_SMOOTH);
}
// ---- объекты поля
static int foHitPart = 0;   // 0 — центр/тело, 1 — первый конец барьера, 2 — второй
static int foHitTest(float mx, float my, int& part) {
    part = 0; if (!layerFO) return -1;
    int best = -1; float bd = uiPx(12);
    for (int k = (int)fieldObjs.size() - 1; k >= 0; k--) {
        const FieldObj& o = fieldObjs[k]; float cx, cy, dd, sc;
        if (o.kind == FO_BARRIER) {
            float ax, ay, bx, by;
            if (!project(o.x, o.y, o.z, ax, ay, dd, sc) || !project(o.x2, o.y2, o.z2, bx, by, dd, sc)) continue;
            float da = std::hypot(mx - ax, my - ay), db = std::hypot(mx - bx, my - by), ds = segDistPx(mx, my, ax, ay, bx, by);
            if (da < bd) { bd = da; best = k; part = 1; } else if (db < bd) { bd = db; best = k; part = 2; } else if (ds < bd * 0.6f) { bd = ds / 0.6f; best = k; part = 0; }
            continue;
        }
        if (!project(o.x, o.y, o.z, cx, cy, dd, sc)) continue;
        float d = std::hypot(mx - cx, my - cy);
        if (d < bd) { bd = d; best = k; part = 0; }
    }
    return best;
}
static void foDelete(int k) {
    if (k < 0 || k >= (int)fieldObjs.size()) return;
    foEditBegin(); fieldObjs.erase(fieldObjs.begin() + k); foEditEnd();
    if (selFieldObj == k) selFieldObj = -1; else if (selFieldObj > k) selFieldObj--;
    foHover = -1; showToast("Объект поля удалён");
}
static void foSetDirFromDrag(FieldObj& o, const double* a, const double* b) {
    double d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (l > 1e-6) { o.dx = d[0] / l; o.dy = d[1] / l; o.dz = d[2] / l; }
}
// новый объект в точке p (для ветра/источника — направление p→q, для барьера — отрезок p–q)
static int foCreate(int kind, const double* p, const double* q, bool dragged) {
    FieldObj o = makeFieldObj(kind, p[0], p[1], p[2]);
    if (kind == FO_EMITTER) { o.elem = currentElemType(); if (EL[o.elem].fixed) o.elem = E_AR; }
    if (kind == FO_VORTEX) { o.dx = -camF[0]; o.dy = -camF[1]; o.dz = -camF[2]; }   // ось — к зрителю
    if (dragged && (kind == FO_WIND || kind == FO_EMITTER)) foSetDirFromDrag(o, p, q);
    if (kind == FO_BARRIER) {
        if (dragged) { o.x = p[0]; o.y = p[1]; o.z = p[2]; o.x2 = q[0]; o.y2 = q[1]; o.z2 = q[2]; }
        {   // пластина содержит отрезок и направление взгляда: пользователь видит её «с ребра», как линию
            double u[3] = {o.x2 - o.x, o.y2 - o.y, o.z2 - o.z}, n[3] = {u[1] * camF[2] - u[2] * camF[1], u[2] * camF[0] - u[0] * camF[2], u[0] * camF[1] - u[1] * camF[0]};
            double nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (nl > 1e-6) { o.dx = n[0] / nl; o.dy = n[1] / nl; o.dz = n[2] / nl; }
            o.R = 0.5 * std::max({S.Lx, S.Ly, S.Lz});
        }
    }
    foEditBegin(); fieldObjs.push_back(o); foEditEnd();
    selFieldObj = (int)fieldObjs.size() - 1;
    showToast(fmt("Объект поля: %s. Колесо — радиус, Shift+колесо — сила, N — вкл/выкл, Del — удалить", T(FO_NAMES[kind])));
    return selFieldObj;
}
// колесо над объектом: радиус (или сила с Shift)
static void foWheel(int k, double notch, bool shift) {
    if (k < 0 || k >= (int)fieldObjs.size()) return;
    FieldObj& o = fieldObjs[k];
    foEditBegin();
    if (shift) { double lo, hi; foStrengthRange(o.kind, lo, hi); double s = std::fabs(o.strength) < 1e-9 ? lo : o.strength;
                 double sg = s < 0 ? -1 : 1; o.strength = sg * clampv(std::fabs(s) * std::pow(1.15, notch), lo, hi); }
    else o.R = clampv(o.R * std::pow(1.1, notch), 0.3, std::max({S.Lx, S.Ly, S.Lz}));
    foEditEnd();
    selFieldObj = k;
}

// ===================================== ИНСТРУМЕНТЫ: названия и подсказки ====================
static const char* TOOL_NAMES[TOOL_N] = {"Добавить / пинцет", "Ластик", "Нагрев", "Охлаждение", "Камера", "Выделение", "Толчок", "Ударная волна", "Ножницы",
                                         "Линейка и угломер", "Объекты поля"};
static const int TOOL_ICON[TOOL_N] = {IC_ADD, IC_ERASE, IC_HEAT, IC_COOL, IC_CAMERA, IC_SELECT, IC_PUSH, IC_SHOCK, IC_CUT, IC_MEASURE, IC_FIELD};
static const char* TOOL_KEY[TOOL_N] = {"Alt+4", "Alt+5", "Alt+6", "Alt+7", "Alt+1", "Alt+2", "Alt+8", "Alt+9", "Alt+0", "Alt+3", "Alt+F"};
// порядок на панели инструментов (−1 — разделитель); Alt+1…Alt+0 — в этом порядке
static const int TOOL_ORDER[] = {TOOL_CAMERA, TOOL_SELECT, TOOL_MEASURE, -1, TOOL_ADD, TOOL_ERASE, -1, TOOL_HEAT, TOOL_COOL, TOOL_PUSH, TOOL_SHOCK, TOOL_CUT, -1, TOOL_FIELD};
static const int TOOL_BY_DIGIT[10] = {TOOL_CUT, TOOL_CAMERA, TOOL_SELECT, TOOL_MEASURE, TOOL_ADD, TOOL_ERASE, TOOL_HEAT, TOOL_COOL, TOOL_PUSH, TOOL_SHOCK};   // Alt+0…Alt+9
static const char* TOOL_HELP[TOOL_N] = {
    "ЛКМ по пустому месту — добавить выбранное вещество (кисть R);\nЛКМ по атому — «пинцет»: атом тянется пружиной за курсором",
    "ЛКМ стирает молекулы под кистью (то же — средняя кнопка мыши)",
    "ЛКМ нагревает атомы под кистью (то же — ПКМ).\nПереданная энергия учитывается как внешняя работа W",
    "ЛКМ охлаждает атомы под кистью (то же — Shift+ПКМ)",
    "ЛКМ — вращение камеры, Shift+ЛКМ — сдвиг.\nВ любом инструменте: Ctrl+ЛКМ — камера, колесо — масштаб",
    "Рамка — выделить (Shift — добавить), щелчок по атому — вся молекула.\nТащить выделенное — сдвинуть, Alt+тащить — бросок (задать скорость),\nAlt+колесо — поворот. Del — удалить, Ctrl+C/Ctrl+V — копия, I — закрепить",
    "ЛКМ — отталкивать атомы от курсора, Shift+ЛКМ — притягивать.\nРадиус — кисть R, сила — «сила инструментов». Работа идёт в W",
    "Щелчок — ударная волна: радиальный импульс атомам в радиусе 2.5·R.\nСила — «сила инструментов». Энергия удара — внешняя работа W",
    "Проведите курсором поперёк связей — они рвутся.\nЭнергия разрыва учитывается как внешняя работа W",
    "Щелчки по атомам: 2 — расстояние, 3 — угол, 4 — двугранный угол.\nЩелчок по пустому месту — сброс. Результаты — во вкладке «Объект»",
    "ЛКМ — поставить объект выбранного вида (ветер/источник — тянуть по направлению,\nбарьер — от точки к точке); тащить объект — двигать.\nКолесо над объектом — радиус, Shift+колесо — сила; N — вкл/выкл; Del или ПКМ — удалить"};
static const char* TOOL_STATUS[TOOL_N] = {
    "ЛКМ — добавить вещество · по атому — пинцет · двойной щелчок — следить",
    "ЛКМ — стереть молекулы под кистью",
    "ЛКМ — нагревать под кистью (ПКМ — то же)",
    "ЛКМ — охлаждать под кистью (Shift+ПКМ — то же)",
    "ЛКМ — вращать · Shift — сдвиг · колесо — масштаб",
    "рамка — выделить (Shift +) · тащить — сдвиг · Alt+тащить — бросок · Alt+колесо — поворот · Del · Ctrl+C/V · I — закрепить",
    "ЛКМ — оттолкнуть · Shift+ЛКМ — притянуть",
    "щелчок — ударная волна (радиус 2.5·R)",
    "тащить поперёк связей — разрезать",
    "щелчки по атомам: расстояние → угол → двугранный · по пустому — сброс",
    "ЛКМ — поставить/двигать · колесо — R · Shift+колесо — сила · N — вкл/выкл · Del/ПКМ — удалить"};
static void setTool(int t) {
    if (world == W_WAVE) { lmbTool = TOOL_CAMERA; showToast("Здесь щелчок по волне — измерение: электрон найдётся в этом месте или волна там исчезнет"); return; }
    if (world == W_SEMI) { lmbTool = TOOL_CAMERA; showToast("Прибором управляют ползунки напряжений справа"); return; }
    if (world != W_MD) { lmbTool = TOOL_CAMERA; showToast("Здесь мышь управляет только камерой: ЛКМ — вращать, Shift+ЛКМ — сдвиг, колесо — масштаб"); return; }
    t = clampv(t, 0, TOOL_N - 1); lmbTool = t;
    showToast(std::string(T("Инструмент: ")) + T(TOOL_NAMES[t]));
}

// ===================================== ВЕРХНЯЯ ПАНЕЛЬ =======================================
static double fps = 60;
static void advanceFlashes(float dt);   // app.inl (вспышки реакций и кольца ударной волны)
static void saveScreenshot(bool sceneOnly); static void toggleRecording(); static bool recording = false; static int recFrames = 0;
static void cmdSave(); static void cmdLoad(); static void cmdReset(); static void cmdUndo();
static void setLanguage(int l);   // app.inl: язык интерфейса на лету (RU | EN, Ctrl+L)
static void drawTopBar() {
    const float H = sceneY;
    rectFill(0, 0, (float)winW, H, C_PANEL); lineH(0, (float)winW, H - 1, C_LINE);
    float x = uiPx(12), bh = H - uiPx(10), by = uiPx(5);
    x += drawText(fontUB, x, (H - fontUB.h) / 2 - 1, "АТОМЫ", C_TEXT_HI) + uiPx(8);
    x += drawText(fontXS, x, (H - fontXS.h) / 2, WORLD_NAMES[world], C_DIM) + uiPx(14);
    auto sep = [&]() { lineV(x, uiPx(8), H - uiPx(8), C_LINE); x += uiPx(9); };
    sep();
    auto textBtn = [&](int id, const char* label, bool on, const char* hint) { float w = textW(fontU, label) + uiPx(20); bool c = uiButton(id, x, by, w, bh, label, on, false, hint); x += w + uiPx(4); return c; };
    if (textBtn(220, "Сцены", menuOn, "Сцены (Tab)\nВсе пресеты с описаниями; повторный выбор — следующий вариант")) { menuOn = !menuOn; ptOn = settingsOn = false; }
    if (textBtn(221, "Таблица", ptOn, "Таблица Менделеева (E)\nВсе 118 элементов: выбрать элемент для добавления, источника, смены элемента")) { ptOn = !ptOn; menuOn = settingsOn = false; }
    x += uiPx(5); sep();
    auto ic = [&](int id, int icon, bool on, const char* hint, RGBA tint = C_TEXT) { bool c = uiIconBtn(id, x, by, bh, bh, icon, on, hint, tint); x += bh + uiPx(2); return c; };
    if (ic(209, P.paused ? IC_PLAY : IC_PAUSE, false, P.paused ? "Продолжить (Пробел)" : "Пауза (Пробел)")) P.paused = !P.paused;
    if (ic(763, IC_STEP, false, "Один шаг (S)\nСтавит на паузу и делает один шаг интегрирования")) { P.paused = true; if (world != W_MD) worldStep(1.0 / 30); else { mdStep(); analysisTick(); } }
    if (ic(764, IC_RESET, false, "Сброс сцены (R)\nЗаново загрузить текущий пресет (объекты поля, поставленные вами, сохраняются)")) cmdReset();
    if (ic(765, IC_UNDO, false, "Отменить (Ctrl+Z)\nВернуть состояние до последнего действия")) cmdUndo();
    x += uiPx(4); sep();
    if (ic(766, IC_OPEN, false, "Открыть состояние (Ctrl+O)\nF9 — быстрая загрузка")) cmdLoad();
    if (ic(767, IC_SAVE, false, "Сохранить состояние (Ctrl+S)\nF5 — быстрое сохранение")) cmdSave();
    if (ic(768, IC_SHOT, false, "Снимок сцены в PNG (F12)\nShift+F12 — всё окно. Куда сохранять — в настройках (F8)")) saveScreenshot(!isDown(VK_SHIFT));
    if (ic(769, IC_REC, recording, recording ? "Остановить запись кадров (Ctrl+F12)" : "Запись серии кадров сцены в PNG (Ctrl+F12)\nПапка «кадры_…»; частота кадров — в настройках (F8)",
           recording ? C_WARN : C_TEXT)) toggleRecording();
    x += uiPx(4); sep();
    if (ic(771, IC_PANEL, graphsOn, "Боковая панель (G)\nСкрыть/показать панель вкладок — сцена на весь экран")) { graphsOn = !graphsOn; viewFitPending = true; }
    {
        float w = textW(fontU, "Справка") + uiPx(20);
        if (uiButton(772, x, by, w, bh, "Справка", helpOn, false, "Клавиши и мышь (H)\nF1 — подробная инструкция в браузере")) helpOn = !helpOn;
        x += w + uiPx(4);
    }
    if (ic(775, IC_GEAR, settingsOn, "Настройки (F8)\nИнтерфейс, графика, камера, расчёт, файлы")) { settingsOn = !settingsOn; menuOn = ptOn = false; }
    x += uiPx(6);
    sep();
    {   // RU | EN — язык интерфейса, сегментный переключатель (Ctrl+L)
        float w = uiPx(34);
        bool cr = uiButton(773, x, by, w, bh, "RU", LANG == LANG_RU, false, "Русский язык интерфейса (Ctrl+L)\nВыбор запоминается в atoms.ini рядом с atoms.exe"); x += w;
        bool ce = uiButton(774, x, by, w, bh, "EN", LANG == LANG_EN, false, "Английский язык интерфейса — English (Ctrl+L)\nВыбор запоминается в atoms.ini рядом с atoms.exe"); x += w + uiPx(8);
        if (cr) setLanguage(LANG_RU); else if (ce) setLanguage(LANG_EN);
    }
    // справа: время модели и частота кадров
    if (opt.clock) {
        std::string s = world != W_MD ? fmt("t %s   %.0f к/с", worldClock().c_str(), fps) : fmt("t %.2f пс   шаг %lld   %.0f к/с", realPs(S.t), S.step, fps);
        float w = textW(fontS, s);
        if (winW - uiPx(12) - w > x + uiPx(10)) drawText(fontS, winW - uiPx(12) - w, (H - fontS.h) / 2, s, C_DIM);
    }
}

// ===================================== ПОКАЗАНИЯ (верх боковой панели) =====================
static float drawReadouts(float x, float y, float w) {
    measure();
    const float pad = uiPx(10), rh = fontS.h + uiPx(3);
    double V = boxVolume(), mass = 0; for (int i = 0; i < S.n; i++) if (!EL[S.ty[i]].fixed) mass += EL[S.ty[i]].m;
    double rho = EN.nmob / V, rhoM = massDensity(mass, V);
    double drift = 0; bool dv = energyRefValid && conserving;
    if (dv) { double norm = std::max(std::fabs(Eref), std::max(1.0, EN.ek)); drift = (EN.total() - Wext - Eref) / norm * 100; }
    float yy = y + uiPx(8), colW = (w - 2 * pad) / 2;
    const float kw = uiPx(30), vw = uiPx(64);
    // ячейка: ключ (подпись), значение (моноширинно, по правому краю столбца), единица/пересчёт
    // st: VS_WARN — светлая плашка под значением, VS_ERR — инверсия (белая плашка, чёрные цифры)
    auto cell = [&](int c, const std::string& k, const std::string& v, const std::string& u, RGBA vc = C_TEXT_HI, int st = VS_OK) {
        float cx = x + pad + c * colW;
        drawText(fontXS, cx, yy + uiPx(1), k, C_DIM);
        if (st != VS_OK) { float tw = textW(fontS, v); statePlate(cx + kw + vw - tw - uiPx(4), yy, tw + uiPx(6), rh - uiPx(1), st); }
        drawTextR(fontS, cx + kw + vw, yy, v, stateInk(st, vc));
        pushClip(cx + kw + vw, yy, colW - kw - vw - uiPx(8), rh); drawText(fontXS, cx + kw + vw + uiPx(5), yy + uiPx(1), u, C_DIM); popClip();
    };
    cell(0, "T", fmt("%.3f", EN.T), fmt("%.1f K", realK(EN.T)), C_WARN);
    cell(1, "P", fmt(EN.P < 0.1 ? "%.4f" : "%.3f", EN.P), fmt("%.1f бар", realBar(EN.P)));
    yy += rh;
    cell(0, "ρ", fmt("%.3f", rho), "σ⁻³");
    cell(1, "ρ", fmt("%.3f", rhoM), "г/см³");
    yy += rh;
    cell(0, "N", fmt("%d", EN.nmob), S.n != EN.nmob ? fmt("+%d неподв.", S.n - EN.nmob) : std::string("атомов"));
    cell(1, "дрейф", dv ? fmt("%+.3f", drift) : std::string(P.npt ? "NPT" : "—"), dv ? "% от E" : "", C_TEXT_HI, dv && std::fabs(drift) > 1 ? VS_ERR : (dv && std::fabs(drift) > 0.2 ? VS_WARN : VS_OK));
    yy += rh;
    cell(0, "E", fmt("%.1f", EN.total()), "ε"); cell(1, "W", fmt("%+.1f", Wext), "ε внешн.");
    yy += rh;
    cell(0, "Eк", fmt("%.1f", EN.ek), "ε"); cell(1, "Eп", fmt("%.1f", EN.enb + EN.egrav), "ε");
    yy += rh;
    cell(0, "Eсв", fmt("%.1f", EN.ebond), "ε");
    cell(1, "dt", fmt("%.4f", P.dt), fmt("τ · %.1f фс", P.dt * cfg::U_T_PS * 1000), C_TEXT_HI, P.dt < P.dtBase * 0.99 ? VS_WARN : VS_OK);
    yy += rh;
    {   // фаза: текст + полоса долей газ/жидкость/твёрдое
        float cx = x + pad, lw = std::max(kw, drawText(fontXS, cx, yy + uiPx(1), "фаза", C_DIM) + uiPx(6));   // «phase» шире, чем «фаза»
        float tw = drawText(fontU, cx + lw, yy - uiPx(1), A::phase, C_TEXT_HI);
        float bx = cx + lw + tw + uiPx(10), bw = x + w - pad - bx, bh = uiPx(6), by = yy + rh / 2 - bh / 2;
        double fs = phaseFrac[0] + phaseFrac[1] + phaseFrac[2];
        if (bw > uiPx(40) && fs > 0.01) {
            float xx = bx;
            MonoAtoms atomsColored;   // доли фаз — цветами режима раскраски «фаза»
            for (int k = 0; k < 3; k++) { float ww = (float)(bw * phaseFrac[k] / fs); rectFill(xx, by, ww, bh, withA(PHASE_C[k], 0.85f)); xx += ww; }
            rectLine(bx, by - 1, bw, bh + 2, C_LINE);
            if (!uiModal && ui.mx >= bx && ui.mx < bx + bw && ui.my >= by - 4 && ui.my < by + bh + 4) {
                std::string hs = fmt("Доли фаз (подвижные атомы без H)\nгаз %.0f%% · жидкость %.0f%% · твёрдое %.0f%%", phaseFrac[0] * 100, phaseFrac[1] * 100, phaseFrac[2] * 100);
                setHot(698, hs.c_str());
            }
        }
        yy += rh;
    }
    // необязательные строки
    if (anyBondable || A::ionsTotal) {   // строка на всю ширину: реакции (ассоциация / обмен / диссоциация), ионы в растворе
        std::string s;
        if (anyBondable) s = fmt("реакций %lld  (ассоц. %lld · обмен %lld · дисс. %lld)", CH.assoc + CH.exch + CH.diss, CH.assoc, CH.exch, CH.diss);
        if (A::ionsTotal) s += fmt("%sионов в растворе %d/%d", s.empty() ? "" : "   ", A::ionsFree, A::ionsTotal);
        pushClip(x + pad, yy, w - 2 * pad, rh); drawText(fontXS, x + pad, yy + uiPx(1), s, C_DIM); popClip();
        yy += rh;
    }
    if (P.efield != 0) {
        static double Iavg = 0; double I = 0;
        for (int i = 0; i < S.n; i++) I += S.q[i] * S.vx[i];
        I /= S.Lx; Iavg += (I - Iavg) * 0.03;
        cell(0, "поле E", fmt("%+.2f", P.efield * cfg::U_E_VNM), "В/нм"); cell(1, "ток I", fmt("%+.4f", Iavg), "e/τ"); yy += rh;
    }
    if (EN.capped) { cell(0, "огр. F", fmt("%d", EN.capped), "случаев", C_TEXT_HI, VS_WARN); yy += rh; }
    if (physAlertActive()) {   // тревога устойчивости (физика)
        float cx = x + pad, h = drawWrapped(fontXS, cx + uiPx(8), yy + uiPx(2), w - 2 * pad - uiPx(8), physAlert, C_WARN);
        rectFill(cx, yy + uiPx(2), uiPx(2), h, C_WARN); yy += h + uiPx(4);
    }
    return yy + uiPx(6);
}

// ===================================== ВКЛАДКА «УПРАВЛЕНИЕ» ================================
static void drawCtrlTab(float x, float y, float w, float h) {
    float yy = scrollBegin(0, x, y, w - uiPx(8), h);
    const float W = w - uiPx(8), sh = uiPx(34), bh = uiPx(24), g = uiPx(4);
    auto slider = [&](int id, const std::string& lab, double* v, double lo, double hi, bool logs, const std::string& val, const char* hint) {
        bool c = uiSlider(id, x, yy, W, sh - uiPx(4), lab, v, lo, hi, logs, val, hint); yy += sh; return c; };
    uiSection(x, yy, W, "Термостат");
    if (uiCycle(200, x, yy, W, bh, "Термостат", TH_NAMES[P.thermostat], P.thermostat != TH_NVE,
                "Термостат: NVE (энергия сохраняется) → Берендсен → Бусси (канонический)\n→ Ланжевен (трение + шум) → Нозе–Гувер → нагрев постоянной мощностью")) {
        P.thermostat = nextThermostat(P.thermostat); S.xi = S.eta = 0; resetEnergyRef(); showToast(std::string(T("Термостат: ")) + T(TH_NAMES[P.thermostat])); }
    yy += bh + g;
    if (slider(100, "T* цель", &P.Tset, 0.02, 6.0, true, fmt("%.3f · %.0f K", P.Tset, realK(P.Tset)),
               "Температура термостата (ε/k). Справа — пересчёт для аргона.\nКолесо мыши над слайдером — точная подстройка")) { if (P.thermostat == TH_NOSE) resetEnergyRef(); }
    slider(113, "τ термостата", &P.tauT, 0.02, 5.0, true, fmt("%.2fτ · %.2f пс", P.tauT, realPs(P.tauT)),
           "Время релаксации термостата: меньше — жёстче держит T,\nбольше — ближе к изолированной системе");
    slider(108, "мощность нагрева", &P.heatPower, -0.15, 0.15, false, fmt("%+.3f ε/τ", P.heatPower),
           "Для режима «нагрев P»: постоянный приток энергии на атом.\nОтрицательная — охлаждение. На фазовом переходе T(t) выходит на плато");
    uiSection(x, yy, W, "Давление и границы");
    {
        float w1 = std::floor(W * 0.68f);
        if (uiCycle(201, x, yy, w1, bh, "Границы", BD_NAMES[P.boundary], P.boundary != B_PERIODIC, "Границы: периодические → стенки → поршень\n(верхняя стенка под давлением P внешн.)")) {
            pushUndo(); P.boundary = (P.boundary + 1) % 3; S.pistonV = 0; pistonGrab = false;
            for (int i = 0; i < S.n; i++) { S.x[i] = clampv(S.x[i], 0.3, S.Lx - 0.3); S.y[i] = clampv(S.y[i], 0.3, S.Ly - 0.3); S.z[i] = clampv(S.z[i], 0.3, S.Lz - 0.3); }
            nlValid = false; computeForces(); resetEnergyRef();
        }
        if (uiButton(202, x + w1 + g, yy, W - w1 - g, bh, "NPT", P.npt, false, "Баростат C-rescale (Бернетти–Бусси): ящик сжимается/расширяется до P внешн.,\nправильные флуктуации объёма (только при периодических границах)")) { P.npt = !P.npt; resetEnergyRef(); }
        yy += bh + g;
    }
    if (slider(106, "P внешн.", &P.pExt, 0.0, 3.0, false, fmt("%.3f · %.0f бар", P.pExt, realBar(P.pExt)), "Внешнее давление: на поршень и для баростата NPT")) resetEnergyRef();
    if (uiCycle(203, x, yy, W, bh, "Тепловые стенки", HW_NAMES[P.heatWalls], P.heatWalls != 0, "Тепловые стенки: левая горячая / правая холодная (теплопроводность)\nили горячее дно (кипение). Только со стенками")) {
        P.heatWalls = (P.heatWalls + 1) % 3; resetEnergyRef(); }
    yy += bh + g;
    slider(109, "T горячей стенки", &P.Thot, 0.1, 6.0, true, fmt("%.2f · %.0f K", P.Thot, realK(P.Thot)), "Температура горячей стенки");
    slider(110, "T холодной стенки", &P.Tcold, 0.02, 3.0, true, fmt("%.2f · %.0f K", P.Tcold, realK(P.Tcold)), "Температура холодной стенки");
    if (slider(111, "смачивание стенок", &P.wallAttr, 0, 2, false, fmt("%.2f", P.wallAttr), "Притяжение атомов к стенкам (потенциал 9-3):\n0 — только отталкивание, 2 — сильное смачивание")) { computeForces(); resetEnergyRef(); }
    if (slider(105, "гравитация g", &P.gravity, 0, 0.05, false, fmt("%.4f ε/(σ·m)", P.gravity), "Сила тяжести (только в ящике со стенками)")) resetEnergyRef();
    uiSection(x, yy, W, "Система");
    { double L = boxTarget > 0 ? boxTarget : S.Lx;
      if (slider(102, "размер ящика L", &L, 6, 240, true, fmt("%.1fσ · %.2f нм", L, realNm(L)), "Размер ящика: атомы масштабируются вместе с ним\n(адиабатическое сжатие/расширение, плавно; работа — в W)")) boxTarget = L; }
    { if (ui.active != 103) sliderN = S.n; slider(103, "число частиц N", &sliderN, 0, 6000, false, fmt("%d", (int)sliderN), "Число частиц: при отпускании добавляются (выбранное вещество)\nили удаляются случайные молекулы");
      if (sliderReleased == 103) { sliderReleased = -1; adjustCount((int)sliderN); } }
    if (slider(101, "ε притяжение ×", &P.epsScale, 0.2, 3.0, false, fmt("%.2f", P.epsScale), "Множитель притяжения (LJ и металлической связи):\nбольше — вещество «липче», выше T плавления и кипения")) {
        buildPairTables(); updatePresence(); computeForces(); resetEnergyRef(); }
    { double e = P.efield * cfg::U_E_VNM;   // в вольтах на нанометр: в воде ионы заметно дрейфуют при ~0.3–1 В/нм
      if (slider(112, "электрическое поле E", &e, -1, 1, false, fmt("%+.2f В/нм", e), "Поле вдоль x: сила qE на заряженные атомы.\nИоны дрейфуют, вода поворачивается диполями. Работа поля — внешняя")) {
          P.efield = std::fabs(e) < 0.015 ? 0 : e / cfg::U_E_VNM; } }
    uiSection(x, yy, W, "Химия");
    {
        float w1 = std::floor((W - g) / 2);
        if (uiButton(205, x, yy, w1, bh, P.chemistry ? "химия: вкл" : "химия: выкл", P.chemistry, false, "Химические реакции: образование и разрыв связей, обмен атомами")) P.chemistry = !P.chemistry;
        if (uiButton(204, x + w1 + g, yy, W - w1 - g, bh, "катализатор", P.catalyst, false, "Катализатор в центре ящика: барьеры реакций ×0.25 внутри круга\n(K — поставить под курсор)")) {
            P.catalyst = !P.catalyst; P.catX = S.Lx / 2; P.catY = S.Ly / 2; P.catZ = S.Lz / 2; }
        yy += bh + g;
    }
    slider(114, "барьеры реакций ×", &P.eaScale, 0, 3, false, fmt("%.2f", P.eaScale), "Множитель собственных барьеров реакций (1 — как в природе).\nМеняет скорости, но не равновесие; ниже ΔH барьер не опускается");
    uiSection(x, yy, W, "Интегрирование и инструменты");
    { double v = P.substeps; if (slider(104, "скорость, шагов/кадр", &v, 1, 60, true, fmt("%d", P.substeps), "Шагов интегрирования за кадр: ускорение времени\n(точность не меняется — шаг dt тот же). Если шаг укоротился\n(раскалённые атомы), шагов за кадр больше, пока хватает процессора")) P.substeps = std::max(1, (int)std::lround(v)); }
    slider(107, "кисть R", &P.brushR, 0.5, 14, true, fmt("%.1fσ · %.2f нм", P.brushR, realNm(P.brushR)), "Радиус кисти: добавление, ластик, нагрев, охлаждение, толчок, удар (×2.5), вспышка L");
    slider(116, "сила инструментов", &toolPower, 0.1, 10, true, fmt("%.2f×", toolPower), "Сила толчка, ударной волны и броска выделения");
    uiSection(x, yy, W, "Отображение");
    if (uiCycle(206, x, yy, W, bh, "Цвет атомов", COLOR_NAMES[colorMode], colorMode != 0, "Режим раскраски (C): элемент → скорость → энергия →\nлокальный порядок → координация → агрегатное состояние")) {
        colorMode = (colorMode + 1) % COLOR_N; }
    yy += bh + g;
    {
        struct LayerItem { int id; const char* name; bool* v; const char* hint; };
        const LayerItem ls[] = {{207, "связи", &bondsOn, "Химические связи (B); кратные — двойной/тройной линией"}, {208, "следы", &trailsOn, "Следы траекторий (T)"},
                        {790, "заряды", &layerCharges, "Частичные заряды: + — яркое сплошное кольцо, − — серое пунктирное"}, {791, "скорости", &layerVel, "Стрелки скоростей атомов"},
                        {792, "силы", &layerForce, "Стрелки сил (длина ∝ log |F|)"}, {793, "объекты поля", &layerFO, "Зоны и значки объектов поля"},
                        {794, "сетка", &layerGrid, "Координатная сетка с шагом в нанометрах"}, {795, "легенда", &layerLegend, "Легенда цветов в углу сцены"},
                        {796, "линейка масштаба", &layerScale, "Линейка масштаба (нм/Å) и оси в углу сцены"}, {797, "закреплённые", &layerPins, "Отметки закреплённых атомов"}};
        float cw = std::floor((W - g) / 2), ch = uiPx(22);
        for (int k = 0; k < 10; k++) uiCheck(ls[k].id, x + (k % 2) * (cw + g), yy + (k / 2) * ch, cw, ch, ls[k].name, ls[k].v, ls[k].hint);
        yy += 5 * ch + g;
    }
    slider(115, "размер шаров", &atomVis, 0.4, 2.2, true, fmt("%.2f×", atomVis), "Только отображение: размер атомов на экране");
    {
        float w3 = std::floor((W - 2 * g) / 3);
        if (uiButton(211, x, yy, w3, bh, "вращение", cam3.autoRot, false, "Автовращение камеры (O)")) cam3.autoRot = !cam3.autoRot;
        if (uiButton(212, x + w3 + g, yy, w3, bh, "полёт", camMode == 1, false, "Полёт камеры (V): WASD — движение, Q/E — вниз/вверх, Shift — быстрее")) {
            camMode ^= 1; showToast(camMode ? "Полёт: WASD, Q/E, Shift — быстрее" : "Камера: орбита"); }
        if (uiButton(213, x + 2 * (w3 + g), yy, W - 2 * (w3 + g), bh, "разрез", sliceOn, false, "Разрез (X): показать только то, что за плоскостью;\nShift+колесо — сдвинуть плоскость")) {
            sliceOn = !sliceOn; sliceOff = 0; }
        yy += bh + g;
        if (uiButton(214, x, yy, W, bh, fullscreen ? "оконный режим" : "полноэкранный режим", fullscreen, false, "Полноэкранный режим (F11 или Alt+Enter)")) toggleFullscreen();
        yy += bh + g;
    }
    scrollEnd(0, yy + uiPx(8));
}

// ===================================== ВКЛАДКА «ОБЪЕКТ» (инспектор) =========================
static std::vector<std::string> atomInfoLines(int i) {
    std::vector<std::string> L;
    const Element& e = EL[S.ty[i]];
    double v = std::sqrt(S.vx[i] * S.vx[i] + S.vy[i] * S.vy[i] + S.vz[i] * S.vz[i]);
    L.push_back(e.Z ? fmt("%s — %s (Z = %d)   #%d", e.sym, T(e.name), e.Z, i) : fmt("%s — %s   #%d", e.sym, T(e.name), i));
    L.push_back(fmt("r = (%.2f, %.2f, %.2f) нм", realNm(S.x[i]), realNm(S.y[i]), realNm(S.z[i])));
    L.push_back(fmt("v = %.3f σ/τ = %.0f м/с   Eк = %.3f ε", v, v * cfg::U_L_NM / cfg::U_T_PS * 1000, 0.5 * e.m * v * v));
    L.push_back(fmt("Eп = %.3f ε   q = %+.2f e   m = %.2f а.е.м.", S.ep[i], S.q[i], e.m * 10));
    std::string bs; for (int k = 0; k < S.nbc[i]; k++) { bs += EL[S.ty[S.nb[i][k]]].sym; bs += S.bo[i][k] == 1 ? "–" : (S.bo[i][k] == 2 ? "=" : "≡"); bs += " "; }
    L.push_back(fmt("связей: %d  своб. валентн.: %d  %s", S.nbc[i], std::max(0, freeVal(i)), bs.c_str()));
    if (S.nbc[i]) { std::vector<int> m; moleculeOf(i, m); L.push_back(m.size() < 48 ? fmt("молекула: %s (%d ат.)", molFormula(i).c_str(), (int)m.size()) : fmt("сетка из %d атомов", (int)m.size())); }
    if (isMetalT(S.ty[i])) L.push_back(fmt("металлическая связь: %.0f%% (окислен на %.0f%%)", metalW(i) * 100, (1 - metalW(i)) * 100));
    if (i < (int)A::coord.size()) {
        L.push_back(fmt("коорд. %d   q̄6 = %.2f  q̄4 = %.2f   %s", A::coord[i], A::ordMag[i], A::ordHue[i], T(ST_NAMES[A::stype[i]])));
    }
    if (i < (int)atomPhase.size() && atomPhase.size() == (size_t)S.n) L.push_back(std::string(T("состояние: ")) + T(PH_NAMES[std::min(3, (int)atomPhase[i])]));
    if (isPinned(i)) L.push_back("закреплён (I — открепить)");
    return L;
}
static void drawObjTab(float x, float y, float w, float h) {
    float yy = scrollBegin(2, x, y, w - uiPx(8), h);
    const float W = w - uiPx(8), bh = uiPx(24), g = uiPx(4), sh = uiPx(34), lh = fontU.h + uiPx(2);
    bool any = false;
    auto text = [&](const std::string& s, RGBA c = C_TEXT, const Font* f = nullptr) { const Font& ff = f ? *f : fontU; drawText(ff, x, yy, s, c); yy += ff.h + uiPx(2); };
    auto kv = [&](const std::string& k, const std::string& v) { drawText(fontXS, x, yy + uiPx(1), k, C_DIM); drawText(fontS, x + uiPx(96), yy, v, C_TEXT_HI); yy += fontS.h + uiPx(3); };
    // --- объект поля
    if (selFieldObj >= 0 && selFieldObj < (int)fieldObjs.size()) {
        any = true;
        FieldObj& o = fieldObjs[selFieldObj];
        uiSection(x, yy, W, fmt("Объект поля #%d", selFieldObj + 1));
        rectFill(x, yy, uiPx(26), uiPx(26), C_ELEM); drawIcon(IC_FO0 + o.kind, x + uiPx(13), yy + uiPx(13), uiPx(16), foColor(o, selFieldObj));
        drawText(fontUB, x + uiPx(34), yy + uiPx(4), FO_NAMES[o.kind], C_TEXT_HI);
        yy += uiPx(32);
        yy += drawWrapped(fontXS, x, yy, W, FO_HINTS[o.kind], C_DIM) + uiPx(4);
        {
            float w3 = std::floor((W - 2 * g) / 3);
            if (uiButton(800, x, yy, w3, bh, o.on ? "включён" : "выключен", o.on, false, "Включить/выключить объект (N)")) { foEditBegin(); o.on = !o.on; foEditEnd(); }
            if (uiButton(802, x + w3 + g, yy, w3, bh, "копия", false, false, "Дублировать объект (со сдвигом)")) {
                FieldObj c = o; double sh2 = 0.15 * std::min(S.Lx, S.Ly); c.x += sh2; c.x2 += sh2; if (isPer()) { c.x = std::fmod(c.x, S.Lx); c.x2 = std::fmod(c.x2, S.Lx); }
                foEditBegin(); fieldObjs.push_back(c); foEditEnd(); selFieldObj = (int)fieldObjs.size() - 1; }
            if (uiButton(801, x + 2 * (w3 + g), yy, W - 2 * (w3 + g), bh, "удалить", false, false, "Удалить объект (Del или ПКМ по значку)")) { foDelete(selFieldObj); scrollEnd(2, yy + bh); return; }
            yy += bh + g;
        }
        FieldObj& oo = fieldObjs[selFieldObj];
        auto foSlider = [&](int id, const std::string& lab, double* v, double lo, double hi, bool logs, const std::string& val, const char* hint) {
            double t = *v; if (uiSlider(id, x, yy, W, sh - uiPx(4), lab, &t, lo, hi, logs, val, hint)) { foEditBegin(); *v = t; foEditEnd(); } yy += sh; };
        foSlider(810, oo.kind == FO_BARRIER ? "полуширина пластины R" : "радиус R", &oo.R, 0.3, std::max(1.0, oo.kind == FO_BARRIER ? 1000.0 : std::max({S.Lx, S.Ly, S.Lz})), true,
                 oo.R >= 1000 ? std::string("∞") : fmt("%.2fσ · %.2f нм", oo.R, realNm(oo.R)), "Радиус действия (колесо над значком объекта)");
        { double lo, hi; foStrengthRange(oo.kind, lo, hi); double s = std::max(lo, std::fabs(oo.strength)), sg = oo.strength < 0 ? -1 : 1;
          double t = s; if (uiSlider(811, x, yy, W, sh - uiPx(4), "сила A", &t, lo, hi, true, fmt("%.3g %s", oo.strength, T(FO_UNITS[oo.kind])), "Сила объекта (Shift+колесо над значком)")) { foEditBegin(); oo.strength = sg * t; foEditEnd(); }
          yy += sh;
          if (oo.kind == FO_VORTEX) { if (uiButton(817, x, yy, W, bh, oo.strength >= 0 ? "вращение: против часовой" : "вращение: по часовой", false, false, "Сменить направление вращения")) { oo.strength = -oo.strength; } yy += bh + g; } }
        if (foUsesT(oo.kind)) foSlider(812, oo.kind == FO_EMITTER ? "T испускания" : "T зоны", &oo.Tset, 0.02, 8.0, true, fmt("%.3f · %.0f K", oo.Tset, realK(oo.Tset)), "Температура зоны / скорость испускаемых атомов");
        if (foUsesDir(oo.kind)) {
            double az = std::atan2(oo.dz, oo.dx) * 180 / PI, el = std::asin(clampv(oo.dy, -1.0, 1.0)) * 180 / PI; if (az < 0) az += 360;
            double a1 = az, e1 = el; bool ch = false;
            ch |= uiSlider(813, x, yy, W, sh - uiPx(4), oo.kind == FO_BARRIER ? "нормаль: азимут" : "направление: азимут", &a1, 0, 360, false, fmt("%.0f°", a1), "Азимут в плоскости xz"); yy += sh;
            ch |= uiSlider(814, x, yy, W, sh - uiPx(4), "угол возвышения", &e1, -90, 90, false, fmt("%+.0f°", e1), "Угол к плоскости xz (+90° — вверх по y)"); yy += sh;
            if (ch) { foEditBegin(); double ca = std::cos(e1 * PI / 180); oo.dx = ca * std::cos(a1 * PI / 180); oo.dz = ca * std::sin(a1 * PI / 180); oo.dy = std::sin(e1 * PI / 180); foEditEnd(); }
            if (uiButton(815, x, yy, W, bh, "направить по взгляду камеры", false, false, "Направление/нормаль := направление взгляда")) { foEditBegin(); oo.dx = camF[0]; oo.dy = camF[1]; oo.dz = camF[2]; foEditEnd(); }
            yy += bh + g;
        }
        if (oo.kind == FO_EMITTER) {
            int t = currentElemType();
            if (uiButton(816, x, yy, W, bh, fmt("элемент: %s  →  взять %s", EL[clampv(oo.elem, 0, NEL - 1)].sym, EL[t].sym), false, false, "Испускаемый элемент := текущее вещество палитры / таблицы (E)")) {
                if (!EL[t].fixed) oo.elem = t; }
            yy += bh + g;
        }
        if (oo.kind == FO_BARRIER) kv("концы, нм", fmt("(%.1f,%.1f,%.1f)–(%.1f,%.1f,%.1f)", realNm(oo.x), realNm(oo.y), realNm(oo.z), realNm(oo.x2), realNm(oo.y2), realNm(oo.z2)));
        else kv("центр, нм", fmt("(%.2f, %.2f, %.2f)", realNm(oo.x), realNm(oo.y), realNm(oo.z)));
        yy += g;
    }
    // --- выделение
    if (!selList.empty()) {
        any = true;
        uiSection(x, yy, W, fmt("Выделение: %d ат.", (int)selList.size()));
        std::map<int, int> cnt; int pinned = 0; double M = 0, px = 0, py = 0, pz = 0, K = 0;
        for (int i : selList) { cnt[S.ty[i]]++; if (isPinned(i)) pinned++; if (EL[S.ty[i]].fixed) continue; double m = EL[S.ty[i]].m; M += m; px += m * S.vx[i]; py += m * S.vy[i]; pz += m * S.vz[i]; K += kinOf(i); }
        std::vector<std::pair<int, int>> cs; for (auto& kv2 : cnt) cs.push_back({-kv2.second, kv2.first}); std::sort(cs.begin(), cs.end());
        std::string comp; for (size_t k = 0; k < cs.size() && k < 7; k++) comp += fmt("%s%s %d", k ? " · " : "", EL[cs[k].second].sym, -cs[k].first);
        if (cs.size() > 7) comp += " …";
        kv("состав", comp);
        int nm = 0; for (int i : selList) if (!EL[S.ty[i]].fixed && !isPinned(i)) nm++;
        double Kcm = M > 0 ? 0.5 * (px * px + py * py + pz * pz) / M : 0, Tsel = nm > 1 ? 2 * (K - Kcm) / (3 * (nm - 1)) : 0;
        kv("T (без ц.м.)", fmt("%.3f · %.0f K", Tsel, realK(Tsel)));
        double cx, cy, cz; selCOM(cx, cy, cz);
        kv("центр масс", fmt("(%.2f, %.2f, %.2f) нм", realNm(cx), realNm(cy), realNm(cz)));
        kv("импульс |p|", fmt("%.3f · v ц.м. %.3f σ/τ", std::sqrt(px * px + py * py + pz * pz), M > 0 ? std::sqrt(px * px + py * py + pz * pz) / M : 0.0));
        kv("масса", fmt("%.1f а.е.м.   закреплено %d", M * 10, pinned));
        yy += g;
        float w2 = std::floor((W - g) / 2);
        int t = currentElemType();
        struct B { int id; std::string label; const char* hint; };
        const B bs[] = {{820, pinned == (int)selList.size() ? "открепить" : "закрепить", "Закрепить / открепить (I): интегратор не двигает закреплённые атомы"},
                        {821, "остановить", "Скорости выделенных атомов := 0 (энергия — в W)"},
                        {822, "нагреть ×1.5 T", "Увеличить тепловые скорости (T ×1.5)"}, {823, "охладить ×0.67 T", "Уменьшить тепловые скорости (T ×0.67)"},
                        {824, fmt("элемент → %s", EL[t].sym), "Заменить элемент выделенных атомов текущим веществом (связи рвутся)"},
                        {825, "удалить", "Удалить выделенные атомы (Del)"}, {826, "копировать", "Копировать в буфер (Ctrl+C)"}, {827, "вставить", "Вставить у курсора (Ctrl+V)"},
                        {828, "повернуть −15°", "Поворот вокруг центра масс (Alt+колесо)"}, {829, "повернуть +15°", "Поворот вокруг центра масс (Alt+колесо)"},
                        {831, "следить", "Камера следит за первым атомом выделения"}, {830, "снять выделение", "Снять выделение (Esc)"}};
        for (int k = 0; k < 12; k++) {
            float bx = x + (k % 2) * (w2 + g);
            if (uiButton(bs[k].id, bx, yy, k % 2 ? W - w2 - g : w2, bh, bs[k].label, false, false, bs[k].hint)) {
                switch (bs[k].id) {
                case 820: selPin(); break;
                case 821: selStop(); break;
                case 822: selScaleThermal(std::sqrt(1.5)); break;
                case 823: selScaleThermal(std::sqrt(0.67)); break;
                case 824: selChangeElement(t); break;
                case 825: selDelete(); break;
                case 826: selCopy(); break;
                case 827: { double c3[3]; selCOM(c3[0], c3[1], c3[2]); pasteAt(c3[0] + 0.3 * S.Lx / 4, c3[1], c3[2]); break; }
                case 828: selRotate(-15 * PI / 180); break;
                case 829: selRotate(15 * PI / 180); break;
                case 830: selClear(); break;
                case 831: if (!selList.empty()) { followAtom = selList[0]; showToast(fmt("Камера следит за атомом #%d", followAtom)); } break;
                }
            }
            if (k % 2) yy += bh + g;
            if (selList.empty()) break;
        }
        yy += g;
    }
    // --- линейка / угломер
    if (measN > 0) {
        any = true;
        uiSection(x, yy, W, "Измерение");
        double d12, ang, dih; measResult(d12, ang, dih);
        std::string atoms; for (int k = 0; k < measN; k++) if (measIdx[k] >= 0 && measIdx[k] < S.n) atoms += fmt("%s%s#%d", k ? " – " : "", EL[S.ty[measIdx[k]]].sym, measIdx[k]);
        kv("атомы", atoms);
        if (d12 >= 0) kv("расстояние 1–2", fmt("%.3f Å · %.3fσ", d12 * 3.405, d12));
        if (ang >= 0) kv("угол 1–2–3", fmt("%.2f°", ang));
        if (measN >= 4) kv("двугранный", fmt("%.2f°", dih));
        if (measN < 2) text("выберите следующий атом…", C_DIM, &fontXS);
        if (uiButton(840, x, yy, std::floor(W / 2), bh, "сбросить", false, false, "Сбросить измерение")) measN = 0;
        yy += bh + g * 2;
    }
    // --- атом (слежение или под курсором)
    {
        int i = followAtom >= 0 && followAtom < S.n ? followAtom : (selList.size() == 1 ? selList[0] : hoverAtom());
        if (i >= 0 && i < S.n) {
            any = true;
            uiSection(x, yy, W, i == followAtom ? "Атом (слежение)" : "Атом");
            auto L = atomInfoLines(i);
            { MonoAtoms atomsColored; rectFill(x, yy, uiPx(2), L.size() * (fontXS.h + uiPx(2)), {EL[S.ty[i]].r, EL[S.ty[i]].g, EL[S.ty[i]].b, 1}); }
            for (size_t k = 0; k < L.size(); k++) { drawText(k == 0 ? fontU : fontXS, x + uiPx(8), yy, L[k], k == 0 ? C_TEXT_HI : C_TEXT); yy += (k == 0 ? fontU.h : fontXS.h) + uiPx(2); }
            yy += g;
            if (uiButton(845, x, yy, std::floor(W / 2), bh, i == followAtom ? "не следить" : "следить камерой", i == followAtom, false, "Камера следит за атомом (двойной щелчок по атому)")) {
                followAtom = i == followAtom ? -1 : i; }
            yy += bh + g * 2;
        }
    }
    // --- список объектов поля
    uiSection(x, yy, W, fmt("Объекты поля: %d", (int)fieldObjs.size()));
    if (fieldObjs.empty()) { yy += drawWrapped(fontXS, x, yy, W, "Нет объектов. Инструмент «Объекты поля» (Alt+F, значок внизу слева): выберите вид и щёлкните в сцене.", C_DIM) + g; }
    for (int k = 0; k < (int)fieldObjs.size() && k < 60; k++) {
        const FieldObj& o = fieldObjs[k];
        std::string lab = fmt("%d. %s  R %.1f  A %.2g%s", k + 1, T(FO_NAMES[o.kind]), o.R >= 1000 ? 999.0 : o.R, o.strength, o.on ? "" : T("  (выкл.)"));
        if (uiButton(850 + k, x, yy, W, uiPx(22), lab, k == selFieldObj, false, FO_HINTS[o.kind])) { selFieldObj = k; }
        yy += uiPx(22) + uiPx(2);
    }
    if (!fieldObjs.empty()) {
        if (uiButton(849, x, yy, std::floor(W / 2), bh, "удалить все", false, false, "Удалить все объекты поля")) { foEditBegin(); fieldObjs.clear(); foEditEnd(); selFieldObj = -1; }
        yy += bh + g;
    }
    if (!any) { yy += g; yy += drawWrapped(fontXS, x, yy, W, "Выделите атомы (инструмент «Выделение», Alt+2), выберите объект поля, измерьте расстояние (Alt+3) или наведите курсор на атом — подробности появятся здесь.", C_DIM); }
    (void)lh;
    scrollEnd(2, yy + uiPx(8));
}

// ===================================== БОКОВАЯ ПАНЕЛЬ ======================================
static int sideTab = 0;   // 0 управление, 1 физика, 2 химия, 3 графики, 4 объект
static const char* TAB_NAMES[5] = {"Управление", "Физика", "Химия", "Графики", "Объект"};
static void drawSidePanel(float x, float y, float w, float h) {
    if (world != W_MD) { worldPanel(x, y, w, h); return; }
    rectFill(x, y, w, h, C_PANEL); lineV(x, y, y + h, C_LINE);
    pushClip(x + 1, y, w - 1, h);
    float yy = drawReadouts(x + 1, y, w - 1);
    // вкладки
    float th = uiPx(28), tx = x + 1, tw0 = 0;
    for (auto t : TAB_NAMES) tw0 += textW(fontUB, t) + uiPx(12);
    float extra = std::max(0.0f, (w - 1 - tw0) / 5);
    rectFill(x + 1, yy, w - 1, th, C_PANEL2); lineH(x + 1, x + w, yy, C_LINE); lineH(x + 1, x + w, yy + th - 1, C_LINE);
    static const char* tabHints[5] = {"Управление: термостат, давление, система, химия, отображение", "Физика: параметры и инструменты физической модели",
                                      "Химия: реакции, библиотека молекул", "Графики: распределения, временные ряды, кинетика реакций",
                                      "Объект: инспектор выделения, объекта поля, измерений и атома"};
    for (int k = 0; k < 5; k++) {
        float tw = std::floor(textW(fontUB, TAB_NAMES[k]) + uiPx(12) + extra);
        if (k == 4) tw = x + w - tx;
        if (uiTab(700 + k, tx, yy, tw, th, TAB_NAMES[k], sideTab == k, tabHints[k])) sideTab = k;
        tx += tw;
    }
    yy += th;
    const float pad = uiPx(10);
    float cx = x + pad, cy = yy + uiPx(6), cw = w - 2 * pad + uiPx(4), ch = y + h - cy - uiPx(4);
    switch (sideTab) {
    case 0: drawCtrlTab(cx, cy, cw, ch); break;
    case 1: pushClip(cx - uiPx(2), cy - uiPx(2), cw + uiPx(4), ch + uiPx(4)); drawPhysPanel(cx, cy, cw - uiPx(4), ch); popClip(); break;
    case 2: pushClip(cx - uiPx(2), cy - uiPx(2), cw + uiPx(4), ch + uiPx(4)); drawChemPanel(cx, cy, cw - uiPx(4), ch); popClip(); break;
    case 3: {
        float yy2 = scrollBegin(1, cx, cy, cw - uiPx(8), ch);
        std::string sub = fmt("кристалл %.0f%% (ГЦК %d · ГПУ %d · ОЦК %d)   коорд. %.2f   D %.4f", A::fCryst * 100, A::stCount[ST_FCC], A::stCount[ST_HCP], A::stCount[ST_BCC], A::meanCoord, std::max(0.0, A::D));
        if (A::Cv > 0) sub += fmt("   Cv/Nk %.2f", A::Cv);
        pushClip(cx, yy2, cw - uiPx(8), fontXS.h + uiPx(4)); drawText(fontXS, cx, yy2, sub, C_DIM); popClip();
        float bottom = drawPlots(cx, yy2 + fontXS.h + uiPx(6), cw - uiPx(8), ch - fontXS.h - uiPx(6), uiPx(118));
        scrollEnd(1, bottom + uiPx(6));
        break; }
    case 4: drawObjTab(cx, cy, cw, ch); break;
    }
    popClip();
}

// ===================================== ПАНЕЛЬ ИНСТРУМЕНТОВ (слева) =========================
static void drawToolbar(float x, float y, float w, float h) {
    rectFill(x, y, w, h, C_PANEL); lineV(x + w - 1, y, y + h, C_LINE);
    float bs = w - uiPx(8), yy = y + uiPx(6);
    for (int t : TOOL_ORDER) {
        if (world != W_MD && t != TOOL_CAMERA) continue;   // в мирах ядер, кварков, волн мышь — только камера
        if (t < 0) { lineH(x + uiPx(8), x + w - uiPx(8), yy + uiPx(3), C_LINE); yy += uiPx(7); continue; }
        std::string hint = fmt("%s (%s)\n%s", T(TOOL_NAMES[t]), TOOL_KEY[t], T(TOOL_HELP[t]));
        if (uiIconBtn(230 + t, x + uiPx(4), yy, bs, bs, TOOL_ICON[t], lmbTool == t, hint.c_str())) setTool(t);
        yy += bs + uiPx(3);
    }
    (void)h;
}
// выбор вида объекта поля (виден при инструменте «объекты поля»): столбец значков у левого края сцены
static void drawFoFlyout() {
    if (lmbTool != TOOL_FIELD) return;
    float bs = uiPx(30), x = sceneX + uiPx(8), y = sceneY + uiPx(46), hh = FO_N * (bs + uiPx(2)) + uiPx(24);
    if (y + hh > sceneY + sceneH - uiPx(8)) { bs = std::max(uiPx(20), (sceneH - uiPx(70)) / FO_N - uiPx(2)); hh = FO_N * (bs + uiPx(2)) + uiPx(24); }
    boxPanel(x, y, bs + uiPx(8), hh, withA(C_PANEL, 0.94f), C_LINE);
    drawTextC(fontXS, x + (bs + uiPx(8)) / 2, y + uiPx(4), "вид", C_DIM);
    float yy = y + uiPx(20);
    for (int k = 0; k < FO_N; k++) {
        std::string hint = FO_HINTS[k];
        if (uiIconBtn(740 + k, x + uiPx(4), yy, bs, bs, IC_FO0 + k, foKind == k, hint.c_str(), k == FO_HEATER ? C_HOT : k == FO_COOLER ? C_COLD : C_TEXT)) {
            foKind = k; showToast(std::string(T("Объект поля: ")) + T(FO_NAMES[k]) + T(" — щёлкните в сцене")); }
        yy += bs + uiPx(2);
    }
}
// ===================================== ПАЛИТРА ВЕЩЕСТВ (под сценой) ========================
static void drawPaletteStrip(float x, float y, float w, float h) {
    if (world != W_MD) { worldStrip(x, y, w, h); return; }
    rectFill(x, y, w, h, C_PANEL); lineH(x, x + w, y, C_LINE);
    pushClip(x, y, w, h);
    float bx = x + uiPx(10), bh = h - uiPx(8), by = y + uiPx(4);
    bx += drawText(fontXS, bx, y + (h - fontXS.h) / 2, "вещество", C_DIM) + uiPx(10);
    float pw = std::max(uiPx(30), std::floor((x + w - uiPx(8) - bx) / (float)palette.size() - uiPx(3)));
    for (size_t k = 0; k < palette.size(); k++) {
        const Tmpl& m = palette[k]; RGBA sc = {0, 0, 0, 0};
        if (m.a.size() == 1 && !m.tool) { const Element& e = EL[m.a[0].t]; sc = {e.r, e.g, e.b, 1}; }
        std::string hint = k == 0 ? fmt("%s — %s\nэлемент, выбранный в таблице Менделеева (E)", EL[m.a[0].t].sym, T(EL[m.a[0].t].name))
                         : m.tool ? std::string("Стена\nЛКМ (инструмент «добавить») рисует перегородку из неподвижных атомов")
                         : m.a.size() == 1 ? fmt("%s — %s", EL[m.a[0].t].sym, T(EL[m.a[0].t].name)) : std::string(T("молекула ")) + T(m.label);
        if (uiButton(300 + (int)k, bx, by, pw, bh, m.label, (int)k == selPal, true, hint.c_str(), sc.a > 0 ? &sc : nullptr, k == 0 && (int)k != selPal)) {
            selPal = (int)k; if (lmbTool != TOOL_ADD && lmbTool != TOOL_FIELD && lmbTool != TOOL_SELECT) lmbTool = TOOL_ADD; }
        bx += pw + uiPx(3);
    }
    popClip();
}
// ===================================== СТРОКА СОСТОЯНИЯ (низ окна) =========================
static void drawStatusBar(float x, float y, float w, float h) {
    rectFill(x, y, w, h, C_PANEL2); lineH(x, x + w, y, C_LINE);
    float ty = y + (h - fontXS.h) / 2, xx = x + uiPx(10);
    // справа: координаты курсора, слежение, запись, пауза
    std::string right;
    if (inScene(mouseX, mouseY)) {
        int hA = hoverAtom(); if (hA >= 0) right = fmt("%s #%d  (%.2f, %.2f, %.2f) нм", EL[S.ty[hA]].sym, hA, realNm(S.x[hA]), realNm(S.y[hA]), realNm(S.z[hA]));
    }
    if (followAtom >= 0 && followAtom < S.n) right += fmt("   слежение #%d", followAtom);
    if (!selList.empty()) right += fmt("   выделено %d", (int)selList.size());
    float rx = x + w - uiPx(10);
    if (P.paused) { rx -= drawTextR(fontUB, rx, y + (h - fontUB.h) / 2, "ПАУЗА", C_WARN) + uiPx(14); }
    if (recording) { std::string r = fmt("● запись: %d кадр.", recFrames); rx -= drawTextR(fontXS, rx, ty, r, C_WARN) + uiPx(14); }
    if (!right.empty()) rx -= drawTextR(fontS, rx, y + (h - fontS.h) / 2, right, C_DIM) + uiPx(14);
    // слева: инструмент и подсказка по мыши (или тревога физики)
    pushClip(x, y, rx - x, h);
    if (world == W_WAVE) {   // у волны нет камеры: мышь — прибор, измеряющий положение электрона
        xx += drawText(fontUB, xx, y + (h - fontUB.h) / 2, "Измерение", C_TEXT_HI) + uiPx(10);
        drawText(fontXS, xx, ty, "щелчок по волне — найти электрон в этом месте: с вероятностью |ψ|² он там, иначе волна там исчезает", C_DIM);
        popClip(); return;
    }
    if (world == W_SEMI) {
        xx += drawText(fontUB, xx, y + (h - fontUB.h) / 2, "Прибор", C_TEXT_HI) + uiPx(10);
        drawText(fontXS, xx, ty, "напряжения на выводах — ползунками справа; точки вольт-амперной характеристики ставятся сами", C_DIM);
        popClip(); return;
    }
    xx += drawText(fontUB, xx, y + (h - fontUB.h) / 2, TOOL_NAMES[clampv(lmbTool, 0, TOOL_N - 1)], C_TEXT_HI) + uiPx(10);
    if (physAlertActive()) drawText(fontXS, xx, ty, physAlert, C_WARN);
    else {
        std::string s = T(TOOL_STATUS[clampv(lmbTool, 0, TOOL_N - 1)]);
        if (lmbTool == TOOL_FIELD) s = std::string(T(FO_NAMES[foKind])) + ": " + s;
        if (lmbTool == TOOL_ADD) s = std::string(palette[selPal].tool ? T("стена") : T(palette[selPal].label)) + ": " + s;
        drawText(fontXS, xx, ty, s, C_DIM);
    }
    popClip();
}
// ===================================== НАДПИСИ НА СЦЕНЕ, УВЕДОМЛЕНИЯ, ПОДСКАЗКА АТОМА ======
static void drawSceneOverlay() {
    pushClip(sceneX, sceneY, sceneW, sceneH);
    float x = sceneX + uiPx(12), y = sceneY + uiPx(8);
    drawText(fontU, x, y, presetLoaded ? std::string(T(presetTitle)) + T("  [загружено]") : presetTitle, withA(C_TEXT, 0.9f));
    if (world != W_MD) { drawText(fontXS, x, y + fontU.h + uiPx(1), worldSubtitle(), C_DIM); popClip(); return; }
    std::string sub = fmt("%s · %s · %s · ящик %.1f×%.1f%s σ (%.2f×%.2f%s нм)", T(TH_NAMES[P.thermostat]), T(BD_NAMES[P.boundary]), T(P.chemistry ? "химия вкл." : "химия выкл."),
                          S.Lx, S.Ly, fmt("×%.1f", S.Lz).c_str(), realNm(S.Lx), realNm(S.Ly), fmt("×%.2f", realNm(S.Lz)).c_str());
    sub += camMode == 1 ? T(" · камера: полёт (WASD, Q/E)") : "";
    if (sliceOn) sub += fmt(" · разрез %+.1fσ (Shift+колесо)", sliceOff);
    drawText(fontXS, x, y + fontU.h + uiPx(1), sub, C_DIM);
    if (!A::sceneNote.empty()) drawText(fontXS, x, y + fontU.h + fontXS.h + uiPx(2), A::sceneNote, C_TEXT);   // живое измерение сцены
    popClip();
}
static void drawToast() {
    if (toastTime <= 0) return;
    float a = (float)std::min(1.0, toastTime);
    float w = std::min(textW(fontU, toast) + uiPx(30), sceneW - uiPx(40)), h = fontU.h + uiPx(14), x = std::floor(sceneX + (sceneW - w) / 2), y = sceneY + sceneH - h - uiPx(56);
    boxPanel(x, y, w, h, withA(C_PANEL, 0.95f * a), withA(C_LINE_H, a)); rectFill(x, y, uiPx(2), h, withA(C_ACC, a));
    pushClip(x + 4, y, w - 8, h); drawText(fontU, x + uiPx(14), y + uiPx(7), toast, withA(C_TEXT_HI, a)); popClip();
}
static int hoverAtom() {
    if (!inScene(mouseX, mouseY) || (int)psx.size() != S.n) return -1;
    int best = -1; double bd = 1e30;
    for (int i = 0; i < S.n; i++) {
        if (!pvis[i]) continue;
        double dx = psx[i] - mouseX, dy = psy[i] - mouseY, rr = std::max(3.0, visSig(S.ty[i]) * 0.6 * std::max(0.7, atomVis) * pscl[i]);
        if (dx * dx + dy * dy > rr * rr) continue;
        if (pdep[i] < bd) { bd = pdep[i]; best = i; }   // ближайший к камере
    }
    return best;
}
static void drawTooltip() {
    if (lmbTool == TOOL_CAMERA || rubberOn || throwDrag) return;
    int i = hoverAtom(); if (i < 0 || grabbed >= 0) return;
    auto L = atomInfoLines(i);
    float w = 0; for (size_t k = 0; k < L.size(); k++) w = std::max(w, textW(k == 0 ? fontU : fontXS, L[k]));
    float lh = fontXS.h + uiPx(2), h = std::max(uiPx(78), fontU.h + uiPx(3) + (L.size() - 1) * lh + uiPx(10));
    const float ms = uiPx(70);   // миниатюра: электронное облако атома
    float x = (float)mouseX + uiPx(18), y = (float)mouseY + uiPx(18);
    if (x + w + ms + uiPx(28) > sceneX + sceneW) x = mouseX - w - ms - uiPx(36);
    if (y + h > sceneY + sceneH) y = mouseY - h - uiPx(8);
    const Element& e = EL[S.ty[i]];
    boxPanel(x, y, w + ms + uiPx(28), h, withA(C_PANEL2, 0.95f), C_LINE_H); { MonoAtoms atomsColored; rectFill(x, y, uiPx(2), h, {e.r, e.g, e.b, 1}); }
    float yy = y + uiPx(5);
    for (size_t k = 0; k < L.size(); k++) { drawText(k == 0 ? fontU : fontXS, x + uiPx(10), yy, L[k], k == 0 ? C_TEXT_HI : C_TEXT); yy += k == 0 ? fontU.h + uiPx(3) : lh; }
    if (e.Z >= 1) { const float mx = x + w + uiPx(18), my = y + uiPx(4); rectFill(mx, my, ms, ms, hexc(0x050505)); drawAtomMini(e.Z, mx, my, ms, (float)uiClock);
                    drawTextR(fontXS, mx + ms - uiPx(2), my + ms - fontXS.h, "F7", C_FAINT); }
}
static void drawHelp() {
    static const char* lines[] = {
        "#МЫШЬ",
        "ЛКМ — текущий инструмент (панель слева, Alt+1…Alt+0, Alt+F); наведите на значок — подсказка",
        "ПКМ — нагрев, Shift+ПКМ — охлаждение; ПКМ по значку объекта поля — удалить;  СКМ — ластик;  колесо — масштаб",
        "Ctrl+ЛКМ — камера в любом инструменте;  двойной щелчок по атому — следить камерой",
        "#ИНСТРУМЕНТЫ",
        "Alt+1 камера   Alt+2 выделение   Alt+3 линейка/угломер   Alt+4 добавить/пинцет   Alt+5 ластик   Alt+6 нагрев   Alt+7 холод",
        "Alt+8 толчок (Shift — притяжение)   Alt+9 ударная волна   Alt+0 ножницы (рвать связи)   Alt+F объекты поля",
        "Выделение: рамка (Shift — добавить), тащить — сдвиг, Alt+тащить — бросок, Alt+колесо — поворот, Ctrl+A — всё",
        "Del — удалить выделенное / объект поля   Ctrl+C / Ctrl+V — копировать / вставить у курсора   I — закрепить/открепить",
        "Объекты поля: колесо над значком — радиус, Shift+колесо — сила, N — вкл/выкл; параметры — во вкладке «Объект»",
        "#КАМЕРА",
        "Ctrl+ЛКМ — вращение, Ctrl+Shift+ЛКМ — сдвиг, стрелки — поворот, PgUp/PgDn — ближе/дальше, F/Home — вписать, O — автовращение",
        "V — полёт: WASD, Q/E, Shift — быстрее;  F2 спереди, F3 сбоку, F4 сверху, F6 изометрия;  X — разрез, Shift+колесо — плоскость",
        "#КЛАВИШИ",
        "Пробел — пауза   S — шаг   R — сброс   G — боковая панель   T — следы   B — связи   C — цвет   H — справка",
        "U — обратить время   M — сброс MSD   K — катализатор   L — вспышка света   [ ] — сжать/растянуть по x",
        "F12 — снимок сцены (PNG), Shift+F12 — всё окно, Ctrl+F12 — запись кадров;  F11 / Alt+Enter — полный экран",
        "F1 — инструкция   F5/F9 — быстрое сохранение/загрузка   Ctrl+S/Ctrl+O — файл   Ctrl+E — CSV   Ctrl+Z — отмена   Esc — закрыть",
        "F8 — настройки (интерфейс, графика, камера, расчёт)   Ctrl+L — язык интерфейса: русский / English",
        "#СЦЕНЫ (Tab — меню; повторное нажатие — вариант)   ·   E — таблица Менделеева",
        "1 идеальный газ   2 плавление   3 кипение   4 конденсация   5 диффузия   6 NaCl в воде   7 горение   8 броуновское движение",
        "9 закалка / стекло   0 равновесие Cl2 ↔ 2Cl   Shift+1…5 — золото, окисление железа, Na в хлоре, метан, электрофорез",
        "в меню (Tab), вещество: теплопроводность, ударная труба, барометрическая формула, эффузия, спекание, адиабатическое сжатие,",
        "смачивание, кристаллизация на затравке, течение Пуазейля, жидкость и пар, адсорбция, выравнивание температур, кавитация, нанопровод",
        "в меню (Tab), химия: кислота в воде, нейтрализация, горение этанола, гремучая смесь, катализ на платине, хлорирование метана,",
        "H2 + I2 ⇌ 2HI, гидрирование, пероксид, ацетилен, H2 + Br2, хлор и бром, H2 + F2, озон, пропан, взрыв NCl3, самовоспламенение",
    };
    const int nl = (int)(sizeof(lines) / sizeof(lines[0]));
    float lh = fontU.h + uiPx(4), w = 0;
    // строка переводится целиком (заголовок — вместе с «#», перевод тоже начинается с «#»), затем «#» отбрасывается
    for (auto l0 : lines) { const char* l = T(l0); w = std::max(w, textW(l[0] == '#' ? fontUB : fontU, l[0] == '#' ? l + 1 : l)); }
    w += uiPx(36); float h = lh * nl + uiPx(56);
    if (w > sceneW - uiPx(20)) w = sceneW - uiPx(20);
    float x = std::floor(sceneX + (sceneW - w) / 2), y = std::floor(sceneY + std::max(uiPx(20), (sceneH - h) / 2));
    boxPanel(x, y, w, h, withA(C_PANEL, 0.97f), C_LINE_H); rectFill(x, y, w, uiPx(2), C_ACC);
    pushClip(x, y, w, h);
    float yy = y + uiPx(14);
    for (int k = 0; k < nl; k++) {
        const char* l = T(lines[k]); bool hdr = l[0] == '#';
        if (hdr && k) yy += uiPx(4);
        drawText(hdr ? fontUB : fontU, x + uiPx(18), yy, hdr ? l + 1 : l, hdr ? C_ACC : C_TEXT);
        yy += lh;
    }
    drawText(fontXS, x + uiPx(18), yy + uiPx(6), "Подробная инструкция с объяснением физики — F1 (ИНСТРУКЦИЯ.html).  H или Esc — закрыть", C_WARN);
    popClip();
}

// ===================================== ТАБЛИЦА МЕНДЕЛЕЕВА / МЕНЮ СЦЕН ======================
// категории: уровень серого полосы × рисунок (металлы — сплошная, неметаллы — штрих, лантаноиды/актиноиды — точки)
static const RGBA CATC[CAT_SPECIAL + 1] = {grayc(1.00f), grayc(0.74f), grayc(0.52f), grayc(0.34f), grayc(1.00f),
                                           grayc(0.74f), grayc(0.52f), grayc(0.34f), grayc(0.80f), grayc(0.48f), grayc(0.40f)};
static const int CAT_PAT[CAT_SPECIAL + 1] = {LS_SOLID, LS_SOLID, LS_SOLID, LS_SOLID, LS_DASH, LS_DASH, LS_DASH, LS_DASH, LS_DOT, LS_DOT, LS_SOLID};
// полоса категории: сплошная / штрих 6-3 / точки 2-2 (px)
static void catStrip(float x, float y, float w, float h, int cat, float a = 1) {
    const RGBA c = withA(CATC[cat], a); const int p = CAT_PAT[cat];
    if (p == LS_SOLID) { rectFill(x, y, w, h, c); return; }
    const float on = uiPx(p == LS_DASH ? 6.0f : 2.0f), off = uiPx(p == LS_DASH ? 3.0f : 2.0f);
    for (float xx = x; xx < x + w; xx += on + off) rectFill(xx, y, std::min(on, x + w - xx), h, c);
}
// метка радиоактивного элемента: залитый треугольник в правом верхнем углу ячейки
static void radioMark(float x, float y, float s, RGBA c) {
    glDisable(GL_TEXTURE_2D); col(c); glBegin(GL_TRIANGLES); glVertex2f(x - s, y); glVertex2f(x, y); glVertex2f(x, y + s); glEnd();
}
// позиция элемента в длинной форме таблицы: строки 0–6 — периоды, 8–9 — лантаноиды и актиноиды
static void ptPos(int Z, int& row, int& col) {
    if (Z == 1) { row = 0; col = 0; } else if (Z == 2) { row = 0; col = 17; }
    else if (Z <= 10) { row = 1; col = Z <= 4 ? Z - 3 : Z + 7; }
    else if (Z <= 18) { row = 2; col = Z <= 12 ? Z - 11 : Z - 1; }
    else if (Z <= 36) { row = 3; col = Z - 19; }
    else if (Z <= 54) { row = 4; col = Z - 37; }
    else if (Z <= 86) { if (Z <= 56) { row = 5; col = Z - 55; } else if (Z <= 71) { row = 8; col = 2 + Z - 57; } else { row = 5; col = Z - 69; } }
    else { if (Z <= 88) { row = 6; col = Z - 87; } else if (Z <= 103) { row = 9; col = 2 + Z - 89; } else { row = 6; col = Z - 101; } }
}
static PR ptRect, menuRect;
static int ptHoverZ = 0;
static std::string elementModelNote(int t) {
    const Element& e = EL[t];
    if (e.Z == 0) return "";
    if (e.cat == CAT_NOB) return fmt("в модели: инертен, только ван-дер-ваальс (LJ: σ = %.2f, ε = %.2f)", e.sig, e.eps);
    if (isMetalT(t)) return fmt("в модели: металлическая связь (многочастичная), E_coh = %.2f эВ; с неметаллами — связи, валентность %d", e.ecoh, e.val);
    std::string s = fmt("в модели: ковалентные связи, валентность %d", e.val);
    if (multiBonder(e.Z)) s += T(", кратные связи");
    if (t == E_O || t == E_H) s += T("; настроенная модель воды (H-связи, тетраэдричность)");
    return s;
}
static void selectElement(int t) {
    setCustomElement(t); selPal = 0; if (lmbTool != TOOL_FIELD && lmbTool != TOOL_SELECT) lmbTool = TOOL_ADD; ptOn = false;
    if (selFieldObj >= 0 && selFieldObj < (int)fieldObjs.size() && fieldObjs[selFieldObj].kind == FO_EMITTER && !EL[t].fixed) fieldObjs[selFieldObj].elem = t;
    showToast(fmt("Выбран %s — %s. ЛКМ в сцене — добавить атомы", EL[t].sym, T(EL[t].name)));
}
static void drawPeriodicTable() {
    float cell = std::floor(std::min((winW - uiPx(60)) / 18.0f, (winH - uiPx(170)) / 10.3f)); cell = clampv(cell, uiPx(30), uiPx(66));
    float pad = uiPx(18), W = 18 * cell + 2 * pad, H = 10.3f * cell + uiPx(100);
    float x0 = std::floor((winW - W) / 2), y0 = std::floor(std::max(uiPx(8), (winH - H) / 2));
    ptRect = {x0, y0, W, H};
    rectFill(0, 0, (float)winW, (float)winH, {0, 0, 0, 0.6f});
    boxPanel(x0, y0, W, H, C_PANEL, C_LINE_H); rectFill(x0, y0, W, uiPx(2), C_ACC);
    drawText(fontL, x0 + pad, y0 + uiPx(12), "Периодическая система элементов", C_TEXT_HI);
    drawTextR(fontXS, x0 + W - pad, y0 + uiPx(17), "щелчок — выбрать · Esc / E — закрыть", C_DIM);
    float gx = x0 + pad, gy = y0 + uiPx(22) + fontL.h + uiPx(18);
    ptHoverZ = 0;
    auto cellRect = [&](int row, int col) { float yy = gy + row * cell - (row >= 8 ? cell * 0.7f : 0); return PR{gx + col * cell + 1, yy + 1, cell - 2, cell - 2}; };
    for (int c = 0; c < 18; c++) { std::string s = std::to_string(c + 1); drawTextC(fontXS, gx + c * cell + cell / 2, gy - uiPx(15), s, C_FAINT); }
    for (int z = 1; z <= 118; z++) {
        int row, col; ptPos(z, row, col); PR r = cellRect(row, col);
        int t = typeOfZ(z); const Element& e = EL[t]; const RGBA& cc = CATC[ZD[z].cat];
        bool hover = inPR(r), sel = t == customType;
        uiRects[400 + z] = r;
        if (hover) { ptHoverZ = z; if (ui.pressed) ui.active = 400 + z; }
        bool clicked = ui.active == 400 + z && ui.released && hover;
        if (ui.active == 400 + z && ui.released) ui.active = -1;
        rectFill(r.x, r.y, r.w, r.h, grayc(cc.r * 0.10f + 0.06f));
        catStrip(r.x, r.y, r.w, uiPx(3), ZD[z].cat, hover ? 1.0f : 0.8f);
        if (hover) rectLine(r.x, r.y, r.w, r.h, C_TEXT_HI);
        if (sel) { rectLine(r.x - 1, r.y - 1, r.w + 2, r.h + 2, C_ACC); rectLine(r.x, r.y, r.w, r.h, C_ACC); }
        if (isRadioactive(z)) radioMark(r.x + r.w - uiPx(2), r.y + uiPx(5), uiPx(7), C_DIM);
        RGBA tc = isRadioactive(z) ? C_TEXT : C_TEXT_HI;
        drawText(fontXS, r.x + uiPx(3), r.y + uiPx(2), std::to_string(z), C_DIM);
        if (cell >= uiPx(44)) {
            drawTextC(fontL, r.x + r.w / 2, r.y + r.h * 0.5f - fontL.h * 0.62f, e.sym, tc);
            pushClip(r.x + 1, r.y, r.w - 2, r.h);
            float nw = textW(fontXS, e.name); drawText(fontXS, r.x + std::max(2.0f, (r.w - nw) / 2), r.y + r.h - fontXS.h - uiPx(2), e.name, C_DIM);
            popClip();
        } else drawTextC(fontM, r.x + r.w / 2, r.y + (r.h - fontM.h) / 2 + uiPx(3), e.sym, tc);
        if (clicked) selectElement(t);
    }
    for (int k = 0; k < 2; k++) { PR r = cellRect(5 + k, 2); rectLine(r.x, r.y, r.w, r.h, withA(CATC[CAT_LAN + k], 0.6f)); catStrip(r.x, r.y, r.w, uiPx(3), CAT_LAN + k, 0.8f);
        const char* s = k ? "89–103" : "57–71"; drawTextC(fontXS, r.x + r.w / 2, r.y + (r.h - fontXS.h) / 2, s, C_DIM); }
    // карточка элемента (в пустой области над переходными металлами)
    {
        int z = ptHoverZ ? ptHoverZ : EL[customType].Z;
        PR c0 = cellRect(0, 2), c1 = cellRect(2, 11);
        float cx = c0.x + uiPx(4), cy = c0.y + uiPx(2), cw = c1.x + c1.w - c0.x - uiPx(8), ch = c1.y + c1.h - c0.y - uiPx(6);
        if (z >= 1) {
            int t = typeOfZ(z); const Element& e = EL[t]; const ZData& d = ZD[z]; const RGBA& cc = CATC[d.cat];
            boxPanel(cx, cy, cw, ch, C_PANEL2, C_LINE);
            float bs = std::min(ch - uiPx(16), 2.2f * cell);
            // миниатюра атома: электронное облако (не орбиты!); щелчок — окно «Строение атома»
            const PR mini{cx + uiPx(8), cy + uiPx(8), bs, bs};
            rectFill(mini.x, mini.y, bs, bs, hexc(0x060606)); catStrip(mini.x, mini.y, bs, uiPx(4), d.cat);
            drawAtomMini(z, mini.x, mini.y + uiPx(4), bs, (float)uiClock);
            { MonoAtoms atomsColored; float q = uiPx(10); rectFill(mini.x + bs - q - uiPx(5), mini.y + bs - q - uiPx(5), q, q, {e.r, e.g, e.b, 1}); }   // цвет атома в сцене
            drawText(fontUB, mini.x + uiPx(5), mini.y + bs - fontUB.h - uiPx(22), e.sym, C_TEXT_HI);
            drawText(fontXS, mini.x + uiPx(5), mini.y + uiPx(6), std::to_string(z), C_TEXT);
            if (uiButton(1790, mini.x, mini.y + bs - uiPx(18), bs, uiPx(18), "строение атома · F7", false, false,
                         "Электронные облака и орбитали этого атома — не планетарная модель, а волны |ψ|²")) { avZ = z; atomViewOn = true; ptOn = false; }
            float tx = cx + bs + uiPx(20), ty = cy + uiPx(8);
            pushClip(tx, cy, cw - bs - uiPx(24), ch);
            drawText(fontL, tx, ty, fmt("%s — %s", e.sym, T(e.name)), C_TEXT_HI); ty += fontL.h + uiPx(4);
            drawText(fontU, tx, ty, fmt("%s%s · атомная масса %.3f", T(CAT_NAMES[d.cat]), isRadioactive(z) ? T(" · радиоактивен") : "", d.mass), C_TEXT); ty += fontU.h + uiPx(2);
            std::string chi = d.chi > 0 ? fmt("χ = %.2f", d.chi) : std::string(T("χ — нет"));
            drawText(fontU, tx, ty, chi + fmt(" · валентность %d · r_ков = %.2f Å%s", d.val, d.rcov, isMetalT(t) ? fmt(" · r_мет = %.2f Å", d.rsig).c_str() : ""), C_TEXT); ty += fontU.h + uiPx(2);
            {   // группа, период, электронная конфигурация; свойства простого вещества
                int grp, per; zGroupPeriod(z, grp, per);
                std::string gp = grp ? fmt("группа %d · период %d", grp, per) : fmt("период %d · f-элемент", per);
                drawText(fontU, tx, ty, gp + " · " + zElectronConfig(z), C_TEXT); ty += fontU.h + uiPx(2);
                const ZPhys& p = ZP[z]; const char* ap = zpEstimated(z) ? "≈" : "";
                auto tc = [&](double K) { return fmt("%s%.0f °C", ap, K - 273.15); };
                if (p.mp <= 0 && p.bp == 0 && p.ie <= 0) {
                    drawText(fontU, tx, ty, "свойства простого вещества не измерены: атомы живут секунды и меньше", C_DIM); ty += fontU.h + uiPx(2);
                } else {
                    std::string s;
                    if (p.bp < 0) s = fmt("возгонка %s (при 1 атм не плавится)", tc(-p.bp).c_str());
                    else {
                        s = p.mp > 0 ? fmt("t плавления %s", tc(p.mp).c_str()) : std::string(T("при 1 атм не твердеет"));
                        if (p.bp > 0) s += fmt(" · t кипения %s", tc(p.bp).c_str());
                    }
                    drawText(fontU, tx, ty, s, C_TEXT); ty += fontU.h + uiPx(2);
                    const double T20 = 293.15;
                    const char* st = p.bp < 0 ? "твёрдое" : (p.mp > 0 && T20 < p.mp) ? "твёрдое" : (p.bp > 0 && T20 < p.bp) ? "жидкость" : p.bp > 0 ? "газ" : "";
                    const bool gas = p.bp > 0 && T20 >= p.bp;
                    std::string s2;
                    if (p.rho > 0) s2 = gas ? fmt("ρ = %.4g г/л", p.rho * 1000) : fmt("ρ = %.4g г/см³", p.rho);
                    if (p.ie > 0) { if (!s2.empty()) s2 += " · "; s2 += fmt("E ионизации %s%.2f эВ", ap, p.ie); }
                    if (*st) { if (!s2.empty()) s2 += " · "; s2 += fmt("при 20 °C — %s", T(st)); }
                    drawText(fontU, tx, ty, s2, C_TEXT); ty += fontU.h + uiPx(2);
                }
            }
            if (ty + fontXS.h < cy + ch) drawWrapped(fontXS, tx, ty, cw - bs - uiPx(32), elementModelNote(t), C_DIM);
            popClip();
        }
    }
    {   // легенда категорий
        float lx = gx, ly = gy + 9.6f * cell + uiPx(8);
        for (int c = 0; c < CAT_SPECIAL; c++) {
            float w = textW(fontXS, CAT_NAMES[c]);
            if (lx + w + uiPx(34) > x0 + W - pad) { lx = gx; ly += fontXS.h + uiPx(3); }
            catStrip(lx, ly + uiPx(6), uiPx(20), uiPx(3), c); drawText(fontXS, lx + uiPx(25), ly, CAT_NAMES[c], C_TEXT); lx += w + uiPx(42);
        }
        if (lx + uiPx(120) > x0 + W - pad) { lx = gx; ly += fontXS.h + uiPx(3); }
        radioMark(lx + uiPx(9), ly + uiPx(2), uiPx(9), C_DIM); drawText(fontXS, lx + uiPx(14), ly, "радиоактивен", C_TEXT);
    }
    if (ui.pressed && !inPR(ptRect)) ptOn = false;   // щелчок мимо — закрыть
}
static void loadPreset(int k, int variant);
static void clearToolState() { selClear(); measN = 0; rubberOn = false; throwDrag = false; foPlacing = false; cutHoverA = cutHoverB = -1; boxTarget = -1; }
static void openScene(int k) {
    pushUndo(); bool same = k == currentPreset;
    loadPresetKeepObjs(k, same ? presetVariant + 1 : 0, same); viewFitPending = true; followAtom = -1; menuOn = false; ptOn = false;
    clearToolState();
}
// Меню сцен: разделы по SceneInfo.group (заголовки SG_NAMES), 3–4 колонки по ширине окна, компактные карточки
// (метка клавиши, название, 1–2 строки описания); высота карточек подстраивается, чтобы всё помещалось без прокрутки.
static void drawScenesMenu() {
    const int NS = (int)(sizeof(SCENES) / sizeof(SCENES[0]));
    const float pad = uiPx(18), gap = uiPx(6), secH = fontUB.h + uiPx(10), headH = uiPx(46);
    // раскладка: сначала 3 (широкое окно — 4) колонки с двумя строками описания; не помещается — на колонку больше,
    // затем одна строка описания (карточка никогда не обрезает строку посередине)
    int cols = 3, rows = 0, nsec = 0; float chh = 0;
    const float h2 = uiPx(20) + fontUB.h + 2 * fontXS.h, h1 = uiPx(18) + fontUB.h + fontXS.h;
    auto rowsFor = [&](int c) { int r = 0, n = 0; for (int g = 0; g < SG_N; g++) { int k = 0; for (int s = 0; s < NS; s++) if (SCENES[s].group == g) k++; if (k) { r += (k + c - 1) / c; n++; } } nsec = n; return r; };
    {
        const int c0 = winW >= uiPx(1500) ? 4 : 3;
        const int tryC[4] = {c0, c0 + 1, c0, c0 + 1}; const float tryH[4] = {h2, h2, h1, h1};
        for (int t = 0; t < 4; t++) {
            cols = tryC[t]; chh = tryH[t]; rows = rowsFor(cols);
            if (headH + nsec * secH + rows * (chh + gap) + uiPx(10) <= winH - uiPx(16)) break;
        }
    }
    float cw = std::floor(std::min(uiPx(330), (winW - uiPx(24) - 2 * pad - (cols - 1) * gap) / (float)cols));
    float W = cols * cw + (cols - 1) * gap + 2 * pad, H = headH + nsec * secH + rows * (chh + gap) + uiPx(10);
    float x0 = std::floor((winW - W) / 2), y0 = std::floor(std::max(uiPx(8), (winH - H) / 2));
    menuRect = {x0, y0, W, H};
    rectFill(0, 0, (float)winW, (float)winH, {0, 0, 0, 0.6f});
    boxPanel(x0, y0, W, H, C_PANEL, C_LINE_H); rectFill(x0, y0, W, uiPx(2), C_ACC);
    drawText(fontL, x0 + pad, y0 + uiPx(12), "Сцены", C_TEXT_HI);
    drawTextR(fontXS, x0 + W - pad, y0 + uiPx(17), "щелчок — открыть (повторный — вариант) · Esc / Tab — закрыть", C_DIM);
    std::vector<PR> rect(NS, PR{0, 0, 0, 0});
    {
        float yy = y0 + headH;
        for (int g = 0; g < SG_N; g++) {
            int c = 0;
            for (int s = 0; s < NS; s++) {
                if (SCENES[s].group != g) continue;
                if (c == 0) { drawText(fontUB, x0 + pad, yy + uiPx(2), SG_NAMES[g], C_DIM); lineH(x0 + pad + textW(fontUB, SG_NAMES[g]) + uiPx(10), x0 + W - pad, yy + uiPx(2) + fontUB.h / 2, C_LINE); yy += secH; }
                rect[s] = {x0 + pad + (c % cols) * (cw + gap), yy + (c / cols) * (chh + gap), cw, chh}; c++;
            }
            if (c) yy += ((c + cols - 1) / cols) * (chh + gap);
        }
    }
    for (int s = 0; s < NS; s++) {
        const SceneInfo& sc = SCENES[s];
        PR r = rect[s];
        bool hover = inPR(r), cur = sc.key == currentPreset; int id = sceneBtnId(sc.key);
        uiRects[id] = r;
        if (hover && ui.pressed) ui.active = id;
        bool clicked = ui.active == id && ui.released && hover;
        if (ui.active == id && ui.released) ui.active = -1;
        rectFill(r.x, r.y, r.w, r.h, cur ? C_ACC_BG : hover ? C_ELEM_H : C_ELEM);
        rectLine(r.x, r.y, r.w, r.h, cur ? withA(C_ACC, 0.8f) : hover ? C_LINE_H : C_LINE);
        if (cur) rectFill(r.x, r.y, uiPx(2), r.h, C_ACC);   // текущая сцена — акцентная полоса слева
        float kw = textW(fontXS, sc.keyLabel) + uiPx(10), ty = r.y + uiPx(7);
        rectLine(r.x + uiPx(8), ty, kw, fontUB.h + uiPx(1), sc.key >= 11 ? withA(C_WARN, 0.7f) : withA(C_ACC, 0.7f));
        drawText(fontXS, r.x + uiPx(13), ty + std::floor((fontUB.h - fontXS.h) / 2) + uiPx(1), sc.keyLabel, sc.key >= 11 ? C_WARN : C_ACC);
        pushClip(r.x, r.y, r.w - uiPx(6), r.h);
        drawText(fontUB, r.x + uiPx(15) + kw, ty, sc.title, C_TEXT_HI);
        pushClip(r.x, r.y, r.w, r.h - uiPx(3)); drawWrapped(fontXS, r.x + uiPx(10), ty + fontUB.h + uiPx(5), r.w - uiPx(20), sc.desc, C_DIM); popClip();
        popClip();
        if (clicked) { openScene(sc.key); return; }
    }
    if (ui.pressed && !inPR(menuRect)) menuOn = false;
}

// ===================================== ВВОД =============================================
struct Input { bool lDown = false, rDown = false, mDown = false; bool lPress = false, lRel = false, mPress = false, rPress = false, dbl = false; int wheel = 0; std::vector<int> keys; } in;
static bool lInScene = false, panning = false, rotating = false; static int dragX = 0, dragY = 0; static double lastAddT = 0;
static bool wallStroke = false; static double lastWallX = 1e9, lastWallY = 1e9, lastWallZ = 1e9; static double grabDepth = 0;

// палитра веществ и таблицы пар (после смены параметров элементов или загрузки файла)
static void rebuildTables() {
    initPalette(); selPal = clampv(selPal, 0, (int)palette.size() - 1);
    buildPairTables(); nlValid = false; trailN = -1;
}
// точка «посередине ящика» на луче под курсором
static bool cursorPoint(double& x, double& y, double& z) {
    double o[3], d[3]; mouseRay(mouseX, mouseY, o, d);
    double t0, t1; if (!rayBox(o, d, t0, t1)) { unprojectAtDepth(mouseX, mouseY, cam3.dist, x, y, z); return false; }
    t0 = std::max(t0, 0.0); double t = 0.5 * (t0 + t1);
    x = o[0] + d[0] * t; y = o[1] + d[1] * t; z = o[2] + d[2] * t; return true;
}

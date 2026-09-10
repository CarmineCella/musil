// plot.h — plotting, C++ half: renders a figure description to a raylib texture or a PNG.
// The Musil half (plot.mu) builds the descriptions. Needs raylib.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// A figure is a list: (list title layers options) where
//   layers  is a list of (list kind data... label): kinds are "line" x y label,
//           "scatter" x y label, "bars" x y label, "image" matrix, "surface" matrix
//   options is a list of (list key value): "xlabel" "ylabel" "xmin" "xmax" "ymin" "ymax" "colormap"
// The hosts (the CLI and the Listener) register `show` and `save-png` on top of
// render_figure, because only the thread that owns the window may draw.

#pragma once
#include "core.h"
#include "raylib.h"
#include "rlgl.h"
#include <cstdio>

namespace musil {

// --- figure description, validated once ---
struct plot_layer { std::string kind, label; varr x, y; std::vector<varr> m; };
struct figure {
    std::string title, xlabel, ylabel;
    std::vector<plot_layer> layers;
    bool has_xmin = false, has_xmax = false, has_ymin = false, has_ymax = false;
    double xmin = 0, xmax = 0, ymin = 0, ymax = 0;
};
[[noreturn]] inline void plot_fail(const std::string& m) { throw std::runtime_error("figure: " + m); }
inline const varr& plot_num(const vptr& v) { if (!v || v->t != Value::NUM) plot_fail("expected a numeric vector"); return v->num; }
inline figure parse_figure(const vptr& v) {
    if (!v || v->t != Value::LIST || v->l.size() < 3) plot_fail("expected (list title layers options)");
    figure f;
    f.title = str_of(v->l[0]);
    if (v->l[1]->t != Value::LIST) plot_fail("layers must be a list");
    for (auto& L : v->l[1]->l) {
        if (L->t != Value::LIST || L->l.empty() || L->l[0]->t != Value::STR) plot_fail("a layer is (list kind data...)");
        plot_layer p; p.kind = L->l[0]->s;
        if (p.kind == "line" || p.kind == "scatter" || p.kind == "bars") {
            if (L->l.size() < 3) plot_fail(p.kind + ": needs x and y");
            p.x = plot_num(L->l[1]); p.y = plot_num(L->l[2]);
            if (p.x.size() != p.y.size()) plot_fail(p.kind + ": x and y must have the same length");
            if (L->l.size() > 3) p.label = str_of(L->l[3]);
        } else if (p.kind == "image" || p.kind == "surface") {
            if (L->l.size() < 2 || L->l[1]->t != Value::LIST || L->l[1]->l.empty()) plot_fail(p.kind + ": needs a matrix (list of rows)");
            size_t c = 0;
            for (auto& r : L->l[1]->l) { const varr& row = plot_num(r); if (c == 0) c = row.size(); if (row.size() != c || c == 0) plot_fail(p.kind + ": ragged or empty matrix"); p.m.push_back(row); }
            if (L->l.size() > 2) p.label = str_of(L->l[2]);
        } else plot_fail("unknown layer kind " + p.kind);
        f.layers.push_back(std::move(p));
    }
    if (v->l[2]->t != Value::LIST) plot_fail("options must be a list");
    for (auto& o : v->l[2]->l) {
        if (o->t != Value::LIST || o->l.size() != 2 || o->l[0]->t != Value::STR) plot_fail("an option is (list key value)");
        const std::string& k = o->l[0]->s; const vptr& val = o->l[1];
        auto num = [&]() { if (val->t != Value::NUM || val->num.size() != 1) plot_fail(k + ": expected a number"); return val->num[0]; };
        if (k == "xlabel") f.xlabel = str_of(val); else if (k == "ylabel") f.ylabel = str_of(val);
        else if (k == "xmin") { f.xmin = num(); f.has_xmin = true; } else if (k == "xmax") { f.xmax = num(); f.has_xmax = true; }
        else if (k == "ymin") { f.ymin = num(); f.has_ymin = true; } else if (k == "ymax") { f.ymax = num(); f.has_ymax = true; }
        else plot_fail("unknown option " + k);
    }
    if (f.layers.empty()) plot_fail("no layers");
    return f;
}

inline Color plot_palette(size_t k) {
    static const Color c[] = { {31,119,180,255}, {255,127,14,255}, {44,160,44,255}, {214,39,40,255}, {148,103,189,255}, {140,86,75,255}, {227,119,194,255}, {127,127,127,255} };
    return c[k % 8];
}
inline Color plot_colormap(double t) {   // viridis-like, t in [0,1]
    static const unsigned char pts[][3] = { {68,1,84}, {59,82,139}, {33,145,140}, {94,201,98}, {253,231,37} };
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    double p = t * 4; int i = (int)p; if (i >= 4) i = 3; double f = p - i;
    return { (unsigned char)(pts[i][0] + f * (pts[i+1][0] - pts[i][0])), (unsigned char)(pts[i][1] + f * (pts[i+1][1] - pts[i][1])), (unsigned char)(pts[i][2] + f * (pts[i+1][2] - pts[i][2])), 255 };
}
// --- interaction state, owned by the host and passed to render/interact ---
struct plot_view {
    bool zoomed = false; double xmin = 0, xmax = 1, ymin = 0, ymax = 1;   // current 2D view when zoomed
    float angle = 0.9f, pitch = 0.6f;                                      // surface camera
    bool has_cursor = false; double cx = 0, cy = 0;                         // data coordinates under the mouse
    bool has_pick = false; double px = 0, py = 0; std::string pick_label;   // nearest data point
    int hover_x = -1, hover_y = -1;                                          // mouse in figure pixels
};
// 2D data range of a figure (before the view is applied)
inline void figure_range(const figure& f, double& xmin, double& xmax, double& ymin, double& ymax) {
    xmin = 1e300; xmax = -1e300; ymin = 1e300; ymax = -1e300; bool image = false;
    for (auto& L : f.layers) {
        if (L.kind == "image") { image = true; xmin = std::min(xmin, 0.0); xmax = std::max(xmax, (double)L.m[0].size()); ymin = std::min(ymin, 0.0); ymax = std::max(ymax, (double)L.m.size()); continue; }
        if (L.kind == "surface") continue;
        for (size_t k = 0; k < L.x.size(); k++) { xmin = std::min(xmin, L.x[k]); xmax = std::max(xmax, L.x[k]); ymin = std::min(ymin, L.y[k]); ymax = std::max(ymax, L.y[k]); }
        if (L.kind == "bars") { ymin = std::min(ymin, 0.0); if (L.x.size() > 1) { double bw = L.x[1] - L.x[0]; xmin = std::min(xmin, L.x[0] - bw / 2); xmax = std::max(xmax, L.x[L.x.size()-1] + bw / 2); } }
    }
    if (xmin > xmax) { xmin = 0; xmax = 1; }
    if (ymin > ymax) { ymin = 0; ymax = 1; }
    if (xmax == xmin) { xmin -= 1; xmax += 1; }
    if (ymax == ymin) { ymin -= 1; ymax += 1; }
    if (!image) { double dy = (ymax - ymin) * 0.05; ymin -= dy; ymax += dy; }
    if (f.has_xmin) xmin = f.xmin;
    if (f.has_xmax) xmax = f.xmax;
    if (f.has_ymin) ymin = f.ymin;
    if (f.has_ymax) ymax = f.ymax;
}
// image layers as textures, built once per figure (drawing them every frame was the slow part)
struct plot_image_cache { const std::vector<varr>* key = nullptr; Texture2D tex = {}; };
inline std::vector<plot_image_cache>& plot_images() { static std::vector<plot_image_cache> c; return c; }
inline Texture2D plot_image_texture(const plot_layer& L) {
    for (auto& c : plot_images()) if (c.key == &L.m) return c.tex;
    double lo = 1e300, hi = -1e300; for (auto& r : L.m) for (double v : r) { lo = std::min(lo, v); hi = std::max(hi, v); }
    if (hi <= lo) hi = lo + 1;
    size_t rows = L.m.size(), cols = L.m[0].size();
    Image img = GenImageColor((int)cols, (int)rows, BLACK);
    for (size_t r = 0; r < rows; r++) for (size_t c = 0; c < cols; c++) ImageDrawPixel(&img, (int)c, (int)(rows - 1 - r), plot_colormap((L.m[r][c] - lo) / (hi - lo)));
    Texture2D tex = LoadTextureFromImage(img); UnloadImage(img);
    plot_images().push_back({ &L.m, tex });
    return tex;
}
inline void plot_cache_clear() { for (auto& c : plot_images()) UnloadTexture(c.tex); plot_images().clear(); }

inline double nice_step(double range, int target) {
    if (range <= 0) return 1;
    double raw = range / target, mag = std::pow(10, std::floor(std::log10(raw))), r = raw / mag;
    return (r < 1.5 ? 1 : r < 3.5 ? 2 : r < 7.5 ? 5 : 10) * mag;
}
inline std::string tick_label(double v) { char b[32]; std::snprintf(b, sizeof b, "%.6g", std::fabs(v) < 1e-12 ? 0.0 : v); return b; }

struct plot_font { Font font; bool custom = false; int size = 14; };
inline plot_font& plot_get_font() { static plot_font f; if (!f.custom) f.font = GetFontDefault(); return f; }
inline void plot_text(const std::string& s, float x, float y, int size, Color c) { DrawTextEx(plot_get_font().font, s.c_str(), { x, y }, (float)size, 1, c); }
inline float plot_text_width(const std::string& s, int size) { return MeasureTextEx(plot_get_font().font, s.c_str(), (float)size, 1).x; }

// 2D layers (line, scatter, bars, image) into the current drawing target of size w x h
inline void render_2d(const figure& f, int w, int h, const plot_view& view) {
    ClearBackground({ 250, 250, 250, 255 });
    const int fs = 14; int left = 64, right = 20, top = f.title.empty() ? 16 : 34, bottom = 44;
    float pw = (float)(w - left - right), ph = (float)(h - top - bottom);
    double xmin, xmax, ymin, ymax; figure_range(f, xmin, xmax, ymin, ymax);
    if (view.zoomed) { xmin = view.xmin; xmax = view.xmax; ymin = view.ymin; ymax = view.ymax; }
    auto X = [&](double x) { return left + (float)((x - xmin) / (xmax - xmin) * pw); };
    auto Y = [&](double y) { return top + ph - (float)((y - ymin) / (ymax - ymin) * ph); };
    if (!f.title.empty()) plot_text(f.title, left + (pw - plot_text_width(f.title, fs + 2)) / 2, 8, fs + 2, { 40, 40, 40, 255 });
    DrawRectangleLines(left, top, (int)pw, (int)ph, { 120, 120, 120, 255 });
    double sx = nice_step(xmax - xmin, 6), sy = nice_step(ymax - ymin, 5);
    for (double t = std::ceil(xmin / sx) * sx; t <= xmax + 1e-9 * sx; t += sx) {
        float px = X(t); DrawLine((int)px, top, (int)px, top + (int)ph, { 225, 225, 225, 255 });
        std::string ss = tick_label(t); plot_text(ss, px - plot_text_width(ss, fs - 2) / 2, top + ph + 4, fs - 2, { 80, 80, 80, 255 });
    }
    for (double t = std::ceil(ymin / sy) * sy; t <= ymax + 1e-9 * sy; t += sy) {
        float py = Y(t); DrawLine(left, (int)py, left + (int)pw, (int)py, { 225, 225, 225, 255 });
        std::string ss = tick_label(t); plot_text(ss, left - 6 - plot_text_width(ss, fs - 2), py - (fs - 2) / 2.0f, fs - 2, { 80, 80, 80, 255 });
    }
    if (!f.xlabel.empty()) plot_text(f.xlabel, left + (pw - plot_text_width(f.xlabel, fs)) / 2, h - fs - 6, fs, { 60, 60, 60, 255 });
    if (!f.ylabel.empty()) { rlPushMatrix(); rlTranslatef(14, top + ph / 2 + plot_text_width(f.ylabel, fs) / 2, 0); rlRotatef(-90, 0, 0, 1); plot_text(f.ylabel, 0, 0, fs, { 60, 60, 60, 255 }); rlPopMatrix(); }
    BeginScissorMode(left, top, (int)pw, (int)ph);
    size_t ci = 0; std::vector<std::pair<std::string, Color>> legend;
    for (auto& L : f.layers) {
        if (L.kind == "surface") continue;
        if (L.kind == "image") {
            Texture2D tex = plot_image_texture(L);
            size_t rows = L.m.size(), cols = L.m[0].size();
            Rectangle dst = { X(0), Y((double)rows), X((double)cols) - X(0), Y(0) - Y((double)rows) };
            DrawTexturePro(tex, { 0, 0, (float)cols, (float)rows }, dst, { 0, 0 }, 0, WHITE);
            continue;
        }
        Color col = plot_palette(ci++);
        if (!L.label.empty()) legend.push_back({ L.label, col });
        if (L.kind == "line") {
            // when there are more points than pixels, draw the min/max envelope of each pixel column
            size_t n = L.x.size();
            if (n > (size_t)pw * 2 && n > 1) {
                double per = (double)n / pw; size_t k = 0;
                while (k < n) {
                    size_t e = std::min(n, k + (size_t)per + 1); double lo = 1e300, hi = -1e300, x0 = L.x[k];
                    for (size_t j = k; j < e; j++) { lo = std::min(lo, L.y[j]); hi = std::max(hi, L.y[j]); }
                    DrawLineEx({ X(x0), Y(lo) }, { X(x0), Y(hi) }, 1.5f, col);
                    if (e < n) DrawLineEx({ X(L.x[e-1]), Y(L.y[e-1]) }, { X(L.x[e]), Y(L.y[e]) }, 1.5f, col);
                    k = e;
                }
            } else for (size_t k = 1; k < n; k++) DrawLineEx({ X(L.x[k-1]), Y(L.y[k-1]) }, { X(L.x[k]), Y(L.y[k]) }, 1.5f, col);
        }
        else if (L.kind == "scatter") for (size_t k = 0; k < L.x.size(); k++) DrawCircleV({ X(L.x[k]), Y(L.y[k]) }, 3.5f, col);
        else if (L.kind == "bars") {
            double bw = L.x.size() > 1 ? (L.x[1] - L.x[0]) * 0.8 : (xmax - xmin) * 0.5;
            for (size_t k = 0; k < L.x.size(); k++) { float x0 = X(L.x[k] - bw / 2), x1 = X(L.x[k] + bw / 2), y0 = Y(0), y1 = Y(L.y[k]); DrawRectangle((int)x0, (int)std::min(y0, y1), (int)(x1 - x0), (int)std::fabs(y1 - y0), col); }
        }
    }
    if (view.has_pick) { DrawCircleLinesV({ X(view.px), Y(view.py) }, 6, { 200, 40, 40, 255 }); }
    EndScissorMode();
    for (size_t k = 0; k < legend.size(); k++) {
        float y = top + 8 + k * (fs + 4), x = left + pw - 12 - plot_text_width(legend[k].first, fs) - 22;
        DrawRectangle((int)x, (int)y + 3, 14, fs - 6, legend[k].second); plot_text(legend[k].first, x + 20, y, fs, { 40, 40, 40, 255 });
    }
    if (view.has_cursor) {
        std::string ss = "x = " + tick_label(view.cx) + "   y = " + tick_label(view.cy);
        if (view.has_pick) ss += "   nearest: (" + tick_label(view.px) + ", " + tick_label(view.py) + ")" + (view.pick_label.empty() ? "" : " " + view.pick_label);
        float tw = plot_text_width(ss, fs - 1) + 12;
        DrawRectangle(left + 4, top + 4, (int)tw, fs + 6, { 255, 255, 255, 220 });
        plot_text(ss, left + 10, top + 7, fs - 1, { 40, 40, 40, 255 });
    }
    if (view.zoomed) plot_text("zoomed: r resets", (float)w - 130, (float)h - fs - 6, fs - 2, { 130, 130, 130, 255 });
}
// Mouse and keys over a figure drawn at rect: wheel zooms around the cursor, drag pans, r resets,
// the cursor's data coordinates and the nearest point are stored in the view (in figure pixels)
inline void plot_interact(const figure& f, const plot_view& in, plot_view& view, Rectangle rect, Vector2 mouse) {
    view = in;
    const int fs = 14; int left = 64, right = 20, top = f.title.empty() ? 16 : 34, bottom = 44;
    (void)fs;
    float pw = rect.width - left - right, ph = rect.height - top - bottom;
    bool surface = false; for (auto& L : f.layers) if (L.kind == "surface") surface = true;
    bool inside = CheckCollisionPointRec(mouse, rect);
    if (surface) {
        if (inside && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { view.angle += GetMouseDelta().x * 0.01f; view.pitch = std::max(0.05f, std::min(1.5f, view.pitch - GetMouseDelta().y * 0.01f)); }
        if (IsKeyDown(KEY_LEFT)) view.angle -= 0.03f;
        if (IsKeyDown(KEY_RIGHT)) view.angle += 0.03f;
        return;
    }
    double xmin, xmax, ymin, ymax; figure_range(f, xmin, xmax, ymin, ymax);
    if (view.zoomed) { xmin = view.xmin; xmax = view.xmax; ymin = view.ymin; ymax = view.ymax; }
    auto dx = [&](float px) { return xmin + (px - rect.x - left) / pw * (xmax - xmin); };
    auto dy = [&](float py) { return ymin + (rect.y + top + ph - py) / ph * (ymax - ymin); };
    view.has_cursor = inside && mouse.x >= rect.x + left && mouse.x <= rect.x + left + pw && mouse.y >= rect.y + top && mouse.y <= rect.y + top + ph;
    view.has_pick = false;
    if (view.has_cursor) {
        view.cx = dx(mouse.x); view.cy = dy(mouse.y);
        double best = 1e300; size_t ci = 0;
        for (auto& L : f.layers) {
            if (L.kind == "image" || L.kind == "surface") continue;
            for (size_t k = 0; k < L.x.size(); k++) {
                double ex = (L.x[k] - view.cx) / (xmax - xmin) * pw, ey = (L.y[k] - view.cy) / (ymax - ymin) * ph, d = ex * ex + ey * ey;
                if (d < best && d < 20 * 20) { best = d; view.has_pick = true; view.px = L.x[k]; view.py = L.y[k]; view.pick_label = L.label.empty() ? "layer " + std::to_string(ci) : L.label; }
            }
            ci++;
        }
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            double zf = wheel > 0 ? 0.8 : 1.25, cx = view.cx, cy = view.cy;
            view.xmin = cx - (cx - xmin) * zf; view.xmax = cx + (xmax - cx) * zf; view.ymin = cy - (cy - ymin) * zf; view.ymax = cy + (ymax - cy) * zf; view.zoomed = true;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 d = GetMouseDelta(); double ddx = -d.x / pw * (xmax - xmin), ddy = d.y / ph * (ymax - ymin);
            if (d.x != 0 || d.y != 0) { view.xmin = xmin + ddx; view.xmax = xmax + ddx; view.ymin = ymin + ddy; view.ymax = ymax + ddy; view.zoomed = true; }
        }
    }
    if (IsKeyPressed(KEY_R)) view.zoomed = false;
}

// 3D surface: z = m[r][c] over a unit square, coloured by height, seen from an orbiting camera
inline void render_surface(const figure& f, const plot_layer& L, int w, int h, const plot_view& view) {
    float angle = view.angle, pitch = view.pitch;
    ClearBackground({ 250, 250, 250, 255 });
    if (!f.title.empty()) plot_text(f.title, (w - plot_text_width(f.title, 16)) / 2, 8, 16, { 40, 40, 40, 255 });
    size_t rows = L.m.size(), cols = L.m[0].size();
    double lo = 1e300, hi = -1e300; for (auto& r : L.m) for (double v : r) { lo = std::min(lo, v); hi = std::max(hi, v); }
    if (hi <= lo) hi = lo + 1;
    float dist = 2.6f;
    Camera3D cam = { { dist * std::cos(angle) * std::cos(pitch), dist * std::sin(pitch), dist * std::sin(angle) * std::cos(pitch) }, { 0, 0.25f, 0 }, { 0, 1, 0 }, 40, CAMERA_PERSPECTIVE };
    BeginMode3D(cam);
    rlDisableBackfaceCulling();
    // large matrices are drawn at reduced resolution (block averages), which keeps rotation fluid
    size_t step_r = std::max<size_t>(1, rows / 160), step_c = std::max<size_t>(1, cols / 160);
    size_t R = (rows + step_r - 1) / step_r, C = (cols + step_c - 1) / step_c;
    auto at = [&](size_t rr, size_t cc) { double s = 0; size_t n = 0; for (size_t r = rr * step_r; r < std::min(rows, (rr + 1) * step_r); r++) for (size_t c = cc * step_c; c < std::min(cols, (cc + 1) * step_c); c++) { s += L.m[r][c]; n++; } return s / n; };
    std::vector<double> z(R * C); for (size_t r = 0; r < R; r++) for (size_t c = 0; c < C; c++) z[r * C + c] = at(r, c);
    auto P = [&](size_t r, size_t c) { return Vector3{ (float)c / std::max<size_t>(1, C - 1) - 0.5f, (float)((z[r * C + c] - lo) / (hi - lo)) * 0.8f, (float)r / std::max<size_t>(1, R - 1) - 0.5f }; };
    rlBegin(RL_TRIANGLES);
    for (size_t r = 0; r + 1 < R; r++) for (size_t c = 0; c + 1 < C; c++) {
        Vector3 a = P(r, c), b = P(r, c + 1), d = P(r + 1, c), e = P(r + 1, c + 1);
        auto col = [&](const Vector3& v) { Color k = plot_colormap(v.y / 0.8f); rlColor4ub(k.r, k.g, k.b, 255); };
        col(a); rlVertex3f(a.x, a.y, a.z); col(d); rlVertex3f(d.x, d.y, d.z); col(b); rlVertex3f(b.x, b.y, b.z);
        col(b); rlVertex3f(b.x, b.y, b.z); col(d); rlVertex3f(d.x, d.y, d.z); col(e); rlVertex3f(e.x, e.y, e.z);
    }
    rlEnd();
    rlEnableBackfaceCulling();
    Color wire = { 60, 60, 60, 110 };
    size_t rs = std::max<size_t>(1, R / 24), cs = std::max<size_t>(1, C / 24);
    for (size_t r = 0; r < R; r += rs) for (size_t c = 0; c + 1 < C; c++) DrawLine3D(P(r, c), P(r, c + 1), wire);
    for (size_t c = 0; c < C; c += cs) for (size_t r = 0; r + 1 < R; r++) DrawLine3D(P(r, c), P(r + 1, c), wire);
    DrawLine3D({ -0.5f, 0, -0.5f }, { 0.5f, 0, -0.5f }, GRAY); DrawLine3D({ -0.5f, 0, -0.5f }, { -0.5f, 0, 0.5f }, GRAY);
    DrawLine3D({ -0.5f, 0, -0.5f }, { -0.5f, 0.8f, -0.5f }, GRAY);
    EndMode3D();
    plot_text((f.xlabel.empty() ? "columns" : f.xlabel) + "  (drag to orbit)", 12, (float)h - 22, 13, { 90, 90, 90, 255 });
    plot_text(tick_label(lo) + " .. " + tick_label(hi), (float)w - 140, (float)h - 22, 13, { 90, 90, 90, 255 });
}

// Draw the figure into the current target.
inline void render_figure(const figure& f, int w, int h, const plot_view& view = plot_view()) {
    for (auto& L : f.layers) if (L.kind == "surface") { render_surface(f, L, w, h, view); return; }
    render_2d(f, w, h, view);
}
// A file name for a saved plot: musil_plot_<title or n>.png in the current directory
inline std::string plot_save_name(const figure& f) {
    static int n = 0; std::string base = f.title.empty() ? "plot" : f.title;
    for (auto& c : base) if (!std::isalnum((unsigned char)c)) c = '_';
    return "musil_" + base + "_" + std::to_string(++n) + ".png";
}
// Render to a PNG file. The caller must own a window (hidden is fine).
inline void figure_to_png(const figure& f, const std::string& path, int w, int h) {
    RenderTexture2D rt = LoadRenderTexture(w, h);
    BeginTextureMode(rt); render_figure(f, w, h); EndTextureMode();
    plot_cache_clear();                       // textures were keyed by this figure's layers
    Image img = LoadImageFromTexture(rt.texture); ImageFlipVertical(&img);
    bool ok = ExportImage(img, path.c_str());
    UnloadImage(img); UnloadRenderTexture(rt);
    if (!ok) throw std::runtime_error("save-png: cannot write " + path);
}

// --- a minimal host: the CLI. show = window until closed or Esc; save-png = hidden window ---
inline void plot_ensure_window(bool visible, int w, int h) {
    if (!IsWindowReady()) { SetTraceLogLevel(LOG_WARNING); if (!visible) SetConfigFlags(FLAG_WINDOW_HIDDEN); InitWindow(w, h, "musil"); if (!IsWindowReady()) throw std::runtime_error("plot: cannot open a window (no display?)"); }
    else { if (visible && IsWindowHidden()) ClearWindowState(FLAG_WINDOW_HIDDEN); SetWindowSize(w, h); }
}
// (save-png fig path [w h]) render a figure to a PNG file (900 x 560 by default)
inline vptr cli_save_png(vlist& a, Interp& i) {
    figure f; try { f = parse_figure(a[0]); } catch (std::exception& e) { i.bad(e.what()); }
    int w = a.size() > 2 ? (int)i.scalar(a[2]) : 900, h = a.size() > 3 ? (int)i.scalar(a[3]) : 560;
    try { plot_ensure_window(false, 64, 64); figure_to_png(f, i.str(a[1]), w, h); } catch (std::exception& e) { i.bad(e.what()); }
    return v_nil();
}
// (show fig [w h]) show a figure: a window in the CLI (Esc closes; none when MUSIL_NOSHOW is set), a panel in the Listener
inline vptr cli_show(vlist& a, Interp& i) {
    figure f; try { f = parse_figure(a[0]); } catch (std::exception& e) { i.bad(e.what()); }
    if (std::getenv("MUSIL_NOSHOW")) return v_nil();     // tests and batch runs
    int w = a.size() > 1 ? (int)i.scalar(a[1]) : 900, h = a.size() > 2 ? (int)i.scalar(a[2]) : 560;
    try { plot_ensure_window(true, w, h); } catch (std::exception& e) { i.bad(e.what()); }
    SetWindowTitle(f.title.empty() ? "musil  (wheel zooms, drag pans, r resets, s saves, Esc closes)" : (f.title + "  (wheel zooms, drag pans, r resets, s saves, Esc closes)").c_str());
    MaximizeWindow();
    plot_view view; plot_cache_clear();
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) break;
        int W = GetScreenWidth(), H = GetScreenHeight();
        plot_interact(f, view, view, { 0, 0, (float)W, (float)H }, GetMousePosition());
        if (IsKeyPressed(KEY_S)) { std::string p = plot_save_name(f); try { figure_to_png(f, p, W, H); *i.out << "saved " << p << "\n" << std::flush; } catch (std::exception& e) { *i.out << e.what() << "\n"; } }
        BeginDrawing(); render_figure(f, W, H, view); EndDrawing();
    }
    plot_cache_clear();
    if (WindowShouldClose()) CloseWindow();
    return v_nil();
}
inline void add_plot(Interp& i) {   // the CLI host; the Listener registers its own show/save-png
    i.def("save-png", cli_save_png, 2, 4);
    i.def("show", cli_show, 1, 3);
}

} // namespace musil

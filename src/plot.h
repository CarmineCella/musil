// plot.h — plotting, C++ half: figures drawn with FLTK, in windows of their own or to PNG.
// The Musil half (plot.mu) builds the figure descriptions.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// A figure is a list: (list title layers options) where
//   layers  is a list of (list kind data... label): kinds are "line" x y label,
//           "scatter" x y label, "bars" x y label, "image" matrix, "surface" matrix,
//           or "subplot" figure (a figure made only of subplots is drawn as a grid)
//   options is a list of (list key value): "xlabel" "ylabel" "xmin" "xmax" "ymin" "ymax" "rows" "cols"
//
// show opens a window and returns: the interpreter's idle hook keeps every open window
// alive (Fl::check), so several plots can be open while the program goes on, from the
// command line as from the IDE. In the IDE, whose interpreter runs on a worker thread,
// windows are created on the FLTK thread through Fl::awake. save-png renders offscreen.
// Keys in a window: + - zoom, W A S D pan, arrows orbit a surface, 0 or r reset,
// e exports a PNG, Esc or q closes; the mouse drags to pan or orbit, the wheel zooms.

#pragma once
#include "core.h"
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>
#include <FL/platform.H>
#include <cstdio>
#include <set>

namespace musil {

// --- figure description, validated once ---
struct figure;
struct plot_layer { std::string kind, label; varr x, y; std::vector<varr> m; std::shared_ptr<figure> sub; };
struct figure {
    std::string title, xlabel, ylabel;
    std::vector<plot_layer> layers;
    bool has_xmin = false, has_xmax = false, has_ymin = false, has_ymax = false;
    double xmin = 0, xmax = 0, ymin = 0, ymax = 0;
    int rows = 0, cols = 0;                 // for a figure made of subplots
    bool is_grid() const { return !layers.empty() && layers[0].kind == "subplot"; }
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
        } else if (p.kind == "subplot") {
            if (L->l.size() < 2) plot_fail("subplot: needs a figure");
            p.sub = std::make_shared<figure>(parse_figure(L->l[1]));
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
        else if (k == "rows") f.rows = (int)num(); else if (k == "cols") f.cols = (int)num();
        else plot_fail("unknown option " + k);
    }
    if (f.layers.empty()) plot_fail("no layers");
    if (f.is_grid()) for (auto& L : f.layers) if (L.kind != "subplot") plot_fail("a figure of subplots holds only subplots");
    return f;
}


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
inline double nice_step(double range, int target) {
    if (range <= 0) return 1;
    double raw = range / target, mag = std::pow(10, std::floor(std::log10(raw))), r = raw / mag;
    return (r < 1.5 ? 1 : r < 3.5 ? 2 : r < 7.5 ? 5 : 10) * mag;
}
inline std::string tick_label(double v) { char b[32]; std::snprintf(b, sizeof b, "%.6g", std::fabs(v) < 1e-12 ? 0.0 : v); return b; }
inline std::string plot_save_name(const figure& f) {
    static int n = 0; std::string base = f.title.empty() ? "plot" : f.title;
    for (auto& c : base) if (!std::isalnum((unsigned char)c)) c = '_';
    std::string name = "musil_" + base + "_" + std::to_string(++n) + ".png";
    std::error_code ec; fs::path dir = fs::current_path(ec);
    auto writable = [](const fs::path& d) { std::ofstream t(d / ".musil_write_test"); bool ok = (bool)t; t.close(); std::error_code e; fs::remove(d / ".musil_write_test", e); return ok; };
    if (ec || dir == "/" || !writable(dir)) { const char* home = std::getenv("HOME"); dir = home ? fs::path(home) : fs::temp_directory_path(); }
    return (dir / name).string();
}

// --- colours ---
struct rgb { unsigned char r, g, b; };
inline rgb plot_palette(size_t k) {
    static const rgb c[] = { {31,119,180}, {255,127,14}, {44,160,44}, {214,39,40}, {148,103,189}, {140,86,75}, {227,119,194}, {127,127,127} };
    return c[k % 8];
}
inline rgb plot_colormap(double t) {   // viridis-like, t in [0,1]
    static const unsigned char pts[][3] = { {68,1,84}, {59,82,139}, {33,145,140}, {94,201,98}, {253,231,37} };
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    double p = t * 4; int i = (int)p; if (i >= 4) i = 3; double f = p - i;
    return { (unsigned char)(pts[i][0] + f * (pts[i+1][0] - pts[i][0])), (unsigned char)(pts[i][1] + f * (pts[i+1][1] - pts[i][1])), (unsigned char)(pts[i][2] + f * (pts[i+1][2] - pts[i][2])) };
}
inline void plot_color(rgb c) { fl_color(c.r, c.g, c.b); }

// --- the view: zoom/pan/orbit state, and what the cursor is over ---
struct plot_view {
    bool zoomed = false; double xmin = 0, xmax = 1, ymin = 0, ymax = 1;
    float angle = 0.9f, pitch = 0.6f, dist = 2.6f, tx = 0, ty = 0, tz = 0;
    bool has_cursor = false; double cx = 0, cy = 0;
    bool has_pick = false; double px = 0, py = 0; std::string pick_label;
};

// --- 2D drawing into the rectangle (ox, oy, w, h) of the current FLTK drawing surface ---
inline void render_2d(const figure& f, int ox, int oy, int w, int h, const plot_view& view) {
    fl_color(250, 250, 250); fl_rectf(ox, oy, w, h);
    const int fs = 13; int left = ox + 64, right = 20, top = oy + (f.title.empty() ? 16 : 34), bottom = 44;
    double pw = w - 64 - right, ph = h - (f.title.empty() ? 16 : 34) - bottom;
    double xmin, xmax, ymin, ymax; figure_range(f, xmin, xmax, ymin, ymax);
    if (view.zoomed) { xmin = view.xmin; xmax = view.xmax; ymin = view.ymin; ymax = view.ymax; }
    auto X = [&](double x) { return left + (x - xmin) / (xmax - xmin) * pw; };
    auto Y = [&](double y) { return top + ph - (y - ymin) / (ymax - ymin) * ph; };
    fl_font(FL_HELVETICA, fs);
    if (!f.title.empty()) { fl_font(FL_HELVETICA_BOLD, fs + 1); fl_color(40, 40, 40); fl_draw(f.title.c_str(), left + (int)((pw - fl_width(f.title.c_str())) / 2), oy + 8 + fs); fl_font(FL_HELVETICA, fs); }
    double sx = nice_step(xmax - xmin, 6), sy = nice_step(ymax - ymin, 5);
    fl_font(FL_HELVETICA, fs - 2);
    for (double t = std::ceil(xmin / sx) * sx; t <= xmax + 1e-9 * sx; t += sx) {
        int px = (int)X(t); fl_color(225, 225, 225); fl_line(px, top, px, top + (int)ph);
        std::string s = tick_label(t); fl_color(80, 80, 80); fl_draw(s.c_str(), px - (int)(fl_width(s.c_str()) / 2), top + (int)ph + fs + 2);
    }
    for (double t = std::ceil(ymin / sy) * sy; t <= ymax + 1e-9 * sy; t += sy) {
        int py = (int)Y(t); fl_color(225, 225, 225); fl_line(left, py, left + (int)pw, py);
        std::string s = tick_label(t); fl_color(80, 80, 80); fl_draw(s.c_str(), left - 6 - (int)fl_width(s.c_str()), py + (fs - 2) / 2);
    }
    fl_color(120, 120, 120); fl_rect(left, top, (int)pw, (int)ph);
    fl_font(FL_HELVETICA, fs); fl_color(60, 60, 60);
    if (!f.xlabel.empty()) fl_draw(f.xlabel.c_str(), left + (int)((pw - fl_width(f.xlabel.c_str())) / 2), oy + h - 6);
    if (!f.ylabel.empty()) fl_draw(90, f.ylabel.c_str(), ox + 14, top + (int)(ph / 2 + fl_width(f.ylabel.c_str()) / 2));
    fl_push_clip(left, top, (int)pw, (int)ph);
    size_t ci = 0; std::vector<std::pair<std::string, rgb>> legend;
    for (auto& L : f.layers) {
        if (L.kind == "surface") continue;
        if (L.kind == "image") {
            double lo = 1e300, hi = -1e300; for (auto& r : L.m) for (double v : r) { lo = std::min(lo, v); hi = std::max(hi, v); }
            if (hi <= lo) hi = lo + 1;
            int rows = (int)L.m.size(), cols = (int)L.m[0].size();
            std::vector<unsigned char> pix((size_t)rows * cols * 3);
            for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) { rgb k = plot_colormap((L.m[r][c] - lo) / (hi - lo)); size_t p = ((size_t)(rows - 1 - r) * cols + c) * 3; pix[p] = k.r; pix[p + 1] = k.g; pix[p + 2] = k.b; }
            Fl_RGB_Image img(pix.data(), cols, rows, 3);
            int x0 = (int)X(0), y0 = (int)Y(rows), x1 = (int)X(cols), y1 = (int)Y(0);
            Fl_RGB_Image* scaled = (Fl_RGB_Image*)img.copy(std::max(1, x1 - x0), std::max(1, y1 - y0));
            scaled->draw(x0, y0); delete scaled;
            continue;
        }
        rgb col = plot_palette(ci++); plot_color(col);
        if (!L.label.empty()) legend.push_back({ L.label, col });
        if (L.kind == "line") {
            size_t n = L.x.size();
            if (n > (size_t)pw * 2 && n > 1) {                // more points than pixels: min/max envelope per column
                double per = (double)n / pw; size_t k = 0;
                while (k < n) {
                    size_t e = std::min(n, k + (size_t)per + 1); double lo = 1e300, hi = -1e300, x0 = L.x[k];
                    for (size_t j = k; j < e; j++) { lo = std::min(lo, L.y[j]); hi = std::max(hi, L.y[j]); }
                    fl_line((int)X(x0), (int)Y(lo), (int)X(x0), (int)Y(hi));
                    k = e;
                }
            } else { fl_line_style(FL_SOLID, 2); fl_begin_line(); for (size_t k = 0; k < n; k++) fl_vertex(X(L.x[k]), Y(L.y[k])); fl_end_line(); fl_line_style(0); }
        }
        else if (L.kind == "scatter") for (size_t k = 0; k < L.x.size(); k++) fl_pie((int)X(L.x[k]) - 3, (int)Y(L.y[k]) - 3, 7, 7, 0, 360);
        else if (L.kind == "bars") {
            double bw = L.x.size() > 1 ? (L.x[1] - L.x[0]) * 0.8 : (xmax - xmin) * 0.5;
            for (size_t k = 0; k < L.x.size(); k++) { int x0 = (int)X(L.x[k] - bw / 2), x1 = (int)X(L.x[k] + bw / 2), y0 = (int)Y(0), y1 = (int)Y(L.y[k]); fl_rectf(x0, std::min(y0, y1), std::max(1, x1 - x0), std::abs(y1 - y0)); }
        }
    }
    if (view.has_pick) { fl_color(200, 40, 40); fl_circle(X(view.px), Y(view.py), 6); }
    fl_pop_clip();
    fl_font(FL_HELVETICA, fs);
    for (size_t k = 0; k < legend.size(); k++) {
        int y = top + 8 + (int)k * (fs + 4), x = left + (int)pw - 12 - (int)fl_width(legend[k].first.c_str()) - 22;
        plot_color(legend[k].second); fl_rectf(x, y + 2, 14, fs - 4); fl_color(40, 40, 40); fl_draw(legend[k].first.c_str(), x + 20, y + fs - 2);
    }
    if (view.has_cursor) {
        std::string ss = "x = " + tick_label(view.cx) + "   y = " + tick_label(view.cy);
        if (view.has_pick) ss += "   nearest: (" + tick_label(view.px) + ", " + tick_label(view.py) + ")" + (view.pick_label.empty() ? "" : " " + view.pick_label);
        fl_font(FL_HELVETICA, fs - 1); int tw = (int)fl_width(ss.c_str()) + 12;
        fl_color(255, 255, 255); fl_rectf(left + 4, top + 4, tw, fs + 6); fl_color(40, 40, 40); fl_draw(ss.c_str(), left + 10, top + 4 + fs);
    }
    if (view.zoomed) { fl_font(FL_HELVETICA, fs - 2); fl_color(130, 130, 130); fl_draw("zoomed: 0 resets", ox + w - 120, oy + h - 8); }
}

// --- 3D surface: projected and painted in software, so it draws and exports like everything else ---
inline void render_surface(const figure& f, const plot_layer& L, int ox, int oy, int w, int h, const plot_view& view) {
    fl_color(250, 250, 250); fl_rectf(ox, oy, w, h);
    fl_font(FL_HELVETICA_BOLD, 14); fl_color(40, 40, 40);
    if (!f.title.empty()) fl_draw(f.title.c_str(), ox + (int)((w - fl_width(f.title.c_str())) / 2), oy + 22);
    size_t rows = L.m.size(), cols = L.m[0].size();
    double lo = 1e300, hi = -1e300; for (auto& r : L.m) for (double v : r) { lo = std::min(lo, v); hi = std::max(hi, v); }
    if (hi <= lo) hi = lo + 1;
    size_t step_r = std::max<size_t>(1, rows / 120), step_c = std::max<size_t>(1, cols / 120);   // block averages keep it fluid
    size_t R = (rows + step_r - 1) / step_r, C = (cols + step_c - 1) / step_c;
    std::vector<double> z(R * C);
    for (size_t rr = 0; rr < R; rr++) for (size_t cc = 0; cc < C; cc++) { double s = 0; size_t n = 0; for (size_t r = rr * step_r; r < std::min(rows, (rr + 1) * step_r); r++) for (size_t c = cc * step_c; c < std::min(cols, (cc + 1) * step_c); c++) { s += L.m[r][c]; n++; } z[rr * C + cc] = s / n; }
    // camera: orbit around a target, perspective projection
    double ca = std::cos(view.angle), sa = std::sin(view.angle), cp = std::cos(view.pitch), sp = std::sin(view.pitch);
    double tx = view.tx, ty = 0.25 + view.ty, tz = view.tz, dist = view.dist;
    double ex = tx + dist * ca * cp, ey = ty + dist * sp, ez = tz + dist * sa * cp;
    double fx = tx - ex, fy = ty - ey, fz = tz - ez; double fl = std::sqrt(fx * fx + fy * fy + fz * fz); fx /= fl; fy /= fl; fz /= fl;
    double rx = fz * 0 - fy * 1 * 0 + (fy * 0 - fz * 1), ry = fz * 0 - fx * 0, rz = fx * 1 - fy * 0;   // right = forward x up(0,1,0)
    rx = fy * 0 - fz * 1; ry = fz * 0 - fx * 0; rz = fx * 1 - fy * 0; { double rl = std::sqrt(rx * rx + ry * ry + rz * rz); rx /= rl; ry /= rl; rz /= rl; }
    double ux = ry * fz - rz * fy, uy = rz * fx - rx * fz, uz = rx * fy - ry * fx;
    double focal = (h - 40) / (2 * std::tan(0.35));
    auto project = [&](double x, double y, double zz, double& sx, double& sy, double& depth) {
        double dx = x - ex, dy = y - ey, dz = zz - ez;
        double cxp = dx * rx + dy * ry + dz * rz, cyp = dx * ux + dy * uy + dz * uz, czp = dx * fx + dy * fy + dz * fz;
        depth = czp; if (czp < 0.05) czp = 0.05;
        sx = ox + w / 2.0 + cxp / czp * focal; sy = oy + h / 2.0 + 10 - cyp / czp * focal;
    };
    auto P = [&](size_t r, size_t c, double& sx, double& sy, double& d) { project((double)c / std::max<size_t>(1, C - 1) - 0.5, (z[r * C + c] - lo) / (hi - lo) * 0.8, (double)r / std::max<size_t>(1, R - 1) - 0.5, sx, sy, d); };
    struct quad { double d; double x[4], y[4]; rgb col; };
    std::vector<quad> quads; quads.reserve((R - 1) * (C - 1));
    for (size_t r = 0; r + 1 < R; r++) for (size_t c = 0; c + 1 < C; c++) {
        quad q; double d0, d1, d2, d3;
        P(r, c, q.x[0], q.y[0], d0); P(r, c + 1, q.x[1], q.y[1], d1); P(r + 1, c + 1, q.x[2], q.y[2], d2); P(r + 1, c, q.x[3], q.y[3], d3);
        q.d = (d0 + d1 + d2 + d3) / 4;
        double zm = (z[r * C + c] + z[r * C + c + 1] + z[(r + 1) * C + c + 1] + z[(r + 1) * C + c]) / 4;
        q.col = plot_colormap((zm - lo) / (hi - lo)); quads.push_back(q);
    }
    std::sort(quads.begin(), quads.end(), [](const quad& a, const quad& b) { return a.d > b.d; });   // painter's algorithm: far first
    fl_push_clip(ox, oy, w, h);
    for (auto& q : quads) {
        plot_color(q.col); fl_polygon((int)q.x[0], (int)q.y[0], (int)q.x[1], (int)q.y[1], (int)q.x[2], (int)q.y[2], (int)q.x[3], (int)q.y[3]);
        if (quads.size() < 2500) { fl_color(170, 170, 170); fl_loop((int)q.x[0], (int)q.y[0], (int)q.x[1], (int)q.y[1], (int)q.x[2], (int)q.y[2], (int)q.x[3], (int)q.y[3]); }
    }
    // the base axes
    double ax, ay, ad, bx, by, bd, cx2, cy2, cd, dx2, dy2, dd;
    project(-0.5, 0, -0.5, ax, ay, ad); project(0.5, 0, -0.5, bx, by, bd); project(-0.5, 0, 0.5, cx2, cy2, cd); project(-0.5, 0.8, -0.5, dx2, dy2, dd);
    fl_color(120, 120, 120); fl_line((int)ax, (int)ay, (int)bx, (int)by); fl_line((int)ax, (int)ay, (int)cx2, (int)cy2); fl_line((int)ax, (int)ay, (int)dx2, (int)dy2);
    fl_pop_clip();
    fl_font(FL_HELVETICA, 12); fl_color(90, 90, 90);
    fl_draw(((f.xlabel.empty() ? "columns" : f.xlabel) + "  (arrows or drag orbit)").c_str(), ox + 12, oy + h - 8);
    std::string range = tick_label(lo) + " .. " + tick_label(hi); fl_draw(range.c_str(), ox + w - 12 - (int)fl_width(range.c_str()), oy + h - 8);
}

// --- a figure into a rectangle: a grid of subplots, a surface, or a 2D plot ---
inline void render_figure_at(const figure& f, int ox, int oy, int w, int h, const plot_view& view) {
    if (f.is_grid()) {
        int n = (int)f.layers.size();
        int cols = f.cols > 0 ? f.cols : (int)std::ceil(std::sqrt((double)n)), rows = f.rows > 0 ? f.rows : (n + cols - 1) / cols;
        int top = f.title.empty() ? 0 : 30;
        fl_color(250, 250, 250); fl_rectf(ox, oy, w, h);
        if (!f.title.empty()) { fl_font(FL_HELVETICA_BOLD, 15); fl_color(40, 40, 40); fl_draw(f.title.c_str(), ox + (int)((w - fl_width(f.title.c_str())) / 2), oy + 22); }
        int cw = w / cols, ch = (h - top) / rows;
        for (int k = 0; k < n; k++) {
            int r = k / cols, c = k % cols;
            render_figure_at(*f.layers[k].sub, ox + c * cw, oy + top + r * ch, cw, ch, plot_view());
            fl_color(215, 215, 215); fl_rect(ox + c * cw, oy + top + r * ch, cw, ch);
        }
        return;
    }
    for (auto& L : f.layers) if (L.kind == "surface") { render_surface(f, L, ox, oy, w, h, view); return; }
    render_2d(f, ox, oy, w, h, view);
}

// --- keys and mouse, shared by the window and any host that embeds a plot ---
// + - zoom, W A S D pan, arrows orbit, 0 or r reset; drag pans/orbits; the wheel zooms around the cursor
inline void plot_key(const figure& f, plot_view& view, int key, bool shift) {
    if (f.is_grid()) return;
    bool surface = false; for (auto& L : f.layers) if (L.kind == "surface") surface = true;
    (void)shift;
    if (surface) {
        if (key == FL_Left) view.angle -= 0.08f;
        if (key == FL_Right) view.angle += 0.08f;
        if (key == FL_Up) view.pitch = std::min(1.5f, view.pitch + 0.05f);
        if (key == FL_Down) view.pitch = std::max(0.05f, view.pitch - 0.05f);
        if (key == '+' || key == '=') view.dist = std::max(0.6f, view.dist * 0.9f);
        if (key == '-') view.dist = std::min(8.0f, view.dist * 1.1f);
        float pan = 0.05f * view.dist, rx = -std::sin(view.angle), rz = std::cos(view.angle);
        if (key == 'a') { view.tx -= pan * rx; view.tz -= pan * rz; }
        if (key == 'd') { view.tx += pan * rx; view.tz += pan * rz; }
        if (key == 'w') view.ty += pan;
        if (key == 's') view.ty -= pan;
        if (key == '0' || key == 'r') { view.angle = 0.9f; view.pitch = 0.6f; view.dist = 2.6f; view.tx = view.ty = view.tz = 0; }
        return;
    }
    double xmin, xmax, ymin, ymax; figure_range(f, xmin, xmax, ymin, ymax);
    if (view.zoomed) { xmin = view.xmin; xmax = view.xmax; ymin = view.ymin; ymax = view.ymax; }
    double cx = (xmin + xmax) / 2, cy = (ymin + ymax) / 2, hw = (xmax - xmin) / 2, hh = (ymax - ymin) / 2;
    auto set = [&](double a, double b, double c, double d) { view.xmin = a; view.xmax = b; view.ymin = c; view.ymax = d; view.zoomed = true; };
    if (key == '+' || key == '=') set(cx - hw * 0.8, cx + hw * 0.8, cy - hh * 0.8, cy + hh * 0.8);
    if (key == '-') set(cx - hw * 1.25, cx + hw * 1.25, cy - hh * 1.25, cy + hh * 1.25);
    double step = 0.1;
    if (key == 'a') set(xmin - hw * step, xmax - hw * step, ymin, ymax);
    if (key == 'd') set(xmin + hw * step, xmax + hw * step, ymin, ymax);
    if (key == 'w') set(xmin, xmax, ymin + hh * step, ymax + hh * step);
    if (key == 's') set(xmin, xmax, ymin - hh * step, ymax - hh * step);
    if (key == '0' || key == 'r') view.zoomed = false;
}
// the cursor's data coordinates and the nearest point, for a mouse at (mx, my) inside (ox, oy, w, h)
inline void plot_cursor(const figure& f, plot_view& view, int ox, int oy, int w, int h, int mx, int my) {
    view.has_cursor = false; view.has_pick = false;
    if (f.is_grid()) return;
    for (auto& L : f.layers) if (L.kind == "surface") return;
    int left = ox + 64, top = oy + (f.title.empty() ? 16 : 34); double pw = w - 64 - 20, ph = h - (f.title.empty() ? 16 : 34) - 44;
    if (mx < left || mx > left + pw || my < top || my > top + ph) return;
    double xmin, xmax, ymin, ymax; figure_range(f, xmin, xmax, ymin, ymax);
    if (view.zoomed) { xmin = view.xmin; xmax = view.xmax; ymin = view.ymin; ymax = view.ymax; }
    view.has_cursor = true; view.cx = xmin + (mx - left) / pw * (xmax - xmin); view.cy = ymin + (top + ph - my) / ph * (ymax - ymin);
    double best = 1e300; size_t ci = 0;
    for (auto& L : f.layers) {
        if (L.kind == "image") continue;
        for (size_t k = 0; k < L.x.size(); k++) {
            double exx = (L.x[k] - view.cx) / (xmax - xmin) * pw, eyy = (L.y[k] - view.cy) / (ymax - ymin) * ph, d = exx * exx + eyy * eyy;
            if (d < best && d < 400) { best = d; view.has_pick = true; view.px = L.x[k]; view.py = L.y[k]; view.pick_label = L.label.empty() ? "layer " + std::to_string(ci) : L.label; }
        }
        ci++;
    }
}
inline void plot_drag(const figure& f, plot_view& view, int ox, int oy, int w, int h, int dx, int dy) {
    if (f.is_grid()) return;
    bool surface = false; for (auto& L : f.layers) if (L.kind == "surface") surface = true;
    if (surface) { view.angle += dx * 0.01f; view.pitch = std::max(0.05f, std::min(1.5f, view.pitch - dy * 0.01f)); return; }
    double xmin, xmax, ymin, ymax; figure_range(f, xmin, xmax, ymin, ymax);
    if (view.zoomed) { xmin = view.xmin; xmax = view.xmax; ymin = view.ymin; ymax = view.ymax; }
    double pw = w - 64 - 20, ph = h - (f.title.empty() ? 16 : 34) - 44;
    double ddx = -dx / pw * (xmax - xmin), ddy = dy / ph * (ymax - ymin);
    view.xmin = xmin + ddx; view.xmax = xmax + ddx; view.ymin = ymin + ddy; view.ymax = ymax + ddy; view.zoomed = true; (void)ox; (void)oy;
}
inline void plot_wheel(const figure& f, plot_view& view, int dir) {
    if (f.is_grid()) return;
    bool surface = false; for (auto& L : f.layers) if (L.kind == "surface") surface = true;
    if (surface) { view.dist = dir < 0 ? std::max(0.6f, view.dist * 0.9f) : std::min(8.0f, view.dist * 1.1f); return; }
    double xmin, xmax, ymin, ymax; figure_range(f, xmin, xmax, ymin, ymax);
    if (view.zoomed) { xmin = view.xmin; xmax = view.xmax; ymin = view.ymin; ymax = view.ymax; }
    double cx = view.has_cursor ? view.cx : (xmin + xmax) / 2, cy = view.has_cursor ? view.cy : (ymin + ymax) / 2, zf = dir < 0 ? 0.8 : 1.25;
    view.xmin = cx - (cx - xmin) * zf; view.xmax = cx + (xmax - cx) * zf; view.ymin = cy - (cy - ymin) * zf; view.ymax = cy + (ymax - cy) * zf; view.zoomed = true;
}

// --- PNG export: draw offscreen, encode (a small PNG writer: stored deflate blocks, no dependency) ---
inline unsigned long plot_crc32(const unsigned char* d, size_t n, unsigned long crc = 0xffffffffUL) {
    static unsigned long table[256]; static bool init = false;
    if (!init) { for (unsigned long k = 0; k < 256; k++) { unsigned long c = k; for (int j = 0; j < 8; j++) c = c & 1 ? 0xedb88320UL ^ (c >> 1) : c >> 1; table[k] = c; } init = true; }
    for (size_t k = 0; k < n; k++) crc = table[(crc ^ d[k]) & 255] ^ (crc >> 8);
    return crc;
}
inline void plot_write_png(const std::string& path, const unsigned char* rgb_pixels, int w, int h) {
    std::string raw; raw.reserve((size_t)h * (w * 3 + 1));
    for (int y = 0; y < h; y++) { raw += '\0'; raw.append((const char*)rgb_pixels + (size_t)y * w * 3, (size_t)w * 3); }
    std::string z; z += '\x78'; z += '\x01';                    // zlib header, then stored blocks of up to 65535 bytes
    size_t pos = 0; while (pos < raw.size()) { size_t n = std::min<size_t>(65535, raw.size() - pos); bool last = pos + n == raw.size();
        z += (char)(last ? 1 : 0); z += (char)(n & 255); z += (char)(n >> 8); z += (char)(~n & 255); z += (char)((~n >> 8) & 255); z.append(raw, pos, n); pos += n; }
    unsigned long a = 1, b = 0; for (unsigned char c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; } unsigned long adler = (b << 16) | a;
    for (int k = 3; k >= 0; k--) z += (char)((adler >> (8 * k)) & 255);
    std::string out = "\x89PNG\r\n\x1a\n";
    auto chunk = [&](const char* type, const std::string& data) {
        unsigned long len = data.size(); for (int k = 3; k >= 0; k--) out += (char)((len >> (8 * k)) & 255);
        std::string td = std::string(type) + data; out += td;
        unsigned long crc = plot_crc32((const unsigned char*)td.data(), td.size()) ^ 0xffffffffUL; for (int k = 3; k >= 0; k--) out += (char)((crc >> (8 * k)) & 255);
    };
    std::string ihdr; for (int k = 3; k >= 0; k--) ihdr += (char)((w >> (8 * k)) & 255); for (int k = 3; k >= 0; k--) ihdr += (char)((h >> (8 * k)) & 255);
    ihdr += '\x08'; ihdr += '\x02'; ihdr += '\0'; ihdr += '\0'; ihdr += '\0';
    chunk("IHDR", ihdr); chunk("IDAT", z); chunk("IEND", "");
    std::ofstream f(path, std::ios::binary); if (!f) throw std::runtime_error("save-png: cannot write " + path); f << out;
}
inline void figure_to_png(const figure& f, const std::string& path, int w, int h) {
    fl_open_display();
    Fl_Image_Surface surf(w, h);
    Fl_Surface_Device::push_current(&surf);
    render_figure_at(f, 0, 0, w, h, plot_view());
    Fl_RGB_Image* img = surf.image();
    Fl_Surface_Device::pop_current();
    if (!img) throw std::runtime_error("save-png: cannot render offscreen");
    std::vector<unsigned char> rgb_pixels((size_t)w * h * 3);
    const unsigned char* src = (const unsigned char*)img->data()[0]; int d = img->d(), ld = img->ld() ? img->ld() : img->w() * d;
    for (int y = 0; y < h && y < img->h(); y++) for (int x = 0; x < w && x < img->w(); x++) for (int c = 0; c < 3; c++) rgb_pixels[((size_t)y * w + x) * 3 + c] = src[(size_t)y * ld + x * d + (d >= 3 ? c : 0)];
    delete img;
    plot_write_png(path, rgb_pixels.data(), w, h);
}

// --- the window ---
struct plot_widget : Fl_Widget {
    figure f; plot_view view; int lastx = 0, lasty = 0;
    plot_widget(int x, int y, int w, int h, figure fig) : Fl_Widget(x, y, w, h), f(std::move(fig)) {}
    void draw() override {
        render_figure_at(f, x(), y(), w(), h() - 18, view);
        fl_color(246, 246, 244); fl_rectf(x(), y() + h() - 18, w(), 18);
        fl_font(FL_HELVETICA, 11); fl_color(150, 150, 150); fl_draw("+ - zoom   W A S D pan   arrows orbit   0 or r reset   drag pans   wheel zooms   e exports a PNG   Esc or q closes", x() + 8, y() + h() - 5);
    }
    int handle(int e) override {
        switch (e) {
        case FL_FOCUS: case FL_UNFOCUS: return 1;
        case FL_ENTER: case FL_LEAVE: return 1;
        case FL_MOVE: plot_cursor(f, view, x(), y(), w(), h() - 18, Fl::event_x(), Fl::event_y()); redraw(); return 1;
        case FL_PUSH: lastx = Fl::event_x(); lasty = Fl::event_y(); take_focus(); return 1;
        case FL_DRAG: plot_drag(f, view, x(), y(), w(), h() - 18, Fl::event_x() - lastx, Fl::event_y() - lasty); lastx = Fl::event_x(); lasty = Fl::event_y(); redraw(); return 1;
        case FL_MOUSEWHEEL: plot_wheel(f, view, Fl::event_dy()); redraw(); return 1;
        case FL_KEYDOWN: {
            int k = Fl::event_key(); const char* t = Fl::event_text();
            if (k == FL_Escape || (t && (t[0] == 'q'))) { window()->hide(); return 1; }
            if (t && t[0] == 'e') { std::string p = plot_save_name(f); try { figure_to_png(f, p, w(), h() - 18); std::cout << "saved " << p << std::endl; } catch (std::exception& ex) { std::cerr << ex.what() << "\n"; } return 1; }
            plot_key(f, view, k >= FL_Left && k <= FL_Down ? k : (t && t[0] ? t[0] : k), Fl::event_state(FL_SHIFT));
            redraw(); return 1;
        }
        }
        return Fl_Widget::handle(e);
    }
};
// The look of every Musil window (the IDE, plots, controls): set once, before the first window
inline void ui_style_once() {
    static bool done = false; if (done) return; done = true;
    Fl::scheme("oxy"); Fl::background(240, 240, 238); Fl::background2(255, 255, 255); Fl::foreground(40, 40, 40);
}
inline std::vector<Fl_Double_Window*>& plot_windows() { static std::vector<Fl_Double_Window*> w; return w; }
inline void plot_open_window(figure f, int w, int h) {
    ui_style_once();
    static int n = 0; int k = n++ % 8;                       // centred on the screen, cascading so several windows do not cover each other
    int x = (Fl::w() - w) / 2 + 30 * k - 100, y = (Fl::h() - h) / 2 + 30 * k - 100;
    Fl_Double_Window* win = new Fl_Double_Window(std::max(0, x), std::max(0, y), w, h, f.title.empty() ? "musil" : f.title.c_str());
    plot_widget* pw = new plot_widget(0, 0, w, h, std::move(f));
    win->resizable(pw); win->end(); win->size_range(300, 200);
    win->callback([](Fl_Widget* wd, void*) { wd->hide(); });
    plot_windows().push_back(win); win->show(); pw->take_focus();
}
inline int plot_windows_open() { int n = 0; for (auto* w : plot_windows()) if (w->shown()) n++; return n; }
// Hosts with the interpreter on another thread (the IDE) set this so windows are made on the FLTK thread
inline bool& plot_needs_awake() { static bool b = false; return b; }
struct plot_request { figure f; int w, h; };
inline void plot_awake_cb(void* p) { plot_request* r = (plot_request*)p; plot_open_window(std::move(r->f), r->w, r->h); delete r; }
inline void plot_show_figure(figure f, int w, int h) {
    if (plot_needs_awake()) { Fl::awake(plot_awake_cb, new plot_request{ std::move(f), w, h }); }
    else plot_open_window(std::move(f), w, h);
}

// --- builtins ---
// (save-png fig path [w h]) render a figure to a PNG file (900 x 560 by default)
inline vptr plot_save_png(vlist& a, Interp& i) {
    figure f; try { f = parse_figure(a[0]); } catch (std::exception& e) { i.bad(e.what()); }
    int w = a.size() > 2 ? (int)i.scalar(a[2]) : 900, h = a.size() > 3 ? (int)i.scalar(a[3]) : 560;
    if (plot_needs_awake()) Fl::lock();
    try { figure_to_png(f, i.str(a[1]), w, h); } catch (std::exception& e) { if (plot_needs_awake()) Fl::unlock(); i.bad(e.what()); }
    if (plot_needs_awake()) Fl::unlock();
    return v_nil();
}
// (show fig [w h]) open the figure in a window of its own and return; the program goes on and the window
//   stays (the idle hook keeps it alive); several can be open. Skipped when MUSIL_NOSHOW is set.
inline vptr plot_show(vlist& a, Interp& i) {
    figure f; try { f = parse_figure(a[0]); } catch (std::exception& e) { i.bad(e.what()); }
    if (std::getenv("MUSIL_NOSHOW")) return v_nil();
    int w = a.size() > 1 ? (int)i.scalar(a[1]) : 900, h = a.size() > 2 ? (int)i.scalar(a[2]) : 600;
    plot_show_figure(std::move(f), w, h);
    if (!plot_needs_awake()) Fl::check();
    return v_nil();
}
// (plot-windows) => how many plot windows are open; (close-plots) closes them
inline vptr plot_windows_count(vlist&, Interp&) { return v_num(plot_windows_open()); }
inline vptr plot_close_all(vlist&, Interp&) { for (auto* w : plot_windows()) w->hide(); return v_nil(); }
inline void add_plot(Interp& i) {
    i.def("save-png", plot_save_png, 2, 4); i.def("show", plot_show, 1, 3);
    i.def("plot-windows", plot_windows_count, 0, 0); i.def("close-plots", plot_close_all, 0, 0);
    if (!plot_needs_awake()) { auto prev = i.idle_fn; i.idle_fn = [prev]() { if (prev) prev(); if (Fl::first_window()) Fl::check(); }; }   // keep the windows alive while the interpreter waits (only once there is one: no GUI setup for plain scripts)
}

} // namespace musil

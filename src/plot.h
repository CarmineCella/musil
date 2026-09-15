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
#include "live.h"
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_draw.H>
#include <FL/platform.H>
#include <cstdio>
#include <set>

namespace musil {

// how a window hands a line of Musil to the interpreter: hosts set it (the IDE queues; the CLI runs it at once)
inline std::function<void(const std::string&)>& plot_submit() { static std::function<void(const std::string&)> f; return f; }

// --- figure description, validated once ---
struct figure;
// A roll: rows (a name and a clef: "treble", "bass" or "none") and bars (row, start, duration, a lane within the
// row, a MIDI pitch or -1, a dynamics label) with a tooltip: a score drawn like music, note heads and duration lines
struct roll_bar { int row = 0; double start = 0, dur = 0; std::string label, tip; int group = 0; int lane = 0, lanes = 1; double midi = -1; std::string dyn; int event_id = -1; };
struct plot_layer { std::string kind, label; varr x, y; std::vector<varr> m; std::shared_ptr<figure> sub; std::vector<std::string> rows, clefs; std::vector<roll_bar> bars; int score_id = -1; };
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
        } else if (p.kind == "roll") {
            // (list "roll" rows bars [score-id]): rows a list of names or (list name clef); a bar (list row start dur label tip group [lane lanes midi dyn event-id])
            if (L->l.size() < 3 || L->l[1]->t != Value::LIST || L->l[2]->t != Value::LIST) plot_fail("roll: needs rows and bars");
            if (L->l.size() > 3 && L->l[3]->t == Value::NUM) p.score_id = (int)L->l[3]->num[0];
            for (auto& r : L->l[1]->l) { if (r->t == Value::LIST && r->l.size() >= 2) { p.rows.push_back(str_of(r->l[0])); p.clefs.push_back(str_of(r->l[1])); } else { p.rows.push_back(str_of(r)); p.clefs.push_back("none"); } }
            for (auto& b : L->l[2]->l) {
                if (b->t != Value::LIST || b->l.size() < 3) plot_fail("roll: a bar is (list row start dur label tip group)");
                roll_bar bar; bar.row = (int)b->l[0]->num[0]; bar.start = b->l[1]->num[0]; bar.dur = b->l[2]->num[0];
                if (b->l.size() > 3) bar.label = str_of(b->l[3]); if (b->l.size() > 4) bar.tip = str_of(b->l[4]); if (b->l.size() > 5) bar.group = (int)b->l[5]->num[0];
                if (b->l.size() > 7) { bar.lane = (int)b->l[6]->num[0]; bar.lanes = std::max(1, (int)b->l[7]->num[0]); }
                if (b->l.size() > 8 && b->l[8]->t == Value::NUM) bar.midi = b->l[8]->num[0];
                if (b->l.size() > 9) bar.dyn = str_of(b->l[9]);
                if (b->l.size() > 10 && b->l[10]->t == Value::NUM) bar.event_id = (int)b->l[10]->num[0];
                p.bars.push_back(bar);
            }
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
        if (L.kind == "roll") { image = true; for (auto& b : L.bars) xmax = std::max(xmax, b.start + b.dur); ymin = 0; ymax = std::max(ymax, (double)std::max<size_t>(1, L.rows.size())); if (xmax <= 0) xmax = 1; xmin = -0.012 * xmax; xmax *= 1.02; continue; }
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
    double row_scale = 1, row_offset = 0;             // the roll: vertical zoom (1 = every row fits) and the first row shown
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

// --- the roll: rows of bars in time (a score), with the hovered bar's tooltip -----------------
struct roll_geom { int left, top; double pw, ph, xmin, xmax; int rows; double row_h; int top_rows; };
inline roll_geom roll_geometry(const figure& f, int ox, int oy, int w, int h, const plot_view& view) {
    roll_geom g; g.left = ox + 110 + 36; g.top = oy + (f.title.empty() ? 16 : 34); g.pw = w - 110 - 36 - 28; g.ph = h - (f.title.empty() ? 16 : 34) - 56;   // 36 for the clefs, 28 and 12 for the scrollbars
    double ymin, ymax; figure_range(f, g.xmin, g.xmax, ymin, ymax);
    if (view.zoomed) { g.xmin = view.xmin; g.xmax = view.xmax; }
    g.rows = 1; for (auto& L : f.layers) if (L.kind == "roll") g.rows = std::max<int>(1, (int)L.rows.size());
    g.row_h = g.ph / g.rows * std::max(1.0, view.row_scale); g.top_rows = g.top - (int)(view.row_offset * g.row_h); return g;
}
// the diatonic step of a MIDI note (C4 = 60 -> 0 at C, counting letters), and whether it needs a sharp
inline int roll_step(int midi, bool& sharp) { static const int step[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 }; static const bool sh[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 }; int pc = ((midi % 12) + 12) % 12; sharp = sh[pc]; return (midi / 12 - 1) * 7 + step[pc]; }
// a row's staff: its five lines' vertical positions and the step of its bottom line (E4 for a treble clef, G2 for a bass one)
struct staff_geom { double y_bottom, sp; int bottom_step; bool has_staff; };
inline staff_geom roll_staff(const roll_geom& g, int r, const std::string& clef) {
    staff_geom st; st.has_staff = clef == "treble" || clef == "bass";
    double top = g.top_rows + r * g.row_h; st.sp = std::max(3.0, g.row_h / 12.0);     // twelve spaces of room: the staff, ledger lines each side, the dynamics below
    st.y_bottom = top + g.row_h * 0.66; bool dummy; st.bottom_step = clef == "bass" ? roll_step(43, dummy) : roll_step(64, dummy);
    return st;
}
inline double roll_note_y(const staff_geom& st, int midi) { bool sh; int step = roll_step(midi, sh); return st.y_bottom - (step - st.bottom_step) * st.sp / 2; }
inline void render_roll(const figure& f, const plot_layer& L, int ox, int oy, int w, int h, const plot_view& view) {
    fl_color(250, 250, 250); fl_rectf(ox, oy, w, h);
    const int fs = 13; roll_geom g = roll_geometry(f, ox, oy, w, h, view);
    auto X = [&](double x) { return g.left + (x - g.xmin) / (g.xmax - g.xmin) * g.pw; };
    fl_font(FL_HELVETICA, fs);
    if (!f.title.empty()) { fl_font(FL_HELVETICA_BOLD, fs + 1); fl_color(40, 40, 40); fl_draw(f.title.c_str(), g.left + (int)((g.pw - fl_width(f.title.c_str())) / 2), oy + 8 + fs); fl_font(FL_HELVETICA, fs); }
    // the time grid
    double sx = nice_step(g.xmax - g.xmin, 8); fl_font(FL_HELVETICA, fs - 2);
    for (double t = std::ceil(g.xmin / sx) * sx; t <= g.xmax + 1e-9 * sx; t += sx) {
        int px = (int)X(t); fl_color(232, 232, 232); fl_line(px, g.top, px, g.top + (int)g.ph);
        std::string sl = tick_label(t); fl_color(80, 80, 80); fl_draw(sl.c_str(), px - (int)(fl_width(sl.c_str()) / 2), g.top + (int)g.ph + fs + 2);
    }
    // rows: the staves (five lines and the clef's letter), or a single line for unpitched rows; the names at the left
    fl_push_clip(ox, g.top, w, (int)g.ph);
    for (int r = 0; r < g.rows; r++) {
        std::string clef = r < (int)L.clefs.size() ? L.clefs[r] : "none"; staff_geom st = roll_staff(g, r, clef);
        int top = g.top_rows + (int)(r * g.row_h);
        if (r % 2) { fl_color(246, 246, 244); fl_rectf(g.left, top, (int)g.pw, (int)std::ceil(g.row_h)); }
        fl_color(120, 120, 120);
        if (st.has_staff) {
            for (int l = 0; l < 5; l++) { int y = (int)(st.y_bottom - l * st.sp); fl_line(g.left - 34, y, g.left + (int)g.pw, y); }
            fl_color(50, 50, 50); fl_line_style(FL_SOLID, std::max(1, (int)(st.sp / 4)));
            double cx = g.left - 20;
            if (clef == "treble") {                        // a treble clef drawn as curves: the spiral around the G line, the stem, the tail
                double gy = st.y_bottom - st.sp, sp = st.sp;
                fl_begin_line();
                fl_curve(cx, gy, cx - 1.0 * sp, gy - 0.9 * sp, cx + 0.2 * sp, gy - 2.0 * sp, cx + 1.1 * sp, gy - 1.0 * sp);   // the loop around G
                fl_curve(cx + 1.1 * sp, gy - 1.0 * sp, cx + 1.6 * sp, gy + 0.2 * sp, cx - 0.4 * sp, gy + 1.5 * sp, cx - 1.2 * sp, gy);
                fl_curve(cx - 1.2 * sp, gy, cx - 1.6 * sp, gy - 1.6 * sp, cx + 0.6 * sp, gy - 3.6 * sp, cx + 0.6 * sp, gy - 4.8 * sp); // up the stem to the top
                fl_curve(cx + 0.6 * sp, gy - 4.8 * sp, cx + 0.8 * sp, gy - 6.0 * sp, cx - 0.3 * sp, gy - 6.0 * sp, cx - 0.2 * sp, gy - 5.0 * sp);  // the top hook
                fl_curve(cx - 0.2 * sp, gy - 5.0 * sp, cx - 0.1 * sp, gy - 3.0 * sp, cx + 0.5 * sp, gy, cx + 0.5 * sp, gy + 1.9 * sp);           // back down the stem
                fl_curve(cx + 0.5 * sp, gy + 1.9 * sp, cx + 0.5 * sp, gy + 2.8 * sp, cx - 0.9 * sp, gy + 2.7 * sp, cx - 0.7 * sp, gy + 2.0 * sp);  // the tail's curl
                fl_end_line();
                fl_pie((int)(cx - 0.9 * sp - sp * 0.28), (int)(gy + 2.0 * sp - sp * 0.28), (int)(sp * 0.56), (int)(sp * 0.56), 0, 360);
            } else {                                       // a bass clef: the curl from the F line, and its two dots
                double fy = st.y_bottom - 3 * st.sp, sp = st.sp;
                fl_begin_line();
                fl_curve(cx - 0.9 * sp, fy, cx - 0.9 * sp, fy - 1.3 * sp, cx + 1.0 * sp, fy - 1.3 * sp, cx + 1.0 * sp, fy);
                fl_curve(cx + 1.0 * sp, fy, cx + 1.0 * sp, fy + 1.5 * sp, cx - 0.4 * sp, fy + 2.6 * sp, cx - 1.3 * sp, fy + 3.1 * sp);
                fl_end_line();
                fl_pie((int)(cx - 0.9 * sp - sp * 0.3), (int)(fy - sp * 0.3), (int)(sp * 0.6), (int)(sp * 0.6), 0, 360);
                fl_pie((int)(cx + 1.4 * sp - sp * 0.22), (int)(fy - 0.5 * sp - sp * 0.22), (int)(sp * 0.44), (int)(sp * 0.44), 0, 360);
                fl_pie((int)(cx + 1.4 * sp - sp * 0.22), (int)(fy + 0.5 * sp - sp * 0.22), (int)(sp * 0.44), (int)(sp * 0.44), 0, 360);
            }
            fl_line_style(0);
        } else { int y = (int)(top + g.row_h * 0.55); fl_line(g.left - 34, y, g.left + (int)g.pw, y); }
        if (r < (int)L.rows.size()) { fl_font(FL_HELVETICA, std::min(fs, (int)(g.row_h / 3))); fl_color(60, 60, 60); fl_draw(L.rows[r].c_str(), ox + 8, top + (int)(g.row_h / 2) + 4); }
    }
    // the events: a head at the onset and a line for the duration; pitched ones on their staff, with ledger lines and sharps.
    // A duration line stops short of a following note's sharp; a dynamic is written when it changes from the previous
    // one in its row and there is room for it (the hover shows every event's anyway)
    fl_push_clip(g.left, g.top, (int)g.pw, (int)g.ph);
    std::vector<size_t> order(L.bars.size()); for (size_t k = 0; k < order.size(); k++) order[k] = k;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b2) { return L.bars[a].row != L.bars[b2].row ? L.bars[a].row < L.bars[b2].row : L.bars[a].start < L.bars[b2].start; });
    std::vector<std::string> last_dyn(g.rows); std::vector<int> last_dyn_x(g.rows, -100000);
    for (size_t oi = 0; oi < order.size(); oi++) {
        size_t k = order[oi]; const roll_bar& b = L.bars[k]; if (b.row < 0 || b.row >= g.rows) continue;
        std::string clef = b.row < (int)L.clefs.size() ? L.clefs[b.row] : "none"; staff_geom st = roll_staff(g, b.row, clef);
        bool hot = view.has_pick && (size_t)view.px == k;
        rgb c = plot_palette((size_t)b.group); if (hot) c = { 30, 30, 30 };
        int x0 = (int)X(b.start), x1 = (int)X(b.start + b.dur); if (x1 <= x0 + 2) x1 = x0 + 2;
        if (st.has_staff && b.midi >= 0) {
            int midi = (int)std::lround(b.midi); bool sharp; int step = roll_step(midi, sharp);
            double y = roll_note_y(st, midi); double head = std::max(3.0, st.sp * 0.62);
            // the next note of this row at the same height with a sharp: end the line before its accidental
            for (size_t oj = oi + 1; oj < order.size(); oj++) { const roll_bar& n = L.bars[order[oj]]; if (n.row != b.row) break; if (n.midi < 0) continue;
                bool nsharp; roll_step((int)std::lround(n.midi), nsharp); double ny = roll_note_y(st, (int)std::lround(n.midi));
                if (std::fabs(ny - y) < head * 1.2) { int nx = (int)X(n.start) - (int)head - (nsharp ? (int)(st.sp * 1.6) : 2); if (nx < x1) x1 = std::max(x0 + 2, nx); break; } }
            fl_color(150, 150, 150);
            for (int sdiff = step - st.bottom_step; sdiff < 0; sdiff++) if (sdiff % 2 == 0) { int ly = (int)(st.y_bottom - sdiff * st.sp / 2); fl_line(x0 - (int)head - 3, ly, x0 + (int)head + 3, ly); }
            for (int sdiff = step - st.bottom_step; sdiff > 8; sdiff--) if (sdiff % 2 == 0) { int ly = (int)(st.y_bottom - sdiff * st.sp / 2); fl_line(x0 - (int)head - 3, ly, x0 + (int)head + 3, ly); }
            plot_color(c); fl_line_style(FL_SOLID, std::max(1, (int)(head * 0.35))); fl_line(x0, (int)y, x1, (int)y); fl_line_style(0);
            fl_pie(x0 - (int)head, (int)(y - head), (int)(2 * head), (int)(2 * head), 0, 360);
            if (sharp) { fl_font(FL_HELVETICA_BOLD, (int)std::max(8.0, st.sp * 1.6)); fl_color(60, 60, 60); fl_draw("#", x0 - (int)head - (int)(st.sp * 1.3), (int)(y + st.sp * 0.6)); }
            if (!b.dyn.empty() && g.row_h > 60) {
                fl_font(FL_TIMES_BOLD_ITALIC, (int)std::max(8.0, st.sp * 1.7)); int dw = (int)fl_width(b.dyn.c_str());
                bool changed = b.dyn != last_dyn[b.row], room = x0 - (int)head > last_dyn_x[b.row] + 6;
                int dx = std::max(g.left + 2, x0 - (int)head);
                if (changed && room) { fl_color(80, 80, 80); fl_draw(b.dyn.c_str(), dx, (int)(g.top_rows + (b.row + 1) * g.row_h - 3)); last_dyn[b.row] = b.dyn; last_dyn_x[b.row] = dx + dw; }
            }
        } else {                                              // unpitched: a square head on the row's line (lanes when they overlap)
            double lane_h = g.row_h / b.lanes; double y = g.top_rows + b.row * g.row_h + (b.lanes == 1 ? g.row_h * 0.55 : lane_h * (b.lane + 0.5));
            int head = std::max(3, (int)std::min(6.0, lane_h * 0.25));
            plot_color(c); fl_line_style(FL_SOLID, 2); fl_line(x0, (int)y, x1, (int)y); fl_line_style(0);
            fl_rectf(x0 - head, (int)y - head, 2 * head, 2 * head);
            if (x1 - x0 > 30 && lane_h > 16) { fl_font(FL_HELVETICA, std::min(fs - 2, (int)lane_h - 4)); fl_color(80, 80, 80); fl_push_clip(x0 + head + 3, (int)y - (int)lane_h / 2, x1 - x0 - head - 4, (int)lane_h); fl_draw(b.label.c_str(), x0 + head + 4, (int)y - 4); fl_pop_clip(); }
        }
    }
    // the playhead, while a score plays
    playhead_state& ph = playhead(); double now = live_now();
    if (ph.on && now < ph.end) { double t = ph.from + (now - ph.t0); if (t >= g.xmin && t <= g.xmax) { fl_color(200, 30, 30); fl_line_style(FL_SOLID, 2); fl_line((int)X(t), g.top, (int)X(t), g.top + (int)g.ph); fl_line_style(0); } }
    fl_pop_clip(); fl_pop_clip();
    fl_color(120, 120, 120); fl_rect(g.left, g.top, (int)g.pw, (int)g.ph);
    fl_font(FL_HELVETICA, fs); fl_color(60, 60, 60); fl_draw((f.xlabel.empty() ? "time (s)" : f.xlabel).c_str(), g.left + (int)(g.pw / 2) - 20, oy + h - 6);
    // scrollbars, when the view is zoomed: the thumb shows the visible part (drag it, or the wheel)
    { double fx0, fx1, fy0, fy1; figure_range(f, fx0, fx1, fy0, fy1);
      if (view.zoomed && (g.xmax - g.xmin) < (fx1 - fx0) * 0.999) {
          int by = g.top + (int)g.ph + fs + 8; fl_color(228, 228, 226); fl_rectf(g.left, by, (int)g.pw, 8);
          double a = (g.xmin - fx0) / (fx1 - fx0), bb = (g.xmax - fx0) / (fx1 - fx0); a = std::max(0.0, a); bb = std::min(1.0, bb);
          fl_color(150, 150, 150); fl_rectf(g.left + (int)(a * g.pw), by, std::max(12, (int)((bb - a) * g.pw)), 8); }
      if (view.row_scale > 1.0001) {
          int bx = g.left + (int)g.pw + 6; fl_color(228, 228, 226); fl_rectf(bx, g.top, 8, (int)g.ph);
          double vis = 1.0 / view.row_scale, a = view.row_offset / g.rows; a = std::min(std::max(0.0, a), 1 - vis);
          fl_color(150, 150, 150); fl_rectf(bx, g.top + (int)(a * g.ph), 8, std::max(12, (int)(vis * g.ph))); } }
    // the tooltip of the hovered event, and the cursor time
    fl_font(FL_HELVETICA, fs - 1);
    if (view.has_cursor) {
        std::string ss = "t = " + tick_label(view.cx) + " s";
        if (view.has_pick && (size_t)view.px < L.bars.size()) { const roll_bar& b = L.bars[(size_t)view.px]; ss += "   " + b.label + "   at " + tick_label(b.start) + " s, " + tick_label(b.dur) + " s" + (b.tip.empty() ? "" : "   " + b.tip); }
        int tw = (int)fl_width(ss.c_str()) + 12;
        fl_color(255, 255, 240); fl_rectf(g.left + 4, g.top + 4, std::min(tw, (int)g.pw - 8), fs + 6); fl_color(120, 120, 120); fl_rect(g.left + 4, g.top + 4, std::min(tw, (int)g.pw - 8), fs + 6);
        fl_color(40, 40, 40); fl_push_clip(g.left + 4, g.top + 4, (int)g.pw - 8, fs + 6); fl_draw(ss.c_str(), g.left + 10, g.top + 4 + fs); fl_pop_clip();
    }
    if (view.zoomed || view.row_scale > 1) { fl_font(FL_HELVETICA, fs - 2); fl_color(130, 130, 130); fl_draw("zoomed: R resets", ox + w - 110, oy + h - 8); }
}
// hit test: the event whose head or line is under (mx, my)
inline int roll_hit(const figure& f, const plot_layer& L, int ox, int oy, int w, int h, const plot_view& view, int mx, int my) {
    roll_geom g = roll_geometry(f, ox, oy, w, h, view);
    auto X = [&](double x) { return g.left + (x - g.xmin) / (g.xmax - g.xmin) * g.pw; };
    int best = -1; double bestd = 1e9;
    for (size_t k = 0; k < L.bars.size(); k++) {
        const roll_bar& b = L.bars[k]; std::string clef = b.row < (int)L.clefs.size() ? L.clefs[b.row] : "none"; staff_geom st = roll_staff(g, b.row, clef);
        double y; if (st.has_staff && b.midi >= 0) y = roll_note_y(st, (int)std::lround(b.midi)); else { double lane_h = g.row_h / b.lanes; y = g.top_rows + b.row * g.row_h + (b.lanes == 1 ? g.row_h * 0.55 : lane_h * (b.lane + 0.5)); }
        int x0 = (int)X(b.start), x1 = (int)X(b.start + b.dur);
        if (mx >= x0 - 6 && mx <= x1 + 2) { double d = std::fabs(my - y); if (d < 7 && d < bestd) { bestd = d; best = (int)k; } }
    }
    return best;
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
    for (auto& L : f.layers) if (L.kind == "roll") { render_roll(f, L, ox, oy, w, h, view); return; }
    render_2d(f, ox, oy, w, h, view);
}

// --- keys and mouse, shared by the window and any host that embeds a plot ---
// + - zoom, W A S D pan, arrows orbit, 0 or r reset; drag pans/orbits; the wheel zooms around the cursor
inline void plot_key(const figure& f, plot_view& view, int key, bool shift) {
    if (f.is_grid()) return;
    bool surface = false, roll = false; for (auto& L : f.layers) { if (L.kind == "surface") surface = true; if (L.kind == "roll") roll = true; }
    (void)shift;
    if (roll) {
        if (key == 'w') view.row_offset = std::max(0.0, view.row_offset - 1);
        if (key == 's') { double rows = 0; for (auto& L : f.layers) if (L.kind == "roll") rows = (double)L.rows.size(); view.row_offset = std::min(std::max(0.0, rows - rows / std::max(1.0, view.row_scale)), view.row_offset + 1); }
        if (key == 'z') view.row_scale = std::min(8.0, view.row_scale * 1.25);
        if (key == 'x') { view.row_scale = std::max(1.0, view.row_scale / 1.25); if (view.row_scale <= 1.0001) view.row_offset = 0; }
        if (key == '0' || key == 'r') { view.zoomed = false; view.row_scale = 1; view.row_offset = 0; }
        if (key == '+' || key == '=' || key == '-' || key == 'a' || key == 'd') { double xmin, xmax, ymin, ymax; figure_range(f, xmin, xmax, ymin, ymax); if (view.zoomed) { xmin = view.xmin; xmax = view.xmax; }
            double cx = (xmin + xmax) / 2, hw = (xmax - xmin) / 2, step = 0.1; auto set = [&](double a, double b) { view.xmin = a; view.xmax = b; view.zoomed = true; };
            if (key == '+' || key == '=') set(cx - hw * 0.8, cx + hw * 0.8); if (key == '-') set(cx - hw * 1.25, cx + hw * 1.25); if (key == 'a') set(xmin - hw * step, xmax - hw * step); if (key == 'd') set(xmin + hw * step, xmax + hw * step); }
        return;
    }
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
    double cx = (xmin + xmax) / 2, cy = (ymin + ymax) / 2, hw = (xmax - xmin) / 2, hh = roll ? 0 : (ymax - ymin) / 2;
    auto set = [&](double a, double b, double c, double d) { view.xmin = a; view.xmax = b; view.ymin = roll ? ymin : c; view.ymax = roll ? ymax : d; view.zoomed = true; };
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
    for (auto& L : f.layers) if (L.kind == "roll") {
        roll_geom g = roll_geometry(f, ox, oy, w, h, view);
        if (mx < g.left || mx > g.left + g.pw || my < g.top || my > g.top + g.ph) return;
        view.has_cursor = true; view.cx = g.xmin + (mx - g.left) / g.pw * (g.xmax - g.xmin); view.cy = 0;
        int k = roll_hit(f, L, ox, oy, w, h, view, mx, my);
        if (k >= 0) { view.has_pick = true; view.px = k; view.py = 0; view.pick_label = L.bars[(size_t)k].label; }
        return;
    }
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
    for (auto& L : f.layers) if (L.kind == "roll") { roll_geom g = roll_geometry(f, ox, oy, w, h, view); double ddx = -dx / g.pw * (g.xmax - g.xmin); view.xmin = g.xmin + ddx; view.xmax = g.xmax + ddx; view.zoomed = true; return; }
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
    bool roll = false; for (auto& L : f.layers) if (L.kind == "roll") roll = true;
    if (roll) { double cx = view.has_cursor ? view.cx : (xmin + xmax) / 2, zf = dir < 0 ? 0.8 : 1.25; view.xmin = cx - (cx - xmin) * zf; view.xmax = cx + (xmax - cx) * zf; view.ymin = ymin; view.ymax = ymax; view.zoomed = true; return; }
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
inline void figure_to_png_view(const figure& f, const std::string& path, int w, int h, const plot_view& view);
inline void figure_to_png(const figure& f, const std::string& path, int w, int h) { figure_to_png_view(f, path, w, h, plot_view()); }
inline void figure_to_png_view(const figure& f, const std::string& path, int w, int h, const plot_view& view) {
    fl_open_display();
    Fl_Image_Surface surf(w, h);
    Fl_Surface_Device::push_current(&surf);
    plot_view v = view; v.has_cursor = false; v.has_pick = false;
    render_figure_at(f, 0, 0, w, h, v);
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
    figure f; plot_view view; int lastx = 0, lasty = 0; bool has_roll = false; int dragging_bar = 0;
    static void follow_playhead(void* p) { plot_widget* w = (plot_widget*)p; if (!w->window() || !w->window()->shown()) return; if (playhead().on) w->redraw(); Fl::repeat_timeout(0.05, follow_playhead, p); }
    plot_widget(int x, int y, int w, int h, figure fig) : Fl_Widget(x, y, w, h), f(std::move(fig)) {}
    void draw() override {
        render_figure_at(f, x(), y(), w(), h() - 18, view);
        fl_color(246, 246, 244); fl_rectf(x(), y() + h() - 18, w(), 18);
        fl_font(FL_HELVETICA, 11); fl_color(150, 150, 150);
        fl_draw(has_roll ? "A D pan   + - zoom in time   W S scroll rows   Z X zoom rows   R reset   wheel scrolls (Ctrl: zooms)   double-click plays an event   Esc or q closes"
                         : "+ - zoom   W A S D pan   arrows orbit   0 or r reset   drag pans   wheel zooms   e exports a PNG   Esc or q closes", x() + 8, y() + h() - 5);
    }
    int handle(int e) override {
        switch (e) {
        case FL_FOCUS: case FL_UNFOCUS: return 1;
        case FL_ENTER: case FL_LEAVE: return 1;
        case FL_MOVE: plot_cursor(f, view, x(), y(), w(), h() - 18, Fl::event_x(), Fl::event_y()); redraw(); return 1;
        case FL_PUSH: {
            lastx = Fl::event_x(); lasty = Fl::event_y(); take_focus(); dragging_bar = 0;
            if (has_roll) {
                roll_geom g = roll_geometry(f, x(), y(), w(), h() - 18, view); const int fs = 13;
                if (lastx >= g.left && lastx <= g.left + g.pw && lasty >= g.top + g.ph + fs + 6 && lasty <= g.top + g.ph + fs + 18) dragging_bar = 1;   // the time scrollbar
                else if (lastx >= g.left + g.pw + 4 && lastx <= g.left + g.pw + 16 && lasty >= g.top && lasty <= g.top + g.ph) dragging_bar = 2;     // the rows' scrollbar
                else if (Fl::event_clicks() > 0 && plot_submit()) {   // a double click on an event plays it
                    for (auto& L : f.layers) if (L.kind == "roll") { int k = roll_hit(f, L, x(), y(), w(), h() - 18, view, lastx, lasty);
                        if (k >= 0 && L.score_id >= 0 && L.bars[(size_t)k].event_id >= 0) plot_submit()("(play-event " + std::to_string(L.score_id) + " " + std::to_string(L.bars[(size_t)k].event_id) + ")"); }
                }
            }
            return 1;
        }
        case FL_DRAG: {
            if (dragging_bar == 1) {                          // drag the time scrollbar's thumb
                roll_geom g = roll_geometry(f, x(), y(), w(), h() - 18, view); double fx0, fx1, fy0, fy1; figure_range(f, fx0, fx1, fy0, fy1);
                double dt = (Fl::event_x() - lastx) / g.pw * (fx1 - fx0); view.xmin = g.xmin + dt; view.xmax = g.xmax + dt; view.zoomed = true;
            } else if (dragging_bar == 2) {                   // drag the rows' scrollbar's thumb
                roll_geom g = roll_geometry(f, x(), y(), w(), h() - 18, view); double rows = (double)g.rows;
                view.row_offset = std::min(std::max(0.0, rows - rows / std::max(1.0, view.row_scale)), std::max(0.0, view.row_offset + (Fl::event_y() - lasty) / g.ph * rows));
            } else plot_drag(f, view, x(), y(), w(), h() - 18, Fl::event_x() - lastx, Fl::event_y() - lasty);
            lastx = Fl::event_x(); lasty = Fl::event_y(); redraw(); return 1;
        }
        case FL_RELEASE: dragging_bar = 0; return 1;
        case FL_MOUSEWHEEL: {
            if (has_roll) {                                  // the wheel scrolls: rows vertically, time horizontally (a sideways wheel, or Shift); Ctrl zooms in time
                int dy = Fl::event_dy(), dx = Fl::event_dx(); bool ctrl = Fl::event_state(FL_CTRL | FL_COMMAND), shift = Fl::event_state(FL_SHIFT);
                if (ctrl) plot_wheel(f, view, dy);
                else if (dx != 0 || shift) { int d = dx != 0 ? dx : dy; plot_key(f, view, d > 0 ? 'd' : 'a', false); }
                else if (dy != 0) plot_key(f, view, dy > 0 ? 's' : 'w', false);
                redraw(); return 1;
            }
            plot_wheel(f, view, Fl::event_dy()); redraw(); return 1;
        }
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
// Export...: a file dialog, then the figure as it is shown (the current zoom and view) to a PNG
inline void plot_export_cb(Fl_Widget*, void* d) {
    plot_widget* pw = (plot_widget*)d;
    Fl_Native_File_Chooser ch; ch.title("Export the figure as PNG"); ch.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE); ch.filter("PNG\t*.png");
    std::string preset = plot_save_name(pw->f); size_t sl = preset.find_last_of('/'); ch.preset_file((sl == std::string::npos ? preset : preset.substr(sl + 1)).c_str()); ch.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    if (ch.show() != 0) return;
    std::string path = ch.filename(); if (path.size() < 4 || path.substr(path.size() - 4) != ".png") path += ".png";
    try { figure_to_png_view(pw->f, path, pw->w(), pw->h() - 18, pw->view); std::cout << "exported " << path << std::endl; } catch (std::exception& ex) { std::cerr << ex.what() << "\n"; }
}
inline void plot_open_window(figure f, int w, int h) {
    ui_style_once();
    static int n = 0; int k = n++ % 8;                       // centred on the screen, cascading so several windows do not cover each other
    int x = (Fl::w() - w) / 2 + 30 * k - 100, y = (Fl::h() - h) / 2 + 30 * k - 100;
    const int bar = 34;
    Fl_Double_Window* win = new Fl_Double_Window(std::max(0, x), std::max(0, y), w, h + bar, f.title.empty() ? "musil" : f.title.c_str());
    plot_widget* pw = new plot_widget(0, bar, w, h, std::move(f));
    for (auto& L : pw->f.layers) if (L.kind == "roll") pw->has_roll = true;
    if (pw->has_roll) Fl::add_timeout(0.05, plot_widget::follow_playhead, pw);
    Fl_Button* exp = new Fl_Button(8, 5, 90, 24, "Export..."); exp->callback(plot_export_cb, pw); exp->clear_visible_focus(); exp->tooltip("the figure as shown, to a PNG file");
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
    if (!plot_submit()) { Interp* ip = &i; plot_submit() = [ip](const std::string& code) { try { ip->run(code, "<window>"); } catch (std::exception& e) { *ip->out << "window: " << e.what() << "\n" << std::flush; } }; }   // a window's action runs at once (the IDE queues instead)
}

} // namespace musil

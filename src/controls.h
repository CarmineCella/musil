// controls.h — the controls window for the command line (raylib): sliders and toggles for
// the hot parameters declared with control / toggle in live. The Listener draws its own
// panel from the same registry. Keys: Up/Down select, Left/Right change (Shift: bigger
// steps), Space toggles, 0 resets, Esc closes. The window runs the interpreter's idle work
// each frame, so live loops, OSC and the editor port keep going while it is open.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
#pragma once
#include "core.h"
#include "live.h"
#include "plot.h"

namespace musil {

// (controls) open the controls window until Esc; the sound and the loops keep running meanwhile
inline vptr fn_controls_window(vlist&, Interp& i) {
    if (controls().empty()) i.bad("no controls declared: (control name lo hi value) first");
    if (std::getenv("MUSIL_NOSHOW")) return v_nil();
    int w = 520, h = std::min(700, 80 + (int)controls().size() * 28);
    try { plot_ensure_window(true, w, h); } catch (std::exception& e) { i.bad(e.what()); }
    SetWindowTitle("musil controls  (arrows change, Space toggles, Esc closes)");
    int sel = 0; const int fs = 16, row = 28;
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) break;
        i.idle();
        std::vector<control>& cs = controls(); if (cs.empty()) break;
        sel = std::max(0, std::min((int)cs.size() - 1, sel));
        if (IsKeyPressed(KEY_UP)) sel = std::max(0, sel - 1);
        if (IsKeyPressed(KEY_DOWN)) sel = std::min((int)cs.size() - 1, sel + 1);
        control& c = cs[sel]; bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        double step = (c.hi - c.lo) * (shift ? 0.1 : 0.01); bool changed = false;
        if (c.toggle) { if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) { c.value = c.value > 0.5 ? 0 : 1; changed = true; } }
        else {
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) { c.value = std::max(c.lo, c.value - step); changed = true; }
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) { c.value = std::min(c.hi, c.value + step); changed = true; }
            if (IsKeyPressed(KEY_ZERO)) { c.value = c.lo; changed = true; }
        }
        if (changed) apply_control(i, c);
        int W = GetScreenWidth();
        BeginDrawing(); ClearBackground({ 246, 246, 244, 255 });
        plot_text("controls   arrows change, Shift bigger steps, Space toggles, 0 resets, Esc closes", 12, 10, fs - 3, { 130, 130, 130, 255 });
        for (size_t k = 0; k < cs.size(); k++) {
            control& d = cs[k]; int y = 40 + (int)k * row; bool s = (int)k == sel;
            if (s) DrawRectangle(6, y - 4, W - 12, row, { 226, 236, 250, 255 });
            plot_text(d.name, 14, (float)y, fs, s ? Color{ 26, 60, 120, 255 } : Color{ 40, 40, 40, 255 });
            int bx = 150, bw = W - bx - 90;
            if (d.toggle) { DrawRectangleLines(bx, y + 2, 16, 16, { 130, 130, 130, 255 }); if (d.value > 0.5) DrawRectangle(bx + 3, y + 5, 10, 10, { 30, 110, 210, 255 }); }
            else { DrawRectangle(bx, y + 8, bw, 4, { 200, 200, 196, 255 }); int px = bx + (int)((d.value - d.lo) / (d.hi - d.lo) * bw); DrawCircle(px, y + 10, 7, s ? Color{ 26, 60, 120, 255 } : Color{ 30, 110, 210, 255 }); }
            std::string vs = d.toggle ? (d.value > 0.5 ? "on" : "off") : tick_label(d.value);
            plot_text(vs, (float)(W - 12) - plot_text_width(vs, fs - 2), (float)y, fs - 2, { 90, 90, 90, 255 });
        }
        EndDrawing();
    }
    CloseWindow();
    return v_nil();
}
inline void add_controls_window(Interp& i) { i.def("controls", fn_controls_window, 0, 0); }

} // namespace musil

// controls.h — the controls window (FLTK): a slider or a check box for every control declared
// with control / toggle in live. Moving one queues an edit that the interpreter applies in its
// idle time (so the window works from the command line and from the IDE, whose interpreter
// runs on another thread); the window follows changes made from code or OSC.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
#pragma once
#include "core.h"
#include "live.h"
#include "plot.h"
#include <FL/Fl_Window.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Scroll.H>

namespace musil {

struct controls_window : Fl_Double_Window {
    struct row { std::string name; Fl_Value_Slider* slider = nullptr; Fl_Check_Button* check = nullptr; };
    std::vector<row> rows; Fl_Scroll* scroll;
    controls_window() : Fl_Double_Window(460, 100, "musil controls") {
        scroll = new Fl_Scroll(0, 0, 460, 100); scroll->type(Fl_Scroll::VERTICAL); end();
        rebuild(); Fl::add_timeout(0.1, tick, this);
        callback([](Fl_Widget* w, void*) { w->hide(); });
    }
    ~controls_window() override { Fl::remove_timeout(tick, this); }
    static void slider_cb(Fl_Widget* w, void* d) { row* r = (row*)d; queue_control_edit(r->name, ((Fl_Value_Slider*)w)->value()); }
    static void check_cb(Fl_Widget* w, void* d) { row* r = (row*)d; queue_control_edit(r->name, ((Fl_Check_Button*)w)->value() ? 1 : 0); }
    void rebuild() {
        scroll->clear(); rows.clear(); rows.reserve(controls().size());
        scroll->begin();
        int y = 8;
        for (auto& c : controls()) {
            rows.push_back({ c.name, nullptr, nullptr }); row& r = rows.back();
            if (c.toggle) { r.check = new Fl_Check_Button(10, y, 430, 24, r.name.c_str()); r.check->value(c.value > 0.5); r.check->callback(check_cb, &r); }
            else { r.slider = new Fl_Value_Slider(120, y, 320, 24, r.name.c_str()); r.slider->type(FL_HOR_NICE_SLIDER); r.slider->align(FL_ALIGN_LEFT); r.slider->bounds(c.lo, c.hi); r.slider->value(c.value); r.slider->callback(slider_cb, &r); }
            y += 30;
        }
        scroll->end();
        int h = std::min(700, y + 8); size(460, h); scroll->size(460, h);
        redraw();
    }
    static void tick(void* p) {                      // follow the registry: new controls, values moved by code or OSC
        controls_window* w = (controls_window*)p;
        if (w->rows.size() != controls().size()) w->rebuild();
        else for (size_t k = 0; k < w->rows.size(); k++) {
            control& c = controls()[k]; row& r = w->rows[k];
            if (r.name != c.name) { w->rebuild(); break; }
            if (r.slider && !Fl::pushed()) { if (r.slider->minimum() != c.lo || r.slider->maximum() != c.hi) r.slider->bounds(c.lo, c.hi); if (r.slider->value() != c.value) r.slider->value(c.value); }
            if (r.check && (r.check->value() != 0) != (c.value > 0.5)) r.check->value(c.value > 0.5);
        }
        if (w->shown()) Fl::repeat_timeout(0.1, tick, p);
    }
};
inline controls_window*& the_controls_window() { static controls_window* w = nullptr; return w; }
inline void controls_open() { if (!the_controls_window()) the_controls_window() = new controls_window(); else the_controls_window()->rebuild(); the_controls_window()->show(); Fl::add_timeout(0.1, controls_window::tick, the_controls_window()); }
inline void controls_awake_cb(void*) { controls_open(); }
// (controls) open the controls window and return; the sound, the loops and the port keep running
inline vptr fn_controls_window(vlist&, Interp& i) {
    if (controls().empty()) i.bad("no controls declared: (control name lo hi value) first");
    if (std::getenv("MUSIL_NOSHOW")) return v_nil();
    if (plot_needs_awake()) Fl::awake(controls_awake_cb, nullptr); else { controls_open(); Fl::check(); }
    return v_nil();
}
inline bool controls_window_open() { return the_controls_window() && the_controls_window()->shown(); }
// (controls-open?) => is the controls window open? A script can wait on it: while (controls-open?) { sleep 0.1 }
inline vptr fn_controls_open(vlist&, Interp&) { return v_bool(controls_window_open()); }
inline void add_controls_window(Interp& i) { i.def("controls", fn_controls_window, 0, 0); i.def("controls-open?", fn_controls_open, 0, 0); }

} // namespace musil

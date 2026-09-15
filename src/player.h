// player.h — the score player (FLTK): the roll as a window that plays, edits, renders and exports a score.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// (display s) opens it. The roll shows the events as bars (rows: instruments in orchestral order, then
// files, buffers, synths, calls). Keys: A D pan in time, + - zoom in time, W S scroll rows, Z X zoom rows,
// R or 0 reset; the wheel zooms in time. Hovering shows an event; click in the background to place the
// cursor; drag a bar to move its event, drag its end to change its length: the score's own numbers change.
// Buttons: Play (from the cursor; Stop while playing), Render (a file dialog; the mix as a WAV, stereo),
// Export (a file dialog; a Musil file defining (generated-score)). Space is Play/Stop.
//
// The window lives on the FLTK thread; everything that touches sound or the score goes to the interpreter
// through player_submit (a line of Musil): in the CLI it runs at once, in the IDE it is queued. Playback
// is scheduled (score-schedule) and returns; the cursor follows the audio clock; when it passes the end
// the synths are freed.

#pragma once
#include "core.h"
#include "live.h"
#include "plot.h"
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <atomic>
#include <mutex>

namespace musil {

// how the player hands Musil code to the interpreter: hosts set it (the IDE queues; the CLI runs it)
inline std::function<void(const std::string&)>& player_submit() { static std::function<void(const std::string&)> f; return f; }

struct player_state {
    int id; vptr score; figure fig; Fl_Double_Window* win = nullptr; struct player_widget* roll = nullptr; Fl_Button* play_btn = nullptr; Fl_Box* status = nullptr;
    std::mutex m; bool playing = false; double t0 = 0, end = 0, from = 0; vlist synths;   // set by the interpreter thread
    double cursor = 0;
};
inline std::vector<std::shared_ptr<player_state>>& players() { static std::vector<std::shared_ptr<player_state>> p; return p; }
inline std::shared_ptr<player_state> player_by_id(int id) { for (auto& p : players()) if (p->id == id) return p; return nullptr; }

struct player_widget : Fl_Widget {
    std::shared_ptr<player_state> st; plot_view view; int lastx = 0, lasty = 0; int grabbed = -1; bool grab_end = false;
    player_widget(int x, int y, int w, int h, std::shared_ptr<player_state> s) : Fl_Widget(x, y, w, h), st(std::move(s)) {}
    plot_layer& roll() { for (auto& L : st->fig.layers) if (L.kind == "roll") return L; return st->fig.layers[0]; }
    void draw() override {
        render_figure_at(st->fig, x(), y(), w(), h() - 18, view);
        // the cursor, and the playing position
        roll_geom g = roll_geometry(st->fig, x(), y(), w(), h() - 18, view);
        auto X = [&](double t) { return g.left + (t - g.xmin) / (g.xmax - g.xmin) * g.pw; };
        double pos; bool playing; { std::lock_guard<std::mutex> lk(st->m); playing = st->playing; pos = playing ? st->from + (live_now() - st->t0) : st->cursor; }
        if (pos >= g.xmin && pos <= g.xmax) { fl_color(200, 30, 30); fl_line_style(FL_SOLID, 2); fl_line((int)X(pos), g.top, (int)X(pos), g.top + (int)g.ph); fl_line_style(0); }
        fl_color(246, 246, 244); fl_rectf(x(), y() + h() - 18, w(), 18);
        fl_font(FL_HELVETICA, 11); fl_color(150, 150, 150);
        fl_draw("A D pan   + - zoom   W S scroll rows   Z X zoom rows   R reset   click sets the cursor   drag a bar to move it, its end to stretch it   Space plays", x() + 8, y() + h() - 5);
    }
    int handle(int e) override {
        int H = h() - 18; plot_layer& L = roll();
        switch (e) {
        case FL_FOCUS: case FL_UNFOCUS: case FL_ENTER: case FL_LEAVE: return 1;
        case FL_MOVE: plot_cursor(st->fig, view, x(), y(), w(), H, Fl::event_x(), Fl::event_y()); redraw(); return 1;
        case FL_PUSH: {
            lastx = Fl::event_x(); lasty = Fl::event_y(); take_focus(); grabbed = -1;
            roll_geom g = roll_geometry(st->fig, x(), y(), w(), H, view);
            int k = roll_hit(st->fig, L, x(), y(), w(), H, view, lastx, lasty);
            if (k >= 0) { grabbed = k; int x1 = (int)(g.left + (L.bars[k].start + L.bars[k].dur - g.xmin) / (g.xmax - g.xmin) * g.pw); grab_end = lastx >= x1 - 6; }
            else if (lastx >= g.left && lastx <= g.left + g.pw) { std::lock_guard<std::mutex> lk(st->m); st->cursor = std::max(0.0, g.xmin + (lastx - g.left) / g.pw * (g.xmax - g.xmin)); }
            redraw(); return 1;
        }
        case FL_RELEASE: grabbed = -1; return 1;
        case FL_DRAG: {
            roll_geom g = roll_geometry(st->fig, x(), y(), w(), H, view); double dt = (Fl::event_x() - lastx) / g.pw * (g.xmax - g.xmin);
            if (grabbed >= 0 && grabbed < (int)L.bars.size()) {
                roll_bar& b = L.bars[(size_t)grabbed];
                if (grab_end) { b.dur = std::max(0.01, b.dur + dt); if (b.dur_ref) b.dur_ref->num[0] = b.dur; }
                else { b.start = std::max(0.0, b.start + dt); if (b.at_ref) b.at_ref->num[0] = b.start; }
                view.has_pick = true; view.px = grabbed;
            } else { view.xmin = g.xmin - dt; view.xmax = g.xmax - dt; view.zoomed = true; }
            lastx = Fl::event_x(); lasty = Fl::event_y(); redraw(); return 1;
        }
        case FL_MOUSEWHEEL: plot_wheel(st->fig, view, Fl::event_dy()); redraw(); return 1;
        case FL_KEYDOWN: {
            int k = Fl::event_key(); const char* t = Fl::event_text(); char c = t && t[0] ? t[0] : 0;
            if (k == FL_Escape || c == 'q') { if (player_submit()) player_submit()("(player-stop " + std::to_string(st->id) + ")"); window()->hide(); return 1; }
            if (c == ' ') { player_toggle(); return 1; }
            if (c == 'z') { view.row_scale = std::min(8.0, view.row_scale * 1.25); }
            else if (c == 'x') { view.row_scale = std::max(1.0, view.row_scale / 1.25); if (view.row_scale <= 1.0001) view.row_offset = 0; }
            else if (c == 'w') { view.row_offset = std::max(0.0, view.row_offset - 1); }
            else if (c == 's') { double rows = (double)L.rows.size(); view.row_offset = std::min(std::max(0.0, rows - rows / std::max(1.0, view.row_scale)), view.row_offset + 1); }
            else if (c == 'r' || c == '0') { view.zoomed = false; view.row_scale = 1; view.row_offset = 0; }
            else plot_key(st->fig, view, k >= FL_Left && k <= FL_Down ? k : c, false);
            redraw(); return 1;
        }
        }
        return Fl_Widget::handle(e);
    }
    void player_toggle();
};

inline void player_status(player_state& p, const std::string& s) { if (p.status) { p.status->copy_label(s.c_str()); p.status->redraw(); } }
inline void player_widget::player_toggle() {
    bool playing; { std::lock_guard<std::mutex> lk(st->m); playing = st->playing; }
    if (!player_submit()) { player_status(*st, "no interpreter to play with"); return; }
    if (playing) { player_submit()("(player-stop " + std::to_string(st->id) + ")"); if (st->play_btn) st->play_btn->copy_label("Play"); }
    else { double from; { std::lock_guard<std::mutex> lk(st->m); from = st->cursor; } if (st->play_btn) { st->play_btn->copy_label("Stop"); st->play_btn->deactivate(); } player_status(*st, "preparing..."); player_submit()("(player-play " + std::to_string(st->id) + " " + std::to_string(from) + ")"); }
}
inline void player_tick(void* p) {                     // the cursor follows the clock; the end frees the synths
    auto* raw = (player_state*)p; auto st = player_by_id(raw->id); if (!st || !st->win || !st->win->shown()) return;
    bool playing, over = false; { std::lock_guard<std::mutex> lk(st->m); playing = st->playing; if (playing && live_now() >= st->end) over = true; }
    if (over && player_submit()) player_submit()("(player-finish " + std::to_string(st->id) + ")");
    if (st->play_btn) {
        bool preparing = st->status && std::string(st->status->label()) == "preparing...";
        if (playing) { st->play_btn->copy_label("Stop"); st->play_btn->activate(); if (preparing) player_status(*st, ""); }
        else if (!preparing) { st->play_btn->copy_label("Play"); st->play_btn->activate(); }
    }
    if (playing) st->roll->redraw();
    Fl::repeat_timeout(0.05, player_tick, p);
}
inline std::string player_ask_path(const char* title, const char* filter, const char* preset) {
    Fl_Native_File_Chooser ch; ch.title(title); ch.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE); ch.filter(filter); ch.preset_file(preset); ch.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    if (ch.show() != 0) return ""; return ch.filename();
}
inline std::string mu_quoted(const std::string& s) { std::string o = "\""; for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; } return o + "\""; }
inline void player_open(std::shared_ptr<player_state> st) {
    ui_style_once();
    int rows = 1; for (auto& L : st->fig.layers) if (L.kind == "roll") rows = std::max<int>(1, (int)L.rows.size());
    int W = 1000, H = std::min(760, std::max(340, 150 + 28 * rows));
    Fl_Double_Window* win = new Fl_Double_Window((Fl::w() - W) / 2, (Fl::h() - H) / 2, W, H, st->fig.title.empty() ? "musil score" : st->fig.title.c_str());
    int x = 8;
    auto button = [&](const char* label, int w, Fl_Callback* cb) { Fl_Button* b = new Fl_Button(x, 6, w, 26, label); b->callback(cb, st.get()); b->clear_visible_focus(); x += w + 6; return b; };
    st->play_btn = button("Play", 70, [](Fl_Widget*, void* d) { auto* p = (player_state*)d; if (p->roll) p->roll->player_toggle(); });
    button("Render...", 90, [](Fl_Widget*, void* d) { auto* p = (player_state*)d; std::string path = player_ask_path("Render the score to a WAV", "WAV\t*.wav", (p->fig.title + ".wav").c_str()); if (path.empty() || !player_submit()) return; player_status(*p, "rendering " + path); player_submit()("(player-render " + std::to_string(p->id) + " " + mu_quoted(path) + ")"); });
    button("Export...", 90, [](Fl_Widget*, void* d) { auto* p = (player_state*)d; std::string path = player_ask_path("Export the score as Musil", "Musil\t*.mu", (p->fig.title + ".mu").c_str()); if (path.empty() || !player_submit()) return; player_submit()("(player-export " + std::to_string(p->id) + " " + mu_quoted(path) + ")"); player_status(*p, "exported " + path + ": (generated-score) rebuilds it"); });
    st->status = new Fl_Box(x + 10, 6, W - x - 18, 26, ""); st->status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE); st->status->labelsize(11); st->status->labelcolor(fl_rgb_color(110, 110, 110));
    st->roll = new player_widget(0, 38, W, H - 38, st);
    win->resizable(st->roll); win->end(); win->size_range(600, 260);
    win->callback([](Fl_Widget* w, void* d) { auto* p = (player_state*)d; if (player_submit()) player_submit()("(player-stop " + std::to_string(p->id) + ")"); w->hide(); }, st.get());
    st->win = win; win->show(); st->roll->take_focus();
    Fl::add_timeout(0.05, player_tick, st.get());
}
inline void player_awake_cb(void* p) { auto* raw = (std::shared_ptr<player_state>*)p; player_open(*raw); delete raw; }

inline bool player_window_open() { for (auto& p : players()) if (p->win && p->win->shown()) return true; return false; }

// --- builtins ---
// (score-player s) open the score in the player window (what display does); => the player's id
inline vptr fn_score_player(vlist& a, Interp& i) {
    vptr roll_fn = i.global->find("score-roll"); if (!roll_fn || roll_fn->t != Value::FN) i.bad("score-player: music.mu is not loaded");
    vptr figv = i.call_fn(roll_fn, { a[0] });
    auto st = std::make_shared<player_state>(); static int next_id = 1; st->id = next_id++; st->score = a[0];
    try { st->fig = parse_figure(figv); } catch (std::exception& e) { i.bad(e.what()); }
    players().push_back(st);
    if (std::getenv("MUSIL_NOSHOW")) return v_num(st->id);
    if (plot_needs_awake()) Fl::awake(player_awake_cb, new std::shared_ptr<player_state>(st)); else { player_open(st); Fl::check(); }
    return v_num(st->id);
}
// (player-score id) => the score a player shows
inline vptr fn_player_score(vlist& a, Interp& i) { auto p = player_by_id((int)i.scalar(a[0])); if (!p) i.bad("no player " + str_of(a[0])); return p->score; }
// (player-play id from) start the player's score from a time (the interpreter thread; buttons call it)
inline vptr fn_player_play(vlist& a, Interp& i) {
    auto p = player_by_id((int)i.scalar(a[0])); if (!p) i.bad("no player " + str_of(a[0]));
    vptr sched = i.global->find("score-schedule"); if (!sched || sched->t != Value::FN) i.bad("player-play: music.mu is not loaded");
    { vlist none; try { live_stop_all(none, i); } catch (...) {} }
    vptr r = i.call_fn(sched, { p->score, v_num(1.0), a[1] });
    std::lock_guard<std::mutex> lk(p->m); p->playing = true; p->synths = r->l[0]->l; p->end = r->l[1]->num[0]; p->t0 = r->l[2]->num[0]; p->from = i.scalar(a[1]);
    return v_nil();
}
inline void player_release(player_state& p, Interp& i) {
    vlist ids; { std::lock_guard<std::mutex> lk(p.m); ids = p.synths; p.synths.clear(); p.playing = false; }
    for (auto& id : ids) { vlist one = { id }; try { live_free(one, i); } catch (...) {} }
}
// (player-stop id) stop the player's playback; (player-finish id) tidy up after it ended
inline vptr fn_player_stop(vlist& a, Interp& i) { auto p = player_by_id((int)i.scalar(a[0])); if (!p) return v_nil(); { vlist none; try { live_stop_all(none, i); } catch (...) {} } { std::lock_guard<std::mutex> lk(p->m); p->cursor = std::max(0.0, std::min(p->end - p->t0 + p->from, p->from + (live_now() - p->t0))); } player_release(*p, i); return v_nil(); }
inline vptr fn_player_finish(vlist& a, Interp& i) { auto p = player_by_id((int)i.scalar(a[0])); if (!p) return v_nil(); { std::lock_guard<std::mutex> lk(p->m); p->cursor = 0; } player_release(*p, i); return v_nil(); }
// (player-render id path) render the player's score (stereo) to a WAV; (player-export id path) write it as Musil
inline vptr fn_player_render(vlist& a, Interp& i) { auto p = player_by_id((int)i.scalar(a[0])); if (!p) i.bad("no player"); vptr f = i.global->find("render"); i.call_fn(f, { p->score, a[1], v_str("stereo") }); *i.out << "rendered " << i.str(a[1]) << "\n" << std::flush; return v_nil(); }
inline vptr fn_player_export(vlist& a, Interp& i) { auto p = player_by_id((int)i.scalar(a[0])); if (!p) i.bad("no player"); vptr f = i.global->find("score-export"); i.call_fn(f, { p->score, a[1] }); *i.out << "exported " << i.str(a[1]) << "\n" << std::flush; return v_nil(); }
// (player-playing? id) => is it playing; (player-cursor id) => the cursor's time
inline vptr fn_player_playing(vlist& a, Interp& i) { auto p = player_by_id((int)i.scalar(a[0])); if (!p) return v_bool(false); std::lock_guard<std::mutex> lk(p->m); return v_bool(p->playing); }
inline vptr fn_player_cursor(vlist& a, Interp& i) { auto p = player_by_id((int)i.scalar(a[0])); if (!p) return v_num(0); std::lock_guard<std::mutex> lk(p->m); return v_num(p->cursor); }
inline void add_player(Interp& i) {
    i.def("score-player", fn_score_player, 1, 1); i.def("player-score", fn_player_score, 1, 1);
    i.def("player-play", fn_player_play, 2, 2); i.def("player-stop", fn_player_stop, 1, 1); i.def("player-finish", fn_player_finish, 1, 1);
    i.def("player-render", fn_player_render, 2, 2); i.def("player-export", fn_player_export, 2, 2);
    i.def("player-playing?", fn_player_playing, 1, 1); i.def("player-cursor", fn_player_cursor, 1, 1);
    if (!player_submit()) { Interp* ip = &i; player_submit() = [ip](const std::string& code) { try { ip->run(code, "<player>"); } catch (std::exception& e) { *ip->out << "player: " << e.what() << "\n" << std::flush; } }; }   // the CLI: run at once; the IDE replaces this with its queue
}

} // namespace musil

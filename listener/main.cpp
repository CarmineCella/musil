// main.cpp — the Musil Listener: a window to run Musil, watch files, plot and play.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// Layout
//   left, top     the last figure shown by (show fig)      [hidden until there is one]
//   left, bottom  the console: everything printed, and a line to type Musil into
//   right, top    files: drop .mu files on the window to run them; they are watched and
//                 re-run when saved; click one to run it again
//   right, bottom variables: the globals, with a preview; double-click prints the value
//
// Threads
//   The window (raylib) owns the main thread. The interpreter runs on a worker thread
//   and never touches the window: print goes through a queue of lines, show through a
//   queue of figures, the variable list is a snapshot posted after every run, and Stop
//   sets a flag that the interpreter checks in yield_fn. Audio is a raylib stream fed
//   from the main thread with samples the worker queued with (play signal sr).

#include "musil.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

using namespace musil;
namespace fs = std::filesystem;

// --- shared state between the interpreter thread and the window --------------------
struct shared {
    std::mutex m;
    std::condition_variable cv;
    std::deque<std::string> commands;                 // to the interpreter
    std::deque<std::string> lines;                    // from print
    std::vector<std::pair<std::string, std::string>> vars;   // name, preview
    std::vector<figure> figures;                      // from show / save-png
    std::vector<double> audio; double audio_sr = 44100; bool audio_new = false;   // from play
    std::atomic<bool> stop{false}, busy{false}, quit{false};
    void post_line(const std::string& s) { std::lock_guard<std::mutex> g(m); lines.push_back(s); }
} S;

// std::ostream that turns the interpreter's output into console lines
struct line_buf : std::streambuf {
    std::string cur;
    int overflow(int c) override {
        if (c == '\n') { S.post_line(cur); cur.clear(); } else cur += (char)c;
        return c;
    }
    int sync() override { if (!cur.empty()) { S.post_line(cur); cur.clear(); } return 0; }
};

// --- builtins the Listener adds to the interpreter -------------------------------
static vptr listener_show(vlist& a, Interp& i) {
    figure f; try { f = parse_figure(a[0]); } catch (std::exception& e) { i.bad(e.what()); }
    std::lock_guard<std::mutex> g(S.m); S.figures.push_back(std::move(f)); return v_nil();
}
static vptr listener_save_png(vlist& a, Interp& i) {   // rendered on the main thread: queued with its path
    figure f; try { f = parse_figure(a[0]); } catch (std::exception& e) { i.bad(e.what()); }
    f.title = "\x01" + i.str(a[1]) + "\x01" + f.title;    // the main thread unpacks the path
    std::lock_guard<std::mutex> g(S.m); S.figures.push_back(std::move(f)); return v_nil();
}
static vptr listener_play(vlist& a, Interp& i) {     // (play signal sr): a vector or (list left right)
    double sr = i.scalar(a[1]);
    std::vector<double> buf;
    if (a[0]->t == Value::NUM) { const varr& v = a[0]->num; for (size_t k = 0; k < v.size(); k++) { buf.push_back(v[k]); buf.push_back(v[k]); } }
    else { vlist& ch = i.list(a[0]); if (ch.size() != 2) i.bad("expected a vector or (list left right)");
        const varr& l = i.num(ch[0]); const varr& r = i.num(ch[1]); if (l.size() != r.size()) i.bad("channels must have the same length");
        for (size_t k = 0; k < l.size(); k++) { buf.push_back(l[k]); buf.push_back(r[k]); } }
    std::lock_guard<std::mutex> g(S.m); S.audio = std::move(buf); S.audio_sr = sr; S.audio_new = true; return v_nil();
}
static vptr listener_stop_audio(vlist&, Interp&) { std::lock_guard<std::mutex> g(S.m); S.audio.clear(); S.audio_new = true; return v_nil(); }

// --- the interpreter thread ----------------------------------------------------------
static std::string preview(const vptr& v) {
    std::string s = str_of(v);
    if (s.size() > 40) s = s.substr(0, 37) + "...";
    for (auto& c : s) if (c == '\n') c = ' ';
    return s;
}
static void snapshot_vars(Interp& I) {   // values first (alphabetical), then user functions; builtins are left out
    std::vector<std::pair<std::string, std::string>> vals, fns;
    for (auto& kv : I.global->vars) {
        if (kv.second->t == Value::FN) { if (!kv.second->op) fns.push_back({ kv.first, "" }); }
        else vals.push_back({ kv.first, preview(kv.second) });
    }
    std::sort(vals.begin(), vals.end()); std::sort(fns.begin(), fns.end());
    vals.insert(vals.end(), fns.begin(), fns.end());
    std::lock_guard<std::mutex> g(S.m); S.vars = std::move(vals);
}
static void interpreter_thread(std::vector<std::string> load_paths) {
    line_buf buf; std::ostream out(&buf);
    Interp I; make_env(I);
    for (auto& p : load_paths) I.load_path.push_back(p);
    I.out = &out;
    I.def("show", listener_show, 1, 3);
    I.def("save-png", listener_save_png, 2, 4);
    I.def("play", listener_play, 2, 2);
    I.def("stop-audio", listener_stop_audio, 0, 0);
    I.def("args", v_list({}));
    I.yield_fn = [&]() { if (S.stop) { S.stop = false; I.err("interrupted"); } };
    S.post_line("musil " MUSIL_VERSION " listener: type Musil below, drop .mu files on the window");
    snapshot_vars(I);
    while (!S.quit) {
        std::string cmd;
        { std::unique_lock<std::mutex> lk(S.m); S.cv.wait(lk, [] { return !S.commands.empty() || S.quit; }); if (S.quit) break; cmd = S.commands.front(); S.commands.pop_front(); }
        S.busy = true;
        try {
            if (cmd.rfind("\x02", 0) == 0) {                     // run a file: not through load, so it always re-runs
                std::string path = cmd.substr(1);
                std::ifstream f(path); if (!f) I.err("cannot open " + path);
                std::stringstream ss; ss << f.rdbuf();
                S.post_line("> " + fs::path(path).filename().string());
                I.run(ss.str(), path);
            } else {
                S.post_line("> " + cmd);
                vptr r = I.run(cmd, "<listener>");
                if (r && r->t != Value::NIL) S.post_line(str_of(r));
            }
        } catch (Exit_signal&) { S.quit = true; }
        catch (std::exception& e) { S.post_line(std::string("error: ") + e.what()); }
        buf.sync();
        I.call_stack.clear(); I.function_depth = 0; I.loop_depth = 0; I.stack_depth = 0;
        snapshot_vars(I);
        S.busy = false;
    }
}

// --- the window -------------------------------------------------------------------------
struct watched { std::string path; fs::file_time_type mtime; };
static std::string asset_path(const std::string& name) {
    std::vector<std::string> dirs;
#ifdef __APPLE__
    dirs.push_back(fs::path(GetApplicationDirectory()).parent_path().string() + "/Resources");
#endif
    dirs.push_back(std::string(GetApplicationDirectory()) + "assets");
    if (const char* home = std::getenv("HOME")) dirs.push_back(std::string(home) + "/.musil/assets");   // cmake --install puts the font there
#ifdef MUSIL_LISTENER_ASSETS
    dirs.push_back(MUSIL_LISTENER_ASSETS);
#endif
    for (auto& d : dirs) if (fs::exists(fs::path(d) / name)) return (fs::path(d) / name).string();
    return "";
}

int main(int argc, char** argv) {
    std::vector<std::string> load_paths;
#ifdef __APPLE__
    load_paths.push_back(fs::path(GetApplicationDirectory()).parent_path().string() + "/Resources/lib");
#endif
    load_paths.push_back(std::string(GetApplicationDirectory()) + "lib");
#ifdef MUSIL_SOURCE_LIB
    load_paths.push_back(MUSIL_SOURCE_LIB);
#endif

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_WINDOW_MAXIMIZED);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1240, 780, "Musil");
    MaximizeWindow();
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    InitAudioDevice();
    std::string fp = asset_path("JetBrainsMono-Regular.ttf");
    Font font = fp.empty() ? GetFontDefault() : LoadFontEx(fp.c_str(), 32, nullptr, 0);
    if (!fp.empty()) { SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR); plot_get_font().font = font; plot_get_font().custom = true; }
    constexpr float FS = 16;
    auto text = [&](const std::string& s, float x, float y, Color c, float size = 16.0f) { DrawTextEx(font, s.c_str(), { x, y }, size, 0.5f, c); };
    auto width = [&](const std::string& s) { return MeasureTextEx(font, s.c_str(), FS, 0.5f).x; };
    const Color BG = { 246, 246, 244, 255 }, PANEL = { 255, 255, 255, 255 }, INK = { 40, 40, 40, 255 }, DIM = { 130, 130, 130, 255 },
                ACCENT = { 26, 60, 120, 255 }, ERR = { 190, 40, 40, 255 }, SEL = { 226, 236, 250, 255 };

    std::thread worker(interpreter_thread, load_paths);
    auto submit = [&](const std::string& c) { { std::lock_guard<std::mutex> g(S.m); S.commands.push_back(c); } S.cv.notify_one(); };

    std::vector<std::string> console; std::string input; std::vector<std::string> history; int hist_pos = -1;
    int scroll = 0, var_scroll = 0; std::vector<watched> files; int sel_file = -1;
    RenderTexture2D plot_rt = {}; bool has_plot = false; figure current; plot_view view;
    AudioStream stream = {}; std::vector<float> pcm; size_t pcm_pos = 0; double stream_sr = 0;
    double last_watch = 0;
    auto ensure_stream = [&](double sr) {
        if (IsAudioStreamValid(stream) && stream_sr == sr) return;
        if (IsAudioStreamValid(stream)) UnloadAudioStream(stream);
        stream = LoadAudioStream((unsigned)sr, 32, 2); stream_sr = sr; PlayAudioStream(stream);
    };
    auto add_file = [&](const std::string& p) {
        for (auto& w : files) if (w.path == p) return;
        std::error_code ec; files.push_back({ p, fs::last_write_time(p, ec) });
    };
    for (int k = 1; k < argc; k++) { add_file(argv[k]); submit(std::string("\x02") + argv[k]); }

    while (!WindowShouldClose() && !S.quit) {
        // ---- input ----
        if (IsFileDropped()) { FilePathList d = LoadDroppedFiles(); for (unsigned k = 0; k < d.count; k++) { add_file(d.paths[k]); submit(std::string("\x02") + d.paths[k]); } UnloadDroppedFiles(d); }
        int ch; while ((ch = GetCharPressed()) > 0) { if (ch >= 32 && ch < 127) input += (char)ch; }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !input.empty()) input.pop_back();
        if (IsKeyPressed(KEY_ENTER) && !input.empty()) { history.push_back(input); hist_pos = -1; submit(input); input.clear(); }
        if (IsKeyPressed(KEY_UP) && !history.empty()) { hist_pos = hist_pos < 0 ? (int)history.size() - 1 : std::max(0, hist_pos - 1); input = history[hist_pos]; }
        if (IsKeyPressed(KEY_DOWN) && hist_pos >= 0) { hist_pos = std::min((int)history.size() - 1, hist_pos + 1); input = history[hist_pos]; }
        if (IsKeyPressed(KEY_ESCAPE)) { if (S.busy) S.stop = true; else if (has_plot) has_plot = false; }
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER)) && IsKeyPressed(KEY_C) && S.busy) S.stop = true;
        // file watcher, twice a second
        if (GetTime() - last_watch > 0.5) {
            last_watch = GetTime();
            for (auto& w : files) { std::error_code ec; auto t = fs::last_write_time(w.path, ec); if (!ec && t != w.mtime) { w.mtime = t; submit(std::string("\x02") + w.path); } }
        }
        // ---- collect what the interpreter produced ----
        std::vector<figure> new_figs; bool new_audio = false; std::vector<double> audio; double asr = 44100;
        { std::lock_guard<std::mutex> g(S.m);
          while (!S.lines.empty()) { console.push_back(S.lines.front()); S.lines.pop_front(); scroll = 0; }
          new_figs.swap(S.figures);
          if (S.audio_new) { new_audio = true; audio.swap(S.audio); asr = S.audio_sr; S.audio_new = false; } }
        for (auto& f : new_figs) {
            if (!f.title.empty() && f.title[0] == '\x01') {          // a save-png request
                size_t e = f.title.find('\x01', 1); std::string path = f.title.substr(1, e - 1); f.title = f.title.substr(e + 1);
                try { figure_to_png(f, path, 900, 560); console.push_back("saved " + path); } catch (std::exception& ex) { console.push_back(std::string("error: ") + ex.what()); }
            } else { plot_cache_clear(); current = f; has_plot = true; view = plot_view(); }
        }
        if (new_audio) { pcm.assign(audio.begin(), audio.end()); pcm_pos = 0; if (!pcm.empty()) ensure_stream(asr); }
        if (IsAudioStreamValid(stream) && IsAudioStreamProcessed(stream)) {
            const unsigned frames = 4096; std::vector<float> chunk(frames * 2, 0.0f);
            for (unsigned k = 0; k < frames * 2 && pcm_pos < pcm.size(); k++) chunk[k] = pcm[pcm_pos++];
            UpdateAudioStream(stream, chunk.data(), frames);
        }
        if (console.size() > 5000) console.erase(console.begin(), console.begin() + 1000);

        // ---- layout ----
        int W = GetScreenWidth(), H = GetScreenHeight();
        int right_w = 300, left_w = W - right_w - 24, x0 = 12, y0 = 12;
        int plot_h = has_plot ? (int)(H * 0.5) : 0;
        Rectangle plot_r = { (float)x0, (float)y0, (float)left_w, (float)plot_h };
        Rectangle cons_r = { (float)x0, (float)(y0 + plot_h + (has_plot ? 12 : 0)), (float)left_w, (float)(H - y0 - plot_h - (has_plot ? 12 : 0) - 12 - 34) };
        Rectangle in_r = { (float)x0, cons_r.y + cons_r.height + 6, (float)left_w, 28 };
        Rectangle files_r = { (float)(x0 + left_w + 12), (float)y0, (float)right_w, (float)(H * 0.35f) };
        Rectangle vars_r = { files_r.x, files_r.y + files_r.height + 12, (float)right_w, (float)(H - y0 - files_r.height - 24) };
        Vector2 mouse = GetMousePosition();

        BeginDrawing();
        ClearBackground(BG);
        // plot panel
        if (has_plot) {
            if (plot_rt.texture.width != (int)plot_r.width || plot_rt.texture.height != (int)plot_r.height) { if (plot_rt.id) UnloadRenderTexture(plot_rt); plot_rt = LoadRenderTexture((int)plot_r.width, (int)plot_r.height); }
            Rectangle close = { plot_r.x + plot_r.width - 26, plot_r.y + 4, 22, 22 };
            Rectangle save = { plot_r.x + plot_r.width - 84, plot_r.y + 4, 52, 22 };
            bool on_button = CheckCollisionPointRec(mouse, close) || CheckCollisionPointRec(mouse, save);
            if (!on_button) plot_interact(current, view, view, plot_r, mouse);
            if (IsKeyPressed(KEY_S) && CheckCollisionPointRec(mouse, plot_r)) { std::string p = plot_save_name(current); try { figure_to_png(current, p, (int)plot_r.width, (int)plot_r.height); console.push_back("saved " + fs::absolute(p).string()); } catch (std::exception& e) { console.push_back(std::string("error: ") + e.what()); } }
            BeginTextureMode(plot_rt); render_figure(current, (int)plot_r.width, (int)plot_r.height, view); EndTextureMode();
            DrawTextureRec(plot_rt.texture, { 0, 0, (float)plot_rt.texture.width, -(float)plot_rt.texture.height }, { plot_r.x, plot_r.y }, WHITE);
            DrawRectangleLinesEx(plot_r, 1, DIM);
            bool hs = CheckCollisionPointRec(mouse, save);
            DrawRectangleRec(save, hs ? SEL : PANEL); DrawRectangleLinesEx(save, 1, DIM); text("save", save.x + 10, save.y + 3, hs ? ACCENT : INK, FS - 2);
            if (hs && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { std::string p = plot_save_name(current); try { figure_to_png(current, p, (int)plot_r.width, (int)plot_r.height); console.push_back("saved " + fs::absolute(p).string()); } catch (std::exception& e) { console.push_back(std::string("error: ") + e.what()); } }
            text("x", close.x + 6, close.y + 1, DIM);
            if (CheckCollisionPointRec(mouse, close) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { has_plot = false; plot_cache_clear(); }
            text("wheel zooms, drag pans, r resets, s saves", plot_r.x + 8, plot_r.y + 6, DIM, FS - 3);
        }
        // console
        DrawRectangleRec(cons_r, PANEL); DrawRectangleLinesEx(cons_r, 1, DIM);
        if (CheckCollisionPointRec(mouse, cons_r)) scroll = std::max(0, std::min((int)console.size(), scroll - (int)GetMouseWheelMove() * 3));
        int line_h = (int)FS + 4, visible = (int)((cons_r.height - 8) / line_h);
        int end = (int)console.size() - scroll, start = std::max(0, end - visible);
        BeginScissorMode((int)cons_r.x, (int)cons_r.y, (int)cons_r.width, (int)cons_r.height);
        for (int k = start; k < end; k++) {
            const std::string& l = console[k];
            Color c = l.rfind("error:", 0) == 0 ? ERR : l.rfind("> ", 0) == 0 ? ACCENT : INK;
            text(l, cons_r.x + 8, cons_r.y + 6 + (k - start) * line_h, c);
        }
        EndScissorMode();
        // input line
        DrawRectangleRec(in_r, PANEL); DrawRectangleLinesEx(in_r, 1, S.busy ? ACCENT : DIM);
        text(S.busy ? "running... (Esc stops)" : "> " + input + (((int)(GetTime() * 2) & 1) ? "_" : ""), in_r.x + 8, in_r.y + 5, S.busy ? DIM : INK);
        // files
        DrawRectangleRec(files_r, PANEL); DrawRectangleLinesEx(files_r, 1, DIM);
        text("files  (drop .mu here; saved files re-run)", files_r.x + 8, files_r.y + 6, DIM, FS - 2);
        {   // the manual, if it travels with the app (Resources/musil_manual.pdf) or the source tree (docs/)
            static std::string manual = asset_path("musil_manual.pdf");
            if (manual.empty()) { std::string d = asset_path("../docs/musil_manual.pdf"); manual = d; }
            if (!manual.empty()) {
                Rectangle b = { files_r.x + files_r.width - 84, files_r.y + 3, 78, 20 };
                bool hover = CheckCollisionPointRec(mouse, b);
                DrawRectangleRec(b, hover ? SEL : PANEL); DrawRectangleLinesEx(b, 1, DIM);
                text("reference", b.x + 8, b.y + 3, hover ? ACCENT : INK, FS - 2);
                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) OpenURL(("file://" + fs::absolute(manual).string()).c_str());
            }
        }
        for (size_t k = 0; k < files.size(); k++) {
            Rectangle row = { files_r.x + 2, files_r.y + 30 + k * line_h, files_r.width - 4, (float)line_h };
            bool hover = CheckCollisionPointRec(mouse, row);
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { sel_file = (int)k; submit(std::string("\x02") + files[k].path); }
            if ((int)k == sel_file) DrawRectangleRec(row, SEL);
            text(fs::path(files[k].path).filename().string(), row.x + 6, row.y + 2, hover ? ACCENT : INK);
        }
        if (files.empty()) text("(none yet)", files_r.x + 8, files_r.y + 32, DIM);
        // variables
        DrawRectangleRec(vars_r, PANEL); DrawRectangleLinesEx(vars_r, 1, DIM);
        text("variables  (click to print)", vars_r.x + 8, vars_r.y + 6, DIM, FS - 2);
        std::vector<std::pair<std::string, std::string>> vars; { std::lock_guard<std::mutex> g(S.m); vars = S.vars; }
        if (CheckCollisionPointRec(mouse, vars_r)) var_scroll = std::max(0, std::min(std::max(0, (int)vars.size() - 5), var_scroll - (int)GetMouseWheelMove() * 3));
        BeginScissorMode((int)vars_r.x, (int)vars_r.y + 26, (int)vars_r.width, (int)vars_r.height - 28);
        for (size_t k = var_scroll; k < vars.size(); k++) {
            Rectangle row = { vars_r.x + 2, vars_r.y + 30 + (k - var_scroll) * line_h, vars_r.width - 4, (float)line_h };
            if (row.y > vars_r.y + vars_r.height) break;
            if (vars[k].second.empty() && (k == 0 || !vars[k-1].second.empty())) { text("functions", row.x + 6, row.y + 2, DIM, FS - 3); continue; }
            bool hover = CheckCollisionPointRec(mouse, row);
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) submit("print " + vars[k].first);
            text(vars[k].first, row.x + 6, row.y + 2, hover ? ACCENT : vars[k].second.empty() ? DIM : INK);
            if (!vars[k].second.empty()) text(vars[k].second, row.x + width(vars[k].first) + 14, row.y + 2, DIM, FS - 2);
        }
        EndScissorMode();
        EndDrawing();
        if (const char* shot = std::getenv("MUSIL_LISTENER_SHOT")) if (GetTime() > 2.0) { TakeScreenshot(shot); break; }
    }
    S.quit = true; S.cv.notify_one(); worker.join();
    plot_cache_clear();
    if (IsAudioStreamValid(stream)) UnloadAudioStream(stream);
    if (plot_rt.id) UnloadRenderTexture(plot_rt);
    CloseAudioDevice(); CloseWindow();
    return 0;
}

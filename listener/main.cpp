// main.cpp — the Musil Listener: a window to run Musil, watch files, and read.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// Layout
//   left          the console: everything printed (wrapped), and a line to type Musil into
//   right, top    files: drop .mu files on the window to run them; they are watched and
//                 re-run when saved; select one and press Enter to run it again
//   right, middle variables: the globals, with a preview; select one and press Enter to print it
//   right, bottom help: search the documentation of every function
//
// Threads
//   The window (raylib) owns the main thread. The interpreter runs on a worker thread
//   and never touches the window: print goes through a queue of lines, the variable
//   list is a snapshot posted after every run, figures to show and PNGs to save go
//   through queues and are rendered by the main thread (save-png waits for it), and
//   Stop sets a flag that the interpreter checks in yield_fn. Sound belongs to the live
//   library, not here.
//
// Figures
//   (show fig) turns the window into the figure, as the CLI's window does: Esc returns to
//   the console. The figures of the session stay in a gallery: , and . page through them,
//   Ctrl-G reopens it. Keys in the figure: + - zoom, 0 reset, W A S D pan, arrows orbit a
//   surface, drag pans, s saves a PNG.
//
// Keyboard first: raylib never loses a key (it queues presses), while a quick trackpad click can
// fall between two frames, so nothing here depends on a mouse click.
//   Tab / Shift-Tab   cycle the focus: console -> files -> variables -> help
//   console           Enter runs (or continues while braces/parens are open; Shift-Enter runs anyway),
//                     Up/Down history, Right arrow completes a name
//   files             Up/Down select, Enter runs the file again
//   variables         Up/Down select, Enter prints the value
//   help              type to search, Enter prints the first match
//   anywhere          Esc or Ctrl-C stops a running program, Ctrl-L clears the console, Ctrl-R runs the
//                     last file again, Ctrl-+ / Ctrl-- / Ctrl-0 (or Ctrl-wheel) change the text size
//   (manual)          a builtin: opens the PDF manual

#include "musil.h"
#include "GLFW/glfw3.h"      // raylib's bundled GLFW: to cancel the window's close request while a figure is shown
#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <tuple>

using namespace musil;
namespace fs = std::filesystem;

// --- shared state between the interpreter thread and the window --------------------
struct shared {
    std::mutex m;
    std::condition_variable cv;
    std::deque<std::string> commands;                 // to the interpreter
    std::deque<std::string> lines;                    // from print
    std::vector<std::tuple<std::string, std::string, char>> vars;   // name, preview, kind (n s l f x)
    std::vector<figure> figures;                      // from show, to the gallery
    struct save_req { figure f; std::string path; int w, h; bool done = false, ok = false; std::string error; };
    std::deque<save_req*> saves;                      // from save-png; the main thread renders and signals
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
static vptr listener_show(vlist& a, Interp& i) {     // (show fig): the window becomes the figure
    figure f; try { f = parse_figure(a[0]); } catch (std::exception& e) { i.bad(e.what()); }
    std::lock_guard<std::mutex> g(S.m); S.figures.push_back(std::move(f)); return v_nil();
}
static vptr listener_save_png(vlist& a, Interp& i) { // (save-png fig path [w h]): rendered by the main thread; waits for it
    shared::save_req r; try { r.f = parse_figure(a[0]); } catch (std::exception& e) { i.bad(e.what()); }
    r.path = i.str(a[1]); r.w = a.size() > 2 ? (int)i.scalar(a[2]) : 900; r.h = a.size() > 3 ? (int)i.scalar(a[3]) : 560;
    { std::lock_guard<std::mutex> g(S.m); S.saves.push_back(&r); }
    { std::unique_lock<std::mutex> lk(S.m); S.cv.wait(lk, [&] { return r.done || S.quit; }); }
    if (!r.ok) i.bad(r.error.empty() ? "save-png: not saved" : r.error);
    return v_nil();
}
static std::string g_manual;                          // set by the window once it knows where the PDF is
static vptr listener_manual(vlist&, Interp& i) {     // (manual): open the PDF manual
    if (g_manual.empty()) i.bad("musil_manual.pdf not found (build it with ./build.sh --docs)");
    *i.out << "opening " << g_manual << "\n" << std::flush;
    OpenURL(g_manual.c_str());                        // `open` on macOS, xdg-open on Linux
    return v_nil();
}

// --- the interpreter thread ----------------------------------------------------------
static std::string preview(const vptr& v) {
    std::string s = str_of(v);
    if (s.size() > 40) s = s.substr(0, 37) + "...";
    for (auto& c : s) if (c == '\n') c = ' ';
    return s;
}
static void snapshot_vars(Interp& I) {   // values first (alphabetical), then user functions; builtins are left out
    std::vector<std::tuple<std::string, std::string, char>> vals, fns;
    for (auto& kv : I.global->vars) {
        const vptr& v = kv.second;
        if (v->t == Value::FN) { if (!v->op) fns.push_back({ kv.first, "", 'f' }); continue; }
        char kind = v->t == Value::NUM ? 'n' : v->t == Value::STR ? 's' : v->t == Value::LIST ? 'l' : v->t == Value::NIL ? 'x' : 'o';
        vals.push_back({ kv.first, preview(v), kind });
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
    I.def("manual", listener_manual, 0, 0);
    I.def("args", v_list({}));
    I.yield_fn = [&]() { if (S.stop) { S.stop = false; I.err("interrupted"); } };
    S.post_line("musil " MUSIL_VERSION " listener: type Musil below, drop .mu files on the window, Tab moves between panels");
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

// Keys are read from raylib's queue of presses (GetKeyPressed), which does not lose short taps.
struct keys_pressed {
    std::vector<int> keys;
    void collect() { keys.clear(); for (int k = GetKeyPressed(); k; k = GetKeyPressed()) keys.push_back(k); }
    bool has(int k) const { return std::find(keys.begin(), keys.end(), k) != keys.end(); }
};
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
// help.txt as a list of entries: names, then the text lines
struct help_entry { std::vector<std::string> names; std::vector<std::string> text; };
static std::vector<help_entry> load_help(const std::vector<std::string>& load_paths) {
    std::vector<help_entry> out;
    for (auto& d : load_paths) {
        std::ifstream f(fs::path(d) / "help.txt"); if (!f) continue;
        std::string line; help_entry cur;
        while (std::getline(f, line)) {
            if (line.rfind("# help", 0) == 0) continue;
            if (!line.empty() && line[0] == '#') {
                if (!cur.names.empty()) out.push_back(cur);
                cur = help_entry(); std::istringstream ss(line.substr(1)); std::string n; while (ss >> n) cur.names.push_back(n);
            } else if (!cur.names.empty()) cur.text.push_back(line.size() > 2 ? line.substr(2) : line);
        }
        if (!cur.names.empty()) out.push_back(cur);
        break;
    }
    return out;
}
// bracket depth of a piece of input, ignoring strings and comments: > 0 means "keep typing"
static int input_depth(const std::string& s) {
    int depth = 0; bool in_str = false, comment = false;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (comment) { if (c == '\n') comment = false; continue; }
        if (in_str) { if (c == '\\' && i + 1 < s.size()) { i++; continue; } if (c == '"') in_str = false; continue; }
        if (c == '"') in_str = true;
        else if (c == '#') comment = true;
        else if (c == '(' || c == '{') depth++;
        else if (c == ')' || c == '}') depth--;
    }
    return depth;
}
static std::string last_word(const std::string& s) {
    size_t e = s.size(), b = e;
    while (b > 0 && !std::isspace((unsigned char)s[b-1]) && s[b-1] != '(' && s[b-1] != ')' && s[b-1] != '{' && s[b-1] != '}') b--;
    return s.substr(b);
}

int main(int argc, char** argv) {
    std::vector<std::string> load_paths;
#ifdef __APPLE__
    load_paths.push_back(fs::path(GetApplicationDirectory()).parent_path().string() + "/Resources/lib");
#endif
    load_paths.push_back(std::string(GetApplicationDirectory()) + "lib");
    if (const char* home = std::getenv("HOME")) load_paths.push_back(std::string(home) + "/.musil");
#ifdef MUSIL_SOURCE_LIB
    load_paths.push_back(MUSIL_SOURCE_LIB);
#endif

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1280, 800, "Musil");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    std::string fp = asset_path("JetBrainsMono-Regular.ttf");
    Font font = fp.empty() ? GetFontDefault() : LoadFontEx(fp.c_str(), 40, nullptr, 0);
    if (!fp.empty()) { SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR); plot_get_font().font = font; plot_get_font().custom = true; }
    float FS = 16;                                                     // Ctrl-+ / Ctrl-- / Ctrl-0
    auto text = [&](const std::string& s, float x, float y, Color c, float size) { DrawTextEx(font, s.c_str(), { x, y }, size, 0.5f, c); };
    auto width = [&](const std::string& s, float size) { return MeasureTextEx(font, s.c_str(), size, 0.5f).x; };
    // palette: a warm light theme; the console stays white for reading, the drop area is a shade darker
    const Color BG = { 236, 236, 232, 255 }, PANEL = { 255, 255, 255, 255 }, DROP = { 244, 244, 240, 255 },
                INK = { 40, 40, 40, 255 }, DIM = { 130, 130, 130, 255 }, LINE = { 200, 200, 196, 255 },
                ACCENT = { 26, 60, 120, 255 }, ERR = { 190, 40, 40, 255 }, SEL = { 226, 236, 250, 255 }, FOCUS = { 60, 110, 190, 255 },
                C_NUM = { 46, 160, 67, 255 }, C_STR = { 220, 130, 30, 255 }, C_LIST = { 140, 80, 190, 255 }, C_FN = { 30, 110, 210, 255 }, C_NIL = { 160, 160, 160, 255 };
    auto kind_color = [&](char k) { return k == 'n' ? C_NUM : k == 's' ? C_STR : k == 'l' ? C_LIST : k == 'f' ? C_FN : C_NIL; };

    std::thread worker(interpreter_thread, load_paths);
    auto submit = [&](const std::string& c) { { std::lock_guard<std::mutex> g(S.m); S.commands.push_back(c); } S.cv.notify_one(); };
    std::vector<help_entry> help = load_help(load_paths);
    std::vector<std::string> help_names; for (auto& h : help) for (auto& n : h.names) help_names.push_back(n);

    std::vector<std::string> console; std::string input; std::vector<std::string> history; int hist_pos = -1;
    int scroll = 0, var_scroll = 0, help_scroll = 0; std::vector<watched> files; int sel_file = -1; std::string last_file;
    std::vector<std::string> rows; size_t rows_from = 0; int rows_w = 0; float rows_fs = 0;   // the console wrapped to the panel width
    std::vector<figure> gallery; int gi = -1; bool figure_mode = false; plot_view view;
    double last_watch = 0; int focus = 0;                                // 0 console, 1 files, 2 variables, 3 help
    int sel_var = 0;
    {   static std::string manual = asset_path("musil_manual.pdf");
#ifdef MUSIL_SOURCE_LIB
        if (manual.empty() && fs::exists(fs::path(MUSIL_SOURCE_LIB) / ".." / "docs" / "musil_manual.pdf")) manual = (fs::path(MUSIL_SOURCE_LIB) / ".." / "docs" / "musil_manual.pdf").string();
#endif
        if (!manual.empty()) g_manual = fs::absolute(manual).string(); }
    std::string query; std::vector<size_t> hits;
    auto search = [&]() {
        hits.clear(); std::string q = query; for (auto& c : q) c = (char)std::tolower((unsigned char)c);
        if (q.empty()) return;
        for (size_t k = 0; k < help.size(); k++) {
            bool exact = false, partial = false;
            for (auto& n : help[k].names) { std::string ln = n; for (auto& c : ln) c = (char)std::tolower((unsigned char)c); if (ln == q) exact = true; if (ln.find(q) != std::string::npos) partial = true; }
            if (!partial) for (auto& t : help[k].text) { std::string lt = t; for (auto& c : lt) c = (char)std::tolower((unsigned char)c); if (lt.find(q) != std::string::npos) { partial = true; break; } }
            if (exact) hits.insert(hits.begin(), k); else if (partial) hits.push_back(k);
        }
        help_scroll = 0;
    };
    auto add_file = [&](const std::string& p) {
        last_file = p;
        for (auto& w : files) if (w.path == p) return;
        std::error_code ec; files.push_back({ p, fs::last_write_time(p, ec) });
    };
    auto run_file = [&](const std::string& p) { last_file = p; submit(std::string("\x02") + p); };
    for (int k = 1; k < argc; k++) { add_file(argv[k]); run_file(argv[k]); }

    keys_pressed kp;
    while (!WindowShouldClose() && !S.quit) {
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
        kp.collect();
        auto pressed = [&](int k) { return kp.has(k) || IsKeyPressedRepeat(k); };
        // ---- keys that work anywhere ----
        if (IsFileDropped()) { FilePathList d = LoadDroppedFiles(); for (unsigned k = 0; k < d.count; k++) { add_file(d.paths[k]); run_file(d.paths[k]); } UnloadDroppedFiles(d); }
        // text size: Ctrl/Cmd with + or - (on any keyboard layout: the US '=' and ']' keys carry '+' on US and
        // Italian layouts, '-' and '/' carry '-'), the keypad keys, or Ctrl/Cmd with the mouse wheel; Ctrl-0 resets
        bool plus = pressed(KEY_EQUAL) || pressed(KEY_KP_ADD) || pressed(KEY_RIGHT_BRACKET);
        bool minus = pressed(KEY_MINUS) || pressed(KEY_KP_SUBTRACT) || pressed(KEY_SLASH);
        if (ctrl && (plus || GetMouseWheelMove() > 0)) FS = std::min(32.0f, FS + 1);
        if (ctrl && (minus || GetMouseWheelMove() < 0)) FS = std::max(10.0f, FS - 1);
        if (ctrl && (pressed(KEY_ZERO) || pressed(KEY_KP_0))) FS = 16;
        if (ctrl && pressed(KEY_L)) { console.clear(); scroll = 0; }
        if (ctrl && pressed(KEY_R) && !last_file.empty()) run_file(last_file);
        if (ctrl && pressed(KEY_C) && S.busy) S.stop = true;
        if (pressed(KEY_ESCAPE) && S.busy) S.stop = true;
        if (ctrl && pressed(KEY_G) && !gallery.empty()) { figure_mode = true; if (gi < 0) gi = (int)gallery.size() - 1; }
        // ---- figure mode: the window is the figure ----
        std::vector<std::tuple<std::string, std::string, char>> vars; { std::lock_guard<std::mutex> g(S.m); vars = S.vars; }
        int line_h = (int)FS + 4;
        if (figure_mode && gi >= 0) {
            // the figure takes the place of the console; the side panels stay
            int W = GetScreenWidth(), H = GetScreenHeight();
            int right_w = std::max(300, (int)(W * 0.26f)), fw = W - right_w - 24, fh = H - 24 - 22;
            Rectangle fr = { 12, 12, (float)fw, (float)fh };
            figure& f = gallery[gi];
            plot_collect_chars(view);
            bool leave = pressed(KEY_ESCAPE) || IsKeyPressed(KEY_ESCAPE) || IsKeyReleased(KEY_ESCAPE) || view.chars.count('q');
            if (WindowShouldClose()) { leave = true; glfwSetWindowShouldClose((GLFWwindow*)GetWindowHandle(), GLFW_FALSE); }   // the red cross closes the figure, not the Listener
            if (leave) { figure_mode = false; continue; }
            if (pressed(KEY_COMMA) || view.chars.count(',')) { if (gi > 0) { gi--; view = plot_view(); plot_cache_clear(); } }
            if (pressed(KEY_PERIOD) || view.chars.count('.')) { if (gi + 1 < (int)gallery.size()) { gi++; view = plot_view(); plot_cache_clear(); } }
            plot_interact(f, view, view, fr, GetMousePosition());
            if (view.chars.count('e')) { std::string p = plot_save_name(f); try { figure_to_png(f, p, (int)fr.width, (int)fr.height); console.push_back("saved " + p); } catch (std::exception& e) { console.push_back(std::string("error: ") + e.what()); } }
            BeginDrawing();
            ClearBackground(BG);
            render_figure_at(f, (int)fr.x, (int)fr.y, (int)fr.width, (int)fr.height, H, view);
            DrawRectangleLinesEx(fr, 1, FOCUS);
            text(std::string(plot_keys_hint) + "   , . previous/next (" + std::to_string(gi + 1) + "/" + std::to_string(gallery.size()) + ")", 12.0f, (float)H - 20, DIM, FS - 3);
            // side panels, read-only while the figure is up
            Rectangle files_r = { (float)(12 + fw + 12), 12, (float)right_w, (float)(H - 24) };
            DrawRectangleRec(files_r, DROP); DrawRectangleLinesEx(files_r, 1, LINE);
            text("figure  (Esc returns to the console)", files_r.x + 8, files_r.y + 6, DIM, FS - 2);
            for (size_t k = 0; k < gallery.size(); k++) {
                Rectangle row = { files_r.x + 2, files_r.y + 30 + k * line_h, files_r.width - 4, (float)line_h };
                if (row.y + line_h > files_r.y + files_r.height) break;
                if ((int)k == gi) DrawRectangleRec(row, SEL);
                text(std::to_string(k + 1) + "  " + (gallery[k].title.empty() ? "(untitled)" : gallery[k].title), row.x + 6, row.y + 2, (int)k == gi ? ACCENT : INK, FS);
            }
            EndDrawing();
            if (const char* shot = std::getenv("MUSIL_LISTENER_SHOT")) if (GetTime() > 2.0) { TakeScreenshot(shot); break; }
            continue;
        }
        // ---- focus and text input ----
        if (pressed(KEY_TAB)) focus = (focus + (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 3 : 1)) % 4;
        if (focus == 0 || focus == 3) {
            std::string& target = focus == 0 ? input : query;
            if (!ctrl) { int ch; while ((ch = GetCharPressed()) > 0) { if (ch >= 32 && ch < 127) target += (char)ch; if (focus == 3) search(); } }
            if (pressed(KEY_BACKSPACE) && !target.empty()) { target.pop_back(); if (focus == 3) search(); }
        }
        if (focus == 0) {
            if (pressed(KEY_RIGHT)) {                // complete the word being typed against variables and documented names
                std::string w = last_word(input);
                if (!w.empty()) {
                    std::vector<std::string> cands; for (auto& v : vars) if (std::get<0>(v).rfind(w, 0) == 0) cands.push_back(std::get<0>(v));
                    for (auto& n : help_names) if (n.rfind(w, 0) == 0) cands.push_back(n);
                    std::sort(cands.begin(), cands.end()); cands.erase(std::unique(cands.begin(), cands.end()), cands.end());
                    if (cands.size() == 1) input += cands[0].substr(w.size());
                    else if (cands.size() > 1) { std::string pre = cands[0]; for (auto& c : cands) pre = pre.substr(0, std::mismatch(pre.begin(), pre.end(), c.begin(), c.end()).first - pre.begin()); input += pre.substr(w.size()); std::string all; for (size_t k = 0; k < cands.size() && k < 12; k++) all += (k ? "  " : "") + cands[k]; console.push_back(all + (cands.size() > 12 ? "  ..." : "")); }
                }
            }
            if (pressed(KEY_ENTER) && !input.empty()) {
                if (input_depth(input) > 0 && !IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT)) input += '\n';   // unbalanced: keep typing
                else { history.push_back(input); hist_pos = -1; submit(input); input.clear(); }
            }
            if (pressed(KEY_UP) && !history.empty() && input.find('\n') == std::string::npos) { hist_pos = hist_pos < 0 ? (int)history.size() - 1 : std::max(0, hist_pos - 1); input = history[hist_pos]; }
            if (pressed(KEY_DOWN) && hist_pos >= 0) { hist_pos = std::min((int)history.size() - 1, hist_pos + 1); input = history[hist_pos]; }
        } else if (focus == 1 && !files.empty()) {
            if (sel_file < 0) sel_file = 0;
            if (pressed(KEY_UP)) sel_file = std::max(0, sel_file - 1);
            if (pressed(KEY_DOWN)) sel_file = std::min((int)files.size() - 1, sel_file + 1);
            if (pressed(KEY_ENTER)) run_file(files[sel_file].path);
        } else if (focus == 2 && !vars.empty()) {
            sel_var = std::max(0, std::min((int)vars.size() - 1, sel_var));
            if (pressed(KEY_UP)) sel_var = std::max(0, sel_var - 1);
            if (pressed(KEY_DOWN)) sel_var = std::min((int)vars.size() - 1, sel_var + 1);
            if (sel_var < var_scroll) var_scroll = sel_var;
            if (pressed(KEY_ENTER)) submit("print " + std::get<0>(vars[sel_var]));
        } else if (focus == 3 && pressed(KEY_ENTER) && !hits.empty()) submit("help " + help[hits[0]].names[0]);
        // file watcher, twice a second
        if (GetTime() - last_watch > 0.5) {
            last_watch = GetTime();
            for (auto& w : files) { std::error_code ec; auto t = fs::last_write_time(w.path, ec); if (!ec && t != w.mtime) { w.mtime = t; run_file(w.path); } }
        }
        // ---- collect what the interpreter produced ----
        std::vector<figure> new_figs; std::deque<shared::save_req*> new_saves;
        { std::lock_guard<std::mutex> g(S.m);
          while (!S.lines.empty()) { console.push_back(S.lines.front()); S.lines.pop_front(); scroll = 0; }
          new_figs.swap(S.figures); new_saves.swap(S.saves); }
        for (auto& f : new_figs) { gallery.push_back(std::move(f)); gi = (int)gallery.size() - 1; figure_mode = true; view = plot_view(); plot_cache_clear(); }
        for (auto* r : new_saves) {
            try { figure_to_png(r->f, r->path, r->w, r->h); r->ok = true; console.push_back("saved " + r->path); }
            catch (std::exception& e) { r->ok = false; r->error = e.what(); }
            { std::lock_guard<std::mutex> g(S.m); r->done = true; } S.cv.notify_all();
        }
        if (console.size() > 5000) { console.erase(console.begin(), console.begin() + 1000); rows.clear(); rows_from = 0; }

        // ---- layout ----
        int W = GetScreenWidth(), H = GetScreenHeight();
        int right_w = std::max(300, (int)(W * 0.26f)), left_w = W - right_w - 24, x0 = 12, y0 = 12;
        int input_lines = 1; for (char c : input) if (c == '\n') input_lines++;
        int in_h = line_h * std::min(input_lines, 8) + 8;
        Rectangle cons_r = { (float)x0, (float)y0, (float)left_w, (float)(H - y0 - 12 - in_h - 6) };
        Rectangle in_r = { (float)x0, cons_r.y + cons_r.height + 6, (float)left_w, (float)in_h };
        int rx = x0 + left_w + 12, avail = H - 2 * y0 - 24;
        Rectangle files_r = { (float)rx, (float)y0, (float)right_w, avail * 0.22f };
        Rectangle vars_r = { (float)rx, files_r.y + files_r.height + 12, (float)right_w, avail * 0.40f };
        Rectangle help_r = { (float)rx, vars_r.y + vars_r.height + 12, (float)right_w, (float)(H - y0 - (vars_r.y + vars_r.height + 12)) };
        Vector2 mouse = GetMousePosition();

        BeginDrawing();
        ClearBackground(BG);
        // console: lines wrapped to the panel width (rebuilt when the width or the text size changes)
        DrawRectangleRec(cons_r, PANEL); DrawRectangleLinesEx(cons_r, 1, focus == 0 ? FOCUS : LINE);
        int cols = std::max(8, (int)((cons_r.width - 20) / std::max(1.0f, width("M", FS))) - 1);
        if (rows_w != cols || rows_fs != FS) { rows.clear(); rows_from = 0; rows_w = cols; rows_fs = FS; }
        for (; rows_from < console.size(); rows_from++) {
            const std::string& l = console[rows_from];
            if (l.empty()) { rows.push_back(""); continue; }
            size_t p = 0;
            while (p < l.size()) {                                   // wrap at a space when there is one in reach
                size_t w = p ? cols - 2 : cols, n = std::min(w, l.size() - p);
                if (p + n < l.size()) { size_t sp = l.rfind(' ', p + n); if (sp != std::string::npos && sp > p + w / 2) n = sp - p; }
                rows.push_back((p ? "  " : "") + l.substr(p, n));
                p += n; while (p < l.size() && l[p] == ' ') p++;
            }
        }
        if (!ctrl && CheckCollisionPointRec(mouse, cons_r)) scroll = std::max(0, std::min((int)rows.size(), scroll - (int)GetMouseWheelMove() * 3));
        int visible = (int)((cons_r.height - 8) / line_h);
        int end = (int)rows.size() - scroll, start = std::max(0, end - visible);
        BeginScissorMode((int)cons_r.x, (int)cons_r.y, (int)cons_r.width, (int)cons_r.height);
        for (int k = start; k < end; k++) {
            const std::string& l = rows[k];
            Color c = l.rfind("error:", 0) == 0 ? ERR : l.rfind("> ", 0) == 0 ? ACCENT : INK;
            text(l, cons_r.x + 8, cons_r.y + 6 + (k - start) * line_h, c, FS);
        }
        EndScissorMode();
        // input line(s)
        DrawRectangleRec(in_r, PANEL); DrawRectangleLinesEx(in_r, 1, S.busy ? ACCENT : focus == 0 ? FOCUS : LINE);
        if (S.busy) {
            text("running...  (Esc or Ctrl-C stops)", in_r.x + 8, in_r.y + 4, DIM, FS);
        } else {
            std::string shown = input + ((focus == 0 && ((int)(GetTime() * 2) & 1)) ? "_" : "");
            size_t pos = 0; int ln = 0;
            while (true) { size_t nl = shown.find('\n', pos); std::string piece = shown.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
                text((ln == 0 ? "> " : "  ") + piece, in_r.x + 8, in_r.y + 4 + ln * line_h, INK, FS); ln++; if (nl == std::string::npos) break; pos = nl + 1; }
            if (input_depth(input) > 0) text("(Enter continues, Shift-Enter runs)", in_r.x + in_r.width - 6 - width("(Enter continues, Shift-Enter runs)", FS - 3), in_r.y + 4, DIM, FS - 3);
        }
        // files: the drop area
        DrawRectangleRec(files_r, DROP); DrawRectangleLinesEx(files_r, 1, focus == 1 ? FOCUS : LINE);
        text("files  (drop .mu here)", files_r.x + 8, files_r.y + 6, DIM, FS - 2);
        for (size_t k = 0; k < files.size(); k++) {
            Rectangle row = { files_r.x + 2, files_r.y + 30 + k * line_h, files_r.width - 4, (float)line_h };
            if (row.y + line_h > files_r.y + files_r.height) break;
            if ((int)k == sel_file) DrawRectangleRec(row, focus == 1 ? SEL : DROP);
            text(fs::path(files[k].path).filename().string(), row.x + 6, row.y + 2, (int)k == sel_file ? ACCENT : INK, FS);
        }
        if (files.empty()) text("<emtpy>", files_r.x + 8, files_r.y + 32, DIM, FS - 1);
        // variables, with a colour per kind
        DrawRectangleRec(vars_r, PANEL); DrawRectangleLinesEx(vars_r, 1, focus == 2 ? FOCUS : LINE);
        text("variables", vars_r.x + 8, vars_r.y + 6, DIM, FS - 2);
        {   float lx = vars_r.x + vars_r.width - 8; const char* names[] = { "fn", "list", "str", "num" }; const char kinds[] = { 'f', 'l', 's', 'n' };
            for (int k = 0; k < 4; k++) { float w = width(names[k], FS - 4); lx -= w; text(names[k], lx, vars_r.y + 7, DIM, FS - 4); lx -= 10; DrawCircle((int)lx + 3, (int)vars_r.y + 12, 3.5f, kind_color(kinds[k])); lx -= 12; } }
        int var_rows = std::max(1, (int)((vars_r.height - 34) / line_h));
        if (focus == 2 && sel_var >= var_scroll + var_rows) var_scroll = sel_var - var_rows + 1;
        if (!ctrl && CheckCollisionPointRec(mouse, vars_r)) var_scroll = std::max(0, std::min(std::max(0, (int)vars.size() - 5), var_scroll - (int)GetMouseWheelMove() * 3));
        BeginScissorMode((int)vars_r.x, (int)vars_r.y + 26, (int)vars_r.width, (int)vars_r.height - 28);
        for (size_t k = var_scroll; k < vars.size(); k++) {
            Rectangle row = { vars_r.x + 2, vars_r.y + 30 + (k - var_scroll) * line_h, vars_r.width - 4, (float)line_h };
            if (row.y > vars_r.y + vars_r.height) break;
            auto& [name, prev, kind] = vars[k];
            bool selected = focus == 2 && (int)k == sel_var;
            if (selected) DrawRectangleRec(row, SEL);
            DrawCircle((int)row.x + 10, (int)row.y + line_h / 2, 3.5f, kind_color(kind));
            text(name, row.x + 20, row.y + 2, selected ? ACCENT : kind == 'f' ? DIM : INK, FS);
            if (!prev.empty()) text(prev, row.x + 20 + width(name, FS) + 12, row.y + 2, DIM, FS - 2);
        }
        EndScissorMode();
        // help: a search over help.txt
        DrawRectangleRec(help_r, PANEL); DrawRectangleLinesEx(help_r, 1, focus == 3 ? FOCUS : LINE);
        text("help  (type to search)", help_r.x + 8, help_r.y + 6, DIM, FS - 2);
        Rectangle q_r = { help_r.x + 6, help_r.y + 26, help_r.width - 12, (float)line_h + 4 };
        DrawRectangleRec(q_r, DROP); DrawRectangleLinesEx(q_r, 1, LINE);
        text(query + ((focus == 3 && ((int)(GetTime() * 2) & 1)) ? "_" : ""), q_r.x + 6, q_r.y + 3, INK, FS);
        if (!ctrl && CheckCollisionPointRec(mouse, help_r)) help_scroll = std::max(0, help_scroll - (int)GetMouseWheelMove() * 3);
        BeginScissorMode((int)help_r.x, (int)q_r.y + line_h + 8, (int)help_r.width, (int)(help_r.y + help_r.height - q_r.y - line_h - 10));
        float hy = q_r.y + line_h + 12 - help_scroll * line_h;
        if (help.empty()) text("(help.txt not found next to the libraries)", help_r.x + 8, hy, DIM, FS - 1);
        else if (query.empty()) text(std::to_string(help.size()) + " entries: type a name or a word", help_r.x + 8, hy, DIM, FS - 1);
        else if (hits.empty()) text("no match", help_r.x + 8, hy, DIM, FS - 1);
        for (size_t k = 0; k < hits.size() && hy < help_r.y + help_r.height; k++) {
            const help_entry& e = help[hits[k]];
            for (size_t t = 0; t < e.text.size(); t++) {
                if (hy + line_h > q_r.y + line_h + 8) {
                    // wrap long lines to the panel width
                    std::string rest = e.text[t]; float maxw = help_r.width - 16; bool first = t == 0;
                    while (!rest.empty()) {
                        size_t n = rest.size();
                        while (n > 1 && width(rest.substr(0, n), FS - 2) > maxw) { size_t sp = rest.rfind(' ', n - 1); n = (sp == std::string::npos || sp == 0) ? n - 1 : sp; }
                        text(rest.substr(0, n), help_r.x + 8, hy, first ? ACCENT : INK, FS - 2); hy += line_h; first = false;
                        rest = n < rest.size() ? rest.substr(rest[n] == ' ' ? n + 1 : n) : "";
                    }
                } else hy += line_h;
            }
            hy += line_h / 2;
        }
        EndScissorMode();
        EndDrawing();
        if (const char* shot = std::getenv("MUSIL_LISTENER_SHOT")) if (GetTime() > 2.0) { TakeScreenshot(shot); break; }
    }
    S.quit = true; S.cv.notify_all(); worker.join();
    plot_cache_clear();
    CloseWindow();
    return 0;
}

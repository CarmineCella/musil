// main.cpp — the Musil IDE (FLTK): an editor, a console with an input line, the variables,
// a help search, and the same interpreter the command line uses. Plots and controls open
// in windows of their own (plot.h, controls.h); editors elsewhere can send code to the
// evaluation port. Adapted from the FLTK IDE of Musil 1.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// Threading
//   FLTK owns the main thread. The interpreter lives on a worker thread that takes commands
//   from a queue and, between them, runs the interpreter's idle work (live loops, OSC, the
//   port). Printed output, the variables snapshot and window requests reach the main thread
//   through Fl::awake; Stop sets a flag the interpreter checks in its yield hook, so a
//   runaway program stops without touching the session.
//
// Keys
//   Cmd-Enter        run the block around the cursor (or the selection); Cmd-Shift-Enter the file
//   Cmd-Alt-Enter    run the current line
//   Esc / Cmd-.      stop a running program
//   Cmd-N/O/S        new, open, save; Cmd-Z/Y undo, redo; Cmd-F find, Cmd-G find next; Cmd-/ comment
//   Cmd-+ / Cmd--    text size (everything);  Cmd-, settings (font, size, line numbers, current line, margin);
//   Cmd-K            clears the console; Cmd-L goes to a line; the toolbar has Run file / Stop / Clear
//   console line     Enter runs it, Up/Down recall history, Tab completes a name

#include "musil.h"
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Preferences.H>
#include <FL/Fl_Group.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <thread>
#include <array>

using namespace musil;
namespace fs = std::filesystem;

// --- shared state between the FLTK thread and the interpreter thread --------------
struct shared {
    std::mutex m; std::condition_variable cv;
    std::deque<std::pair<std::string, std::string>> commands;   // code, source name
    std::atomic<bool> stop{false}, busy{false}, quit{false};
} S;
struct var_entry { std::string name, preview; char kind; };

// widgets
static Fl_Double_Window* win;
static Fl_Text_Editor* editor; static Fl_Text_Buffer* text_buf; static Fl_Text_Buffer* style_buf;
static Fl_Text_Display* console; static Fl_Text_Buffer* console_buf;
static Fl_Input* input; static Fl_Hold_Browser* vars; static Fl_Input* help_query; static Fl_Text_Display* help_view; static Fl_Text_Buffer* help_buf;
static Fl_Box* status; static Fl_Group* toolbar; static Fl_Button* run_btn = nullptr; static Fl_Button* stop_btn = nullptr;
static std::string filename; static bool changed = false; static int font_size = 13;
static std::string buffer_origin;                     // where a buffer came from when it has no file name (an example): relative data paths resolve there
static std::vector<std::string> example_files;        // the examples found next to the executable, in the bundle, or in the source tree
static int editor_font = 0; static bool show_line_numbers = true, highlight_current_line = true; static int margin_col = 80; static int serve_port = 7770;   // editor_font: an index into font_names
static Fl_Preferences prefs(Fl_Preferences::USER, "carminecella", "musil");

static std::vector<std::string> history; static int hist_pos = -1;
static std::vector<std::string> load_paths;

// --- help.txt: entries for the search panel, names for the colouring and the completion ---
struct help_entry { std::vector<std::string> names; std::vector<std::string> text; };
static std::vector<help_entry> help_entries; static std::set<std::string> builtin_names;
static void load_help() {
    for (auto& d : load_paths) {
        std::ifstream f(fs::path(d) / "help.txt"); if (!f) continue;
        std::string line; help_entry cur;
        while (std::getline(f, line)) {
            if (line.rfind("# help", 0) == 0) continue;
            if (!line.empty() && line[0] == '#') { if (!cur.names.empty()) help_entries.push_back(cur); cur = help_entry(); std::istringstream ss(line.substr(1)); std::string n; while (ss >> n) { cur.names.push_back(n); builtin_names.insert(n); } }
            else if (!cur.names.empty()) cur.text.push_back(line.size() > 2 ? line.substr(2) : line);
        }
        if (!cur.names.empty()) help_entries.push_back(cur);
        break;
    }
}

// --- console output from the interpreter thread: collected under a mutex, moved to the console by a timer
// in one append per tick, so a program printing at full speed cannot flood the window ---
struct awake_text { std::string s; };
static std::mutex console_m; static std::string console_pending;
static const size_t CONSOLE_MAX = 400000;             // characters kept on screen
static void console_flush(void*) {
    std::string text; { std::lock_guard<std::mutex> g(console_m); text.swap(console_pending); }
    if (!text.empty()) {
        if (text.size() > CONSOLE_MAX) text = "[... output trimmed ...]\n" + text.substr(text.size() - CONSOLE_MAX);
        console_buf->append(text.c_str());
        if (console_buf->length() > (int)(CONSOLE_MAX * 2)) console_buf->remove(0, console_buf->length() - (int)CONSOLE_MAX);
        console->insert_position(console_buf->length()); console->show_insert_position();
    }
    Fl::repeat_timeout(0.03, console_flush, nullptr);
}
static void console_post(const std::string& s) {
    std::lock_guard<std::mutex> g(console_m);
    console_pending += s;
    if (console_pending.size() > CONSOLE_MAX * 2) console_pending.erase(0, console_pending.size() - CONSOLE_MAX);   // a runaway print: keep the tail
}
struct line_buf : std::streambuf {   // the interpreter's output stream: lines go to the console as they complete
    std::string cur;
    int overflow(int c) override { if (c == EOF) return 0; cur += (char)c; if (c == '\n') { console_post(cur); cur.clear(); } return c; }
    int sync() override { if (!cur.empty()) { console_post(cur); cur.clear(); } return 0; }
};
static void status_cb(void* p) {
    awake_text* t = (awake_text*)p; status->copy_label(t->s.c_str()); delete t;
    if (run_btn && stop_btn) { if (S.busy) { run_btn->deactivate(); stop_btn->activate(); } else { run_btn->activate(); stop_btn->deactivate(); } }
}
static void set_status(const std::string& s) { Fl::awake(status_cb, new awake_text{ s }); }

// --- the variables snapshot ---
static std::string preview(const vptr& v) {
    std::string s = str_of(v); if (s.size() > 60) s = s.substr(0, 57) + "...";
    for (auto& c : s) if (c == '\n') c = ' ';
    return s;
}
static void vars_update_cb(void* p) {
    auto* list = (std::vector<var_entry>*)p; int top = vars->topline();
    vars->clear();
    for (auto& e : *list) {
        const char* col = e.kind == 'n' ? "@C58" : e.kind == 's' ? "@C92" : e.kind == 'l' ? "@C136" : e.kind == 'f' ? "@C4" : "@C44";
        std::string row = std::string(col) + "@." + e.name + (e.preview.empty() ? "" : "\t@C44@." + e.preview);
        vars->add(row.c_str());
    }
    vars->topline(top); delete list;
}
static void snapshot_vars(Interp& I) {
    auto* list = new std::vector<var_entry>(); std::vector<var_entry> vals, fns;
    for (auto& kv : I.global->vars) {
        const vptr& v = kv.second;
        if (v->t == Value::FN) { if (!v->op) fns.push_back({ kv.first, "", 'f' }); continue; }
        char kind = v->t == Value::NUM ? 'n' : v->t == Value::STR ? 's' : v->t == Value::LIST ? 'l' : v->t == Value::NIL ? 'x' : 'o';
        vals.push_back({ kv.first, preview(v), kind });
    }
    auto by_name = [](const var_entry& a, const var_entry& b) { return a.name < b.name; };
    std::sort(vals.begin(), vals.end(), by_name); std::sort(fns.begin(), fns.end(), by_name);
    list->insert(list->end(), vals.begin(), vals.end()); list->insert(list->end(), fns.begin(), fns.end());
    Fl::awake(vars_update_cb, list);
}

// --- the interpreter thread ---
static void interpreter_thread() {
    line_buf buf; std::ostream out(&buf);
    Interp I;
    plot_needs_awake() = true;                            // plot and controls windows are made on the FLTK thread
    make_env(I);
    for (auto& p : load_paths) I.load_path.push_back(p);
    I.out = &out;
    I.def("args", v_list({}));
    I.yield_fn = [&I]() { if (S.stop) { S.stop = false; I.err("interrupted"); } };
    static std::atomic<bool> need_snapshot{false};
    server().on_receive = [](const std::string& code) { console_post("> [editor] " + code.substr(0, code.find('\n')) + (code.find('\n') != std::string::npos ? " ..." : "") + "\n"); need_snapshot = true; };
    if (serve_port > 0) {
        try { serve_start(I, serve_port); console_post("musil " MUSIL_VERSION " ide: evaluation port " + std::to_string(serve_port) + " open (other editors can send code to it)\n"); }
        catch (std::exception&) { console_post("musil " MUSIL_VERSION " ide (port " + std::to_string(serve_port) + " busy: no external editor connection)\n"); }
    } else console_post("musil " MUSIL_VERSION " ide (evaluation port off: Settings sets it)\n");
    snapshot_vars(I);
    if (!example_files.empty()) console_post(std::to_string(example_files.size()) + " examples in Help > Examples (each opens as a copy in an untitled buffer)\n");
    while (!S.quit) {
        std::pair<std::string, std::string> cmd;
        { std::unique_lock<std::mutex> lk(S.m);
          while (S.commands.empty() && !S.quit) {
              lk.unlock();
              try { I.idle(); } catch (std::exception& e) { console_post(std::string("error: ") + e.what() + "\n"); }
              buf.sync();
              if (need_snapshot) { need_snapshot = false; snapshot_vars(I); }
              lk.lock(); S.cv.wait_for(lk, std::chrono::milliseconds(10));
          }
          if (S.quit) break;
          cmd = S.commands.front(); S.commands.pop_front(); }
        S.busy = true; set_status("running...  (Esc stops)");
        try {
            vptr r = I.run(cmd.first, cmd.second);
            if (r && r->t != Value::NIL && cmd.second == "<console>") console_post(str_of(r) + "\n");
        } catch (Exit_signal&) { S.quit = true; }
        catch (std::exception& e) { console_post(std::string("error: ") + e.what() + "\n"); }
        buf.sync();
        I.call_stack.clear(); I.function_depth = 0; I.loop_depth = 0; I.stack_depth = 0;
        snapshot_vars(I);
        S.busy = false; set_status("");
    }
    serve_stop();
    live_shutdown();
    Fl::awake([](void*) { win->hide(); }, nullptr);
}
static void submit(const std::string& code, const std::string& name) { { std::lock_guard<std::mutex> g(S.m); S.commands.push_back({ code, name }); } S.cv.notify_one(); }

struct musil_editor;
static void editor_reset_current_line();
// --- syntax colouring (Fl_Text_Display style table) ---
// A..G plain, a..g the same on the current line's background (Fl_Text_Display styles carry a bgcolor)
static Fl_Text_Display::Style_Table_Entry styles[14];
static const Fl_Color style_colors[7] = { FL_BLACK, fl_rgb_color(120, 120, 120), fl_rgb_color(170, 40, 40), fl_rgb_color(26, 60, 140), fl_rgb_color(90, 90, 90), fl_rgb_color(30, 110, 60), fl_rgb_color(120, 60, 160) };
// The fixed-width fonts offered: FLTK's two, plus system ones by name (a missing one falls back to Courier)
static const char* const font_names[] = { "Courier", "Screen", "Menlo", "Monaco", "Consolas", "DejaVu Sans Mono", "JetBrains Mono", "Fira Code" };
static const int font_count = 8;
static Fl_Font font_id(int k) {                      // the Fl_Font of the k-th name; system fonts are registered once in the free slots
    if (k <= 0) return FL_COURIER; if (k == 1) return FL_SCREEN;
    static bool registered[font_count] = { false };
    Fl_Font id = (Fl_Font)(FL_FREE_FONT + k);
    if (!registered[k]) { Fl::set_font(id, font_names[k]); registered[k] = true; }
    return id;
}
static void build_styles() {                          // one font for everything: styles differ by colour only
    Fl_Font base = font_id(editor_font);
    Fl_Color cur = fl_rgb_color(255, 252, 220);
    for (int k = 0; k < 7; k++) {
        styles[k] = { style_colors[k], base, font_size, 0, 0 };
        styles[7 + k] = { style_colors[k], base, font_size, Fl_Text_Display::ATTR_BGCOLOR, cur };
    }
}
static const std::set<std::string> keywords = { "var", "set", "function", "return", "if", "while", "for", "break", "continue", "try", "catch", "do", "quote", "expr", "eval", "apply", "load", "help", "and", "or", "not" };
static bool ident_char(char c) { return std::isalnum((unsigned char)c) || c == '-' || c == '?' || c == '!' || c == '>' || c == '<' || c == '=' || c == '_' || c == '*' || c == '+' || c == '/'; }
static void style_parse(const char* text, char* style, int length) {
    int i = 0; bool in_comment = false, in_string = false;
    while (i < length) {
        char c = text[i];
        if (in_comment) { style[i] = 'B'; if (c == '\n') in_comment = false; i++; continue; }
        if (in_string) { style[i] = 'C'; if (c == '\\' && i + 1 < length) { style[i + 1] = 'C'; i += 2; continue; } if (c == '"') in_string = false; i++; continue; }
        if (c == '#') { in_comment = true; style[i] = 'B'; i++; continue; }
        if (c == '"') { in_string = true; style[i] = 'C'; i++; continue; }
        if (c == '(' || c == ')' || c == '{' || c == '}') { style[i] = 'E'; i++; continue; }
        if (c == '\'' && i + 1 < length && ident_char(text[i + 1])) {
            int j = i + 1; while (j < length && ident_char(text[j])) j++;
            for (int k = i; k < j; k++) style[k] = 'G';
            i = j; continue;
        }
        if (std::isdigit((unsigned char)c) || (c == '-' && i + 1 < length && std::isdigit((unsigned char)text[i + 1]) && (i == 0 || !ident_char(text[i - 1])))) {
            int j = i + 1; while (j < length && (std::isdigit((unsigned char)text[j]) || text[j] == '.' || text[j] == 'e' || text[j] == 'E')) j++;
            for (int k = i; k < j; k++) style[k] = 'G';
            i = j; continue;
        }
        if (ident_char(c)) {
            int j = i + 1; while (j < length && ident_char(text[j])) j++;
            std::string id(text + i, text + j); char m = keywords.count(id) ? 'D' : builtin_names.count(id) ? 'F' : 'A';
            for (int k = i; k < j; k++) style[k] = m;
            i = j; continue;
        }
        style[i] = 'A'; i++;
    }
}
static void restyle_all() { char* t = text_buf->text(); int n = text_buf->length(); std::string style(n, 'A'); if (n) style_parse(t, &style[0], n); free(t); style_buf->text(style.c_str()); editor_reset_current_line(); }
static void style_update(int, int inserted, int deleted, int, const char*, void*) { if (inserted || deleted) restyle_all(); }   // whole-buffer restyle: strings and comments can span edits, and files are small
static void apply_settings() {
    build_styles();
    Fl_Font f = font_id(editor_font);
    editor->textfont(f); editor->textsize(font_size); editor->linenumber_width(show_line_numbers ? 40 : 0); editor->linenumber_size(font_size - 2);
    console->textfont(f); console->textsize(font_size); input->textfont(f); input->textsize(font_size);
    help_view->textfont(f); help_view->textsize(font_size - 1); help_query->textfont(f); help_query->textsize(font_size); vars->textfont(f); vars->textsize(font_size - 1);
    restyle_all(); editor->redraw(); console->redraw(); vars->redraw(); help_view->redraw(); input->redraw();
    prefs.set("font_size", font_size); prefs.set("editor_font", editor_font); prefs.set("line_numbers", show_line_numbers ? 1 : 0); prefs.set("current_line", highlight_current_line ? 1 : 0); prefs.set("margin_col", margin_col); prefs.flush();
}
static void apply_font_size() { apply_settings(); }

// --- the editor: blocks, running, files ---
static bool blank_line(int pos) { char* l = text_buf->line_text(pos); bool b = std::string(l).find_first_not_of(" \t\r") == std::string::npos; free(l); return b; }
static void flash_range(int a, int b) { text_buf->highlight(a, b); Fl::add_timeout(0.3, [](void*) { text_buf->unhighlight(); }, nullptr); }
static std::string block_around(int pos) {           // the lines between blank lines around pos
    int start = text_buf->line_start(pos), end = text_buf->line_end(pos);
    while (start > 0) { int prev = text_buf->line_start(start - 1); if (blank_line(prev)) break; start = prev; }
    while (end < text_buf->length()) { int next_start = end + 1; if (next_start >= text_buf->length() || blank_line(next_start)) break; end = text_buf->line_end(next_start); }
    char* t = text_buf->text_range(start, end); std::string s = t; free(t);
    flash_range(start, end);
    return s;
}
static std::string source_name() { return !filename.empty() ? filename : !buffer_origin.empty() ? buffer_origin : "<editor>"; }
static void run_block(Fl_Widget*, void*) {
    int a, b; std::string code;
    if (text_buf->selected() && text_buf->selection_position(&a, &b)) { char* t = text_buf->text_range(a, b); code = t; free(t); flash_range(a, b); }
    else code = block_around(editor->insert_position());
    if (code.find_first_not_of(" \t\r\n") != std::string::npos) submit(code, source_name());
}
static void run_line(Fl_Widget*, void*) { char* l = text_buf->line_text(editor->insert_position()); std::string s = l; free(l); if (!s.empty()) submit(s, source_name()); }
static void run_file(Fl_Widget*, void*) { char* t = text_buf->text(); std::string s = t; free(t); flash_range(0, text_buf->length()); submit(s, source_name()); }
static void stop_cb(Fl_Widget*, void*) { if (S.busy) S.stop = true; }
static void update_title() { std::string t = std::string("Musil  ") + (filename.empty() ? "untitled" : fs::path(filename).filename().string()) + (changed ? " *" : ""); win->copy_label(t.c_str()); }
static void text_changed(int, int ins, int del, int, const char*, void*) { if ((ins || del) && !changed) { changed = true; update_title(); } }
void save_cb(Fl_Widget*, void*);
static bool ok_to_discard() { if (!changed) return true; int r = fl_choice("The file has unsaved changes.", "Cancel", "Save", "Discard"); if (r == 0) return false; if (r == 1) { save_cb(nullptr, nullptr); return !changed; } return true; }
static std::string last_dir() { char* d = nullptr; prefs.get("last_dir", d, ""); std::string r = d ? d : ""; free(d); return r; }
static void remember_dir(const std::string& path) { prefs.set("last_dir", fs::absolute(path).parent_path().string().c_str()); prefs.flush(); }
static void load_file(const std::string& path) { if (text_buf->loadfile(path.c_str())) { fl_alert("cannot open %s", path.c_str()); return; } filename = path; buffer_origin.clear(); changed = false; restyle_all(); update_title(); remember_dir(path); console_buf->append(("opened " + path + "\n").c_str()); }
// An example is copied into an untitled buffer: the original stays untouched; its data is still found (buffer_origin)
static void open_example_cb(Fl_Widget*, void* d) {
    const std::string& path = example_files[(size_t)(intptr_t)d];
    if (!ok_to_discard()) return;
    if (text_buf->loadfile(path.c_str())) { fl_alert("cannot open %s", path.c_str()); return; }
    filename.clear(); buffer_origin = path; changed = true; restyle_all(); update_title();
    console_buf->append(("example " + fs::path(path).filename().string() + " copied into an untitled buffer (save it under a name of yours)\n").c_str());
}
static void find_examples() {
    std::vector<fs::path> dirs;
    for (auto& d : load_paths) dirs.push_back(fs::path(d) / ".." / "examples");     // dist/lib -> dist/examples; Resources/lib -> Resources/examples; src -> examples
    for (auto& d : dirs) {
        std::error_code ec; if (!fs::is_directory(d, ec)) continue;
        for (auto& e : fs::directory_iterator(d, ec)) if (e.path().extension() == ".mu") example_files.push_back(fs::weakly_canonical(e.path(), ec).string());
        if (!example_files.empty()) break;
    }
    std::sort(example_files.begin(), example_files.end());
}
void save_cb(Fl_Widget*, void*) {
    if (filename.empty()) { Fl_Native_File_Chooser ch; ch.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE); ch.filter("Musil\t*.mu"); ch.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM); ch.directory(last_dir().c_str()); if (ch.show() != 0) return; filename = ch.filename(); if (fs::path(filename).extension().empty()) filename += ".mu"; remember_dir(filename); }
    if (text_buf->savefile(filename.c_str())) { fl_alert("cannot save %s", filename.c_str()); return; }
    buffer_origin.clear(); changed = false; update_title();
}
static void save_as_cb(Fl_Widget* w, void* d) { std::string old = filename; filename.clear(); save_cb(w, d); if (filename.empty()) filename = old; update_title(); }
static void open_cb(Fl_Widget*, void*) { if (!ok_to_discard()) return; Fl_Native_File_Chooser ch; ch.type(Fl_Native_File_Chooser::BROWSE_FILE); ch.filter("Musil\t*.mu"); ch.directory(last_dir().c_str()); if (ch.show() == 0) load_file(ch.filename()); }
static void new_cb(Fl_Widget*, void*) { if (!ok_to_discard()) return; text_buf->text(""); filename.clear(); buffer_origin.clear(); changed = false; restyle_all(); update_title(); }
static void quit_cb(Fl_Widget*, void*) { if (!ok_to_discard()) return; S.quit = true; S.cv.notify_all(); win->hide(); }
static void new_cb_origin_reset() { buffer_origin.clear(); }
static void clear_console_cb(Fl_Widget*, void*) { console_buf->text(""); }
static void zoom_cb(Fl_Widget*, void* d) { font_size = std::max(9, std::min(28, font_size + (d ? 1 : -1))); apply_font_size(); }
static void comment_cb(Fl_Widget*, void*) {
    int a, b; if (!(text_buf->selected() && text_buf->selection_position(&a, &b))) { a = b = editor->insert_position(); }
    int start = text_buf->line_start(a), end = text_buf->line_end(b);
    char* t = text_buf->text_range(start, end); std::string s = t; free(t);
    std::vector<std::string> lines; size_t p = 0; while (true) { size_t nl = s.find('\n', p); lines.push_back(s.substr(p, nl == std::string::npos ? std::string::npos : nl - p)); if (nl == std::string::npos) break; p = nl + 1; }
    bool all = true; for (auto& l : lines) { size_t i = l.find_first_not_of(" \t"); if (i != std::string::npos && l[i] != '#') all = false; }
    std::string out;
    for (size_t k = 0; k < lines.size(); k++) { std::string l = lines[k]; size_t i = l.find_first_not_of(" \t"); if (all) { if (i != std::string::npos && l[i] == '#') l.erase(i, i + 1 < l.size() && l[i + 1] == ' ' ? 2 : 1); } else l = "# " + l; out += l + (k + 1 < lines.size() ? "\n" : ""); }
    text_buf->replace(start, end, out.c_str());
}
static void manual_cb(Fl_Widget*, void*) {
    for (auto& d : load_paths) for (auto& c : { (fs::path(d) / "musil_manual.pdf").string(), (fs::path(d) / ".." / "docs" / "musil_manual.pdf").string(), (fs::path(d) / ".." / "musil_manual.pdf").string() })
        if (fs::exists(c)) {
#ifdef __APPLE__
            std::string cmd = "open '" + fs::absolute(c).string() + "' &";
#else
            std::string cmd = "xdg-open '" + fs::absolute(c).string() + "' &";
#endif
            (void)!std::system(cmd.c_str()); return;
        }
    fl_alert("musil_manual.pdf not found (build it with ./build.sh --docs)");
}
static void about_cb(Fl_Widget*, void*) { fl_message("Musil %s\nA scripting language for sound and music computing\n(c) 2026 Carmine-Emanuele Cella", MUSIL_VERSION); }

// find
static std::string last_find;
static void find_next() { if (last_find.empty()) return; int pos = editor->insert_position(), found; if (text_buf->search_forward(pos, last_find.c_str(), &found) || text_buf->search_forward(0, last_find.c_str(), &found)) { text_buf->select(found, found + (int)last_find.size()); editor->insert_position(found + (int)last_find.size()); editor->show_insert_position(); } else fl_beep(); }
static void find_cb(Fl_Widget*, void*) { const char* s = fl_input("Find:", last_find.c_str()); if (s && *s) { last_find = s; find_next(); } }
static void find_next_cb(Fl_Widget*, void*) { if (last_find.empty()) find_cb(nullptr, nullptr); else find_next(); }
static void goto_line_cb(Fl_Widget*, void*) {
    const char* s = fl_input("Go to line:", ""); if (!s || !*s) return;
    int want = std::atoi(s); if (want < 1) return;
    int pos = 0; for (int k = 1; k < want && pos < text_buf->length(); k++) pos = text_buf->line_end(pos) + 1;
    pos = std::min(pos, text_buf->length()); editor->insert_position(pos); editor->show_insert_position(); editor->take_focus(); restyle_all();
}

// --- the console input line ---
static std::string last_word(const std::string& s) { size_t e = s.size(), b = e; while (b > 0 && !std::isspace((unsigned char)s[b - 1]) && s[b - 1] != '(' && s[b - 1] != ')' && s[b - 1] != '{' && s[b - 1] != '}') b--; return s.substr(b); }
static std::string var_name_of_row(int i) { const char* t = vars->text(i); if (!t) return ""; std::string row = t; size_t at = row.find("@."); std::string name = at == std::string::npos ? row : row.substr(at + 2); return name.substr(0, name.find('\t')); }
struct console_input : Fl_Input {
    console_input(int x, int y, int w, int h) : Fl_Input(x, y, w, h) {}
    int handle(int e) override {
        if (e == FL_KEYDOWN) {
            int k = Fl::event_key();
            if (k == FL_Enter || k == FL_KP_Enter) { std::string s = value(); if (!s.empty()) { console_buf->append(("> " + s + "\n").c_str()); history.push_back(s); hist_pos = -1; submit(s, "<console>"); value(""); } return 1; }
            if (k == FL_Up && !history.empty()) { hist_pos = hist_pos < 0 ? (int)history.size() - 1 : std::max(0, hist_pos - 1); value(history[hist_pos].c_str()); insert_position((int)strlen(value())); return 1; }
            if (k == FL_Down && hist_pos >= 0) { hist_pos = std::min((int)history.size() - 1, hist_pos + 1); value(history[hist_pos].c_str()); insert_position((int)strlen(value())); return 1; }
            if (k == FL_Tab) {
                std::string s = value(), w = last_word(s); if (w.empty()) return 1;
                std::vector<std::string> cands; for (auto& n : builtin_names) if (n.rfind(w, 0) == 0) cands.push_back(n);
                for (int i = 1; i <= vars->size(); i++) { std::string name = var_name_of_row(i); if (name.rfind(w, 0) == 0) cands.push_back(name); }
                std::sort(cands.begin(), cands.end()); cands.erase(std::unique(cands.begin(), cands.end()), cands.end());
                if (cands.size() == 1) value((s + cands[0].substr(w.size())).c_str());
                else if (cands.size() > 1) { std::string pre = cands[0]; for (auto& c : cands) pre = pre.substr(0, std::mismatch(pre.begin(), pre.end(), c.begin(), c.end()).first - pre.begin()); value((s + pre.substr(w.size())).c_str()); std::string all; for (size_t k = 0; k < cands.size() && k < 12; k++) all += (k ? "  " : "") + cands[k]; console_buf->append((all + (cands.size() > 12 ? "  ..." : "") + "\n").c_str()); }
                insert_position((int)strlen(value())); return 1;
            }
            if (k == FL_Escape) { if (S.busy) S.stop = true; return 1; }
        }
        return Fl_Input::handle(e);
    }
};

// --- help search ---
static void help_search_cb(Fl_Widget*, void*) {
    std::string q = help_query->value(); for (auto& c : q) c = (char)std::tolower((unsigned char)c);
    if (q.empty()) { help_buf->text((std::to_string(help_entries.size()) + " entries: type a name or a word\n").c_str()); return; }
    std::vector<const help_entry*> exact, partial;
    for (auto& e : help_entries) {
        bool ex = false, pa = false;
        for (auto& nm : e.names) { std::string l = nm; for (auto& c : l) c = (char)std::tolower((unsigned char)c); if (l == q) ex = true; if (l.find(q) != std::string::npos) pa = true; }
        if (!pa) for (auto& t : e.text) { std::string l = t; for (auto& c : l) c = (char)std::tolower((unsigned char)c); if (l.find(q) != std::string::npos) { pa = true; break; } }
        if (ex) exact.push_back(&e); else if (pa) partial.push_back(&e);
    }
    std::string out; int n = 0;
    for (auto* e : exact) { for (auto& t : e->text) out += t + "\n"; out += "\n"; n++; }
    for (auto* e : partial) { if (n++ > 40) { out += "...\n"; break; } for (auto& t : e->text) out += t + "\n"; out += "\n"; }
    if (out.empty()) out = "no match\n";
    help_buf->text(out.c_str());
}
static void vars_cb(Fl_Widget*, void*) {   // double-click a variable: print it
    int i = vars->value(); if (i <= 0 || !Fl::event_clicks()) return;
    std::string name = var_name_of_row(i); if (name.empty()) return;
    console_buf->append(("> print " + name + "\n").c_str()); submit("print " + name, "<console>");
}

// --- the editor widget: keys and file drops ---
struct musil_editor : Fl_Text_Editor {
    int cur_start = -1, cur_end = -1;
    musil_editor(int x, int y, int w, int h) : Fl_Text_Editor(x, y, w, h) {}
    void draw() override {
        Fl_Text_Editor::draw();
        if (highlight_current_line && buffer()) {                // the current line's background to the right edge (the styles cover the text only)
            int pos = insert_position(), le = buffer()->line_end(pos), lx, ly;
            if (position_to_xy(le, &lx, &ly)) {
                fl_font(textfont(), textsize()); int lh = fl_height();
                int right = x() + w() - (scrollbar_width() + 4);
                if (lx < right) { fl_color(fl_rgb_color(255, 252, 220)); fl_rectf(lx, ly, right - lx, lh); }
            }
        }
        if (margin_col > 0) {                                  // the guide at the margin column
            fl_font(textfont(), textsize()); int lx = x() + (show_line_numbers ? linenumber_width() : 0) + 3 + margin_col * (int)fl_width("M");
            if (lx > x() && lx < x() + w()) { fl_color(fl_rgb_color(225, 225, 225)); fl_line(lx, y(), lx, y() + h()); }
        }
    }
    void update_current_line() {                               // restyle the old and the new current line
        if (!buffer() || !style_buf) return;
        int pos = insert_position(), a = buffer()->line_start(pos), b = buffer()->line_end(pos);
        if (a == cur_start && b == cur_end) return;
        auto set_case = [&](int from, int to, bool lower) {
            if (from < 0 || to > style_buf->length() || from >= to) return;
            char* st = style_buf->text_range(from, to); std::string t = st; free(st);
            for (auto& c : t) { if (lower && c >= 'A' && c <= 'G') c = (char)(c + 32); if (!lower && c >= 'a' && c <= 'g') c = (char)(c - 32); }
            style_buf->replace(from, to, t.c_str());
        };
        if (cur_start >= 0) set_case(cur_start, std::min(cur_end, style_buf->length()), false);
        if (highlight_current_line) set_case(a, b, true);
        cur_start = a; cur_end = b; redisplay_range(0, buffer()->length());
    }
    int handle(int e) override {
        int r = 0;
        if (e == FL_KEYDOWN && (Fl::event_key() == FL_Enter || Fl::event_key() == FL_KP_Enter) && (Fl::event_state() & FL_COMMAND)) {
            if (Fl::event_state() & FL_SHIFT) run_file(nullptr, nullptr); else if (Fl::event_state() & FL_ALT) run_line(nullptr, nullptr); else run_block(nullptr, nullptr);
            return 1;
        }
        if (e == FL_KEYDOWN && Fl::event_key() == FL_Escape) { if (S.busy) S.stop = true; return 1; }
        if (e == FL_DND_ENTER || e == FL_DND_DRAG || e == FL_DND_RELEASE) return 1;
        if (e == FL_PASTE && Fl::event_text() && std::string(Fl::event_text()).rfind("file://", 0) == 0) {
            std::string t = Fl::event_text(); size_t eol = t.find_first_of("\r\n"); std::string path = t.substr(7, eol == std::string::npos ? std::string::npos : eol - 7);
            std::string decoded; for (size_t k = 0; k < path.size(); k++) { if (path[k] == '%' && k + 2 < path.size()) { decoded += (char)std::stoi(path.substr(k + 1, 2), nullptr, 16); k += 2; } else decoded += path[k]; }
            if (ok_to_discard()) load_file(decoded);
            return 1;
        }
        r = Fl_Text_Editor::handle(e);
        if (e == FL_KEYDOWN || e == FL_KEYUP || e == FL_PUSH || e == FL_RELEASE || e == FL_DRAG) update_current_line();
        return r;
    }
};

static void editor_reset_current_line() { if (editor) { ((musil_editor*)editor)->cur_start = -1; ((musil_editor*)editor)->update_current_line(); } }

// --- settings dialog ---
struct settings_dialog { Fl_Double_Window* win; Fl_Choice* font; Fl_Value_Input* size; Fl_Check_Button* numbers; Fl_Check_Button* curline; Fl_Value_Input* margin; Fl_Value_Input* port; };
static void settings_ok_cb(Fl_Widget*, void* d) {
    settings_dialog* dlg = (settings_dialog*)d;
    editor_font = std::max(0, std::min(font_count - 1, dlg->font->value())); font_size = std::max(8, std::min(32, (int)dlg->size->value()));
    show_line_numbers = dlg->numbers->value() != 0; highlight_current_line = dlg->curline->value() != 0; margin_col = std::max(0, (int)dlg->margin->value());
    int new_port = std::max(0, std::min(65535, (int)dlg->port->value()));
    if (new_port != serve_port) { serve_port = new_port; prefs.set("serve_port", serve_port); prefs.flush(); submit(new_port > 0 ? "(serve " + std::to_string(new_port) + ")" : "(serve-stop)", "<settings>"); console_buf->append(new_port > 0 ? ("evaluation port now " + std::to_string(new_port) + "\n").c_str() : "evaluation port closed\n"); }
    apply_settings(); dlg->win->hide();
}
static void settings_cb(Fl_Widget*, void*) {
    static settings_dialog* dlg = nullptr;
    if (!dlg) {
        dlg = new settings_dialog();
        dlg->win = new Fl_Double_Window(380, 265, "Settings");
        dlg->font = new Fl_Choice(130, 15, 220, 26, "Font:"); for (int k = 0; k < font_count; k++) dlg->font->add(font_names[k]);
        dlg->size = new Fl_Value_Input(130, 50, 80, 26, "Text size:"); dlg->size->range(8, 32); dlg->size->step(1);
        dlg->numbers = new Fl_Check_Button(130, 85, 220, 26, "Line numbers");
        dlg->curline = new Fl_Check_Button(130, 115, 220, 26, "Highlight the current line");
        dlg->margin = new Fl_Value_Input(130, 150, 80, 26, "Margin at column:"); dlg->margin->range(0, 200); dlg->margin->step(1); dlg->margin->tooltip("0 for no guide");
        dlg->port = new Fl_Value_Input(130, 185, 80, 26, "Evaluation port:"); dlg->port->range(0, 65535); dlg->port->step(1); dlg->port->tooltip("the port editors send code to (VS Code, musil --send); 0 turns it off");
        Fl_Button* ok = new Fl_Button(280, 225, 80, 28, "OK"); ok->callback(settings_ok_cb, dlg);
        Fl_Button* cancel = new Fl_Button(190, 225, 80, 28, "Cancel"); cancel->callback([](Fl_Widget* w, void*) { w->window()->hide(); });
        dlg->win->end(); dlg->win->set_modal();
    }
    dlg->font->value(editor_font); dlg->size->value(font_size);
    dlg->numbers->value(show_line_numbers); dlg->curline->value(highlight_current_line); dlg->margin->value(margin_col); dlg->port->value(serve_port);
    dlg->win->show();
}
static const int BANNER_H = 20;
static Fl_Box* banner(int x, int y, int w, const char* text) {
    Fl_Box* b = new Fl_Box(x, y, w, BANNER_H, text); b->box(FL_FLAT_BOX); b->color(fl_rgb_color(228, 228, 226)); b->labelsize(11); b->labelcolor(fl_rgb_color(100, 100, 100)); b->labelfont(FL_HELVETICA_BOLD); b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE); return b;
}
static Fl_Button* tool_button(int& x, int y, int w, const char* label, const char* tip, Fl_Callback* cb, void* d = nullptr, Fl_Color col = FL_BLACK) {
    Fl_Button* b = new Fl_Button(x, y, w, 24, label); b->box(FL_FLAT_BOX); b->down_box(FL_FLAT_BOX); b->labelsize(14); b->labelcolor(col); b->color(fl_rgb_color(236, 236, 234)); b->selection_color(fl_rgb_color(205, 215, 235)); b->tooltip(tip); b->callback(cb, d); b->clear_visible_focus(); x += w + 2; return b;
}

// The main window scales the layout uniformly: whatever proportions the user dragged the tile to are kept
struct ide_window : Fl_Double_Window {
    Fl_Tile* tile = nullptr;
    ide_window(int w, int h, const char* t) : Fl_Double_Window(w, h, t) {}
    void resize(int X, int Y, int W, int H) override {
        int ow = w(), oh = h();
        std::vector<std::array<double, 4>> fr;                // each tile child's box as fractions of the tile
        if (tile && ow > 0 && oh > 0) for (int k = 0; k < tile->children(); k++) { Fl_Widget* c = tile->child(k); fr.push_back({ (double)(c->x() - tile->x()) / tile->w(), (double)(c->y() - tile->y()) / tile->h(), (double)c->w() / tile->w(), (double)c->h() / tile->h() }); }
        Fl_Double_Window::resize(X, Y, W, H);
        if (tile && !fr.empty()) {
            int tx = tile->x(), ty = tile->y(), tw = tile->w(), th = tile->h();
            for (int k = 0; k < tile->children(); k++) { Fl_Widget* c = tile->child(k); auto& f = fr[k]; c->resize(tx + (int)std::lround(f[0] * tw), ty + (int)std::lround(f[1] * th), (int)std::lround(f[2] * tw), (int)std::lround(f[3] * th)); }
            tile->init_sizes(); tile->redraw();
        }
    }
};

int main(int argc, char** argv) {
    Fl::lock();                                           // enables Fl::awake from the interpreter thread
    Fl::keyboard_screen_scaling(0);                       // Ctrl-+ / Ctrl-- are ours (text size), not FLTK's window scaling
    Fl::scheme("oxy");
    Fl::background(240, 240, 238); Fl::background2(255, 255, 255); Fl::foreground(40, 40, 40);
    prefs.get("font_size", font_size, 13); prefs.get("editor_font", editor_font, 0); if (editor_font < 0 || editor_font >= font_count) editor_font = 0; int ln = 1, cl = 1; prefs.get("line_numbers", ln, 1); prefs.get("current_line", cl, 1); prefs.get("margin_col", margin_col, 80); prefs.get("serve_port", serve_port, 7770);
    show_line_numbers = ln != 0; highlight_current_line = cl != 0; build_styles();
    // libraries: next to the executable, in the bundle, ~/.musil, the source tree
    fs::path exe = fs::absolute(argv[0]).parent_path();
    load_paths.push_back((exe / "lib").string()); load_paths.push_back((exe / ".." / "Resources" / "lib").string());
    if (const char* home = std::getenv("HOME")) load_paths.push_back(std::string(home) + "/.musil");
#ifdef MUSIL_SOURCE_LIB
    load_paths.push_back(MUSIL_SOURCE_LIB);
#endif
    load_help();

    const int W = 1280, H = 820, STATUS_H = 22, INPUT_H = 30;
    win = new ide_window(W, H, "Musil");
    win->position((Fl::w() - W) / 2, (Fl::h() - H) / 2);
    Fl_Sys_Menu_Bar* menu = new Fl_Sys_Menu_Bar(0, 0, W, 26);
    menu->add("&File/&New", FL_COMMAND + 'n', new_cb); menu->add("&File/&Open...", FL_COMMAND + 'o', open_cb);
    menu->add("&File/&Save", FL_COMMAND + 's', save_cb); menu->add("&File/Save &As...", FL_COMMAND + FL_SHIFT + 's', save_as_cb, nullptr, FL_MENU_DIVIDER);
    menu->add("&File/&Quit", FL_COMMAND + 'q', quit_cb);
    menu->add("&Edit/&Undo", FL_COMMAND + 'z', [](Fl_Widget*, void*) { Fl_Text_Editor::kf_undo(0, editor); });
    menu->add("&Edit/&Redo", FL_COMMAND + 'y', [](Fl_Widget*, void*) { Fl_Text_Editor::kf_redo(0, editor); }, nullptr, FL_MENU_DIVIDER);
    menu->add("&Edit/Cu&t", FL_COMMAND + 'x', [](Fl_Widget*, void*) { Fl_Text_Editor::kf_cut(0, editor); });
    menu->add("&Edit/&Copy", FL_COMMAND + 'c', [](Fl_Widget*, void*) { Fl_Text_Editor::kf_copy(0, editor); });
    menu->add("&Edit/&Paste", FL_COMMAND + 'v', [](Fl_Widget*, void*) { Fl_Text_Editor::kf_paste(0, editor); }, nullptr, FL_MENU_DIVIDER);
    menu->add("&Edit/&Find...", FL_COMMAND + 'f', find_cb); menu->add("&Edit/Find &Next", FL_COMMAND + 'g', find_next_cb);
    menu->add("&Edit/Go to &Line...", FL_COMMAND + 'l', goto_line_cb);
    menu->add("&Edit/Comment\\/Uncomment", FL_COMMAND + '/', comment_cb, nullptr, FL_MENU_DIVIDER);
    menu->add("&Edit/Bigger Text", FL_COMMAND + '+', zoom_cb, (void*)1); menu->add("&Edit/Smaller Text", FL_COMMAND + '-', zoom_cb, (void*)0, FL_MENU_DIVIDER);
    menu->add("&Edit/&Settings...", FL_COMMAND + ',', settings_cb);
    menu->add("&Run/Run &Block or Selection", FL_COMMAND + FL_Enter, run_block);
    menu->add("&Run/Run &Line", FL_COMMAND + FL_ALT + FL_Enter, run_line);
    menu->add("&Run/Run &File", FL_COMMAND + FL_SHIFT + FL_Enter, run_file, nullptr, FL_MENU_DIVIDER);
    menu->add("&Run/&Stop", FL_COMMAND + '.', stop_cb, nullptr, FL_MENU_DIVIDER);
    menu->add("&Run/&Clear Console", FL_COMMAND + 'k', clear_console_cb);
    find_examples();
    for (size_t k = 0; k < example_files.size(); k++) { std::string item = "&Help/&Examples/" + fs::path(example_files[k]).stem().string(); menu->add(item.c_str(), 0, open_example_cb, (void*)(intptr_t)k); }
    menu->add("&Help/&Manual", 0, manual_cb, nullptr, example_files.empty() ? 0 : 0); menu->add("&Help/&About Musil", 0, about_cb);
    int top = 26;
#ifdef __APPLE__
    top = 0;                                              // the menu is in the system bar
#endif
    const int TOOL_H = 32;
    toolbar = new Fl_Group(0, top, W, TOOL_H); toolbar->box(FL_FLAT_BOX); toolbar->color(fl_rgb_color(236, 236, 234));
    { int x = 8, y = top + 4;
      run_btn = tool_button(x, y, 34, "@>", "Run the file (Cmd-Shift-Enter); Cmd-Enter runs the block around the cursor", run_file, nullptr, fl_rgb_color(30, 130, 70));
      stop_btn = tool_button(x, y, 34, "@square", "Stop the running program (Esc)", stop_cb, nullptr, fl_rgb_color(190, 50, 50));
      stop_btn->deactivate();
      x += 10;
      Fl_Button* clr = tool_button(x, y, 34, "\xe2\x9c\x95", "Clear the console (Cmd-K)", clear_console_cb, nullptr, fl_rgb_color(90, 90, 90)); clr->labelsize(16);
      Fl_Box* pad = new Fl_Box(x, y, W - x, 24); toolbar->resizable(pad); }
    toolbar->end();
    top += TOOL_H;
    int tile_h = H - top - STATUS_H, left_w = 900, right_w = W - left_w, ed_h = (int)(tile_h * 0.58), cons_h = tile_h - ed_h - INPUT_H, vars_h = (int)(tile_h * 0.5);
    Fl_Tile* tile = new Fl_Tile(0, top, W, tile_h); ((ide_window*)win)->tile = tile;
    text_buf = new Fl_Text_Buffer(); style_buf = new Fl_Text_Buffer(); text_buf->tab_distance(4);
    Fl_Group* edg = new Fl_Group(0, top, left_w, ed_h);
    banner(0, top, left_w, "  Editor");
    editor = new musil_editor(0, top + BANNER_H, left_w, ed_h - BANNER_H); editor->buffer(text_buf); editor->textfont(font_id(editor_font)); editor->textsize(font_size);
    editor->highlight_data(style_buf, styles, 14, 'A', nullptr, nullptr);
    editor->linenumber_width(show_line_numbers ? 40 : 0); editor->linenumber_size(font_size - 2); editor->linenumber_bgcolor(fl_rgb_color(246, 246, 244)); editor->linenumber_fgcolor(fl_rgb_color(150, 150, 150));
    text_buf->add_modify_callback(style_update, nullptr); text_buf->add_modify_callback(text_changed, nullptr);
    edg->resizable(editor); edg->end();
    Fl_Group* consg = new Fl_Group(0, top + ed_h, left_w, cons_h + INPUT_H);
    banner(0, top + ed_h, left_w, "  Console");
    console_buf = new Fl_Text_Buffer(); console = new Fl_Text_Display(0, top + ed_h + BANNER_H, left_w, cons_h - BANNER_H); console->buffer(console_buf); console->textfont(FL_COURIER); console->textsize(font_size); console->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    input = new console_input(0, top + ed_h + cons_h, left_w, INPUT_H); input->textfont(FL_COURIER); input->textsize(font_size); input->tooltip("type Musil and press Enter; Up/Down history; Tab completes; Esc stops");
    consg->resizable(console); consg->end();
    Fl_Group* varsg = new Fl_Group(left_w, top, right_w, vars_h);
    banner(left_w, top, right_w, "  Environment");
    vars = new Fl_Hold_Browser(left_w, top + BANNER_H, right_w, vars_h - BANNER_H); vars->textfont(FL_COURIER); vars->textsize(font_size - 1); vars->callback(vars_cb); vars->tooltip("variables: green numbers, orange strings, purple lists, blue functions; double-click prints one");
    { static int widths[] = { 160, 0 }; vars->column_widths(widths); vars->column_char('\t'); }
    varsg->resizable(vars); varsg->end();
    Fl_Group* helpg = new Fl_Group(left_w, top + vars_h, right_w, tile_h - vars_h);
    banner(left_w, top + vars_h, right_w, "  Help");
    help_query = new Fl_Input(left_w, top + vars_h + BANNER_H, right_w, 26); help_query->textfont(FL_COURIER); help_query->textsize(font_size); help_query->when(FL_WHEN_CHANGED); help_query->callback(help_search_cb); help_query->tooltip("help: search the documentation of every function");
    help_buf = new Fl_Text_Buffer(); help_view = new Fl_Text_Display(left_w, top + vars_h + BANNER_H + 26, right_w, tile_h - vars_h - BANNER_H - 26); help_view->buffer(help_buf); help_view->textfont(FL_COURIER); help_view->textsize(font_size - 1); help_view->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    helpg->resizable(help_view); helpg->end();
    tile->end();
    status = new Fl_Box(0, H - STATUS_H, W, STATUS_H, ""); status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE); status->labelsize(11); status->labelcolor(fl_rgb_color(120, 120, 120)); status->box(FL_FLAT_BOX);
    win->resizable(tile); win->size_range(800, 500); win->end();
    win->callback(quit_cb);
    help_search_cb(nullptr, nullptr);
    apply_settings();
    update_title();
    Fl::add_timeout(0.03, console_flush, nullptr);
    win->show();
    std::thread worker(interpreter_thread);
    for (int k = 1; k < argc; k++) if (argv[k][0] != '-') { load_file(argv[k]); break; }
    Fl::run();
    S.quit = true; S.cv.notify_all(); worker.join();
    return 0;
}

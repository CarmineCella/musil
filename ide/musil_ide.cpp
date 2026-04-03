// musil_ide.cpp — Musil IDE  (adapted for core.h v0.5)
//
// Threading: evaluation runs on a detached worker thread.
// Yield: not yet implemented — stop button signals the thread but cannot
//        interrupt a running exec() call mid-way. Will be fixed later.

#include <FL/Fl.H>
#ifdef __APPLE__
    #include <FL/x.H>
#endif
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Tile.H>
#include <FL/fl_ask.H>
#include <FL/filename.H>
#include <FL/fl_string_functions.h>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Preferences.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Select_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Sys_Menu_Bar.H>

#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <system_error>

// Threading
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <limits.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

// ── Musil core + libraries ────────────────────────────────────────────────────
// Adjust includes to match your project layout.
#include "core.h"
// Uncomment libraries you have compiled:
// #include "signals.h"
// #include "scientific.h"
// #include "system.h"
// #include "plotting.h"

namespace fs = std::filesystem;

// ── Version ───────────────────────────────────────────────────────────────────
#ifndef VERSION
#define VERSION "0.5"
#endif
#ifndef COPYRIGHT
#define COPYRIGHT "2024 Carmine Cella"
#endif

// ── Globals ───────────────────────────────────────────────────────────────────

Fl_Double_Window  *app_window         = nullptr;
Fl_Menu_Bar       *app_menu_bar       = nullptr;
Fl_Tile           *app_tile           = nullptr;
Fl_Text_Editor    *app_editor         = nullptr;
Fl_Text_Buffer    *app_text_buffer    = nullptr;
Fl_Text_Display   *app_console        = nullptr;
Fl_Text_Buffer    *app_console_buffer = nullptr;

class ListenerInput;
ListenerInput     *app_listener       = nullptr;

Fl_Select_Browser *app_var_browser    = nullptr;

Fl_Button* g_btn_run_script    = nullptr;
Fl_Button* g_btn_run_selection = nullptr;
Fl_Button* g_btn_stop          = nullptr;

bool text_changed = false;
char app_filename[FL_PATH_MAX] = "";

// ── Musil environment (new API) ───────────────────────────────────────────────
Environment musil_env;   // value type — not a pointer

int g_font_size = 16;
Fl_Text_Buffer *app_style_buffer = nullptr;

std::vector<std::string> g_builtin_keywords;  // static keywords for highlighting
std::vector<std::string> g_env_symbols;       // symbols from live env
std::vector<std::string> g_browser_symbols;   // parallel to var browser rows

Fl_Preferences g_prefs(Fl_Preferences::USER, "carminecella", "musil_ide");

std::vector<std::string> g_listener_history;
int g_listener_history_pos = -1;

int g_paren_pos1 = -1;
int g_paren_pos2 = -1;

Fl_Window *g_find_win      = nullptr;
Fl_Input  *g_find_input    = nullptr;
Fl_Input  *g_replace_input = nullptr;

// Threading
std::atomic<bool>   g_eval_running{false};
std::atomic<bool>   g_eval_stop_requested{false};
std::atomic<bool>   g_keywords_need_update{false};
std::mutex          g_console_mutex;
std::queue<std::string> g_console_queue;

std::string g_exe_dir;

// ── Forward declarations ──────────────────────────────────────────────────────

void menu_new_callback(Fl_Widget*, void*);
void menu_open_callback(Fl_Widget*, void*);
void menu_save_callback(Fl_Widget*, void*);
void menu_save_as_callback(Fl_Widget*, void*);
void menu_quit_callback(Fl_Widget*, void*);

void menu_undo_callback(Fl_Widget*, void*);
void menu_redo_callback(Fl_Widget*, void*);
void menu_cut_callback(Fl_Widget*, void*);
void menu_copy_callback(Fl_Widget*, void*);
void menu_paste_callback(Fl_Widget*, void*);
void menu_delete_callback(Fl_Widget*, void*);

void menu_find_dialog_callback(Fl_Widget*, void*);
void menu_find_next_callback(Fl_Widget*, void*);
void menu_replace_one_callback(Fl_Widget*, void*);
void menu_replace_all_callback(Fl_Widget*, void*);

void menu_run_script_callback(Fl_Widget*, void*);
void menu_run_selection_callback(Fl_Widget*, void*);
void menu_stop_callback(Fl_Widget*, void*);
void menu_clear_env_callback(Fl_Widget*, void*);
void menu_paths_callback(Fl_Widget*, void*);
void menu_install_libraries_callback(Fl_Widget*, void*);
void menu_goto_line_callback(Fl_Widget*, void*);

void menu_zoom_in_callback(Fl_Widget*, void*);
void menu_zoom_out_callback(Fl_Widget*, void*);
void menu_syntaxhighlight_callback(Fl_Widget*, void*);
void menu_clear_console_callback(Fl_Widget*, void*);
void menu_about_callback(Fl_Widget*, void*);

void listener_eval_line();

void init_base_keywords();
void update_keywords_from_env_and_browser();

void create_find_dialog();
int  editor_find_next(bool from_start);
void editor_replace_one();
void editor_replace_all();

void build_paths_dialog();
void build_app_window();
void build_app_menu_bar();
void build_main_editor_console_listener();
void update_eval_ui_state();

void console_append(const std::string &s);
void console_append_threadsafe(const std::string &s);
void process_console_queue();

void style_init();
void style_parse_musil(const char*, char*, int);
void var_browser_cb(Fl_Widget*, void*);
void apply_font_size();
void load_file_into_editor(const char*);
void text_changed_callback(int, int, int, int, const char*, void*);

// ── stdout capture ────────────────────────────────────────────────────────────

struct CoutRedirect {
    std::streambuf*    old_buf;
    std::ostringstream capture;

    CoutRedirect() { old_buf = std::cout.rdbuf(capture.rdbuf()); }
    ~CoutRedirect() { std::cout.rdbuf(old_buf); }

    std::string consume() {
        std::string s = capture.str();
        if (!s.empty()) { capture.str(""); capture.clear(); }
        return s;
    }
};

// ── Title / filename tracking ─────────────────────────────────────────────────

void update_title() {
    const char *fname = nullptr;
    if (app_filename[0]) fname = fl_filename_name(app_filename);
    if (fname) {
        char buf[FL_PATH_MAX + 3];
        buf[FL_PATH_MAX + 2] = '\0';
        if (text_changed) snprintf(buf, FL_PATH_MAX + 2, "%s *", fname);
        else              snprintf(buf, FL_PATH_MAX + 2, "%s", fname);
        app_window->copy_label(buf);
    } else {
        app_window->label("Musil IDE");
    }
}

void set_changed(bool v) {
    if (v != text_changed) { text_changed = v; update_title(); }
}

void set_filename(const char *new_filename) {
    if (new_filename) fl_strlcpy(app_filename, new_filename, FL_PATH_MAX);
    else              app_filename[0] = 0;
    update_title();
}

// ── Console helpers (thread-safe) ─────────────────────────────────────────────

void console_append_threadsafe(const std::string &s) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    g_console_queue.push(s);
    Fl::awake();
}

void process_console_queue() {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    while (!g_console_queue.empty()) {
        const std::string& s = g_console_queue.front();
        if (app_console_buffer) {
            app_console_buffer->append(s.c_str());
            app_console->insert_position(app_console_buffer->length());
            app_console->show_insert_position();
            app_console->redraw();
        }
        g_console_queue.pop();
    }
}

void console_append(const std::string &s) {
    if (g_eval_running) {
        console_append_threadsafe(s);
    } else {
        if (app_console_buffer) {
            app_console_buffer->append(s.c_str());
            app_console->insert_position(app_console_buffer->length());
            app_console->show_insert_position();
            app_console->redraw();
        }
    }
}

void console_clear() {
    if (!app_console_buffer) return;
    app_console_buffer->text("");
    app_console->insert_position(0);
    app_console->show_insert_position();
    app_console->redraw();
}

void menu_clear_console_callback(Fl_Widget*, void*) { console_clear(); }

// ── UI state ──────────────────────────────────────────────────────────────────

void update_eval_ui_state() {
    bool running = g_eval_running;

    const Fl_Menu_Item* run_script_item = app_menu_bar->find_item("Evaluate/Run script");
    const Fl_Menu_Item* run_sel_item    = app_menu_bar->find_item("Evaluate/Run selection");
    const Fl_Menu_Item* stop_item       = app_menu_bar->find_item("Evaluate/Stop");

    if (run_script_item) {
        int idx = app_menu_bar->find_index(run_script_item);
        app_menu_bar->mode(idx, running ? FL_MENU_INACTIVE : 0);
    }
    if (run_sel_item) {
        int idx = app_menu_bar->find_index(run_sel_item);
        app_menu_bar->mode(idx, running ? FL_MENU_INACTIVE : 0);
    }
    if (stop_item) {
        int idx = app_menu_bar->find_index(stop_item);
        app_menu_bar->mode(idx, running ? 0 : FL_MENU_INACTIVE);
    }

    if (g_btn_run_script) {
        running ? g_btn_run_script->deactivate() : g_btn_run_script->activate();
        g_btn_run_script->redraw();
    }
    if (g_btn_run_selection) {
        running ? g_btn_run_selection->deactivate() : g_btn_run_selection->activate();
        g_btn_run_selection->redraw();
    }
    if (g_btn_stop) {
        running ? g_btn_stop->activate() : g_btn_stop->deactivate();
        g_btn_stop->redraw();
    }

    Fl::check();
}

// ── Editor buffer change callback ─────────────────────────────────────────────

void text_changed_callback(int, int n_inserted, int n_deleted,
                           int, const char*, void*) {
    if (n_inserted || n_deleted) set_changed(true);
}

// ── Platform helpers ──────────────────────────────────────────────────────────

static std::string get_home_directory() {
#ifdef _WIN32
    const char* h = getenv("USERPROFILE");
    if (!h) h = getenv("HOMEDRIVE");
    return h ? h : "C:\\Users\\Default";
#else
    const char* h = getenv("HOME");
    return h ? h : "/tmp";
#endif
}

static std::string get_resources_dir() {
#ifdef __APPLE__
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        fs::path exe(buf);
        fs::path resources = exe.parent_path().parent_path() / "Resources";
        if (fs::exists(resources)) return resources.string();
    }
    return (fs::current_path() / "Resources").string();
#elif defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len > 0) {
        fs::path resources = fs::path(buf).parent_path() / "Resources";
        if (fs::exists(resources)) return resources.string();
    }
    return (fs::current_path() / "Resources").string();
#else
    return (fs::current_path() / "Resources").string();
#endif
}

// ── File I/O helpers ──────────────────────────────────────────────────────────

void load_file_into_editor(const char *filename) {
    if (app_text_buffer->loadfile(filename) == 0) {
        set_filename(filename);
        set_changed(false);
    } else {
        fl_alert("Failed to load file\n%s\n%s", filename, strerror(errno));
    }
}

static bool install_musil_libraries() {
    std::string src_dir_str;
#ifdef __APPLE__
    src_dir_str = get_resources_dir();
#else
    src_dir_str = !g_exe_dir.empty() ? g_exe_dir : fs::current_path().string();
#endif

    fs::path src_dir(src_dir_str);
    if (!fs::exists(src_dir) || !fs::is_directory(src_dir)) {
        fl_alert("Source directory for Musil libraries not found:\n%s", src_dir_str.c_str());
        return false;
    }

    fs::path dest_dir = fs::path(get_home_directory()) / ".musil";
    std::error_code ec;
    fs::create_directories(dest_dir, ec);
    if (ec) {
        fl_alert("Failed to create destination directory:\n%s", dest_dir.string().c_str());
        return false;
    }

    int copied = 0;
    ec.clear();
    for (auto &entry : fs::directory_iterator(src_dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        fs::path p = entry.path();
        if (p.extension() == ".mu") {   // Musil files use .mu extension
            fs::path dest = dest_dir / p.filename();
            std::error_code copy_ec;
            fs::copy_file(p, dest, fs::copy_options::overwrite_existing, copy_ec);
            console_append(p.filename().string() + "\n");
            if (!copy_ec) ++copied;
        }
    }

    if (ec) {
        fl_alert("Error while scanning Musil library directory:\n%s", src_dir_str.c_str());
        return false;
    }
    if (copied == 0) {
        fl_alert("No .mu files found in:\n%s", src_dir_str.c_str());
        return false;
    }

    fl_message("Installed %d .mu file(s) into:\n%s", copied, dest_dir.string().c_str());
    return true;
}

// ── Environment paths (save/load via preferences) ─────────────────────────────

static void save_env_paths() {
    g_prefs.deleteGroup("paths");
    for (int i = 0; i < (int)musil_env.paths.size(); ++i) {
        char key[32];
        snprintf(key, sizeof(key), "path%d", i);
        g_prefs.set(key, musil_env.paths[i].c_str());
    }
    g_prefs.set("path_count", (int)musil_env.paths.size());
    g_prefs.flush();
}

static void load_env_paths() {
    musil_env.paths.clear();
    int count = 0;
    g_prefs.get("path_count", count, 0);
    for (int i = 0; i < count; ++i) {
        char key[32];
        snprintf(key, sizeof(key), "path%d", i);
        char val[FL_PATH_MAX] = "";
        g_prefs.get(key, val, "", FL_PATH_MAX - 1);
        if (val[0]) musil_env.paths.emplace_back(val);
    }
}

// ── Musil environment initialisation ─────────────────────────────────────────
//
// Creates a fresh Environment, registers all libraries, overrides `load`
// to search env.paths in addition to ~/.musil/, and loads stdlib.

void init_musil_env() {
    // Reconstruct environment by assignment (resets globals/procs/builtins)
    musil_env = Environment{};

    // Register libraries (uncomment what you have compiled)
    // add_signals(musil_env);
    // add_scientific(musil_env);
    // add_system(musil_env);
    // add_plotting(musil_env);

    // Override the built-in `load` to also search env.paths
    musil_env.register_builtin("load",
        [](std::vector<Value>& args, Interpreter& I) -> Value {
            if (args.size() != 1 || !std::holds_alternative<std::string>(args[0]))
                throw Error{I.filename, I.cur_line(),
                            "load: expected 1 string argument"};

            std::string requested = std::get<std::string>(args[0]);
            std::string resolved;
            std::ifstream f;

            // 1. Direct path
            f.open(requested);
            if (f.is_open()) { resolved = requested; }

            // 2. ~/.musil/
            if (!f.is_open()) {
                const char* home = getenv("HOME");
                if (home) {
                    resolved = std::string(home) + "/.musil/" + requested;
                    f.open(resolved);
                }
            }

            // 3. Paths registered in the IDE
            if (!f.is_open()) {
                for (const auto& dir : musil_env.paths) {
                    resolved = dir + "/" + requested;
                    f.open(resolved);
                    if (f.is_open()) break;
                }
            }

            if (!f.is_open())
                throw Error{I.filename, I.cur_line(),
                            "load: can't open '" + requested + "'"};

            std::string src(std::istreambuf_iterator<char>(f), {});
            if (I.load_fn) I.load_fn(src, resolved);
            return NumVal{0.0};
        });

    // Restore saved paths
    load_env_paths();

    std::ostringstream out;
    out << "[Musil v" << VERSION << "]\n\n";
    out << "music scripting language\n";
    out << "(c) " << COPYRIGHT << "  www.carminecella.com\n\n";
    console_append(out.str());

    init_base_keywords();
    update_keywords_from_env_and_browser();
}

// ── Syntax highlighting ───────────────────────────────────────────────────────
//
// Style codes:
//   A - Plain text
//   B - Comment   ( # ... end-of-line )
//   C - String    ( "..." )
//   D - Keyword   ( var, proc, if, while, for, return, ... )
//   E - Bracket   ( { } [ ] ( ) )
//   F - Matching bracket highlight

Fl_Text_Display::Style_Table_Entry styletable[] = {
    { FL_BLACK,      FL_SCREEN,       16 }, // A  plain
    { FL_DARK_GREEN, FL_SCREEN,       16 }, // B  comment
    { FL_BLUE,       FL_SCREEN,       16 }, // C  string
    { FL_DARK_RED,   FL_SCREEN_BOLD,  16 }, // D  keyword
    { FL_DARK_BLUE,  FL_SCREEN_BOLD,  16 }, // E  bracket
    { FL_RED,        FL_SCREEN_BOLD,  16 }  // F  matching bracket
};
const int N_STYLES = sizeof(styletable) / sizeof(styletable[0]);

// Core Musil keywords (language builtins + stdlib constants)
static const char* musil_builtin_keywords[] = {
    // Language keywords
    "var", "proc", "if", "else", "while", "for", "in",
    "return", "break", "and", "or", "not", "print", "load",
    // Builtins
    "abs", "asc", "append", "apply",
    "arr", "ceil", "char", "clock", "concat", "copy",
    "cos", "eval", "exec", "exit", "exp",
    "filter", "find", "floor",
    "input", "join", "keys", "len",
    "linspace", "log", "log2", "lower",
    "map", "num", "ones", "pop", "pow",
    "push", "rand", "range", "read",
    "reduce", "remove", "shuffle", "sin", "slice",
    "split", "sqrt", "str", "sub", "sum",
    "tan", "to_arr", "to_vec", "type", "upper",
    "vec", "write", "zeros",
    // stdlib constants
    "PI", "TAU", "E", "PHI", "LN2", "SQRT2",
    // stdlib functions
    "between", "clamp", "contains", "cov_from_zscore",
    "count_str", "deg2rad", "dot", "ends_with",
    "even", "flatten", "fmt_fixed", "fmt_pct",
    "gcd", "hypot", "index_of", "ipow",
    "lcm", "lerp", "ltrim", "map_range",
    "maximum", "mean", "minimum", "mod",
    "normalize", "odd", "pad_left", "pad_right",
    "rad2deg", "repeat_str", "replace", "reversed",
    "round", "round_to", "rtrim", "sign",
    "sorted", "standardize", "starts_with", "stdev",
    "total", "trim", "zip",
    // test framework
    "assert", "assert_eq", "assert_near", "test_summary"
};
const int N_BUILTIN_KEYWORDS =
    sizeof(musil_builtin_keywords) / sizeof(musil_builtin_keywords[0]);

void init_base_keywords() {
    if (!g_builtin_keywords.empty()) return;
    g_builtin_keywords.reserve(N_BUILTIN_KEYWORDS);
    for (int i = 0; i < N_BUILTIN_KEYWORDS; ++i)
        g_builtin_keywords.emplace_back(musil_builtin_keywords[i]);
}

// Musil identifier: starts with letter or _, continues with alphanumeric or _
bool is_ident_start(char c) {
    return std::isalpha((unsigned char)c) || c == '_';
}
bool is_ident_char(char c) {
    return std::isalnum((unsigned char)c) || c == '_';
}

bool is_keyword(const std::string& s) {
    for (const auto &k : g_builtin_keywords) if (s == k) return true;
    for (const auto &k : g_env_symbols)      if (s == k) return true;
    return false;
}

void style_parse_musil(const char* text, char* style, int length) {
    bool in_comment = false;
    bool in_string  = false;
    int i = 0;
    while (i < length) {
        char c = text[i];

        if (in_comment) {
            style[i] = 'B';
            if (c == '\n') in_comment = false;
            ++i; continue;
        }
        if (in_string) {
            style[i] = 'C';
            if (c == '"' && (i == 0 || text[i-1] != '\\')) in_string = false;
            ++i; continue;
        }

        // Musil uses # for comments
        if (c == '#') {
            in_comment = true;
            style[i] = 'B';
            ++i; continue;
        }

        if (c == '"') {
            in_string = true;
            style[i] = 'C';
            ++i; continue;
        }

        if (c == '(' || c == ')' ||
            c == '{' || c == '}' ||
            c == '[' || c == ']') {
            style[i] = 'E';
            ++i; continue;
        }

        if (is_ident_start(c)) {
            int start = i;
            int j = i + 1;
            while (j < length && is_ident_char(text[j])) j++;
            std::string ident(text + start, text + j);
            char mode = is_keyword(ident) ? 'D' : 'A';
            for (int k = start; k < j; ++k) style[k] = mode;
            i = j; continue;
        }

        style[i] = 'A';
        ++i;
    }
}

void update_paren_match();  // forward declaration

void style_init() {
    int len = app_text_buffer->length();
    if (len < 0) len = 0;

    char* text  = app_text_buffer->text();
    char* style = new char[len + 1];

    if (!text) std::memset(style, 'A', len);
    else       style_parse_musil(text, style, len);
    style[len] = '\0';

    if (!app_style_buffer) app_style_buffer = new Fl_Text_Buffer(len);
    app_style_buffer->text(style);

    delete [] style;
    if (text) free(text);
    update_paren_match();
}

void style_update(int, int, int, int, const char*, void* cbArg) {
    style_init();
    int end = app_text_buffer->length();
    ((Fl_Text_Editor*)cbArg)->redisplay_range(0, end);
    update_paren_match();
}

void menu_syntaxhighlight_callback(Fl_Widget* w, void*) {
    Fl_Menu_Bar* menu = static_cast<Fl_Menu_Bar*>(w);
    const Fl_Menu_Item* item = menu->mvalue();
    if (!item) return;
    if (item->value()) {
        style_init();
        app_editor->highlight_data(app_style_buffer, styletable, N_STYLES, 'A', nullptr, 0);
        app_text_buffer->add_modify_callback(style_update, app_editor);
    } else {
        app_text_buffer->remove_modify_callback(style_update, app_editor);
        app_editor->highlight_data(nullptr, nullptr, 0, 'A', nullptr, 0);
    }
    app_editor->redraw();
}

// ── Variable browser update ───────────────────────────────────────────────────
//
// Iterates the live Environment to populate the browser panel.
// Categorises entries as: Builtins (dark blue), Procs (blue), Globals (green).

void update_keywords_from_env_and_browser() {
    g_env_symbols.clear();
    g_browser_symbols.clear();

    if (!app_var_browser) return;

    // Collect and sort each category
    std::vector<std::string> builtins_list, procs_list, globals_list;

    for (const auto& kv : musil_env.builtins)
        builtins_list.push_back(kv.first);
    for (const auto& kv : musil_env.procs)
        procs_list.push_back(kv.first);
    for (const auto& kv : musil_env.globals)
        globals_list.push_back(kv.first);

    std::sort(builtins_list.begin(), builtins_list.end());
    std::sort(procs_list.begin(),    procs_list.end());
    std::sort(globals_list.begin(),  globals_list.end());

    app_var_browser->clear();
    app_var_browser->format_char('@');

    auto add_header = [&](const char* title) {
        std::ostringstream oss;
        oss << "@B" << (int)FL_GRAY << "@C" << (int)FL_BLACK << " " << title;
        app_var_browser->add(oss.str().c_str());
        g_browser_symbols.push_back("");  // header → non-clickable
    };

    auto add_entry = [&](const std::string& name, Fl_Color color) {
        std::ostringstream oss;
        oss << "@C" << (int)color << "  " << name;
        app_var_browser->add(oss.str().c_str());
        g_browser_symbols.push_back(name);
        g_env_symbols.push_back(name);
    };

    if (!globals_list.empty()) {
        add_header("Variables");
        for (const auto& n : globals_list) add_entry(n, FL_DARK_GREEN);
    }
    if (!procs_list.empty()) {
        add_header("Procs");
        for (const auto& n : procs_list)  add_entry(n, FL_BLUE);
    }
    if (!builtins_list.empty()) {
        add_header("Builtins");
        for (const auto& n : builtins_list) add_entry(n, FL_DARK_BLUE);
    }

    app_var_browser->redraw();
}

// ── Parenthesis matching ──────────────────────────────────────────────────────

void update_paren_match() {
    if (!app_text_buffer || !app_style_buffer || !app_editor) return;

    int len = app_text_buffer->length();
    if (len <= 0) { g_paren_pos1 = g_paren_pos2 = -1; return; }

    auto restore_style_at = [](int pos) {
        if (pos < 0 || pos >= app_style_buffer->length()) return;
        char buf[2] = {'E', '\0'};
        app_style_buffer->replace(pos, pos + 1, buf);
    };

    restore_style_at(g_paren_pos1);
    restore_style_at(g_paren_pos2);
    g_paren_pos1 = g_paren_pos2 = -1;

    char* text = app_text_buffer->text();
    if (!text) return;

    int cursor = app_editor->insert_position();
    if (cursor < 0 || cursor > len) { free(text); return; }

    int paren_pos = -1;
    static const std::string PARENS = "(){}[]";
    if (cursor > 0 && PARENS.find(text[cursor-1]) != std::string::npos)
        paren_pos = cursor - 1;
    else if (cursor < len && PARENS.find(text[cursor]) != std::string::npos)
        paren_pos = cursor;

    if (paren_pos < 0) {
        free(text);
        app_editor->redisplay_range(0, len);
        return;
    }

    char c = text[paren_pos];
    int match_pos = -1;

    auto match_forward = [&](char open_c, char close_c, int start) {
        int depth = 1;
        for (int i = start; i < len; ++i) {
            if (text[i] == open_c) depth++;
            else if (text[i] == close_c) { if (--depth == 0) return i; }
        }
        return -1;
    };
    auto match_backward = [&](char open_c, char close_c, int start) {
        int depth = 1;
        for (int i = start; i >= 0; --i) {
            if (text[i] == close_c) depth++;
            else if (text[i] == open_c) { if (--depth == 0) return i; }
        }
        return -1;
    };

    if      (c == '(') match_pos = match_forward ('(', ')', paren_pos + 1);
    else if (c == ')') match_pos = match_backward('(', ')', paren_pos - 1);
    else if (c == '{') match_pos = match_forward ('{', '}', paren_pos + 1);
    else if (c == '}') match_pos = match_backward('{', '}', paren_pos - 1);
    else if (c == '[') match_pos = match_forward ('[', ']', paren_pos + 1);
    else if (c == ']') match_pos = match_backward('[', ']', paren_pos - 1);

    if (match_pos < 0) {
        free(text);
        app_editor->redisplay_range(0, len);
        return;
    }

    auto set_match_style = [](int pos) {
        if (pos < 0 || pos >= app_style_buffer->length()) return;
        char buf[2] = {'F', '\0'};
        app_style_buffer->replace(pos, pos + 1, buf);
    };

    set_match_style(paren_pos);
    set_match_style(match_pos);
    g_paren_pos1 = paren_pos;
    g_paren_pos2 = match_pos;

    int lo = std::min(paren_pos, match_pos);
    int hi = std::max(paren_pos, match_pos);
    app_editor->redisplay_range(lo, hi + 1);
    free(text);
}

// ── Autocomplete ──────────────────────────────────────────────────────────────

std::vector<std::string> autocomplete_candidates(const std::string& prefix) {
    std::vector<std::string> res;
    if (prefix.empty()) return res;

    auto add_if_match = [&](const std::string& s) {
        if (s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0)
            res.push_back(s);
    };

    for (const auto& k : g_builtin_keywords) add_if_match(k);
    for (const auto& k : g_env_symbols)      add_if_match(k);

    std::sort(res.begin(), res.end());
    res.erase(std::unique(res.begin(), res.end()), res.end());
    return res;
}

class MusilEditor : public Fl_Text_Editor {
public:
    MusilEditor(int X, int Y, int W, int H, const char* L = 0)
        : Fl_Text_Editor(X, Y, W, H, L) {}

    void do_autocomplete() {
        int pos = insert_position();
        if (pos <= 0) return;
        int start = pos - 1;
        char *txt = app_text_buffer->text();
        if (!txt) return;
        while (start >= 0 && is_ident_char(txt[start])) --start;
        start++;
        if (start >= pos) { free(txt); return; }
        std::string prefix(txt + start, txt + pos);
        free(txt);

        auto cand = autocomplete_candidates(prefix);
        if (cand.empty()) return;

        if (cand.size() == 1) {
            const std::string& full = cand[0];
            if (full.size() > prefix.size()) {
                std::string extra = full.substr(prefix.size());
                app_text_buffer->insert(pos, extra.c_str());
                insert_position(pos + (int)extra.size());
            }
            return;
        }

        int cx, cy;
        this->position_to_xy(pos, &cx, &cy);
        int sx = this->x() + cx;
        int sy = this->y() + cy + this->textsize() + 4;

        Fl_Menu_Button popup(sx, sy, 0, 0);
        popup.type(Fl_Menu_Button::POPUP3);

        std::vector<Fl_Menu_Item> items;
        items.reserve(cand.size() + 1);
        for (const auto& s : cand) {
            Fl_Menu_Item mi = { s.c_str(), 0, 0, 0, 0, 0, 0, 0, 0 };
            items.push_back(mi);
        }
        items.push_back({ nullptr, 0, 0, 0, 0, 0, 0, 0, 0 });
        popup.menu(items.data());
        popup.position(Fl::event_x_root(), Fl::event_y_root());
        const Fl_Menu_Item* picked = popup.popup();
        if (picked && picked->text) {
            std::string full = picked->text;
            if (full.size() > prefix.size()) {
                std::string extra = full.substr(prefix.size());
                app_text_buffer->insert(pos, extra.c_str());
                insert_position(pos + (int)extra.size());
            }
        }
    }

    int handle(int ev) override {
        int ret = Fl_Text_Editor::handle(ev);
        if (ev == FL_KEYDOWN) {
            if (Fl::event_key() == FL_Tab && (Fl::event_state() & FL_CTRL)) {
                do_autocomplete();
                return 1;
            }
        }
        if (ev == FL_KEYDOWN || ev == FL_KEYUP  ||
            ev == FL_FOCUS   || ev == FL_UNFOCUS ||
            ev == FL_PUSH    || ev == FL_DRAG    || ev == FL_RELEASE) {
            update_paren_match();
        }
        return ret;
    }
};

// ── Listener input ────────────────────────────────────────────────────────────

class ListenerInput : public Fl_Input {
public:
    ListenerInput(int X, int Y, int W, int H, const char* L = 0)
        : Fl_Input(X, Y, W, H, L) {}

    int handle(int ev) override {
        if (ev == FL_KEYDOWN) {
            int key = Fl::event_key();
            if (key == FL_Enter) { listener_eval_line(); return 1; }
            if (key == FL_Up) {
                if (!g_listener_history.empty()) {
                    if (g_listener_history_pos < 0 ||
                        g_listener_history_pos > (int)g_listener_history.size())
                        g_listener_history_pos = (int)g_listener_history.size();
                    if (g_listener_history_pos > 0) {
                        g_listener_history_pos--;
                        value(g_listener_history[g_listener_history_pos].c_str());
                        insert_position(size());
                    }
                }
                return 1;
            }
            if (key == FL_Down) {
                if (!g_listener_history.empty()) {
                    if (g_listener_history_pos < 0)
                        g_listener_history_pos = (int)g_listener_history.size();
                    if (g_listener_history_pos < (int)g_listener_history.size() - 1) {
                        g_listener_history_pos++;
                        value(g_listener_history[g_listener_history_pos].c_str());
                    } else {
                        g_listener_history_pos = (int)g_listener_history.size();
                        value("");
                    }
                    insert_position(size());
                }
                return 1;
            }
        }
        return Fl_Input::handle(ev);
    }
};

void eval_code(const std::string &code, bool is_script = false);

void listener_eval_line() {
    if (!app_listener) return;
    std::string line = app_listener->value();
    if (line.empty()) { app_listener->value(""); return; }

    if (g_listener_history.empty() || g_listener_history.back() != line)
        g_listener_history.push_back(line);
    g_listener_history_pos = (int)g_listener_history.size();

    console_append(">> " + line + "\n");
    eval_code(line, false);
    console_append("\n");
    app_listener->value("");
}

// ── Threaded evaluation ───────────────────────────────────────────────────────
//
// eval_code_worker runs on a detached thread.
// It redirects stdout and calls musil_env.exec(code, filename).
// NOTE: without a yield hook, Stop can only interrupt *between* top-level
//       statements in the future; for now it has no effect on a running exec().

void eval_code_worker(const std::string code, bool is_script) {
    struct EvalGuard {
        ~EvalGuard() {
            g_eval_running         = false;
            g_keywords_need_update = true;
        }
    } guard;

    CoutRedirect redirect;

    // Determine filename for error messages
    std::string fname = app_filename[0] ? std::string(app_filename) : "<listener>";

    try {
        if (g_eval_stop_requested) {
            console_append_threadsafe("[Evaluation stopped]\n");
            return;
        }

        musil_env.exec(code, fname);

    } catch (Error& e) {
        // Musil language error — has file/line/message/trace
        std::string msg = format_error(e) + "\n";
        std::cout << msg;
    } catch (std::exception &ex) {
        std::cout << "error: " << ex.what() << "\n";
    } catch (...) {
        std::cout << "unknown error during evaluation\n";
    }

    // Flush any remaining captured output
    std::string output = redirect.consume();
    if (!output.empty()) console_append_threadsafe(output);
}

void eval_code(const std::string &code, bool is_script) {
    if (g_eval_running) {
        fl_alert("Evaluation is already running.\nPlease wait or click Stop.");
        return;
    }

    g_eval_running        = true;
    g_eval_stop_requested = false;
    update_eval_ui_state();
    Fl::check();

    std::thread worker([code, is_script]() {
        eval_code_worker(code, is_script);
    });
    worker.detach();
}

// ── Evaluate menu callbacks ───────────────────────────────────────────────────

void menu_run_script_callback(Fl_Widget*, void*) {
    console_append("[Run script]\n");
    char *text = app_text_buffer->text();
    if (!text) { console_append("(empty buffer)\n\n"); return; }
    std::string code(text);
    free(text);
    eval_code(code, true);
    console_append("\n");
}

void menu_run_selection_callback(Fl_Widget*, void*) {
    int start, end;
    if (!app_text_buffer->selection_position(&start, &end)) {
        console_append("[Run selection] no selection — running entire script.\n");
        menu_run_script_callback(nullptr, nullptr);
        return;
    }
    char *sel = app_text_buffer->text_range(start, end);
    if (!sel) { console_append("[Run selection] empty selection.\n\n"); return; }
    std::string code(sel);
    free(sel);
    console_append("[Run selection]\n");
    eval_code(code, false);
    console_append("\n");
}

void menu_stop_callback(Fl_Widget*, void*) {
    if (!g_eval_running) {
        fl_message("No evaluation is currently running.");
        return;
    }
    g_eval_stop_requested = true;
    console_append("\n[Stop requested — takes effect between statements]\n");
}

void menu_clear_env_callback(Fl_Widget*, void*) {
    console_clear();
    init_musil_env();
}

void menu_paths_callback(Fl_Widget*, void*) { build_paths_dialog(); }

void menu_install_libraries_callback(Fl_Widget*, void*) {
    int r = fl_choice(
        "Install Musil libraries?\n\n"
        "This will copy all *.mu files from the Resources folder\n"
        "into your ~/.musil directory.",
        "No", "Yes", nullptr);
    if (r == 1) install_musil_libraries();
}

// ── Zoom ──────────────────────────────────────────────────────────────────────

void apply_font_size() {
    for (int i = 0; i < N_STYLES; ++i)
        styletable[i].size = g_font_size;
    if (app_editor)      { app_editor->textsize(g_font_size);      app_editor->redraw(); }
    if (app_console)     { app_console->textsize(g_font_size);     app_console->redraw(); }
    if (app_listener)    { app_listener->textsize(g_font_size);    app_listener->redraw(); }
    if (app_var_browser) { app_var_browser->textsize(g_font_size); app_var_browser->redraw(); }
}

void menu_zoom_in_callback(Fl_Widget*, void*) {
    g_font_size += 2;
    if (g_font_size > 32) g_font_size = 32;
    apply_font_size();
}
void menu_zoom_out_callback(Fl_Widget*, void*) {
    g_font_size -= 2;
    if (g_font_size < 8) g_font_size = 8;
    apply_font_size();
}

// ── Edit menu callbacks ───────────────────────────────────────────────────────

void menu_undo_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor) Fl_Text_Editor::kf_undo(0, (Fl_Text_Editor*)e);
}
void menu_redo_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor) Fl_Text_Editor::kf_redo(0, (Fl_Text_Editor*)e);
}
void menu_cut_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor) Fl_Text_Editor::kf_cut(0, (Fl_Text_Editor*)e);
}
void menu_copy_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor) Fl_Text_Editor::kf_copy(0, (Fl_Text_Editor*)e);
}
void menu_paste_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor) Fl_Text_Editor::kf_paste(0, (Fl_Text_Editor*)e);
}
void menu_delete_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor) Fl_Text_Editor::kf_delete(0, (Fl_Text_Editor*)e);
}

void menu_goto_line_callback(Fl_Widget*, void*) {
    if (!app_editor || !app_text_buffer) return;
    int current_pos  = app_editor->insert_position();
    int current_line = app_text_buffer->count_lines(0, current_pos) + 1;

    const char* input = fl_input("Go to line:", std::to_string(current_line).c_str());
    if (!input) return;
    int line_num = atoi(input);
    if (line_num < 1) { fl_alert("Invalid line number (must be >= 1)."); return; }

    int total_lines = app_text_buffer->count_lines(0, app_text_buffer->length());
    if (line_num > total_lines) {
        fl_alert("Line %d is beyond the last line (%d).", line_num, total_lines);
        return;
    }

    int pos = app_text_buffer->skip_lines(0, line_num - 1);
    app_editor->insert_position(pos);
    app_editor->show_insert_position();
    app_text_buffer->select(pos, app_text_buffer->line_end(pos));
    app_editor->redraw();
}

// ── File menu callbacks ───────────────────────────────────────────────────────

void menu_quit_callback(Fl_Widget*, void*) {
    if (text_changed) {
        int r = fl_choice("The current file has not been saved.\n"
                          "Would you like to save it now?",
                          "Cancel", "Save", "Don't Save");
        if (r == 0) return;
        if (r == 1) { menu_save_as_callback(nullptr, nullptr); return; }
    }

    if (g_eval_running) {
        g_eval_stop_requested = true;
        console_append("[Waiting for evaluation to stop...]\n");
        int wait = 0;
        while (g_eval_running && wait < 50) { Fl::wait(0.1); process_console_queue(); wait++; }
        if (g_eval_running) {
            int r = fl_choice("Evaluation is still running.\nForce quit anyway?",
                              "Cancel", "Force Quit", nullptr);
            if (r == 0) return;
        }
    }

    if (app_window) {
        g_prefs.set("win_x", app_window->x());
        g_prefs.set("win_y", app_window->y());
        g_prefs.set("win_w", app_window->w());
        g_prefs.set("win_h", app_window->h());
        g_prefs.flush();
    }
    Fl::hide_all_windows();
}

void menu_new_callback(Fl_Widget*, void*) {
    if (text_changed) {
        int c = fl_choice("Changes have not been saved.\nStart a new file anyway?",
                          "New", "Cancel", nullptr);
        if (c == 1) return;
    }
    app_text_buffer->text("");
    set_filename(nullptr);
    set_changed(false);
}

void menu_open_callback(Fl_Widget*, void*) {
    if (text_changed) {
        int r = fl_choice("The current file has not been saved.\n"
                          "Would you like to save it now?",
                          "Cancel", "Save", "Don't Save");
        if (r == 0) return;
        if (r == 1) menu_save_as_callback(nullptr, nullptr);
    }

    Fl_Native_File_Chooser fc;
    fc.title("Open File...");
    fc.type(Fl_Native_File_Chooser::BROWSE_FILE);
    if (app_filename[0]) {
        char tmp[FL_PATH_MAX];
        fl_strlcpy(tmp, app_filename, FL_PATH_MAX);
        const char *name = fl_filename_name(tmp);
        if (name) {
            fc.preset_file(name);
            tmp[name - tmp] = 0;
            fc.directory(tmp);
        }
    }
    if (fc.show() == 0) load_file_into_editor(fc.filename());
}

void menu_save_as_callback(Fl_Widget*, void*) {
    Fl_Native_File_Chooser fc;
    fc.title("Save File As...");
    fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    if (app_filename[0]) {
        char tmp[FL_PATH_MAX];
        fl_strlcpy(tmp, app_filename, FL_PATH_MAX);
        const char *name = fl_filename_name(tmp);
        if (name) {
            fc.preset_file(name);
            tmp[name - tmp] = 0;
            fc.directory(tmp);
        }
    }
    if (fc.show() == 0) {
        if (app_text_buffer->savefile(fc.filename()) == 0) {
            set_filename(fc.filename());
            set_changed(false);
        } else {
            fl_alert("Failed to save file\n%s\n%s", fc.filename(), strerror(errno));
        }
    }
}

void menu_save_callback(Fl_Widget*, void*) {
    if (!app_filename[0]) {
        menu_save_as_callback(nullptr, nullptr);
    } else {
        if (app_text_buffer->savefile(app_filename) == 0) set_changed(false);
        else fl_alert("Failed to save file\n%s\n%s", app_filename, strerror(errno));
    }
}

// ── Find / Replace ────────────────────────────────────────────────────────────

void create_find_dialog() {
    if (g_find_win) { g_find_win->show(); return; }

    g_find_win      = new Fl_Window(320, 130, "Find / Replace");
    g_find_input    = new Fl_Input(80, 10, 230, 25, "Find:");
    g_replace_input = new Fl_Input(80, 40, 230, 25, "Replace:");

    Fl_Button* btn_find_next = new Fl_Button(10,  80, 90, 25, "Find next");
    Fl_Button* btn_replace   = new Fl_Button(110, 80, 90, 25, "Replace");
    Fl_Button* btn_all       = new Fl_Button(210, 80, 90, 25, "Replace all");

    btn_find_next->callback([](Fl_Widget*, void*) { menu_find_next_callback(nullptr, nullptr); });
    btn_replace->callback  ([](Fl_Widget*, void*) { menu_replace_one_callback(nullptr, nullptr); });
    btn_all->callback      ([](Fl_Widget*, void*) { menu_replace_all_callback(nullptr, nullptr); });

    g_find_win->end();
    g_find_win->set_non_modal();
    g_find_win->show();
}

int editor_find_next(bool from_start) {
    if (!app_text_buffer || !g_find_input) return -1;
    const char* needle = g_find_input->value();
    if (!needle || !*needle) return -1;
    int start = from_start ? 0 : app_editor->insert_position();
    int found = app_text_buffer->search_forward(start, needle, &start);
    if (found) {
        int len = (int)strlen(needle);
        app_editor->insert_position(start + len);
        app_editor->show_insert_position();
        app_text_buffer->select(start, start + len);
        return start;
    }
    return -1;
}

void editor_replace_one() {
    if (!app_text_buffer || !g_find_input || !g_replace_input) return;
    const char* needle = g_find_input->value();
    const char* repl   = g_replace_input->value();
    if (!needle || !*needle) return;
    int start = 0, end = 0;
    if (app_text_buffer->selection_position(&start, &end) && end > start)
        app_text_buffer->replace_selection(repl ? repl : "");
    else if (editor_find_next(false) >= 0)
        app_text_buffer->replace_selection(repl ? repl : "");
}

void editor_replace_all() {
    if (!app_text_buffer || !g_find_input || !g_replace_input) return;
    const char* needle = g_find_input->value();
    const char* repl   = g_replace_input->value();
    if (!needle || !*needle) return;
    int pos = 0, len = (int)strlen(needle);
    app_editor->insert_position(0);
    while (app_text_buffer->search_forward(pos, needle, &pos)) {
        app_text_buffer->select(pos, pos + len);
        app_text_buffer->replace_selection(repl ? repl : "");
        pos += (int)strlen(repl ? repl : "");
    }
}

void menu_find_dialog_callback(Fl_Widget*, void*) { create_find_dialog(); }
void menu_find_next_callback(Fl_Widget*, void*)    { editor_find_next(false); }
void menu_replace_one_callback(Fl_Widget*, void*)  { editor_replace_one(); }
void menu_replace_all_callback(Fl_Widget*, void*)  { editor_replace_all(); }

// ── Paths dialog ──────────────────────────────────────────────────────────────

struct PathsDialog {
    Fl_Double_Window  *win  = nullptr;
    Fl_Select_Browser *list = nullptr;
};

static void paths_add_cb(Fl_Widget*, void *userdata) {
    PathsDialog *dlg = static_cast<PathsDialog*>(userdata);
    if (!dlg || !dlg->list) return;

    Fl_Native_File_Chooser ch;
    ch.title("Select folder to add to search paths");
    ch.type(Fl_Native_File_Chooser::BROWSE_DIRECTORY);
    if (ch.show() == 0) {
        const char *dir = ch.filename();
        if (dir && *dir) {
            std::string s = dir;
            auto it = std::find(musil_env.paths.begin(), musil_env.paths.end(), s);
            if (it == musil_env.paths.end()) {
                musil_env.paths.push_back(s);
                dlg->list->add(s.c_str());
            }
        }
    }
}

static void paths_remove_cb(Fl_Widget*, void *userdata) {
    PathsDialog *dlg = static_cast<PathsDialog*>(userdata);
    if (!dlg || !dlg->list) return;
    int idx = dlg->list->value();
    if (idx <= 0) return;
    dlg->list->remove(idx);
    if (idx - 1 >= 0 && idx - 1 < (int)musil_env.paths.size())
        musil_env.paths.erase(musil_env.paths.begin() + (idx - 1));
}

static void paths_close_cb(Fl_Widget*, void *userdata) {
    PathsDialog *dlg = static_cast<PathsDialog*>(userdata);
    if (!dlg || !dlg->win) return;
    save_env_paths();
    load_env_paths();
    dlg->win->hide();
}

void build_paths_dialog() {
    PathsDialog *dlg = new PathsDialog;
    int W = 500, H = 320;
    dlg->win  = new Fl_Double_Window(W, H, "Environment Paths");
    dlg->win->set_modal();
    dlg->list = new Fl_Select_Browser(10, 10, W - 20, H - 70);
    dlg->list->when(FL_WHEN_RELEASE);

    for (const auto &p : musil_env.paths) dlg->list->add(p.c_str());
    if (!musil_env.paths.empty()) dlg->list->select(1);

    dlg->list->callback(
        [](Fl_Widget *w, void*) {
            int line = static_cast<Fl_Select_Browser*>(w)->value();
            if (line > 0) static_cast<Fl_Select_Browser*>(w)->select(line);
        }, dlg);

    new Fl_Button(10,     H - 50, 80, 30, "Add...")->callback(paths_add_cb,    dlg);
    new Fl_Button(100,    H - 50, 80, 30, "Remove")->callback(paths_remove_cb, dlg);
    new Fl_Button(W - 90, H - 50, 80, 30, "Close") ->callback(paths_close_cb,  dlg);

    dlg->win->end();
    dlg->win->show();
}

// ── Variable browser callback ─────────────────────────────────────────────────
//
// Clicking a name in the browser prints its value to the console
// by executing  print <name>  in the Musil environment.

void var_browser_cb(Fl_Widget *w, void *) {
    auto *browser = static_cast<Fl_Select_Browser*>(w);
    int line = browser->value();
    if (line <= 0 || line > (int)g_browser_symbols.size()) return;
    const std::string& name = g_browser_symbols[line - 1];
    if (name.empty()) return;   // header row

    std::string code = "print " + name;
    console_append(">> " + code + "\n");
    eval_code(code, false);
    console_append("\n");
}

// ── About ──────────────────────────────────────────────────────────────────────

void menu_about_callback(Fl_Widget*, void*) {
    std::ostringstream oss;
    oss << "Musil IDE\n\nVersion " << VERSION << "\n"
        << "Music scripting language and IDE\n\n"
        << "(c) " << COPYRIGHT << "\n"
        << "www.carminecella.com";
    fl_message("%s", oss.str().c_str());
}

// ── Timeout callback (console queue + UI refresh) ─────────────────────────────

void console_timeout_callback(void*) {
    process_console_queue();
    update_eval_ui_state();

    if (g_keywords_need_update) {
        g_keywords_need_update = false;
        update_keywords_from_env_and_browser();
        if (app_editor && app_text_buffer && app_style_buffer) {
            style_init();
            app_editor->redisplay_range(0, app_text_buffer->length());
        }
    }

    Fl::repeat_timeout(0.1, console_timeout_callback);
}

// ── UI building ───────────────────────────────────────────────────────────────

void build_app_window() {
    app_window = new Fl_Double_Window(800, 600, "Musil IDE");
}

void build_app_menu_bar() {
    app_window->begin();

#ifdef __APPLE__
    app_menu_bar = new Fl_Sys_Menu_Bar(0, 0, app_window->w(), 25);
#else
    app_menu_bar = new Fl_Menu_Bar(0, 0, app_window->w(), 25);
#endif

    // File
    app_menu_bar->add("File/New",         FL_COMMAND+'n', menu_new_callback);
    app_menu_bar->add("File/Open...",     FL_COMMAND+'o', menu_open_callback);
    app_menu_bar->add("File/Save",        FL_COMMAND+'s', menu_save_callback);
    app_menu_bar->add("File/Save as...",  FL_COMMAND+'S', menu_save_as_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("File/Quit",        FL_COMMAND+'q', menu_quit_callback);

    // Edit
    app_menu_bar->add("Edit/Undo",        FL_COMMAND+'z', menu_undo_callback);
    app_menu_bar->add("Edit/Redo",        FL_COMMAND+'Z', menu_redo_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("Edit/Cut",         FL_COMMAND+'x', menu_cut_callback);
    app_menu_bar->add("Edit/Copy",        FL_COMMAND+'c', menu_copy_callback);
    app_menu_bar->add("Edit/Paste",       FL_COMMAND+'v', menu_paste_callback);
    app_menu_bar->add("Edit/Delete",      0,              menu_delete_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("Edit/Find...",     FL_COMMAND+'f', menu_find_dialog_callback);
    app_menu_bar->add("Edit/Find next",   FL_COMMAND+'g', menu_find_next_callback);
    app_menu_bar->add("Edit/Replace...",  FL_COMMAND+'h', menu_find_dialog_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("Edit/Go to Line...", FL_COMMAND+'l', menu_goto_line_callback);

    // Evaluate
    app_menu_bar->add("Evaluate/Run script",          FL_COMMAND+'r', menu_run_script_callback);
    app_menu_bar->add("Evaluate/Run selection",       FL_COMMAND+'e', menu_run_selection_callback);
    app_menu_bar->add("Evaluate/Stop",                FL_COMMAND+',', menu_stop_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("Evaluate/Reset environment",   FL_COMMAND+'j', menu_clear_env_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("Evaluate/Paths...",            0,              menu_paths_callback);
    app_menu_bar->add("Evaluate/Install libraries...", 0,             menu_install_libraries_callback);

    // View
    app_menu_bar->add("View/Zoom in",      FL_COMMAND+'+', menu_zoom_in_callback);
    app_menu_bar->add("View/Zoom out",     FL_COMMAND+'-', menu_zoom_out_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("View/Clear console", FL_COMMAND+'k', menu_clear_console_callback);

#ifndef __APPLE__
    app_menu_bar->add("Help/About...", 0, menu_about_callback);
#endif

    app_window->callback(menu_quit_callback);
    app_window->end();
}

void build_main_editor_console_listener() {
    int win_w = app_window->w();
    int win_h = app_window->h();

    int menu_h = 0;
#ifndef __APPLE__
    menu_h = app_menu_bar ? app_menu_bar->h() : 0;
#endif
    int toolbar_h = 28;

    app_window->begin();

    app_text_buffer = new Fl_Text_Buffer();
    app_text_buffer->add_modify_callback(text_changed_callback, nullptr);

    // Toolbar
    Fl_Group* toolbar = new Fl_Group(0, menu_h, win_w, toolbar_h);
    toolbar->box(FL_FLAT_BOX);
    Fl_Color toolbar_col = fl_rgb_color(230, 230, 230);
    toolbar->color(toolbar_col);

    int bx = 6, by = menu_h + 4, bw = 26, bh = toolbar_h - 8;

    auto make_icon_button = [&](const char* sym, const char* tip,
                                Fl_Callback* cb) -> Fl_Button* {
        Fl_Button* b = new Fl_Button(bx, by, bw, bh);
        b->box(FL_FLAT_BOX);
        b->color(toolbar_col);
        b->selection_color(fl_color_average(FL_BLACK, toolbar_col, 0.1f));
        b->label(sym);
        b->labeltype(FL_SYMBOL_LABEL);
        b->labelcolor(FL_DARK3);
        b->callback(cb);
        if (tip) b->tooltip(tip);
        bx += bw + 4;
        return b;
    };

    g_btn_run_script = make_icon_button("@>",      "Run script",       menu_run_script_callback);
    g_btn_stop       = make_icon_button("@square", "Stop",             menu_stop_callback);
    make_icon_button("@reload", "Reset environment", menu_clear_env_callback);
    make_icon_button("X",       "Clear console",     menu_clear_console_callback);

    toolbar->end();

    // Main tile
    int tile_y = menu_h + toolbar_h;
    int tile_h = win_h - tile_y;
    int right_panel_w = 200;

    app_tile = new Fl_Tile(0, tile_y, win_w - right_panel_w, tile_h);

    int editor_h = (app_tile->h() * 3) / 5;

    app_editor = new MusilEditor(app_tile->x(), app_tile->y(),
                                 app_tile->w(), editor_h);
    app_editor->buffer(app_text_buffer);
    app_editor->textfont(FL_SCREEN);
    app_editor->textsize(g_font_size);
    app_editor->linenumber_width(50);
    app_tile->add(app_editor);

    // Bottom group: listener + console
    int bottom_y = app_editor->y() + app_editor->h();
    int bottom_h = app_tile->h() - app_editor->h();
    Fl_Group *bottom_group = new Fl_Group(app_tile->x(), bottom_y,
                                          app_tile->w(), bottom_h);
    int bg_x = bottom_group->x(), bg_y = bottom_group->y();
    int bg_w = bottom_group->w(), bg_h = bottom_group->h();
    int listener_h = 26;

    app_listener = new ListenerInput(bg_x, bg_y, bg_w, listener_h);
    app_listener->textfont(FL_SCREEN);
    app_listener->textsize(g_font_size);

    app_console_buffer = new Fl_Text_Buffer();
    app_console = new Fl_Text_Display(bg_x, bg_y + listener_h,
                                      bg_w, bg_h - listener_h);
    app_console->buffer(app_console_buffer);
    app_console->textfont(FL_SCREEN);
    app_console->textsize(g_font_size);
    app_console->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);

    bottom_group->resizable(app_console);
    bottom_group->end();
    app_tile->add(bottom_group);
    app_tile->resizable(app_editor);
    app_tile->end();

    // Right-side variables browser
    int right_x = app_tile->x() + app_tile->w();
    int right_y = tile_y;
    int right_w = win_w - app_tile->w();
    int right_h = win_h - right_y;

    app_var_browser = new Fl_Select_Browser(right_x, right_y,
                                            right_w, right_h, "Vars");
    app_var_browser->textfont(FL_HELVETICA);
    app_var_browser->textsize(8);
    app_var_browser->when(FL_WHEN_RELEASE);
    app_var_browser->callback(var_browser_cb);

    app_window->resizable(app_tile);
    app_window->end();
    app_tile->init_sizes();
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
    try {
        Fl::scheme("oxy");
        Fl::args_to_utf8(argc, argv);

        try {
            if (argc > 0 && argv[0]) {
                std::error_code ec;
                fs::path canon = fs::canonical(fs::path(argv[0]), ec);
                g_exe_dir = (ec ? fs::path(argv[0]) : canon).parent_path().string();
            } else {
                g_exe_dir = fs::current_path().string();
            }
        } catch (...) {
            g_exe_dir = fs::current_path().string();
        }

        build_app_window();
        build_app_menu_bar();
        build_main_editor_console_listener();

#ifdef __APPLE__
        fl_mac_set_about(menu_about_callback, nullptr);
#endif

        if (argc > 1 && argv[1] && argv[1][0] != '-')
            load_file_into_editor(argv[1]);

        int win_x = 100, win_y = 100, win_w = 640, win_h = 480;
        g_prefs.get("win_x", win_x, win_x);
        g_prefs.get("win_y", win_y, win_y);
        g_prefs.get("win_w", win_w, win_w);
        g_prefs.get("win_h", win_h, win_h);

        app_window->resize(win_x, win_y, win_w, win_h);
        app_window->show(argc, argv);

        init_musil_env();

        style_init();
        app_editor->highlight_data(app_style_buffer, styletable,
                                   N_STYLES, 'A', nullptr, 0);
        app_text_buffer->add_modify_callback(style_update, app_editor);

        apply_font_size();
        update_title();
        update_eval_ui_state();

        Fl::add_timeout(0.1, console_timeout_callback);
        return Fl::run();

    } catch (std::exception &e) {
        fl_alert("Fatal error: %s", e.what());
        return 1;
    } catch (...) {
        fl_alert("Fatal unknown error");
        return 1;
    }
}

// eof

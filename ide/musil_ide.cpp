// musil_ide.cpp
//

#include <FL/Fl.H>
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

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <limits.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;


#include "musil.h" // your interpreter core

// -----------------------------------------------------------------------------
// Globals (editor/console/listener state)
// -----------------------------------------------------------------------------

Fl_Double_Window *app_window         = nullptr;
Fl_Menu_Bar      *app_menu_bar       = nullptr;

Fl_Tile          *app_tile           = nullptr;
Fl_Text_Editor   *app_editor         = nullptr;
Fl_Text_Buffer   *app_text_buffer    = nullptr;

Fl_Text_Display  *app_console        = nullptr;
Fl_Text_Buffer   *app_console_buffer = nullptr;

class ListenerInput;
ListenerInput    *app_listener       = nullptr;

bool  text_changed = false;
char  app_filename[FL_PATH_MAX] = "";

// Musil environment
AtomPtr musil_env;

// font size for editor/console/listener
int g_font_size = 16;

// Style buffer for syntax highlighting
Fl_Text_Buffer *app_style_buffer = nullptr;

// Dynamic keyword + vars infrastructure
std::vector<std::string> g_builtin_keywords;  // static core keywords
std::vector<std::string> g_env_symbols;       // symbols from (info 'vars)
std::vector<AtomType> g_env_kinds;
std::vector<std::string> g_browser_symbols;

// Right-side browser showing current variables
Fl_Select_Browser *app_var_browser = nullptr;

Fl_Preferences g_prefs(Fl_Preferences::USER,
                       "carminecella",   // vendor / org
                       "musil_ide");     // app name

std::vector<std::string> g_listener_history;
int g_listener_history_pos = -1; // index into history, or history.size() when "at end"

int g_paren_pos1 = -1;
int g_paren_pos2 = -1;

Fl_Window    *g_find_win      = nullptr;
Fl_Input     *g_find_input    = nullptr;
Fl_Input     *g_replace_input = nullptr;
bool          g_find_case     = false;

// -----------------------------------------------------------------------------
// Small helper to capture std::cout output
// -----------------------------------------------------------------------------

struct CoutRedirect {
    std::streambuf* old_buf;
    std::ostringstream capture;

    CoutRedirect() {
        old_buf = std::cout.rdbuf(capture.rdbuf());
    }
    ~CoutRedirect() {
        std::cout.rdbuf(old_buf);
    }
    std::string str() const { return capture.str(); }
};

// -----------------------------------------------------------------------------
// Title + filename helpers
// -----------------------------------------------------------------------------

void update_title() {
    const char *fname = nullptr;
    if (app_filename[0])
        fname = fl_filename_name(app_filename);

    if (fname) {
        char buf[FL_PATH_MAX + 3];
        buf[FL_PATH_MAX + 2] = '\0';
        if (text_changed) {
            snprintf(buf, FL_PATH_MAX + 2, "%s *", fname);
        } else {
            snprintf(buf, FL_PATH_MAX + 2, "%s", fname);
        }
        app_window->copy_label(buf);
    } else {
        // std::stringstream title;
        // title <<"musil, ver. " << VERSION;
        app_window->label("Musil IDE");
    }
}

void set_changed(bool v) {
    if (v != text_changed) {
        text_changed = v;
        update_title();
    }
}

void set_filename(const char *new_filename) {
    if (new_filename) {
        fl_strlcpy(app_filename, new_filename, FL_PATH_MAX);
    } else {
        app_filename[0] = 0;
    }
    update_title();
}

// -----------------------------------------------------------------------------
// Console helpers
// -----------------------------------------------------------------------------

void console_append(const std::string &s) {
    if (!app_console_buffer) return;
    app_console_buffer->append(s.c_str());
    app_console->insert_position(app_console_buffer->length());
    app_console->show_insert_position();
    app_console->redraw();
}

void console_clear() {
    if (!app_console_buffer) return;
    app_console_buffer->text("");
    app_console->insert_position(0);
    app_console->show_insert_position();
    app_console->redraw();
}

// -----------------------------------------------------------------------------
// Editor change callback
// -----------------------------------------------------------------------------

void text_changed_callback(int, int n_inserted, int n_deleted,
                           int, const char*, void*) {
    if (n_inserted || n_deleted)
        set_changed(true);
}

// -----------------------------------------------------------------------------
// Forward declarations for menu callbacks
// -----------------------------------------------------------------------------

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

void menu_run_script_callback(Fl_Widget*, void*);
void menu_run_selection_callback(Fl_Widget*, void*);

void menu_zoom_in_callback(Fl_Widget*, void*);
void menu_zoom_out_callback(Fl_Widget*, void*);
void menu_syntaxhighlight_callback(Fl_Widget*, void*);

// Listener eval helper
void listener_eval_line();

// -----------------------------------------------------------------------------
// Musil environment initialization
// -----------------------------------------------------------------------------

void init_base_keywords();
void update_keywords_from_env_and_browser();
void init_musil_env() {
    musil_env = make_env();
    
    load_env_paths (musil_env);

    for (unsigned i = 0; i < musil_env->paths.size (); ++i) {
        std::cout << musil_env->paths.at (i) << std::endl;
    }
    std::stringstream out;
    out << "[musil, version " << VERSION <<"]" << std::endl <<  std::endl;
    out << "music scripting language" << std::endl;
    out << "(c) " << COPYRIGHT << ", www.carminecella.com" << std::endl << std::endl;

    console_append(out.str ().c_str ());

    // Initialize builtin keywords once and pull current vars
    init_base_keywords();
    update_keywords_from_env_and_browser();
}


// -----------------------------------------------------------------------------
// File-related helpers
// -----------------------------------------------------------------------------

static std::string get_resources_dir() {
#ifdef __APPLE__
    // Try to detect macOS bundle: MyApp.app/Contents/MacOS/MyApp
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        fs::path exe(buf);
        fs::path macos_dir = exe.parent_path();          // .../Contents/MacOS
        fs::path contents  = macos_dir.parent_path();    // .../Contents
        fs::path resources = contents / "Resources";     // .../Contents/Resources
        if (fs::exists(resources)) {
            return resources.string();
        }
    }
    // fallback: ./Resources relative to current dir
    return (fs::current_path() / "Resources").string();

#elif defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len > 0) {
        fs::path exe(buf);
        fs::path resources = exe.parent_path() / "Resources";
        if (fs::exists(resources)) {
            return resources.string();
        }
    }
    return (fs::current_path() / "Resources").string();

#else
    // Linux / other: ./Resources
    return (fs::current_path() / "Resources").string();
#endif
}

void load_file_into_editor(const char *filename) {
    if (app_text_buffer->loadfile(filename) == 0) {
        set_filename(filename);
        set_changed(false);
    } else {
        fl_alert("Failed to load file\n%s\n%s",
                 filename,
                 strerror(errno));
    }
}

static bool install_musil_libraries() {
    std::string src_dir_str = get_resources_dir();
    fs::path src_dir(src_dir_str);

    if (!fs::exists(src_dir) || !fs::is_directory(src_dir)) {
        fl_alert("Resources directory not found:\n%s", src_dir_str.c_str());
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
        if (p.extension() == ".scm") {
            fs::path dest = dest_dir / p.filename();
            std::error_code copy_ec;
            fs::copy_file(p, dest, fs::copy_options::overwrite_existing, copy_ec);
            if (!copy_ec) {
                ++copied;
            }
        }
    }

    if (ec) {
        fl_alert("Error while scanning Resources directory:\n%s", src_dir_str.c_str());
        return false;
    }

    if (copied == 0) {
        fl_alert("No .scm files found in Resources directory:\n%s", src_dir_str.c_str());
        return false;
    }

    fl_message("Installed %d .scm file(s) into:\n%s",
               copied, dest_dir.string().c_str());
    return true;
}

void menu_quit_callback(Fl_Widget *, void *) {
    if (text_changed) {
        int r = fl_choice("The current file has not been saved.\n"
                          "Would you like to save it now?",
                          "Cancel", "Save", "Don't Save");
        if (r == 0)   // cancel
            return;
        if (r == 1) { // save
            menu_save_as_callback(nullptr, nullptr);
            return;
        }
        // r == 2 -> don't save
    }

    // --- save window geometry here ---
    if (app_window) {
        g_prefs.set("win_x", app_window->x());
        g_prefs.set("win_y", app_window->y());
        g_prefs.set("win_w", app_window->w());
        g_prefs.set("win_h", app_window->h());
        g_prefs.flush();   // ensure it’s written to disk
    }

    Fl::hide_all_windows();
}

void menu_new_callback(Fl_Widget*, void*) {
    if (text_changed) {
        int c = fl_choice("Changes in your text have not been saved.\n"
                          "Do you want to start a new text anyway?",
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
        if (r == 0) // cancel
            return;
        if (r == 1) // save
            menu_save_as_callback(nullptr, nullptr);
    }

    Fl_Native_File_Chooser file_chooser;
    file_chooser.title("Open File...");
    file_chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
    if (app_filename[0]) {
        char temp_filename[FL_PATH_MAX];
        fl_strlcpy(temp_filename, app_filename, FL_PATH_MAX);
        const char *name = fl_filename_name(temp_filename);
        if (name) {
            file_chooser.preset_file(name);
            temp_filename[name - temp_filename] = 0;
            file_chooser.directory(temp_filename);
        }
    }
    if (file_chooser.show() == 0) {
        load_file_into_editor(file_chooser.filename());
    }
}

void menu_save_as_callback(Fl_Widget*, void*) {
    Fl_Native_File_Chooser file_chooser;
    file_chooser.title("Save File As...");
    file_chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    if (app_filename[0]) {
        char temp_filename[FL_PATH_MAX];
        fl_strlcpy(temp_filename, app_filename, FL_PATH_MAX);
        const char *name = fl_filename_name(temp_filename);
        if (name) {
            file_chooser.preset_file(name);
            temp_filename[name - temp_filename] = 0;
            file_chooser.directory(temp_filename);
        }
    }
    if (file_chooser.show() == 0) {
        if (app_text_buffer->savefile(file_chooser.filename()) == 0) {
            set_filename(file_chooser.filename());
            set_changed(false);
        } else {
            fl_alert("Failed to save file\n%s\n%s",
                     file_chooser.filename(),
                     strerror(errno));
        }
    }
}

void menu_save_callback(Fl_Widget*, void*) {
    if (!app_filename[0]) {
        menu_save_as_callback(nullptr, nullptr);
    } else {
        if (app_text_buffer->savefile(app_filename) == 0) {
            set_changed(false);
        } else {
            fl_alert("Failed to save file\n%s\n%s",
                     app_filename,
                     strerror(errno));
        }
    }
}

// -----------------------------------------------------------------------------
// Edit callbacks
// -----------------------------------------------------------------------------

void menu_undo_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor)
        Fl_Text_Editor::kf_undo(0, (Fl_Text_Editor*)e);
}

void menu_redo_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor)
        Fl_Text_Editor::kf_redo(0, (Fl_Text_Editor*)e);
}

void menu_cut_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor)
        Fl_Text_Editor::kf_cut(0, (Fl_Text_Editor*)e);
}

void menu_copy_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor)
        Fl_Text_Editor::kf_copy(0, (Fl_Text_Editor*)e);
}

void menu_paste_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor)
        Fl_Text_Editor::kf_paste(0, (Fl_Text_Editor*)e);
}

void menu_delete_callback(Fl_Widget*, void*) {
    Fl_Widget *e = Fl::focus();
    if (e && e == app_editor)
        Fl_Text_Editor::kf_delete(0, (Fl_Text_Editor*)e);
}

void create_find_dialog();
int editor_find_next(bool);
void editor_replace_one();
void  editor_replace_all();

void menu_find_dialog_callback(Fl_Widget*, void*) {
    create_find_dialog();
}

void menu_find_next_callback(Fl_Widget*, void*) {
    editor_find_next(false);
}

void menu_replace_one_callback(Fl_Widget*, void*) {
    editor_replace_one();
}

void menu_replace_all_callback(Fl_Widget*, void*) {
    editor_replace_all();
}

// -----------------------------------------------------------------------------
// Evaluate (Musil) helpers
// -----------------------------------------------------------------------------

void eval_string_in_musil(const std::string &code, bool is_script = false) {
    CoutRedirect redirect;
    try {
        std::istringstream in(code);
        unsigned linenum = 0;

        while (true) {
            AtomPtr expr = read(in, linenum);
            if (!expr && in.eof()) break;
            if (!expr) continue;

            AtomPtr res = eval(expr, musil_env);

            if (!is_script) {
                // REPL-like: print result
                std::ostringstream oss;
                print(res, oss);
                oss << "\n";
                std::cout << oss.str(); // captured
            }
        }
    } catch (AtomPtr &e) {
        std::ostringstream oss;
        oss << "error: ";
        print(e, oss);
        oss << "\n";
        std::cout << oss.str();
    } catch (std::exception &e) {
        std::cout << "exception: " << e.what() << "\n";
    } catch (...) {
        std::cout << "fatal unknown error\n";
    }
    std::string out = redirect.str();
    if (!out.empty())
        console_append(out);

    // Environment may have changed (new defs, sets, etc.)
    update_keywords_from_env_and_browser();
}

void menu_run_script_callback(Fl_Widget*, void*) {
    console_append("[Run script]\n");

    char *text = app_text_buffer->text();
    if (!text) {
        console_append("(empty buffer)\n\n");
        return;
    }

    std::string code(text);
    free(text);

    eval_string_in_musil(code, true);
    console_append("\n");
}

void menu_run_selection_callback(Fl_Widget*, void*) {
    int start, end;
    if (!app_text_buffer->selection_position(&start, &end)) {
        console_append("[Run selection] no selection; running entire script.\n");
        menu_run_script_callback(nullptr, nullptr);
        return;
    }

    char *sel = app_text_buffer->text_range(start, end);
    if (!sel) {
        console_append("[Run selection] selection empty.\n\n");
        return;
    }

    std::string code(sel);
    free(sel);

    console_append("[Run selection]\n");
    eval_string_in_musil(code, true);
    console_append("\n");
}
void menu_clear_env_callback(Fl_Widget*, void*) {
    // int r = fl_choice("You are about to reset the current environment.\n"
    //                     "Continue?",
    //                     "Yes", "No", "");
    // if (r == 0)   // yes
        console_clear ();
        init_musil_env ();
    // if (r == 1) { // no
    //     return;
    // }
}

void build_paths_dialog();
void menu_paths_callback(Fl_Widget*, void*) {
    build_paths_dialog();
}

void menu_install_libraries_callback(Fl_Widget*, void*) {
    int r = fl_choice(
        "Install Musil libraries?\n\n"
        "This will copy all *.scm files from the Resources folder\n"
        "into your ~/.musil directory.",
        "No", "Yes", nullptr
    );

    if (r == 1) { // "Yes"
        install_musil_libraries();
    }
}

// -----------------------------------------------------------------------------
// Listener input widget
// -----------------------------------------------------------------------------

void update_paren_match() {
    if (!app_text_buffer || !app_style_buffer || !app_editor) return;

    int len = app_text_buffer->length();
    if (len <= 0) {
        g_paren_pos1 = g_paren_pos2 = -1;
        return;
    }

    auto restore_style_at = [](int pos) {
        if (pos < 0) return;
        if (pos >= app_style_buffer->length()) return;
        char buf[2] = {'E', '\0'};   // back to normal paren style
        app_style_buffer->replace(pos, pos + 1, buf);
    };

    restore_style_at(g_paren_pos1);
    restore_style_at(g_paren_pos2);
    g_paren_pos1 = g_paren_pos2 = -1;

    char* text = app_text_buffer->text();
    if (!text) return;

    int cursor = app_editor->insert_position();
    if (cursor < 0 || cursor > len) {
        free(text);
        return;
    }

    int paren_pos = -1;
    if (cursor > 0 &&
        (text[cursor - 1] == '(' || text[cursor - 1] == ')' ||
         text[cursor - 1] == '{' || text[cursor - 1] == '}')) {
        paren_pos = cursor - 1;
    } else if (cursor < len &&
               (text[cursor] == '(' || text[cursor] == ')' ||
                text[cursor] == '{' || text[cursor] == '}')) {
        paren_pos = cursor;
    }

    if (paren_pos < 0) {
        free(text);
        app_editor->redisplay_range(0, len);
        return;
    }

    char c = text[paren_pos];
    if (c != '(' && c != ')' && c != '{' && c != '}') {
        free(text);
        app_editor->redisplay_range(0, len);
        return;
    }

    int match_pos = -1;

    auto match_forward = [&](char open_c, char close_c, int start) {
        int depth = 1;
        for (int i = start; i < len; ++i) {
            if (text[i] == open_c) depth++;
            else if (text[i] == close_c) depth--;
            if (depth == 0) return i;
        }
        return -1;
    };

    auto match_backward = [&](char open_c, char close_c, int start) {
        int depth = 1;
        for (int i = start; i >= 0; --i) {
            if (text[i] == close_c) depth++;
            else if (text[i] == open_c) depth--;
            if (depth == 0) return i;
        }
        return -1;
    };

    if (c == '(') {
        match_pos = match_forward('(', ')', paren_pos + 1);
    } else if (c == ')') {
        match_pos = match_backward('(', ')', paren_pos - 1);
    } else if (c == '{') {
        match_pos = match_forward('{', '}', paren_pos + 1);
    } else if (c == '}') {
        match_pos = match_backward('{', '}', paren_pos - 1);
    }

    if (match_pos < 0) {
        free(text);
        app_editor->redisplay_range(0, len);
        return;
    }

    auto set_match_style = [](int pos) {
        if (pos < 0) return;
        if (pos >= app_style_buffer->length()) return;
        char buf[2] = {'F', '\0'}; // match style
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

class ListenerInput : public Fl_Input {
public:
    ListenerInput(int X, int Y, int W, int H, const char* L = 0)
        : Fl_Input(X, Y, W, H, L) {}

    int handle(int ev) override {
        if (ev == FL_KEYDOWN) {
            int key = Fl::event_key();

            // ENTER → evaluate the line
            if (key == FL_Enter) {
                listener_eval_line();
                return 1;
            }

            // ↑ (history up)
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

            // ↓ (history down)
            if (key == FL_Down) {
                if (!g_listener_history.empty()) {
                    if (g_listener_history_pos < 0)
                        g_listener_history_pos = (int)g_listener_history.size();

                    if (g_listener_history_pos < (int)g_listener_history.size() - 1) {
                        g_listener_history_pos++;
                        value(g_listener_history[g_listener_history_pos].c_str());
                    } else {
                        // Past last entry → empty line
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
void listener_eval_line() {
    if (!app_listener) return;

    std::string line = app_listener->value();
    if (line.empty()) {
        app_listener->value("");
        return;
    }

    // Add to history unless duplicate of last
    if (g_listener_history.empty() || g_listener_history.back() != line) {
        g_listener_history.push_back(line);
    }
    g_listener_history_pos = (int)g_listener_history.size();

    // Echo to console
    console_append(">> " + line + "\n");

    // Evaluate using Musil interpreter
    eval_string_in_musil(line);

    // Print a separating newline
    console_append("\n");

    // Clear the input
    app_listener->value("");
}

std::vector<std::string> autocomplete_candidates(const std::string& prefix) {
    std::vector<std::string> res;
    if (prefix.empty()) return res;

    auto add_if_match = [&](const std::string& s) {
        if (s.size() >= prefix.size() &&
            s.compare(0, prefix.size(), prefix) == 0) {
            res.push_back(s);
        }
    };

    for (const auto& k : g_builtin_keywords) add_if_match(k);
    for (const auto& k : g_env_symbols)      add_if_match(k);

    std::sort(res.begin(), res.end());
    res.erase(std::unique(res.begin(), res.end()), res.end());
    return res;
}

bool is_ident_char(char c);
class MusilEditor : public Fl_Text_Editor {
public:
    MusilEditor(int X, int Y, int W, int H, const char* L = 0)
        : Fl_Text_Editor(X, Y, W, H, L) {}

    void do_autocomplete() {
        int pos = insert_position();
        if (pos <= 0) return;

        // Get word start
        int start = pos - 1;
        char *txt = app_text_buffer->text();
        if (!txt) return;
        while (start >= 0 && is_ident_char(txt[start])) {
            --start;
        }
        start++; // first ident char
        if (start >= pos) {
            free(txt);
            return;
        }

        std::string prefix(txt + start, txt + pos);
        free(txt);

        auto cand = autocomplete_candidates(prefix);
        if (cand.empty()) return;

        if (cand.size() == 1) {
            // Single match → inline complete
            const std::string& full = cand[0];
            if (full.size() > prefix.size()) {
                std::string extra = full.substr(prefix.size());
                app_text_buffer->insert(pos, extra.c_str());
                insert_position(pos + (int)extra.size());
            }
            return;
        }

        // Multiple candidates → popup menu at caret
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
        Fl_Menu_Item terminator = { nullptr, 0, 0, 0, 0, 0, 0, 0, 0 };
        items.push_back(terminator);

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
                // Ctrl+Tab? If you prefer Ctrl+Space, see below
                do_autocomplete();
                return 1;
            }
            if (Fl::event_key() == ' ' && (Fl::event_state() & FL_CTRL)) {
                // Ctrl+Space = autocomplete
                do_autocomplete();
                return 1;
            }
        }

        // paren/brace match logic
        if (ev == FL_KEYDOWN || ev == FL_KEYUP ||
            ev == FL_FOCUS   || ev == FL_UNFOCUS ||
            ev == FL_PUSH    || ev == FL_DRAG   ||
            ev == FL_RELEASE) {
            update_paren_match();
        }

        return ret;
    }
};


// -----------------------------------------------------------------------------
// Musil-oriented syntax highlighting
// -----------------------------------------------------------------------------

// Styles:
//  A - Plain
//  B - Comment   ( ; ... end-of-line )
//  C - String    ( "..." )
//  D - Keyword   (def, lambda, if, ...)
//  E - Paren     (all parentheses)
//  F - Match     (matching pair of parens under cursor)

Fl_Text_Display::Style_Table_Entry styletable[] = {
    { FL_BLACK,      FL_SCREEN,      16 }, // A - plain
    { FL_DARK_GREEN, FL_SCREEN, 16 }, // B - comments
    { FL_BLUE,       FL_SCREEN,      16 }, // C - strings
    { FL_DARK_RED,   FL_SCREEN_BOLD, 16 }, // D - keywords
    { FL_DARK_BLUE,  FL_SCREEN_BOLD, 16 }, // E - parens
    { FL_RED,        FL_SCREEN_BOLD, 16 }  // F - matching parens
}; 
const int N_STYLES = sizeof(styletable) / sizeof(styletable[0]);

// Builtin keywords (static); we’ll copy these into g_builtin_keywords once
static const char* musil_builtin_keywords[] = {
    "%schedule",
    "*",
    "+",
    "-",
    "/",
    "<",
    "<=",
    "<>",
    "=",
    "==",
    ">",
    ">=",
    "E",
    "LOG2",
    "SQRT2",
    "TWOPI",
    "abs",
    "acos",
    "ack",
    "addpaths",
    "and",
    "apply",
    "array",
    "array2list",
    "asin",
    "assign",
    "atan",
    "begin",
    "break",
    "car",
    "cdr",
    "clearpaths",
    "clock",
    "comp",
    "compare",
    "cos",
    "cosh",
    "def",
    "diff",
    "dirlist",
    "dot",
    "dup",
    "elem",
    "eq",
    "eval",
    "exec",
    "exit",
    "fac",
    "fib",
    "filter",
    "filestat",
    "flip",
    "floor",
    "foldl",
    "fourth",
    "function",
    "getval",
    "getvar",
    "if",
    "info",
    "lambda",
    "lappend",
    "lhead",
    "lindex",
    "length",
    "let",
    "list",
    "llast",
    "llength",
    "lrange",
    "lreplace",
    "lreverse",
    "lset",
    "lshuffle",
    "lsplit",
    "ltail",
    "ltake",
    "ldrop",
    "load",
    "log",
    "log10",
    "macro",
    "map",
    "map2",
    "match",
    "max",
    "mean",
    "min",
    "mod",
    "neg",
    "normal",
    "not",
    "or",
    "ortho",
    "pred",
    "print",
    "quotient",
    "read",
    "remainder",
    "round",
    "save",
    "schedule",
    "second",
    "setval",
    "sign",
    "sin",
    "sinh",
    "size",
    "slice",
    "sleep",
    "sqrt",
    "square",
    "standard",
    "stddev",
    "str",
    "sum",
    "succ",
    "tan",
    "tanh",
    "third",
    "tostr",
    "twice",
    "udprecv",
    "udpsend",
    "unless",
    "when",
    "while",
    "zip"
};

const int N_BUILTIN_KEYWORDS =
    sizeof(musil_builtin_keywords) / sizeof(musil_builtin_keywords[0]);

    // Initialize builtin keyword vector once
void init_base_keywords() {
    if (!g_builtin_keywords.empty()) return; // already done
    g_builtin_keywords.reserve(N_BUILTIN_KEYWORDS);
    for (int i = 0; i < N_BUILTIN_KEYWORDS; ++i) {
        g_builtin_keywords.emplace_back(musil_builtin_keywords[i]);
    }
}

void update_keywords_from_env_and_browser() {
    if (!musil_env) return;

    g_env_symbols.clear();
    g_env_kinds.clear();
    g_browser_symbols.clear();

    struct Named {
        std::string name;
        AtomType    kind;
    };

    std::vector<Named> ops;
    std::vector<Named> lambdas;
    std::vector<Named> others;

    // Collect symbols from top-level env
    for (unsigned i = 1; i < musil_env->tail.size(); ++i) {
        AtomPtr binding = musil_env->tail.at(i);
        if (is_nil(binding) || binding->tail.size() < 2) continue;

        AtomPtr sym = binding->tail.at(0);
        AtomPtr val = binding->tail.at(1);
        if (is_nil(sym) || sym->type != SYMBOL) continue;

        Named n{ sym->lexeme, val->type };

        switch (val->type) {
            case OP:
                ops.push_back(n);
                break;
            case LAMBDA:
            case MACRO:
                lambdas.push_back(n);
                break;
            default:
                others.push_back(n);
                break;
        }
    }

    auto by_name = [](const Named& a, const Named& b) {
        return a.name < b.name;
    };
    std::sort(ops.begin(),     ops.end(),     by_name);
    std::sort(lambdas.begin(), lambdas.end(), by_name);
    std::sort(others.begin(),  others.end(),  by_name);

    if (app_var_browser) {
        app_var_browser->clear();
        app_var_browser->format_char('@'); // enable FLTK markup

        // Header: gray background, black text, non-clickable
        auto add_header = [&](const char* title) {
            std::ostringstream oss;
            // @Bnnn = background color, @Cnnn = text color
            oss << "@B" << (int)FL_GRAY << "@C" << (int)FL_BLACK
                << " " << title;
            app_var_browser->add(oss.str().c_str());
            g_browser_symbols.push_back(""); // header → non-clickable
        };

        // Symbol entries, colored by kind (clickable)
        auto add_symbol = [&](const Named& n) {
            Fl_Color color;
            switch (n.kind) {
                case OP:                 color = FL_DARK_BLUE; break;
                case LAMBDA:
                case MACRO:              color = FL_BLUE;      break;
                default:                 color = FL_DARK_GREEN;       break;
            }

            std::ostringstream oss;
            oss << "@C" << (int)color << "  " << n.name;
            app_var_browser->add(oss.str().c_str());
            g_browser_symbols.push_back(n.name);

            // also add for syntax highlighting
            g_env_symbols.push_back(n.name);
            g_env_kinds.push_back(n.kind);
        };

        // Order: Others → Lambdas & Macros → Operators
        if (!others.empty()) {
            add_header("Data/lists");
            for (const auto& n : others) add_symbol(n);
        }
        if (!lambdas.empty()) {
            add_header("Lambdas/Macros");
            for (const auto& n : lambdas) add_symbol(n);
        }
        if (!ops.empty()) {
            add_header("Operators");
            for (const auto& n : ops) add_symbol(n);
        }

        app_var_browser->redraw();
    }
}

bool is_ident_start(char c) {
    return std::isalpha((unsigned char)c) || c == '_' || c == '!';
}

bool is_ident_char(char c) {
    // allow typical Scheme-ish chars in symbols
    return std::isalnum((unsigned char)c) ||
           c == '_' || c == '!' || c == '?' ||
           c == '-' || c == '+' || c == '*' ||
           c == '/' || c == '<' || c == '>' || c == '=';
}

bool is_keyword(const std::string& s) {
    // Builtins
    for (const auto &k : g_builtin_keywords) {
        if (s == k) return true;
    }
    // Vars from env
    for (const auto &k : g_env_symbols) {
        if (s == k) return true;
    }
    return false;
}

// Very simple Musil lexer -> style buffer
void style_parse_musil(const char* text, char* style, int length) {
    bool in_comment = false;
    bool in_string  = false;

    int i = 0;
    while (i < length) {
        char c = text[i];

        if (in_comment) {
            style[i] = 'B';
            if (c == '\n') {
                in_comment = false;
            }
            ++i;
            continue;
        }

        if (in_string) {
            style[i] = 'C';
            if (c == '"' && (i == 0 || text[i-1] != '\\')) {
                in_string = false;
            }
            ++i;
            continue;
        }

        // Not in comment/string:
        if (c == ';') {
            // start comment until end of line
            in_comment = true;
            style[i] = 'B';
            ++i;
            continue;
        }

        if (c == '"') {
            in_string = true;
            style[i] = 'C';
            ++i;
            continue;
        }

        if (c == '(' || c == ')') {
            style[i] = 'E';
            ++i;
            continue;
        }

        if (c == '{' || c == '}') {
            style[i] = 'E';
            ++i;
            continue;
        }
        if (is_ident_start(c)) {
            int start = i;
            int j = i + 1;
            while (j < length && is_ident_char(text[j])) j++;
            std::string ident(text + start, text + j);
            char mode = is_keyword(ident) ? 'D' : 'A';
            for (int k = start; k < j; ++k) {
                style[k] = mode;
            }
            i = j;
            continue;
        }

        // default
        style[i] = 'A';
        ++i;
    }
}

// Style initialization: create style buffer for entire text
void style_init() {
    int len = app_text_buffer->length();
    if (len < 0) len = 0;

    char* text  = app_text_buffer->text();
    char* style = new char[len + 1];

    if (!text) {
        std::memset(style, 'A', len);
    } else {
        style_parse_musil(text, style, len);
    }
    style[len] = '\0';

    if (!app_style_buffer) {
        app_style_buffer = new Fl_Text_Buffer(len);
    }
    app_style_buffer->text(style);

    delete [] style;
    if (text) free(text);

    update_paren_match();
}

// Simple style_update: re-parse entire buffer when text changes
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
        app_editor->highlight_data(app_style_buffer, styletable,
                                   N_STYLES,
                                   'A', nullptr, 0);
        app_text_buffer->add_modify_callback(style_update, app_editor);
    } else {
        app_text_buffer->remove_modify_callback(style_update, app_editor);
        app_editor->highlight_data(nullptr, nullptr, 0, 'A', nullptr, 0);
    }
    app_editor->redraw();
}

void menu_clear_console_callback(Fl_Widget*, void*) {
    console_clear ();
}

// -----------------------------------------------------------------------------
// Zoom helpers
// -----------------------------------------------------------------------------

void apply_font_size() {
    for (int i = 0; i < N_STYLES; ++i) {
        styletable[i].size = g_font_size;
    }

    if (app_editor) {
        app_editor->textsize(g_font_size);
        app_editor->redraw();
    }
    if (app_console) {
        app_console->textsize(g_font_size);
        app_console->redraw();
    }
    if (app_listener) {
        app_listener->textsize(g_font_size);
        app_listener->redraw();
    }
    if (app_var_browser) {
        app_var_browser->textsize(g_font_size);
        app_var_browser->redraw();
    }
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


// -----------------------------------------------------------------------------
// Path helpers
// -----------------------------------------------------------------------------

struct PathsDialog {
    Fl_Double_Window *win  = nullptr;
    Fl_Select_Browser *list = nullptr;
};

static void paths_add_cb(Fl_Widget *w, void *userdata) {
    PathsDialog *dlg = static_cast<PathsDialog*>(userdata);
    if (!dlg || !dlg->list) return;

    Fl_Native_File_Chooser ch;
    ch.title("Select folder to add to search paths");
    ch.type(Fl_Native_File_Chooser::BROWSE_DIRECTORY);

    if (ch.show() == 0) { // user chose a directory
        const char *dir = ch.filename();
        if (dir && *dir) {
            std::string s = dir;
            // avoid duplicates
            auto it = std::find(musil_env->paths.begin(), musil_env->paths.end(), s);
            if (it == musil_env->paths.end()) {
                musil_env->paths.push_back(s);
                dlg->list->add(s.c_str());
            }
        }
    }
}

static void paths_remove_cb(Fl_Widget *w, void *userdata) {
    PathsDialog *dlg = static_cast<PathsDialog*>(userdata);
    if (!dlg || !dlg->list) return;

    int idx = dlg->list->value(); // 1-based index of selected line
    if (idx <= 0) return;

    dlg->list->remove(idx);

    if (idx - 1 >= 0 && idx - 1 < static_cast<int>(musil_env->paths.size())) {
        musil_env->paths.erase(musil_env->paths.begin() + (idx - 1));
    }
}

static void paths_close_cb(Fl_Widget *w, void *userdata) {
    PathsDialog *dlg = static_cast<PathsDialog*>(userdata);
    if (!dlg || !dlg->win) return;

    // Save current list to preferences and reload it
    save_env_paths (musil_env);
    load_env_paths (musil_env);

    dlg->win->hide();
}

// -----------------------------------------------------------------------------
// Building UI
// -----------------------------------------------------------------------------

void var_browser_cb(Fl_Widget *w, void *) {
    if (!musil_env) return;
    auto *browser = static_cast<Fl_Select_Browser*>(w);
    int line = browser->value();
    if (line <= 0) return;

    if (line > (int)g_browser_symbols.size()) return;
    const std::string& name = g_browser_symbols[line - 1];

    // Headers / empty lines are non-clickable
    if (name.empty()) return;

    std::string code = name;
    console_append(">> " + code + "\n");
    eval_string_in_musil(code);
    console_append("\n");
}

void create_find_dialog() {
    if (g_find_win) { g_find_win->show(); return; }

    g_find_win = new Fl_Window(320, 130, "Find / Replace");
    g_find_input    = new Fl_Input(80, 10, 230, 25, "Find:");
    g_replace_input = new Fl_Input(80, 40, 230, 25, "Replace:");

    Fl_Button* btn_find_next = new Fl_Button(10, 80, 90, 25, "Find next");
    Fl_Button* btn_replace   = new Fl_Button(110, 80, 90, 25, "Replace");
    Fl_Button* btn_all       = new Fl_Button(210, 80, 90, 25, "Replace all");

    btn_find_next->callback([](Fl_Widget*, void*) {
        // simulate "find next"
        extern void menu_find_next_callback(Fl_Widget*, void*);
        menu_find_next_callback(nullptr, nullptr);
    });

    btn_replace->callback([](Fl_Widget*, void*) {
        extern void menu_replace_one_callback(Fl_Widget*, void*);
        menu_replace_one_callback(nullptr, nullptr);
    });

    btn_all->callback([](Fl_Widget*, void*) {
        extern void menu_replace_all_callback(Fl_Widget*, void*);
        menu_replace_all_callback(nullptr, nullptr);
    });

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
    int has_sel = app_text_buffer->selection_position(&start, &end);

    if (has_sel && end > start) {
        // Replace current selection
        app_text_buffer->replace_selection(repl ? repl : "");
    } else {
        // No selection → find next, then replace that
        if (editor_find_next(false) >= 0) {
            app_text_buffer->replace_selection(repl ? repl : "");
        }
    }
}

void editor_replace_all() {
    if (!app_text_buffer || !g_find_input || !g_replace_input) return;
    const char* needle = g_find_input->value();
    const char* repl   = g_replace_input->value();
    if (!needle || !*needle) return;

    int pos = 0;
    int len = (int)strlen(needle);
    int count = 0;

    app_editor->insert_position(0);

    while (app_text_buffer->search_forward(pos, needle, &pos)) {
        app_text_buffer->select(pos, pos + len);
        app_text_buffer->replace_selection(repl ? repl : "");
        pos += (int)strlen(repl ? repl : "");
        ++count;
    }
}


void build_paths_dialog() {
    PathsDialog *dlg = new PathsDialog;

    int W = 500;
    int H = 320;

    dlg->win = new Fl_Double_Window(W, H, "Environment Paths");
    dlg->win->set_modal();

    dlg->list = new Fl_Select_Browser(10, 10, W - 20, H - 70);
    dlg->list->when(FL_WHEN_RELEASE); // fire callback on mouse up

    for (const auto &p : musil_env->paths){
        dlg->list->add(p.c_str());
    }

    // Optional: no auto-selection; user explicitly clicks
    // If you prefer a default, you can uncomment:
    if (!musil_env->paths.empty()) dlg->list->select(1);

    // Callback to ensure click updates the current selection
    dlg->list->callback(
        [](Fl_Widget *w, void *data) {
            auto browser = static_cast<Fl_Select_Browser*>(w);
            // Fl_Select_Browser already manages selection internally,
            // but this makes sure we always have a valid "current" line.
            int line = browser->value();
            if (line > 0) {
                browser->select(line); // ensure it's visibly selected
            }
        },
        dlg
    );

    Fl_Button *btn_add    = new Fl_Button(10,      H - 50, 80, 30, "Add...");
    Fl_Button *btn_remove = new Fl_Button(100,     H - 50, 80, 30, "Remove");
    Fl_Button *btn_close  = new Fl_Button(W - 90,  H - 50, 80, 30, "Close");

    btn_add->callback(paths_add_cb, dlg);
    btn_remove->callback(paths_remove_cb, dlg);
    btn_close->callback(paths_close_cb, dlg);

    dlg->win->end();
    dlg->win->show();
}

void build_app_window() {
    // std::stringstream title;
    // title <<"musil, ver. " << VERSION;
    app_window = new Fl_Double_Window(800, 600, "Musil IDE");
}

void build_app_menu_bar() {
    app_window->begin();
    
    #ifdef __APPLE__
    // Use system menu bar on macOS
    app_menu_bar = new Fl_Sys_Menu_Bar(0, 0, app_window->w(), 25);
#else
    // Use regular menu bar on other platforms
    app_menu_bar = new Fl_Menu_Bar(0, 0, app_window->w(), 25);
#endif

    // File
    app_menu_bar->add("File/New",         FL_COMMAND + 'n', menu_new_callback);
    app_menu_bar->add("File/Open...",     FL_COMMAND + 'o', menu_open_callback);
    app_menu_bar->add("File/Save",        FL_COMMAND + 's', menu_save_callback);
    app_menu_bar->add("File/Save as...",  FL_COMMAND + 'S', menu_save_as_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("File/Quit",        FL_COMMAND + 'q', menu_quit_callback);

    // Edit
    app_menu_bar->add("Edit/Undo",   FL_COMMAND + 'z', menu_undo_callback);
    app_menu_bar->add("Edit/Redo",   FL_COMMAND + 'Z', menu_redo_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("Edit/Cut",    FL_COMMAND + 'x', menu_cut_callback);
    app_menu_bar->add("Edit/Copy",   FL_COMMAND + 'c', menu_copy_callback);
    app_menu_bar->add("Edit/Paste",  FL_COMMAND + 'v', menu_paste_callback);
    app_menu_bar->add("Edit/Delete", 0,                menu_delete_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("Edit/Find...",       FL_COMMAND + 'f', menu_find_dialog_callback);
    app_menu_bar->add("Edit/Find next",     FL_COMMAND + 'g', menu_find_next_callback);
    app_menu_bar->add("Edit/Replace...",    FL_COMMAND + 'h', menu_find_dialog_callback);


    // Evaluate
    app_menu_bar->add("Evaluate/Run script",     FL_COMMAND + 'r', menu_run_script_callback);
    app_menu_bar->add("Evaluate/Run selection", FL_COMMAND + 'e', menu_run_selection_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("Evaluate/Reset environment", FL_COMMAND + 'j', menu_clear_env_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("Evaluate/Paths...",   0, menu_paths_callback);
    app_menu_bar->add("Evaluate/Install libraries...", 0, menu_install_libraries_callback);

    // View
    app_menu_bar->add("View/Zoom in",           FL_COMMAND + '+', menu_zoom_in_callback);
    app_menu_bar->add("View/Zoom out",          FL_COMMAND + '-', menu_zoom_out_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("View/Clear donsole", FL_COMMAND + 'k', menu_clear_console_callback);
    // app_menu_bar->add("View/Syntax highlighting", 0, menu_syntaxhighlight_callback, nullptr, FL_MENU_TOGGLE);

    app_window->callback(menu_quit_callback);
    app_window->end();
}
void build_main_editor_console_listener() {
    int win_w  = app_window->w();
    int win_h  = app_window->h();

    int menu_h = 0;
#ifdef __APPLE__
    // Native system menu bar; it does not live inside the window
    menu_h = 0;
#else
    menu_h = app_menu_bar ? app_menu_bar->h() : 0;
#endif

    int toolbar_h = 28; // height for the top toolbar

    app_window->begin();

    // -----------------------------------------------------------------
    // Text buffer for editor
    // -----------------------------------------------------------------
    app_text_buffer = new Fl_Text_Buffer();
    app_text_buffer->add_modify_callback(text_changed_callback, nullptr);

    // -----------------------------------------------------------------
    // Toolbar (Run, Run selection, Reset env, Clear console) with icons
    // -----------------------------------------------------------------
    Fl_Group* toolbar = new Fl_Group(0, menu_h, win_w, toolbar_h);
    toolbar->box(FL_FLAT_BOX);
    Fl_Color toolbar_col = fl_rgb_color(230, 230, 230);
    toolbar->color(toolbar_col);

    int bx = 6;
    int by = menu_h + 4;
    int bw = 26;            // small square-ish buttons
    int bh = toolbar_h - 8;

    // Common helper for icon buttons
    auto make_icon_button = [&](const char* sym, const char* tip,
                                Fl_Callback* cb) -> Fl_Button* {
        Fl_Button* b = new Fl_Button(bx, by, bw, bh);
        b->box(FL_FLAT_BOX);
        b->color(toolbar_col);
        b->selection_color(fl_color_average(FL_BLACK, toolbar_col, 0.1)); // subtle pressed color
        b->label(sym);
        b->labeltype(FL_SYMBOL_LABEL);
        b->labelcolor(FL_DARK3); // gray icon
        b->callback(cb);
        if (tip) b->tooltip(tip);
        bx += bw + 4;
        return b;
    };

    // Play / run script
    make_icon_button("@>", "Run script", menu_run_script_callback);

    // Play selection (double arrow)
    make_icon_button("@<->", "Run selection", menu_run_selection_callback);

    // Reset env (we'll use a circular-ish symbol)
    make_icon_button("@reload", "Reset environment", menu_clear_env_callback);
    // If '@reload' is not recognized by your FLTK version, fall back to "@circle"

    // Clear console (use an 'X'-like symbol)
    make_icon_button("X", "Clear console", menu_clear_console_callback);

    toolbar->end();

    // -----------------------------------------------------------------
    // Main tile: editor (top) + listener+console (bottom)
    // -----------------------------------------------------------------
    int tile_y  = menu_h + toolbar_h;
    int tile_h  = win_h - tile_y;
    int right_panel_w = 200;

    app_tile = new Fl_Tile(0, tile_y, win_w - right_panel_w, tile_h);

    // Editor: top portion of the tile
    int editor_h = (app_tile->h() * 3) / 5; // give more space to editor

    app_editor = new MusilEditor(app_tile->x(), app_tile->y(),
                                 app_tile->w(), editor_h);
    app_editor->buffer(app_text_buffer);
    app_editor->textfont(FL_SCREEN);
    app_editor->textsize(g_font_size);
    app_editor->linenumber_width(50);
    app_tile->add(app_editor);

    // Bottom group: listener + console
    int bottom_y  = app_editor->y() + app_editor->h();
    int bottom_h  = app_tile->h() - app_editor->h();
    Fl_Group *bottom_group = new Fl_Group(app_tile->x(), bottom_y,
                                          app_tile->w(), bottom_h);

    int bg_x = bottom_group->x();
    int bg_y = bottom_group->y();
    int bg_w = bottom_group->w();
    int bg_h = bottom_group->h();

    int listener_h = 26;

    // Listener input (REPL-style)
    app_listener = new ListenerInput(bg_x, bg_y, bg_w, listener_h);
    app_listener->textfont(FL_SCREEN);
    app_listener->textsize(g_font_size);

    // Console display
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

    // Make the editor the resizable part within the tile
    app_tile->resizable(app_editor);
    app_tile->end();

    // -----------------------------------------------------------------
    // Right-side variables browser
    // -----------------------------------------------------------------
    int right_x = app_tile->x() + app_tile->w();
    int right_y = tile_y;
    int right_w = win_w - app_tile->w();
    int right_h = win_h - right_y;

    app_var_browser = new Fl_Select_Browser(right_x, right_y, right_w, right_h, "Vars");
    app_var_browser->textfont(FL_SCREEN);
    app_var_browser->textsize(8);
    app_var_browser->when(FL_WHEN_RELEASE);
    app_var_browser->callback(var_browser_cb);

    // -----------------------------------------------------------------
    // Window resizable region
    // -----------------------------------------------------------------
    app_window->resizable(app_tile);
    app_window->end();
    app_tile->init_sizes();
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char **argv) {
    try {
        Fl::scheme("oxy");   // set global scheme
        // Fl::set_color(FL_BACKGROUND_COLOR,  30,  30,  30);   // Dark gray
        // Fl::set_color(FL_BACKGROUND2_COLOR, 45,  45,  48);   // Slightly lighter
        // Fl::set_color(FL_FOREGROUND_COLOR, 212, 212, 212);   // Light text
        // Fl::set_color(FL_SELECTION_COLOR,   14,  99, 156);   // Blue selection        

        Fl::args_to_utf8(argc, argv);

        build_app_window();
        build_app_menu_bar();
        build_main_editor_console_listener();

        // Load initial file from command line if provided
        if (argc > 1 && argv[1] && argv[1][0] != '-') {
            load_file_into_editor(argv[1]);
        }

        // Default position and size
        int win_x = 100;
        int win_y = 100;
        int win_w = 640;
        int win_h = 480;

        // Try to load previous values, falling back to current ones
        g_prefs.get("win_x", win_x, win_x);
        g_prefs.get("win_y", win_y, win_y);
        g_prefs.get("win_w", win_w, win_w);
        g_prefs.get("win_h", win_h, win_h);

        // Apply to the window
        app_window->resize(win_x, win_y, win_w, win_h);
        app_window->show(argc, argv);

        // Initialize Musil environment after UI is ready
        init_musil_env();

        // Turn on syntax highlighting by default
        {
            // mark menu item checked
            // Fl_Menu_Item* item = const_cast<Fl_Menu_Item*>(
            //     app_menu_bar->find_item("View/Syntax Highlighting"));
            // if (item) item->set();
            style_init();
            app_editor->highlight_data(app_style_buffer, styletable,
                                       N_STYLES,
                                       'A', nullptr, 0);
            app_text_buffer->add_modify_callback(style_update, app_editor);
        }

        // Apply initial font size to all widgets/styles
        apply_font_size();

        return Fl::run();
    } catch (std::exception &e) {
        fl_alert("Fatal error: %s", e.what());
        return 1;
    } catch (...) {
        fl_alert("Fatal unknown error");
        return 1;
    }
}

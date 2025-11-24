// musil_ide.cpp
//
// Musil IDE with FLTK:
//  - Top: text editor for Musil scripts
//  - Middle: single-line "listener" input (REPL)
//  - Bottom: console text display for evaluation output
//  - Draggable splitter between editor and bottom pane (listener+console)
//  - Musil-oriented syntax highlighting (comments, strings, parens, keywords)
//  - Zoom in/out (View/Zoom In, View/Zoom Out)
//  - Evaluate/Run Script (Cmd+R) and Evaluate/Run Selection (Cmd+E)
//
// Requires: FLTK, musil.h in ../src
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

#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cctype>

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

Fl_Preferences g_prefs(Fl_Preferences::USER,
                       "carminecella",   // vendor / org
                       "musil_ide");     // app name

std::vector<std::string> g_search_paths;

int g_paren_pos1 = -1;
int g_paren_pos2 = -1;

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

void load_env_paths_from_prefs() {
    g_search_paths.clear();

    Fl_Preferences paths_group(g_prefs, "paths"); // child group "paths"

    int count = 0;
    paths_group.get("count", count, 0);

    char buf[FL_PATH_MAX];
    for (int i = 0; i < count; ++i) {
        char key[32];
        std::snprintf(key, sizeof(key), "path%d", i);
        buf[0] = '\0';
        if (paths_group.get(key, buf, "", FL_PATH_MAX) && buf[0] != '\0') {
            g_search_paths.emplace_back(buf);
        }
    }
}

void save_env_paths_to_prefs() {
    Fl_Preferences paths_group(g_prefs, "paths");

    // remove old entries
    int old_count = 0;
    paths_group.get("count", old_count, 0);
    for (int i = 0; i < old_count; ++i) {
        char key[32];
        std::snprintf(key, sizeof(key), "path%d", i);
        paths_group.deleteEntry(key);
    }

    int count = static_cast<int>(g_search_paths.size());
    paths_group.set("count", count);

    for (int i = 0; i < count; ++i) {
        char key[32];
        std::snprintf(key, sizeof(key), "path%d", i);
        paths_group.set(key, g_search_paths[i].c_str());
    }
}


void init_musil_env() {
    musil_env = make_env();
    
    // Load search paths from preferences and add them to the environment
    load_env_paths_from_prefs();
    for (const std::string &p : g_search_paths) {
        // assuming Env type behind musil_env has: std::vector<std::string> paths;
        musil_env->paths.push_back(p);
        // If you have a helper like add_path(musil_env, p), call that instead.
    }    

    std::stringstream out;
    out << "[musil, version " << VERSION <<"]" << std::endl <<  std::endl;
    out << "music scripting language" << std::endl;
    out << "(c) " << COPYRIGHT << ", www.carminecella.com" << std::endl << std::endl;

    console_append(out.str ().c_str ());
}

// -----------------------------------------------------------------------------
// File-related helpers
// -----------------------------------------------------------------------------

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

    // 1. Revert previous matches (F -> E for parens)
    auto restore_style_at = [](int pos) {
        if (pos < 0) return;
        if (pos >= app_style_buffer->length()) return;
        char buf[2] = {'E', '\0'};
        app_style_buffer->replace(pos, pos + 1, buf);
    };

    restore_style_at(g_paren_pos1);
    restore_style_at(g_paren_pos2);
    g_paren_pos1 = g_paren_pos2 = -1;

    // 2. Get text and cursor position
    char* text = app_text_buffer->text();
    if (!text) return;

    int cursor = app_editor->insert_position();
    if (cursor < 0 || cursor > len) {
        free(text);
        return;
    }

    // Look at character before cursor, or at cursor if needed
    int paren_pos = -1;
    if (cursor > 0 && (text[cursor - 1] == '(' || text[cursor - 1] == ')')) {
        paren_pos = cursor - 1;
    } else if (cursor < len && (text[cursor] == '(' || text[cursor] == ')')) {
        paren_pos = cursor;
    }

    if (paren_pos < 0) {
        free(text);
        app_editor->redisplay_range(0, len);
        return;
    }

    char c = text[paren_pos];
    if (c != '(' && c != ')') {
        free(text);
        app_editor->redisplay_range(0, len);
        return;
    }

    // 3. Find matching paren
    int match_pos = -1;
    if (c == '(') {
        int depth = 1;
        for (int i = paren_pos + 1; i < len; ++i) {
            if (text[i] == '(') depth++;
            else if (text[i] == ')') depth--;
            if (depth == 0) {
                match_pos = i;
                break;
            }
        }
    } else { // c == ')'
        int depth = 1;
        for (int i = paren_pos - 1; i >= 0; --i) {
            if (text[i] == ')') depth++;
            else if (text[i] == '(') depth--;
            if (depth == 0) {
                match_pos = i;
                break;
            }
        }
    }

    if (match_pos < 0) {
        free(text);
        app_editor->redisplay_range(0, len);
        return;
    }

    // 4. Highlight both positions with style 'F'
    auto set_match_style = [](int pos) {
        if (pos < 0) return;
        if (pos >= app_style_buffer->length()) return;
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

class ListenerInput : public Fl_Input {
public:
    ListenerInput(int X, int Y, int W, int H, const char* L = 0)
        : Fl_Input(X, Y, W, H, L) {}

    int handle(int ev) override {
        if (ev == FL_KEYDOWN && Fl::event_key() == FL_Enter) {
            listener_eval_line();
            return 1; // we handled it
        }
        return Fl_Input::handle(ev);
    }
};

class MusilEditor : public Fl_Text_Editor {
public:
    MusilEditor(int X, int Y, int W, int H, const char* L = 0)
        : Fl_Text_Editor(X, Y, W, H, L) {}

    int handle(int ev) override {
        int ret = Fl_Text_Editor::handle(ev);
        // After key / mouse navigation, update paren match
        if (ev == FL_KEYDOWN || ev == FL_KEYUP ||
            ev == FL_FOCUS   || ev == FL_UNFOCUS ||
            ev == FL_PUSH    || ev == FL_DRAG   ||
            ev == FL_RELEASE) {
            update_paren_match();
        }
        return ret;
    }
};

void listener_eval_line() {
    if (!app_listener) return;
    const char* text = app_listener->value();
    std::string line = text ? text : "";
    if (line.empty()) return;

    app_listener->value("");
    console_append(">> " + line + "\n");
    eval_string_in_musil(line);
    console_append("\n");
}

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

// Musil-ish keywords (extend as you like)
const char* musil_keywords[] = {
    "%",
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

const int N_KEYWORDS = sizeof(musil_keywords) / sizeof(musil_keywords[0]);

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
    for (int i = 0; i < N_KEYWORDS; ++i) {
        if (s == musil_keywords[i]) return true;
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

// Forward declaration
void save_env_paths_to_prefs();

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
            auto it = std::find(g_search_paths.begin(), g_search_paths.end(), s);
            if (it == g_search_paths.end()) {
                g_search_paths.push_back(s);
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

    if (idx - 1 >= 0 && idx - 1 < static_cast<int>(g_search_paths.size())) {
        g_search_paths.erase(g_search_paths.begin() + (idx - 1));
    }
}

static void paths_close_cb(Fl_Widget *w, void *userdata) {
    PathsDialog *dlg = static_cast<PathsDialog*>(userdata);
    if (!dlg || !dlg->win) return;

    // Save current list to preferences and reload it
    save_env_paths_to_prefs();
    load_env_paths_from_prefs();

    dlg->win->hide();
}

// -----------------------------------------------------------------------------
// Building UI
// -----------------------------------------------------------------------------

void build_paths_dialog() {
    PathsDialog *dlg = new PathsDialog;

    int W = 500;
    int H = 320;

    dlg->win = new Fl_Double_Window(W, H, "Environment Paths");
    dlg->win->set_modal();

    dlg->list = new Fl_Select_Browser(10, 10, W - 20, H - 70);
    dlg->list->when(FL_WHEN_RELEASE); // fire callback on mouse up

    for (const auto &p : g_search_paths) {
        dlg->list->add(p.c_str());
    }

    // Optional: no auto-selection; user explicitly clicks
    // If you prefer a default, you can uncomment:
    // if (!g_search_paths.empty()) dlg->list->select(1);

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
    app_menu_bar = new Fl_Menu_Bar(0, 0, app_window->w(), 25);

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
    app_menu_bar->add("Edit/Delete", 0,                menu_delete_callback);

    // Evaluate
    app_menu_bar->add("Evaluate/Run script",     FL_COMMAND + 'r', menu_run_script_callback);
    app_menu_bar->add("Evaluate/Run selection", FL_COMMAND + 'e', menu_run_selection_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("Evaluate/Reset environment", FL_COMMAND + 'j', menu_clear_env_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("Evaluate/Paths...",   0, menu_paths_callback);

    // View
    app_menu_bar->add("View/Zoom in",           FL_COMMAND + '+', menu_zoom_in_callback);
    app_menu_bar->add("View/Zoom out",          FL_COMMAND + '-', menu_zoom_out_callback, nullptr, FL_MENU_DIVIDER);
    app_menu_bar->add("View/Clear donsole", FL_COMMAND + 'k', menu_clear_console_callback);
    // app_menu_bar->add("View/Syntax highlighting", 0, menu_syntaxhighlight_callback, nullptr, FL_MENU_TOGGLE);

    app_window->callback(menu_quit_callback);
    app_window->end();
}

void build_main_editor_console_listener() {
    int menu_h = app_menu_bar->h();
    int win_w  = app_window->w();
    int win_h  = app_window->h();

    app_window->begin();

    // Text buffer
    app_text_buffer = new Fl_Text_Buffer();
    app_text_buffer->add_modify_callback(text_changed_callback, nullptr);

    // Tile: splits editor (top) and bottom pane (listener+console)
    app_tile = new Fl_Tile(0, menu_h, win_w, win_h - menu_h);

    // Editor: top half (initial)
    int editor_h = (app_tile->h() * 3) / 5; // more space for editor
    // app_editor = new Fl_Text_Editor(app_tile->x(), app_tile->y(),
    //                                 app_tile->w(), editor_h);
    // 
    app_editor = new MusilEditor(app_tile->x(), app_tile->y(),
                             app_tile->w(), editor_h);

    app_editor->buffer(app_text_buffer);
    app_editor->textfont(FL_SCREEN);
    app_editor->textsize(g_font_size);
    app_editor->cursor_style (Fl_Text_Display::HEAVY_CURSOR);
    app_editor->linenumber_width (50);
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

    // Make tile resizable & set ranges so split behaves
    app_tile->resizable(app_editor);
    app_tile->size_range(0, 50, 50); // min height for editor
    app_tile->size_range(1, 50, 50); // min height for bottom group
    app_tile->end();

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
            Fl_Menu_Item* item = const_cast<Fl_Menu_Item*>(
                app_menu_bar->find_item("View/Syntax Highlighting"));
            if (item) item->set();
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

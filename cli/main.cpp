// main.cpp — musil command-line interface.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
//   musil                              REPL
//   musil a.mu b.mu ...                run files in order (one interpreter)
//   musil a.mu -- x y z                everything after -- is visible as the list `args`
//   musil -i a.mu                      run, then enter the REPL
//   musil -e "print 42"                evaluate a string (before any files)
//   musil --version | --help

#include "musil.h"
#include <fstream>
#include <sstream>

static void usage(std::ostream& o) {
    o << "usage: musil [options] [file.mu ...] [-- args...]\n"
         "  -i           enter the REPL after running the files\n"
         "  -e CODE      evaluate CODE before running the files\n"
         "  --stack N    maximum evaluation depth (default 10000)\n"
         "  --version    print version and exit\n"
         "  --help       this text\n"
         "Files are loaded in order into one interpreter. Arguments after -- are\n"
         "passed to the program as the list `args`.\n";
}

static bool run_file(musil::Interp& I, const std::string& file) {
    std::ifstream f(file);
    if (!f) { std::cerr << "cannot open " << file << "\n"; return false; }
    std::stringstream ss; ss << f.rdbuf();
    I.run(ss.str(), file);
    return true;
}

int main(int argc, char** argv) {
    musil::Interp I;
    musil::make_env(I);

    bool interactive = false;
    std::vector<std::string> files, code, rest;
    for (int k = 1; k < argc; ++k) {
        std::string a = argv[k];
        if (a == "--") { for (++k; k < argc; ++k) rest.push_back(argv[k]); break; }
        if (a == "-i") interactive = true;
        else if (a == "-e" && k + 1 < argc) code.push_back(argv[++k]);
        else if (a == "--stack" && k + 1 < argc) I.max_stack = std::atoi(argv[++k]);
        else if (a == "--version") { std::cout << "musil " << MUSIL_VERSION << "\n"; return 0; }
        else if (a == "--help" || a == "-h") { usage(std::cout); return 0; }
        else if (a.size() > 1 && a[0] == '-') { 
            std::cerr << "unknown option " << a << "\n"; usage(std::cerr); return 2; 
        }
        else files.push_back(a);
    }
    musil::vlist args; for (auto& s : rest) args.push_back(musil::v_str(s));
    I.def("args", musil::v_list(std::move(args)));

    try {
        for (auto& c : code) I.run(c, "<cmdline>");
        for (auto& f : files) if (!run_file(I, f)) return 1;
    }
    catch (musil::Exit_signal& e) { return e.code; }
    catch (const std::exception& e) { std::cerr << "error: " << e.what() << "\n"; return 1; }

    if (files.empty() && code.empty()) {
        std::cout << "[musil " << MUSIL_VERSION << "]\n"
                  << "scripting language for sound and music computing\n"
                  << "(c) 2026 Carmine-Emanuele Cella\n\n";
        interactive = true;
    }
    if (interactive) {
        try { I.repl(); } catch (musil::Exit_signal& e) { return e.code; }
    }
    return 0;
}

// eof


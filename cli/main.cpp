// main.cpp — musil command-line interface.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
//   musil                      REPL
//   musil file.mu [args...]    run a file; args are visible as the list `args`
//   musil -i file.mu           run a file, then enter the REPL
//   musil -e "print 42"        evaluate a string
//   musil --version | --help

#include "musil.h"
#include <fstream>
#include <sstream>

static void usage(std::ostream& o) {
    o << "usage: musil [options] [file.mu] [args...]\n"
         "  -i           enter the REPL after running the file\n"
         "  -e CODE      evaluate CODE and exit\n"
         "  --stack N    maximum evaluation depth (default 10000)\n"
         "  --version    print version and exit\n"
         "  --help       this text\n";
}

int main(int argc, char** argv) {
    musil::interp I;
    musil::add_all(I);
    // Installed layout: <prefix>/bin/musil and <prefix>/share/musil/lang
    std::error_code ec;
    auto exe = std::filesystem::weakly_canonical(std::filesystem::absolute(argv[0], ec), ec);
    if (!ec) I.load_path.push_back((exe.parent_path().parent_path() / "share" / "musil" / "lang").string());

    bool interactive = false;
    std::string file, code;
    std::vector<std::string> rest;
    for (int k = 1; k < argc; ++k) {
        std::string a = argv[k];
        if (!file.empty()) { rest.push_back(a); continue; }
        if (a == "-i") interactive = true;
        else if (a == "-e" && k + 1 < argc) code = argv[++k];
        else if (a == "--stack" && k + 1 < argc) I.max_stack = std::atoi(argv[++k]);
        else if (a == "--version") { std::cout << "musil " << MUSIL_VERSION << "\n"; return 0; }
        else if (a == "--help" || a == "-h") { usage(std::cout); return 0; }
        else if (!a.empty() && a[0] == '-') { std::cerr << "unknown option " << a << "\n"; usage(std::cerr); return 2; }
        else file = a;
    }
    musil::vlist args; for (auto& s : rest) args.push_back(musil::v_str(s));
    I.def("args", musil::v_list(std::move(args)));

    try {
        if (!code.empty()) { I.run(code, "<cmdline>"); }
        if (!file.empty()) {
            std::ifstream f(file);
            if (!f) { std::cerr << "cannot open " << file << "\n"; return 1; }
            std::stringstream ss; ss << f.rdbuf();
            I.run(ss.str(), file);
        }
    }
    catch (musil::exit_signal& e) { return e.code; }
    catch (const std::exception& e) { std::cerr << "error: " << e.what() << "\n"; return 1; }

    if (file.empty() && code.empty()) {
        std::cout << "[musil " << MUSIL_VERSION << "]\n"
                  << "scripting language for sound and music computing\n"
                  << "(c) 2026 Carmine-Emanuele Cella\n\n";
        interactive = true;
    }
    if (interactive) {
        try { I.repl(); } catch (musil::exit_signal& e) { return e.code; }
    }
    return 0;
}

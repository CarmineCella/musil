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

#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
// --send PORT [file]: send code to a running Musil (the IDE, or musil --serve) and print the reply
static int send_to_port(int port, const std::string& code) {
    int sock = socket(AF_INET, SOCK_STREAM, 0); if (sock < 0) { std::cerr << "cannot open a socket\n"; return 1; }
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons((uint16_t)port); addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(sock, (sockaddr*)&addr, sizeof addr) < 0) { std::cerr << "nothing listens on port " << port << " (start the IDE, or musil --serve " << port << " -i)\n"; ::close(sock); return 1; }
    std::string msg = code + "\n\x04";
    if (write(sock, msg.data(), msg.size()) < 0) { ::close(sock); return 1; }
    shutdown(sock, SHUT_WR);
    char buf[4096]; long n; std::string reply;
    while ((n = read(sock, buf, sizeof buf)) > 0) reply.append(buf, (size_t)n);
    ::close(sock); std::cout << reply; return reply.rfind("error:", 0) == 0 ? 1 : 0;
}
#endif
static void usage(std::ostream& o) {
    o << "usage: musil [options] [file.mu ...] [-- args...]\n"
         "  -i           enter the REPL after running the files\n"
         "  -e CODE      evaluate CODE before running the files\n"
         "  --stack N    maximum evaluation depth (default 100000)\n"
        "  --serve PORT open the evaluation port: editors send code to localhost:PORT (with -i to stay alive)\n"
        "  --send PORT [file]  send a file (or stdin) to a running Musil on that port and print its reply\n"
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
#ifndef _WIN32
        else if (a == "--send" && k + 1 < argc) {
            int port = std::atoi(argv[++k]); std::string code;
            if (k + 1 < argc && argv[k + 1][0] != '-') { std::ifstream f(argv[++k]); if (!f) { std::cerr << "cannot open " << argv[k] << "\n"; return 1; } std::stringstream ss; ss << f.rdbuf(); code = ss.str(); }
            else { std::stringstream ss; ss << std::cin.rdbuf(); code = ss.str(); }
            return send_to_port(port, code);
        }
#endif
        else if (a == "--serve" && k + 1 < argc) { int port = std::atoi(argv[++k]); try { musil::serve_start(I, port); std::cerr << "evaluation port " << port << " open\n"; } catch (std::exception& e) { std::cerr << e.what() << "\n"; return 1; } }
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
    catch (musil::Exit_signal& e) { musil::live_shutdown(); return e.code; }
    catch (const std::exception& e) { std::cerr << "error: " << e.what() << "\n"; musil::live_shutdown(); return 1; }

    if (files.empty() && code.empty()) {
        std::cout << "[musil " << MUSIL_VERSION << "]\n\n"
                  << "scripting language for sound and music computing\n"
                  << "(c) 2026 Carmine-Emanuele Cella\n\n";
        interactive = true;
    }
    if (interactive) {
        try { I.repl(); } catch (musil::Exit_signal& e) { musil::live_shutdown(); return e.code; }
    }
#ifdef MUSIL_HAS_FLTK
    // a script that opened plot or controls windows: keep them (and the loops, the port) alive until they are closed
    while (musil::plot_windows_open() > 0 || musil::controls_window_open()) { I.idle(); Fl::wait(0.05); }
#endif
    musil::live_shutdown();
    return 0;
}

// eof


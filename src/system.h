// system.h — operating-system access, C++ half: processes, environment,
// directories, file metadata, CSV and WAV files, UDP/OSC. The Musil half
// is system.mu. Ported from Musil 1.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.

#pragma once
#include "core.h"
#include "system/wav_tools.h"
#include "system/csv_tools.h"
#include <cstdio>
#include <cstring>
#include <thread>
#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace musil {

// --- processes and environment ---
// (exec "command") => the command's standard output as a string
inline vptr sys_exec(vlist& a, Interp& i) {
    const std::string& cmd = i.str(a[0]);
    FILE* p = popen(cmd.c_str(), "r"); if (!p) i.bad("cannot run " + cmd);
    std::string out; char buf[256]; while (fgets(buf, sizeof(buf), p)) out += buf;
    pclose(p); return v_str(std::move(out));
}
// (getenv "NAME") => value or nil
inline vptr sys_getenv(vlist& a, Interp& i) { const char* v = std::getenv(i.str(a[0]).c_str()); return v ? v_str(v) : v_nil(); }
// (sleep seconds)
// (find-file "name") => the path of a file as load resolves it (the current directory, the file being run, the
//   load path: MUSIL_PATH, ~/.musil, the libraries), or "" when not found
inline vptr sys_find_file(vlist& a, Interp& i) { return v_str(i.find_file(i.str(a[0]))); }
// (resolve-path "name") => the path a reading function would open for a relative name: as given if it exists from the
//   current directory, else next to the file being run, else next to the program that was started
inline vptr sys_resolve_path(vlist& a, Interp& i) { return v_str(i.read_path(i.str(a[0]))); }
// (interactive?) => is the standard input a terminal? (a script asks before prompting for input)
inline vptr sys_interactive(vlist&, Interp&) {
#ifndef _WIN32
    return v_bool(isatty(0));
#else
    return v_bool(true);
#endif
}
// (breathe) let the background work run once (windows, schedulers, the score player): for a long computation
//   done from a window's button, once per step
inline vptr sys_breathe(vlist&, Interp& i) { if (i.yield_fn) i.yield_fn(); i.idle(); return v_nil(); }
inline vptr sys_sleep(vlist& a, Interp& i) {   // in slices, so a stop request and background work (live loops) get through
    double secs = i.scalar(a[0]); auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(secs);
    while (true) {
        auto now = std::chrono::steady_clock::now(); if (now >= end) break;
        std::this_thread::sleep_for(std::min(std::chrono::duration<double>(0.005), std::chrono::duration<double>(end - now)));
        if (i.yield_fn) i.yield_fn();
        i.idle();
    }
    return v_nil();
}
// (now) => seconds since the Unix epoch (wall clock; clock is the monotonic one)
inline vptr sys_now(vlist&, Interp&) { return v_num(std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count()); }

// --- directories and files ---
// (cwd) => current directory
inline vptr sys_cwd(vlist&, Interp&) { return v_str(fs::current_path().string()); }
// (ls [dir]) => list of entry names, sorted
inline vptr sys_ls(vlist& a, Interp& i) {
    fs::path d = a.empty() ? fs::current_path() : fs::path(i.str(a[0]));
    std::error_code ec; std::vector<std::string> names;
    for (auto& e : fs::directory_iterator(d, ec)) names.push_back(e.path().filename().string());
    if (ec) i.bad("cannot list " + d.string());
    std::sort(names.begin(), names.end());
    vlist out; for (auto& n : names) out.push_back(v_str(n));
    return v_list(std::move(out));
}
// (mkdir path) => creates the directory and its parents; nil
inline vptr sys_mkdir(vlist& a, Interp& i) { std::error_code ec; fs::create_directories(i.str(a[0]), ec); if (ec) i.bad("cannot create " + i.str(a[0])); return v_nil(); }
// (remove path) => deletes a file or an empty directory; 1 if something was removed
inline vptr sys_remove(vlist& a, Interp& i) { std::error_code ec; bool r = fs::remove(i.str(a[0]), ec); if (ec) i.bad("cannot remove " + i.str(a[0])); return v_bool(r); }
// (stat path) => (list size is-dir modified-seconds) or nil if it does not exist
inline vptr sys_stat(vlist& a, Interp& i) {
    fs::path p(i.read_path(i.str(a[0]))); std::error_code ec;
    if (!fs::exists(p, ec)) return v_nil();
    bool dir = fs::is_directory(p, ec);
    double size = dir ? 0.0 : (double)fs::file_size(p, ec);
    // file_clock's epoch is implementation-defined in C++17: re-base it on the system clock.
    auto t = fs::last_write_time(p, ec);
    auto sys = std::chrono::system_clock::now() + (t - fs::file_time_type::clock::now());
    double secs = std::chrono::duration<double>(sys.time_since_epoch()).count();
    return v_list({ v_num(size), v_bool(dir), v_num(secs) });
}

// --- CSV: a table is a list of rows, a row a list of cells; numeric cells become numbers ---
// (read-csv path) => table. write-csv is in system.mu.
inline vptr sys_read_csv(vlist& a, Interp& i) {
    std::string path = i.read_path(i.str(a[0]));
    std::ifstream f(path); if (!f) i.bad("cannot open " + path);
    vlist rows;
    for (auto& row : readCSV(f)) {
        vlist r;
        for (auto& cell : row) {
            char* e = nullptr; double x = std::strtod(cell.c_str(), &e);
            r.push_back(!cell.empty() && e && *e == '\0' ? v_num(x) : v_str(cell));
        }
        rows.push_back(v_list(std::move(r)));
    }
    return v_list(std::move(rows));
}
// --- WAV ---
// (read-wav path) => (list sample-rate (list channel-vector ...)): 8, 16, 24 and 32-bit PCM or 32/64-bit float, any channels and rate
inline vptr sys_read_wav(vlist& a, Interp& i) {
    WAVHeader h{}; std::vector<std::vector<double>> chans;
    try { chans = read_wav_raw(i.read_path(i.str(a[0])).c_str(), h); } catch (std::exception& e) { i.bad(e.what()); }
    vlist cl;
    for (auto& c : chans) { varr v(c.size()); for (size_t k = 0; k < c.size(); k++) v[k] = c[k]; cl.push_back(v_arr(std::move(v))); }
    return v_list({ v_num((double)h.sampleRate), v_list(std::move(cl)) });
}
// (write-wav path sample-rate channels [bits]) => nil. channels: a vector (mono) or a list of vectors (any number);
//   bits 8, 16 (default), 24 or 32 for integer PCM, "float" (or 32.0 with a decimal point: use "float") for 32-bit IEEE
//   float, "double" for 64-bit; any sample rate. Integer files clip at +-1.
inline vptr sys_write_wav(vlist& a, Interp& i) {
    double sr = i.scalar(a[1]); if (sr <= 0) i.bad("sample rate must be > 0");
    int bits = 16, fmt = 1;
    if (a.size() > 3) {
        if (a[3]->t == Value::STR || a[3]->t == Value::SYM) { std::string w = a[3]->s; if (w == "float") { bits = 32; fmt = 3; } else if (w == "double") { bits = 64; fmt = 3; } else i.bad("bits: 8, 16, 24, 32, \"float\" or \"double\""); }
        else { bits = (int)i.scalar(a[3]); if (bits != 8 && bits != 16 && bits != 24 && bits != 32) i.bad("bits: 8, 16, 24, 32, \"float\" or \"double\""); }
    }
    std::vector<std::vector<double>> chans;
    auto add = [&](const vptr& v) { const varr& n = i.num(v); chans.emplace_back(std::begin(n), std::end(n)); };
    if (a[2]->t == Value::NUM) add(a[2]);
    else for (auto& c : i.list(a[2])) add(c);
    if (chans.empty()) i.bad("no channels");
    for (auto& c : chans) if (c.size() != chans[0].size()) i.bad("all channels must have the same length");
    WAVHeader h{}; h.audioFormat = (uint16_t)fmt; h.numChannels = (uint16_t)chans.size(); h.sampleRate = (uint32_t)sr; h.bitsPerSample = (uint16_t)bits;
    try { write_wav_raw(i.str(a[0]).c_str(), chans, h); } catch (std::exception& e) { i.bad(e.what()); }
    return v_nil();
}

// --- UDP and OSC (POSIX) ---
#ifndef _WIN32
// (udp-send host port payload [osc?]) => 1 on success, 0 on failure. With osc?=1 the payload is
// sent as a minimal OSC message (address only, padded, empty type tag).
inline vptr sys_udp_send(vlist& a, Interp& i) {
    std::string host = i.str(a[0]); int port = (int)i.scalar(a[1]);
    std::string payload = str_of(a[2]); bool osc = a.size() > 3 && truthy(a[3]);
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); if (sock < 0) return v_bool(false);
    sockaddr_in srv{}; srv.sin_family = AF_INET; srv.sin_port = htons((uint16_t)port); srv.sin_addr.s_addr = inet_addr(host.c_str());
    std::vector<char> buf(payload.begin(), payload.end());
    if (osc) { size_t n = payload.size(), pad = ((n + 4) & ~size_t(3)) - n; buf.resize(n + pad + 4, '\0'); buf[n + pad] = ','; }
    long r = sendto(sock, buf.data(), buf.size(), 0, (sockaddr*)&srv, sizeof(srv));
    ::close(sock); return v_bool(r >= 0);
}
// (udp-receive host port [timeout-seconds]) => the next datagram as a string, or nil on timeout/failure
inline vptr sys_udp_receive(vlist& a, Interp& i) {
    std::string host = i.str(a[0]); int port = (int)i.scalar(a[1]);
    double timeout = a.size() > 2 ? i.scalar(a[2]) : -1;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); if (sock < 0) return v_nil();
    if (timeout >= 0) {   // tv_sec/tv_usec have platform-specific types: assign, don't brace-initialise
        timeval tv{};
        tv.tv_sec  = static_cast<decltype(tv.tv_sec)>(timeout);
        tv.tv_usec = static_cast<decltype(tv.tv_usec)>((timeout - static_cast<double>(tv.tv_sec)) * 1e6);
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    sockaddr_in srv{}; srv.sin_family = AF_INET; srv.sin_port = htons((uint16_t)port); srv.sin_addr.s_addr = inet_addr(host.c_str());
    if (::bind(sock, (sockaddr*)&srv, sizeof(srv)) < 0) { ::close(sock); return v_nil(); }
    char msg[65536]; sockaddr_in cli{}; socklen_t cl = sizeof(cli);
    long n = recvfrom(sock, msg, sizeof(msg), 0, (sockaddr*)&cli, &cl);
    ::close(sock);
    if (n < 0) return v_nil();
    return v_str(std::string(msg, (size_t)n));
}
#endif


// --- the evaluation port: a local TCP socket that evaluates the text it receives -------------------
// Editors send code here (the VS Code extension, or `musil --send`): a message is the text up to the
// end of the connection (or a line holding only \x04), it is evaluated in the global environment as if
// typed in the console, its printed output goes where the host's output goes, and a one-line reply
// ("ok", the value, or "error: ...") is written back. The server is polled from the interpreter's
// idle hook, so it works in the CLI (`musil --serve 7770 -i`), during a running script, and in the
// IDE. Only local connections (127.0.0.1) are accepted.


struct eval_server {
    int sock = -1, port = 0;
    struct client { int fd; std::string buf; };
    std::vector<client> clients;
    std::function<void(const std::string&)> on_receive;   // hosts may show what arrived (the IDE's console)
};
inline eval_server& server() { static eval_server s; return s; }

#ifndef _WIN32
inline void serve_start(Interp& i, int port) {
    eval_server& s = server();
    if (s.sock >= 0) { ::close(s.sock); s.sock = -1; }
    s.sock = socket(AF_INET, SOCK_STREAM, 0); if (s.sock < 0) i.bad("cannot open a socket");
    int yes = 1; setsockopt(s.sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons((uint16_t)port); addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(s.sock, (sockaddr*)&addr, sizeof addr) < 0 || listen(s.sock, 8) < 0) { ::close(s.sock); s.sock = -1; i.bad("cannot listen on port " + std::to_string(port)); }
    fcntl(s.sock, F_SETFL, O_NONBLOCK); s.port = port;
}
inline void serve_stop() { eval_server& s = server(); for (auto& c : s.clients) ::close(c.fd); s.clients.clear(); if (s.sock >= 0) ::close(s.sock); s.sock = -1; s.port = 0; }
inline void serve_evaluate(Interp& i, int fd, std::string code) {
    while (!code.empty() && (code.back() == '\x04' || code.back() == '\n' || code.back() == '\r')) code.pop_back();
    if (server().on_receive) server().on_receive(code);
    std::string reply;
    try { vptr r = i.run(code, "<editor>"); reply = r && r->t != Value::NIL ? str_of(r) : "ok"; }
    catch (Exit_signal&) { reply = "exit"; }
    catch (std::exception& e) { reply = std::string("error: ") + e.what(); *i.out << "error: " << e.what() << "\n" << std::flush; }
    i.call_stack.clear(); i.function_depth = 0; i.loop_depth = 0; i.stack_depth = 0;
    reply += "\n"; (void)!write(fd, reply.data(), reply.size()); ::close(fd);
}
// Accept connections and read messages; called from the idle hook
inline void serve_poll(Interp& i) {
    eval_server& s = server(); if (s.sock < 0) return;
    for (int k = 0; k < 8; k++) {
        sockaddr_in from{}; socklen_t fl = sizeof from; int fd = accept(s.sock, (sockaddr*)&from, &fl);
        if (fd < 0) break;
        fcntl(fd, F_SETFL, O_NONBLOCK); s.clients.push_back({ fd, "" });
    }
    for (size_t k = 0; k < s.clients.size();) {
        client_read:
        char buf[4096]; long n = read(s.clients[k].fd, buf, sizeof buf);
        if (n > 0) { s.clients[k].buf.append(buf, (size_t)n); if (s.clients[k].buf.find('\x04') == std::string::npos) goto client_read; }
        bool done = n == 0 || (n > 0 && s.clients[k].buf.find('\x04') != std::string::npos);
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) done = true;
        if (done) { eval_server::client c = s.clients[k]; s.clients.erase(s.clients.begin() + k); serve_evaluate(i, c.fd, c.buf); }
        else k++;
    }
}
#else
inline void serve_start(Interp& i, int) { i.bad("the evaluation port is not available on this platform"); }
inline void serve_stop() {}
inline void serve_poll(Interp&) {}
#endif

// (serve port) start the evaluation port; (serve-stop); (serving) => the port or 0
inline vptr fn_serve(vlist& a, Interp& i) { serve_start(i, (int)i.scalar(a[0])); return v_num(server().port); }
inline vptr fn_serve_stop(vlist&, Interp&) { serve_stop(); return v_nil(); }
inline vptr fn_serving(vlist&, Interp&) { return v_num(server().port); }
inline void add_serve(Interp& i) {         // called by add_system: the port is served from the idle hook
    i.def("serve", fn_serve, 1, 1); i.def("serve-stop", fn_serve_stop, 0, 0); i.def("serving", fn_serving, 0, 0);
    auto prev = i.idle_fn;
    i.idle_fn = [&i, prev]() { if (prev) prev(); serve_poll(i); };
}


inline void add_system(Interp& i) {
    i.def("exec", sys_exec, 1, 1); i.def("getenv", sys_getenv, 1, 1); i.def("sleep", sys_sleep, 1, 1); i.def("breathe", sys_breathe, 0, 0); i.def("interactive?", sys_interactive, 0, 0); i.def("now", sys_now, 0, 0);
    i.def("find-file", sys_find_file, 1, 1); i.def("resolve-path", sys_resolve_path, 1, 1);
    add_serve(i);
    i.def("cwd", sys_cwd, 0, 0); i.def("ls", sys_ls, 0, 1); i.def("mkdir", sys_mkdir, 1, 1); i.def("remove", sys_remove, 1, 1); i.def("stat", sys_stat, 1, 1);
    i.def("read-csv", sys_read_csv, 1, 1);
    i.def("read-wav", sys_read_wav, 1, 1); i.def("write-wav", sys_write_wav, 3, 4);
#ifndef _WIN32
    i.def("udp-send", sys_udp_send, 3, 4); i.def("udp-receive", sys_udp_receive, 2, 3);
#endif
}

} // namespace musil

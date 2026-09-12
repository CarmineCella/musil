// serve.h — the evaluation port: a local TCP socket that evaluates the text it receives.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// Editors send code here (the VS Code extension, or `nc localhost 7770 < block.mu`): a
// message is the text up to the end of the connection (or a line holding only \x04), it is
// evaluated in the global environment as if typed in the console, its printed output goes
// where the host's output goes, and a one-line reply ("ok", the value, or "error: ...")
// is written back. The server is polled from the interpreter's idle hook, so it works in
// the CLI (`musil --serve 7770 -i`), during a running script, and in the Listener.
// Only local connections (127.0.0.1) are accepted.

#pragma once
#include "core.h"
#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace musil {

struct eval_server {
    int sock = -1, port = 0;
    struct client { int fd; std::string buf; };
    std::vector<client> clients;
    std::function<void(const std::string&)> on_receive;   // hosts may show what arrived (the Listener's code pane)
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
inline void add_serve(Interp& i) {
    i.def("serve", fn_serve, 1, 1); i.def("serve-stop", fn_serve_stop, 0, 0); i.def("serving", fn_serving, 0, 0);
    auto prev = i.idle_fn;
    i.idle_fn = [&i, prev]() { if (prev) prev(); serve_poll(i); };
}

} // namespace musil

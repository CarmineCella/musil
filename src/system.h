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
inline vptr sys_sleep(vlist& a, Interp& i) { std::this_thread::sleep_for(std::chrono::duration<double>(i.scalar(a[0]))); return v_nil(); }
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
// (read-wav path) => (list sample-rate (list channel-vector ...)); 16-bit PCM or 32-bit float WAV
inline vptr sys_read_wav(vlist& a, Interp& i) {
    WAVHeader h{}; std::vector<std::vector<double>> chans;
    try { chans = read_wav_raw(i.read_path(i.str(a[0])).c_str(), h); } catch (std::exception& e) { i.bad(e.what()); }
    vlist cl;
    for (auto& c : chans) { varr v(c.size()); for (size_t k = 0; k < c.size(); k++) v[k] = c[k]; cl.push_back(v_arr(std::move(v))); }
    return v_list({ v_num((double)h.sampleRate), v_list(std::move(cl)) });
}
// (write-wav path sample-rate channels [bits]) => nil. channels: a vector (mono) or a list of vectors; bits 16 (default) or 32 (float)
inline vptr sys_write_wav(vlist& a, Interp& i) {
    double sr = i.scalar(a[1]); if (sr <= 0) i.bad("sample rate must be > 0");
    int bits = a.size() > 3 ? (int)i.scalar(a[3]) : 16;
    if (bits != 16 && bits != 32) i.bad("bits must be 16 or 32");
    std::vector<std::vector<double>> chans;
    auto add = [&](const vptr& v) { const varr& n = i.num(v); chans.emplace_back(std::begin(n), std::end(n)); };
    if (a[2]->t == Value::NUM) add(a[2]);
    else for (auto& c : i.list(a[2])) add(c);
    if (chans.empty()) i.bad("no channels");
    for (auto& c : chans) if (c.size() != chans[0].size()) i.bad("all channels must have the same length");
    WAVHeader h{};
    std::memcpy(h.riff, "RIFF", 4); std::memcpy(h.wave, "WAVE", 4); std::memcpy(h.fmt, "fmt ", 4); std::memcpy(h.data, "data", 4);
    h.subchunk1Size = 16; h.audioFormat = bits == 32 ? 3 : 1;
    h.numChannels = (uint16_t)chans.size(); h.sampleRate = (uint32_t)sr; h.bitsPerSample = (uint16_t)bits;
    h.blockAlign = (uint16_t)(h.numChannels * bits / 8); h.byteRate = h.sampleRate * h.blockAlign;
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

inline void add_system(Interp& i) {
    i.def("exec", sys_exec, 1, 1); i.def("getenv", sys_getenv, 1, 1); i.def("sleep", sys_sleep, 1, 1); i.def("now", sys_now, 0, 0);
    i.def("cwd", sys_cwd, 0, 0); i.def("ls", sys_ls, 0, 1); i.def("mkdir", sys_mkdir, 1, 1); i.def("remove", sys_remove, 1, 1); i.def("stat", sys_stat, 1, 1);
    i.def("read-csv", sys_read_csv, 1, 1);
    i.def("read-wav", sys_read_wav, 1, 1); i.def("write-wav", sys_write_wav, 3, 4);
#ifndef _WIN32
    i.def("udp-send", sys_udp_send, 3, 4); i.def("udp-receive", sys_udp_receive, 2, 3);
#endif
}

} // namespace musil

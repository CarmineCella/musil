// system.h — operating-system access 
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.

#pragma once
#include "core.h"
#include <cstdio>
#include <thread>

namespace musil {

// (exec "command") => the command's standard output as a string
inline vptr sys_exec(vlist& a, Interp& i) {
    const std::string& cmd = i.str(a[0]);
    FILE* p = popen(cmd.c_str(), "r"); if (!p) i.bad("cannot run " + cmd);
    std::string out; char buf[256]; while (fgets(buf, sizeof(buf), p)) out += buf;
    pclose(p); return v_str(std::move(out));
}
// (getenv "NAME") => value or nil
inline vptr sys_getenv(vlist& a, Interp& i) { const char* v = std::getenv(i.str(a[0]).c_str()); return v ? v_str(v) : v_nil(); }
// (cwd) => current directory
inline vptr sys_cwd(vlist&, Interp&) { return v_str(fs::current_path().string()); }
// (ls [dir]) => list of entry names
inline vptr sys_ls(vlist& a, Interp& i) {
    fs::path d = a.empty() ? fs::current_path() : fs::path(i.str(a[0]));
    std::error_code ec; vlist out;
    for (auto& e : fs::directory_iterator(d, ec)) out.push_back(v_str(e.path().filename().string()));
    if (ec) i.bad("cannot list " + d.string());
    return v_list(std::move(out));
}
// (sleep seconds)
inline vptr sys_sleep(vlist& a, Interp& i) { std::this_thread::sleep_for(std::chrono::duration<double>(i.scalar(a[0]))); return v_nil(); }

inline void add_system(Interp& i) {
    i.def("exec", sys_exec, 1, 1);
    i.def("getenv", sys_getenv, 1, 1);
    i.def("cwd", sys_cwd, 0, 0);
    i.def("ls", sys_ls, 0, 1);
    i.def("sleep", sys_sleep, 1, 1);
}

} // namespace musil

// eof


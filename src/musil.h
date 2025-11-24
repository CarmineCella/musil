// musil.h
//

#ifndef MUSIL_H
#define MUSIL_H

#include <sstream>
#include <cstdlib>

#include "core.h"
#include "system.h"

#define BOLDWHITE   "\033[1m\033[37m"
#define BOLDBLUE    "\033[1m\033[34m"
#define RED     	"\033[31m" 
#define RESET   	"\033[0m"

static std::string get_home_directory() {
    std::string home;
#ifdef _WIN32
    if (const char* up = std::getenv("USERPROFILE")) {
        home = up;
    } else {
        const char* hd = std::getenv("HOMEDRIVE");
        const char* hp = std::getenv("HOMEPATH");
        if (hd && hp) home = std::string(hd) + hp;
    }
#else // POSIX
    if (const char* h = std::getenv("HOME")) home = h;
#endif
    if (home.empty()) home = ".";
    return home;
}

AtomPtr make_env () {
    AtomPtr env = make_atom ();
    env->tail.push_back (make_atom ()); // no parent env
    add_core (env);
    add_system (env);
    // add_signals (env);
    // add_learning (env);
    // add_plotting (env);

    std::string longname = get_home_directory();
	longname += "/.musil";
    env->paths.push_back (longname);
    return env;
}
#endif // MUSIL_H

// eof

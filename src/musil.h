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

AtomPtr make_env () {
    AtomPtr env = make_atom ();
    env->tail.push_back (make_atom ()); // no parent env
    add_core (env);
    add_system (env);
    // add_signals (env);
    // add_learning (env);
    // add_plotting (env);
    return env;
}
#endif // MUSIL_H

// eof

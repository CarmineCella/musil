// musil.h
//

#ifndef MUSIL_H
#define MUSIL_H

#include <sstream>
#include <cstdlib>

#include "core.h"
#include "system.h"
#include "scientific.h"
#include "plotting.h"

AtomPtr make_env (YieldFunction yield_fn = nullptr) {
    set_yield (yield_fn);
    AtomPtr env = make_atom ();
    env->tail.push_back (make_atom ()); // no parent env
    add_core (env);
    add_system (env);
    add_scientific (env);
    add_plotting (env);
    // add_signals (env);
    return env;
}
#endif // MUSIL_H

// eof

// musil.h — umbrella header: the core plus every bundled library.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//

#pragma once
#include "core.h"
#include "std.h"
#include "system.h"
// #include "signals.h"
// #include "scientific.h"

namespace musil {
inline void make_env(Interp& i) {
    add_std(i);
    add_system(i);
    // add_signals(i); add_scientific(i);
}
}

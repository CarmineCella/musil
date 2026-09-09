// musil.h — umbrella header: the core plus every bundled library.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//
// Hosts that want only the language include "core.h". Hosts that want the
// batteries include this file and call musil::add_all(interp).

#pragma once
#include "core.h"
// Libraries register themselves through interp::def(name, op_t).
// #include "system.h"
// #include "signals.h"
// #include "scientific.h"

namespace musil {
inline void add_all(interp& i) {
    (void)i;
    // add_system(i); add_signals(i); add_scientific(i);
}
}

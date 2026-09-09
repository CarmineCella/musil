// musil.h — umbrella header: the core plus every bundled library.
//
// Copyright (c) 2026 Carmine-Emanuele Cella. All rights reserved.
//

#pragma once
#include "core.h"
#include "std.h"
#include "system.h"
#include "scientific.h"
#include "signals.h"

namespace musil {
// C++ halves of the bundled libraries.
inline void make_env(Interp& i) {
    add_std(i);
    add_system(i);
    add_scientific(i);
    add_signals(i);
}
// The Musil halves (std.mu, system.mu) are not loaded automatically:
// a program says (load "std.mu") and load finds it in MUSIL_PATH or ~/.musil.
}

// musil.h

#ifndef MUSIL_H
#define MUSIL_H

#include "core.h"
#include "scientific.h"
#include "system.h"

Environment* make_environment() {
    Environment* env = new Environment();

    add_scientific(*env);
    add_system(*env);

    return env;
}


#endif // MUSIL_H


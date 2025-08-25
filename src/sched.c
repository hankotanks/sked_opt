#include "sched.h"

#include <stdarg.h>

#include <lp_lib.h>

#include "hh.h"

#include "ilp.h"

void
sched_start(void) {
    Program ilp;
    Program_init(&ilp);
    Program_dump(&ilp);
    HH_DBG("Loading ILP");
    Program_load_skycov(&ilp);
    HH_DBG("Running ILP");
    if(Program_solve(&ilp)) HH_DBG("Optimal solution found");
    else HH_DBG("Unable to find optimal solution");
    Program_free(&ilp);
}


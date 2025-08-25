#include "sched.h"

#include <lp_lib.h>

#include "hh.h"

#include "ilp.h"

void
sched_start(void) {
    Program ilp;
    Program_init(&ilp);
    Program_dump(&ilp);
    Program_free(&ilp);

#if 0
    lprec* lp = make_lp(0, 0);
    HH_ASSERT(lp != NULL, "Failed to construct ILP.");

    set_col_name(lp, 1, "x");
    set_col_name(lp, 2, "y");

    double row[3], var[2];
#if 0
    max: 143 x + 60 y; 
    120 x + 210 y <= 15000; 
    110 x + 30 y <= 4000; 
    x + y <= 75;
#endif
    row[1] = 143.0;
    row[2] = 60.0;
    set_obj_fn(lp, row);
    set_maxim(lp);
    row[1] = 120.0; row[2] = 210.0;
    add_constraint(lp, row, LE, 15000);
    row[1] = 110.0; row[2] = 30.0;
    add_constraint(lp, row, LE, 4000);
    row[1] = 1.0; row[2] = 1.0;
    add_constraint(lp, row, LE, 75);
    if (solve(lp) == OPTIMAL) {
        get_variables(lp, var);
        printf("Optimal value: %f\n", get_objective(lp));
        printf("x = %f\n", var[0]);
        printf("y = %f\n", var[1]);
    } else {
        printf("No optimal solution found.\n");
    }

    delete_lp(lp);
#endif
}

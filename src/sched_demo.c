#include "sched.h"

#include <stdbool.h>

#define VAR_TYPES \
    X(FIRST) \
    X(SECOND)

#include "ilp_fwd.h"

ILP_VAR_COUNT_IMPL(FIRST) { (void) prog; return 1; }
ILP_VAR_INDEX_IMPL(FIRST) { (void) prog; (void) args; return 0; }

ILP_VAR_COUNT_IMPL(SECOND) { (void) prog; return 1; }
ILP_VAR_INDEX_IMPL(SECOND) { (void) prog; (void) args; return 0; }

#include "ilp.h"

SCHED_IMPL(SCHED_DEMO) {
    (void) out;
    ILP prog;
    ILP_init(&prog);
    ILP_dump(&prog);

    row_begin(&prog);
    row_set(&prog, 120.0, FIRST);
    row_set(&prog, 210.0, SECOND);
    row_end_as_constr(&prog, LE, 15000.0);

    row_begin(&prog);
    row_set(&prog, 110.0, FIRST);
    row_set(&prog, 30.0, SECOND);
    row_end_as_constr(&prog, LE, 4000.0);

    row_begin(&prog);
    row_set(&prog, 1.0, FIRST);
    row_set(&prog, 1.0, SECOND);
    row_end_as_constr(&prog, LE, 75.0);

    row_begin(&prog);
    row_set(&prog, 143.0, FIRST);
    row_set(&prog, 60.0, SECOND);
    row_end_as_obj(&prog);
    set_maxim(prog.rec);

    write_LP(prog.rec, stdout);

    if(ILP_solve(&prog)) {
        printf("Objective value: %f\n", get_objective(prog.rec));
        row_begin_load(&prog);
        HH_MSG("x: %lf", row_get(&prog, FIRST));
        HH_MSG("y: %lf", row_get(&prog, SECOND));
        ILP_free(&prog);
        return true;
    } else {
        ILP_free(&prog);
        return false;
    }
}


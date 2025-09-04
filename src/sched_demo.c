#include "sched.h"

#define VAR_TYPES \
    X(FIRST, false) \
    X(SECOND, false)

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

    row_begin(&prog);
    row_set(&prog, 120.0, FIRST);
    row_set(&prog, 210.0, SECOND);
    row_end_as_constr(&prog, '<', 15000.0);

    row_begin(&prog);
    row_set(&prog, 110.0, FIRST);
    row_set(&prog, 30.0, SECOND);
    row_end_as_constr(&prog, '<', 4000.0);

    row_begin(&prog);
    row_set(&prog, 1.0, FIRST);
    row_set(&prog, 1.0, SECOND);
    row_end_as_constr(&prog, '<', 75.0);

    row_begin(&prog);
    row_set(&prog, 143.0, FIRST);
    row_set(&prog, 60.0, SECOND);
    row_end_as_obj(&prog, true);

    if(ILP_solve(&prog)) {
        HH_MSG("x: %lf", ILP_get_sol(&prog, FIRST));
        HH_MSG("y: %lf", ILP_get_sol(&prog, SECOND));
    } else {
        HH_MSG("Optimal solution not found :(");
    }

    return true;
}

#include "sched.h"

#define VAR_TYPES \
    VAR_CON(FST, -1e100, 1e100) \
    VAR_CON(SND, -1e100, 1e100)

#include "ilp_fwd.h"

VAR_IMPL(FST, { (void) prog; return 1; }, { (void) prog; (void) args; return 0; })
VAR_IMPL(SND, { (void) prog; return 1; }, { (void) prog; (void) args; return 0; })

#include "ilp.h"

SCHED_IMPL(SCHED_TEST) {
    (void) out;
    ILP prog;
    ILP_init(&prog);
    // 120x + 210y < 15000
    row_begin(&prog);
    row_set(&prog, 120.0, FST);
    row_set(&prog, 210.0, SND);
    row_end_as_constr(&prog, '<', 15000.0);
    // 110x + 30y < 4000
    row_begin(&prog);
    row_set(&prog, 110.0, FST);
    row_set(&prog, 30.0, SND);
    row_end_as_constr(&prog, '<', 4000.0);
    // x + y < 75
    row_begin(&prog);
    row_set(&prog, 1.0, FST);
    row_set(&prog, 1.0, SND);
    row_end_as_constr(&prog, '<', 75.0);
    // maximize 143x + 60y
    row_begin(&prog);
    row_set(&prog, 143.0, FST);
    row_set(&prog, 60.0, SND);
    row_end_as_obj(&prog, true);
    // solve the model
    if(ILP_solve(&prog)) {
        HH_MSG("x: %lf", ILP_get_sol(&prog, FST));
        HH_MSG("y: %lf", ILP_get_sol(&prog, SND));
    } else {
        HH_MSG("Optimal solution not found.");
    }
    // cleanup
    ILP_free(&prog);
    return true;
}

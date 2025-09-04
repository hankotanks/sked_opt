#include "sched.h"

#include "hh.h"

void
sched_start(enum sched_type ty) {
    Output out;
    bool ret;
    switch(ty) {
#define X(ty_) \
    case ty_: \
        ret = SCHED_DECL(ty_)(&out); \
        break;
    SCHED_TYPES
#undef X
    default: HH_UNREACHABLE;
    }
    if(ret) {
        HH_MSG("Successfully completed schedule.");
    } else {
        HH_MSG("Failed to complete schedule.");
    }
    // TODO: Do something with the schedule
    // Output should contain the scans
}

#include "sched.h"

#include "hh.h"

#include "time_sys.h"

typedef struct {
    const Source* target;
    uintptr_t* sta;
} Scan;

void
Scan_dump(const Scan* const scan) {
    printf("[");
    cat_name_print(scan->target->name);
    printf(": ");
    for(size_t i = 0, j = hh_arrlen(scan->sta); i < j; ++i) {
        printf("%c%c", ((const Station*) scan->sta[i])->id[0], ((const Station*) scan->sta[i])->id[1]);
    }
    printf("]");
}

struct SCHED_H__Sched { 
    Scan** scans; 
    size_t seg;
};

void
Sched_init(Sched* const out) {
    HH_CALLOC(out->scans, sizeof(Scan*) * (TIME_SYS->duration / TIME_SYS->scan_length));
}

void
Sched_push_begin(Sched* const out, size_t seg, const Source* const target) {
    Scan curr;
    curr.target = target;
    curr.sta = NULL;
    hh_arrput(out->scans[seg], curr);
    out->seg = seg;
}

void
Sched_push(Sched* const out, const Station* const sta) {
    HH_ASSERT(out->seg != SIZE_MAX, "Can only call Sched_push after Sched_push_begin.");
    hh_arrput(hh_arrlast(out->scans[out->seg]).sta, (uintptr_t) sta);
}

void
Sched_push_end(Sched* const out) {
    if(hh_arrlast(out->scans[out->seg]).sta == NULL) (void) hh_arrpop(out->scans[out->seg]);
    out->seg = SIZE_MAX;
}

void
Sched_dump(const Sched* const out) {
    for(size_t i = 0, j, k; i < (TIME_SYS->duration / TIME_SYS->scan_length); ++i) {
        printf("%zu: ", i);
        for(j = 0, k = hh_arrlen(out->scans[i]); j < k; ++j) {
            Scan_dump(&(out->scans[i][j]));
        }
        printf("\n");
    }
}

void
start(enum sched_type ty) {
    // initialize output
    Sched out;
    HH_CALLOC(out.scans, sizeof(Scan*) * (TIME_SYS->duration / TIME_SYS->scan_length));
    // run the schedule
    bool ret;
    switch(ty) {
#define X(ty_) \
    case ty_: \
        ret = SCHED_H__load_##ty_(&out); \
        break;
    SCHED_TYPES
#undef X
    default: HH_UNREACHABLE;
    }
    HH_ASSERT(ret, "Failed to complete schedule.");
    // Sched_dump(&out);
    // free the schedule
    for(size_t i = 0, j, k; i < (TIME_SYS->duration / TIME_SYS->scan_length); ++i) {
        for(j = 0, k = hh_arrlen(out.scans[i]); j < k; ++j) hh_arrfree(out.scans[i][j].sta);
        hh_arrfree(out.scans[i]);
    }
    free(out.scans);
}

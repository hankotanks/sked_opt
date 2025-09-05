#include "sched.h"

#include "hh.h"

#include "time_sys.h"

void
Scan_init(Scan* const scan, const Source* target) {
    scan->target = target;
    scan->sta = NULL;
}

void
Scan_free(Scan* scan) {
    hh_arrfree(scan->sta);
}

void
Scan_add(Scan* const scan, const Station* sta) {
    hh_arrput(scan->sta, (uintptr_t) sta);
}

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

void
Output_init(Output* const out) {
    HH_CALLOC(out->scans, sizeof(Scan*) * (TIME_SYS->duration / TIME_SYS->scan_length));
}

void
Output_free(Output* out) {
    for(size_t i = 0, j, k; i < (TIME_SYS->duration / TIME_SYS->scan_length); ++i) {
        for(j = 0, k = hh_arrlen(out->scans[i]); j < k; ++j) Scan_free(&(out->scans[i][j]));
        hh_arrfree(out->scans[i]);
    }
    free(out->scans);
}

void
Output_add(Output* const out, size_t seg, Scan scan) {
    hh_arrput(out->scans[seg], scan);
}

void
Output_dump(const Output* const out) {
    for(size_t i = 0, j, k; i < (TIME_SYS->duration / TIME_SYS->scan_length); ++i) {
        printf("%zu: ", i);
        for(j = 0, k = hh_arrlen(out->scans[i]); j < k; ++j) {
            Scan_dump(&(out->scans[i][j]));
        }
        printf("\n");
    }
}

void
sched_start(enum sched_type ty) {
    // initialize output
    Output out;
    Output_init(&out);
    // run the schedule
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
    HH_ASSERT(ret, "Failed to complete schedule.");
    Output_dump(&out);
    Output_free(&out);
}

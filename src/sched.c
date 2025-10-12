#include "sched.h"

#include <math.h>

#include "hh.h"

#include "meta.h"
#include "time_sys.h"
#include "network.h"

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

void printf_padded_size_t(size_t val, size_t max) {
    size_t digits = 1;
    size_t tmp = max;
    while(tmp >= 10) {
        tmp /= 10;
        digits++;
    }
    printf("%0*zu", (int) digits, val);
}


char*
Sched_activity(const Sched* const out, const Station* const sta) {
    uintptr_t* activity = NULL;
    size_t count_seg = TIME_SYS->duration / TIME_SYS->scan_length;
    size_t count_sta = 0;
    size_t count_scans;
    Scan curr;
    size_t i, j, k;
    for(i = 0; i < count_seg; ++i) {
        for(j = 0, count_scans = hh_arrlen(out->scans[i]); j < count_scans; ++j) {
            curr = out->scans[i][j];
            for(k = 0, count_sta = hh_arrlen(curr.sta); k < count_sta; ++k) {
                if(sta == ((const Station*) curr.sta[k])) {
                    hh_arrput(activity, (uintptr_t) curr.target);
                    goto sched_activity_break;
                }
            }
        }
        hh_arrput(activity, (uintptr_t) NULL);
sched_activity_break:   
        continue;
    }
    const Source* target = NULL;
    unsigned int sec[2], sec_slew;
    char* activity_str = NULL;
    hh_arradd(activity_str, hh_arrlen(activity));
    size_t count_slew;
    for(i = hh_arrlen(activity); i >= 1; --i) {
        j = i - 1;
        if(activity[j] != (uintptr_t) NULL) {
            if(target != NULL) {
                if((const Source*) activity[j] != target) {
                    sec[0] = (unsigned int) j * TIME_SYS->scan_length;
                    sec_slew = Station_slew_time(sta, (const Source*[2]) { (const Source*) activity[j], target }, sec);
                    count_slew = (sec_slew + TIME_SYS->scan_length - 1) / TIME_SYS->scan_length + 1;
                    if(count_slew) {
                        for(k = count_slew; k > 0; --k) {
#if 0
                            HH_ASSERT(activity_str[j + k] != '+', "Unreachable!");
                            activity_str[j + k] = '.';
#else
                            activity_str[j + k] = (activity_str[j + k] == '+') ? '@' : '.';
#endif
                        }
                    }
                }
            }
            target = (const Source*) activity[j];
            sec[1] = (unsigned int) j * TIME_SYS->scan_length;
            activity_str[j] = '+';
        } else activity_str[j] = ' ';
    }
    hh_arrput(activity_str, '\0');
    hh_arrfree(activity);
    return activity_str;
}

void
Sched_dump(const Sched* const out) {
    size_t count_seg = TIME_SYS->duration / TIME_SYS->scan_length;
    HH_DBG("Dumping generated schedule.");
    for(size_t i = 0, j, k; i < count_seg; ++i) {
        printf_padded_size_t(i, count_seg);
        printf(": ");
        for(j = 0, k = hh_arrlen(out->scans[i]); j < k; ++j) {
            Scan_dump(&(out->scans[i][j]));
        }
        printf("\n");
    }
    const Station* sta;
    char* activity;
    HH_DBG("Dumping station activity.");
    net_it_active(sta) {
        activity = Sched_activity(out, sta);
        printf("%c%c: %s\n", sta->id[0], sta->id[1], activity);
        hh_arrfree(activity);
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
    {
        // TODO
        HH_MSG("out scans: %s", meta_file());
        HH_MSG("out stats: %s", meta_file_stat());
    }
    // free the schedule
    for(size_t i = 0, j, k; i < (TIME_SYS->duration / TIME_SYS->scan_length); ++i) {
        for(j = 0, k = hh_arrlen(out.scans[i]); j < k; ++j) hh_arrfree(out.scans[i][j].sta);
        hh_arrfree(out.scans[i]);
    }
    free(out.scans);
}

#include "sched.h"

#include <math.h>

#include "hh.h"

#include "time_sys.h"
#include "network.h"
#include "cat.h"
#include "map.h"

void
Scan_dump(const Scan* const scan, const struct map* const map) {
    printf("[");
    const Source* target = map_src_get(map, scan->target);
    HH_ASSERT(target != NULL, "Unreachable!");
    cat_name_print(target->name);
    printf(": ");
    const Station* curr;
    for(size_t i = 0, j = hh_arrlen(scan->sta); i < j; ++i) {
        curr = map_sta_get(map, scan->sta[i]);
        HH_ASSERT(curr != NULL, "Unreachable!");
        printf("%c%c", curr->id[0], curr->id[1]);
    }
    printf("]");
}

struct SCHED_H__Sched {
    struct map map;
    Scan** scans; 
    size_t seg;
    enum station_state** activity;
};

void
Sched_push_begin(Sched* const out, size_t seg, const Source* const target) {
    Scan curr;
    curr.target = map_src_idx(&(out->map), target);
    HH_ASSERT(curr.target != SIZE_MAX, "Unreachable!");
    curr.sta = NULL;
    hh_arrput(out->scans[seg], curr);
    out->seg = seg;
}

void
Sched_push(Sched* const out, const Station* const sta) {
    HH_ASSERT(out->seg != SIZE_MAX, "Can only call Sched_push after Sched_push_begin.");
    size_t sta_idx = map_sta_idx(&(out->map), sta);
    HH_ASSERT(sta_idx != SIZE_MAX, "Unreachable!");
    hh_arrput(hh_arrlast(out->scans[out->seg]).sta, sta_idx);
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

enum station_state*
Sched_activity(const Sched* const out, const Station* const sta) {
    const struct map* map = &(out->map);
    size_t sta_idx = map_sta_idx(map, sta);
    HH_ASSERT(sta_idx != SIZE_MAX, "Unreachable!");
    size_t* activity = NULL;
    size_t count_sta = 0;
    size_t count_scans;
    Scan curr;
    size_t i, j, k;
    map_seg_it(map, i) {
        for(j = 0, count_scans = hh_arrlen(out->scans[i]); j < count_scans; ++j) {
            curr = out->scans[i][j];
            for(k = 0, count_sta = hh_arrlen(curr.sta); k < count_sta; ++k) {
                if(sta_idx == curr.sta[k]) {
                    hh_arrput(activity, curr.target);
                    goto sched_activity_break;
                }
            }
        }
        hh_arrput(activity, SIZE_MAX);
sched_activity_break:   
        continue;
    }
    const Source* target = NULL;
    const Source* at;
    unsigned int sec[2], sec_slew;
    enum station_state* state = NULL;
    hh_arradd(state, hh_arrlen(activity));
    size_t count_slew;
    for(i = hh_arrlen(activity); i >= 1; --i) {
        j = i - 1;
        if(activity[j] != SIZE_MAX) {
            at = map_src_get(map, activity[j]);
            HH_ASSERT(at != NULL, "Unreachable!");
            if(target != NULL) {
                if(target != at) {
                    sec[0] = (unsigned int) j * TIME_SYS->scan_length;
                    sec_slew = Station_slew_time(sta, (const Source*[2]) { at, target }, sec);
                    count_slew = (sec_slew + TIME_SYS->scan_length - 1) / TIME_SYS->scan_length + 1;
                    if(count_slew) {
                        for(k = count_slew; k > 0; --k) {
#if 1
                            HH_ASSERT(state[j + k] != STATE_SCAN, "Unreachable!");
                            state[j + k] = STATE_SLEW;
#else
                            state[j + k] = (state[j + k] == STATE_SCAN) ? STATE_FAIL : STATE_SLEW;
#endif
                        }
                    }
                }
            }
            target = at;
            sec[1] = (unsigned int) j * TIME_SYS->scan_length;
            state[j] = STATE_SCAN;
        } else state[j] = STATE_IDLE;
    }
    hh_arrfree(activity);
    return state;
}

Sched*
Sched_init(enum sched_type ty) {
    Sched* out;
    HH_MALLOC(out, sizeof(Sched));
    // register station and source counts
    map_init(&(out->map));
    // allocate remaining buffers
    HH_CALLOC(out->scans, sizeof(Scan*) * (TIME_SYS->duration / TIME_SYS->scan_length));
    HH_CALLOC(out->activity, sizeof(enum station_state*) * out->map.count_sta);
    // solve the corresponding ILP
    bool ret;
    switch(ty) {
#define X(ty_) \
    case ty_: \
        ret = SCHED_H__load_##ty_(out); \
        break;
    SCHED_TYPES
#undef X
    default: HH_UNREACHABLE;
    }
    HH_ASSERT(ret, "Failed to complete schedule.");
    // build activity log
    const Station* sta;
    map_sta_it(&out->map, sta) out->activity[sta_idx] = Sched_activity(out, sta);
    return out;
}

void
Sched_free(Sched* out) {
    // free scans
    for(size_t i = 0, j, k; i < out->map.count_seg; ++i) {
        for(j = 0, k = hh_arrlen(out->scans[i]); j < k; ++j) 
            hh_arrfree(out->scans[i][j].sta);
        hh_arrfree(out->scans[i]);
    }
    free(out->scans);
    // free station activity
    const Station* sta;
    map_sta_it(&out->map, sta) hh_arrfree(out->activity[sta_idx]);
    (void) sta;
    free(out->activity);
    // free map
    map_free(&out->map);
    // free schedule
    free(out);
}

void
Sched_dump(const Sched* const out) {
    HH_DBG("Dumping generated schedule.");
    for(size_t i = 0, j, k; i < out->map.count_seg; ++i) {
        printf_padded_size_t(i, out->map.count_seg);
        printf(": ");
        for(j = 0, k = hh_arrlen(out->scans[i]); j < k; ++j) {
            Scan_dump(&(out->scans[i][j]), &(out->map));
        }
        printf("\n");
    }
    const Station* sta;
    enum station_state* activity;
    HH_DBG("Dumping station activity.");
    map_sta_it(&out->map, sta) {
        activity = out->activity[sta_idx];
        printf("%c%c: ", sta->id[0], sta->id[1]);
        HH_ASSERT(hh_arrlen(activity) == out->map.count_seg, "Unreachable!");
        for(size_t i = 0; i < out->map.count_seg; ++i) {
            switch(activity[i]) {
            case STATE_IDLE: printf(" "); break;
            case STATE_SLEW: printf("."); break;
            case STATE_SCAN: printf("+"); break;
            case STATE_FAIL: printf("!"); break;
            default: HH_UNREACHABLE;
            }
        }
        printf("\n");
    }
}

size_t
Sched_get(const Sched* const out, size_t seg, Scan** const scans) {
    if(hh_arrlen(out->scans[seg]) > 0) (*scans) = out->scans[seg];
    else (*scans) = NULL;
    return hh_arrlen(out->scans[seg]);
}

const enum station_state*
Sched_get_activity(const Sched* const out, const Station* const sta) {
    return out->activity[map_sta_idx(&out->map, sta)];
}

const struct map*
Sched_map(const Sched* const out) {
    return &(out->map);
}

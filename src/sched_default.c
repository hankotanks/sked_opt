#include "sched.h"

#include <math.h>
#include <stdbool.h>
#include <limits.h>

#include <sofam.h>

#include "cat.h"
#include "station.h"
#include "time_sys.h"

#define VAR_TYPES \
    X(STA_ACTIVE, true) \
    X(SRC_OBS, true) \
    X(STA_SKY_COV, true) \
    X(OBJ_MINIMA, false)

#include "ilp_fwd.h"

#define SKY_COV_CELL_COUNT 13
// TODO: This should be configurable somehow
#define SEG_MAX_SLEWING 3

ILP_VAR_COUNT_IMPL(STA_ACTIVE) { return prog->count_seg * prog->count_src * prog->count_sta; }
ILP_VAR_INDEX_IMPL(STA_ACTIVE) { // seg, src, sta
    size_t seg, src, sta;
    seg = va_arg(args, size_t);
    src = va_arg(args, size_t);
    sta = va_arg(args, size_t);
    return seg * prog->count_src * prog->count_sta + src * prog->count_sta + sta;
}

ILP_VAR_COUNT_IMPL(SRC_OBS) { return prog->count_seg * prog->count_src; }
ILP_VAR_INDEX_IMPL(SRC_OBS) { // seg, src
    size_t seg, src;
    seg = va_arg(args, size_t);
    src = va_arg(args, size_t);
    return seg * prog->count_src + src;
}

ILP_VAR_COUNT_IMPL(STA_SKY_COV) { return prog->count_sta * SKY_COV_CELL_COUNT; }
ILP_VAR_INDEX_IMPL(STA_SKY_COV) { // sta, box
    (void) prog;
    size_t sta, box;
    sta = va_arg(args, size_t);
    box = va_arg(args, size_t);
    return sta * SKY_COV_CELL_COUNT + box;
}

ILP_VAR_COUNT_IMPL(OBJ_MINIMA) { (void) prog; return 1; }
ILP_VAR_INDEX_IMPL(OBJ_MINIMA) { (void) prog; (void) args; return 0; } // no params

#include "ilp.h"

// helper function definitions

unsigned int
slew_time(const Station* const sta, 
    const Source* const src_fst, size_t seg_fst, 
    const Source* const src_snd, size_t seg_snd
) {
    (void) sta; (void) src_fst; (void) seg_fst; (void) src_snd; (void) seg_snd;
    unsigned int seconds_fst, seconds_snd;
    seconds_fst = (unsigned int) seg_fst * TIME_SYS->scan_length;
    seconds_snd = (unsigned int) seg_snd * TIME_SYS->scan_length;
    double az_fst, el_fst, az_snd, el_snd;
    Station_az_el(sta, src_fst, seconds_fst, &az_fst, &el_fst);
    Station_az_el(sta, src_snd, seconds_snd, &az_snd, &el_snd);
    // TODO: Continue from SchedulerILP.cpp:248
    // look at Station::isVisible
    if(!Station_src_is_vis(sta, src_snd, seconds_snd)) return UINT_MAX;
    unsigned int seconds_slew = Station_slew_time(sta, src_fst, src_snd, seconds_fst, seconds_snd);
    return seconds_slew;
}

#if 1
void
slew_time_debug(const ILP* const prog, const Station* const sta, size_t seg_fst, size_t seg_snd) {
    printf("Slew times for %.*s\n", (int) cat_name_len(sta->name), sta->name);
    volatile unsigned int seconds_slew;
    ILP_map_src_it(prog, src_fst) {
        ILP_map_src_it(prog, src_snd) {
            seconds_slew = slew_time(sta, &src_fst, seg_fst, &src_snd, seg_snd);
            if(seconds_slew == UINT_MAX) printf("   ");
            else printf("%03u ", seconds_slew);
            (void) seconds_slew;
        }
        printf("\n");
    }
}
#endif

inline double 
wrap_to_two_pi(double angle) {
    angle = fmod(angle, D2PI);
    if(angle < 0) angle += D2PI;
    return angle;
}

size_t
sky_cov_idx(size_t seg, const Source* const src, const Station* const sta) {
    double az, el;
    Station_az_el(sta, src, (unsigned int) seg * TIME_SYS->scan_length, &az, &el);
    size_t row = (size_t) floor(el / (DPI / 4.0));
    double n = row ? 4.0 : 9.0;
    size_t col = (size_t) round(wrap_to_two_pi(az) / (D2PI / n));
    if((double) col > n - 1) col = 0;
    return row ? col + 9 : col;
}

SCHED_IMPL(SCHED_DEFAULT) {
    (void) out;
    ILP prog;
    ILP_init(&prog);
    HH_DBG("Initialized ILP.");
    // each station can only observe one source at a time
    // SchedulerILP.cpp:96
    {
    for(size_t t = 0; t < prog.count_seg; ++t) {
        for(size_t b = 0; b < prog.count_sta; ++b) {
            row_begin(&prog);
            for(size_t k = 0; k < prog.count_src; ++k) 
                row_set(&prog, 1.0, STA_ACTIVE, t, k, b);
            row_end_as_constr(&prog, '<', 1.0);
        }
    }
    HH_DBG("Added contraint: Stations can only observe one source at a time.");
    } // SCOPE END
    // a valid scan requires >= 2 participating stations
    // SchedulerILP.cpp:120
    {
    for(size_t t = 0; t < prog.count_seg; ++t) {
        for(size_t k = 0; k < prog.count_src; ++k) {
            row_begin(&prog);
            for(size_t b = 0; b < prog.count_sta; ++b) 
                row_set(&prog, 1.0, STA_ACTIVE, t, k, b);
            row_set(&prog, (double) prog.count_sta * -1.0, SRC_OBS, t, k);
            row_end_as_constr(&prog, '<', 0.0);
        }
    }
    } // SCOPE END
    // SchedulerILP.cpp:121
    {
    for(size_t t = 0; t < prog.count_seg; ++t) {
        for(size_t k = 0; k < prog.count_src; ++k) {
            row_begin(&prog);
            for(size_t b = 0; b < prog.count_sta; ++b) 
                row_set(&prog, 1.0, STA_ACTIVE, t, k, b);
            row_set(&prog, -2.0, SRC_OBS, t, k);
            row_end_as_constr(&prog, '>', 0.0);
        }
    }
    HH_DBG("Added constraint: A valid scan requires >= 2 participating stations.");
    } // SCOPE END
    // must be sufficient time to slew between two targets
    // SchedulerILP.cpp:110
    {
    unsigned int sec_slew;
    // size_t count = 0, count_max = prog.count_sta * prog.count_src * prog.count_src * prog.count_seg * (prog.count_seg - 1) / 2;
    ILP_map_sta_it(&prog, sta) {
        ILP_map_src_it(&prog, src_fst) {
            ILP_map_src_it(&prog, src_snd) {
                for(size_t seg_fst = 0, seg_snd; seg_fst < prog.count_seg - 1; ++seg_fst) {
                    for(seg_snd = seg_fst + 1; seg_snd < prog.count_seg; ++seg_snd) {
                        sec_slew = slew_time(&sta, &src_fst, seg_fst, &src_snd, seg_snd);
#if 0
                        if(sec_slew != UINT_MAX) HH_DBG("[%zu / %zu] %zu: %u sec for %.*s to slew between %.*s and %.*s", 
                            count++, count_max, 
                            seg_fst,
                            sec_slew,
                            (int) cat_name_len(sta.name), sta.name, 
                            (int) cat_name_len(src_fst.name), src_fst.name,
                            (int) cat_name_len(src_snd.name), src_snd.name);
#endif
                        if((seg_snd - seg_fst - 1) * TIME_SYS->scan_length >= sec_slew) continue;
                        row_begin(&prog);
                        row_set(&prog, 1.0, STA_ACTIVE, seg_fst, src_fst_idx, sta_idx);
                        row_set(&prog, 1.0, STA_ACTIVE, seg_snd, src_snd_idx, sta_idx);
                        row_end_as_constr(&prog, '<', 1.0);
                    }
                }
            }
        }
    }
    HH_DBG("Added constraint: Must be sufficient time to slew between two sources.");
    } // SCOPE END
    // keep sky coverage cells up to date
    // SchedulerILP.cpp:136
    {
    ILP_map_sta_it(&prog, sta) {
        for(size_t box_idx = 0; box_idx < SKY_COV_CELL_COUNT; ++box_idx) {
            row_begin(&prog);
            for(size_t seg = 0; seg < prog.count_seg; ++seg) {
                ILP_map_src_it(&prog, src) if(sky_cov_idx(seg, &src, &sta) == box_idx) 
                    row_set(&prog, -1.0, STA_ACTIVE, seg, src_idx, sta_idx);
            }
            row_set(&prog, 1.0, STA_SKY_COV, sta_idx, box_idx);
            row_end_as_constr(&prog, '<', 0.0);
        }
    } 
    HH_DBG("Added constraint: Keep sky coverage cells up to date.");
    } // SCOPE END
    // objective
    // ShedulerILP.cpp:146
    {
    double co = -1.0 / (double) SKY_COV_CELL_COUNT;
    ILP_map_sta_it(&prog, sta) {
        for(size_t box_idx = 0; box_idx < SKY_COV_CELL_COUNT; ++box_idx)
            row_set(&prog, co, STA_SKY_COV, sta_idx, box_idx);
        row_set(&prog, 1.0, OBJ_MINIMA);
        row_end_as_constr(&prog, '<', 0.0);
    }
    row_begin(&prog);
    row_set(&prog, 1.0, OBJ_MINIMA);
    row_end_as_obj(&prog, true);
    HH_DBG("Added objective.");
    } // SCOPE END
    HH_DBG("Finished building constraints and objective function.");
    if(ILP_solve(&prog)) {
        for(size_t seg = 0, src; seg < prog.count_seg; ++seg) {
            printf("%zu [", seg);
            ILP_map_src_it(&prog, src_temp) {
                if(ILP_get_sol(&prog, SRC_OBS, seg, src_temp_idx) > 0.0) {
                    cat_name_print(src_temp.name);
                    src = src_temp_idx;
                    break;
                }
            }
            printf("]: ");
            ILP_map_sta_it(&prog, sta) {
                if(ILP_get_sol(&prog, STA_ACTIVE, seg, src, sta_idx) > 0.0) {
                    printf("%c%c ", sta.id[0], sta.id[1]);
                }
            }
            printf("\n");
        }
        ILP_free(&prog);
        return true;
    } else {
        ILP_free(&prog);
        return false;
    }
    HH_DBG("Finished solving ILP.");
    // clean up
    ILP_free(&prog);
    return true;
}

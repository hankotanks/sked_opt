#include "sched.h"

#include <math.h>
#include <stdbool.h>
#include <limits.h>

#include <sofam.h>

#include "cat.h"
#include "station.h"
#include "time_sys.h"

#define VAR_TYPES \
    VAR_BIN(STA_ACTIVE) \
    VAR_BIN(SRC_OBS) \
    VAR_BIN(STA_SKY_COV) \
    VAR_CON(OBJ_MINIMA, 0.0, 1e100)

#include "ilp_fwd.h"

VAR_IMPL(STA_ACTIVE, { return prog->count_seg * prog->count_src * prog->count_sta; }, {
    size_t seg = va_arg(args, size_t);
    size_t src = va_arg(args, size_t);
    size_t sta = va_arg(args, size_t);
    return seg * prog->count_src * prog->count_sta + src * prog->count_sta + sta;
})

VAR_IMPL(SRC_OBS, { return prog->count_seg * prog->count_src; }, {
    size_t seg = va_arg(args, size_t);
    size_t src = va_arg(args, size_t);
    return seg * prog->count_src + src;
})

VAR_IMPL(STA_SKY_COV, { return prog->count_sta * STATION_SRC_SKY_COV_MAX; }, {
    (void) prog;
    size_t sta = va_arg(args, size_t);
    size_t box = va_arg(args, size_t);
    return sta * STATION_SRC_SKY_COV_MAX + box;
})

VAR_IMPL(OBJ_MINIMA, { (void) prog; return 1; }, { (void) prog; (void) args; return 0; })

#include "ilp.h"

SCHED_IMPL(SCHED_DEFAULT) {
    ILP prog;
    ILP_init(&prog);
    HH_DBG("Initialized ILP.");
    // TODO: This constraint appears broken
    // each station can only observe one source at a time
    // SchedulerILP.cpp:96
    for(size_t t = 0; t < prog.count_seg; ++t) {
        for(size_t b = 0; b < prog.count_sta; ++b) {
            row_begin(&prog);
            for(size_t k = 0; k < prog.count_src; ++k) 
                row_set(&prog, 1.0, STA_ACTIVE, t, k, b);
            row_end_as_constr(&prog, '<', 1.0);
        }
    }
    HH_DBG("Added contraint: Stations can only observe one source at a time.");
    // a valid scan requires >= 2 participating stations
    // SchedulerILP.cpp:120
    for(size_t t = 0; t < prog.count_seg; ++t) {
        for(size_t k = 0; k < prog.count_src; ++k) {
            row_begin(&prog);
            for(size_t b = 0; b < prog.count_sta; ++b) 
                row_set(&prog, 1.0, STA_ACTIVE, t, k, b);
            row_set(&prog, (double) prog.count_sta * -1.0, SRC_OBS, t, k);
            row_end_as_constr(&prog, '<', 0.0);
        }
    }
    // SchedulerILP.cpp:121
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
    // must be sufficient time to slew between two targets
    // SchedulerILP.cpp:110
    const Source* src_fst;
    const Source* src_snd;
    const Station* sta;
    unsigned int sec[2];
    unsigned int sec_slew;
    ILP_sta_it(&prog, sta) {
        ILP_src_it(&prog, src_fst) {
            ILP_src_it(&prog, src_snd) {
                for(size_t seg_fst = 0, seg_snd; seg_fst < prog.count_seg - 1; ++seg_fst) {
                    for(seg_snd = seg_fst + 1; seg_snd < prog.count_seg; ++seg_snd) {
                        sec[0] = (unsigned int) seg_fst * TIME_SYS->scan_length;
                        sec[1] = (unsigned int) seg_snd * TIME_SYS->scan_length;
                        sec_slew = Station_slew_time(sta, (const Source*[2]) { src_fst, src_snd }, sec);
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
    // keep sky coverage cells up to date
    // SchedulerILP.cpp:136
    ILP_sta_it(&prog, sta) {
        for(size_t box_idx = 0; box_idx < STATION_SRC_SKY_COV_MAX; ++box_idx) {
            row_begin(&prog);
            for(size_t seg = 0; seg < prog.count_seg; ++seg) {
                ILP_src_it(&prog, src_fst) 
                    if(Station_src_sky_cov_idx(sta, src_fst, (unsigned int) seg * TIME_SYS->scan_length) == box_idx) 
                        row_set(&prog, -1.0, STA_ACTIVE, seg, src_fst_idx, sta_idx);
            }
            row_set(&prog, 1.0, STA_SKY_COV, sta_idx, box_idx);
            row_end_as_constr(&prog, '<', 0.0);
        }
    } 
    HH_DBG("Added constraint: Keep sky coverage cells up to date.");
    // objective
    // ShedulerILP.cpp:146
    double co = -1.0 / (double) STATION_SRC_SKY_COV_MAX;
    ILP_sta_it(&prog, sta) {
        (void) sta;
        for(size_t box_idx = 0; box_idx < STATION_SRC_SKY_COV_MAX; ++box_idx)
            row_set(&prog, co, STA_SKY_COV, sta_idx, box_idx);
        row_set(&prog, 1.0, OBJ_MINIMA);
        row_end_as_constr(&prog, '<', 0.0);
    }
    row_begin(&prog);
    row_set(&prog, 1.0, OBJ_MINIMA);
    row_end_as_obj(&prog, true);
    HH_DBG("Finished building constraints and objective function.");
    if(!ILP_solve(&prog)) {
        ILP_free(&prog);
        return false;
    }
    for(size_t seg = 0; seg < prog.count_seg; ++seg) {
        ILP_src_it(&prog, src_fst) {
            if(ILP_get_sol(&prog, SRC_OBS, seg, src_fst_idx) > 0.5) {
                Sched_push_begin(out, seg, src_fst);
                ILP_sta_it(&prog, sta)
                    if(ILP_get_sol(&prog, STA_ACTIVE, seg, src_fst_idx, sta_idx) > 0.5) 
                        Sched_push(out, sta);
                Sched_push_end(out);
            }
        }
    }
    ILP_free(&prog);
    return true;
}

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
    VAR_BIN(BASELINES) \
    VAR_CON(OBJ_SKY_COV, 0.0, 1.0) \
    VAR_CON(OBJ_BASELINES, 0.0, 1.0) \
    VAR_CON(OBJ_SCANS, 0.0, 1.0)

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

VAR_IMPL(BASELINES, { return prog->count_seg * prog->count_sta * (prog->count_sta - 1) / 2; }, {
    size_t seg = va_arg(args, size_t);
    size_t fst = va_arg(args, size_t);
    size_t snd = va_arg(args, size_t);
    size_t pair = fst * (prog->count_sta - 1) - fst * (fst - 1) / 2 + (snd - fst - 1);
    return seg * (prog->count_sta * ( prog->count_sta - 1 ) / 2) + pair;
})

VAR_IMPL(OBJ_SKY_COV, { (void) prog; return 1; }, { (void) prog; (void) args; return 0; })
VAR_IMPL(OBJ_BASELINES, { (void) prog; return 1; }, { (void) prog; (void) args; return 0; })
VAR_IMPL(OBJ_SCANS, { (void) prog; return 1; }, { (void) prog; (void) args; return 0; })

#include "ilp.h"

#define SEG_LOOKAHEAD 2

SCHED_IMPL(SCHED_DEFAULT) {
    ILP prog;
    ILP_init(&prog);
    HH_DBG("Initialized ILP.");
    size_t count;
    // each station can only observe one source at a time
    // SchedulerILP.cpp:96
    count = 0;
    for(size_t t = 0; t < prog.count_seg; ++t) {
        for(size_t b = 0; b < prog.count_sta; ++b) {
            row_begin(&prog);
            for(size_t k = 0; k < prog.count_src; ++k) 
                row_set(&prog, 1.0, STA_ACTIVE, t, k, b);
            row_end_as_constr(&prog, '<', 1.0);
            count++;
        }
    }
    HH_DBG("Added %zu contraints: Stations can only observe one source at a time.", count);
    // forbid impossible observations
    const Station* sta;
    const Source* src;
    count = 0;
    for(size_t t = 0; t < prog.count_seg - 1; ++t) {
        ILP_sta_it(&prog, sta) {
            ILP_src_it(&prog, src) {
                if(Station_src_visible(sta, src, (unsigned int) t * TIME_SYS->scan_length) && \
                    Station_src_visible(sta, src, (unsigned int) (t + 1) * TIME_SYS->scan_length)) continue;
                row_begin(&prog);
                row_set(&prog, 1.0, STA_ACTIVE, t, src_idx, sta_idx);
                row_end_as_constr(&prog, '<', 0.0);
                count++;
            }
        }
    }
    HH_DBG("Added %zu constraints: Explicitly forbid physically impossible observations.", count);
    // constrain baseline variables
    count = 0;
    for(size_t t = 0; t < prog.count_seg; ++t) {
        for(size_t b_fst = 0, b_snd; b_fst < prog.count_sta - 1; ++b_fst) {
            for(b_snd = b_fst + 1; b_snd < prog.count_sta; ++b_snd) {
                row_begin(&prog);
                row_set(&prog, 1.0, BASELINES, t, b_fst, b_snd);
                for(size_t src = 0; src < prog.count_src; ++src) {
                    row_set(&prog, -1.0, STA_ACTIVE, t, src, b_fst);
                }
                row_end_as_constr(&prog, '<', 0.0);
                count++;
                row_begin(&prog);
                row_set(&prog, 1.0, BASELINES, t, b_fst, b_snd);
                for(size_t src = 0; src < prog.count_src; ++src) {
                    row_set(&prog, -1.0, STA_ACTIVE, t, src, b_snd);
                }
                row_end_as_constr(&prog, '<', 0.0);
                count++;
                for(size_t src = 0; src < prog.count_src; ++src) {
                    row_begin(&prog);
                    row_set(&prog, 1.0, BASELINES, t, b_fst, b_snd);
                    row_set(&prog, -1.0, STA_ACTIVE, t, src, b_fst);
                    row_set(&prog, -1.0, STA_ACTIVE, t, src, b_snd);
                    row_end_as_constr(&prog, '>', -1.0);
                    count++;
                }
            }
        }
    }
    HH_DBG("Added %zu contraints: Maintain variables representing baselines.", count);
    // a valid scan requires >= 2 participating stations
    // SchedulerILP.cpp:120
    count = 0;
    for(size_t t = 0; t < prog.count_seg; ++t) {
        for(size_t k = 0; k < prog.count_src; ++k) {
            row_begin(&prog);
            for(size_t b = 0; b < prog.count_sta; ++b) row_set(&prog, 1.0, STA_ACTIVE, t, k, b);
            row_set(&prog, -2.0, SRC_OBS, t, k);
            row_end_as_constr(&prog, '>', 0.0);
            count++;
            row_begin(&prog);
            for(size_t b = 0; b < prog.count_sta; ++b) row_set(&prog, 1.0, STA_ACTIVE, t, k, b);
            row_set(&prog, -1.0 * (double) prog.count_sta, SRC_OBS, t, k);
            row_end_as_constr(&prog, '<', 1.0);
            count++;
        }
    }
    HH_DBG("Added %zu constraints: A valid scan requires >= 2 participating stations.", count);
    // must be sufficient time to slew between two targets
    // SchedulerILP.cpp:110
    const Source* src_fst;
    const Source* src_snd;
    unsigned int sec[2];
    unsigned int sec_slew;
    count = 0;
#if 1
    ILP_sta_it(&prog, sta) {
        ILP_src_it(&prog, src_fst) {
            ILP_src_it(&prog, src_snd) {
                for(size_t seg_fst = 0, seg_snd, seg_max; seg_fst < prog.count_seg - 1; ++seg_fst) {
                    seg_max = (seg_fst + SEG_LOOKAHEAD < prog.count_seg) ? seg_fst + SEG_LOOKAHEAD : prog.count_seg;
                    for(seg_snd = seg_fst + 1; seg_snd < seg_max; ++seg_snd) {
                        sec[0] = (unsigned int) seg_fst * TIME_SYS->scan_length;
                        sec[1] = (unsigned int) seg_snd * TIME_SYS->scan_length;
                        sec_slew = Station_slew_time(sta, (const Source*[2]) { src_fst, src_snd }, sec);
                        if((seg_snd - seg_fst - 1) * TIME_SYS->scan_length >= sec_slew || sec_slew == UINT_MAX) continue;
                        row_begin(&prog);
                        row_set(&prog, 1.0, STA_ACTIVE, seg_fst, src_fst_idx, sta_idx);
                        row_set(&prog, 1.0, STA_ACTIVE, seg_snd, src_snd_idx, sta_idx);
                        // row_end_as_constr(&prog, '<', 1.0);
                        row_end_as_indicator(&prog, '=', 0.0);
                        count++;
                    }
                }
            }
        }
    }
#else
    ILP_sta_it(&prog, sta) {
        ILP_src_it(&prog, src_fst) {
            ILP_src_it(&prog, src_snd) {
                if(src_fst == src_snd) continue;
                for(size_t seg_fst = 0, seg_snd; seg_fst < prog.count_seg; ++seg_fst) {
                    sec[0] = (unsigned int) seg_fst * TIME_SYS->scan_length;
                    for(seg_snd = seg_fst + 1; seg_snd < prog.count_seg; ++seg_snd) {
                        sec[1] = (unsigned int) seg_snd * TIME_SYS->scan_length;
                        sec_slew = Station_slew_time(sta, (const Source*[2]) { src_fst, src_snd }, sec);
                        if((seg_snd - seg_fst) * TIME_SYS->scan_length >= sec_slew) continue;
                        row_begin(&prog);
                        row_set(&prog, 1.0, STA_ACTIVE, seg_fst, src_fst_idx, sta_idx);
                        row_set(&prog, 1.0, STA_ACTIVE, seg_snd, src_snd_idx, sta_idx);
                        // row_end_as_constr(&prog, '<', 1.0);
                        row_end_as_indicator(&prog, '=', 0.0);
                        count++;
                    }
                }
            }
        }
    }
#endif
    HH_DBG("Added %zu constraints: Must be sufficient time to slew between two sources.", count);
    // keep sky coverage cells up to date
    // SchedulerILP.cpp:136
    count = 0;
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
            count++;
        }
    } 
    HH_DBG("Added %zu constraints: Maintain sky coverages.", count);
    // objective
    // ShedulerILP.cpp:146
    count = 0;
    double co = -1.0 / (double) STATION_SRC_SKY_COV_MAX;
    ILP_sta_it(&prog, sta) {
        (void) sta;
        row_begin(&prog);
        for(size_t box_idx = 0; box_idx < STATION_SRC_SKY_COV_MAX; ++box_idx)
            row_set(&prog, co, STA_SKY_COV, sta_idx, box_idx);
        row_set(&prog, 1.0, OBJ_SKY_COV);
        row_end_as_constr(&prog, '<', 0.0);
        count++;
    }
    HH_DBG("Added %zu constraints: Sky coverage objective.", count);
    count = 0;
    co = 1.0 / (double) prog.count_seg;
    ILP_sta_it(&prog, sta) {
        row_begin(&prog);
        for(size_t t = 0; t < prog.count_seg; ++t) {
            ILP_src_it(&prog, src_fst) row_set(&prog, co, STA_ACTIVE, t, src_fst_idx, sta_idx);
        }
        row_set(&prog, -1.0, OBJ_SCANS);
        row_end_as_constr(&prog, '<', 0.0);
        count++;
        row_begin(&prog);
        row_set(&prog, 1.0, OBJ_SCANS);
        for(size_t t = 0; t < prog.count_seg; ++t) {
            ILP_src_it(&prog, src_fst) row_set(&prog, -1.0 * co, STA_ACTIVE, t, src_fst_idx, sta_idx);
        }
        row_end_as_constr(&prog, '<', 0.0);
        count++;
    }
    HH_DBG("Added %zu constraints: Scan count objective.", count);
    count = 0;
    co = 1.0 / (double) prog.count_seg;
    for(size_t b_fst = 0; b_fst < prog.count_sta; ++b_fst) {
        for(size_t b_snd = b_fst + 1; b_snd < prog.count_sta; ++b_snd) {
            row_begin(&prog);
            for(size_t t = 0; t < prog.count_seg; ++t) row_set(&prog, co, BASELINES, t, b_fst, b_snd);
            row_set(&prog, -1.0, OBJ_BASELINES);
            row_end_as_constr(&prog, '<', 0.0);
            count++;
            row_begin(&prog);
            row_set(&prog, 1.0, OBJ_BASELINES);
            for(size_t t = 0; t < prog.count_seg; ++t) row_set(&prog, -1.0 * co, BASELINES, t, b_fst, b_snd);
            row_end_as_constr(&prog, '<', 0.0);
            count++;
        }
    }
    HH_DBG("Added %zu constraints: Baseline distribution objective.", count);
    row_begin(&prog);
    row_set(&prog, 1.0, OBJ_SKY_COV);
    row_set(&prog, 1.0, OBJ_BASELINES);
    row_set(&prog, 1.0, OBJ_SCANS);  
    row_end_as_obj(&prog, true);
    HH_DBG("Finished building constraints and objective function.");
    // set parameters
    ILP_param_int(&prog, "MIPFocus", 3);
    ILP_param_int(&prog, "Cuts", 2);
    ILP_param_double(&prog, "Heuristics", 0.05);
    ILP_param_int(&prog, "PreSolve", 2);
    if(!ILP_solve(&prog)) {
        ILP_free(&prog);
        return false;
    }
    HH_MSG("Sky coverage objective: %lf", ILP_get_sol(&prog, OBJ_SKY_COV));
    HH_MSG("Baseline distribution objective: %lf", ILP_get_sol(&prog, OBJ_BASELINES));
    HH_MSG("Scan count objective: %lf", ILP_get_sol(&prog, OBJ_SCANS));
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
    Sched_dump(out);
    size_t active;
    for(size_t seg = 0; seg < prog.count_seg; ++seg) {
        ILP_sta_it(&prog, sta) {
            active = 0;
            ILP_src_it(&prog, src_fst)
                if(ILP_get_sol(&prog, STA_ACTIVE, seg, src_fst_idx, sta_idx) > 0.5) active++;
            HH_ASSERT(active < 2, "Station observes 2 sources simultaneously.");
        }
    }
    ILP_free(&prog);
    return true;
}

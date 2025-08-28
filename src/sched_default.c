#include "sched.h"

#define VAR_TYPES \
    X(STA_ACTIVE) \
    X(SRC_OBS) \
    X(STA_SKY_COV) \
    X(OBJ_MINIMA)

#include "ilp_fwd.h"

#define SKY_COV_CELL_COUNT 13

VAR_NUM_IMPL(STA_ACTIVE) { return prog->count_seg * prog->count_src * prog->count_sta; }
// seg, src, sta
VAR_IDX_IMPL(STA_ACTIVE) {
    size_t seg, src, sta;
    seg = va_arg(args, size_t);
    src = va_arg(args, size_t);
    sta = va_arg(args, size_t);
    return seg * prog->count_src * prog->count_sta + src * prog->count_sta + sta;
}

VAR_NUM_IMPL(SRC_OBS) { return prog->count_seg * prog->count_src; }
// seg, src
VAR_IDX_IMPL(SRC_OBS) {
    size_t seg, src;
    seg = va_arg(args, size_t);
    src = va_arg(args, size_t);
    return seg * prog->count_src + src;
}

VAR_NUM_IMPL(STA_SKY_COV) { return prog->count_sta * SKY_COV_CELL_COUNT; }
// sta, box
VAR_IDX_IMPL(STA_SKY_COV) {
    (void) prog;
    size_t sta, box;
    sta = va_arg(args, size_t);
    box = va_arg(args, size_t);
    return sta * SKY_COV_CELL_COUNT + box;
}

VAR_NUM_IMPL(OBJ_MINIMA) { (void) prog; return 1; }
VAR_IDX_IMPL(OBJ_MINIMA) { (void) prog; (void) args; return 0; } // no params

#include "ilp.h"

unsigned int
slew_time(const Station* const sta, 
    const Source* const src_fst, size_t seg_fst, 
    const Source* const src_snd, size_t seg_snd
) {
    (void) sta; (void) src_fst; (void) seg_fst; (void) src_snd; (void) seg_snd;
    return 0;
}

size_t
sky_cov_idx(size_t seg, const Source* const src, const Station* const sta) {
    (void) seg; (void) src; (void) sta;
    return 0;
}

void
dump(const ILP* const prog) {
    HH_DBG("datetime start: %s", time_sys_text(0));
    HH_DBG("datetime final: %s", time_sys_text(time_sys->duration));
    HH_DBG("duration [s]: %u", time_sys->duration);
    HH_DBG("scan length [s]: %u", time_sys->scan_length);
    HH_DBG("scan count: %zu", prog->count_seg);
    HH_DBG("num stations: %zu", prog->count_sta);
    HH_DBG("num sources: %zu",  prog->count_src);
    HH_DBG("sky coverage regions: %d", SKY_COV_CELL_COUNT);
    size_t count_x = prog->count_seg * prog->count_src * prog->count_sta;
    HH_DBG("x count: %zu", count_x);
    size_t count_y = prog->count_seg * prog->count_src;
    HH_DBG("y count: %zu", count_y);
    size_t count_z = prog->count_sta * SKY_COV_CELL_COUNT;
    HH_DBG("z count: %zu", count_z);
    HH_DBG("total variable count: %zu", prog->total_var);
}

void
sched_start_default(void) {
    ILP prog;
    ILP_init(&prog);
    dump(&prog);
    // each station can only observe one source at a time
    // SchedulerILP.cpp:96
    for(size_t t = 0; t < prog.count_seg; ++t) {
        for(size_t b = 0; b < prog.count_sta; ++b) {
            row_begin(&prog);
            for(size_t k = 0; k < prog.count_src; ++k) row_set(&prog, 1.0, STA_ACTIVE, t, k, b);
            row_end_as_constr(&prog, LE, 1.0);
        }
    }
    // a valid scan requires >= 2 participating stations
    // SchedulerILP.cpp:120
    for(size_t t = 0; t < prog.count_seg; ++t) {
        for(size_t k = 0; k < prog.count_src; ++k) {
            row_begin(&prog);
            for(size_t b = 0; b < prog.count_sta; ++b) row_set(&prog, 1.0, STA_ACTIVE, t, k, b);
            row_set(&prog, (double) prog.count_sta * -1.0, SRC_OBS, t, k);
            row_end_as_constr(&prog, LE, 0.0);
        }
    }
    // SchedulerILP.cpp:121
    for(size_t t = 0; t < prog.count_seg; ++t) {
        for(size_t k = 0; k < prog.count_src; ++k) {
            row_begin(&prog);
            for(size_t b = 0; b < prog.count_sta; ++b) row_set(&prog, 1.0, STA_ACTIVE, t, k, b);
            row_set(&prog, -2.0, SRC_OBS, t, k);
            row_end_as_constr(&prog, GE, 0.0);
        }
    }
    // must be sufficient time to slew between two targets
    // SchedulerILP.cpp:110
    {
        ILP_map_sta_it(&prog, sta) {
            ILP_map_src_it(&prog, src_fst) {
                ILP_map_src_it(&prog, src_snd) {
                    for(size_t seg_fst = 0, seg_snd; seg_fst < prog.count_seg - 1; ++seg_fst) {
                        for(seg_snd = seg_fst + 1; seg_snd < prog.count_seg; ++seg_snd) {
                            unsigned int sec_slew = slew_time(&sta, &src_fst, seg_fst, &src_snd, seg_snd);
                            if((seg_snd - seg_fst - 1) * time_sys->scan_length >= sec_slew) continue;
                            row_begin(&prog);
                            row_set(&prog, 1.0, STA_ACTIVE, seg_fst, src_fst_idx, sta_idx);
                            row_set(&prog, 1.0, STA_ACTIVE, seg_snd, src_snd_idx, sta_idx);
                            row_end_as_constr(&prog, LE, 1.0);
                        }
                    }
                }
            }
        }
    }
    // keep sky coverage cells up to date
    // SchedulerILP.cpp:136
    {
        ILP_map_sta_it(&prog, sta) {
            for(size_t box_idx = 0; box_idx < SKY_COV_CELL_COUNT; ++box_idx) {
                row_begin(&prog);
                for(size_t seg = 0; seg < prog.count_seg; ++seg) {
                    ILP_map_src_it(&prog, src) {
                        if(sky_cov_idx(seg, &src, &sta) == box_idx) 
                            row_set(&prog, -1.0, STA_ACTIVE, seg, src_idx, sta_idx);
                    }
                }
                row_set(&prog, 1.0, STA_SKY_COV, sta_idx, box_idx);
                row_end_as_constr(&prog, LE, 0.0);
            }
        }
    }
    // objective
    // ShedulerILP.cpp:146
    {
        ILP_map_sta_it(&prog, sta) {
            for(size_t box_idx = 0; box_idx < SKY_COV_CELL_COUNT; ++box_idx)
                row_set(&prog, -1.0 / (double) SKY_COV_CELL_COUNT, STA_SKY_COV, sta_idx, box_idx);
            row_set(&prog, 1.0, OBJ_MINIMA);
            row_end_as_constr(&prog, LE, 0.0);
        }
        row_begin(&prog);
        row_set(&prog, 1.0, OBJ_MINIMA);
        row_end_as_obj(&prog);
        set_maxim(prog.rec);
    }
    HH_DBG("Finished building constraints.");
    ILP_solve(&prog);
    HH_DBG("Finished solving ILP.");
    ILP_free(&prog);
}


#include "sched.h"

#include <stdbool.h>

#include "cat.h"

#define VAR_TYPES \
    X(STA_ACTIVE) \
    X(SRC_OBS) \
    X(STA_SKY_COV) \
    X(OBJ_MINIMA)

#include "ilp_fwd.h"

#define SKY_COV_CELL_COUNT 13

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
    // TODO: IMPLEMENT
    (void) sta; (void) src_fst; (void) seg_fst; (void) src_snd; (void) seg_snd;
    return 0;
}

size_t
sky_cov_idx(size_t seg, const Source* const src, const Station* const sta) {
    // TODO: IMPLEMENT
    (void) seg; (void) src; (void) sta;
    return 0;
}

SCHED_IMPL(SCHED_DEFAULT) {
    (void) out;
    ILP prog;
    ILP_init(&prog);
    ILP_dump(&prog);
    // each station can only observe one source at a time
    // SchedulerILP.cpp:96
    {
    for(size_t t = 0; t < prog.count_seg; ++t) {
        for(size_t b = 0; b < prog.count_sta; ++b) {
            row_begin(&prog);
            for(size_t k = 0; k < prog.count_src; ++k) 
                row_set(&prog, 1.0, STA_ACTIVE, t, k, b);
            row_end_as_constr(&prog, LE, 1.0);
        }
    }
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
            row_end_as_constr(&prog, LE, 0.0);
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
            row_end_as_constr(&prog, GE, 0.0);
        }
    }
    } // SCOPE END
    // must be sufficient time to slew between two targets
    // SchedulerILP.cpp:110
    {
    unsigned int sec_slew;
    ILP_map_sta_it(&prog, sta) {
        ILP_map_src_it(&prog, src_fst) {
            ILP_map_src_it(&prog, src_snd) {
                for(size_t seg_fst = 0, seg_snd; seg_fst < prog.count_seg - 1; ++seg_fst) {
                    for(seg_snd = seg_fst + 1; seg_snd < prog.count_seg; ++seg_snd) {
                        sec_slew = slew_time(&sta, &src_fst, seg_fst, &src_snd, seg_snd);
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
            row_end_as_constr(&prog, LE, 0.0);
        }
    } // SCOPE END
    }
    // objective
    // ShedulerILP.cpp:146
    {
    double co = -1.0 / (double) SKY_COV_CELL_COUNT;
    ILP_map_sta_it(&prog, sta) {
        for(size_t box_idx = 0; box_idx < SKY_COV_CELL_COUNT; ++box_idx)
            row_set(&prog, co, STA_SKY_COV, sta_idx, box_idx);
        row_set(&prog, 1.0, OBJ_MINIMA);
        row_end_as_constr(&prog, LE, 0.0);
    }
    row_begin(&prog);
    row_set(&prog, 1.0, OBJ_MINIMA);
    row_end_as_obj(&prog);
    set_maxim(prog.rec);
    } // SCOPE END
    HH_DBG("Finished building constraints.");
    if(ILP_solve(&prog)) {
        row_begin_load(&prog);
        for(size_t seg = 0, src; seg < prog.count_seg; ++seg) {
            printf("%zu [", seg);
            ILP_map_src_it(&prog, src_temp) {
                if(row_get(&prog, SRC_OBS, seg, src_temp_idx) > 0.0) {
                    cat_name_print(src_temp.name);
                    src = src_temp_idx;
                    break;
                }
            }
            printf("]: ");
            ILP_map_sta_it(&prog, sta) {
                if(row_get(&prog, STA_ACTIVE, seg, src, sta_idx) > 0.0) {
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

    // solve the ILP
    
    ILP_solve(&prog);
    HH_DBG("Finished solving ILP.");
    // clean up
    ILP_free(&prog);
    return true;
}


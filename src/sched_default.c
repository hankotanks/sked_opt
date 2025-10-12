#include "sched.h"

#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <float.h>

#include <sofam.h>
#include <stddef.h>

#include "cat.h"
#include "station.h"
#include "time_sys.h"

#if 0
#define MAX_SCAN_REPETITIONS 4
#endif

#define WEIGHT_SKY_COV  1.0 // 70%
#define WEIGHT_BASELINE 0.0 // 30%

#define VAR_TYPES \
    VAR_BIN(STA_ACTIVE) \
    VAR_BIN(BASELINE) \
    VAR_BIN(OBJ_SKY_COV)

#include "ilp_fwd.h"

static inline size_t
sta_active_index(const ILP* const prog, size_t seg, size_t src, size_t sta) {
    return seg * prog->count_src * prog->count_sta + src * prog->count_sta + sta;
}

VAR_IMPL(STA_ACTIVE, { return prog->count_seg * prog->count_src * prog->count_sta; }, {
    size_t seg = va_arg(args, size_t);
    size_t src = va_arg(args, size_t);
    size_t sta = va_arg(args, size_t);
    return sta_active_index(prog, seg, src, sta);
})

static inline size_t 
baseline_count(size_t count_sta) {
    return count_sta * (count_sta - 1) / 2;
}

static inline size_t 
baseline_index_inner(const ILP* const prog, size_t sta_a, size_t sta_b) {
    HH_ASSERT(sta_a < sta_b, "Unreachable!");
    return sta_a * (2 * prog->count_sta - sta_a - 1) / 2 + (sta_b - sta_a - 1);
}

static inline size_t 
baseline_index(const ILP* const prog, size_t sta_a, size_t sta_b) {
    return baseline_index_inner(prog, HH_MIN(sta_a, sta_b), HH_MAX(sta_a, sta_b));
}

VAR_IMPL(BASELINE, { return prog->count_seg * prog->count_src * baseline_count(prog->count_sta); }, {
    size_t seg = va_arg(args, size_t);
    size_t src = va_arg(args, size_t);
    size_t sta_a = va_arg(args, size_t);
    size_t sta_b = va_arg(args, size_t);
    size_t n = baseline_count(prog->count_sta);
    return seg * prog->count_src * n + src * n + baseline_index(prog, sta_a, sta_b);
})

VAR_IMPL(OBJ_SKY_COV, { return prog->count_sta * STATION_SRC_SKY_COV_MAX; }, {
    (void) prog;
    size_t sta = va_arg(args, size_t);
    size_t box = va_arg(args, size_t);
    return sta * STATION_SRC_SKY_COV_MAX + box;
})

#include "ilp.h"

static void
add_constr_sta_exclusion(ILP* const prog) {
    const Station* sta;
    const Source* src;
    size_t count = 0;
    for(size_t seg = 0; seg < prog->count_seg; ++seg) {
        ILP_sta_it(prog, sta) {
            ILP_row_begin(prog);
            ILP_src_it(prog, src) {
                ILP_row_set(prog, 1.0, STA_ACTIVE, seg, src_idx, sta_idx);
            }
            ILP_row_end_as_constr(prog, '<', 1.0);
            count++;
        }
    }
    (void) sta;
    HH_DBG("Added %zu contraints: Stations can only observe one source at a time.", count);
}

static bool*
forbid_sta(ILP* const prog) {
    bool* viewable;
    HH_CALLOC(viewable, sizeof(bool) * prog->count_seg * prog->count_src * prog->count_sta);
    const Station* sta;
    const Source* src;
    size_t count = 0;
    for(size_t seg = 0; seg < prog->count_seg; ++seg) {
        ILP_sta_it(prog, sta) {
            ILP_src_it(prog, src) {
                if( Station_src_visible(sta, src, (unsigned int) seg * TIME_SYS->scan_length) && \
                    Station_src_visible(sta, src, (unsigned int) (seg + 1) * TIME_SYS->scan_length)) {
                    viewable[sta_active_index(prog, seg, src_idx, sta_idx)] = true;
                    continue;
                }
                ILP_var_param_dbl(prog, "LB", 0.0, STA_ACTIVE, seg, src_idx, sta_idx);
                ILP_var_param_dbl(prog, "UB", 0.0, STA_ACTIVE, seg, src_idx, sta_idx);
                count++;
            }
        }
    }
    HH_DBG("Forbid %zu STA_ACTIVE variables.", count);
    return viewable;
}

static void
add_constr_sta_participation(ILP* const prog) {
    const Station* sta_a;
    const Station* sta_b;
    const Source* src;
    size_t count = 0;
    for(size_t seg = 0; seg < prog->count_seg; ++seg) {
        ILP_src_it(prog, src) {
            ILP_sta_it(prog, sta_a) {
                ILP_row_begin(prog);
                ILP_row_set(prog, 1.0, STA_ACTIVE, seg, src_idx, sta_a_idx);
                ILP_sta_it(prog, sta_b) {
                    if(sta_a_idx == sta_b_idx) continue;
                    ILP_row_set(prog, -1.0, STA_ACTIVE, seg, src_idx, sta_b_idx);
                }
                ILP_row_end_as_constr(prog, '<', 0.0);
                count++;
            }
        }
    }
    (void) sta_a;
    (void) sta_b;
    HH_DBG("Added %zu constraints: 2 participants are required for a scan.", count);
}

#if 0
static void
add_constr_sta_repetition(ILP* const prog) {
    const Station* sta;
    const Source* src;
    size_t count = 0;
    ILP_sta_it(prog, sta) { 
        ILP_src_it(prog, src) {
            for(size_t seg = 0, seg_off; seg < prog->count_seg - MAX_SCAN_REPETITIONS - 1; ++seg) {
                ILP_row_begin(prog);
                for(seg_off = 0; seg_off <= MAX_SCAN_REPETITIONS; ++seg_off) {
                    ILP_row_set(prog, 1.0, STA_ACTIVE, seg + seg_off, src_idx, sta_idx);
                }
                ILP_row_end_as_constr(prog, '<', MAX_SCAN_REPETITIONS);
                count++;
            }
        }
    }
    (void) sta;
    HH_DBG("Added %zu contraints: Prevent scans longer than %u seconds.", count, TIME_SYS->scan_length * MAX_SCAN_REPETITIONS);
}
#endif

static void
add_constr_sta_slew(ILP* const prog, const bool* const viewable) {
    unsigned int sec[2], sec_slew;
    const Station* sta;
    const Source* src_a;
    const Source* src_b;
    size_t count = 0;
    ILP_sta_it(prog, sta) {
        ILP_src_it(prog, src_a) { ILP_src_it(prog, src_b) { if(src_a == src_b) continue;
            for(size_t seg_a = 0, seg_b; seg_a < prog->count_seg; ++seg_a) {
                if(!viewable[sta_active_index(prog, seg_a, src_a_idx, sta_idx)]) continue;
                sec[0] = (unsigned int) seg_a * TIME_SYS->scan_length;
                for(seg_b = seg_a + 1; seg_b < prog->count_seg; ++seg_b) {
                    if(!viewable[sta_active_index(prog, seg_b, src_b_idx, sta_idx)]) continue;
                    sec[1] = (unsigned int) seg_b * TIME_SYS->scan_length;
                    sec_slew = Station_slew_time(sta, (const Source*[2]) { src_a, src_b }, sec);
                    if(seg_b - seg_a - 1 > (sec_slew + TIME_SYS->scan_length - 1) / TIME_SYS->scan_length) continue;
                    ILP_row_begin(prog);
                    ILP_row_set(prog, 1.0, STA_ACTIVE, seg_a, src_a_idx, sta_idx);
                    ILP_row_set(prog, 1.0, STA_ACTIVE, seg_b, src_b_idx, sta_idx);
                    ILP_row_end_as_constr(prog, '<', 1.0);
                    count++;
                }
            }
        } }
    }
    HH_DBG("Added %zu constraints: Must be sufficient time to slew between two sources.", count);
}

static void
add_constr_baseline_link(ILP* const prog, const bool* const viewable) {
    const Station* bln_a;
    const Station* bln_b;
    const Source* src;
    size_t count = 0;
    ILP_bln_it(prog, bln_a, bln_b) {
        for(size_t seg = 0; seg < prog->count_seg; ++seg) {
            ILP_src_it(prog, src) {
                if( !viewable[sta_active_index(prog, seg, src_idx, bln_a_idx)] || \
                    !viewable[sta_active_index(prog, seg, src_idx, bln_b_idx)]) continue;
                ILP_row_begin(prog);
                ILP_row_set(prog, 1.0, BASELINE, seg, src_idx, bln_a_idx, bln_b_idx);
                ILP_row_set(prog, -1.0, STA_ACTIVE, seg, src_idx, bln_a_idx);
                ILP_row_end_as_constr(prog, '<', 0.0);
                count++;
                ILP_row_begin(prog);
                ILP_row_set(prog, 1.0, BASELINE, seg, src_idx, bln_a_idx, bln_b_idx);
                ILP_row_set(prog, -1.0, STA_ACTIVE, seg, src_idx, bln_b_idx);
                ILP_row_end_as_constr(prog, '<', 0.0);
                count++;
            }
        }
    }
    (void) bln_a;
    (void) bln_b;
    HH_DBG("Added %zu contraints: Tie BASELINE to STA_ACTIVE.", count);
}

static void
forbid_baseline(ILP* const prog, const bool* const viewable) {
    const Station* bln_a;
    const Station* bln_b;
    const Source* src;
    size_t count = 0;
    ILP_bln_it(prog, bln_a, bln_b) {
        for(size_t seg = 0; seg < prog->count_seg; ++seg) {
            ILP_src_it(prog, src) {
                if( viewable[sta_active_index(prog, seg, src_idx, bln_a_idx)] && \
                    viewable[sta_active_index(prog, seg, src_idx, bln_b_idx)]) continue;
                ILP_var_param_dbl(prog, "LB", 0.0, BASELINE, seg, src_idx, bln_a_idx, bln_b_idx);
                ILP_var_param_dbl(prog, "UB", 0.0, BASELINE, seg, src_idx, bln_a_idx, bln_b_idx);
                count++;
            }
        }
    }
    (void) bln_a;
    (void) bln_b;
    HH_DBG("Forbid %zu BASELINE variables.", count);
}

#if 0
static void
add_constr_baseline_concurrent(ILP* const prog) {
    const Station* bln_a;
    const Station* bln_b;
    const Source* src;
    size_t count = 0;
    ILP_bln_it(prog, bln_a, bln_b) {
        for(size_t seg = 0; seg < prog->count_seg; ++seg) {
            ILP_row_begin(prog);
            ILP_src_it(prog, src) {
                ILP_row_set(prog, 1.0, BASELINE, seg, src_idx, bln_a_idx, bln_b_idx);
            }
            ILP_row_end_as_constr(prog, '<', 1.0);
            count++;
        }
    }
    (void) bln_a;
    (void) bln_b;
    HH_DBG("Added %zu contraints: Prevent simultaneous baseline observations.", count);
}
#endif

static void
add_constr_obj_sky_cov(ILP* const prog) {
    const Station* sta;
    const Source* src;
    size_t count = 0;
    ILP_sta_it(prog, sta) {
        for(size_t box_idx = 0; box_idx < STATION_SRC_SKY_COV_MAX; ++box_idx) {
            ILP_row_begin(prog);
            for(size_t seg = 0; seg < prog->count_seg; ++seg) {
                ILP_src_it(prog, src) 
                    if(Station_src_sky_cov_idx(sta, src, (unsigned int) seg * TIME_SYS->scan_length) == box_idx) 
                        ILP_row_set(prog, -1.0, STA_ACTIVE, seg, src_idx, sta_idx);
            }
            ILP_row_set(prog, 1.0, OBJ_SKY_COV, sta_idx, box_idx);
            ILP_row_end_as_constr(prog, '<', 0.0);
            count++;
        }
    } 
    HH_DBG("Added %zu constraints: Maintain sky coverages.", count);
}

static void
add_obj_sky_cov(ILP* const prog) {
    const Station* sta;
    double co_sky_cov = 1.0 / (double) STATION_SRC_SKY_COV_MAX / (double) prog->count_sta;
    size_t count = 0;
    ILP_sta_it(prog, sta) {
        (void) sta;
        for(size_t box_idx = 0; box_idx < STATION_SRC_SKY_COV_MAX; ++box_idx) {
            ILP_row_set(prog, co_sky_cov * WEIGHT_SKY_COV, OBJ_SKY_COV, sta_idx, box_idx);
            count++;
        }
    }
    HH_DBG("Added %zu variables to the objective: Sky coverage.", count);
}

static double*
add_obj_baseline_activation(ILP* const prog, const bool* const viewable) {
    const Station* bln_a;
    const Station* bln_b;
    double* co_baseline, baseline_dist_max = 0.0;
    HH_MALLOC(co_baseline, sizeof(double) * baseline_count(prog->count_sta));
    ILP_bln_it(prog, bln_a, bln_b) {
        co_baseline[baseline_index(prog, bln_a_idx, bln_b_idx)] = Station_baseline_dist(bln_a, bln_b);
        baseline_dist_max = HH_MAX(baseline_dist_max, Station_baseline_dist(bln_a, bln_b));
    }
    double baseline_dist_exp_sum = 0.0;
    for(size_t i = 0, j = baseline_count(prog->count_sta); i < j; ++i) {
        co_baseline[i] = exp(co_baseline[i] / baseline_dist_max);
        baseline_dist_exp_sum += co_baseline[i];
    }
    for(size_t i = 0, j = baseline_count(prog->count_sta); i < j; ++i) {
        co_baseline[i] /= baseline_dist_exp_sum;
    }
    const Source* src;
    double co;
    size_t count = 0;
    ILP_bln_it(prog, bln_a, bln_b) {
        co = co_baseline[baseline_index(prog, bln_a_idx, bln_b_idx)] / (double) prog->count_seg * WEIGHT_BASELINE;
        for(size_t seg = 0; seg < prog->count_seg; ++seg) {
            ILP_src_it(prog, src) {
                if( !viewable[sta_active_index(prog, seg, src_idx, bln_a_idx)] || \
                    !viewable[sta_active_index(prog, seg, src_idx, bln_b_idx)]) continue;
                ILP_row_set(prog, co, BASELINE, seg, src_idx, bln_a_idx, bln_b_idx);
                count++;
            }
        } 
    }
    HH_DBG("Added %zu variables to the objective: Baseline activations.", count);
    return co_baseline;
}

static void
dump_sky_cov(ILP* const prog) {
    const Station* sta;
    double sum;
    double var, co;
    co = 1.0 / (double) prog->count_sta * WEIGHT_SKY_COV;
    ILP_sta_it(prog, sta) {
        (void) sta;
        sum = 0.0;
        for(size_t box_idx = 0; box_idx < STATION_SRC_SKY_COV_MAX; ++box_idx) sum += ILP_get_sol(prog, OBJ_SKY_COV, sta_idx, box_idx);
        var = sum / (double) STATION_SRC_SKY_COV_MAX;
        HH_MSG("Sky coverage objective [%c%c, co: %lf]: var: %lf [obj: %lf]", 
            sta->id[0], sta->id[1], 
            co, var, co * var);
    }
}

static void
dump_baseline_activation(ILP* const prog, double* co_baseline) {
    const Station* bln_a;
    const Station* bln_b;
    const Source* src;
    double var, co;
    ILP_bln_it(prog, bln_a, bln_b) {
        var = 0.0;
        for(size_t seg = 0; seg < prog->count_seg; ++seg) {
            ILP_src_it(prog, src) {
                var += ILP_get_sol(prog, BASELINE, seg, src_idx, bln_a_idx, bln_b_idx);
            }
        }
        var /= (double) prog->count_seg;
        co = co_baseline[baseline_index(prog, bln_a_idx, bln_b_idx)] * WEIGHT_BASELINE;
        HH_MSG("Baseline objective [%c%c-%c%c, co: %lf]: var: %lf [obj: %lf]", 
            bln_a->id[0], bln_a->id[1], 
            bln_b->id[0], bln_b->id[1], co, var, co * var);
    }
}

static void
sched_construct(ILP* const prog, Sched* const out) {
    const Station* sta;
    const Source* src;
    for(size_t seg = 0; seg < prog->count_seg; ++seg) {
        ILP_src_it(prog, src) {
            Sched_push_begin(out, seg, src);
            ILP_sta_it(prog, sta) {
                if(ILP_get_sol(prog, STA_ACTIVE, seg, src_idx, sta_idx) > 0.5) Sched_push(out, sta);
            }
            Sched_push_end(out);
        }
    }
}

static bool
sched_validate(ILP* const prog) {
    const Station* sta;
    const Source* src;
    size_t active;
    for(size_t seg = 0; seg < prog->count_seg; ++seg) {
        ILP_sta_it(prog, sta) {
            active = 0;
            ILP_src_it(prog, src)
                if(ILP_get_sol(prog, STA_ACTIVE, seg, src_idx, sta_idx) > 0.5) active++;
            if(active >= 2) return false;
        }
    }
    (void) sta;
    return true;
}

#ifdef DEBUG_SKY_COV
static void
debug_sky_cov(const ILP* const prog) {
    const Station* sta;
    const Source* src;
    ILP_sta_it(prog, sta) {
        ILP_src_it(prog, src) {
            printf("%c%c at ", sta->id[0], sta->id[1]);
            cat_name_print(src->name);
            printf(": ");
            for(size_t seg = 0; seg < prog->count_seg; ++seg) 
                printf("%c", 'a' + (char) Station_src_sky_cov_idx(sta, src, (unsigned int) seg * TIME_SYS->scan_length));
            printf("\n");
        }
    }
}
#endif

SCHED_IMPL(SCHED_DEFAULT) {
    ILP prog;
    ILP_init(&prog);
    HH_DBG("Initialized ILP.");
#ifdef DEBUG_SKY_COV
    debug_sky_cov(&prog);
    exit(0);
#endif
    // add constraints for stations
    add_constr_sta_exclusion(&prog);
    bool* viewable = forbid_sta(&prog);
    add_constr_sta_participation(&prog);
    add_constr_sta_slew(&prog, viewable);  
    // add baseline constraints
    add_constr_baseline_link(&prog, viewable);
    forbid_baseline(&prog, viewable);
    // prepare objective constraints
    add_constr_obj_sky_cov(&prog);
    // add objectives
    ILP_row_begin(&prog);
    add_obj_sky_cov(&prog);
    double* co_baseline = add_obj_baseline_activation(&prog, viewable);
    free(viewable);
    ILP_row_end_as_obj(&prog, true);
    HH_DBG("Finished building constraints and objective function.");
    // solve the model
    if(!ILP_solve(&prog)) {
        ILP_free(&prog);
        return false;
    }
    // output objective values
    dump_sky_cov(&prog);
    dump_baseline_activation(&prog, co_baseline);
    free(co_baseline);
    // build schedule and validate
    sched_construct(&prog, out);
    HH_ASSERT(sched_validate(&prog), "Station observes multiple sources simultaneously.");
    Sched_dump(out);
    // clean up
    ILP_free(&prog);
    return true;
}

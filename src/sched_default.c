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

#define SKY_COV_CELL_COUNT 13
#define SKY_COV_CELL_INDEX Station_sky_cov_idx_13v1

#define WEIGHT_SKY_COV  1.0
#define WEIGHT_BASELINE 0.0

#define VAR_TYPES \
    VAR_BIN(STA_ACTIVE) \
    VAR_BIN(BASELINE) \
    VAR_BIN(OBJ_SKY_COV)

#include "ilp_fwd.h"

static inline size_t
sta_active_index(const struct map* const map, size_t seg, size_t src, size_t sta) {
    return seg * map->count_src * map->count_sta + src * map->count_sta + sta;
}

VAR_IMPL(STA_ACTIVE, { return map->count_seg * map->count_src * map->count_sta; }, {
    size_t seg = va_arg(args, size_t);
    size_t src = va_arg(args, size_t);
    size_t sta = va_arg(args, size_t);
    return sta_active_index(map, seg, src, sta);
})

static inline size_t 
baseline_count(size_t count_sta) {
    return count_sta * (count_sta - 1) / 2;
}

static inline size_t 
baseline_index_inner(const struct map* const map, size_t sta_a, size_t sta_b) {
    HH_ASSERT(sta_a < sta_b, "Unreachable!");
    return sta_a * (2 * map->count_sta - sta_a - 1) / 2 + (sta_b - sta_a - 1);
}

static inline size_t 
baseline_index(const struct map* const map, size_t sta_a, size_t sta_b) {
    return baseline_index_inner(map, HH_MIN(sta_a, sta_b), HH_MAX(sta_a, sta_b));
}

VAR_IMPL(BASELINE, { return map->count_seg * map->count_src * baseline_count(map->count_sta); }, {
    size_t seg = va_arg(args, size_t);
    size_t src = va_arg(args, size_t);
    size_t sta_a = va_arg(args, size_t);
    size_t sta_b = va_arg(args, size_t);
    size_t n = baseline_count(map->count_sta);
    return seg * map->count_src * n + src * n + baseline_index(map, sta_a, sta_b);
})

VAR_IMPL(OBJ_SKY_COV, { return map->count_sta * SKY_COV_CELL_COUNT; }, {
    (void) map;
    size_t sta = va_arg(args, size_t);
    size_t box = va_arg(args, size_t);
    return sta * SKY_COV_CELL_COUNT + box;
})

#include "ilp.h"

static void
add_constr_sta_exclusion(ILP* const prog) {
    const Station* sta;
    const Source* src;
    size_t count = 0;
    size_t seg;
    map_seg_it(prog->map, seg) {
        map_sta_it(prog->map, sta) {
            ILP_row_begin(prog);
            map_src_it(prog->map, src) {
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
    HH_CALLOC(viewable, sizeof(bool) * prog->map->count_seg * prog->map->count_src * prog->map->count_sta);
    const Station* sta;
    const Source* src;
    size_t count = 0;
    size_t seg;
    map_seg_it(prog->map, seg) {
        map_sta_it(prog->map, sta) {
            map_src_it(prog->map, src) {
                if( Station_src_visible(sta, src, (unsigned int) seg * TIME_SYS->scan_length) && \
                    Station_src_visible(sta, src, (unsigned int) (seg + 1) * TIME_SYS->scan_length)) {
                    viewable[sta_active_index(prog->map, seg, src_idx, sta_idx)] = true;
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
    size_t seg;
    map_seg_it(prog->map, seg) {
        map_src_it(prog->map, src) {
            map_sta_it(prog->map, sta_a) {
                ILP_row_begin(prog);
                ILP_row_set(prog, 1.0, STA_ACTIVE, seg, src_idx, sta_a_idx);
                map_sta_it(prog->map, sta_b) {
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

static void
add_constr_sta_slew(ILP* const prog, const bool* const viewable) {
    unsigned int sec[2], sec_slew;
    const Station* sta;
    const Source* src_a;
    const Source* src_b;
    size_t count = 0;
    size_t seg_a, seg_b;
    map_sta_it(prog->map, sta) {
        map_src_it(prog->map, src_a) { map_src_it(prog->map, src_b) { if(src_a == src_b) continue;
            map_seg_it(prog->map, seg_a) {
                if(!viewable[sta_active_index(prog->map, seg_a, src_a_idx, sta_idx)]) continue;
                sec[0] = (unsigned int) seg_a * TIME_SYS->scan_length;
                for(seg_b = seg_a + 1; seg_b < prog->map->count_seg; ++seg_b) {
                    if(!viewable[sta_active_index(prog->map, seg_b, src_b_idx, sta_idx)]) continue;
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
    size_t seg;
    map_bln_it(prog->map, bln_a, bln_b) {
        map_seg_it(prog->map, seg) {
            map_src_it(prog->map, src) {
                if( !viewable[sta_active_index(prog->map, seg, src_idx, bln_a_idx)] || \
                    !viewable[sta_active_index(prog->map, seg, src_idx, bln_b_idx)]) continue;
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
    size_t seg;
    map_bln_it(prog->map, bln_a, bln_b) {
        map_seg_it(prog->map, seg) {
            map_src_it(prog->map, src) {
                if( viewable[sta_active_index(prog->map, seg, src_idx, bln_a_idx)] && \
                    viewable[sta_active_index(prog->map, seg, src_idx, bln_b_idx)]) continue;
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

static void
add_constr_obj_sky_cov(ILP* const prog) {
    const Station* sta;
    const Source* src;
    size_t count = 0;
    size_t seg;
    map_sta_it(prog->map, sta) {
        for(size_t box_idx = 0; box_idx < SKY_COV_CELL_COUNT; ++box_idx) {
            ILP_row_begin(prog);
            map_seg_it(prog->map, seg) {
                map_src_it(prog->map, src) 
                    if(SKY_COV_CELL_INDEX(sta, src, (unsigned int) seg * TIME_SYS->scan_length) == box_idx) 
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
    double co_sky_cov = 1.0 / (double) SKY_COV_CELL_COUNT / (double) prog->map->count_sta;
    size_t count = 0;
    map_sta_it(prog->map, sta) {
        (void) sta;
        for(size_t box_idx = 0; box_idx < SKY_COV_CELL_COUNT; ++box_idx) {
            ILP_row_set(prog, co_sky_cov * WEIGHT_SKY_COV, OBJ_SKY_COV, sta_idx, box_idx);
            count++;
        }
    }
    HH_DBG("Added %zu variables to the objective: Sky coverage.", count);
}

double
baseline_dist(const Station* const sta_fst, const Station* const sta_snd) {
    double dx, dy, dz;
    dx = sta_fst->x - sta_snd->x;
    dy = sta_fst->y - sta_snd->y;
    dz = sta_fst->z - sta_snd->z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

static double*
add_obj_baseline_activation(ILP* const prog, const bool* const viewable) {
    const Station* bln_a;
    const Station* bln_b;
    double* co_baseline, baseline_dist_max = 0.0;
    HH_MALLOC(co_baseline, sizeof(double) * baseline_count(prog->map->count_sta));
    map_bln_it(prog->map, bln_a, bln_b) {
        co_baseline[baseline_index(prog->map, bln_a_idx, bln_b_idx)] = baseline_dist(bln_a, bln_b);
        baseline_dist_max = HH_MAX(baseline_dist_max, baseline_dist(bln_a, bln_b));
    }
    double baseline_dist_exp_sum = 0.0;
    for(size_t i = 0, j = baseline_count(prog->map->count_sta); i < j; ++i) {
        co_baseline[i] = exp(co_baseline[i] / baseline_dist_max);
        baseline_dist_exp_sum += co_baseline[i];
    }
    for(size_t i = 0, j = baseline_count(prog->map->count_sta); i < j; ++i) {
        co_baseline[i] /= baseline_dist_exp_sum;
    }
    const Source* src;
    double co;
    size_t count = 0;
    size_t seg;
    map_bln_it(prog->map, bln_a, bln_b) {
        co = co_baseline[baseline_index(prog->map, bln_a_idx, bln_b_idx)] / (double) prog->map->count_seg * WEIGHT_BASELINE;
        map_seg_it(prog->map, seg) {
            map_src_it(prog->map, src) {
                if( !viewable[sta_active_index(prog->map, seg, src_idx, bln_a_idx)] || \
                    !viewable[sta_active_index(prog->map, seg, src_idx, bln_b_idx)]) continue;
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
    co = 1.0 / (double) prog->map->count_sta * WEIGHT_SKY_COV;
    map_sta_it(prog->map, sta) {
        (void) sta;
        sum = 0.0;
        for(size_t box_idx = 0; box_idx < SKY_COV_CELL_COUNT; ++box_idx) sum += ILP_get_sol(prog, OBJ_SKY_COV, sta_idx, box_idx);
        var = sum / (double) SKY_COV_CELL_COUNT;
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
    size_t seg;
    map_bln_it(prog->map, bln_a, bln_b) {
        var = 0.0;
        map_seg_it(prog->map, seg) {
            map_src_it(prog->map, src) {
                var += ILP_get_sol(prog, BASELINE, seg, src_idx, bln_a_idx, bln_b_idx);
            }
        }
        var /= (double) prog->map->count_seg;
        co = co_baseline[baseline_index(prog->map, bln_a_idx, bln_b_idx)] * WEIGHT_BASELINE;
        HH_MSG("Baseline objective [%c%c-%c%c, co: %lf]: var: %lf [obj: %lf]", 
            bln_a->id[0], bln_a->id[1], 
            bln_b->id[0], bln_b->id[1], co, var, co * var);
    }
}

static void
sched_construct(ILP* const prog, Sched* const out) {
    const Station* sta;
    const Source* src;
    size_t seg;
    map_seg_it(prog->map, seg) {
        map_src_it(prog->map, src) {
            Sched_push_begin(out, seg, src);
            map_sta_it(prog->map, sta) {
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
    size_t seg;
    map_seg_it(prog->map, seg) {
        map_sta_it(prog->map, sta) {
            active = 0;
            map_src_it(prog->map, src)
                if(ILP_get_sol(prog, STA_ACTIVE, seg, src_idx, sta_idx) > 0.5) active++;
            if(active >= 2) return false;
        }
    }
    (void) sta;
    return true;
}

SCHED_IMPL(SCHED_DEFAULT) {
    const struct map* map = Sched_map(out);
    ILP prog;
    ILP_init(&prog, map);
    HH_DBG("Initialized ILP.");
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
    // clean up
    ILP_free(&prog);
    return true;
}

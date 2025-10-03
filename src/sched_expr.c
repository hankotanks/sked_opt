#include "sched.h"

#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <float.h>

#include <sofam.h>

#include "cat.h"
#include "station.h"
#include "time_sys.h"

#define VAR_TYPES \
    VAR_BIN(BASELINE_2) \
    VAR_BIN(STA_SKY_COV_2) \
    VAR_CON(OBJ_BASELINE_2, 0.0, 1.0)

#include "ilp_fwd.h"

static inline size_t 
baseline_count(size_t count_sta) {
    return count_sta * (count_sta - 1) / 2;
}

static inline size_t 
baseline_index_inner(const ILP* const prog, size_t sta_fst, size_t sta_snd) {
    HH_ASSERT(sta_fst < sta_snd, "Unreachable!");
    return sta_fst * (2 * prog->count_sta - sta_fst - 1) / 2 + (sta_snd - sta_fst - 1);
}

static inline size_t 
baseline_index(const ILP* const prog, size_t sta_fst, size_t sta_snd) {
    return baseline_index_inner(prog, HH_MIN(sta_fst, sta_snd), HH_MAX(sta_fst, sta_snd));
}

VAR_IMPL(BASELINE_2, { return prog->count_seg * prog->count_src * baseline_count(prog->count_sta); }, {
    size_t seg = va_arg(args, size_t);
    size_t src = va_arg(args, size_t);
    size_t sta_fst = va_arg(args, size_t);
    size_t sta_snd = va_arg(args, size_t);
    size_t n = baseline_count(prog->count_sta);
    return seg * prog->count_src * n + src * n + baseline_index(prog, sta_fst, sta_snd);
})

VAR_IMPL(STA_SKY_COV_2, { return prog->count_sta * STATION_SRC_SKY_COV_MAX; }, {
    (void) prog;
    size_t sta = va_arg(args, size_t);
    size_t box = va_arg(args, size_t);
    return sta * STATION_SRC_SKY_COV_MAX + box;
})

VAR_IMPL(OBJ_BASELINE_2, { (void) prog; return baseline_count(prog->count_sta); }, { 
    size_t sta_fst = va_arg(args, size_t);
    size_t sta_snd = va_arg(args, size_t);
    return baseline_index(prog, sta_fst, sta_snd);
})

#include "ilp.h"

// each station can only observe one source at a time
size_t 
constr_exclusion(ILP* const prog) {
    const Station* sta;
    const Station* sta_fst;
    const Station* sta_snd;
    const Source* src;
    size_t count = 0;
    for(size_t seg = 0; seg < prog->count_seg; ++seg) {
        ILP_sta_it(prog, sta) {
            row_begin(prog);
            ILP_sta_it(prog, sta_fst) {
                ILP_sta_it(prog, sta_snd) {
                    if(sta_fst_idx >= sta_snd_idx) continue;
                    if(sta_idx != sta_fst_idx && sta_idx != sta_snd_idx) continue;
                    ILP_src_it(prog, src) {
                        row_set(prog, 1.0, BASELINE_2, seg, src_idx, sta_fst_idx, sta_snd_idx);
                    }
                }
            }
            row_end_as_constr(prog, '<', 1.0);
            count++;
        }
    }
    (void) sta;
    (void) sta_fst;
    (void) sta_snd;
    (void) src;
    HH_DBG("Added %zu constraints: Stations can only observe one source at a time.", count);
    return count;
}

size_t 
viewable_index(const ILP* const prog, size_t n, size_t seg, size_t src, size_t sta_fst, size_t sta_snd) {
    return seg * prog->count_src * n + src * n + baseline_index(prog, sta_fst, sta_snd);
}

size_t
constr_viewable(ILP* const prog, bool** viewable) {
    HH_CALLOC(*viewable, prog->count_seg * prog->count_src * baseline_count(prog->count_sta));
    const Station* sta;
    const Station* sta_fst;
    const Station* sta_snd;
    const Source* src;
    size_t count = 0;
    size_t n = baseline_count(prog->count_sta);
    for(size_t seg = 0; seg < prog->count_seg; ++seg) {
        ILP_sta_it(prog, sta_fst) {
            ILP_sta_it(prog, sta_snd) {
                if(sta_fst_idx >= sta_snd_idx) continue;
                ILP_src_it(prog, src) {
                    if( \
                        Station_src_visible(sta_fst, src, (unsigned int) seg * TIME_SYS->scan_length) &&
                        Station_src_visible(sta_snd, src, (unsigned int) seg * TIME_SYS->scan_length) &&
                        Station_src_visible(sta_fst, src, (unsigned int) (seg + 1) * TIME_SYS->scan_length) &&
                        Station_src_visible(sta_snd, src, (unsigned int) (seg + 1) * TIME_SYS->scan_length)) {
                        (*viewable)[viewable_index(prog, n, seg, src_idx, sta_fst_idx, sta_snd_idx)] = true;
                        continue;
                    }
                    row_begin(prog);
                    row_set(prog, 1.0, BASELINE_2, seg, src_idx, sta_fst_idx, sta_snd_idx);
                    row_end_as_constr(prog, '=', 0.0);
                    count++;
                }
            }
        }
    }
    (void) sta;
    (void) sta_fst;
    (void) sta_snd;
    (void) src;
    HH_DBG("Added %zu constraints: Explicitly forbid physically impossible observations.", count);
    return count;
}

size_t
constr_slew(ILP* const prog, bool* viewable) {
    unsigned int sec[2], sec_slew;
    const Station* sta_a_fst;
    const Station* sta_a_snd;
    const Station* sta_b_fst;
    const Station* sta_b_snd;
    const Source* src_fst;
    const Source* src_snd;
#if 0
    sec[0] = 41 * TIME_SYS->scan_length;
    sec[1] = 42 * TIME_SYS->scan_length;
    HH_MSG("%u", Station_slew_time(net_sta("Ny"), (const Source*[2]) { sky_src("1751+288"), sky_src("1807+698") }, sec));
#endif    
    size_t count = 0;
    size_t n = baseline_count(prog->count_sta);
    bool share_a_fst, share_a_snd;
    ILP_src_it(prog, src_fst) { ILP_src_it(prog, src_snd) { if(src_fst == src_snd) continue;
        for(size_t seg_fst = 0, seg_snd; seg_fst < prog->count_seg; ++seg_fst) {
            sec[0] = (unsigned int) seg_fst * TIME_SYS->scan_length;
            for(seg_snd = seg_fst + 1; seg_snd < prog->count_seg; ++seg_snd) {
                sec[1] = (unsigned int) seg_snd * TIME_SYS->scan_length;
                ILP_sta_it(prog, sta_a_fst) { ILP_sta_it(prog, sta_a_snd) { if(sta_a_fst_idx >= sta_a_snd_idx) continue;
                    if( \
                        !viewable[viewable_index(prog, n, seg_fst, src_fst_idx, sta_a_fst_idx, sta_a_snd_idx)] ||
                        !viewable[viewable_index(prog, n, seg_snd, src_snd_idx, sta_a_fst_idx, sta_a_snd_idx)]) continue;
                    ILP_sta_it(prog, sta_b_fst) { ILP_sta_it(prog, sta_b_snd) { if(sta_b_fst_idx >= sta_b_snd_idx) continue;
                        if( \
                            !viewable[viewable_index(prog, n, seg_fst, src_fst_idx, sta_b_fst_idx, sta_b_snd_idx)] ||
                            !viewable[viewable_index(prog, n, seg_snd, src_snd_idx, sta_b_fst_idx, sta_b_snd_idx)]) continue;
                        share_a_fst = sta_a_fst_idx == sta_b_fst_idx || sta_a_fst_idx == sta_b_snd_idx;
                        share_a_snd = sta_a_snd_idx == sta_b_fst_idx || sta_a_snd_idx == sta_b_snd_idx;
                        if(!share_a_fst && !share_a_snd) continue;
                        if(share_a_fst) {
                            sec_slew = Station_slew_time(sta_a_fst, (const Source*[2]) { src_fst, src_snd }, sec);
                            if(seg_snd - seg_fst > (size_t) ceilf((float) sec_slew / (float) TIME_SYS->scan_length)) continue;
                        }
                        if(share_a_snd) {
                            sec_slew = Station_slew_time(sta_a_snd, (const Source*[2]) { src_fst, src_snd }, sec);
                            if(seg_snd - seg_fst > (size_t) ceilf((float) sec_slew / (float) TIME_SYS->scan_length)) continue;
                        }
                        row_begin(prog);
                        row_set(prog, 1.0, BASELINE_2, seg_fst, src_fst_idx, sta_a_fst_idx, sta_a_snd_idx);
                        row_set(prog, 1.0, BASELINE_2, seg_snd, src_snd_idx, sta_b_fst_idx, sta_b_snd_idx);
                        row_end_as_constr(prog, '<', 1.0);
                        count++;
#if 0
                        HH_MSG("%c%c%c%c-%c%c%c%c, %.*s [%zu] to %.*s [%zu]",
                            sta_a_fst->id[0], sta_a_fst->id[1],
                            sta_a_snd->id[0], sta_a_snd->id[1],
                            sta_b_fst->id[0], sta_b_fst->id[1],
                            sta_b_snd->id[0], sta_b_snd->id[1],
                            (int) cat_name_len(src_fst->name), src_fst->name,
                            seg_fst,
                            (int) cat_name_len(src_snd->name), src_snd->name,
                            seg_snd);
#endif
                    } }
                } }
            }
        }
    } }
    (void) sta_a_fst;
    (void) sta_a_snd;
    (void) sta_b_fst;
    (void) sta_b_snd;
    (void) src_fst;
    (void) src_snd;
    HH_DBG("Added %zu constraints: Must be sufficient time to slew between two sources.", count);
    return count;
}

size_t
constr_sky_cov(ILP* const prog) {
    const Station* sta;
    const Station* sta_fst;
    const Station* sta_snd;
    const Source* src;
    size_t count = 0;
    ILP_sta_it(prog, sta) {
        for(size_t box_idx = 0; box_idx < STATION_SRC_SKY_COV_MAX; ++box_idx) {
            ILP_sta_it(prog, sta_fst) { ILP_sta_it(prog, sta_snd) { if(sta_fst_idx >= sta_snd_idx) continue;
                if(sta_idx != sta_fst_idx && sta_idx != sta_snd_idx) continue;
                row_begin(prog);
                for(size_t seg = 0; seg < prog->count_seg; ++seg) {
                    ILP_src_it(prog, src) {
                        if(Station_src_sky_cov_idx(sta, src, (unsigned int) seg * TIME_SYS->scan_length) == box_idx) {
                            row_set(prog, -1.0, BASELINE_2, seg, src_idx, sta_fst_idx, sta_snd_idx);
                        }
                    }
                }
                row_set(prog, 1.0, STA_SKY_COV_2, sta_idx, box_idx);
                row_end_as_constr(prog, '<', 0.0);
                count++;
            } }
        }
    }
    (void) sta;
    (void) sta_fst;
    (void) sta_snd;
    (void) src;
    HH_DBG("Added %zu constraints: Maintain sky coverages.", count);
    return count;
}

size_t 
constr_baseline(ILP* const prog) {
    const Station* sta_fst;
    const Station* sta_snd;
    const Source* src;
    size_t count = 0;
    double co = -1.0 / (double) prog->count_seg;
    ILP_sta_it(prog, sta_fst) {
        ILP_sta_it(prog, sta_snd) {
            if(sta_fst_idx >= sta_snd_idx) continue;
            row_begin(prog);
            for(size_t seg = 0; seg < prog->count_seg; ++seg) {
                ILP_src_it(prog, src) {
                    row_set(prog, co, BASELINE_2, seg, src_idx, sta_fst_idx, sta_snd_idx);
                }
            }
            row_set(prog, 1.0, OBJ_BASELINE_2, sta_fst_idx, sta_snd_idx);
            row_end_as_constr(prog, '<', 0.0);
            count++;
        }
    }
    (void) sta_fst;
    (void) sta_snd;
    (void) src;
    HH_DBG("Added %zu constraints: Baseline objective.", count);
    return count;
}

double*
obj(ILP* const prog) {
    const Station* sta;
    const Station* sta_fst;
    const Station* sta_snd;
    row_begin(prog);
    double co = 1.0 / (double) STATION_SRC_SKY_COV_MAX / (double) prog->count_sta;
    ILP_sta_it(prog, sta) {
        (void) sta;
        for(size_t box_idx = 0; box_idx < STATION_SRC_SKY_COV_MAX; ++box_idx)
            row_set(prog, co, STA_SKY_COV_2, sta_idx, box_idx);
    }
    double* baseline_dist, baseline_dist_max = 0.0;
    HH_MALLOC(baseline_dist, sizeof(double) * baseline_count(prog->count_sta));
    ILP_sta_it(prog, sta_fst) {
        ILP_sta_it(prog, sta_snd) {
            if(sta_fst_idx >= sta_snd_idx) continue;
            baseline_dist[baseline_index(prog, sta_fst_idx, sta_snd_idx)] = Station_baseline_dist(sta_fst, sta_snd);
            baseline_dist_max = HH_MAX(baseline_dist_max, Station_baseline_dist(sta_fst, sta_snd));
        }
    }
    double baseline_dist_exp_sum = 0.0;
    for(size_t i = 0, j = baseline_count(prog->count_sta); i < j; ++i) {
        baseline_dist[i] = exp(baseline_dist[i] / baseline_dist_max);
        baseline_dist_exp_sum += baseline_dist[i];
    }
    for(size_t i = 0, j = baseline_count(prog->count_sta); i < j; ++i) {
        baseline_dist[i] /= baseline_dist_exp_sum;
    }
    ILP_sta_it(prog, sta_fst) {
        ILP_sta_it(prog, sta_snd) {
            if(sta_fst_idx >= sta_snd_idx) continue;
            co = baseline_dist[baseline_index(prog, sta_fst_idx, sta_snd_idx)];
            row_set(prog, co, OBJ_BASELINE_2, sta_fst_idx, sta_snd_idx);
        }
    }
    row_end_as_obj(prog, true);
    HH_DBG("Finished building constraints and objective function.");
    return baseline_dist;
}

void
obj_dump(ILP* const prog, double* baseline_dist) {
    const Station* sta;
    double sum;
    ILP_sta_it(prog, sta) {
        (void) sta;
        sum = 0.0;
        for(size_t box_idx = 0; box_idx < STATION_SRC_SKY_COV_MAX; ++box_idx) sum += ILP_get_sol(prog, STA_SKY_COV_2, sta_idx, box_idx);
        HH_MSG("Sky coverage objective [%c%c]: %lf [%lf]", 
            sta->id[0], sta->id[1], 
            sum, sum / (double) STATION_SRC_SKY_COV_MAX);
    }
    const Station* sta_fst;
    const Station* sta_snd;
    ILP_sta_it(prog, sta_fst) {
        ILP_sta_it(prog, sta_snd) {
            if(sta_fst_idx >= sta_snd_idx) continue;
            HH_MSG("Baseline objective [%c%c-%c%c, %lf]: %lf", 
                sta_fst->id[0], sta_fst->id[1], 
                sta_snd->id[0], sta_snd->id[1], 
                baseline_dist[baseline_index(prog, sta_fst_idx, sta_snd_idx)],
                ILP_get_sol(prog, OBJ_BASELINE_2, sta_fst_idx, sta_snd_idx));
        }
    }
}

void
sched_build(ILP* const prog, Sched* const out) {
    const Station* sta_fst;
    const Station* sta_snd;
    const Source* src;
    bool* sta_pushed;
    HH_CALLOC(sta_pushed, sizeof(bool) * prog->count_sta);
    for(size_t seg = 0; seg < prog->count_seg; ++seg) {
        ILP_src_it(prog, src) {
            Sched_push_begin(out, seg, src);
            ILP_sta_it(prog, sta_fst) {
                ILP_sta_it(prog, sta_snd) {
                    if(sta_fst_idx >= sta_snd_idx) continue;
                    if(ILP_get_sol(prog, BASELINE_2, seg, src_idx, sta_fst_idx, sta_snd_idx) > 0.5) {
                        if(!sta_pushed[sta_fst_idx]) {
                            Sched_push(out, sta_fst);
                            sta_pushed[sta_fst_idx] = true;
                        }
                        if(!sta_pushed[sta_snd_idx]) {
                            Sched_push(out, sta_snd);
                            sta_pushed[sta_snd_idx] = true;
                        }
                    }
                }
            }
            Sched_push_end(out);
            memset(sta_pushed, 0, sizeof(bool) * prog->count_sta);
        }
    }
    free(sta_pushed);
}

void
sched_validate(const ILP* const prog) {
    const Station* sta_fst;
    const Station* sta_snd;
    const Source* src;
    size_t active;
    for(size_t seg = 0; seg < prog->count_seg; ++seg) {
        ILP_sta_it(prog, sta_fst) {
            ILP_sta_it(prog, sta_snd) {
                if(sta_fst_idx >= sta_snd_idx) continue;
                active = 0;
                ILP_src_it(prog, src) {
                    if(ILP_get_sol(prog, BASELINE_2, seg, src_idx, sta_fst_idx, sta_snd_idx) > 0.5) active++;
                }
                HH_ASSERT(active < 2, "Two identical baselines are active simultaneously.");
            }
        }
    }
    (void) sta_fst;
    (void) sta_snd;
}

SCHED_IMPL(SCHED_EXPR) {
    ILP prog;
    ILP_init(&prog);
    HH_DBG("Initialized ILP.");
    // physical constraints
    constr_exclusion(&prog);
    bool* viewable;
    constr_viewable(&prog, &viewable);
    constr_slew(&prog, viewable);
    free(viewable);
    // objective constraints
    constr_sky_cov(&prog);
    constr_baseline(&prog);
    // objective
    double* baseline_dist = obj(&prog);
    // solve model
    ILP_param_int(&prog, "MIPFocus", 3);
    ILP_param_int(&prog, "Cuts", 2);
    ILP_param_double(&prog, "Heuristics", 0.05);
    ILP_param_int(&prog, "PreSolve", 2);
    if(!ILP_solve(&prog)) {
        ILP_free(&prog);
        return false;
    }
    // dump objective values
    obj_dump(&prog, baseline_dist);
    free(baseline_dist);
    // build schedule
    sched_validate(&prog);
    sched_build(&prog, out);
    Sched_dump(out);
    // clean up
    ILP_free(&prog);
    return true;
}

#if 0
#if 0
    for(size_t seg = 0; seg < prog.count_seg; ++seg) {
        ILP_src_it(&prog, src) {
            ILP_sta_it(&prog, sta_fst) {
                ILP_sta_it(&prog, sta_snd) {
                    if (sta_fst_idx >= sta_snd_idx) continue;
                    ILP_sta_it(&prog, sta_thd) {
                        if (sta_snd_idx >= sta_thd_idx) continue;
                        row_begin(&prog);
                        row_set(&prog,  1.0, BASELINE_2, seg, src_idx, sta_snd_idx, sta_thd_idx);
                        row_set(&prog, -1.0, BASELINE_2, seg, src_idx, sta_fst_idx, sta_snd_idx);
                        row_set(&prog, -1.0, BASELINE_2, seg, src_idx, sta_fst_idx, sta_thd_idx);
                        row_end_as_constr(&prog, '>', -1.0);
                        count++;
                        row_begin(&prog);
                        row_set(&prog,  1.0, BASELINE_2, seg, src_idx, sta_fst_idx, sta_thd_idx);
                        row_set(&prog, -1.0, BASELINE_2, seg, src_idx, sta_fst_idx, sta_snd_idx);
                        row_set(&prog, -1.0, BASELINE_2, seg, src_idx, sta_snd_idx, sta_thd_idx);
                        row_end_as_constr(&prog, '>', -1.0);
                        count++;
                        row_begin(&prog);
                        row_set(&prog,  1.0, BASELINE_2, seg, src_idx, sta_fst_idx, sta_snd_idx);
                        row_set(&prog, -1.0, BASELINE_2, seg, src_idx, sta_fst_idx, sta_thd_idx);
                        row_set(&prog, -1.0, BASELINE_2, seg, src_idx, sta_snd_idx, sta_thd_idx);
                        row_end_as_constr(&prog, '>', -1.0);
                        count++;
                    }
                }
            }
        }
    }
#else
    const Station *sta_a1;
    const Station *sta_a2;
    const Station *sta_i, *sta_j, *sta_k, *sta_l;
    for (size_t seg = 0; seg < prog.count_seg; ++seg) {
        ILP_src_it(&prog, src) {
            ILP_sta_it(&prog, sta_i) {
                ILP_sta_it(&prog, sta_j) {
                    if (sta_i_idx >= sta_j_idx) continue;
                    ILP_sta_it(&prog, sta_k) {
                        ILP_sta_it(&prog, sta_l) {
                            if (sta_k_idx >= sta_l_idx) continue;
                            // skip the baseline itself
                            if ((sta_k_idx == sta_i_idx && sta_l_idx == sta_j_idx) ||
                                (sta_k_idx == sta_j_idx && sta_l_idx == sta_i_idx)) continue;

                            // only consider baselines containing i or j
                            bool shares_i = (sta_k_idx == sta_i_idx || sta_l_idx == sta_i_idx);
                            bool shares_j = (sta_k_idx == sta_j_idx || sta_l_idx == sta_j_idx);
                            if (!shares_i && !shares_j) continue;

                            // determine the second baseline that shares the other station
                            size_t other_sta = 0;
                            if (shares_i && shares_j) continue; // skip baselines that share both stations
                            if (shares_i) {
                                // the baseline should be between j and the other station in (k,l)
                                other_sta = (sta_k_idx == sta_i_idx) ? sta_l_idx : sta_k_idx;
                                row_begin(&prog);
                                row_set(&prog, 1.0, BASELINE_2, seg, src_idx, sta_i_idx, sta_j_idx);
                                row_set(&prog, -1.0, BASELINE_2, seg, src_idx, (other_sta < sta_i_idx) ? other_sta : sta_i_idx, (other_sta < sta_i_idx) ? sta_i_idx : other_sta);
                                row_set(&prog, -1.0, BASELINE_2, seg, src_idx, (other_sta < sta_j_idx) ? other_sta : sta_j_idx, (other_sta < sta_j_idx) ? sta_j_idx : other_sta);
                                row_end_as_constr(&prog, '>', -1.0);
                                count++;
                            } else if (shares_j) {
                                // the baseline should be between i and the other station in (k,l)
                                other_sta = (sta_k_idx == sta_j_idx) ? sta_l_idx : sta_k_idx;
                                row_begin(&prog);
                                row_set(&prog, 1.0, BASELINE_2, seg, src_idx, sta_i_idx, sta_j_idx);
                                row_set(&prog, -1.0, BASELINE_2, seg, src_idx, (other_sta < sta_j_idx) ? other_sta : sta_j_idx, (other_sta < sta_j_idx) ? sta_j_idx : other_sta);
                                row_set(&prog, -1.0, BASELINE_2, seg, src_idx, (other_sta < sta_i_idx) ? other_sta : sta_i_idx, (other_sta < sta_i_idx) ? sta_i_idx : other_sta);
                                row_end_as_constr(&prog, '>', -1.0);
                                count++;
                            }
                        }
                    }
                }
            }
        }
    }
    (void) sta_i; (void) sta_j; (void) sta_k; (void) sta_l;
    (void) sta_a1;
    (void) sta_a2;
#endif
    HH_DBG("Added %zu constraints: Force baselines where stations are already involved in a scan.", count);
#endif

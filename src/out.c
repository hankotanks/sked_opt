#include "out.h"

#include <X11/Xlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "hh.h"

#include "map.h"
#include "meta.h"
#include "sched.h"
#include "station.h"
#include "time_sys.h"

void
generate_schedule(const Sched* const out) {
    // TODO: Implement
    (void) out;
}

static char OUT_H__TEMP[256];
#define ADD_FIELD(field_fmt_, ...) do { \
        snprintf(OUT_H__TEMP, sizeof(OUT_H__TEMP), field_fmt_, __VA_ARGS__); \
        hh_strput(fields, OUT_H__TEMP); \
        hh_strput(fields, ","); \
    } while(0)

#define ADD_VALUE(value_fmt_, ...) do { \
        snprintf(OUT_H__TEMP, sizeof(OUT_H__TEMP), value_fmt_, __VA_ARGS__); \
        hh_strput(values, OUT_H__TEMP); \
        hh_strput(values, ","); \
    } while(0)

inline size_t 
baseline_count(size_t count_sta) {
    return count_sta * (count_sta - 1) / 2;
}

inline size_t 
baseline_index_inner(const struct map* const map, size_t sta_a, size_t sta_b) {
    HH_ASSERT(sta_a < sta_b, "Unreachable!");
    return sta_a * (2 * map->count_sta - sta_a - 1) / 2 + (sta_b - sta_a - 1);
}

inline size_t 
baseline_index(const struct map* const map, size_t sta_a, size_t sta_b) {
    return baseline_index_inner(map, HH_MIN(sta_a, sta_b), HH_MAX(sta_a, sta_b));
}

struct subnet {
    size_t target;
    bool* obs, ext, single_source;
};

bool
subnet_contains_scan(const struct map* const map, const struct subnet* const sub, const Scan* const scan) {
    if(sub->target != scan->target) return false;
    const Station* sta;
    for(size_t i = 0; i < hh_arrlen(scan->sta); ++i) {
        map_sta_it(map, sta) {
            if(scan->sta[i] == sta_idx) continue;
            if(sub->obs[baseline_index(map, scan->sta[i], sta_idx)]) {
                return true;
            }
        }
    }
    (void) sta;
    return false;
}

bool
subnet_extend(const struct map* const map, struct subnet* const sub, const Scan* const scan) {
    bool* curr, single_source = true;
    for(size_t j = 0; j < hh_arrlen(scan->sta); ++j) {
        for(size_t k = 0; k < hh_arrlen(scan->sta); ++k) {
            if(scan->sta[j] == scan->sta[k]) continue;
            curr = &sub->obs[baseline_index(map, scan->sta[j], scan->sta[k])];
            if(!(*curr)) single_source = false;
            (*curr) = true;
        }
    }   
    return single_source;
}

static unsigned int SKY_COV_DUR[] = { 30, 60 }; // in minutes

struct stats_sky_cov {
    double a13[sizeof(SKY_COV_DUR) / sizeof(unsigned int)];
    double a25[sizeof(SKY_COV_DUR) / sizeof(unsigned int)];
    double a37[sizeof(SKY_COV_DUR) / sizeof(unsigned int)];
};

bool
scan_contains_sta(const Scan* const scan, size_t sta_idx) {
    for(size_t i = 0; i < hh_arrlen(scan->sta); ++i) {
        if(scan->sta[i] == sta_idx) return true;
    }
    return false;
}

double
sky_cov_generic(const Sched* const out, const Station* const sta, unsigned int seconds, size_t count_cells,
    Station_sky_cov_idx v1, Station_sky_cov_idx v2) {
    double score = 0.0;
    const struct map* map = Sched_map(out);
    size_t sta_idx = map_sta_idx(map, sta);
    size_t seg = 0;
    size_t count_scans;
    size_t count_win = 0;
    bool* v1_hit;
    bool* v2_hit;
    HH_MALLOC(v1_hit, sizeof(bool) * count_cells);
    HH_MALLOC(v2_hit, sizeof(bool) * count_cells);
    size_t v1_score;
    size_t v2_score;
    Scan* scans = NULL;
    for(unsigned int t_start = 0, t_end, t_seg; t_start < TIME_SYS->duration; t_start += seconds / 2) {
        t_end = (t_start += seconds);
        memset(v1_hit, 0, sizeof(bool) * count_cells);
        memset(v2_hit, 0, sizeof(bool) * count_cells);
        while(seg < map->count_seg && seg *  TIME_SYS->scan_length < t_end) {
            t_seg = (unsigned int) seg * TIME_SYS->scan_length;
            count_scans = Sched_get(out, seg, &scans);
            for(size_t i = 0; i < count_scans; ++i) {
                if(scan_contains_sta(&scans[i], sta_idx)) {
                    v1_hit[v1(sta, map_src_get(map, scans[i].target), t_seg)] = true;
                    v1_hit[v1(sta, map_src_get(map, scans[i].target), t_seg + TIME_SYS->scan_length)] = true;
                    v2_hit[v2(sta, map_src_get(map, scans[i].target), t_seg)] = true;
                    v2_hit[v2(sta, map_src_get(map, scans[i].target), t_seg + TIME_SYS->scan_length)] = true;
                }
            }
            seg++;
        }
        v1_score = 0;
        v2_score = 0;
        for(size_t i = 0; i < count_cells; ++i) {
            v1_score += (size_t) v1_hit[i];
            v2_score += (size_t) v2_hit[i];
        }
        score += (((double) v1_score / 2.0) + ((double) v2_score / 2.0)) / (double) count_cells;
        count_win++;
    }
    free(v1_hit);
    free(v2_hit);
    return score / (double) count_win;
}

double
sky_cov_a13(const Sched* const out, const Station* const sta, unsigned int seconds) {
    return sky_cov_generic(out, sta, seconds, 13, 
        Station_sky_cov_idx_13v1, Station_sky_cov_idx_13v2);
}

double
sky_cov_a25(const Sched* const out, const Station* const sta, unsigned int seconds) {
    return sky_cov_generic(out, sta, seconds, 25, 
        Station_sky_cov_idx_25v1, Station_sky_cov_idx_25v2);
}

double
sky_cov_a37(const Sched* const out, const Station* const sta, unsigned int seconds) {
    return sky_cov_generic(out, sta, seconds, 37, 
        Station_sky_cov_idx_37v1, Station_sky_cov_idx_37v2);
}

struct stats_sta {
    size_t scans;
    size_t obs;
    struct stats_sky_cov sky_cov;
    unsigned int sec_observation;
    unsigned int sec_preob;
    unsigned int sec_slew;
    unsigned int sec_idle;
    unsigned int sec_field_system;
    double percent_observation;
    double percent_preob;
    double percent_slew;
    double percent_idle;
    double percent_field_system;
};

struct stats_src {
    size_t scans;
    size_t obs;
};

struct stats {
    size_t n_single_source_scans;
    size_t n_subnetting_scans;
    size_t n_observations;
    struct stats_sta* n_sta;
    struct stats_src* n_src;
    size_t* n_bl_obs;
    struct stats_sky_cov avg_sky_cov;
    size_t* n_station_scans;
    double avg_percent_observation;
    double avg_percent_preob;
    double avg_percent_slew;
    double avg_percent_idle;
    double avg_percent_field_system;
};

void
subnet_free(const struct map* const map, struct subnet* const sub, struct stats* const stats) {
    bool* mask;
    HH_CALLOC(mask, sizeof(bool) * map->count_sta);
    const Station* bln_a;
    const Station* bln_b;
    map_bln_it(map, bln_a, bln_b) {
        if(sub->obs[baseline_index(map, bln_a_idx, bln_b_idx)]) {
            stats->n_observations++;
            stats->n_sta[bln_a_idx].obs++;
            stats->n_sta[bln_b_idx].obs++;
            stats->n_bl_obs[baseline_index(map, bln_a_idx, bln_b_idx)]++;
            stats->n_src[sub->target].obs++;
            mask[bln_a_idx] = true;
            mask[bln_b_idx] = true;
        }
    }
    stats->n_src[sub->target].scans++;
    size_t participants = 0;
    for(size_t j = 0; j < map->count_sta; ++j) {
        if(mask[j]) {
            stats->n_sta[j].scans++;
            participants++;
        }
    }
    HH_ASSERT(participants > 1, "Unreachable!");
    stats->n_station_scans[participants - 2]++;
    if(sub->single_source) stats->n_single_source_scans++;
    else stats->n_subnetting_scans++;
    free(mask);
    free(sub->obs);
    (void) bln_a;
    (void) bln_b;
}

void
stats_compute(const Sched* const out, struct stats* stats) {
    const struct map* map = Sched_map(out);
    // clear stats
    stats->n_single_source_scans = 0;
    stats->n_subnetting_scans = 0;
    stats->n_observations = 0;
    HH_CALLOC(stats->n_sta, sizeof(struct stats_sta) * map->count_sta);
    HH_CALLOC(stats->n_src, sizeof(struct stats_src) * map->count_src);
    HH_CALLOC(stats->n_bl_obs, sizeof(size_t) * baseline_count(map->count_sta));
    HH_CALLOC(stats->n_station_scans, sizeof(size_t) * (map->count_sta - 1));
    // dynamic array of current scans
    Scan* scans = NULL;
    // list of current subnets
    struct subnet* subnet_list = NULL, subnet_curr;
    // indicates if the current scan is part of an active subnet
    bool found;
    for(size_t i = 0, j, k, count_scans; i < map->count_seg; ++i) {
        // remove extension flag from all subnets
        for(j = 0; j < hh_arrlen(subnet_list); ++j) 
            subnet_list[j].ext = false;
        // get current scans
        count_scans = Sched_get(out, i, &scans);
        if(count_scans) {
            // for each current scan
            // check if it shares a station and a source with any previous scans
            // if it does, append it to that subnet
            for(j = 0; j < count_scans; ++j) {
                found = false;
                for(k = 0; k < hh_arrlen(subnet_list); ++k) {
                    if(subnet_contains_scan(map, &subnet_list[k], &scans[j])) {
                        if(!subnet_extend(map, &subnet_list[k], &scans[j])) 
                            subnet_list[k].single_source = false;
                        subnet_list[k].ext = true;
                        found = true;
                        break;
                    }
                }
                if(!found) {
                    subnet_curr.target = scans[j].target;
                    subnet_curr.ext = true;
                    subnet_curr.single_source = true;
                    HH_CALLOC(subnet_curr.obs, sizeof(bool) * baseline_count(map->count_sta));
                    subnet_extend(map, &subnet_curr, &scans[j]);
                    hh_arrput(subnet_list, subnet_curr);
                }
            }
            // remove all subnets that werent extended
            for(j = 0; j < hh_arrlen(subnet_list); ++j) {
                if(!subnet_list[j].ext) {
                    subnet_free(map, &subnet_list[j], stats);
                    // remove from list
                    if(&subnet_list[j] == &hh_arrlast(subnet_list)) {
                        (void) hh_arrpop(subnet_list);
                    } else {
                        subnet_curr = hh_arrpop(subnet_list);
                        subnet_list[j] = subnet_curr;
                    }
                }
            }
        } else {
            // if there are no scans in the segment, we clear the active subnet list
            for(j = 0; j < hh_arrlen(subnet_list); ++j) {
                subnet_free(map, &subnet_list[j], stats);
            }
            hh_arrclear(subnet_list);
        }
    }
    // clear all remaining subnets
    for(size_t i = 0; i < hh_arrlen(subnet_list); ++i) {
        subnet_free(map, &subnet_list[i], stats);
    }
    // clean up
    hh_arrfree(subnet_list);
    // sum of per-station time
    const Station* sta;
    const enum station_state* sta_activity;
    map_sta_it(map, sta) {
        sta_activity = Sched_get_activity(out, sta);
        for(size_t i = 0; i < hh_arrlen(sta_activity); ++i) {
            // TODO: Not considering preob and field_system time
            switch(sta_activity[i]) {
                case STATE_IDLE: 
                    stats->n_sta[sta_idx].sec_idle += TIME_SYS->scan_length; 
                    break;
                case STATE_SLEW:
                    stats->n_sta[sta_idx].sec_slew += TIME_SYS->scan_length; 
                    break;
                case STATE_SCAN:
                    stats->n_sta[sta_idx].sec_observation += TIME_SYS->scan_length; 
                    break;
                case STATE_FAIL:
                default: HH_UNREACHABLE;
            }
        }
    }
    // compute per-station percentage time spent
    map_sta_it(map, sta) {
        stats->n_sta[sta_idx].percent_observation = (double) stats->n_sta[sta_idx].sec_observation / (double) TIME_SYS->duration;
        stats->n_sta[sta_idx].percent_preob = (double) stats->n_sta[sta_idx].sec_preob / (double) TIME_SYS->duration;
        stats->n_sta[sta_idx].percent_slew = (double) stats->n_sta[sta_idx].sec_slew / (double) TIME_SYS->duration;
        stats->n_sta[sta_idx].percent_idle = (double) stats->n_sta[sta_idx].sec_idle / (double) TIME_SYS->duration;
        stats->n_sta[sta_idx].percent_field_system = (double) stats->n_sta[sta_idx].sec_field_system / (double) TIME_SYS->duration;
        stats->n_sta[sta_idx].percent_observation *= 100.0;
        stats->n_sta[sta_idx].percent_preob *= 100.0;
        stats->n_sta[sta_idx].percent_slew *= 100.0;
        stats->n_sta[sta_idx].percent_idle *= 100.0;
        stats->n_sta[sta_idx].percent_field_system *= 100.0;
    }
    // average time spent in each mode
    double sum;
    // observation
    sum = 0.0;
    map_sta_it(map, sta) sum += stats->n_sta[sta_idx].percent_observation;
    stats->avg_percent_observation = sum / (double) map->count_sta;
    // preob
    sum = 0.0;
    map_sta_it(map, sta) sum += stats->n_sta[sta_idx].percent_preob;
    stats->avg_percent_preob = sum / (double) map->count_sta;
    // slew
    sum = 0.0;
    map_sta_it(map, sta) sum += stats->n_sta[sta_idx].percent_slew;
    stats->avg_percent_slew = sum / (double) map->count_sta;
    // idle
    sum = 0.0;
    map_sta_it(map, sta) sum += stats->n_sta[sta_idx].percent_idle;
    stats->avg_percent_idle = sum / (double) map->count_sta;
    // field_system
    sum = 0.0;
    map_sta_it(map, sta) sum += stats->n_sta[sta_idx].percent_field_system;
    stats->avg_percent_field_system = sum / (double) map->count_sta;
    // sky coverage calculation
    map_sta_it(map, sta) {
        for(size_t i = 0; i < (sizeof(SKY_COV_DUR) / sizeof(unsigned int)); ++i) {
            stats->n_sta[sta_idx].sky_cov.a13[i] = sky_cov_a13(out, sta, SKY_COV_DUR[i] * 60);
            stats->n_sta[sta_idx].sky_cov.a25[i] = sky_cov_a25(out, sta, SKY_COV_DUR[i] * 60);
            stats->n_sta[sta_idx].sky_cov.a37[i] = sky_cov_a37(out, sta, SKY_COV_DUR[i] * 60);
        }
    }
    // average sky coverage calculation
    for(size_t i = 0; i < (sizeof(SKY_COV_DUR) / sizeof(unsigned int)); ++i) {
        stats->avg_sky_cov.a13[i] = 0.0;
        stats->avg_sky_cov.a25[i] = 0.0;
        stats->avg_sky_cov.a37[i] = 0.0;
        map_sta_it(map, sta) {
            stats->avg_sky_cov.a13[i] += stats->n_sta[sta_idx].sky_cov.a13[i];
            stats->avg_sky_cov.a25[i] += stats->n_sta[sta_idx].sky_cov.a25[i];
            stats->avg_sky_cov.a37[i] += stats->n_sta[sta_idx].sky_cov.a37[i];
        }
        stats->avg_sky_cov.a13[i] /= (double) map->count_sta;
        stats->avg_sky_cov.a25[i] /= (double) map->count_sta;
        stats->avg_sky_cov.a37[i] /= (double) map->count_sta;
    }
}

void
generate_statistics(const Sched* const out) {
    const struct map* map = Sched_map(out);
    Sched_dump(out);
    // two buffers, one for fields and one for values
    char* fields = NULL;
    char* values = NULL;
    // general
    struct stats stats;
    stats_compute(out, &stats);
    ADD_FIELD("version",               NULL); ADD_VALUE("0",   NULL);
    ADD_FIELD("n_scans",               NULL); ADD_VALUE("%zu", stats.n_single_source_scans + stats.n_subnetting_scans);
    ADD_FIELD("n_single_source_scans", NULL); ADD_VALUE("%zu", stats.n_single_source_scans);
    ADD_FIELD("n_subnetting_scans",    NULL); ADD_VALUE("%zu", stats.n_subnetting_scans);
    ADD_FIELD("n_fillin-mode_scans",   NULL); ADD_VALUE("0",   NULL);
    ADD_FIELD("n_calibrator_scans",    NULL); ADD_VALUE("0",   NULL);
    ADD_FIELD("n_observations",        NULL); ADD_VALUE("%zu", stats.n_observations);
    ADD_FIELD("n_stations",            NULL); ADD_VALUE("%zu", map->count_sta);
    ADD_FIELD("n_sources",             NULL); ADD_VALUE("%zu", map->count_src);
    // time_average
    ADD_FIELD("time_average_observation",  NULL); ADD_VALUE("%lf", stats.avg_percent_observation);
    ADD_FIELD("time_average_preob",        NULL); ADD_VALUE("%lf", stats.avg_percent_preob);
    ADD_FIELD("time_average_slew",         NULL); ADD_VALUE("%lf", stats.avg_percent_slew);
    ADD_FIELD("time_average_idle",         NULL); ADD_VALUE("%lf", stats.avg_percent_idle);
    ADD_FIELD("time_average_field_system", NULL); ADD_VALUE("%lf", stats.avg_percent_field_system);
    // sky-coverage_average
    for(size_t i = 0; i < (sizeof(SKY_COV_DUR) / sizeof(unsigned int)); ++i) {
        ADD_FIELD("sky-coverage_average_13_areas_%u_min", SKY_COV_DUR[i]);
        ADD_VALUE("%lf", stats.avg_sky_cov.a13[i]);
        ADD_FIELD("sky-coverage_average_25_areas_%u_min", SKY_COV_DUR[i]);
        ADD_VALUE("%lf", stats.avg_sky_cov.a25[i]);
        ADD_FIELD("sky-coverage_average_37_areas_%u_min", SKY_COV_DUR[i]);
        ADD_VALUE("%lf", stats.avg_sky_cov.a37[i]);
    }
    // weight_factor
    ADD_FIELD("weight_factor_sky_coverage",                 NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_number_of_observations",       NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_duration",                     NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_average_sources",              NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_average_stations",             NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_average_baselines",            NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_idle_time",                    NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_idle_time_interval",           NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_closures",                     NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_max_closures",                 NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_low_declination",              NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_low_declination_start_weight", NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_low_declination_full_weight",  NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_low_elevation",                NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_low_elevation_start_weight",   NULL); ADD_VALUE("%lf", 0.0);
    ADD_FIELD("weight_factor_low_elevation_full_weight",    NULL); ADD_VALUE("%lf", 0.0);
    // time_sta_observation
    const Station* sta;
    map_sta_it(map, sta) {
        ADD_FIELD("time_%.*s_observation", (int) cat_name_len(sta->name), sta->name);
        ADD_VALUE("%lf", stats.n_sta[sta_idx].percent_observation);
    }
    // time_sta_preob
    map_sta_it(map, sta) {
        ADD_FIELD("time_%.*s_preob", (int) cat_name_len(sta->name), sta->name);
        ADD_VALUE("%lf", stats.n_sta[sta_idx].percent_preob);
    }
    // time_sta_slew
    map_sta_it(map, sta) {
        ADD_FIELD("time_%.*s_slew", (int) cat_name_len(sta->name), sta->name);
        ADD_VALUE("%lf", stats.n_sta[sta_idx].percent_slew);
    }
    // time_sta_idle
    map_sta_it(map, sta) {
        ADD_FIELD("time_%.*s_idle", (int) cat_name_len(sta->name), sta->name);
        ADD_VALUE("%lf", stats.n_sta[sta_idx].percent_idle);
    }
    // time_sta_field_system
    map_sta_it(map, sta) {
        ADD_FIELD("time_%.*s_field_system", (int) cat_name_len(sta->name), sta->name);
        ADD_VALUE("%lf", stats.n_sta[sta_idx].percent_field_system);
    }
    // sky-coverage
    for(size_t i = 0; i < (sizeof(SKY_COV_DUR) / sizeof(unsigned int)); ++i) {
        map_sta_it(map, sta) {
            ADD_FIELD("sky-coverage_%.*s_13_areas_%u_min", (int) cat_name_len(sta->name), sta->name, SKY_COV_DUR[i]);
            ADD_VALUE("%lf", stats.n_sta[sta_idx].sky_cov.a13[i]);
        }
        map_sta_it(map, sta) {
            ADD_FIELD("sky-coverage_%.*s_25_areas_%u_min", (int) cat_name_len(sta->name), sta->name, SKY_COV_DUR[i]);
            ADD_VALUE("%lf", stats.n_sta[sta_idx].sky_cov.a25[i]);
        }
        map_sta_it(map, sta) {
            ADD_FIELD("sky-coverage_%.*s_37_areas_%u_min", (int) cat_name_len(sta->name), sta->name, SKY_COV_DUR[i]);
            ADD_VALUE("%lf", stats.n_sta[sta_idx].sky_cov.a37[i]);
        }
    }
    // n_sta
    map_sta_it(map, sta) {
        ADD_FIELD("n_sta_scans_%.*s", (int) cat_name_len(sta->name), sta->name);
        ADD_VALUE("%zu", stats.n_sta[sta_idx].scans);
    }
    map_sta_it(map, sta) {
        ADD_FIELD("n_sta_obs_%.*s", (int) cat_name_len(sta->name), sta->name);
        ADD_VALUE("%zu", stats.n_sta[sta_idx].obs);
    }
    // n_bl_obs
    const Station* bln_a;
    const Station* bln_b;
    map_bln_it(map, bln_a, bln_b) {
        ADD_FIELD("n_bl_obs_%c%c-%c%c", bln_a->id[0], bln_a->id[1], bln_b->id[0], bln_b->id[1]);
        ADD_VALUE("%zu", stats.n_bl_obs[baseline_index(map, bln_a_idx, bln_b_idx)]);
    }
    // n_src
    const Source* src;
    map_src_it(map, src) {
        ADD_FIELD("n_src_scans_%.*s", (int) cat_name_len(src->name), src->name);
        ADD_VALUE("%zu", stats.n_src[src_idx].scans);
    }
    map_src_it(map, src) {
        ADD_FIELD("n_src_obs_%.*s", (int) cat_name_len(src->name), src->name);
        ADD_VALUE("%zu", stats.n_src[src_idx].obs);
    }
    // n_src_closure_phases
    map_src_it(map, src) {
        ADD_FIELD("n_src_closure_phases_%.*s", (int) cat_name_len(src->name), src->name);
        ADD_VALUE("%d", 0); // TODO
    }
    // n_src_closures
    map_src_it(map, src) {
        ADD_FIELD("n_src_closures_%.*s", (int) cat_name_len(src->name), src->name);
        ADD_VALUE("%d", 0); // TODO
    }
    // station_scans
    for(size_t i = 0; i < (map->count_sta - 1); ++i) {
        ADD_FIELD("%zu-station_scans", i + 2);
        ADD_VALUE("%zu", stats.n_station_scans[i]);
    }
    // write results to file
    char* path = hh_path_join(hh_path(META->path_parent), meta_file_stat());
    FILE* file = fopen(path, "w");
    HH_ASSERT(file, "Failed to write statistics to file [%s].", path);
    fprintf(file, "%s\n%s\n", fields, values);
    fclose(file);
    hh_arrfree(path);
}

#undef ADD_FIELD
#undef ADD_VALUE

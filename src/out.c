#include "out.h"

#include <X11/Xlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "hh.h"

#include "meta.h"
#include "sched.h"

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

struct stats_sta {
    size_t scans;
    size_t obs;
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
    for(size_t j = 0; j < map->count_sta; ++j) 
        stats->n_sta[j].scans += (size_t) mask[j];
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
    stats->n_single_source_scans = 0;
    stats->n_subnetting_scans = 0;
    stats->n_observations = 0;
    HH_CALLOC(stats->n_sta, sizeof(struct stats_sta) * map->count_sta);
    HH_CALLOC(stats->n_src, sizeof(struct stats_src) * map->count_src);
    HH_CALLOC(stats->n_bl_obs, sizeof(size_t) * baseline_count(map->count_sta));

    Scan* scans = NULL;
    Sched_get(out, 0, &scans);
    
    size_t count_scans = hh_arrlen(scans);

    struct subnet* subnet_list = NULL, subnet_curr;

    bool found;
    for(size_t seg = 0; seg < map->count_seg; ++seg) {
        // remove extension flag from all subnets
        for(size_t i = 0; i < hh_arrlen(subnet_list); ++i) subnet_list[i].ext = false;
        // get current scans
        count_scans = Sched_get(out, seg, &scans);
        if(count_scans) {
            // for each current scan
            // check if it shares a station and a source with any previous scans
            // if it does, append it to that subnet
            for(size_t i = 0; i < count_scans; ++i) {
                found = false;
                for(size_t j = 0; j < hh_arrlen(subnet_list); ++j) {
                    if(subnet_contains_scan(map, &subnet_list[j], &scans[i])) {
                        if(!subnet_extend(map, &subnet_list[j], &scans[i])) subnet_list[j].single_source = false;
                        subnet_list[j].ext = true;
                        found = true;
                        break;
                    }
                }
                if(!found) {
                    subnet_curr.target = scans[i].target;
                    subnet_curr.ext = true;
                    subnet_curr.single_source = true;
                    HH_CALLOC(subnet_curr.obs, sizeof(bool) * baseline_count(map->count_sta));
                    subnet_extend(map, &subnet_curr, &scans[i]);
                    hh_arrput(subnet_list, subnet_curr);
                }
            }
            // remove all subnets that werent extended
            for(size_t i = 0; i < hh_arrlen(subnet_list); ++i) {
                if(!subnet_list[i].ext) {
                    subnet_free(map, &subnet_list[i], stats);
                    // remove from list
                    if(&subnet_list[i] == &hh_arrlast(subnet_list)) {
                        (void) hh_arrpop(subnet_list);
                    } else {
                        subnet_curr = hh_arrpop(subnet_list);
                        subnet_list[i] = subnet_curr;
                    }
                }
            }
        } else {
            // if there are no scans in the segment, we clear the active subnet list
            for(size_t i = 0; i < hh_arrlen(subnet_list); ++i) {
                subnet_free(map, &subnet_list[i], stats);
            }
            hh_arrclear(subnet_list);
        }
    }
    for(size_t i = 0; i < hh_arrlen(subnet_list); ++i) {
        subnet_free(map, &subnet_list[i], stats);
    }
    hh_arrfree(subnet_list);
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
    ADD_FIELD("time_average_observation",  NULL); ADD_VALUE("0", NULL); // TODO
    ADD_FIELD("time_average_preob",        NULL); ADD_VALUE("0", NULL); // TODO
    ADD_FIELD("time_average_slew",         NULL); ADD_VALUE("0", NULL); // TODO
    ADD_FIELD("time_average_idle",         NULL); ADD_VALUE("0", NULL); // TODO
    ADD_FIELD("time_average_field_system", NULL); ADD_VALUE("0", NULL); // TODO
    // sky-coverage_average
    size_t cell_num[] = { 13, 25, 37 };
    size_t cell_dur[] = { 30, 60 };
    for(size_t i = 0; i < (sizeof(cell_dur) / sizeof(cell_dur[0])); ++i) {
        for(size_t j = 0; j < (sizeof(cell_num) / sizeof(cell_num[0])); ++j) {
            ADD_FIELD("sky-coverage_average_%zu_areas_%zu_min", cell_num[j], cell_dur[i]);
            ADD_VALUE("%lf", 0.0); // TODO
        }
    }
    // weight_factor
    ADD_FIELD("weight_factor_sky_coverage",                 NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_number_of_observations",       NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_duration",                     NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_average_sources",              NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_average_stations",             NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_average_baselines",            NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_idle_time",                    NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_idle_time_interval",           NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_closures",                     NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_max_closures",                 NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_low_declination",              NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_low_declination_start_weight", NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_low_declination_full_weight",  NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_low_elevation",                NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_low_elevation_start_weight",   NULL); ADD_VALUE("%lf", 0.0); // TODO
    ADD_FIELD("weight_factor_low_elevation_full_weight",    NULL); ADD_VALUE("%lf", 0.0); // TODO
    // time_sta_observation
    const Station* sta;
    map_sta_it(map, sta) {
        ADD_FIELD("time_%.*s_observation", (int) cat_name_len(sta->name), sta->name);
        ADD_VALUE("%lf", 0.0); // TODO
    }
    // time_sta_preob
    map_sta_it(map, sta) {
        ADD_FIELD("time_%.*s_preob", (int) cat_name_len(sta->name), sta->name);
        ADD_VALUE("%lf", 0.0); // TODO
    }
    // time_sta_slew
    map_sta_it(map, sta) {
        ADD_FIELD("time_%.*s_slew", (int) cat_name_len(sta->name), sta->name);
        ADD_VALUE("%lf", 0.0); // TODO
    }
    // time_sta_idle
    map_sta_it(map, sta) {
        ADD_FIELD("time_%.*s_idle", (int) cat_name_len(sta->name), sta->name);
        ADD_VALUE("%lf", 0.0); // TODO
    }
    // time_sta_field_system
    map_sta_it(map, sta) {
        ADD_FIELD("time_%.*s_field_system", (int) cat_name_len(sta->name), sta->name);
        ADD_VALUE("%lf", 0.0); // TODO
    }
    // sky-coverage
    for(size_t i = 0; i < (sizeof(cell_dur) / sizeof(cell_dur[0])); ++i) {
        for(size_t j = 0; j < (sizeof(cell_num) / sizeof(cell_num[0])); ++j) {
            map_sta_it(map, sta) {
                ADD_FIELD("sky-coverage_%.*s_%zu_areas_%zu_min", (int) cat_name_len(sta->name), sta->name, cell_num[j], cell_dur[i]);
                ADD_VALUE("%lf", 0.0); // TODO
            }
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
    // n_bl_obs n_bl_obs_Ht-Ma
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
    // n_src_closure_phases
    map_src_it(map, src) {
        ADD_FIELD("n_src_closures_%.*s", (int) cat_name_len(src->name), src->name);
        ADD_VALUE("%d", 0); // TODO
    }
    // station_scans
    for(size_t i = 2; i <= map->count_sta; ++i) {
        ADD_FIELD("%zu-station_scans", i);
        ADD_VALUE("%d", 0); // TODO
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

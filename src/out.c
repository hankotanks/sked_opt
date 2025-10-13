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

static char OUT_H__ADD_FIELD_BUF[32];
#define ADD_FIELD(field_fmt_, field_, value_fmt_, value_) do { \
        if(field_ != NULL) snprintf(OUT_H__ADD_FIELD_BUF, sizeof(OUT_H__ADD_FIELD_BUF), "%s", (char*) field_); \
        else snprintf(OUT_H__ADD_FIELD_BUF, sizeof(OUT_H__ADD_FIELD_BUF), "%s", field_fmt_); \
        hh_strput(fields, OUT_H__ADD_FIELD_BUF); \
        hh_strput(fields, ","); \
        snprintf(OUT_H__ADD_FIELD_BUF, sizeof(OUT_H__ADD_FIELD_BUF), value_fmt_, value_); \
        hh_strput(values, OUT_H__ADD_FIELD_BUF); \
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

void
stats_general(const Sched* const out, size_t* n_single_source_scans, size_t* n_subnetting_scans, size_t* n_observations) {
    (*n_single_source_scans) = 0;
    (*n_subnetting_scans) = 0;
    (*n_observations) = 0;
    Scan* scans = NULL;
    Sched_get(out, 0, &scans);
    const struct map* map = Sched_map(out);
    size_t count_scans = hh_arrlen(scans);
    size_t count_baseline = baseline_count(map->count_sta);
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
                    // count observations
                    for(size_t j = 0; j < count_baseline; ++j) {
                        (*n_observations) += (size_t) subnet_list[i].obs[j];
                    }
                    // increment appropriate scan type
                    if(subnet_list[i].single_source) (*n_single_source_scans)++;
                    else (*n_subnetting_scans)++;
                    // remove from list
                    if(&subnet_list[i] == &hh_arrlast(subnet_list)) {
                        subnet_curr = hh_arrpop(subnet_list);
                        free(subnet_curr.obs);
                    } else {
                        subnet_curr = hh_arrpop(subnet_list);
                        free(subnet_list[i].obs);
                        subnet_list[i] = subnet_curr;
                    }
                }
            }
        } else {
            // if there are no scans in the segment, we clear the active subnet list
            for(size_t i = 0; i < hh_arrlen(subnet_list); ++i) {
                for(size_t j = 0; j < count_baseline; ++j) {
                    (*n_observations) += (size_t) subnet_list[i].obs[j];
                }
                if(subnet_list[i].single_source) (*n_single_source_scans)++;
                else (*n_subnetting_scans)++;
                free(subnet_list[i].obs);
            }
            hh_arrclear(subnet_list);
        }
    }
    for(size_t i = 0; i < hh_arrlen(subnet_list); ++i) {
        for(size_t j = 0; j < count_baseline; ++j) {
            (*n_observations) += (size_t) subnet_list[i].obs[j];
        }
        if(subnet_list[i].single_source) (*n_single_source_scans)++;
        else (*n_subnetting_scans)++;
        free(subnet_list[i].obs);
    }
    hh_arrfree(subnet_list);
}

void
generate_statistics(const Sched* const out) {
    Sched_dump(out);
    // two buffers, one for fields and one for values
    char* fields = NULL;
    char* values = NULL;
    //
    // general
    ADD_FIELD("version",               NULL, "%d",  0);
    size_t n_single_source_scans, n_subnetting_scans, n_observations;
    stats_general(out, &n_single_source_scans, &n_subnetting_scans, &n_observations);
    ADD_FIELD("n_scans",               NULL, "%zu", n_single_source_scans + n_subnetting_scans);
    ADD_FIELD("n_single_source_scans", NULL, "%zu", n_single_source_scans);
    ADD_FIELD("n_subnetting_scans",    NULL, "%zu", n_subnetting_scans);
    ADD_FIELD("n_fillin-mode_scans",   NULL, "%d",  0);
    ADD_FIELD("n_calibrator_scans",    NULL, "%d",  0);
    ADD_FIELD("n_observations",        NULL, "%zu", n_observations);
    ADD_FIELD("n_stations",            NULL, "%zu", Sched_map(out)->count_sta);
    ADD_FIELD("n_sources",             NULL, "%zu", Sched_map(out)->count_src);
    //
    // station
    // TODO
    // write results to file
    char* path = hh_path_join(hh_path(META->path_parent), meta_file_stat());
    FILE* file = fopen(path, "w");
    HH_ASSERT(file, "Failed to write statistics to file [%s].", path);
    fprintf(file, "%s\n%s\n", fields, values);
    fclose(file);
    hh_arrfree(path);
}

#undef ADD_FIELD

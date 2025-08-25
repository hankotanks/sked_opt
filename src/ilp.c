#include "ilp.h"

#include <limits.h>

#include "station.h"
#include "network.h"
#include "source.h"
#include "sky.h"
#include "time_sys.h"

#define SKY_COV_CELL_COUNT 13

void
Program_init(Program* ilp) {
    ilp->count_b = 0;
    ilp->count_k = 0;
    ilp->count_t = time_sys->duration / time_sys->scan_length;
    HH_CALLOC(ilp->map_sta_to_idx, sizeof(*(ilp->map_sta_to_idx)) * net->count);
    HH_CALLOC(ilp->map_src_to_idx, sizeof(*(ilp->map_src_to_idx)) * sky->count);
    bool* active;
    for(size_t i = 0; i < net->count; ++i) {
        Station sta;
        active = net_get_sta_by_idx(i, &sta);
        if(active != NULL && *active) ilp->map_sta_to_idx[i] = (ilp->count_b)++;
        else ilp->map_sta_to_idx[i] = SIZE_MAX;
    }
    for(size_t i = 0; i < sky->count; ++i) {
        Source src;
        active = sky_get_src_by_idx(i, &src);
        if(active != NULL && *active) ilp->map_src_to_idx[i] = (ilp->count_k)++;
        else ilp->map_src_to_idx[i] = SIZE_MAX;
    }
    HH_CALLOC(ilp->map_idx_to_sta, sizeof(*(ilp->map_idx_to_sta)) * ilp->count_b);
    HH_CALLOC(ilp->map_idx_to_src, sizeof(*(ilp->map_idx_to_src)) * ilp->count_k);
    for(size_t i = 0; i < net->count; ++i) {
        if(ilp->map_sta_to_idx[i] != SIZE_MAX) 
            ilp->map_idx_to_sta[ilp->map_sta_to_idx[i]] = i;
    }
    for(size_t i = 0; i < sky->count; ++i) {
        if(ilp->map_src_to_idx[i] != SIZE_MAX) 
            ilp->map_idx_to_src[ilp->map_src_to_idx[i]] = i;
    }
    // x: |K| * |T| * |B| --- if a station is active in a scan
    ilp->count_x = ilp->count_t * ilp->count_k * ilp->count_b;
    // y: |K| * |T| --- if a source is being targeted by a scan
    ilp->count_y = ilp->count_t * ilp->count_k;
    // z: 13 * |B| --- whether a sky coverage bucket has been hit
    ilp->count_z = ilp->count_b * SKY_COV_CELL_COUNT;
    HH_MALLOC(ilp->buf, sizeof(*(ilp->buf)) * (ilp->count_x + ilp->count_y + ilp->count_z));
    ilp->rec = make_lp(0, 0);
    HH_ASSERT(ilp->rec != NULL, "Failed to construct ILP.");
}

bool
Program_solve(const Program* const ilp) {
    return solve(ilp->rec) == OPTIMAL;
}

void
Program_free(Program* const ilp) {
    free(ilp->buf);
    free(ilp->map_sta_to_idx);
    free(ilp->map_src_to_idx);
    free(ilp->map_idx_to_sta);
    free(ilp->map_idx_to_src);
    delete_lp(ilp->rec);
}

void
Program_dump(const Program* const ilp) {
    HH_DBG("datetime start: %s", time_sys_text(0));
    HH_DBG("datetime final: %s", time_sys_text(time_sys->duration));
    HH_DBG("duration [s]: %u", time_sys->duration);
    HH_DBG("scan length [s]: %u", time_sys->scan_length);
    HH_DBG("scan count: %zu", ilp->count_t);
    HH_DBG("num stations: %zu", ilp->count_b);
    HH_DBG("num sources: %zu",  ilp->count_k);
    HH_DBG("sky coverage regions: %d", SKY_COV_CELL_COUNT);
    size_t count_x = ilp->count_t * ilp->count_k * ilp->count_b;
    HH_DBG("x count: %zu", count_x);
    size_t count_y = ilp->count_t * ilp->count_k;
    HH_DBG("y count: %zu", count_y);
    size_t count_z = ilp->count_b * SKY_COV_CELL_COUNT;
    HH_DBG("z count: %zu", count_z);
    HH_DBG("total variable count: %zu", ilp->count_x + ilp->count_y + ilp->count_z);
}

Station
Program_idx_to_sta(const Program* const ilp, size_t bi) {
    Station sta;
    bool* active = net_get_sta_by_idx(ilp->map_idx_to_sta[bi], &sta);
    if(active && (*active)) return sta;
    HH_UNREACHABLE;
}

Source
Program_idx_to_src(const Program* const ilp, size_t ki) {
    Source src;
    bool* active = sky_get_src_by_idx(ilp->map_idx_to_src[ki], &src);
    if(active && (*active)) return src;
    HH_UNREACHABLE;
}

void
row_begin(Program* const ilp) {
    memset(ilp->buf, 0, sizeof(*(ilp->buf)) * (ilp->count_x + ilp->count_y + ilp->count_z));
}

void
row_end_as_constr(Program* const ilp, int constr_type, double rhs) {
    add_constraint(ilp->rec, ilp->buf, constr_type, rhs);
}

void
row_end_as_obj(Program* const ilp) {
    set_obj_fn(ilp->rec, ilp->buf);
}

void
row_x(Program* const ilp, double coeff, size_t t, size_t k, size_t b) {
    row_xi(ilp, coeff, t, ilp->map_src_to_idx[k], ilp->map_sta_to_idx[b]);
}

void
row_y(Program* const ilp, double coeff, size_t t, size_t k) {
    row_yi(ilp, coeff, t, ilp->map_src_to_idx[k]);
}

void
row_z(Program* const ilp, double coeff, size_t b, size_t c) {
    row_zi(ilp, coeff, ilp->map_sta_to_idx[b], c);
}

void
row_xi(Program* const ilp, double coeff, size_t t, size_t ki, size_t bi) {
    ilp->buf[ \
        t * ilp->count_k * ilp->count_b + ki * ilp->count_b + bi] = coeff;
}

void
row_yi(Program* const ilp, double coeff, size_t t, size_t ki) {
    ilp->buf[ilp->count_x + \
        t * ilp->count_k + ki] = coeff;
}

void
row_zi(Program* const ilp, double coeff, size_t bi, size_t c) {
    ilp->buf[ilp->count_x + ilp->count_y + \
        bi * SKY_COV_CELL_COUNT + c] = coeff;
}

//
// declarations for different ILP configurations
//

void
Program_load_skycov(Program* const ilp) {
    // each station can only observe one source at a time
    // SchedulerILP.cpp:96
    for(size_t t = 0; t < ilp->count_t; ++t) {
        for(size_t bi = 0; bi < ilp->count_b; ++bi) {
            row_begin(ilp);
            for(size_t ki = 0; ki < ilp->count_k; ++ki) row_xi(ilp, 1.0, t, ki, bi);
            row_end_as_constr(ilp, LE, 1.0);
        }
    }
    
    // must be sufficient time to slew between two targets
    // SchedulerILP.cpp:110
    row_begin(ilp);
    // TODO
    row_end_as_constr(ilp, LE, 1.0);
    // a valid scan requires >= 2 participating stations
    // SchedulerILP.cpp:120
    for(size_t t = 0; t < ilp->count_t; ++t) {
        for(size_t ki = 0; ki < ilp->count_k; ++ki) {
            row_begin(ilp);
            for(size_t bi = 0; bi < ilp->count_b; ++bi) row_xi(ilp, 1.0, t, ki, bi);
            row_yi(ilp, (double) ilp->count_b * -1.0, t, ki);
            row_end_as_constr(ilp, LE, 0.0);
        }
    }
    // SchedulerILP.cpp:121
    for(size_t t = 0; t < ilp->count_t; ++t) {
        for(size_t ki = 0; ki < ilp->count_k; ++ki) {
            row_begin(ilp);
            for(size_t bi = 0; bi < ilp->count_b; ++bi) row_xi(ilp, 1.0, t, ki, bi);
            row_yi(ilp, -2.0, t, ki);
            row_end_as_constr(ilp, GE, 0.0);
        }
    }
    // keep sky coverage cells up to date
    // SchedulerILP.cpp:136
    row_begin(ilp);
    // TODO
    row_end_as_constr(ilp, LE, 0.0);
    // OBJECTIVE FUNCTION
    row_begin(ilp);
    // TODO
    row_end_as_obj(ilp);
    set_maxim(ilp->rec);
}

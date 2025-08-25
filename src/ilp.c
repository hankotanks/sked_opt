#include "ilp.h"

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
    HH_CALLOC(ilp->map_sta, sizeof(*(ilp->map_sta)) * net->count);
    HH_CALLOC(ilp->map_src, sizeof(*(ilp->map_src)) * sky->count);
    bool* active;
    for(size_t i = 0; i < net->count; ++i) {
        Station sta;
        active = net_get_sta_by_idx(i, &sta);
        if(active != NULL && *active) ilp->map_sta[i] = (ilp->count_b)++;
    }
    for(size_t i = 0; i < sky->count; ++i) {
        Source src;
        active = sky_get_src_by_idx(i, &src);
        if(active != NULL && *active) ilp->map_src[i] = (ilp->count_k)++;
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

void
Program_free(Program* const ilp) {
    free(ilp->buf);
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

void
constr_begin(Program* const ilp) {
    memset(ilp->buf, 0, sizeof(*(ilp->buf)) * (ilp->count_x + ilp->count_y + ilp->count_z));
}

void
constr_x(Program* const ilp, double coeff, size_t t, size_t k, size_t b) {
    ilp->buf[ \
        t * ilp->count_k * ilp->count_b + ilp->map_src[k] * ilp->count_b + ilp->map_sta[b]] = coeff;
}

void
constr_y(Program* const ilp, double coeff, size_t t, size_t k) {
    ilp->buf[ilp->count_x + \
        t * ilp->count_k + ilp->map_src[k]] = coeff;
}

void
constr_z(Program* const ilp, double coeff, size_t b, size_t c) {
    ilp->buf[ilp->count_x + ilp->count_y + \
        ilp->map_sta[b] * SKY_COV_CELL_COUNT + c] = coeff;
}

void
constr_end(Program* const ilp, int constr_type, double rhs) {
    add_constraint(ilp->rec, ilp->buf, constr_type, rhs);
}

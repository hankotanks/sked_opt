#ifndef ILP_H__
#define ILP_H__

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#include <lp_lib.h>

#include "station.h"
#include "network.h"
#include "source.h"
#include "sky.h"
#include "time_sys.h"

#include "ilp_fwd.h"

#define ILP_map_src_it(prog_, it_) \
    Source it_; \
    size_t it_##_idx = 0; \
    ILP_H__ILP_map_src_it_helper(prog_, &it_##_idx, &it_); \
    for(bool it_##_term = true; it_##_term; it_##_term = ILP_H__ILP_map_src_it_helper(prog_, &it_##_idx, &it_))

#define ILP_map_sta_it(prog_, it_) \
    Station it_; \
    net_get_sta((prog_)->map_sta, &it_); \
    for(size_t it_##_idx = 0; it_##_idx < (prog_)->count_sta; net_get_sta(&((prog_)->map_sta[(++it_##_idx) * 2]), &it_))

#if defined(__GNUC__) || defined(__clang__)
#define ILP_H__UNUSED __attribute__((unused))
#else
#define ILP_H__UNUSED
#endif

static void
ILP_map_sta_build(ILP* const prog);
static size_t
ILP_get_sta_idx(const ILP* const prog, const char id[static 2]);
static void
ILP_map_src_build(ILP* const prog);
static size_t
ILP_get_src_idx(const ILP* const prog, const char name[static 8]);
static void
ILP_init(ILP* const prog);
static void
ILP_free(ILP* prog);
static void
ILP_dump(const ILP* const prog);
static bool
ILP_solve(const ILP* const prog);
static void
row_begin(ILP* const prog);
static void
row_end_as_constr(ILP* const prog, int constr_type, double rhs);
static void
row_end_as_obj(ILP* const prog);
static void
row_set(ILP* const prog, double co, enum var_type ty, ...);
static double
row_get(const ILP* const prog, enum var_type ty, ...);

//
// Implementations
//

static void
ILP_map_sta_build(ILP* const prog) {
    prog->count_sta = 0;
    Station sta;
    bool* active;
    for(size_t i = 0; i < net->count; ++i) {
        active = net_get_sta_by_idx(i, &sta);
        if(active != NULL && *active) (prog->count_sta)++;
    }
    HH_CALLOC(prog->map_sta, prog->count_sta * 2 + 1);
    for(size_t i = 0, j = 0; i < net->count; ++i) {
        active = net_get_sta_by_idx(i, &sta);
        if(active != NULL && *active) {
            prog->map_sta[j++] = sta.id[0];
            prog->map_sta[j++] = sta.id[1];
        }
    }
}

static size_t ILP_H__UNUSED
ILP_get_sta_idx(const ILP* const prog, const char id[static 2]) {
    char needle[3] = { id[0], id[1], '\0' };
    const char* pos = strstr(prog->map_sta, needle);
    if(!pos) return SIZE_MAX;
    return (size_t) ((pos - prog->map_sta) / 2);
}

static void
ILP_map_src_build(ILP* const prog) {
    prog->count_src = 0;
    Source src;
    bool* active;
    for(size_t i = 0; i < sky->count; ++i) {
        active = sky_get_src_by_idx(i, &src);
        if(active != NULL && *active) (prog->count_src)++;
    }
    HH_CALLOC(prog->map_src, prog->count_src * 8 + 1);
    for(size_t i = 0, j = 0, k; i < sky->count; ++i) {
        active = sky_get_src_by_idx(i, &src);
        if(active != NULL && *active) {
            k = hh_strnlen(src.name, 8);
            memcpy(prog->map_src + j * 8, src.name, k);
            for(; k < 8; ++k) prog->map_src[j * 8 + k] = ' ';
            j++;
        }
    }
}

static size_t ILP_H__UNUSED
ILP_get_src_idx(const ILP* const prog, const char name[static 8]) {
    char needle[9];
    memset(needle, 0, 8);
    needle[8] = '\0';
    for(size_t i = 0; i < 8; i++) {
        if(name[i] == '\0') break;
        needle[i] = name[i];
    }
    const char* pos = strstr(prog->map_src, needle);
    if(!pos) return SIZE_MAX;
    return (size_t) ((pos - prog->map_src) / 8);
}

static void ILP_H__UNUSED
ILP_init(ILP* const prog) {
    ILP_map_sta_build(prog);
    ILP_map_src_build(prog);
    prog->count_seg = time_sys->duration / time_sys->scan_length;
#define X(ty_) prog->count_var[(size_t) ty_] = ILP_VAR_COUNT_DECL(ty_)(prog);
    VAR_TYPES
#undef X
    prog->total_var = 0;
    for(size_t i = 0; i < VAR_COUNT; ++i) prog->total_var += prog->count_var[i];
    HH_MALLOC(prog->buf, sizeof(*(prog->buf)) * prog->total_var);
    prog->rec = make_lp(0, (int) prog->total_var);
    HH_ASSERT(prog->rec != NULL, "Failed to construct ILP.");
}

static void ILP_H__UNUSED
ILP_free(ILP* prog) {
    free(prog->buf);
    free(prog->map_sta);
    free(prog->map_src);
    delete_lp(prog->rec);
}

// TODO: Determine if schedule parameters should be dumped here
// ... or just variable and constraint counts
static void ILP_H__UNUSED
ILP_dump(const ILP* const prog) {
    HH_DBG("datetime start: %s", time_sys_text(0));
    HH_DBG("datetime final: %s", time_sys_text(time_sys->duration));
    HH_DBG("duration [s]: %u", time_sys->duration);
    HH_DBG("scan length [s]: %u", time_sys->scan_length);
    HH_DBG("scan count: %zu", prog->count_seg);
    HH_DBG("num stations: %zu", prog->count_sta);
    HH_DBG("num sources: %zu",  prog->count_src);
#define X(ty_) HH_DBG("%s count: %zu", #ty_, prog->count_var[(size_t) ty_]);
    VAR_TYPES
#undef X
    HH_DBG("total variable count: %zu", prog->total_var);
}

static bool ILP_H__UNUSED
ILP_solve(const ILP* const prog) {
    return solve(prog->rec) == OPTIMAL;
}

static void ILP_H__UNUSED
row_begin(ILP* const prog) {
    memset(prog->buf, 0, sizeof(*(prog->buf)) * prog->total_var);
}

static void ILP_H__UNUSED
row_begin_load(ILP* const prog) {
    get_variables(prog->rec, prog->buf + 1);
}

static void ILP_H__UNUSED
row_end_as_constr(ILP* const prog, int constr_type, double rhs) {
    add_constraint(prog->rec, prog->buf, constr_type, rhs);
}

static void ILP_H__UNUSED
row_end_as_obj(ILP* const prog) {
    set_obj_fn(prog->rec, prog->buf);
}

static size_t
ILP_H__row_idx(const ILP* const prog, enum var_type ty, va_list args);

static void ILP_H__UNUSED
row_set(ILP* const prog, double co, enum var_type ty, ...) {
    va_list args;
    va_start(args, ty);
    prog->buf[ILP_H__row_idx(prog, ty, args)] = co;
    va_end(args);
}

static double ILP_H__UNUSED
row_get(const ILP* const prog, enum var_type ty, ...) {
    va_list args;
    va_start(args, ty);
    size_t idx = ILP_H__row_idx(prog, ty, args);
    va_end(args);
    return prog->buf[idx];
}

//
// Internal helper functions
//

static bool ILP_H__UNUSED
ILP_H__ILP_map_src_it_helper(const ILP* const prog, size_t* i, Source* src) {
    char buf[8];
    memcpy(buf, &(prog->map_src[8 * (*i)++]), 8);
    for(size_t j = 8; j-- > 0;) {
        if(buf[j] == ' ') buf[j] = '\0';
        else break;
    }
    Source temp;
    if(sky_get_src(buf, &temp) == NULL) return false;
    (*src) = temp;
    return true;
}

static size_t
ILP_H__row_idx(const ILP* const prog, enum var_type ty, va_list args) {
    size_t idx = SIZE_MAX, offset = 1;
#define X(ty_) \
    if(ty_ == ty) goto row_idx_post_offset; \
    offset += ILP_VAR_COUNT_DECL(ty_)(prog);
    VAR_TYPES
#undef X
if(false) goto row_idx_post_offset; // avoid unused warning
row_idx_post_offset:
#define X(ty_) if(ty_ == ty) idx = ILP_VAR_INDEX_DECL(ty_)(prog, args);
    VAR_TYPES
#undef X
    if(idx == SIZE_MAX) HH_UNREACHABLE;
    (void) prog;
    (void) ty;
    (void) args;
    return idx + offset;
}

#endif // ILP_H__

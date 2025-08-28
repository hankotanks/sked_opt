#ifndef ILP_FWD_H__
#define ILP_FWD_H__

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#include <lp_lib.h>

#ifndef VAR_TYPES
#define VAR_TYPES
#endif

enum var {
#define X(var_) var_,
    VAR_TYPES
#undef X
    VAR_COUNT
};

#if defined(__GNUC__) || defined(__clang__)
#define SCHED_H__UNUSED __attribute__((unused))
#else
#define SCHED_H__UNUSED
#endif

typedef struct {
    size_t count_sta, count_src, count_seg;
    size_t count_var[VAR_COUNT];
    size_t total_var;
    lprec* rec;
    double* buf;
    char* map_sta;
    char* map_src;
} ILP;

#define VAR_NUM_IMPL(var_) size_t num_##var_(const ILP* const prog)
#define VAR_IDX_IMPL(var_) size_t idx_##var_(const ILP* const prog, va_list args)

#define X(var_) VAR_NUM_IMPL(var_);
    VAR_TYPES
#undef X

#define X(var_) VAR_IDX_IMPL(var_);
    VAR_TYPES
#undef X

#endif // ILP_FWD_H__

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

enum var_type {
#define X(ty_, is_binary_) ty_,
    VAR_TYPES
#undef X
    VAR_COUNT
};

typedef struct {
    size_t count_sta, count_src, count_seg;
    size_t count_var[VAR_COUNT];
    size_t total_var;
    lprec* rec;
    double* buf;
    char* map_sta;
    char* map_src;
} ILP;

#define ILP_VAR_COUNT_DECL(ty_) ILP_FWD_H__count_##ty_
#define ILP_VAR_COUNT_IMPL(ty_) size_t ILP_VAR_COUNT_DECL(ty_)(const ILP* const prog)

#define ILP_VAR_INDEX_DECL(ty_) ILP_FWD_H__index_##ty_
#define ILP_VAR_INDEX_IMPL(ty_) size_t ILP_VAR_INDEX_DECL(ty_)(const ILP* const prog, va_list args)

#define X(ty_, is_binary_) ILP_VAR_COUNT_IMPL(ty_);
    VAR_TYPES
#undef X

#define X(ty_, is_binary_) ILP_VAR_INDEX_IMPL(ty_);
    VAR_TYPES
#undef X

#endif // ILP_FWD_H__

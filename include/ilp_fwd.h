#ifndef ILP_FWD_H__
#define ILP_FWD_H__

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#include "map.h"

/*
// TODO: Clean up this documentation
// To implement an ILP:
// - define variable groups using VAR_TYPES
//   EX: #define VAR_TYPES \
//           VAR_BIN(FIRST_NAME) \
//           VAR_CON(SECOND_NAME, 0.0, 1e100)
//   Three types of variables can be declared
//   - VAR_BIN(NAME) // a binary variable
//   - VAR_CON(NAME, 0.0, 1e100) // a continuous variable with upper and lower bounds (double)
//   - VAR_INT(NAME, 0, 100) // an integer variable with upper and lower bounds (int)
// - implement each variable group by supplying function bodies for indexing and total var count
//   EX for a single variable group:
//       VAR_IMPL(FST, { (void) prog; return 1; }, { (void) prog; (void) args; return 0; })
//   The first function body has signature (const ILP* const prog) -> size_t
//   The second function body has signature (const ILP* const prog, va_list args)
//   - args is passed through when calling row_set
//   - it is only responsible for indexing WITHIN that group
//     EX: [0..TOTAL_VAR_COUNT]
*/

#define VAR_IMPL(ty_, count_fn_body_, index_fn_body_) \
    size_t ILP_FWD_H__count_##ty_(const struct map* const map) count_fn_body_ \
    size_t ILP_FWD_H__index_##ty_(const struct map* const map, va_list args) index_fn_body_

//
// internal implementation details
//

#ifndef VAR_TYPES
#define VAR_TYPES
#endif

enum var_type {
#define VAR_BIN(ty_) ty_,
#define VAR_CON(ty_, lb_, ub_) ty_,
#define VAR_INT(ty_, lb_, ub_) ty_,
    VAR_TYPES
#undef VAR_BIN
#undef VAR_CON
#undef VAR_INT
    VAR_COUNT
};

typedef struct _GRBenv GRBenv;
typedef struct _GRBmodel GRBmodel;

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef struct {
    size_t count_var[VAR_COUNT];
    size_t total_var;
    GRBenv* env;
    GRBmodel* model;
#ifdef _WIN32
    HMODULE handle;
#else
    void* handle;
#endif
    int* constr_idx;
    double* constr_co;
    // NOTE: ILP does not own the map
    const struct map* map;
} ILP;

#define X(ty_) size_t ILP_FWD_H__count_##ty_(const struct map* const);
#define VAR_BIN(ty_) X(ty_)
#define VAR_CON(ty_, lb_, ub_) X(ty_)
#define VAR_INT(ty_, lb_, ub_) X(ty_)
    VAR_TYPES
#undef VAR_BIN
#undef VAR_CON
#undef VAR_INT
#undef X

#define X(ty_) size_t ILP_FWD_H__index_##ty_(const struct map* const, va_list);
#define VAR_BIN(ty_) X(ty_)
#define VAR_CON(ty_, lb_, ub_) X(ty_)
#define VAR_INT(ty_, lb_, ub_) X(ty_)
    VAR_TYPES
#undef VAR_BIN
#undef VAR_CON
#undef VAR_INT
#undef X

#endif // ILP_FWD_H__

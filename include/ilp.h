#ifndef ILP_H__
#define ILP_H__

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "hh.h"

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

static void
ILP_H__ILP_map_sta(ILP* const prog);
static void
ILP_H__ILP_map_src(ILP* const prog);
static void
ILP_init(ILP* const prog);
static void
ILP_free(ILP* prog);
static bool
ILP_solve(const ILP* const prog);
static void
row_begin(ILP* const prog);
static void
row_end_as_constr(ILP* const prog, char constr_type, double rhs);
static void
row_end_as_obj(ILP* const prog, bool maximize);
static void
row_set(ILP* const prog, double co, enum var_type ty, ...);
static double
ILP_get_sol(const ILP* const prog, enum var_type ty, ...);

//
// Implementations
//

#if defined(__GNUC__) || defined(__clang__)
#define ILP_H__UNUSED __attribute__((unused))
#else
#define ILP_H__UNUSED
#endif


static void
ILP_H__ILP_map_sta(ILP* const prog) {
    prog->count_sta = 0;
    Station sta;
    bool* active;
    for(size_t i = 0; i < NET->count; ++i) {
        active = net_get_sta_by_idx(i, &sta);
        if(active != NULL && *active) (prog->count_sta)++;
    }
    HH_CALLOC(prog->map_sta, prog->count_sta * 2 + 1);
    for(size_t i = 0, j = 0; i < NET->count; ++i) {
        active = net_get_sta_by_idx(i, &sta);
        if(active != NULL && *active) {
            prog->map_sta[j++] = sta.id[0];
            prog->map_sta[j++] = sta.id[1];
        }
    }
}

static void
ILP_H__ILP_map_src(ILP* const prog) {
    prog->count_src = 0;
    Source src;
    bool* active;
    for(size_t i = 0; i < SKY->count; ++i) {
        active = sky_get_src_by_idx(i, &src);
        if(active != NULL && *active) (prog->count_src)++;
    }
    HH_CALLOC(prog->map_src, prog->count_src * 8 + 1);
    for(size_t i = 0, j = 0, k; i < SKY->count; ++i) {
        active = sky_get_src_by_idx(i, &src);
        if(active != NULL && *active) {
            k = hh_strnlen(src.name, 8);
            memcpy(prog->map_src + j * 8, src.name, k);
            for(; k < 8; ++k) prog->map_src[j * 8 + k] = ' ';
            j++;
        }
    }
}

#define GUROBI_DECL(name_, ...) typedef int (*name_##_t)(__VA_ARGS__); static name_##_t name_

GUROBI_DECL(GRBloadenvinternal, GRBenv** envP, const char* logfilename, int major, int minor, int tech);
GUROBI_DECL(GRBfreeenv, GRBenv* env);
GUROBI_DECL(GRBnewmodel, GRBenv* env, GRBmodel** modelP, const char* Pname, int numvars, double* obj, double* lb, double* ub, char* vtype, char** varnames);
GUROBI_DECL(GRBupdatemodel, GRBmodel* model);
GUROBI_DECL(GRBfreemodel, GRBmodel* model);
GUROBI_DECL(GRBaddvar, GRBmodel* model, int numnz, int* vind, double* vval, double obj, double lb, double ub, char vtype, const char* varname);
GUROBI_DECL(GRBgetvarbyname, GRBmodel *model, const char *name, int *indexP);
GUROBI_DECL(GRBaddconstr, GRBmodel *model, int numnz, int *cind, double *cval, char sense, double rhs, const char *constrname);
GUROBI_DECL(GRBsetdblattrelement, GRBmodel *model, const char *attrname, int element, double newvalue);
GUROBI_DECL(GRBoptimize, GRBmodel *model);
GUROBI_DECL(GRBsetdblattrlist, GRBmodel *model, const char *attrname, int len, int *ind, double *newvalues);
GUROBI_DECL(GRBsetintattr, GRBmodel *model, const char *attrname, int newvalue);
GUROBI_DECL(GRBgetdblattrelement, GRBmodel *model, const char *attrname, int element, double *valueP);
GUROBI_DECL(GRBgetintattr, GRBmodel *model, const char *attrname, int *valueP);

static void ILP_H__UNUSED
ILP_H__ILP_load_func(ILP* const prog);

#define GRB_VERSION_MAJOR     12
#define GRB_VERSION_MINOR     0
#define GRB_VERSION_TECHNICAL 2

static void ILP_H__UNUSED
ILP_init(ILP* const prog) {
    prog->constr_idx = NULL;
    prog->constr_co = NULL;
    ILP_H__ILP_map_sta(prog);
    ILP_H__ILP_map_src(prog);
    prog->count_seg = TIME_SYS->duration / TIME_SYS->scan_length;
#define X(ty_, is_binary_) prog->count_var[(size_t) ty_] = ILP_VAR_COUNT_DECL(ty_)(prog);
    VAR_TYPES
#undef X
    prog->total_var = 0;
    for(size_t i = 0; i < VAR_COUNT; ++i) prog->total_var += prog->count_var[i];
    ILP_H__ILP_load_func(prog);
    HH_ASSERT(GRBloadenvinternal(&(prog->env), NULL, GRB_VERSION_MAJOR, GRB_VERSION_MINOR, GRB_VERSION_TECHNICAL) == 0, "Failed to load Gurobi environment.");
    HH_ASSERT(GRBnewmodel(prog->env, &(prog->model), "sked_opt", 0, NULL, NULL, NULL, NULL, NULL) == 0, "Failed to initialize Gurobi model.");
    size_t idx = 0;
#define X(ty_, is_binary_) \
    if(is_binary_) for(size_t j = 0; j < prog->count_var[idx]; ++j) { \
        GRBaddvar(prog->model, 0, NULL, NULL, 0.0, 0.0, 1.0, 'B', NULL); \
    } else for(size_t j = 0; j < prog->count_var[idx]; ++j) { \
        GRBaddvar(prog->model, 0, NULL, NULL, 0.0, 0.0, DBL_MAX, 'C', NULL); \
    } \
    idx++;
    VAR_TYPES
#undef X
    (void) idx;
}

static void ILP_H__UNUSED
ILP_free(ILP* prog) {
    hh_arrfree(prog->constr_idx);
    hh_arrfree(prog->constr_co);
    free(prog->map_sta);
    free(prog->map_src);
    GRBfreemodel(prog->model);
    GRBfreeenv(prog->env);
#ifdef _WIN32
    FreeLibrary(prog->handle);
#else
    dlclose(prog->handle);
#endif
}

static bool ILP_H__UNUSED
ILP_solve(const ILP* const prog) {
    GRBoptimize(prog->model);
    int status;
    GRBgetintattr(prog->model, "Status", &status);
    return (status == 2);
}

static void ILP_H__UNUSED
row_begin(ILP* const prog) {
    hh_arrclear(prog->constr_idx);
    hh_arrclear(prog->constr_co);
}

static void ILP_H__UNUSED
row_end_as_constr(ILP* const prog, char constr_type, double rhs) {
    HH_ASSERT(hh_arrlen(prog->constr_idx) == hh_arrlen(prog->constr_co), "UNREACHABLE");
    GRBaddconstr(prog->model, (int) hh_arrlen(prog->constr_idx), prog->constr_idx, prog->constr_co, constr_type, rhs, "NULL");
}

static void ILP_H__UNUSED
row_end_as_obj(ILP* const prog, bool maximize) {
    HH_ASSERT(hh_arrlen(prog->constr_idx) == hh_arrlen(prog->constr_co), "UNREACHABLE");
    GRBsetdblattrlist(prog->model, "Obj", (int) hh_arrlen(prog->constr_idx), prog->constr_idx, prog->constr_co);
    GRBsetintattr(prog->model, "ModelSense", maximize ? -1 : 1);
}

static size_t
ILP_H__row_idx(const ILP* const prog, enum var_type ty, va_list args);

static void ILP_H__UNUSED
row_set(ILP* const prog, double co, enum var_type ty, ...) {
    va_list args;
    va_start(args, ty);
    hh_arrput(prog->constr_idx, (int) ILP_H__row_idx(prog, ty, args));
    va_end(args);
    hh_arrput(prog->constr_co, co);
}

static double ILP_H__UNUSED
ILP_get_sol(const ILP* const prog, enum var_type ty, ...) {
    va_list args;
    va_start(args, ty);
    double val;
    GRBgetdblattrelement(prog->model, "X", (int) ILP_H__row_idx(prog, ty, args), &val);
    va_end(args);
    return val;
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
    size_t idx = SIZE_MAX, offset = 0;
#define X(ty_, is_binary_) \
    if(ty_ == ty) goto row_idx_post_offset; \
    offset += ILP_VAR_COUNT_DECL(ty_)(prog);
    VAR_TYPES
#undef X
    if(false) goto row_idx_post_offset; // avoid unused warning
row_idx_post_offset:
#define X(ty_, is_binary_) if(ty_ == ty) idx = ILP_VAR_INDEX_DECL(ty_)(prog, args);
    VAR_TYPES
#undef X
    if(idx == SIZE_MAX) HH_UNREACHABLE;
    (void) prog;
    (void) ty;
    (void) args;
    return idx + offset;
}

#ifdef _WIN32
#define GUROBI_IMPL(handle_, name_) do { \
        name_ = (name_##_t) GetProcAddress(handle_, #name_); \
        HH_ASSERT(name_ != NULL, "Failed to load Gurobi."); \
    } while(0);
#else
#define GUROBI_IMPL(handle_, name_) do { \
        name_ = (name_##_t) dlsym(handle_, #name_); \
        HH_ASSERT(name_ != NULL, "Failed to load Gurobi."); \
    } while(0);
#endif

#define GUROBI_VERSION 120
#define GUROBI_LIB_NAME "gurobi" HH_STR(GUROBI_VERSION)
#ifdef _WIN32
#define GUROBI_LIB_FILE GUROBI_LIB_NAME ".dll"
#else
#define GUROBI_LIB_FILE "lib" GUROBI_LIB_NAME ".so"
#endif

static void ILP_H__UNUSED
ILP_H__ILP_load_func(ILP* const prog) {
    char* gurobi_home = getenv("GUROBI_HOME");
    HH_ASSERT(gurobi_home != NULL, "Failed to load Gurobi.");
    char* path = hh_path_join(hh_path_join(hh_path(gurobi_home), "lib"), GUROBI_LIB_FILE);
    HH_ASSERT(hh_path_exists(path), "Failed to load Gurobi.");
#ifdef _WIN32
    prog->handle = LoadLibrary(path);
#else
    prog->handle = dlopen(path, RTLD_LAZY);
#endif
    HH_ASSERT(prog->handle, "Failed to load Gurobi.");
    GUROBI_IMPL(prog->handle, GRBloadenvinternal);
    GUROBI_IMPL(prog->handle, GRBfreeenv);
    GUROBI_IMPL(prog->handle, GRBnewmodel);
    GUROBI_IMPL(prog->handle, GRBupdatemodel);
    GUROBI_IMPL(prog->handle, GRBfreemodel);
    GUROBI_IMPL(prog->handle, GRBaddvar);
    GUROBI_IMPL(prog->handle, GRBgetvarbyname);
    GUROBI_IMPL(prog->handle, GRBaddconstr);
    GUROBI_IMPL(prog->handle, GRBsetdblattrelement);
    GUROBI_IMPL(prog->handle, GRBoptimize);
    GUROBI_IMPL(prog->handle, GRBsetdblattrlist);
    GUROBI_IMPL(prog->handle, GRBsetintattr);
    GUROBI_IMPL(prog->handle, GRBgetdblattrelement);
    GUROBI_IMPL(prog->handle, GRBgetintattr);
    HH_MSG("Gurobi library loaded successfully!");
    hh_arrfree(path);
}

#endif // ILP_H__

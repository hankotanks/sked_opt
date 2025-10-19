#ifndef ILP_H__
#define ILP_H__

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "hh.h"

#include "map.h"

#include "ilp_fwd.h"

static void
ILP_init(ILP* const prog, const struct map* const map);
static void
ILP_free(ILP* prog);
static void
ILP_param_int(ILP* const prog, const char* param, int val);
static void
ILP_param_dbl(ILP* const prog, const char* param, double val);
static bool
ILP_solve(const ILP* const prog);
static void
ILP_write(const ILP* const prog, const char* path, bool iis);
static void
ILP_row_begin(ILP* const prog);
static void
ILP_row_end_as_constr(ILP* const prog, char constr_type, double rhs);
static void
ILP_row_end_as_indicator(ILP* const prog, char constr_type, double rhs);
static void
ILP_row_end_as_obj(ILP* const prog, bool maximize);
static void
ILP_row_set(ILP* const prog, double co, enum var_type ty, ...);
static double
ILP_get_sol(const ILP* const prog, enum var_type ty, ...);
static void
ILP_var_param_int(ILP* const prog, const char* const attr, int val, enum var_type ty, ...);
static void
ILP_var_param_dbl(ILP* const prog, const char* const attr, double val, enum var_type ty, ...);

//
// Implementations
//

#define GUROBI_DECL(name_, ret_, ...) typedef ret_ (*name_##_t)(__VA_ARGS__); static name_##_t name_

GUROBI_DECL(GRBloadenvinternal, int, GRBenv** envP, const char* logfilename, int major, int minor, int tech);
GUROBI_DECL(GRBfreeenv, int, GRBenv* env);
GUROBI_DECL(GRBnewmodel, int, GRBenv* env, GRBmodel** modelP, const char* Pname, int numvars, double* obj, double* lb, double* ub, char* vtype, char** varnames);
GUROBI_DECL(GRBupdatemodel, int, GRBmodel* model);
GUROBI_DECL(GRBfreemodel, int, GRBmodel* model);
GUROBI_DECL(GRBaddvar, int, GRBmodel* model, int numnz, int* vind, double* vval, double obj, double lb, double ub, char vtype, const char* varname);
GUROBI_DECL(GRBgetvarbyname, int, GRBmodel *model, const char *name, int *indexP);
GUROBI_DECL(GRBaddconstr, int, GRBmodel *model, int numnz, int *cind, double *cval, char sense, double rhs, const char *constrname);
GUROBI_DECL(GRBsetdblattrelement, int, GRBmodel *model, const char *attrname, int element, double newvalue);
GUROBI_DECL(GRBoptimize, int, GRBmodel *model);
GUROBI_DECL(GRBsetdblattrlist, int, GRBmodel *model, const char *attrname, int len, int *ind, double *newvalues);
GUROBI_DECL(GRBsetintattr, int, GRBmodel *model, const char *attrname, int newvalue);
GUROBI_DECL(GRBgetdblattrelement, int, GRBmodel *model, const char *attrname, int element, double *valueP);
GUROBI_DECL(GRBgetintattr, int, GRBmodel *model, const char *attrname, int *valueP);
GUROBI_DECL(GRBgeterrormsg, const char*, GRBenv *env);
GUROBI_DECL(GRBsetintparam, int, GRBenv *env, const char *paramname, int value);
GUROBI_DECL(GRBsetdblparam, int, GRBenv *env, const char *paramname, double value);
GUROBI_DECL(GRBwrite, int, GRBmodel *model, const char *filename);
GUROBI_DECL(GRBcomputeIIS, int, GRBmodel *model);
GUROBI_DECL(GRBaddgenconstrIndicator, int, GRBmodel *model, const char *name, int binvar, int binval, int nvars, const int *vars, const double *vals, char sense, double rhs);
GUROBI_DECL(GRBsetintattrelement, int, GRBmodel *model, const char *attrname, int element, int newvalue);

static void HH_UNUSED
ILP_H__load_gurobi(ILP* const prog);

#define GRB_VERSION_MAJOR     12
#define GRB_VERSION_MINOR     0
#define GRB_VERSION_TECHNICAL 2

static void HH_UNUSED
ILP_init(ILP* const prog, const struct map* const map) {
    prog->constr_idx = NULL;
    prog->constr_co = NULL;
    prog->map = map;
#define X(ty_) prog->count_var[(size_t) ty_] = ILP_FWD_H__count_##ty_(prog->map);
#define VAR_BIN(ty_) X(ty_)
#define VAR_CON(ty_, lb_, ub_) X(ty_)
#define VAR_INT(ty_, lb_, ub_) X(ty_)
    VAR_TYPES
#undef VAR_BIN
#undef VAR_CON
#undef VAR_INT
#undef X
    prog->total_var = 0;
    for(size_t i = 0; i < VAR_COUNT; ++i) prog->total_var += prog->count_var[i];
    ILP_H__load_gurobi(prog);
    int err;
    err = GRBloadenvinternal(&(prog->env), NULL, GRB_VERSION_MAJOR, GRB_VERSION_MINOR, GRB_VERSION_TECHNICAL);
    HH_ASSERT(!err, "Failed to load Gurobi environment: %d.", err);
    // ILP_param_int(prog, "Threads", 1);
    err = GRBnewmodel(prog->env, &(prog->model), "sked_opt", 0, NULL, NULL, NULL, NULL, NULL);
    HH_ASSERT(!err, "Failed to initialize Gurobi model.");
    size_t idx = 0;
#define X(ty_, lb_, ub_, vtype_) for(size_t j = 0; j < prog->count_var[idx]; ++j) { \
        char vname_[64]; \
        snprintf(vname_, sizeof vname_, "%s_%zu", #ty_, j); \
        err = GRBaddvar(prog->model, 0, NULL, NULL, 0.0, (lb_), (ub_), (vtype_), vname_); \
        HH_ASSERT(!err, "Failed to add %s variable to Gurobi model: %s", #ty_, GRBgeterrormsg(prog->env)); } \
        idx++;
#define VAR_BIN(ty_) X(ty_, 0.0, 1.0, 'B')
#define VAR_CON(ty_, lb_, ub_) X(ty_, lb_, ub_, 'C')
#define VAR_INT(ty_, lb_, ub_) X(ty_, (double) lb_, (double) ub_, 'I')
    VAR_TYPES
#undef VAR_BIN
#undef VAR_CON
#undef VAR_INT
#undef X
    GRBupdatemodel(prog->model);
    int total_var;
    err = GRBgetintattr(prog->model, "NumVars", &total_var);
    HH_ASSERT(!err, "Failed to query Gurobi model's column count: %s", GRBgeterrormsg(prog->env));
    HH_ASSERT((size_t) total_var == prog->total_var, "Wrong number of columns in Gurobi model.");
    HH_MSG("Total var count: %d", total_var);
    (void) idx;
}

static void HH_UNUSED
ILP_free(ILP* prog) {
    hh_arrfree(prog->constr_idx);
    hh_arrfree(prog->constr_co);
    int err;
    err = GRBfreemodel(prog->model);
    HH_ASSERT(!err, "Failed to free Gurobi model.");
    err = GRBfreeenv(prog->env);
    HH_ASSERT(!err, "Failed to free Gurobi environment.");
#ifdef _WIN32
    FreeLibrary(prog->handle);
#else
    dlclose(prog->handle);
#endif
}

static void HH_UNUSED
ILP_param_int(ILP* const prog, const char* param, int val) {
    int err = GRBsetintparam(prog->env, param, val);
    HH_ASSERT(!err, "Failed to configure Gurobi model: %s", GRBgeterrormsg(prog->env));
}

static void HH_UNUSED
ILP_param_dbl(ILP* const prog, const char* param, double val) {
    int err = GRBsetdblparam(prog->env, param, val);
    HH_ASSERT(!err, "Failed to configure Gurobi model: %s", GRBgeterrormsg(prog->env));
}

static bool HH_UNUSED
ILP_solve(const ILP* const prog) {
    int err;
    err = GRBupdatemodel(prog->model);
    HH_ASSERT(!err, "Failed to update Gurobi model: %s", GRBgeterrormsg(prog->env));
    err = GRBoptimize(prog->model);
    HH_ASSERT(!err, "Failed to optimize Gurobi model: %s", GRBgeterrormsg(prog->env));
    int status;
    err = GRBgetintattr(prog->model, "Status", &status);
    HH_ASSERT(!err, "Failed to retrieve status of Gurobi model: %s", GRBgeterrormsg(prog->env));
    return (status == 2);
}

static void HH_UNUSED
ILP_write(const ILP* const prog, const char* path, bool iis) {
    int err;
    if(iis) {
        err = GRBcomputeIIS(prog->model);
        HH_ASSERT(!err, "Failed to compute Irreducible Infeasible Subset (IIS): %s", GRBgeterrormsg(prog->env));
    }
    err = GRBwrite(prog->model, path);
    HH_ASSERT(!err, "Failed to dump Gurobi model: %s", GRBgeterrormsg(prog->env));
}

static void HH_UNUSED
ILP_row_begin(ILP* const prog) {
    hh_arrclear(prog->constr_idx);
    hh_arrclear(prog->constr_co);
}

static void HH_UNUSED
ILP_row_end_as_constr(ILP* const prog, char constr_type, double rhs) {
    HH_ASSERT(hh_arrlen(prog->constr_idx) == hh_arrlen(prog->constr_co), "UNREACHABLE");
    int err = GRBaddconstr(prog->model, (int) hh_arrlen(prog->constr_idx), prog->constr_idx, prog->constr_co, constr_type, rhs, NULL);
    HH_ASSERT(!err, "Failed to add constraint to Gurobi model: %s", GRBgeterrormsg(prog->env));
}

static void HH_UNUSED
ILP_row_end_as_indicator(ILP* const prog, char constr_type, double rhs) {
    HH_ASSERT(hh_arrlen(prog->constr_idx) == hh_arrlen(prog->constr_co), "UNREACHABLE");
    int err = GRBaddgenconstrIndicator(prog->model, NULL, prog->constr_idx[0], (int) prog->constr_co[0], (int) hh_arrlen(prog->constr_idx) - 1, prog->constr_idx + 1, prog->constr_co + 1, constr_type, rhs);
    HH_ASSERT(!err, "Failed to add constraint to Gurobi model: %s", GRBgeterrormsg(prog->env));
}

static void HH_UNUSED
ILP_row_end_as_obj(ILP* const prog, bool maximize) {
    HH_ASSERT(hh_arrlen(prog->constr_idx) == hh_arrlen(prog->constr_co), "UNREACHABLE");
    // for(size_t i = 0, len = hh_arrlen(prog->constr_idx); i < len; ++i) printf("[%d: %lf] ", prog->constr_idx[i], prog->constr_co[i]);
    // printf("\n");
    int err;
    err = GRBsetdblattrlist(prog->model, "Obj", (int) hh_arrlen(prog->constr_idx), prog->constr_idx, prog->constr_co);
    HH_ASSERT(!err, "Failed to set Gurobi model's objective coefficients: %s", GRBgeterrormsg(prog->env));
    err = GRBsetintattr(prog->model, "ModelSense", maximize ? -1 : 1);
    HH_ASSERT(!err, "Failed to set Gurobi model's model sense: %s", GRBgeterrormsg(prog->env));
}

static size_t
ILP_H__row_idx(const ILP* const prog, enum var_type ty, va_list args);

static void HH_UNUSED
ILP_row_set(ILP* const prog, double co, enum var_type ty, ...) {
    HH_ASSERT(hh_arrlen(prog->constr_idx) == hh_arrlen(prog->constr_co), "UNREACHABLE");
    va_list args;
    va_start(args, ty);
    int idx = (int) ILP_H__row_idx(prog, ty, args);
    va_end(args);
    for(size_t i = 0, len = hh_arrlen(prog->constr_idx); i < len; ++i) {
        if(prog->constr_idx[i] == idx) {
            prog->constr_co[i] += co;
            return;
        }
    }
    hh_arrput(prog->constr_idx, idx);
    hh_arrput(prog->constr_co, co);
}

static double HH_UNUSED
ILP_get_sol(const ILP* const prog, enum var_type ty, ...) {
    va_list args;
    va_start(args, ty);
    double val;
    int err = GRBgetdblattrelement(prog->model, "X", (int) ILP_H__row_idx(prog, ty, args), &val);
    HH_ASSERT(!err, "Failed to retrieve solution from Gurobi model: %s", GRBgeterrormsg(prog->env));
    va_end(args);
    return val;
}

static void HH_UNUSED
ILP_var_param_int(ILP* const prog, const char* const attr, int val, enum var_type ty, ...) {
    va_list args;
    va_start(args, ty);
    int err = GRBsetintattrelement(prog->model, attr, (int) ILP_H__row_idx(prog, ty, args), val);
    HH_ASSERT(!err, "Failed to retrieve solution from Gurobi model: %s", GRBgeterrormsg(prog->env));
    va_end(args);
}

static void HH_UNUSED
ILP_var_param_dbl(ILP* const prog, const char* const attr, double val, enum var_type ty, ...) {
    va_list args;
    va_start(args, ty);
    int err = GRBsetdblattrelement(prog->model, attr, (int) ILP_H__row_idx(prog, ty, args), val);
    HH_ASSERT(!err, "Failed to retrieve solution from Gurobi model: %s", GRBgeterrormsg(prog->env));
    va_end(args);
}

//
// Internal helper functions
//

static size_t
ILP_H__row_idx(const ILP* const prog, enum var_type ty, va_list args) {
    size_t idx = SIZE_MAX, offset = 0;
#define X(ty_) \
    if(ty_ == ty) goto row_idx_post_offset; \
    offset += ILP_FWD_H__count_##ty_(prog->map);
#define VAR_BIN(ty_) X(ty_)
#define VAR_CON(ty_, lb_, ub_) X(ty_)
#define VAR_INT(ty_, lb_, ub_) X(ty_)
    VAR_TYPES
#undef VAR_BIN
#undef VAR_CON
#undef VAR_INT
#undef X
    if(false) goto row_idx_post_offset; // avoid unused warning
row_idx_post_offset:
#define X(ty_) if(ty_ == ty) idx = ILP_FWD_H__index_##ty_(prog->map, args);
#define VAR_BIN(ty_) X(ty_)
#define VAR_CON(ty_, lb_, ub_) X(ty_)
#define VAR_INT(ty_, lb_, ub_) X(ty_)
    VAR_TYPES
#undef VAR_BIN
#undef VAR_CON
#undef VAR_INT
#undef X
    if(idx == SIZE_MAX) HH_UNREACHABLE;
    (void) prog;
    (void) ty;
    (void) args;
    return idx + offset;
}

#ifdef _WIN32
#define GUROBI_IMPL(handle_, name_) do { \
        union { FARPROC obj; name_##_t fn; } ILP_H__GUROBI_IMPL_helper; \
        ILP_H__GUROBI_IMPL_helper.obj = GetProcAddress(handle_, #name_); \
        name_ = ILP_H__GUROBI_IMPL_helper.fn; \
        HH_ASSERT(name_ != NULL, "Failed to load Gurobi."); \
    } while (0)
#else
#define GUROBI_IMPL(handle_, name_) do { \
        union { void *obj; name_##_t fn; } ILP_H__GUROBI_IMPL_helper; \
        ILP_H__GUROBI_IMPL_helper.obj = dlsym(handle_, #name_); \
        name_ = ILP_H__GUROBI_IMPL_helper.fn; \
        HH_ASSERT(name_ != NULL, "Failed to load Gurobi."); \
    } while(0)
#endif

#define GUROBI_VERSION 120
#define GUROBI_LIB_NAME "gurobi" HH_STR(GUROBI_VERSION)
#ifdef _WIN32
#define GUROBI_LIB_FILE GUROBI_LIB_NAME ".dll"
#else
#define GUROBI_LIB_FILE "lib" GUROBI_LIB_NAME ".so"
#endif

static void HH_UNUSED
ILP_H__load_gurobi(ILP* const prog) {
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
    GUROBI_IMPL(prog->handle, GRBgeterrormsg);
    GUROBI_IMPL(prog->handle, GRBsetintparam);
    GUROBI_IMPL(prog->handle, GRBsetdblparam);
    GUROBI_IMPL(prog->handle, GRBwrite);
    GUROBI_IMPL(prog->handle, GRBcomputeIIS);
    GUROBI_IMPL(prog->handle, GRBaddgenconstrIndicator);
    GUROBI_IMPL(prog->handle, GRBsetintattrelement);
    HH_MSG("Gurobi library loaded successfully!");
    hh_arrfree(path);
}

#endif // ILP_H__

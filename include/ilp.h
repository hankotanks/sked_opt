#ifndef ILP_H__
#define ILP_H__

#include <lp_lib.h>

#include "hh.h"

#include "station.h"
#include "source.h"

typedef struct {
    size_t count_b, count_k, count_t;
    size_t count_x, count_y, count_z;
    lprec* rec;
    double* buf;
    size_t* map_sta_to_idx;
    size_t* map_src_to_idx;
    size_t* map_idx_to_sta;
    size_t* map_idx_to_src;
} Program;

// Program
void
Program_init(Program* ilp);
bool // returns truthy if the solution is optimal
Program_solve(const Program* const ilp);
void
Program_free(Program* const ilp);
void
Program_dump(const Program* const ilp);

// Program helper functions
Station
Program_idx_to_sta(const Program* const ilp, size_t bi);
Source
Program_idx_to_src(const Program* const ilp, size_t ki);

// constraints
void
row_begin(Program* const ilp);
void
row_end_as_constr(Program* const ilp, int constr_type, double rhs);
void
row_end_as_obj(Program* const ilp);
// group of functions for indexing using net and sky indices
void
row_x(Program* const ilp, double coeff, size_t t, size_t k, size_t b);
void
row_y(Program* const ilp, double coeff, size_t t, size_t k);
void
row_z(Program* const ilp, double coeff, size_t b, size_t c);
// indexing using internal indices
void
row_xi(Program* const ilp, double coeff, size_t t, size_t ki, size_t bi);
void
row_yi(Program* const ilp, double coeff, size_t t, size_t ki);
void
row_zi(Program* const ilp, double coeff, size_t bi, size_t c);

//
// declarations for different ILP configurations
//

// basic ILP using only sky coverage as scoring function
void
Program_load_skycov(Program* const ilp); 


#endif // ILP_H__

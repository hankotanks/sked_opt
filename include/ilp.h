#ifndef ILP_H__
#define ILP_H__

#include <lp_lib.h>

#include "hh.h"

typedef struct {
    size_t count_b, count_k, count_t;
    size_t count_x, count_y, count_z;
    lprec* rec;
    double* buf;
    size_t* map_sta;
    size_t* map_src;
} Program;

void
Program_init(Program* ilp);
void
Program_free(Program* const ilp);
void
Program_dump(const Program* const ilp);

void
constr_begin(Program* const ilp);
void
constr_x(Program* const ilp, double coeff, size_t t, size_t k, size_t b);
void
constr_y(Program* const ilp, double coeff, size_t t, size_t k);
void
constr_z(Program* const ilp, double coeff, size_t b, size_t c);
void
constr_end(Program* const ilp, int constr_type, double rhs);


#endif // ILP_H__

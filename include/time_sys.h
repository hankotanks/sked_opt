#ifndef TIME_SYS_H__
#define TIME_SYS_H__

#include <stddef.h>
#include <time.h>

#include "xml.h"

#define TIME_SYS_H__MONTHS \
    X(JAN,  1) \
    X(FEB,  2) \
    X(MAR,  3) \
    X(APR,  4) \
    X(MAY,  5) \
    X(JUN,  6) \
    X(JUL,  7) \
    X(AUG,  8) \
    X(SEP,  9) \
    X(OCT, 10) \
    X(NOV, 11) \
    X(DEC, 12)

enum month {
#define X(name_, val_) name_ = val_,
    TIME_SYS_H__MONTHS
#undef X
};

extern const char* months[12];

size_t
months_count_days(size_t yrs, enum month mon);

typedef struct {
    size_t yrs;
    enum month mon;
    size_t day;
    size_t hrs;
    size_t min;
    double sec;
} DateTime;

double
DateTime_to_mjd(DateTime dt);
DateTime
DateTime_from_mjd(double mjd);
double
DateTime_to_gmst(DateTime dt);

typedef struct {
    DateTime start;
    unsigned int duration, scan_length; // seconds
} TimeSys;

extern TimeSys* time_sys;

void
time_sys_init(void);
void
time_sys_xml_parse(struct xml_node* root);

#endif // TIME_SYS_H__

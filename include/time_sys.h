#ifndef TIME_SYS_H__
#define TIME_SYS_H__

#include <time.h>

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

typedef struct {
    DateTime start;
    unsigned int duration; // seconds
} TimeSys;

extern TimeSys* time_sys;

void
time_sys_init(void);
DateTime
time_sys_to_datetime(unsigned int seconds);

// const char*
// time_sys_start_string(void);

#endif // TIME_SYS_H__

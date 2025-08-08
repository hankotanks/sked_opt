#ifndef TIME_SYS_H__
#define TIME_SYS_H__

#include <time.h>

enum month { JAN = 1, FEB, MAR, APR, MAY, JUN, JUL, AUG, SEP, OCT, NOV, DEC };

typedef struct {
    size_t year;
    enum month month;
    size_t day;
    size_t hour;
    size_t min;
    double sec;
} DateTime;

typedef struct {
    time_t start;
    double start_mjd; // julian date
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

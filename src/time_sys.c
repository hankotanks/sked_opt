#include "time_sys.h"

#include <math.h>
#include <time.h>

#include "hh.h"

double
DateTime_to_mjd(DateTime dt) {
    int yrs = (int) dt.yrs - (int) (dt.mon == JAN || dt.mon == FEB);
    int mon = (int) dt.mon + ((dt.mon == JAN || dt.mon == FEB) ? 12 : 0) + 1;
    int day_int = (int) dt.day;
    int A = yrs / 100;
    int B = 2 - A + (A / 4);
    double JD = floor(365.25 * (yrs + 4716)) + floor(30.6001 * (mon + 1)) + day_int + (double) B - 1524.5;
    double day_frac = ((double) dt.hrs + ((double) dt.min + (double) dt.sec / 60.0) / 60.0) / 24.0;
    return (JD + day_frac) - 2400000.5;
}

DateTime
DateTime_from_mjd(double mjd) {
    DateTime dt;
    double jd = (mjd + 2400000.5);
    double jd_int = floor(jd + 0.5);
    double jd_frac = jd + 0.5 - jd_int;
    size_t L = (size_t) jd_int + 68569;
    size_t N = (4 * L) / 146097;
    L = L - (146097 * N + 3) / 4;
    size_t I = (4000 * (L + 1)) / 1461001;
    L = L - (1461 * I) / 4 + 31;
    size_t J = (80 * L) / 2447;
    size_t K = L - (2447 * J) / 80;
    L = J / 11;
    J = J + 2 - 12 * L;
    I = 100 * (N - 49) + I + L;
    dt.yrs = I;
    dt.mon = (enum month) (J - 1);
    dt.day = K;
    double jd_min = jd_frac * 1440.0;
    dt.hrs = (size_t) (jd_min / 60.0);
    dt.min = ((size_t) jd_min) % 60;
    dt.sec = (jd_min - ((double) dt.hrs * 60.0 + (double) dt.min)) * 60.0;
    return dt;
}


const char* months[12] = {
#define X(name_, val_) #name_,
    TIME_SYS_H__MONTHS
#undef X
};

static TimeSys TIME_SYS_H__time_sys; TimeSys* time_sys = &TIME_SYS_H__time_sys;

void
time_sys_init(void) {
    time_sys->start = DateTime_from_mjd(((double) time(NULL)) / 86400.0 + 2440587.5  - 2400000.5);
    time_sys->duration = 86400;
}

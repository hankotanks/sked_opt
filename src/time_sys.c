#include "time_sys.h"

#include <math.h>
#include <time.h>

static TimeSys TIME_SYS_H__time_sys; TimeSys* time_sys = &TIME_SYS_H__time_sys;

void
time_sys_init(void) {
    time_sys->start = time(NULL);
    time_sys->start_mjd = ((double) time_sys->start) / 86400.0 + 2440587.5  - 2400000.5;
    time_sys->duration = 86164;
}

#if 0
const char*
time_sys_start_string(void) {
    struct tm* start_info = localtime(&(time_sys->start));
    static char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", start_info);
    return buf;
}
#endif

DateTime
time_sys_to_datetime(unsigned int seconds) {
    DateTime dt;
    double jd = (time_sys->start_mjd + 2400000.5) + seconds / 86400.0;
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
    dt.year = I;
    dt.month = (enum month) J;
    dt.day = K;
    double jd_min = jd_frac * 1440.0;
    dt.hour = (size_t) (jd_min / 60.0);
    dt.min = ((size_t) jd_min) % 60;
    return dt;
}

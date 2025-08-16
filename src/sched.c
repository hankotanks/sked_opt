#include "sched.h"

#include "hh.h"

#include "time_sys.h"
#include "network.h"
#include "sky.h"

const char*
DateTime_format(DateTime dt) {
    static char buf[16];
    snprintf(buf, 5, "%04zu", dt.yrs);
    snprintf(buf + 4, 4, "%s", months[dt.mon - 1]);
    snprintf(buf + 7, 3, "%02zu", dt.day);
    buf[9] = ' ';
    snprintf(buf + 10, 3, "%02zu", dt.hrs);
    buf[12] = ':';
    snprintf(buf + 13, 3, "%02zu", dt.min);
    return buf;
}

void
sched_start(void) {
    HH_DBG("datetime start: %s", DateTime_format(time_sys->start));
    DateTime final = DateTime_from_mjd(DateTime_to_mjd(time_sys->start) + time_sys->duration / 86400.0);
    HH_DBG("datetime final: %s", DateTime_format(final));
    HH_DBG("duration [s]: %u", time_sys->duration);
    HH_DBG("scan length [s]: %u", time_sys->scan_length);
    HH_DBG("scan count: %u", time_sys->duration / time_sys->scan_length);
    size_t count = 0;
    bool* active;
    for(size_t i = 0; i < net->count; ++i) {
        Station sta;
        active = net_get_sta_by_idx(i, &sta);
        if(active != NULL && *active) ++count;
    }
    HH_DBG("num stations: %zu", count);
    count = 0;
    for(size_t i = 0; i < sky->count; ++i) {
        Source src;
        active = sky_get_src_by_idx(i, &src);
        if(active != NULL && *active) ++count;
    }
    HH_DBG("num sources: %zu", count);

    
}

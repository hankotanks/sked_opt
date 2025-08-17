#include "sched.h"

#include "hh.h"

#include "time_sys.h"
#include "network.h"
#include "sky.h"

void
sched_start(void) {
    HH_DBG("datetime start: %s", time_sys_text(0));
    HH_DBG("datetime final: %s", time_sys_text(time_sys->duration));
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

#include "astro.h"

#include <sofa.h>
#include <sofam.h>

#include "hh.h"

#include "time_sys.h"

static struct ASTRO_H__EARTH_PARAMS ASTRO_H__EARTH_PARAMS; 
struct ASTRO_H__EARTH_PARAMS* EARTH_PARAMS = &ASTRO_H__EARTH_PARAMS;

#define MJD_INITIAL 2400000.5
#define AUD_TO_MS (DAU / DAYSEC)

void
earth_params_init(void) {
    EARTH_PARAMS->nut.x = NULL;
    EARTH_PARAMS->nut.y = NULL;
    EARTH_PARAMS->nut.s = NULL;
    EARTH_PARAMS->nut.t = NULL;
    // TODO: Should this always be fixed?
    EARTH_PARAMS->nut.dt = 3600;

    double mjd, mjd_start = DateTime_to_mjd(TIME_SYS->start);
    double x, y, s;

    unsigned int t_ref, counter = 0;
    do {
        t_ref = counter * EARTH_PARAMS->nut.dt;
        mjd = mjd_start + ((double) t_ref) / 86400.0;
        iauXys06a(MJD_INITIAL, mjd, &x, &y, &s);
        hh_arrput(EARTH_PARAMS->nut.x, x);
        hh_arrput(EARTH_PARAMS->nut.y, y);
        hh_arrput(EARTH_PARAMS->nut.s, s);
        hh_arrput(EARTH_PARAMS->nut.t, t_ref);
        counter++;
    } while(t_ref < TIME_SYS->duration + EARTH_PARAMS->nut.dt);

    mjd = mjd_start + ((double) TIME_SYS->duration) / 2.0 / 86400.0;

    double pvh[2][3];
    double pvb[2][3];
    iauEpv00(MJD_INITIAL, mjd, pvh, pvb);
    EARTH_PARAMS->vel[0] = AUD_TO_MS * pvb[1][0];
    EARTH_PARAMS->vel[1] = AUD_TO_MS * pvb[1][1];
    EARTH_PARAMS->vel[2] = AUD_TO_MS * pvb[1][2];
}

#undef MJD_INITIAL
#undef AUD_TO_MS

void
earth_params_free(void) {
    hh_arrfree(EARTH_PARAMS->nut.x);
    hh_arrfree(EARTH_PARAMS->nut.y);
    hh_arrfree(EARTH_PARAMS->nut.s);
    hh_arrfree(EARTH_PARAMS->nut.t);
}

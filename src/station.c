#include "station.h"

#include <stdbool.h>
#include <stdio.h>

#include <sofa.h>
#include <sofam.h>

#include "source.h"
#include "time_sys.h"
#include "astro.h"

void
Station_dump(const Station* const sta) {
    printf("%c%c [", sta->id[0], sta->id[1]);
    cat_name_print(sta->name);
    printf("]\n");
    printf("  ecef: [%.4lf, %.4lf, %.4lf]\n", sta->x, sta->y, sta->z);
    printf("  lat: %.2lf\n", sta->lat);
    printf("  lon: %.2lf\n", sta->lon);
    printf("  axis limits [");
    switch(sta->axes) {
    case AXES_AZEL:
        printf("azi");
        break;
    case AXES_HADC:
        printf("hr-angle");
        break;
    case AXES_XYEW:
        printf("e-w");
        break;
    case AXES_XYNS:
        printf("n-s");
        break;
    default: HH_UNREACHABLE;
    }
    printf("]:\n");
    printf("    max slew rate [deg/min]: %.2lf\n", sta->axes_limits[0].rate);
    printf("    acc. [deg/min^2]: %zu\n", sta->axes_limits[0].c);
    printf("    min. [deg]: %.2lf\n", sta->axes_limits[0].limits[0]);
    printf("    max. [deg]: %.2lf\n", sta->axes_limits[0].limits[1]);
    printf("  axis limits [");
    switch(sta->axes) {
    case AXES_AZEL:
        printf("el");
        break;
    case AXES_HADC:
        printf("decl");
        break;
    case AXES_XYEW:
        printf("n-s");
        break;
    case AXES_XYNS:
        printf("e-w");
        break;
    default: HH_UNREACHABLE;
    }
    printf("]:\n");
    printf("    max slew rate [deg/min]: %.2lf\n", sta->axes_limits[1].rate);
    printf("    acc. [deg/min^2]: %zu\n", sta->axes_limits[1].c);
    printf("    phys. min. [deg]: %.2lf\n", sta->axes_limits[1].limits[0]);
    printf("    phys. max. [deg]: %.2lf\n", sta->axes_limits[1].limits[1]);
}

#define OMEGA 7.2921151467069805e-05
#define MJD_INITIAL 2400000.5

void
Station_az_el(const Station* const sta, const Source* const src, unsigned int seconds, double* az, double* el) {
    DateTime dt = TIME_SYS->start;
    dt.sec += (double) seconds;
    double mjd = DateTime_to_mjd(dt);

    double era = iauEra00(MJD_INITIAL, mjd);

    double C[3][3] = {{ 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 }};

    size_t nut_idx = 0;
    while(EARTH_PARAMS->nut.t[nut_idx + 1] < seconds) ++nut_idx;

    unsigned int seconds_delta = seconds - EARTH_PARAMS->nut.t[nut_idx];

    double x, y, s;
    x = EARTH_PARAMS->nut.x[nut_idx];
    x = (EARTH_PARAMS->nut.x[nut_idx + 1] - x) / EARTH_PARAMS->nut.dt * seconds_delta;
    y = EARTH_PARAMS->nut.y[nut_idx];
    y = (EARTH_PARAMS->nut.y[nut_idx + 1] - y) / EARTH_PARAMS->nut.dt * seconds_delta;
    s = EARTH_PARAMS->nut.s[nut_idx];
    s = (EARTH_PARAMS->nut.s[nut_idx + 1] - s) / EARTH_PARAMS->nut.dt * seconds_delta;

    iauC2ixys(x, y, s, C);

    double W[3][3] = {{ 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 }};
    double c2t[3][3];
    iauC2tcio(C, era, W, c2t);
    double t2c[3][3] = {{ 0.0 }};
    iauTr(c2t, t2c);

    double v1[3] = { -OMEGA * sta->x, OMEGA * sta->y, 0.0 };
    double v1R[3] = { 0.0 };
    iauRxp(t2c, v1, v1R);

    double k1a[3] = { 0.0 };
    double k1a_t1[3];

    k1a_t1[0] = ( EARTH_PARAMS->vel[0] + v1[0] ) / CMPS;
    k1a_t1[1] = ( EARTH_PARAMS->vel[1] + v1[1] ) / CMPS;
    k1a_t1[2] = ( EARTH_PARAMS->vel[2] + v1[2] ) / CMPS;

    double rqu[3] = { src->crs[0], src->crs[1], src->crs[2] };

    double k1a_t2[3] = { 0.0 };
    iauSxp( iauPdp( rqu, k1a_t1 ), rqu, k1a_t2 );
    k1a_t2[0] = -k1a_t2[0];
    k1a_t2[1] = -k1a_t2[1];
    k1a_t2[2] = -k1a_t2[2];

    double k1a_temp[3] = { 0.0 };
    iauPpp( rqu, k1a_t1, k1a_temp );
    iauPpp( k1a_temp, k1a_t2, k1a );

    double rq[3] = { 0.0 };
    iauRxp( c2t, k1a, rq );

    double g2l[3][3];
    Station_geo_to_loc(sta, g2l);

    double lq[3] = { 0.0 };
    iauRxp(g2l, rq, lq);

    double zd = acos(lq[2]);
    (*el) = DPI / 2.0 - zd;

    double saz = atan2(lq[1], lq[0]);
    if(lq[1] < 0.0) saz = DPI * 2.0 + saz;
    (*az) = fmod(saz + DPI, DPI * 2.0);
}

#undef OMEGA
#undef MJD_INITIAL

#define A 6378136.6
#define F 1.0 / 298.25642
#define E2 (2.0 * F - F * F)

void
Station_geo_to_loc(const Station* const sta, double g2l[static 3][3]) {
    double lon, lat, alt;
    lon = atan2(sta->x, sta->y);
    double r = sqrt(sta->x * sta->x + sta->y * sta->y);
    lat = atan2(sta->z, r);

    double N;
    for(size_t i = 0; i < 6; ++i) {
        N = A / sqrt(1.0 - E2 * sin(lat) * sin(lat));
        alt = r / cos(lat) - N;
        lat = atan2(sta->z * (N + alt), r * ((1.0 - E2) * N + alt));
    }

    double theta = DPI / 2.0 - lat;

    double theta_cos = cos(theta);
    double theta_sin = sin(theta);
    double roty[3][3] = {{ theta_cos, 0.0, -theta_sin }, { 0.0, -1.0, 0.0 }, { theta_sin, 0.0, theta_cos }};

    double lon_cos = cos(lon);
    double lon_sin = sin(lon);

    double rotz[3][3] = {{ lon_cos, lon_sin, 0.0 }, { -lon_sin, lon_cos, 0.0 }, { 0.0, 0.0, 1.0 }};
    iauRxr( roty, rotz, g2l);
}

#include "station.h"

#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>

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
    printf("    const. overhead: %u\n", sta->axes_limits[0].overhead);
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
    printf("    const. overhead: %u\n", sta->axes_limits[1].overhead);
    printf("    phys. min. [deg]: %.2lf\n", sta->axes_limits[1].limits[0]);
    printf("    phys. max. [deg]: %.2lf\n", sta->axes_limits[1].limits[1]);
}

#define A 6378136.6
#define F 1.0 / 298.25642
#define E2 (2.0 * F - F * F)

void
Station_lat_lon_alt(const Station* const sta, double* lon, double* lat, double* alt) {
    (*lon) = atan2(sta->y, sta->x);
    double r = sqrt(sta->x * sta->x + sta->y * sta->y);
    (*lat) = atan2(sta->z, r);
    double N;
    for(size_t i = 0; i < 6; ++i) {
        N = A / sqrt(1.0 - E2 * sin(*lat) * sin(*lat));
        (*alt) = r / cos(*lat) - N;
        (*lat) = atan2(sta->z * (N + (*alt)), r * ((1.0 - E2) * N + (*alt)));
    }
}

void
Station_geo_to_loc(const Station* const sta, double g2l[static 3][3]) {
    double lon, lat, alt;
    Station_lat_lon_alt(sta, &lon, &lat, &alt);

    double theta = DPI / 2.0 - lat;

    double theta_cos = cos(theta);
    double theta_sin = sin(theta);
    double roty[3][3] = {{ theta_cos, 0.0, -theta_sin }, { 0.0, -1.0, 0.0 }, { theta_sin, 0.0, theta_cos }};

    double lon_cos = cos(lon);
    double lon_sin = sin(lon);

    double rotz[3][3] = {{ lon_cos, lon_sin, 0.0 }, { -lon_sin, lon_cos, 0.0 }, { 0.0, 0.0, 1.0 }};
    iauRxr( roty, rotz, g2l);
}

#undef A
#undef F
#undef E2

#define OMEGA 7.2921151467069805e-05
#define MJD_INITIAL 2400000.5

void
Station_src_az_el(const Station* const sta, const Source* const src, unsigned int seconds, double* az, double* el) {
    DateTime dt = TIME_SYS->start;
    dt.sec += (double) seconds;
    double mjd = DateTime_to_mjd(dt);

    double era = iauEra00(MJD_INITIAL, mjd);

    double C[3][3] = {{ 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 }};

    size_t nut_idx = 0;
    while(EARTH_PARAMS->nut.t[nut_idx + 1] < seconds) ++nut_idx;

    unsigned int seconds_delta = seconds - EARTH_PARAMS->nut.t[nut_idx];

    double x, y, s;
    x = EARTH_PARAMS->nut.x[nut_idx] + (EARTH_PARAMS->nut.x[nut_idx + 1] - EARTH_PARAMS->nut.x[nut_idx]) / EARTH_PARAMS->nut.dt * seconds_delta;
    y = EARTH_PARAMS->nut.y[nut_idx] + (EARTH_PARAMS->nut.y[nut_idx + 1] - EARTH_PARAMS->nut.y[nut_idx]) / EARTH_PARAMS->nut.dt * seconds_delta;
    s = EARTH_PARAMS->nut.s[nut_idx] + (EARTH_PARAMS->nut.s[nut_idx + 1] - EARTH_PARAMS->nut.s[nut_idx]) / EARTH_PARAMS->nut.dt * seconds_delta;

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
    iauSxp(iauPdp(rqu, k1a_t1), rqu, k1a_t2);
    k1a_t2[0] = -k1a_t2[0];
    k1a_t2[1] = -k1a_t2[1];
    k1a_t2[2] = -k1a_t2[2];

    double k1a_temp[3] = { 0.0 };
    iauPpp(rqu, k1a_t1, k1a_temp);
    iauPpp(k1a_temp, k1a_t2, k1a);

    double rq[3] = { 0.0 };
    iauRxp(c2t, k1a, rq);

    double g2l[3][3];
    Station_geo_to_loc(sta, g2l);

    double lq[3] = { 0.0 };
    iauRxp(g2l, rq, lq);
    if(lq[2] >  1.0) lq[2] =  1.0;
    if(lq[2] < -1.0) lq[2] = -1.0;

    double zd = acos(lq[2]);
    (*el) = DPI / 2.0 - zd;

    double saz = atan2(lq[1], lq[0]);
    if(lq[1] < 0.0) saz = DPI * 2.0 + saz;
    (*az) = fmod(saz + DPI, DPI * 2.0);
}

#undef OMEGA
#undef MJD_INITIAL

void
Station_src_ha_dc(const Station* const sta, const Source* const src, unsigned int seconds, double* ha, double* dc) {
    DateTime dt = TIME_SYS->start;
    dt.sec += (double) seconds;
    double gmst = DateTime_to_gmst(dt) * DPI / 180.0; // TODO: It needs to be clear that DateTime_to_gmst returns degrees

    double lon, lat, alt;
    Station_lat_lon_alt(sta, &lon, &lat, &alt);

    (*dc) = src->decl_rad;
    (*ha) = gmst + lon - src->raan_rad;
    while((*ha) >  DPI) (*ha) -= D2PI;
    while((*ha) < -DPI) (*ha) += D2PI;
}

bool
Station_axis_inside_cable_wrap(const Station* const sta, double ax_fst, double ax_snd) {
    // TODO: Check importance of axes offsets
    // See: AbstractCableWrap.cpp:121
    struct { double offset[2]; } ax_off_fst = { 0 };
    struct { double offset[2]; } ax_off_snd = { 0 };
    struct dish_limits ax_lim_fst, ax_lim_snd;
    ax_lim_fst = sta->axes_limits[0];
    ax_lim_fst.limits[0] *= (DPI / 180.0);
    ax_lim_fst.limits[1] *= (DPI / 180.0);
    ax_lim_snd = sta->axes_limits[1];
    ax_lim_snd.limits[0] *= (DPI / 180.0);
    ax_lim_snd.limits[1] *= (DPI / 180.0);
    if(((ax_lim_fst.limits[1] - ax_off_fst.offset[1]) - (ax_lim_fst.limits[0] + ax_off_fst.offset[0])) < D2PI) {
        double ax_lim_fst_low = fmod(ax_lim_fst.limits[0] + ax_off_fst.offset[0], D2PI);
        double ax_lim_fst_up = fmod(ax_lim_fst.limits[1] - ax_off_fst.offset[1], D2PI);
        if(ax_lim_fst_up < ax_lim_fst_low) {
            // over 0 point
            if(((ax_fst < ax_lim_fst_low) && (ax_fst > ax_lim_fst_up)) || \
                (ax_snd < (ax_lim_snd.limits[0] + ax_off_snd.offset[0])) || \
                (ax_snd > (ax_lim_snd.limits[1] - ax_off_snd.offset[1]))) return false;
        } else {
            // not over 0 point
            if(((ax_fst < ax_lim_fst_low) || (ax_fst > ax_lim_fst_up)) || \
                (ax_snd < (ax_lim_snd.limits[0] + ax_off_snd.offset[0])) || \
                (ax_snd > (ax_lim_snd.limits[1] - ax_off_snd.offset[1]))) return false;
        }
    } else {
        if((ax_snd < (ax_lim_snd.limits[0] + ax_off_snd.offset[0])) || \
            ax_snd > (ax_lim_snd.limits[1] - ax_off_snd.offset[1])) return false;
    }
    return true;
}

bool
Station_src_visible(const Station* const sta, const Source* const src, unsigned int seconds) {
    double ax_fst, ax_snd;
    switch(sta->axes) {
    case AXES_AZEL:
        Station_src_az_el(sta, src, seconds, &ax_fst, &ax_snd);
        break;
    case AXES_HADC:
        Station_src_ha_dc(sta, src, seconds, &ax_fst, &ax_snd);
        break;
    case AXES_XYEW: {
        double az, el;
        Station_src_az_el(sta, src, seconds, &az, &el);
        ax_fst = atan2(cos(el) * cos(az), sin(el));
        ax_snd = asin(cos(el) * sin(az));
    } break;
    case AXES_XYNS: {
        double az, el;
        Station_src_az_el(sta, src, seconds, &az, &el);
        ax_fst = atan2(cos(el) * sin(az), sin(el)); // same as XYEW case, just rotated
        ax_snd = asin(cos(el) * cos(az));
    } break;
    default: HH_UNREACHABLE;
    }
    return Station_axis_inside_cable_wrap(sta, ax_fst, ax_snd);  
    // TODO: Must also check horizon masks and minimum station elevation
}

unsigned int
Station_slew_time_by_axis(const Station* const sta, double delta, bool snd) {
    double rate_deg = sta->axes_limits[snd].rate;
    double rate = rate_deg * (DPI / 180.0) / 60.0;
    double acc = rate;
    unsigned int overhead = sta->axes_limits[snd].overhead;
    double t_acc = rate / acc; // TODO: This is currently always 1.0
    double s_acc = 2.0 * (acc * t_acc * t_acc / 2.0); // TODO: Can be simplified?
    double t;
    if(delta < s_acc) t = 2.0 * sqrt(delta / acc);
    else t = 2.0 * t_acc + (delta - s_acc) / rate;
    if(fmod(t, 1.0) > 0.85) ++t;
    // TODO: Minor deviation from VieSched++
    // See: AbstractAntenna.cpp:73
    if(rate < 0.015) ++t;
    return (unsigned int) ceil(t) + overhead;
}

void azel_to_xyew(double az_cos, double az_sin, double el, double* ax1, double* ax2) {
    double el_cos = cos(el);
    double el_sin = sin(el);
    // default: EW case
    *ax1 = atan2(el_cos * az_cos, el_sin);
    *ax2 = asin(fmax(fmin(el_cos * az_sin, 1.0), -1.0));
}

unsigned int
Station_slew_time_raw(const Station* const sta, const Source* const src[2], unsigned int seconds[2]){
    // TODO: There are special cases for a few antennas that I need to handle
    // See: Initializer.cpp: 437
    double ax_fst[2], ax_snd[2];
    switch(sta->axes) {
    case AXES_AZEL:
        Station_src_az_el(sta, src[0], seconds[0], &ax_fst[0], &ax_fst[1]);
        Station_src_az_el(sta, src[1], seconds[1], &ax_snd[0], &ax_snd[1]);
        break;
    case AXES_HADC:
        Station_src_ha_dc(sta, src[0], seconds[0], &ax_fst[0], &ax_fst[1]);
        Station_src_ha_dc(sta, src[1], seconds[1], &ax_snd[0], &ax_snd[1]);
        break;
    case AXES_XYNS:
    case AXES_XYEW: {
        double az_fst, el_fst, az_snd, el_snd;
        Station_src_az_el(sta, src[0], seconds[0], &az_fst, &el_fst);
        Station_src_az_el(sta, src[1], seconds[1], &az_snd, &el_snd);
        double az_cos_fst, az_sin_fst, az_cos_snd, az_sin_snd;
        az_cos_fst = cos(az_fst);
        az_sin_fst = sin(az_fst);
        az_cos_snd = cos(az_snd);
        az_sin_snd = sin(az_snd);
        if(sta->axes == AXES_XYNS) {
            azel_to_xyew(az_sin_fst, az_cos_fst, el_fst, &ax_fst[0], &ax_fst[1]);
            azel_to_xyew(az_sin_snd, az_cos_snd, el_snd, &ax_snd[0], &ax_snd[1]);
        } else {
            azel_to_xyew(az_cos_fst, az_sin_fst, el_fst, &ax_fst[0], &ax_fst[1]);
            azel_to_xyew(az_cos_snd, az_sin_snd, el_snd, &ax_snd[0], &ax_snd[1]);
        }
    } break;
    default: HH_UNREACHABLE;
    }
    unsigned int t_fst, t_snd;
    t_snd = Station_slew_time_by_axis(sta, fabs(ax_fst[1] - ax_snd[1]), false);
    t_fst = Station_slew_time_by_axis(sta, fabs(ax_fst[0] - ax_snd[0]), true);
    return t_fst > t_snd ? t_fst : t_snd;
}

unsigned int
Station_slew_time(const Station* const sta, const Source* const src[2], unsigned int seconds[2]) {
    if(!Station_src_visible(sta, src[1], seconds[1])) return UINT_MAX;
    return Station_slew_time_raw(sta, src, seconds);
}

inline double 
wrap_to_two_pi(double angle) {
    angle = fmod(angle, D2PI);
    if(angle < 0) angle += D2PI;
    return angle;
}

size_t
Station_src_sky_cov_idx(const Station* const sta, const Source* const src, unsigned int seconds) {
    double az, el;
    Station_src_az_el(sta, src, seconds, &az, &el);
    size_t row = (size_t) floor(el / (DPI / 4.0));
    double n = row ? 4.0 : 9.0;
    size_t col = (size_t) round(wrap_to_two_pi(az) / (D2PI / n));
    if((double) col > n - 1) col = 0;
    return row ? col + 9 : col;
}

double
Station_baseline_dist(const Station* const sta_fst, const Station* const sta_snd) {
    double dx, dy, dz;
    dx = sta_fst->x - sta_snd->x;
    dy = sta_fst->y - sta_snd->y;
    dz = sta_fst->z - sta_snd->z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

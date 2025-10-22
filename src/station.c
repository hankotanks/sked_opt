#include "station.h"

#include <stdbool.h>
#include <stdint.h>
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
    iauRxr(roty, rotz, g2l);
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

    k1a_t1[0] = (EARTH_PARAMS->vel[0] + v1[0]) / CMPS;
    k1a_t1[1] = (EARTH_PARAMS->vel[1] + v1[1]) / CMPS;
    k1a_t1[2] = (EARTH_PARAMS->vel[2] + v1[2]) / CMPS;

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
    double lon, lat, alt;
    Station_lat_lon_alt(sta, &lon, &lat, &alt);
    (*dc) = src->decl_rad;
    (*ha) = DateTime_to_gmst(dt) + lon - src->raan_rad;
    while((*ha) >  DPI) (*ha) -= D2PI;
    while((*ha) < -DPI) (*ha) += D2PI;
}

bool
Station_axis_inside_cable_wrap(const Station* const sta, double ax_fst, double ax_snd) {
    // NOTE: Check importance of axes offsets
    // See: AbstractCableWrap.cpp:121
    struct { double offset[2]; } ax_off_fst = { 0 };
    struct { double offset[2]; } ax_off_snd = { 0 };
#define AX_OFF_LOW (5.0 * DPI / 180.0)
    ax_off_fst.offset[0] = AX_OFF_LOW;
    ax_off_snd.offset[0] = AX_OFF_LOW;
#undef AX_OFF_LOW
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
Station_src_visible_mask(const Station* const sta, double az, double el) {
    HH_ASSERT(sta->mask_count != 0, "Unreachable!");
    az = fmod(az, D2PI);
    if(az < 0.0) az += D2PI;
    // convert to deg  
    az /= DPI;
    az *= 180.0;
    el /= DPI;
    el *= 180.0;
    if(sta->mask_count % 2 == 0) {
        // line mask
        size_t mask_fst, mask_snd = 1;
        while(az > sta->mask[mask_snd * 2]) ++mask_snd;
        HH_ASSERT(mask_snd > 0, "Unreachable!");
        mask_fst = mask_snd - 1;
        double delta = az - sta->mask[mask_fst * 2];
        return el >= (sta->mask[mask_fst * 2 + 1] + 
            (sta->mask[mask_snd * 2 + 1] - sta->mask[mask_fst * 2 + 1]) / 
            (sta->mask[mask_snd * 2] - sta->mask[mask_fst * 2]) * delta);
    } else {
        // step mask
        size_t mask = 1;
        while(az > sta->mask[mask * 2]) ++mask;
        return el >= sta->mask[mask * 2 - 1];
    }
}

bool
Station_src_visible(const Station* const sta, const Source* const src, unsigned int seconds) {
    // check against minimum elevation
    double az, el;
    Station_src_az_el(sta, src, seconds, &az, &el); 
    if((el * 180.0 / DPI) < sta->mask_min) return false;
    // check horizon mask if available
    if(sta->mask_count > 0 && !Station_src_visible_mask(sta, az, el)) 
        return false;
    // calculate source availability given antenna axes limitations
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
}

unsigned int
Station_slew_time_by_axis(const Station* const sta, double delta, bool snd) {
    double rate_deg = sta->axes_limits[snd].rate;
    double rate = rate_deg * DPI / 180.0;
    double acc = rate;
    unsigned int overhead = sta->axes_limits[snd].overhead;
    double t_acc = rate / acc; // NOTE: This is currently always 1.0
    double s_acc = 2.0 * (acc * t_acc * t_acc / 2.0); // NOTE: Can be simplified?
    double t;
    if(delta < s_acc) t = 2.0 * sqrt(delta / acc);
    else t = 2.0 * t_acc + (delta - s_acc) / rate;
    if(fmod(t, 1.0) > 0.85) ++t;
    // NOTE: Minor deviation from VieSched++
    // See: AbstractAntenna.cpp:73
    if(rate < 0.015) ++t;
    return (unsigned int) ceil(t) + overhead;
}

void 
azel_to_xyew(double az_cos, double az_sin, double el, double* ax1, double* ax2) {
    double el_cos = cos(el);
    double el_sin = sin(el);
    // default: EW case
    *ax1 = atan2(el_cos * az_cos, el_sin);
    *ax2 = asin(fmax(fmin(el_cos * az_sin, 1.0), -1.0));
}

inline double
slew_time_GGAO_helper(double x1, double x2, double vel, double acc) {
    double dist = fabs(x1 - x2);
    double t_acc = vel / acc;
    return (dist <= acc * t_acc * t_acc) ? (2.0 * sqrt(dist / acc)) : (dist / vel + t_acc);
}

unsigned int
slew_time_ggao12m(const Station* const sta, const Source* const src[2], unsigned int seconds[2]) {
    // NOTE: Taken from SKED's ggao_slew.f and referenced from Antenna_GGAO.cpp:32

    double az_off = sta->axes_limits[0].overhead;
    double el_off = sta->axes_limits[1].overhead;

    double az_vel = sta->axes_limits[0].rate * DPI / 180.0; 
    double el_vel = sta->axes_limits[1].rate * DPI / 180.0; 

    double az_beg, az_end, el_beg, el_end;
    Station_src_az_el(sta, src[0], seconds[0], &az_beg, &el_beg); // starting point
    Station_src_az_el(sta, src[1], seconds[1], &az_end, &el_end); // ending point

    double az_pk1 = 192.0;
    double az_pk2 = 552.0;
    double el_pk = 42.0;
    double fudge = 1.0;
    double half_width = el_pk;

    double az_pk1_lft = az_pk1 - half_width;
    double az_pk1_rt = az_pk1 + half_width;
    double az_pk2_lft = az_pk2 - half_width;
    double az_pk2_rt = az_pk2 + half_width;

    double az_acc = az_vel / az_off;
    double el_acc = el_vel / el_off;

    double tmp;
    if(az_beg > az_end) {
        tmp = az_beg;
        az_beg = az_end;
        az_end = tmp;
        tmp = el_beg;
        el_beg = el_end;
        el_end = tmp;
    }

    double el_slewt = slew_time_GGAO_helper(el_beg, el_end, el_vel, el_acc);
    double az_slewt = slew_time_GGAO_helper(az_beg, az_end, az_vel, az_acc);

    double slew0 = HH_MAX(az_slewt, el_slewt);
    
    // Above the mask
    if(el_beg >= el_pk && el_end >= el_pk) return (unsigned int) ceil(slew0);
    // Both to the left of the first mask
    if(az_beg <= az_pk1_lft && az_end <= az_pk1_lft) return (unsigned int) ceil(slew0);
    // Both to the right of the second mask
    if(az_beg >= az_pk2_rt && az_end >= az_pk2_rt) return (unsigned int) ceil(slew0);
    // Both between the masks
    if((az_beg >= az_pk1_rt && az_beg <= az_pk2_lft ) && ( az_end >= az_pk1_rt && az_end <= az_pk2_lft)) return (unsigned int) ceil(slew0);

    // This handles case where starting and ending below mask and both starting and ending points are in same valley.
    // starting and ending below the peaks  
    if((el_beg <= el_pk && el_end <= el_pk)) {
        // Both to the left of the first mask.
        if(az_beg <= az_pk1 && az_end <= az_pk1) return (unsigned int) ceil(slew0);
        if(az_beg >= az_pk2 && az_end >= az_pk2) return (unsigned int) ceil(slew0);
        if((az_beg >= az_pk1 && az_beg <= az_pk2) && (az_end >= az_pk1 && az_end <= az_pk2)) return (unsigned int) ceil(slew0);
    }

    // Handle some rare cases.  Both within LHS of mask or RHS of mask.   Assume normal slewing.
    if((az_beg >= az_pk1_lft && az_beg <= az_pk1) && (az_end >= az_pk1_lft && az_end <= az_pk1)) return (unsigned int) ceil(slew0);
    if((az_beg >= az_pk2_lft && az_beg <= az_pk2) && (az_end >= az_pk2_lft && az_end <= az_pk2)) return (unsigned int) ceil(slew0);
    if((az_beg >= az_pk1 && az_beg <= az_pk1_rt) && (az_end >= az_pk1 && az_end <= az_pk1_rt)) return (unsigned int) ceil(slew0);
    if((az_beg >= az_pk2 && az_beg <= az_pk2_rt) && (az_end >= az_pk2 && az_end <= az_pk2_rt)) return (unsigned int) ceil(slew0);

    // In the region of a peak and going up. This is OK if going up from right side of peak.
    if(el_end > el_beg && el_end > el_pk) {
        if((az_beg > az_pk1 && az_beg < az_pk1_rt) || (az_beg > az_pk2 && az_beg < az_pk2_rt)) return (unsigned int) ceil(slew0);
    }
    // In the region of a peak and going done. This is OK if coming down from left side.
    if(el_beg > el_end && el_beg > el_pk) {
        if((az_end > az_pk1_lft && az_end < az_pk1) || (az_end > az_pk2_lft && az_end < az_pk2)) return (unsigned int) ceil(slew0);
    }

    // For many of the remaining cases we split the motion into two or three line segments.
    // Each line segment starts or ends at a peak.
    double el_mid = el_pk + fudge;  // for many parts below assume that one line segment ends at a peak.

    // FIRST CASE.
    // The beginning and ending elevation are below the peak.
    // This means that we start in one valley and end in another.
    // (The case where we started and ended in the same valley are covered above.)

    // We split the calculation into several segments.
    // 1. To the top of a peak.
    // 2. Down from a peak.  (May not be the first peak as before.
    // 3. Optional:  travel time between the peaks.
    // For segments 1&2:
    //    For the elevation time we add in the full-offset since we come to a stop.
    //    For the azimuth time we add in only 1/2 the offset since we only have to account for starting acceleration.
    // For segment 3
    //    We  don't have to account for azimuth acceleration since we are already at speed.
    if(el_beg <= el_mid && el_end <= el_mid) { // both starting and ending points below a peak.
        // Break the problem into pieces.
        // 1. What peak do we have to climb?
        // 2. What peak do we descend.
        // 3. Did we go over both peaks.

        // 1. Find which peak we are climbing
        double az_mid1 = (az_beg <= az_pk1) ? az_pk1_lft : az_pk2_lft;
 
        // Find slew time for first segment.
        az_mid1 = HH_MAX(az_beg, az_mid1);  // handles rare case when within rectangular mask
        double az_slew1 = slew_time_GGAO_helper(az_beg, az_mid1, az_vel, az_acc);
        double el_slew1 = slew_time_GGAO_helper(el_beg, el_mid, el_vel, el_acc);

        // 2. Find which peak we are descending
        double az_mid2 = (az_end >= az_pk2) ? az_pk2_rt : az_pk1_rt;
 
        // Find slew time for second segment
        az_mid2 = HH_MIN(az_mid2, az_end);  // handles rare case when within rectangular mask
        double az_slew2 = slew_time_GGAO_helper(az_mid2, az_end, az_vel, az_acc);
        double el_slew2 = slew_time_GGAO_helper(el_mid, el_end, el_vel, el_acc);

        // Slew values used for comparison of time.
        // Subtract 1/2 offset because we don't worry about stopping/starting
        double el_slew2p = el_slew2 - el_off / 2.0;
        double az_slew1p = az_slew1 - az_off / 2.0;
        double az_slew2p = az_slew2 - az_off / 2.0;
        double el_slew1p = el_slew1 - el_off / 2.0;

        double slewt;
        if(az_slew1p >= el_slew1p && az_slew2p >= el_slew2p) // One very long slew in azimuth
            slewt = slew_time_GGAO_helper( az_beg, az_end, az_vel, az_acc);
        // A long slew in Az followed by the descent in Elevation
        // Subtract 1/2 of the offset because this coincides with el starting.
        else if(az_slew1p >= el_slew1p && az_slew2p <= el_slew2p) 
            slewt = slew_time_GGAO_helper(az_beg, az_mid2, az_vel, az_acc) + el_slew2 - az_off / 2.0;
        else if(az_slew1p <= el_slew1p && az_slew2p >= el_slew2p)
            slewt = el_slew1 + slew_time_GGAO_helper(az_mid1, az_end, az_vel, az_acc) - az_off / 2.0;
        else slewt = el_slew1 + (az_mid2 - az_mid1) / az_vel + el_slew2;

        return (unsigned int) ceil(slewt);
    }

    // SECOND CASE
    // Start in a valley and and above a peak
    // --OR--
    // Start above a peak and end in a valley.
    // In both cases ceck if we would hit a peak in the normal course of business.
    // If we don't can use the normal slewing.

    // First case. Start low, come up high.
    double az_mid1;
    if(el_beg < el_end) az_mid1 = (az_beg < az_pk1) ? az_pk1_lft : az_pk2_lft;
    // Start high, come down low
    else az_mid1 = (az_beg < az_pk1_rt) ? az_pk1_rt : az_pk2_rt;

    az_mid1 = HH_MAX(az_beg, az_mid1);  // middle can't be before beginning
    az_mid1 = HH_MIN(az_mid1, az_end);  // middle can't be after ending

    double az_slew1 = slew_time_GGAO_helper(az_beg, az_mid1, az_vel, az_acc);
    double el_slew1 = slew_time_GGAO_helper(el_beg, el_mid, el_vel, el_acc);
    (void) az_slew1;
    (void) el_slew1;

    // This is slew time used for comparison. Don't worry about stopping
    double az_slew1p = fabs(az_beg - az_mid1) / az_vel + az_off / 2.0;
    double el_slew1p = fabs(el_beg - el_mid) / el_vel + el_off / 2.0;
    
    double slewt;
    if(el_beg < el_end) {
        if(az_slew1p >= el_slew1p) return (unsigned int) ceil(slew0); // Don't hit side on the way up. Normal slew.
        // Two possibilities.
        // 1. A long slew in elevation
        // 2. A slew in elevation followed by one in azimuth
        double az_slew2 = slew_time_GGAO_helper(az_mid1, az_end, az_vel, az_acc);
        slewt = HH_MAX(el_slewt, el_slew1p + az_slew2);
    } else {
        if(el_slew1p > az_slew1p) return (unsigned int) ceil(slew0); // don't hit top on the way down. Normal slew
        // Two possibilities.
        // 1. A long slew in azimuth
        // 2. A slew in azimuth followed by one in elevation.
        double el_slew2 = slew_time_GGAO_helper(el_mid, el_end, el_vel, el_acc);
        // Use az_slew1p because antenna is still moving. It will stop while el is moving.
        slewt = HH_MAX(az_slewt, az_slew1p + el_slew2);
    }

    return (unsigned int) ceil(slewt);
}

struct rate_onsala {
    double very_slow_rate;
    double slow_rate;
    double normal_rate;
    double very_slow_lower;
    double slow_lower;
    double slow_upper;
    double very_slow_upper;
};

const struct rate_onsala RATE_AZ = {
    DPI / 180.0 *   1.0, 
    DPI / 180.0 *   3.5, 
    DPI / 180.0 *  12.0, 
    DPI / 180.0 * -65.0, 
    DPI / 180.0 * -40.0, 
    DPI / 180.0 * 400.0, 
    DPI / 180.0 * 425.0
};

const struct rate_onsala RATE_EL = {
    DPI / 180.0 *  0.3, 
    DPI / 180.0 *  3.5, 
    DPI / 180.0 *  6.0, 
    DPI / 180.0 *  5.0, 
    DPI / 180.0 * 15.0, 
    DPI / 180.0 * 85.0, 
    DPI / 180.0 * 95.0
};

double
slew_time_rate_onsala_a(const struct rate_onsala* const rate, double start, double end) {
    if (start <= rate->very_slow_lower && end <= rate->very_slow_lower)
        return fabs(end - start) / ((end < start) ? rate->very_slow_rate : rate->normal_rate);
    if(start < rate->very_slow_lower)
        return (rate->very_slow_lower - start) / rate->normal_rate;
    if(end < rate->very_slow_lower)
        return (rate->very_slow_lower - end) / rate->very_slow_rate;
    return 0.0;
}

double
slew_time_rate_onsala_b(const struct rate_onsala* const rate, double start, double end) {
    if((start >= rate->slow_lower && end >= rate->slow_lower) || (start <= rate->very_slow_lower && end <= rate->very_slow_lower)) 
        return 0.0;
    if(end < start) return (HH_MIN(start, rate->slow_lower) - HH_MAX(rate->very_slow_lower, end)) / rate->slow_rate;
    else return (HH_MIN(rate->slow_lower, end) - HH_MAX(start, rate->very_slow_lower)) / rate->normal_rate;
}

double
slew_time_rate_onsala_c(const struct rate_onsala* const rate, double start, double end) {
    if((start <= rate->slow_lower && end <= rate->slow_lower) || (start >= rate->slow_upper && end >= rate->slow_upper))
        return 0.0;
    // full slew through C section (unlikely)
    if((start < rate->slow_lower && end > rate->slow_upper) || (start > rate->slow_upper && end < rate->slow_lower)) {
        return (rate->slow_upper - rate->slow_lower) / rate->normal_rate;
    } else {
        double tmp;
        if(start > end) {
            tmp = start;
            start = end;
            end = tmp;
        }
        return (HH_MIN(end, rate->slow_upper) - HH_MAX(start, rate->slow_lower)) / rate->normal_rate;
    }
}

double
slew_time_rate_onsala_d(const struct rate_onsala* const rate, double start, double end) {
    if((start <= rate->slow_upper && end <= rate->slow_upper) || (start >= rate->very_slow_upper && end >= rate->very_slow_upper))
        return 0.0;
    if(end > start) return (HH_MIN(rate->very_slow_upper, end) - HH_MAX(start, rate->slow_upper)) / rate->slow_rate;
    else return (HH_MIN(rate->very_slow_upper, start) - HH_MAX(end, rate->slow_upper)) / rate->normal_rate;
}

double
slew_time_rate_onsala_e(const struct rate_onsala* const rate, double start, double end) {
    if(start >= rate->very_slow_upper && end >= rate->very_slow_upper)
        return fabs(end - start) / ((start < end) ? rate->very_slow_rate : rate->normal_rate);
    if(start >= rate->very_slow_upper)
        return (start - rate->very_slow_upper) / rate->normal_rate;
    if(end >= rate->very_slow_upper)
        return (end - rate->very_slow_upper) / rate->very_slow_rate;
    return 0.0;
}

unsigned int
slew_time_rate_onsala(const struct rate_onsala* const rate, double start, double end) {
    double a, b, c, d, e;
    a = slew_time_rate_onsala_a(rate, start, end);
    b = slew_time_rate_onsala_b(rate, start, end);
    c = slew_time_rate_onsala_c(rate, start, end);
    d = slew_time_rate_onsala_d(rate, start, end);
    e = slew_time_rate_onsala_e(rate, start, end);
    return (unsigned int) (a + b + c + d + e);
}

unsigned int
slew_time_onsala(const Station* const sta, const Source* const src[2], unsigned int seconds[2]) {
    double az_fst, el_fst, az_snd, el_snd;
    Station_src_az_el(sta, src[0], seconds[0], &az_fst, &el_fst);
    Station_src_az_el(sta, src[0], seconds[0], &az_snd, &el_snd);
    unsigned int t_az, t_el;
    t_az = slew_time_rate_onsala(&RATE_AZ, az_fst, az_snd) + sta->axes_limits[0].overhead;
    t_el = slew_time_rate_onsala(&RATE_EL, el_fst, el_snd) + sta->axes_limits[1].overhead;
    return (t_az > t_el) ? t_az : t_el;
}

unsigned int
slew_time_vlba_pietown_helper(double delta, double rate, double acc, double dec, double settle) {
    double t_acc = rate / acc;
    double t_dec = rate / dec;
    double s_acc = acc * t_acc * t_acc / 2.0;
    double s_dec = dec * t_dec * t_dec / 2.0;
    double t;
    if(delta < s_acc + s_dec) {
        double t1 = (sqrt(2.0) * sqrt(dec) * sqrt(delta)) / (sqrt(acc * (acc + dec)));
        double t2 = (sqrt(2.0) * acc * sqrt(delta)) / (sqrt(dec) * sqrt(acc * (acc + dec)));
        t = t1 + t2 + settle;
    } else t = t_acc + t_dec + (delta - s_acc - s_dec) / rate + settle;
    return (unsigned int) ceil(t);
}

unsigned int
slew_time_vlba_pietown(const Station* const sta, const Source* const src[2], unsigned int seconds[2]) {
    double az_fst, el_fst, az_snd, el_snd;
    Station_src_az_el(sta, src[0], seconds[0], &az_fst, &el_fst);
    Station_src_az_el(sta, src[1], seconds[1], &az_snd, &el_snd);
    double delta1 = fabs(az_fst - az_snd);
    double delta2 = fabs(el_fst - el_snd);
#define AZ_ACC (0.750 * DPI / 180.0)
#define AZ_DEC (0.750 * DPI / 180.0)
#define EL_ACC (0.250 * DPI / 180.0)
#define EL_DEC (0.250 * DPI / 180.0)
    unsigned int t_fst = slew_time_vlba_pietown_helper(delta1, sta->axes_limits[0].rate * DPI / 180.0, AZ_ACC, AZ_DEC, sta->axes_limits[0].overhead);
    unsigned int t_snd = slew_time_vlba_pietown_helper(delta2, sta->axes_limits[1].rate * DPI / 180.0, EL_ACC, EL_DEC, sta->axes_limits[1].overhead);
#undef AZ_ACC
#undef AZ_DEC
#undef EL_ACC
#undef EL_DEC
    return (t_fst > t_snd) ? t_fst : t_snd;
}

unsigned int
Station_slew_time_raw(const Station* const sta, const Source* const src[2], unsigned int seconds[2]) {
    if(sta->id[0] == 'G' && sta->id[1] == 's') {
        // special case for GGAO12M
        return slew_time_ggao12m(sta, src, seconds);
    } else if(sta->id[0] == 'O' && (sta->id[1] == 'w' || sta->id[1] == 'e')) {
        // special case for ONSA13SW and ONSA13NE
        return slew_time_onsala(sta, src, seconds);
    } else {
        // special case for all stations ending in "VLBA" and PIETOWN
        size_t sta_name_len = cat_name_len(sta->name);
        if(sta_name_len > 4) {
            bool matches = true;
            for(size_t i = 0; i < 4; ++i) matches &= sta->name[sta_name_len - 4 + i] == "VLBA"[i];
            matches |= (sta->id[0] == 'P' && sta->id[1] == 't');
            if(matches) return slew_time_vlba_pietown(sta, src, seconds);
        }
    }
    // standard cases
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
    return (t_fst > t_snd) ? t_fst : t_snd;
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
Station_sky_cov_idx_13v1(const Station* const sta, const Source* const src, unsigned int seconds) {
    double az, el;
    Station_src_az_el(sta, src, seconds, &az, &el);
    size_t row = (size_t) floor(el / (DPI / 4.0));
    double n = row ? 4.0 : 9.0;
    size_t col = (size_t) round(wrap_to_two_pi(az) / (D2PI / n));
    if((double) col > n - 1.0) col = 0;
    size_t idx = row ? col + 9 : col;
    HH_ASSERT(idx < 13, "Unreachable!");
    return idx;
}

size_t
Station_sky_cov_idx_13v2(const Station* const sta, const Source* const src, unsigned int seconds) {
    double el_space = DPI / 5.5;
    double az, el;
    Station_src_az_el(sta, src, seconds, &az, &el);
    size_t row = (size_t) floor(el / el_space);
    if(row > 1) return 12;
    double n = row ? 4.0 : 8.0;
    double az_space = D2PI / n;
    size_t col = (size_t) round(wrap_to_two_pi(az / az_space));
    if((double) col > n - 1.0) col = 0;
    size_t idx = row ? col + 8 : col;
    HH_ASSERT(idx < 13, "Unreachable!");
    return idx;
}

size_t
Station_sky_cov_idx_25v1(const Station* const sta, const Source* const src, unsigned int seconds) {
    double el_space = DPI / 6.0;
    double az, el;
    Station_src_az_el(sta, src, seconds, &az, &el);
    size_t row = (size_t) floor(el / el_space);
    double n;
    switch(row) {
    case 0 : n = 13.0; break;
    case 1 : n =  9.0; break;
    default: n =  3.0;
    }
    double az_space = D2PI / n;
    size_t col = (size_t) round(wrap_to_two_pi(az / az_space));
    if((double) col > n - 1.0) col = 0;
    size_t idx;
    switch(row) {
    case 0 : idx = col; break;
    case 1 : idx = col + 13; break;
    default: idx = col + 22;
    }
    HH_ASSERT(idx < 25, "Unreachable!");
    return idx;
}

size_t
Station_sky_cov_idx_25v2(const Station* const sta, const Source* const src, unsigned int seconds) {
    double el_space = DPI / 7.5;
    double az, el;
    Station_src_az_el(sta, src, seconds, &az, &el);
    size_t row = (size_t) floor(el / el_space);
    double n;
    switch(row) {
    case 0 : n = 12.0; break;
    case 1 : n =  8.0; break;
    case 2 : n =  4.0; break;
    default: return 24;
    }
    double az_space = D2PI / n;
    size_t col = (size_t) round(wrap_to_two_pi(az / az_space));
    if((double) col > n - 1.0) col = 0;
    size_t idx = col;
    switch(row) {
    case 0 : break;
    case 1 : idx += 12; break;
    case 2 : idx += 20; break;
    default: HH_UNREACHABLE;
    }
    HH_ASSERT(idx < 25, "Unreachable!");
    return idx;
}

size_t
Station_sky_cov_idx_37v1(const Station* const sta, const Source* const src, unsigned int seconds) {
    double el_space = DPI / 8.0;
    double az, el;
    Station_src_az_el(sta, src, seconds, &az, &el);
    size_t row = (size_t) floor(el / el_space);
    double n;
    switch(row) {
    case 0 : n = 14.0; break;
    case 1 : n = 12.0; break;
    case 2 : n =  8.0; break;
    default: n =  3.0;
    }
    double az_space = D2PI / n;
    size_t col = (size_t) round(wrap_to_two_pi(az / az_space));
    if((double) col > n - 1.0) col = 0;
    size_t idx = col;
    switch(row) {
    case 0: break;
    case 1 : idx += 14; break;
    case 2 : idx += 26; break; 
    default: idx += 34;
    }
    HH_ASSERT(idx < 37, "Unreachable!");
    return idx;
}

size_t
Station_sky_cov_idx_37v2(const Station* const sta, const Source* const src, unsigned int seconds) {
    double el_space = DPI / 9.5;
    double az, el;
    Station_src_az_el(sta, src, seconds, &az, &el);
    size_t row = (size_t) floor(el / el_space);
    double n;
    switch(row) {
    case 0 : n = 13.0; break;
    case 1 : n = 12.0; break;
    case 2 : n =  7.0; break;
    case 3 : n =  4.0; break;
    default: return 36;
    }
    double az_space = D2PI / n;
    size_t col = (size_t) round(wrap_to_two_pi(az / az_space));
    if((double) col > n - 1.0) col = 0;
    size_t idx = col;
    switch(row) {
    case 0 : break;
    case 1 : idx += 13; break;
    case 2 : idx += 25; break;
    case 3 : idx += 32; break; 
    default: HH_UNREACHABLE;
    }
    HH_ASSERT(idx < 37, "Unreachable!");
    return idx;
}

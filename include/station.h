#ifndef STATION_H__
#define STATION_H__

#include <stdbool.h>
#include <stdint.h>

#include "cat.h"
#include "source.h"

typedef struct {
    char id[2];
    char name[8];
    double x, y, z;
    double lon, lat;
    enum dish_axes axes;
    struct dish_limits axes_limits[2];
    // TODO: Figure out which bands each station can observe
    bool band[BAND_OTHER];
    double sefd[BAND_OTHER];
} Station;

void
Station_dump(const Station* const sta);
void
Station_lat_lon_alt_from_crs(const Station* const sta, 
    double* lon, double* lat, double* alt);
void
Station_geo_to_loc(const Station* const sta, double g2l[static 3][3]);
void
Station_az_el(const Station* const sta, const Source* const src, unsigned int seconds,
    double* az, double* el);
void
Station_ha_dc(const Station* const sta, const Source* const src, unsigned int seconds,
    double* ha, double* dc);
bool
Station_axis_inside_cable_wrap(const Station* const sta, double axis_fst, double axis_snd);
bool
Station_src_is_vis(const Station* const sta, const Source* const src, unsigned int seconds);
unsigned int
Station_slew_time(const Station* const sta, const Source* const src_fst, const Source* const src_snd, unsigned int seconds_fst, unsigned int seconds_snd);

#endif // STATION_H__

#ifndef STATION_H__
#define STATION_H__

#include "cat.h"
#include "source.h"

typedef struct {
    char id[2];
    char name[8];
    double x, y, z, lat, lon;
    enum dish_axes axes;
    struct dish_limits axes_limits[2];
    // TODO: Figure out which bands each station can observe
    bool band[BAND_OTHER];
    double sefd[BAND_OTHER];
} Station;

void
Station_dump(const Station* const sta);
void
Station_az_el(const Station* const sta, const Source* const src, unsigned int seconds,
    double* az, double* el);
void
Station_geo_to_loc(const Station* const sta, double g2l[static 3][3]);

#endif // STATION_H__

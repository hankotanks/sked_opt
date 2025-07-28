#include "station.h"
#include "cat.h"

#include <stdbool.h>
#include <stdio.h>

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

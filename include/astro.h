#ifndef ASTRO_H__
#define ASTRO_H__

struct ASTRO_H__EARTH_PARAMS {
    struct {
        double* x;
        double* y;
        double* s;
        unsigned int* t, dt;
    } nut;
    double vel[3];
};

extern struct ASTRO_H__EARTH_PARAMS* EARTH_PARAMS;

void
earth_params_init(void);
void
earth_params_free(void);

#endif // ASTRO_H__

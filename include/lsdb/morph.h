#ifndef MORPH_H
#define MORPH_H

#include <stdbool.h>

typedef struct _morph_t morph_t;

morph_t *morph_new(size_t np);
void morph_free(morph_t *m);

bool morph_init(morph_t *m,
    const double *xf, const double *yf, size_t lenf,
    const double *xg, const double *yg, size_t leng);

double morph_eval(const morph_t *m, double t, double x, bool normalize);

bool morph_get_domain(const morph_t *m, double *xmin, double *xmax);

#endif  /* MORPH_H */

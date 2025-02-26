#ifndef MORPHP_H
#define MORPHP_H

#include <stddef.h>
#include <gsl/gsl_spline.h>

#include <morph.h>

struct _morph_t {
    size_t np;
    gsl_spline *spline_f, *spline_M;
    gsl_interp_accel *acc_f, *acc_M;
    double xmin, xmax;
    double norm_f, norm_g;
};

typedef struct {
    gsl_spline *spline_g, *spline_f_inv;
    gsl_interp_accel *acc_g, *acc_f_inv;
    double *F, *G;
    double *x, *M;
} morph_aux_t;

morph_aux_t *morph_aux_new(size_t np);
void morph_aux_free(morph_aux_t *m);

#endif  /* MORPHP_H */

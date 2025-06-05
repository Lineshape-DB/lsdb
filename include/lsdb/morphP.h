/*
 * This file is part of the LSDB library & utilities.
 *
 * Copyright (C) 2025 Weizmann Institute of Science
 *
 * Author: Evgeny Stambulchik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * The license text can be found in the LGPL-3.0.txt file.
 */

#ifndef MORPHP_H
#define MORPHP_H

#include <stddef.h>
#include <gsl/gsl_spline.h>

#include <lsdb/morph.h>

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

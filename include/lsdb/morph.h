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

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

#include <stdbool.h>

#ifndef LSDB_H
#define LSDB_H

#define LSDB_SUCCESS    0
#define LSDB_FAILURE    1

typedef enum {
    LSDB_ACCESS_RO,
    LSDB_ACCESS_RW,
    LSDB_ACCESS_INIT
} lsdb_access_t;

typedef enum {
    LSDB_UNITS_NONE = 0,
    LSDB_UNITS_INV_CM,
    LSDB_UNITS_EV,
    LSDB_UNITS_AU,
    LSDB_UNITS_CUSTOM = 99
} lsdb_units_t;

typedef struct _lsdb_t lsdb_t;

typedef struct _lsdb_interp_t lsdb_interp_t;

typedef struct {
    unsigned long id;
    const char *name;
    const char *descr;
} lsdb_model_t;

typedef struct {
    unsigned long id;
    const char *name;
    const char *descr;
} lsdb_environment_t;

typedef struct {
    unsigned long id;
    const char *sym;
    unsigned int anum;
    double mass;
    unsigned int zsp;
} lsdb_radiator_t;

typedef struct {
    unsigned long id;
    const char *name;
    double energy;
} lsdb_line_t;

typedef struct {
    unsigned long id;
    unsigned long mid;
    unsigned long eid;
    double n;
    double T;
} lsdb_dataset_t;

typedef struct {
    double n;
    double T;
    size_t len;
    double *x;
    double *y;
} lsdb_dataset_data_t;

typedef struct {
    unsigned long id;
    const char *name;
    const char *value;
} lsdb_line_property_t;

typedef int (*lsdb_model_sink_t)(const lsdb_t *lsdb,
    const lsdb_model_t *m, void *udata);
typedef int (*lsdb_environment_sink_t)(const lsdb_t *lsdb,
    const lsdb_environment_t *e, void *udata);
typedef int (*lsdb_radiator_sink_t)(const lsdb_t *lsdb,
    const lsdb_radiator_t *r, void *udata);
typedef int (*lsdb_line_sink_t)(const lsdb_t *lsdb,
    const lsdb_line_t *l, void *udata);
typedef int (*lsdb_dataset_sink_t)(const lsdb_t *lsdb,
    const lsdb_dataset_t *cbdata, void *udata);
typedef int (*lsdb_line_property_sink_t)(const lsdb_t *lsdb,
    const lsdb_line_property_t *l, void *udata);

lsdb_t *lsdb_open(const char *fname, lsdb_access_t access);
void lsdb_close(lsdb_t *lsdb);

int lsdb_set_units(lsdb_t *lsdb, lsdb_units_t units);
lsdb_units_t lsdb_get_units(const lsdb_t *lsdb);

int lsdb_add_model(lsdb_t *lsdb, const char *name, const char *descr);
int lsdb_get_models(const lsdb_t *lsdb,
    lsdb_model_sink_t sink, void *udata);
int lsdb_del_model(lsdb_t *lsdb, unsigned long id);

int lsdb_add_environment(lsdb_t *lsdb, const char *name, const char *descr);
int lsdb_get_environments(const lsdb_t *lsdb,
    lsdb_environment_sink_t sink, void *udata);
int lsdb_del_environment(lsdb_t *lsdb, unsigned long id);

int lsdb_add_radiator(lsdb_t *lsdb,
    const char *symbol, int anum, double mass, int zsp);
int lsdb_get_radiators(const lsdb_t *lsdb,
    lsdb_radiator_sink_t sink, void *udata);
int lsdb_del_radiator(lsdb_t *lsdb, unsigned long id);

int lsdb_add_line(lsdb_t *lsdb,
    unsigned int rid, const char *name, double energy);
int lsdb_get_lines(const lsdb_t *lsdb, unsigned long rid,
    lsdb_line_sink_t sink, void *udata);
int lsdb_del_line(lsdb_t *lsdb, unsigned long id);

int lsdb_add_line_property(lsdb_t *lsdb,
    unsigned int lid, const char *name, const char *value);
int lsdb_get_line_properties(const lsdb_t *lsdb, unsigned long lid,
    lsdb_line_property_sink_t sink, void *udata);
int lsdb_del_line_property(lsdb_t *lsdb, unsigned long id);

int lsdb_add_dataset(lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double n, double T,
    const double *x, const double *y, size_t len);
int lsdb_get_datasets(const lsdb_t *lsdb, unsigned long lid,
    lsdb_dataset_sink_t sink, void *udata);
int lsdb_del_dataset(lsdb_t *lsdb, unsigned long id);

lsdb_dataset_data_t *lsdb_get_dataset_data(const lsdb_t *lsdb, int did);
void lsdb_dataset_data_free(lsdb_dataset_data_t *ds);

lsdb_dataset_data_t *lsdb_dataset_data_new(double n, double T, size_t len);

int lsdb_get_limits(const lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double *nmin, double *nmax, double *Tmin, double *Tmax);

lsdb_interp_t *lsdb_prepare_interpolation(const lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double n, double T, unsigned int len);
lsdb_dataset_data_t *lsdb_get_interpolation(const lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double n, double T, unsigned int len, double sigma, double gamma);
void lsdb_interp_free(lsdb_interp_t *interp);
int lsdb_interp_get_domain(const lsdb_interp_t *interp, double *xmin, double *xmax);
double lsdb_interp_eval(const lsdb_interp_t *interp, double x, bool normalize);

double lsdb_get_doppler_sigma(const lsdb_t *lsdb, unsigned long lid, double T);

double lsdb_convert_units(lsdb_units_t from_units, lsdb_units_t to_units);
double lsdb_convert_to_units(const lsdb_t *lsdb, lsdb_units_t to_units);

#endif /* LSDB_H */

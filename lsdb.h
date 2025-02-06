#ifndef LSDB_H
#define LSDB_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>

#define LSDB_SUCCESS    0
#define LSDB_FAILURE    1

#define LSDB_OPEN_RO    0
#define LSDB_OPEN_RW    1
#define LSDB_OPEN_INIT  2

struct _lsdb_t {
    sqlite3 *db;
    int      db_format;

    FILE    *err_fp;

    void    *udata;
};

typedef struct _lsdb_t lsdb_t;

typedef struct {
    unsigned long id;
    const char *name;
    const char *descr;
} lsdb_model_data_t;

typedef struct {
    unsigned long id;
    const char *name;
    const char *descr;
} lsdb_environment_data_t;

typedef struct {
    unsigned long id;
    const char *sym;
    unsigned int anum;
    double mass;
    unsigned int zsp;
} lsdb_radiator_data_t;

typedef struct {
    unsigned long id;
    const char *name;
    double energy;
} lsdb_line_data_t;

typedef struct {
    unsigned long id;
    unsigned long mid;
    unsigned long eid;
    double n;
    double T;
} lsdb_dataset_data_t;

typedef struct {
    double n;
    double T;
    size_t len;
    double *x;
    double *y;
} lsdb_dataset_t;

typedef int (*lsdb_model_sink_t)(const lsdb_t *cdb,
    const lsdb_model_data_t *cbdata, void *udata);
typedef int (*lsdb_environment_sink_t)(const lsdb_t *cdb,
    const lsdb_environment_data_t *cbdata, void *udata);
typedef int (*lsdb_radiator_sink_t)(const lsdb_t *cdb,
    const lsdb_radiator_data_t *cbdata, void *udata);
typedef int (*lsdb_line_sink_t)(const lsdb_t *cdb,
    const lsdb_line_data_t *cbdata, void *udata);
typedef int (*lsdb_dataset_sink_t)(const lsdb_t *cdb,
    const lsdb_dataset_data_t *cbdata, void *udata);

lsdb_t *lsdb_open(const char *fname, int access);
void lsdb_close(lsdb_t *lsdb);

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

int lsdb_add_dataset(lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double n, double T,
    const double *x, const double *y, size_t len);
int lsdb_get_datasets(const lsdb_t *lsdb, unsigned long lid,
    lsdb_dataset_sink_t sink, void *udata);
int lsdb_del_dataset(lsdb_t *lsdb, unsigned long id);

lsdb_dataset_t *lsdb_get_dataset(lsdb_t *lsdb, int did);
void lsdb_dataset_free(lsdb_dataset_t *ds);

lsdb_dataset_t *lsdb_dataset_new(double n, double T, size_t len);

int lsdb_get_closest_dids(const lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double n, double T,
    unsigned long *did1, unsigned long *did2,
    unsigned long *did3, unsigned long *did4);

#endif /* LSDB_H */

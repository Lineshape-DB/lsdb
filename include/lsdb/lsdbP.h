#ifndef LSDBP_H
#define LSDBP_H

#include <stdio.h>
#include <sqlite3.h>

#include <lsdb/lsdb.h>
#include <lsdb/morph.h>

#define LSDB_CONVERT_EV_TO_INV_CM   8065.54394
#define LSDB_CONVERT_AU_TO_EV       27.2113862

struct _lsdb_t {
    sqlite3     *db;
    int          db_format;
    lsdb_units_t units;

    void *udata;
};

struct _lsdb_interp_t {
    morph_t *morph;
    double   t;
};

void lsdb_errmsg(const lsdb_t *lsdb, const char *fmt, ...);

int lsdb_get_closest_dids(const lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double n, double T,
    unsigned long *did1, unsigned long *did2,
    unsigned long *did3, unsigned long *did4);

#endif /* LSDBP_H */

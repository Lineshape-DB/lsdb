#ifndef LSDBP_H
#define LSDBP_H

#include <stdio.h>
#include <sqlite3.h>

#include <lsdb/lsdb.h>

#define LSDB_CONVERT_EV_TO_INV_CM   8065.54394
#define LSDB_CONVERT_AU_TO_EV       27.2113862

struct _lsdb_t {
    sqlite3     *db;
    int          db_format;
    lsdb_units_t units;

    FILE *err_fp;

    void *udata;
};

#endif /* LSDBP_H */

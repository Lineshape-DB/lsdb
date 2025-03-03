#ifndef LSDBP_H
#define LSDBP_H

#include <stdio.h>
#include <sqlite3.h>

#include <lsdb/lsdb.h>

struct _lsdb_t {
    sqlite3     *db;
    int          db_format;
    lsdb_units_t units;

    FILE *err_fp;

    void *udata;
};

#endif /* LSDBP_H */

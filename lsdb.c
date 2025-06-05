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

#include <stdlib.h>
#include <string.h>

#include <lsdb/lsdbP.h>

#include "schema.i"

#define SQLITE3_BIND_STR(stmt, id, txt) \
        sqlite3_bind_text(stmt, id, txt, -1, SQLITE_STATIC)

void lsdb_get_version_numbers(int *major, int *minor, int *nano)
{
    *major = LSDB_VERSION_MAJOR;
    *minor = LSDB_VERSION_MINOR;
    *nano  = LSDB_VERSION_NANO;
}

void lsdb_dataset_data_free(lsdb_dataset_data_t *ds)
{
    if (ds) {
        if (ds->x) {
            free(ds->x);
        }
        if (ds->y) {
            free(ds->y);
        }

        free(ds);
    }
}

lsdb_dataset_data_t *lsdb_dataset_data_new(double n, double T, size_t len)
{
    lsdb_dataset_data_t *ds;

    ds = malloc(sizeof(lsdb_dataset_data_t));
    if (ds) {
        ds->n = n;
        ds->T = T;

        ds->x = calloc(len, sizeof(double));
        ds->y = calloc(len, sizeof(double));
        if (!ds->x || !ds->y) {
            lsdb_dataset_data_free(ds);
            return NULL;
        }

        ds->len = len;
    }

    return ds;
}

void lsdb_close(lsdb_t *lsdb)
{
    if (lsdb) {
        sqlite3_close(lsdb->db);

        free(lsdb);
    }
}

void lsdb_errmsg(const lsdb_t *lsdb, const char *fmt, ...)
{
    va_list args;
    (void) lsdb;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

static int format_cb(void *udata,
    int argc, char **argv, char **colNames)
{
    int *db_format = udata;

    if (argc != 1 || !argv[0] || !colNames[0]) {
        return -1;
    }

    *db_format = atol(argv[0]);

    return 0;
}

lsdb_t *lsdb_open(const char *fname, lsdb_access_t access)
{
    lsdb_t *lsdb = NULL;

    const char *sql;
    char *errmsg;
    int rc;
    int flags;

    lsdb = malloc(sizeof(lsdb_t));
    if (!lsdb) {
        return NULL;
    }
    memset(lsdb, 0, sizeof(lsdb_t));

    if (access == LSDB_ACCESS_RW) {
        flags = SQLITE_OPEN_READWRITE;
    } else
    if (access == LSDB_ACCESS_INIT) {
        flags = SQLITE_OPEN_CREATE|SQLITE_OPEN_READWRITE;
    } else {
        flags = SQLITE_OPEN_READONLY;
    }

    rc = sqlite3_open_v2(fname, &lsdb->db, flags, NULL);
    if (rc) {
        lsdb_errmsg(lsdb, "Cannot open database \"%s\": %s\n",
            fname, sqlite3_errmsg(lsdb->db));
        lsdb_close(lsdb);
        return NULL;
    }

    rc = sqlite3_exec(lsdb->db, "PRAGMA foreign_keys = ON", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        lsdb_errmsg(lsdb, "SQL error: %s\n", errmsg);
        sqlite3_free(errmsg);
        lsdb_close(lsdb);
        return NULL;
    }

    if (access == LSDB_ACCESS_INIT) {
        int i = 0;
        while ((sql = schema_str[i])) {
            rc = sqlite3_exec(lsdb->db, sql, NULL, NULL, &errmsg);
            if (rc != SQLITE_OK) {
                lsdb_errmsg(lsdb, "SQL error: %s\n", errmsg);
                sqlite3_free(errmsg);
                lsdb_close(lsdb);
                return NULL;
            }
            i++;
        }
    } else {
        /* verify the version/format is compatible */
        sql = "SELECT value FROM lsdb WHERE property = 'format'";

        rc = sqlite3_exec(lsdb->db, sql, format_cb, &lsdb->db_format, &errmsg);
        if (rc != SQLITE_OK) {
            lsdb_errmsg(lsdb, "Wrong DB format\n");
            lsdb_close(lsdb);
            return NULL;
        }

        if (lsdb->db_format != 1) {
            lsdb_errmsg(lsdb, "Unsupported DB format version %d\n", lsdb->db_format);
            lsdb_close(lsdb);
            return NULL;
        }

        /* obtain units */
        sql = "SELECT value FROM lsdb WHERE property = 'units'";

        rc = sqlite3_exec(lsdb->db, sql, format_cb, &lsdb->units, &errmsg);
        if (rc != SQLITE_OK) {
            lsdb_errmsg(lsdb, "Wrong DB format\n");
            lsdb_close(lsdb);
            return NULL;
        }
    }

    return lsdb;
}

int lsdb_set_units(lsdb_t *lsdb, lsdb_units_t units)
{
    sqlite3_stmt *stmt;
    int rc;
    char *sql = "UPDATE lsdb SET value=? WHERE property = 'units'";

    if (!lsdb) {
        return LSDB_FAILURE;
    }

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, units);

    rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        if (sqlite3_changes(lsdb->db) == 1) {
            lsdb->units = units;
            return LSDB_SUCCESS;
        } else {
            return LSDB_FAILURE;
        }
    } else {
        lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
        return LSDB_FAILURE;
    }
}

lsdb_units_t lsdb_get_units(const lsdb_t *lsdb) {
    return lsdb->units;
}

static int lsdb_del_entity(lsdb_t *lsdb, const char *tname, unsigned long id)
{
    sqlite3_stmt *stmt;
    char sql[64];
    int rc;

    if (!lsdb || id == 0) {
        return LSDB_FAILURE;
    }

    sprintf(sql, "DELETE FROM %s WHERE id = ?", tname);

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        if (sqlite3_changes(lsdb->db) == 1) {
            return LSDB_SUCCESS;
        } else {
            return LSDB_FAILURE;
        }
    } else {
        lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
        return LSDB_FAILURE;
    }
}

int lsdb_add_model(lsdb_t *lsdb, const char *name, const char *descr)
{
    const char *sql;
    sqlite3_stmt *stmt;
    int rc;
    int rid;

    sql = "INSERT INTO models (name, descr) VALUES (?, ?)";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    SQLITE3_BIND_STR(stmt, 1, name);
    SQLITE3_BIND_STR(stmt, 2, descr);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        rid = sqlite3_last_insert_rowid(lsdb->db);
    } else {
        lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
        rid = -1;
    }

    sqlite3_finalize(stmt);

    return rid;
}

int lsdb_get_models(const lsdb_t *lsdb,
    lsdb_model_sink_t sink, void *udata)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    if (!lsdb) {
        return LSDB_FAILURE;
    }

    sql = "SELECT id, name, descr" \
          " FROM models" \
          " ORDER BY id";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    do {
        lsdb_model_t m;

        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            m.id    = sqlite3_column_int   (stmt, 0);
            m.name  = (char *) sqlite3_column_text(stmt, 1);
            m.descr = (char *) sqlite3_column_text(stmt, 2);

            if (sink(lsdb, &m, udata) != LSDB_SUCCESS) {
                sqlite3_finalize(stmt);
                return LSDB_FAILURE;
            }

            break;
        default:
            lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            return LSDB_FAILURE;
            break;
        }
    } while (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);

    return LSDB_SUCCESS;
}

int lsdb_del_model(lsdb_t *lsdb, unsigned long id)
{
    return lsdb_del_entity(lsdb, "models", id);
}

int lsdb_add_environment(lsdb_t *lsdb, const char *name, const char *descr)
{
    const char *sql;
    sqlite3_stmt *stmt;
    int rc;
    int rid;

    sql = "INSERT INTO environments (name, descr) VALUES (?, ?)";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    SQLITE3_BIND_STR(stmt, 1, name);
    SQLITE3_BIND_STR(stmt, 2, descr);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        rid = sqlite3_last_insert_rowid(lsdb->db);
    } else {
        lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
        rid = -1;
    }

    sqlite3_finalize(stmt);

    return rid;
}

int lsdb_get_environments(const lsdb_t *lsdb,
    lsdb_environment_sink_t sink, void *udata)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    if (!lsdb) {
        return LSDB_FAILURE;
    }

    sql = "SELECT id, name, descr" \
          " FROM environments" \
          " ORDER BY id";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    do {
        lsdb_environment_t e;

        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            e.id    = sqlite3_column_int   (stmt, 0);
            e.name  = (char *) sqlite3_column_text(stmt, 1);
            e.descr = (char *) sqlite3_column_text(stmt, 2);

            if (sink(lsdb, &e, udata) != LSDB_SUCCESS) {
                sqlite3_finalize(stmt);
                return LSDB_FAILURE;
            }

            break;
        default:
            lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            return LSDB_FAILURE;
            break;
        }
    } while (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);

    return LSDB_SUCCESS;
}

int lsdb_del_environment(lsdb_t *lsdb, unsigned long id)
{
    return lsdb_del_entity(lsdb, "environments", id);
}

int lsdb_add_radiator(lsdb_t *lsdb,
    const char *symbol, int anum, double mass, int zsp)
{
    const char *sql;
    sqlite3_stmt *stmt;
    int rc;
    int rid;

    sql = "INSERT INTO radiators (symbol, anum, mass, zsp) VALUES (?, ?, ?, ?)";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    SQLITE3_BIND_STR   (stmt, 1, symbol);
    sqlite3_bind_int   (stmt, 2, anum);
    sqlite3_bind_double(stmt, 3, mass);
    sqlite3_bind_int   (stmt, 4, zsp);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        rid = sqlite3_last_insert_rowid(lsdb->db);
    } else {
        lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
        rid = -1;
    }

    sqlite3_finalize(stmt);

    return rid;
}

int lsdb_get_radiators(const lsdb_t *lsdb,
    lsdb_radiator_sink_t sink, void *udata)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    if (!lsdb) {
        return LSDB_FAILURE;
    }

    sql = "SELECT id, symbol, anum, mass, zsp" \
          " FROM radiators" \
          " ORDER BY id";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    do {
        lsdb_radiator_t r;

        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            r.id   = sqlite3_column_int   (stmt, 0);
            r.sym  = (char *) sqlite3_column_text(stmt, 1);
            r.anum = sqlite3_column_int   (stmt, 2);
            r.mass = sqlite3_column_double(stmt, 3);
            r.zsp  = sqlite3_column_int   (stmt, 4);

            if (sink(lsdb, &r, udata) != LSDB_SUCCESS) {
                sqlite3_finalize(stmt);
                return LSDB_FAILURE;
            }

            break;
        default:
            lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            return LSDB_FAILURE;
            break;
        }
    } while (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);

    return LSDB_SUCCESS;
}

int lsdb_del_radiator(lsdb_t *lsdb, unsigned long id)
{
    return lsdb_del_entity(lsdb, "radiators", id);
}

int lsdb_add_line(lsdb_t *lsdb,
    unsigned int rid, const char *name, double energy)
{
    const char *sql;
    sqlite3_stmt *stmt;
    int rc;
    int lid;

    sql = "INSERT INTO lines (rid, name, energy) VALUES (?, ?, ?)";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    sqlite3_bind_int   (stmt, 1, rid);
    SQLITE3_BIND_STR   (stmt, 2, name);
    sqlite3_bind_double(stmt, 3, energy);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        lid = sqlite3_last_insert_rowid(lsdb->db);
    } else {
        lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
        lid = -1;
    }

    sqlite3_finalize(stmt);

    return lid;
}

int lsdb_get_lines(const lsdb_t *lsdb, unsigned long rid,
    lsdb_line_sink_t sink, void *udata)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    if (!lsdb) {
        return LSDB_FAILURE;
    }

    sql = "SELECT id, name, energy" \
          " FROM lines" \
          " WHERE rid = ?" \
          " ORDER BY id";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, rid);

    do {
        lsdb_line_t l;

        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            l.id     = sqlite3_column_int(stmt, 0);
            l.name   = (char *) sqlite3_column_text(stmt, 1);
            l.energy = sqlite3_column_double(stmt, 2);

            if (sink(lsdb, &l, udata) != LSDB_SUCCESS) {
                sqlite3_finalize(stmt);
                return LSDB_FAILURE;
            }

            break;
        default:
            lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            return LSDB_FAILURE;
            break;
        }
    } while (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);

    return LSDB_SUCCESS;
}

int lsdb_del_line(lsdb_t *lsdb, unsigned long id)
{
    return lsdb_del_entity(lsdb, "lines", id);
}

int lsdb_add_line_property(lsdb_t *lsdb,
    unsigned int lid, const char *name, const char *value)
{
    const char *sql;
    sqlite3_stmt *stmt;
    int rc;
    int pid;

    sql = "INSERT INTO line_properties (lid, name, value) VALUES (?, ?, ?)";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, lid);
    SQLITE3_BIND_STR(stmt, 2, name);
    SQLITE3_BIND_STR(stmt, 3, value);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        pid = sqlite3_last_insert_rowid(lsdb->db);
    } else {
        lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
        pid = -1;
    }

    sqlite3_finalize(stmt);

    return pid;
}

int lsdb_get_line_properties(const lsdb_t *lsdb, unsigned long lid,
    lsdb_line_property_sink_t sink, void *udata)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    if (!lsdb) {
        return LSDB_FAILURE;
    }

    sql = "SELECT id, name, value" \
          " FROM line_properties" \
          " WHERE lid = ?" \
          " ORDER BY id";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, lid);

    do {
        lsdb_line_property_t p;

        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            p.id    = sqlite3_column_int(stmt, 0);
            p.name  = (char *) sqlite3_column_text(stmt, 1);
            p.value = (char *) sqlite3_column_text(stmt, 2);

            if (sink(lsdb, &p, udata) != LSDB_SUCCESS) {
                sqlite3_finalize(stmt);
                return LSDB_FAILURE;
            }

            break;
        default:
            lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            return LSDB_FAILURE;
            break;
        }
    } while (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);

    return LSDB_SUCCESS;
}

int lsdb_del_line_property(lsdb_t *lsdb, unsigned long id)
{
    return lsdb_del_entity(lsdb, "line_properties", id);
}

int lsdb_add_dataset(lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double n, double T,
    const double *x, const double *y, size_t len)
{
    const char *sql;
    sqlite3_stmt *stmt;
    int rc;
    unsigned int i;
    int did;

    if (len < 2 || !x || !y) {
        lsdb_errmsg(lsdb, "Adding empty dataset refused\n");
        return -1;
    }

    sqlite3_exec(lsdb->db, "BEGIN", 0, 0, 0);

    sql = "INSERT INTO datasets (mid, eid, lid, n, T) VALUES (?, ?, ?, ?, ?)";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    sqlite3_bind_int   (stmt, 1, mid);
    sqlite3_bind_int   (stmt, 2, eid);
    sqlite3_bind_int   (stmt, 3, lid);
    sqlite3_bind_double(stmt, 4, n);
    sqlite3_bind_double(stmt, 5, T);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
        sqlite3_finalize(stmt);
        sqlite3_exec(lsdb->db, "ROLLBACK", 0, 0, 0);
        return -1;
    } else {
        did = sqlite3_last_insert_rowid(lsdb->db);

        /* reuse stmt */
        sqlite3_finalize(stmt);

        sql = "INSERT INTO data (did, x, y) VALUES (?, ?, ?)";
        sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

        sqlite3_bind_int(stmt, 1, did);
        for (i = 0; i < len; i++) {
            sqlite3_bind_double(stmt, 2, x[i]);
            sqlite3_bind_double(stmt, 3, y[i]);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
                sqlite3_finalize(stmt);
                sqlite3_exec(lsdb->db, "ROLLBACK", 0, 0, 0);
                return -1;
            }

            sqlite3_reset(stmt);
        }
    }

    sqlite3_exec(lsdb->db, "COMMIT", 0, 0, 0);

    return did;
}

int lsdb_get_datasets(const lsdb_t *lsdb, unsigned long lid,
    lsdb_dataset_sink_t sink, void *udata)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    if (!lsdb) {
        return LSDB_FAILURE;
    }

    sql = "SELECT id, mid, eid, n, T" \
          " FROM datasets" \
          " WHERE lid = ?" \
          " ORDER BY mid, eid, n, T";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, lid);

    do {
        lsdb_dataset_t cbdata;

        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            cbdata.id  = sqlite3_column_int(stmt, 0);
            cbdata.mid = sqlite3_column_int(stmt, 1);
            cbdata.eid = sqlite3_column_int(stmt, 2);
            cbdata.n   = sqlite3_column_double(stmt, 3);
            cbdata.T   = sqlite3_column_double(stmt, 4);

            if (sink(lsdb, &cbdata, udata) != LSDB_SUCCESS) {
                sqlite3_finalize(stmt);
                return LSDB_FAILURE;
            }

            break;
        default:
            lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            return LSDB_FAILURE;
            break;
        }
    } while (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);

    return LSDB_SUCCESS;
}

int lsdb_del_dataset(lsdb_t *lsdb, unsigned long id)
{
    return lsdb_del_entity(lsdb, "datasets", id);
}

lsdb_dataset_data_t *lsdb_get_dataset_data(const lsdb_t *lsdb, int did)
{
    lsdb_dataset_data_t *ds;
    const char *sql;
    sqlite3_stmt *stmt;
    int rc;
    unsigned int i;

    sql = "SELECT ds.n, ds.T, count(*)" \
          " FROM datasets AS ds INNER JOIN data AS d ON (ds.id = d.did)" \
          " WHERE ds.id = ?";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, did);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        double n, T;
        size_t len;

        n   = sqlite3_column_double(stmt, 0);
        T   = sqlite3_column_double(stmt, 1);
        len = sqlite3_column_int64 (stmt, 2);

        sqlite3_finalize(stmt);

        if (len == 0) {
            lsdb_errmsg(lsdb, "Dataset %d not found\n", did);
            return NULL;
        }
        ds = lsdb_dataset_data_new(n, T, len);
        if (!ds) {
            lsdb_errmsg(lsdb, "Dataset allocation failed\n");
            return NULL;
        }
    } else {
        lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
        sqlite3_finalize(stmt);
        return NULL;
    }

    sql = "SELECT x, y FROM data WHERE data.did = ? ORDER BY x";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, did);

    i = 0;
    do {
        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            ds->x[i] = sqlite3_column_double(stmt, 0);
            ds->y[i] = sqlite3_column_double(stmt, 1);
            i++;
            break;
        default:
            lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            lsdb_dataset_data_free(ds);
            return NULL;
        }
    } while (rc == SQLITE_ROW && i < ds->len);

    sqlite3_finalize(stmt);

    return ds;
}

/*
 * Find nearest four datasets (the list can be partly or fully degenerate)
 * In the (n, T) plane, did1...did4 correspond to the bottom-left, bottom-right,
 * top-right, and top-left, respectively
 */
int lsdb_get_closest_dids(const lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double n, double T,
    unsigned long *did1, unsigned long *did2,
    unsigned long *did3, unsigned long *did4)
{
    const char *sql;
    sqlite3_stmt *stmt;
    bool found = false;
    int rc;

    *did1 = *did2 = *did3 = *did4 = 0;

    if (n <= 0 || T <= 0) {
        return LSDB_FAILURE;
    }

    sql = "SELECT id, (n - ?)/? AS dn, (T - ?)/? AS dT" \
          " FROM datasets WHERE mid = ? AND eid = ? AND lid = ?" \
          " ORDER BY dn*dn + dT*dT";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    sqlite3_bind_double(stmt, 1, n);
    sqlite3_bind_double(stmt, 2, n);
    sqlite3_bind_double(stmt, 3, T);
    sqlite3_bind_double(stmt, 4, T);
    sqlite3_bind_int(stmt, 5, mid);
    sqlite3_bind_int(stmt, 6, eid);
    sqlite3_bind_int(stmt, 7, lid);

    do {
        unsigned int id;
        double dn, dT;

        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            id  = sqlite3_column_int64 (stmt, 0);
            dn  = sqlite3_column_double(stmt, 1);
            dT  = sqlite3_column_double(stmt, 2);

            if (*did1 == 0 && dn <= 0 && dT <= 0) {
                *did1 = id;
            }
            if (*did2 == 0 && dn >= 0 && dT <= 0) {
                *did2 = id;
            }
            if (*did3 == 0 && dn >= 0 && dT >= 0) {
                *did3 = id;
            }
            if (*did4 == 0 && dn <= 0 && dT >= 0) {
                *did4 = id;
            }

            if (*did1 != 0 && *did2 != 0 && *did3 != 0 && *did4 != 0) {
                found = true;
            }

            break;
        default:
            lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            break;
        }
    } while (rc == SQLITE_ROW && !found);

    sqlite3_finalize(stmt);

    if (found) {
        return LSDB_SUCCESS;
    } else {
        return LSDB_FAILURE;
    }
}

int lsdb_get_limits(const lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double *nmin, double *nmax, double *Tmin, double *Tmax)
{
    const char *sql;
    sqlite3_stmt *stmt;
    int rc;

    sql = "SELECT MIN(n), MAX(n), MIN(T), MAX(T)" \
          " FROM datasets WHERE mid = ? AND eid = ? AND lid = ?";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, mid);
    sqlite3_bind_int(stmt, 2, eid);
    sqlite3_bind_int(stmt, 3, lid);

    do {
        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            *nmin = sqlite3_column_double(stmt, 0);
            *nmax = sqlite3_column_double(stmt, 1);
            *Tmin = sqlite3_column_double(stmt, 2);
            *Tmax = sqlite3_column_double(stmt, 3);

            break;
        default:
            lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            return LSDB_FAILURE;
            break;
        }
    } while (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);
    return LSDB_SUCCESS;
}

double lsdb_convert_units(lsdb_units_t from_units, lsdb_units_t to_units)
{
    double unit_scale = 0.0;

    if (to_units == from_units) {
        unit_scale = 1.0;
    } else {
        switch (to_units) {
        case LSDB_UNITS_INV_CM:
            switch (from_units) {
            case LSDB_UNITS_EV:
                unit_scale = LSDB_CONVERT_EV_TO_INV_CM;
                break;
            case LSDB_UNITS_AU:
                unit_scale = LSDB_CONVERT_AU_TO_EV*LSDB_CONVERT_EV_TO_INV_CM;
                break;
            default:
                break;
            }
            break;
        case LSDB_UNITS_EV:
            switch (from_units) {
            case LSDB_UNITS_INV_CM:
                unit_scale = 1.0/LSDB_CONVERT_EV_TO_INV_CM;
                break;
            case LSDB_UNITS_AU:
                unit_scale = LSDB_CONVERT_AU_TO_EV;
                break;
            default:
                break;
            }
            break;
        case LSDB_UNITS_AU:
            switch (from_units) {
            case LSDB_UNITS_INV_CM:
                unit_scale = 1.0/(LSDB_CONVERT_AU_TO_EV*LSDB_CONVERT_EV_TO_INV_CM);
                break;
            case LSDB_UNITS_EV:
                unit_scale = 1.0/LSDB_CONVERT_AU_TO_EV;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }

    return unit_scale;
}

double lsdb_convert_to_units(const lsdb_t *lsdb, lsdb_units_t to_units)
{
    return lsdb_convert_units(lsdb->units, to_units);
}

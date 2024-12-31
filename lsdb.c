#include <lsdb.h>

#include "schema.i"

#define SQLITE3_BIND_STR(stmt, id, txt) \
        sqlite3_bind_text(stmt, id, txt, -1, SQLITE_STATIC)

void lsdb_dataset_free(lsdb_dataset_t *ds)
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

lsdb_dataset_t *lsdb_dataset_new(double n, double T, size_t len)
{
    lsdb_dataset_t *ds;

    ds = malloc(sizeof(lsdb_dataset_t));
    if (ds) {
        ds->n = n;
        ds->T = T;

        ds->x = calloc(len, sizeof(double));
        ds->y = calloc(len, sizeof(double));
        if (!ds->x || !ds->y) {
            lsdb_dataset_free(ds);
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

static void lsdb_errmsg(const lsdb_t *lsdb, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(lsdb->err_fp, fmt, args);
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

lsdb_t *lsdb_open(const char *fname, int access)
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
    lsdb->err_fp = stderr;

    if (access == LSDB_OPEN_RW) {
        flags = SQLITE_OPEN_READWRITE;
    } else
    if (access == LSDB_OPEN_INIT) {
        flags = SQLITE_OPEN_CREATE|SQLITE_OPEN_READWRITE;
    } else {
        flags = SQLITE_OPEN_READONLY;
    }

    rc = sqlite3_open_v2(fname, &lsdb->db, flags, NULL);
    if (rc) {
        fprintf(stderr, "Cannot open database \"%s\": %s\n",
            fname, sqlite3_errmsg(lsdb->db));
        lsdb_close(lsdb);
        return NULL;
    }

    rc = sqlite3_exec(lsdb->db, "PRAGMA foreign_keys = ON", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errmsg);
        sqlite3_free(errmsg);
        lsdb_close(lsdb);
        return NULL;
    }

    if (access == LSDB_OPEN_INIT) {
        int i = 0;
        while ((sql = schema_str[i])) {
            rc = sqlite3_exec(lsdb->db, sql, NULL, NULL, &errmsg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", errmsg);
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
            fprintf(stderr, "Wrong DB format\n");
            lsdb_close(lsdb);
            return NULL;
        }
    }

    return lsdb;
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
        lsdb_model_data_t cbdata;

        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            cbdata.id    = sqlite3_column_int   (stmt, 0);
            cbdata.name  = (char *) sqlite3_column_text(stmt, 1);
            cbdata.descr = (char *) sqlite3_column_text(stmt, 2);

            if (sink(lsdb, &cbdata, udata) != LSDB_SUCCESS) {
                sqlite3_finalize(stmt);
                return LSDB_FAILURE;
            }

            break;
        default:
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            return LSDB_FAILURE;
            break;
        }
    } while (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);

    return LSDB_SUCCESS;
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
        lsdb_environment_data_t cbdata;

        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            cbdata.id    = sqlite3_column_int   (stmt, 0);
            cbdata.name  = (char *) sqlite3_column_text(stmt, 1);
            cbdata.descr = (char *) sqlite3_column_text(stmt, 2);

            if (sink(lsdb, &cbdata, udata) != LSDB_SUCCESS) {
                sqlite3_finalize(stmt);
                return LSDB_FAILURE;
            }

            break;
        default:
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            return LSDB_FAILURE;
            break;
        }
    } while (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);

    return LSDB_SUCCESS;
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
        lsdb_radiator_data_t cbdata;

        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            cbdata.id   = sqlite3_column_int   (stmt, 0);
            cbdata.sym  = (char *) sqlite3_column_text(stmt, 1);
            cbdata.anum = sqlite3_column_int   (stmt, 2);
            cbdata.mass = sqlite3_column_double(stmt, 3);
            cbdata.zsp  = sqlite3_column_int   (stmt, 4);

            if (sink(lsdb, &cbdata, udata) != LSDB_SUCCESS) {
                sqlite3_finalize(stmt);
                return LSDB_FAILURE;
            }

            break;
        default:
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            return LSDB_FAILURE;
            break;
        }
    } while (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);

    return LSDB_SUCCESS;
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

int lsdb_get_lines(const lsdb_t *lsdb, unsigned long int rid,
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
        lsdb_line_data_t cbdata;

        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_DONE:
        case SQLITE_OK:
            break;
        case SQLITE_ROW:
            cbdata.id     = sqlite3_column_int(stmt, 0);
            cbdata.name   = (char *) sqlite3_column_text(stmt, 1);
            cbdata.energy = sqlite3_column_double(stmt, 2);

            if (sink(lsdb, &cbdata, udata) != LSDB_SUCCESS) {
                sqlite3_finalize(stmt);
                return LSDB_FAILURE;
            }

            break;
        default:
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            return LSDB_FAILURE;
            break;
        }
    } while (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);

    return LSDB_SUCCESS;
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

int lsdb_get_datasets(const lsdb_t *lsdb, unsigned long int lid,
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
        lsdb_dataset_data_t cbdata;

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
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
            sqlite3_finalize(stmt);
            return LSDB_FAILURE;
            break;
        }
    } while (rc == SQLITE_ROW);

    sqlite3_finalize(stmt);

    return LSDB_SUCCESS;
}

lsdb_dataset_t *lsdb_get_dataset(lsdb_t *lsdb, int did)
{
    lsdb_dataset_t *ds;
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
        ds = lsdb_dataset_new(n, T, len);
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
            lsdb_dataset_free(ds);
            return NULL;
        }
    } while (rc == SQLITE_ROW && i < ds->len);

    sqlite3_finalize(stmt);

    return ds;
}

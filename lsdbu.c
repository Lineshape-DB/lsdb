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
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

#include <lsdb/lsdb.h>

enum {
    LSDBU_ACTION_NONE,
    LSDBU_ACTION_INFO,
    LSDBU_ACTION_INIT,
    LSDBU_ACTION_SET_UNITS,
    LSDBU_ACTION_ADD_MODEL,
    LSDBU_ACTION_ADD_ENV,
    LSDBU_ACTION_ADD_RADIATOR,
    LSDBU_ACTION_ADD_LINE,
    LSDBU_ACTION_ADD_DATA,
    LSDBU_ACTION_ADD_PROPERTY,
    LSDBU_ACTION_DEL_ENTITY,
    LSDBU_ACTION_GET_DATA,
    LSDBU_ACTION_INTERPOLATE
};

typedef struct {
    FILE         *fp_out;

    bool          verbose;

    unsigned long mid;
    unsigned long eid;
    unsigned long rid;
    unsigned long lid;
    double        n;
    double        T;
} lsdbu_t;

static int read_in(FILE *fp, double **xap, double **yap, size_t *lenp)
{
    size_t in_allocated = 0, len = 0;
    double *xa = NULL, *ya = NULL;

    while (1) {
        char strbuf[1024];
        double x, y;

        if (fgets(strbuf, 1024, fp) == NULL) {
            break;
        }

        /* skip comments and empty lines */
        if (strbuf[0] == '#' || strlen(strbuf) == 1) {
            continue;
        }

        if (sscanf(strbuf, "%lg %lg", &x, &y) != 2) {
            fprintf(stderr,
                "Unparseable string '%s'\n", strbuf);
            return LSDB_FAILURE;
        }
        if (y < 0) {
            fprintf(stderr, "y must be >= 0\n");
            return LSDB_FAILURE;
        }

        if (len >= in_allocated) {
            xa = realloc(xa, sizeof(double)*(in_allocated + 128));
            ya = realloc(ya, sizeof(double)*(in_allocated + 128));

            if (!xa || !ya) {
                fprintf(stderr, "Memory allocation failed\n");
                return LSDB_FAILURE;
            }
            in_allocated += 128;
        }

        xa[len] = x;
        ya[len] = y;

        len++;
    }

    *xap  = xa;
    *yap  = ya;
    *lenp = len;

    return LSDB_SUCCESS;
}

static int line_property_sink(const lsdb_t *lsdb,
    const lsdb_line_property_t *p, void *udata)
{
    lsdbu_t *lsdbu = udata;
    (void)(lsdb);

    fprintf(lsdbu->fp_out,
        "      id = %lu: \"%s\" => \"%s\"\n",
        p->id, p->name, p->value);

    return LSDB_SUCCESS;
}

static int dataset_sink(const lsdb_t *lsdb,
    const lsdb_dataset_t *ds, void *udata)
{
    lsdbu_t *lsdbu = udata;
    (void)(lsdb);

    if ((lsdbu->eid != 0 && ds->eid != lsdbu->eid) ||
        (lsdbu->mid != 0 && ds->mid != lsdbu->mid) ||
        (lsdbu->n   >  0 && ds->n   != lsdbu->n)   ||
        (lsdbu->T   >  0 && ds->T   != lsdbu->T)) {
        return LSDB_SUCCESS;
    }

    fprintf(lsdbu->fp_out,
        "      id = %lu: (mid = %lu, eid = %lu, n_e = %g cm^-3, T = %g eV)\n",
        ds->id, ds->mid, ds->eid, ds->n, ds->T);

    return LSDB_SUCCESS;
}

static int line_sink(const lsdb_t *lsdb,
    const lsdb_line_t *l, void *udata)
{
    lsdbu_t *lsdbu = udata;
    lsdb_units_t units = lsdb_get_units(lsdb);
    double w_cm, e_eV;

    if (lsdbu->lid != 0 && l->id != lsdbu->lid) {
        return LSDB_SUCCESS;
    }

    w_cm = l->energy*lsdb_convert_units(units, LSDB_UNITS_INV_CM);
    e_eV = l->energy*lsdb_convert_units(units, LSDB_UNITS_EV);

    fprintf(lsdbu->fp_out, "    id = %lu: \"%s\"", l->id, l->name);

    if (lsdbu->verbose && e_eV > 0.0) {
        fprintf(lsdbu->fp_out, " (%g cm^-1 => %g eV)\n", w_cm, e_eV);
    } else {
        fputc('\n', lsdbu->fp_out);
    }

    if (lsdbu->mid > 0 && lsdbu->eid > 0) {
        double n_min, n_max, T_min, T_max;
        lsdb_get_limits(lsdb, lsdbu->mid, lsdbu->eid, l->id,
            &n_min, &n_max, &T_min, &T_max);
        fprintf(lsdbu->fp_out, "    Dataset domains:\n");
        fprintf(lsdbu->fp_out, "      n: (%g - %g) 1/cm^3\n", n_min, n_max);
        fprintf(lsdbu->fp_out, "      T: (%g - %g) eV\n", T_min, T_max);
    }

    if (lsdbu->verbose) {
        fprintf(lsdbu->fp_out, "    Properties:\n");
        lsdb_get_line_properties(lsdb, l->id, line_property_sink, lsdbu);

        fprintf(lsdbu->fp_out, "    Datasets:\n");
        lsdb_get_datasets(lsdb, l->id, dataset_sink, lsdbu);
    }

    return LSDB_SUCCESS;
}

static int radiator_sink(const lsdb_t *lsdb,
    const lsdb_radiator_t *r, void *udata)
{
    lsdbu_t *lsdbu = udata;

    if (lsdbu->rid != 0 && r->id != lsdbu->rid) {
        return LSDB_SUCCESS;
    }

    fprintf(lsdbu->fp_out, "  id = %lu: \"%s\"", r->id, r->sym);

    if (lsdbu->verbose) {
        fprintf(lsdbu->fp_out, " (A = %d, Zsp = %d, mass = %g)\n",
            r->anum, r->zsp, r->mass);
    } else {
        fputc('\n', lsdbu->fp_out);
    }

    fprintf(lsdbu->fp_out, "  Lines:\n");
    lsdb_get_lines(lsdb, r->id, line_sink, lsdbu);

    return LSDB_SUCCESS;
}

static int environment_sink(const lsdb_t *lsdb,
    const lsdb_environment_t *e, void *udata)
{
    lsdbu_t *lsdbu = udata;
    (void)(lsdb);

    if (lsdbu->eid != 0 && e->id != lsdbu->eid) {
        return LSDB_SUCCESS;
    }

    fprintf(lsdbu->fp_out, "  id = %lu: \"%s\"", e->id, e->name);
    if (lsdbu->verbose && e->descr && strlen(e->descr) > 0) {
        fprintf(lsdbu->fp_out, " (%s)\n", e->descr);
    } else {
        fprintf(lsdbu->fp_out, "\n");
    }

    return LSDB_SUCCESS;
}

static int model_sink(const lsdb_t *lsdb,
    const lsdb_model_t *m, void *udata)
{
    lsdbu_t *lsdbu = udata;
    (void)(lsdb);

    if (lsdbu->mid != 0 && m->id != lsdbu->mid) {
        return LSDB_SUCCESS;
    }

    fprintf(lsdbu->fp_out, "  id = %lu: \"%s\"", m->id, m->name);
    if (lsdbu->verbose && m->descr && strlen(m->descr) > 0) {
        fprintf(lsdbu->fp_out, " (%s)\n", m->descr);
    } else {
        fprintf(lsdbu->fp_out, "\n");
    }

    return LSDB_SUCCESS;
}

static void usage(const char *arg0, FILE *out)
{
    fprintf(out, "Usage: %s [options] <database>\n", arg0);
    fprintf(out, "Available options:\n");
    fprintf(out, "  -i                    print basic information about the DB\n");
    fprintf(out, "  -d <id>               fetch dataset by its ID\n");
    fprintf(out, "  -o <filename>         output to filename [stdout]\n");
    fprintf(out, "  -m <id>               set model ID [none]\n");
    fprintf(out, "  -e <id>               set environment ID [none]\n");
    fprintf(out, "  -r <id>               set radiator ID [none]\n");
    fprintf(out, "  -l <id>               set line ID [none]\n");
    fprintf(out, "  -t <id>               set line property ID [none]\n");
    fprintf(out, "  -n <n>                set electron density to n/cc [0]\n");
    fprintf(out, "  -T <T>                set temperature to T eV [0]\n");
    fprintf(out, "  -p                    print interpolated lineshape\n");
    fprintf(out, "  -c                    convolve with the Doppler broadening\n");
    fprintf(out, "  -I                    initialize the DB\n");
    fprintf(out, "  -U <units>            set units (1/cm|eV|au|custom) [none]\n");
    fprintf(out, "  -M <name[,descr]>     add a model\n");
    fprintf(out, "  -E <name[,descr]>     add an environment\n");
    fprintf(out, "  -R <sym,A,Zsp,M>      add a radiator\n");
    fprintf(out, "  -L <name,w0>          add a line\n");
    fprintf(out, "  -D <filename>         add a dataset\n");
    fprintf(out, "  -P <name,value>       add a line property\n");
    fprintf(out, "  -X                    delete an entity by its ID\n");
    fprintf(out, "  -v                    be more verbose (together with \"-i\")\n");
    fprintf(out, "  -V                    print version info and exit\n");
    fprintf(out, "  -h                    print this help and exit\n");
}

static void about(void)
{
    int major, minor, nano;
    lsdb_get_version_numbers(&major, &minor, &nano);
    fprintf(stdout, "lsdbu-1.1 (using LSDB API v%d.%d.%d)\n",
        major, minor, nano);
    fprintf(stdout,
        "Copyright (C) 2025,2026 Weizmann Institute of Science\n\n");
    fprintf(stdout, "Written by Evgeny Stambulchik\n");
}

int main(int argc, char **argv)
{
    lsdbu_t LSDBU, *lsdbu = &LSDBU;

    int action = LSDBU_ACTION_NONE;
    char *dbfile;
    int db_access = LSDB_ACCESS_RO;
    lsdb_t *lsdb;
    bool OK = true;
    FILE *fp_f = NULL;
    long int id = 0, did = 0, pid = 0;
    const char *token;
    int ntoken = 0;
    const char *mname = NULL, *ename = NULL, *mdescr = "", *edescr = "";
    const char *symbol = NULL, *lname = NULL;
    const char *pname = NULL, *pvalue = NULL;
    int anum = 0, zsp = 0;
    double mass = 0, w0 = 0, *x = NULL, *y = NULL;
    size_t len;
    bool doppler = false;
    lsdb_units_t units = LSDB_UNITS_NONE;

    int opt;

    memset(lsdbu, 0, sizeof(lsdbu_t));
    lsdbu->fp_out = stdout;
    lsdbu->verbose = false;

    while ((opt = getopt(argc, argv,
        "id:o:m:e:r:l:t:n:T:pcIU:M:E:R:L:D:P:XvVh")) != -1) {
        switch (opt) {
        case 'i':
            action = LSDBU_ACTION_INFO;
            break;
        case 'd':
            action = LSDBU_ACTION_GET_DATA;
            did = atoi(optarg);
            if (did <= 0) {
                fprintf(stderr, "ID must be positive\n");
                exit(1);
            }
            break;
        case 'm':
            id = atoi(optarg);
            if (id <= 0) {
                fprintf(stderr, "ID must be positive\n");
                exit(1);
            }
            lsdbu->mid = id;
            break;
        case 'e':
            id = atoi(optarg);
            if (id <= 0) {
                fprintf(stderr, "ID must be positive\n");
                exit(1);
            }
            lsdbu->eid = id;
            break;
        case 'r':
            id = atoi(optarg);
            if (id <= 0) {
                fprintf(stderr, "ID must be positive\n");
                exit(1);
            }
            lsdbu->rid = id;
            break;
        case 'l':
            id = atoi(optarg);
            if (id <= 0) {
                fprintf(stderr, "ID must be positive\n");
                exit(1);
            }
            lsdbu->lid = id;
            break;
        case 't':
            id = atoi(optarg);
            if (id <= 0) {
                fprintf(stderr, "ID must be positive\n");
                exit(1);
            }
            pid = id;
            break;
        case 'o':
            lsdbu->fp_out = fopen(optarg, "wb");
            if (!lsdbu->fp_out) {
                fprintf(stderr, "Failed openning file %s for writing\n",
                    optarg);
                exit(1);
            }
            break;
        case 'n':
            lsdbu->n = atof(optarg);
            if (lsdbu->n <= 0) {
                fprintf(stderr, "Density must be positive\n");
                exit(1);
            }
            break;
        case 'T':
            lsdbu->T = atof(optarg);
            if (lsdbu->T <= 0) {
                fprintf(stderr, "Temperature must be positive\n");
                exit(1);
            }
            break;
        case 'p':
            action = LSDBU_ACTION_INTERPOLATE;
            break;
        case 'c':
            doppler = true;
            break;
        case 'I':
            action = LSDBU_ACTION_INIT;
            break;
        case 'U':
            action = LSDBU_ACTION_SET_UNITS;
            if (!strcmp(optarg, "1/cm")) {
                units = LSDB_UNITS_INV_CM;
            } else
            if (!strcmp(optarg, "eV")) {
                units = LSDB_UNITS_EV;
            } else
            if (!strcmp(optarg, "au")) {
                units = LSDB_UNITS_AU;
            } else
            if (!strcmp(optarg, "custom")) {
                units = LSDB_UNITS_CUSTOM;
            } else {
                fprintf(stderr, "Unrecognized units %s\n", optarg);
                exit(1);
            }
            break;
        case 'M':
            action = LSDBU_ACTION_ADD_MODEL;
            token = strtok(optarg, ",");
            ntoken = 1;
            while (token != NULL) {
                switch (ntoken) {
                case 1:
                    mname = token;
                    break;
                case 2:
                    mdescr = token;
                    break;
                default:
                    usage(argv[0], stderr);
                    exit(1);
                }
                token = strtok(NULL, ",");
                ntoken++;
            }
            break;
        case 'E':
            action = LSDBU_ACTION_ADD_ENV;
            token = strtok(optarg, ",");
            ntoken = 1;
            while (token != NULL) {
                switch (ntoken) {
                case 1:
                    ename = token;
                    break;
                case 2:
                    edescr = token;
                    break;
                default:
                    usage(argv[0], stderr);
                    exit(1);
                }
                token = strtok(NULL, ",");
                ntoken++;
            }
            break;
        case 'R':
            action = LSDBU_ACTION_ADD_RADIATOR;
            token = strtok(optarg, ",");
            ntoken = 1;
            while (token != NULL) {
                switch (ntoken) {
                case 1:
                    symbol = token;
                    break;
                case 2:
                    anum = atoi(token);
                    break;
                case 3:
                    zsp = atoi(token);
                    break;
                case 4:
                    mass = atof(token);
                    break;
                default:
                    usage(argv[0], stderr);
                    exit(1);
                }
                token = strtok(NULL, ",");
                ntoken++;
            }
            break;
        case 'L':
            action = LSDBU_ACTION_ADD_LINE;
            token = strtok(optarg, ",");
            ntoken = 1;
            while (token != NULL) {
                switch (ntoken) {
                case 1:
                    lname = token;
                    break;
                case 2:
                    w0 = atof(token);
                    break;
                default:
                    usage(argv[0], stderr);
                    exit(1);
                }
                token = strtok(NULL, ",");
                ntoken++;
            }
            break;
        case 'D':
            action = LSDBU_ACTION_ADD_DATA;
            fp_f = fopen(optarg, "rb");
            if (!fp_f) {
                fprintf(stderr, "Failed openning file %s\n", optarg);
                exit(1);
            }
            break;
        case 'P':
            action = LSDBU_ACTION_ADD_PROPERTY;
            token = strtok(optarg, ",");
            ntoken = 1;
            while (token != NULL) {
                switch (ntoken) {
                case 1:
                    pname = token;
                    break;
                case 2:
                    pvalue = token;
                    break;
                default:
                    usage(argv[0], stderr);
                    exit(1);
                }
                token = strtok(NULL, ",");
                ntoken++;
            }
            break;
        case 'X':
            action = LSDBU_ACTION_DEL_ENTITY;
            break;
        case 'v':
            lsdbu->verbose = true;
            break;
        case 'V':
            about();
            exit(0);
            break;
        case 'h':
            usage(argv[0], stdout);
            exit(0);
            break;
        default:
            usage(argv[0], stderr);
            exit(1);
            break;
        }
    }

    if (optind >= argc) {
        usage(argv[0], stderr);
        exit(1);
    }

    dbfile = argv[optind];

    switch (action) {
    case LSDBU_ACTION_INFO:
    case LSDBU_ACTION_GET_DATA:
    case LSDBU_ACTION_INTERPOLATE:
        db_access = LSDB_ACCESS_RO;
        break;
    case LSDBU_ACTION_INIT:
        db_access = LSDB_ACCESS_INIT;
        break;
    case LSDBU_ACTION_SET_UNITS:
    case LSDBU_ACTION_ADD_MODEL:
    case LSDBU_ACTION_ADD_ENV:
    case LSDBU_ACTION_ADD_RADIATOR:
    case LSDBU_ACTION_ADD_LINE:
    case LSDBU_ACTION_ADD_DATA:
    case LSDBU_ACTION_ADD_PROPERTY:
    case LSDBU_ACTION_DEL_ENTITY:
        db_access = LSDB_ACCESS_RW;
        break;
    case LSDBU_ACTION_NONE:
    default:
        usage(argv[0], stderr);
        exit(1);
        break;
    }

    lsdb = lsdb_open(dbfile, db_access);
    if (!lsdb) {
        fprintf(stderr, "DB initialization failed\n");
        exit(1);
    }

    if (action == LSDBU_ACTION_INIT) {
        ;
    } else
    if (action == LSDBU_ACTION_SET_UNITS) {
        id = lsdb_set_units(lsdb, units);
        if (lsdb_set_units(lsdb, units) != LSDB_SUCCESS) {
            fprintf(stderr, "Setting units failed\n");
            OK = false;
        }
    } else
    if (action == LSDBU_ACTION_ADD_MODEL) {
        id = lsdb_add_model(lsdb, mname, mdescr);
        if (id > 0) {
            fprintf(lsdbu->fp_out, "OK: id = %ld\n", id);
	} else {
            fprintf(stderr, "Adding radiators failed\n");
            OK = false;
        }
    } else
    if (action == LSDBU_ACTION_ADD_ENV) {
        id = lsdb_add_environment(lsdb, ename, edescr);
        if (id > 0) {
            fprintf(lsdbu->fp_out, "OK: id = %ld\n", id);
	} else {
            fprintf(stderr, "Adding radiators failed\n");
            OK = false;
        }
    } else
    if (action == LSDBU_ACTION_ADD_RADIATOR) {
        id = lsdb_add_radiator(lsdb, symbol, anum, mass, zsp);
        if (id > 0) {
            fprintf(lsdbu->fp_out, "OK: id = %ld\n", id);
	} else {
            fprintf(stderr, "Adding radiators failed\n");
            OK = false;
        }
    } else
    if (action == LSDBU_ACTION_ADD_LINE) {
        id = lsdb_add_line(lsdb, lsdbu->rid, lname, w0);
        if (id > 0) {
            fprintf(lsdbu->fp_out, "OK: id = %ld\n", id);
	} else {
            fprintf(stderr, "Adding line failed\n");
            OK = false;
        }
    } else
    if (action == LSDBU_ACTION_ADD_DATA) {
        if (lsdbu->lid == 0) {
            fprintf(stderr, "Line ID must be defined\n");
            OK = false;
        } else
        if (lsdbu->mid == 0 || lsdbu->eid == 0) {
            fprintf(stderr, "Environment and model IDs must be defined\n");
            OK = false;
        } else
        if (lsdbu->n == 0.0 || lsdbu->T == 0.0) {
            fprintf(stderr, "Density and temperature must be defined\n");
            OK = false;
        } else
        if (read_in(fp_f, &x, &y, &len) == LSDB_SUCCESS) {
            int did = lsdb_add_dataset(lsdb, lsdbu->mid, lsdbu->eid, lsdbu->lid,
                lsdbu->n, lsdbu->T, x, y, len);
            if (did <= 0) {
                fprintf(stderr, "Adding dataset failed\n");
                OK = false;
            }
            if (x) {
                free(x);
            }
            if (y) {
                free(y);
            }
        } else {
            OK = false;
        }
        fclose(fp_f);
    } else
    if (action == LSDBU_ACTION_ADD_PROPERTY) {
        if (lsdbu->lid == 0) {
            fprintf(stderr, "Line ID must be defined\n");
            OK = false;
        } else {
            pid = lsdb_add_line_property(lsdb, lsdbu->lid, pname, pvalue);
            if (pid > 0) {
                fprintf(lsdbu->fp_out, "OK: id = %ld\n", pid);
            } else {
                fprintf(stderr, "Adding line property failed\n");
                OK = false;
            }
        }
    } else
    if (action == LSDBU_ACTION_DEL_ENTITY) {
        if (pid > 0) {
            if (lsdb_del_line_property(lsdb, pid)) {
                fprintf(stderr, "Operation failed\n");
                OK = false;
            }
        } else
        if (did > 0) {
            if (lsdb_del_dataset(lsdb, did)) {
                fprintf(stderr, "Operation failed\n");
                OK = false;
            }
        } else
        if (lsdbu->lid > 0) {
            if (lsdb_del_line(lsdb, lsdbu->lid)) {
                fprintf(stderr, "Operation failed\n");
                OK = false;
            }
        } else
        if (lsdbu->rid > 0) {
            if (lsdb_del_radiator(lsdb, lsdbu->rid)) {
                fprintf(stderr, "Operation failed\n");
                OK = false;
            }
        } else
        if (lsdbu->eid > 0) {
            if (lsdb_del_environment(lsdb, lsdbu->eid)) {
                fprintf(stderr, "Operation failed\n");
                OK = false;
            }
        } else
        if (lsdbu->mid > 0) {
            if (lsdb_del_model(lsdb, lsdbu->mid)) {
                fprintf(stderr, "Operation failed\n");
                OK = false;
            }
        } else {
            fprintf(stderr, "No entity to delete specified\n");
            OK = false;
        }
    } else
    if (action == LSDBU_ACTION_INFO) {
        lsdb_units_t units = lsdb_get_units(lsdb);
        char *ustr = "none";
        switch (units) {
        case LSDB_UNITS_NONE:
            ustr = "none";
            break;
        case LSDB_UNITS_INV_CM:
            ustr = "cm^-1";
            break;
        case LSDB_UNITS_EV:
            ustr = "eV";
            break;
        case LSDB_UNITS_AU:
            ustr = "at. units";
            break;
        case LSDB_UNITS_CUSTOM:
            ustr = "custom";
            break;
        }
        if (lsdbu->verbose) {
            fprintf(lsdbu->fp_out, "Units: %s\n", ustr);
        }
        fprintf(lsdbu->fp_out, "Models:\n");
        lsdb_get_models(lsdb, model_sink, lsdbu);
        fprintf(lsdbu->fp_out, "Environments:\n");
        lsdb_get_environments(lsdb, environment_sink, lsdbu);
        fprintf(lsdbu->fp_out, "Radiators:\n");
        lsdb_get_radiators(lsdb, radiator_sink, lsdbu);
    } else
    if (action == LSDBU_ACTION_GET_DATA) {
        lsdb_dataset_data_t *ds = lsdb_get_dataset_data(lsdb, did);
        if (ds) {
            for (unsigned int i = 0; i < ds->len; i++) {
                fprintf(lsdbu->fp_out, "%g %g\n", ds->x[i], ds->y[i]);
            }

            lsdb_dataset_data_free(ds);
        } else {
            fprintf(stderr, "Failed fetching dataset %ld\n", did);
            OK = false;
        }
    } else
    if (action == LSDBU_ACTION_INTERPOLATE) {
        if (lsdbu->lid == 0) {
            fprintf(stderr, "Line ID must be defined\n");
            OK = false;
        } else
        if (lsdbu->mid == 0 || lsdbu->eid == 0) {
            fprintf(stderr, "Environment and model IDs must be defined\n");
            OK = false;
        } else
        if (lsdbu->n == 0.0 || lsdbu->T == 0.0) {
            fprintf(stderr, "Density and temperature must be defined\n");
            OK = false;
        } else {
            double sigma = 0.0;
            if (doppler) {
                sigma = lsdb_get_doppler_sigma(lsdb, lsdbu->lid, lsdbu->T);
            }
            lsdb_dataset_data_t *dsi = lsdb_get_interpolation(lsdb,
                lsdbu->mid, lsdbu->eid, lsdbu->lid,
                lsdbu->n, lsdbu->T, 2001, sigma, 0);

            if (dsi) {
                for (unsigned int i = 0; i < 2001; i++) {
                    fprintf(lsdbu->fp_out, "%g %g\n", dsi->x[i], dsi->y[i]);
                }

                lsdb_dataset_data_free(dsi);
            } else {
                fprintf(stderr, "Interpolation failed\n");
                OK = false;
            }

        }
    }

    lsdb_close(lsdb);

    fclose(lsdbu->fp_out);

    exit(OK ? 0:1);
}

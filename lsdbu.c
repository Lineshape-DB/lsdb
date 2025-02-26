#include <getopt.h>
#include <math.h>

#include <lsdb.h>
#include <morph.h>

#define LSDBU_ACTION_NONE           0
#define LSDBU_ACTION_INFO           1
#define LSDBU_ACTION_INIT           2
#define LSDBU_ACTION_ADD_MODEL      3
#define LSDBU_ACTION_ADD_ENV        4
#define LSDBU_ACTION_ADD_RADIATOR   5
#define LSDBU_ACTION_ADD_LINE       6
#define LSDBU_ACTION_ADD_DATA       7
#define LSDBU_ACTION_DEL_ENTITY     8
#define LSDBU_ACTION_GET_DATA       9
#define LSDBU_ACTION_INTERPOLATE   10

typedef struct {
    FILE *fp_out;
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

static int dataset_sink(const lsdb_t *lsdb,
    const lsdb_dataset_t *cbdata, void *udata)
{
    lsdbu_t *lsdbu = udata;
    (void)(lsdb);

    if ((lsdbu->eid != 0 && cbdata->eid != lsdbu->eid) ||
        (lsdbu->mid != 0 && cbdata->mid != lsdbu->mid) ||
        (lsdbu->n   >  0 && cbdata->n   != lsdbu->n)   ||
        (lsdbu->T   >  0 && cbdata->T   != lsdbu->T)) {
        return LSDB_SUCCESS;
    }

    fprintf(lsdbu->fp_out,
        "      id = %lu: (mid = %lu, eid = %lu, n_e = %g cm^-3, T = %g eV)\n",
        cbdata->id, cbdata->mid, cbdata->eid, cbdata->n, cbdata->T);

    return LSDB_SUCCESS;
}

static int line_sink(const lsdb_t *lsdb,
    const lsdb_line_t *l, void *udata)
{
    lsdbu_t *lsdbu = udata;

    if (lsdbu->lid != 0 && l->id != lsdbu->lid) {
        return LSDB_SUCCESS;
    }

    fprintf(lsdbu->fp_out, "    id = %lu: \"%s\" (%g cm^-1 => %g eV)\n",
        l->id, l->name, l->energy, l->energy/8065.5);

    fprintf(lsdbu->fp_out, "    Datasets:\n");
    lsdb_get_datasets(lsdb, l->id, dataset_sink, lsdbu);

    return LSDB_SUCCESS;
}

static int radiator_sink(const lsdb_t *lsdb,
    const lsdb_radiator_t *r, void *udata)
{
    lsdbu_t *lsdbu = udata;

    if (lsdbu->rid != 0 && r->id != lsdbu->rid) {
        return LSDB_SUCCESS;
    }

    fprintf(lsdbu->fp_out, "  id = %lu: \"%s\" (A = %d, Zsp = %d, mass = %g)\n",
        r->id, r->sym, r->anum, r->zsp, r->mass);

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
    if (e->descr && strlen(e->descr) > 0) {
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
    if (m->descr && strlen(m->descr) > 0) {
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
    fprintf(out, "  -n <n>                set electron density to n/cc [0]\n");
    fprintf(out, "  -T <T>                set temperature to T eV [0]\n");
    fprintf(out, "  -p                    print interpolated lineshape\n");
    fprintf(out, "  -c                    convolve with the Doppler broadening\n");
    fprintf(out, "  -I                    initialize the DB\n");
    fprintf(out, "  -M <name[,descr]>     add a model\n");
    fprintf(out, "  -E <name[,descr]>     add an environment\n");
    fprintf(out, "  -R <sym,A,Zsp,M>      add a radiator\n");
    fprintf(out, "  -L <name,w0>          add a line\n");
    fprintf(out, "  -D <filename>         add a dataset\n");
    fprintf(out, "  -X                    delete an entity by its ID\n");
    fprintf(out, "  -h                    print this help\n");
}

int main(int argc, char **argv)
{
    lsdbu_t LSDBU, *lsdbu = &LSDBU;

    int action = LSDBU_ACTION_NONE;
    char *dbfile;
    int db_access = LSDB_OPEN_RO;
    lsdb_t *lsdb;
    bool OK = true;
    FILE *fp_f = NULL;
    long int id = 0, did = 0;
    const char *token;
    int ntoken = 0;
    const char *mname = NULL, *ename = NULL, *mdescr = "", *edescr = "";
    const char *symbol = NULL, *lname = NULL;
    int anum = 0, zsp = 0;
    double mass = 0, w0 = 0, *x = NULL, *y = NULL;
    size_t len;
    bool doppler = false;

    int opt;

    memset(lsdbu, 0, sizeof(lsdbu_t));
    lsdbu->fp_out = stdout;

    while ((opt = getopt(argc, argv, "id:o:m:e:r:l:n:T:pcIM:E:R:L:D:Xh")) != -1) {
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
        case 'X':
            action = LSDBU_ACTION_DEL_ENTITY;
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
        db_access = LSDB_OPEN_RO;
        break;
    case LSDBU_ACTION_INIT:
        db_access = LSDB_OPEN_INIT;
        break;
    case LSDBU_ACTION_ADD_MODEL:
    case LSDBU_ACTION_ADD_ENV:
    case LSDBU_ACTION_ADD_RADIATOR:
    case LSDBU_ACTION_ADD_LINE:
    case LSDBU_ACTION_ADD_DATA:
    case LSDBU_ACTION_DEL_ENTITY:
        db_access = LSDB_OPEN_RW;
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
    if (action == LSDBU_ACTION_ADD_MODEL) {
        int rid = lsdb_add_model(lsdb, mname, mdescr);
        if (rid <= 0) {
            fprintf(stderr, "Adding radiators failed\n");
            OK = false;
        }
    } else
    if (action == LSDBU_ACTION_ADD_ENV) {
        int rid = lsdb_add_environment(lsdb, ename, edescr);
        if (rid <= 0) {
            fprintf(stderr, "Adding radiators failed\n");
            OK = false;
        }
    } else
    if (action == LSDBU_ACTION_ADD_RADIATOR) {
        id = lsdb_add_radiator(lsdb, symbol, anum, mass, zsp);
        if (id <= 0) {
            fprintf(stderr, "Adding radiators failed\n");
            OK = false;
        }
    } else
    if (action == LSDBU_ACTION_ADD_LINE) {
        id = lsdb_add_line(lsdb, lsdbu->rid, lname, w0);
        if (id <= 0) {
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
    if (action == LSDBU_ACTION_DEL_ENTITY) {
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
            lsdb_dataset_data_t *dsi = lsdb_get_interpolation(lsdb,
                lsdbu->mid, lsdbu->eid, lsdbu->lid,
                lsdbu->n, lsdbu->T, 2001, doppler);

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

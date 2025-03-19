#include <stdlib.h>
#include <math.h>
#include <fftw3.h>

#include <lsdb/lsdbP.h>
#include <lsdb/morph.h>

double lsdb_get_doppler_sigma(const lsdb_t *lsdb, unsigned long lid, double T)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    double sigma = 0.0;

    if (!lsdb) {
        return 0.0;
    }

    sql = "SELECT l.energy, r.mass " \
          " FROM lines AS l INNER JOIN radiators AS r ON (r.id = l.rid)" \
          " WHERE l.id = ?";

    sqlite3_prepare_v2(lsdb->db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, lid);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        double energy = sqlite3_column_double(stmt, 0);
        double mass   = sqlite3_column_double(stmt, 1);

        sigma = 3.265e-5*energy*sqrt(T/mass);
    } else {
        lsdb_errmsg(lsdb, "SQL error: %s\n", sqlite3_errmsg(lsdb->db));
    }

    sqlite3_finalize(stmt);

    return sigma;
}

/* convolution with a Voigt function; original data are replaced! */
static int voigt_conv(double *y, size_t n, double dx, double sigma, double gamma)
{
    size_t i;
    fftw_plan xplan, zplan;
    double *yf;

    yf = malloc(sizeof(double)*n);
    if (!yf) {
        return LSDB_FAILURE;
    }

    xplan = fftw_plan_r2r_1d(n, y, yf, FFTW_REDFT00, FFTW_ESTIMATE);
    fftw_execute(xplan);
    fftw_destroy_plan(xplan);

    for (i = 0; i < n; i++) {
        /* 2 due to symmetry - we use half-length FFT */
        double t = 2*M_PI*i/(2*(n - 1)*dx);
        yf[i] *= exp(-gamma*t - sigma*sigma*t*t/2)/(2*(n - 1));
    }

    zplan = fftw_plan_r2r_1d(n, yf, y, FFTW_REDFT00, FFTW_ESTIMATE);
    fftw_execute(zplan);
    fftw_destroy_plan(zplan);

    free(yf);

    return LSDB_SUCCESS;
}


lsdb_interp_t *lsdb_prepare_interpolation(const lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double n, double T, unsigned int len)
{
    long unsigned did1, did2, did3, did4;
    int rc;
    bool OK = true;
    lsdb_interp_t *interp = NULL;

    rc = lsdb_get_closest_dids(lsdb, mid, eid, lid, n, T, &did1, &did2, &did3, &did4);
    if (rc == LSDB_SUCCESS) {
        lsdb_dataset_data_t *ds1, *ds2, *ds3, *ds4;

        ds1 = lsdb_get_dataset_data(lsdb, did1);
        ds2 = lsdb_get_dataset_data(lsdb, did2);
        ds3 = lsdb_get_dataset_data(lsdb, did3);
        ds4 = lsdb_get_dataset_data(lsdb, did4);

        if (ds1 && ds2 && ds3 && ds4) {
            double n1 = ds1->n, n2 = ds2->n, n3 = ds3->n, n4 = ds4->n;
            double T1 = ds1->T, T2 = ds2->T, T3 = ds3->T, T4 = ds4->T;
            double xmin, xmax, t;
            double Tm1, Tm2;
            double *xm1, *xm2, *ym1, *ym2;
            double x, r;

            morph_t *m = morph_new(len);

            xm1 = malloc(len*sizeof(double));
            xm2 = malloc(len*sizeof(double));
            ym1 = malloc(len*sizeof(double));
            ym2 = malloc(len*sizeof(double));

            morph_init(m, ds1->x, ds1->y, ds1->len,
                ds2->x, ds2->y, ds2->len);

            morph_get_domain(m, &xmin, &xmax);

            if (n1 == n2) {
                t = 0.0;
            } else {
                t = sqrt(log(n/n1)/log(n2/n1));
            }
            Tm1 = T1*pow(T2/T1, t*t);

            for (unsigned int i = 0; i < len; i++) {
                x = xmin + i*(xmax - xmin)/(len - 1);
                /* safety check against rounding error */
                if (x > xmax) {
                    x = xmax;
                }
                r = morph_eval(m, t, x, false);

                xm1[i] = x;
                ym1[i] = r;
            }

            morph_init(m, ds4->x, ds4->y, ds4->len,
                ds3->x, ds3->y, ds3->len);

            morph_get_domain(m, &xmin, &xmax);

            if (n3 == n4) {
                t = 0.0;
            } else {
                t = sqrt(log(n/n4)/log(n3/n4));
            }
            Tm2 = T4*pow(T3/T4, t*t);

            for (unsigned int i = 0; i < len; i++) {
                x = xmin + i*(xmax - xmin)/(len - 1);
                /* safety check against rounding error */
                if (x > xmax) {
                    x = xmax;
                }
                r = morph_eval(m, t, x, false);

                xm2[i] = x;
                ym2[i] = r;
            }

            morph_init(m, xm1, ym1, len, xm2, ym2, len);

            morph_get_domain(m, &xmin, &xmax);

            if (Tm1 == Tm2) {
                t = 0.0;
            } else {
                t = sqrt(log(T/Tm1)/log(Tm2/Tm1));
            }

            interp = malloc(sizeof(lsdb_interp_t));
            interp->morph = m;
            interp->t     = t;

            free(xm1);
            free(xm2);
            free(ym1);
            free(ym2);
        } else {
            lsdb_errmsg(lsdb, "Failed fetching dataset(s)\n");
            OK = false;
        }

        lsdb_dataset_data_free(ds1);
        lsdb_dataset_data_free(ds2);
        lsdb_dataset_data_free(ds3);
        lsdb_dataset_data_free(ds4);
    } else {
        OK = false;
    }

    return OK ? interp:NULL;
}

void lsdb_interp_free(lsdb_interp_t *interp)
{
    if (interp) {
        morph_free(interp->morph);
        free(interp);
    }
}

int lsdb_interp_get_domain(const lsdb_interp_t *interp, double *xmin, double *xmax)
{
    if (interp && morph_get_domain(interp->morph, xmin, xmax)) {
        return LSDB_SUCCESS;
    } else {
        return LSDB_FAILURE;
    }
}

double lsdb_interp_eval(const lsdb_interp_t *interp, double x, bool normalize)
{
    return morph_eval(interp->morph, interp->t, x, normalize);
}

lsdb_dataset_data_t *lsdb_get_interpolation(const lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double n, double T, unsigned int len, double sigma, double gamma)
{
    bool OK = true;
    lsdb_dataset_data_t *dsi = NULL;
    lsdb_interp_t *interp;

    interp = lsdb_prepare_interpolation(lsdb, mid, eid, lid, n, T, len);
    if (interp != NULL) {
        dsi = lsdb_dataset_data_new(n, T, len);

        if (dsi) {
            double xmin, xmax;
            double x, r;

            lsdb_interp_get_domain(interp, &xmin, &xmax);

            double dx = (xmax - xmin)/(len - 1);
            for (unsigned int i = 0; i < len; i++) {
                x = xmin + i*dx;
                /* safety check against rounding error */
                if (x > xmax) {
                    x = xmax;
                }
                r = lsdb_interp_eval(interp, x, false);

                dsi->x[i] = x;
                dsi->y[i] = r;
            }

            if (OK && (sigma > 0.0 || gamma > 0.0)) {
                if (voigt_conv(dsi->y, len, dx, sigma, gamma) != LSDB_SUCCESS) {
                    lsdb_errmsg(lsdb, "Convolution failed\n");
                    OK = false;
                }
            }
        } else {
            lsdb_errmsg(lsdb, "Failed allocating dataset\n");
            OK = false;
        }

        lsdb_interp_free(interp);
    } else {
        OK = false;
    }

    return OK ? dsi:NULL;
}

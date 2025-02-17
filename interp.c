#include <stdbool.h>
#include <lsdb.h>
#include <morph.h>
#include <math.h>

lsdb_dataset_data_t *lsdb_get_interpolation(const lsdb_t *lsdb,
    unsigned int mid, unsigned int eid, unsigned int lid,
    double n, double T, unsigned int len)
{
    long unsigned did1, did2, did3, did4;
    int rc;
    bool OK = true;
    lsdb_dataset_data_t *dsi = NULL;

    rc = lsdb_get_closest_dids(lsdb, mid, eid, lid, n, T,
        &did1, &did2, &did3, &did4);
    if (rc == LSDB_SUCCESS) {
        lsdb_dataset_data_t *ds1, *ds2, *ds3, *ds4;

        ds1 = lsdb_get_dataset_data(lsdb, did1);
        ds2 = lsdb_get_dataset_data(lsdb, did2);
        ds3 = lsdb_get_dataset_data(lsdb, did3);
        ds4 = lsdb_get_dataset_data(lsdb, did4);

        dsi = lsdb_dataset_data_new(n, T, len);

        if (ds1 && ds2 && ds3 && ds4 && dsi) {
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

            for (unsigned int i = 0; i < len; i++) {
                x = xmin + i*(xmax - xmin)/(len - 1);
                r = morph_eval(m, t, x, false);

                dsi->x[i] = x;
                dsi->y[i] = r;
            }

            free(xm1);
            free(xm2);
            free(ym1);
            free(ym2);

            morph_free(m);
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

    return OK ? dsi:NULL;
}

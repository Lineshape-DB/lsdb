#include <string.h>
#include <math.h>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>

#include <morphP.h>

#define MAX2(a,b) ((a) > (b) ? (a):(b))
#define MIN2(a,b) ((a) < (b) ? (a):(b))

void morph_free(morph_t *m)
{
    if (m) {
        if (m->acc_f) {
            gsl_interp_accel_free(m->acc_f);
        }
        if (m->acc_M) {
            gsl_interp_accel_free(m->acc_M);
        }

        if (m->spline_f) {
            gsl_spline_free(m->spline_f);
        }
        if (m->spline_M) {
            gsl_spline_free(m->spline_M);
        }

        free(m);
    }
}

morph_t *morph_new(size_t np)
{
    morph_t *m = malloc(sizeof(morph_t));
    if (!m) {
        return NULL;
    }
    memset(m, 0, sizeof(morph_t));

    m->np = np;

    m->spline_f = gsl_spline_alloc(gsl_interp_steffen, np);
    if (!m->spline_f) {
        morph_free(m);
        return NULL;
    }
    m->spline_M = gsl_spline_alloc(gsl_interp_steffen, np);
    if (!m->spline_M) {
        morph_free(m);
        return NULL;
    }

    m->acc_f = gsl_interp_accel_alloc();
    m->acc_M = gsl_interp_accel_alloc();
    if (!m->acc_f || !m->acc_M) {
        morph_free(m);
        return NULL;
    }

    return m;
}

void morph_aux_free(morph_aux_t *m)
{
    if (m) {
        if (m->x) {
            free(m->x);
        }
        if (m->M) {
            free(m->M);
        }
        if (m->F) {
            free(m->F);
        }
        if (m->G) {
            free(m->G);
        }

        if (m->acc_g) {
            gsl_interp_accel_free(m->acc_g);
        }
        if (m->acc_f_inv) {
            gsl_interp_accel_free(m->acc_f_inv);
        }

        if (m->spline_g) {
            gsl_spline_free(m->spline_g);
        }
        if (m->spline_f_inv) {
            gsl_spline_free(m->spline_f_inv);
        }

        free(m);
    }
}

morph_aux_t *morph_aux_new(size_t np)
{
    morph_aux_t *ma = malloc(sizeof(morph_aux_t));
    if (!ma) {
        return NULL;
    }
    memset(ma, 0, sizeof(morph_aux_t));

    ma->F = calloc(np, sizeof(double));
    ma->G = calloc(np, sizeof(double));

    ma->x = malloc(np*sizeof(double));
    ma->M = malloc(np*sizeof(double));
    if (!ma->x || !ma->M || !ma->F || !ma->G) {
        morph_aux_free(ma);
        return NULL;
    }

    ma->spline_f_inv = gsl_spline_alloc(gsl_interp_steffen, np);
    if (!ma->spline_f_inv) {
        morph_aux_free(ma);
        return NULL;
    }

    ma->acc_g     = gsl_interp_accel_alloc();
    ma->acc_f_inv = gsl_interp_accel_alloc();
    if (!ma->acc_g || !ma->acc_f_inv) {
        morph_aux_free(ma);
        return NULL;
    }

    return ma;
}

bool morph_init(morph_t *m,
    const double *xf, const double *yf, size_t lenf,
    const double *xg, const double *yg, size_t leng)
{
    double xf_min, xf_max, xg_min, xg_max;

    morph_aux_t *ma = morph_aux_new(m->np);
    if (!ma) {
        return false;
    }
    ma->spline_g = gsl_spline_alloc(gsl_interp_steffen, leng);
    if (!ma->spline_g) {
        morph_aux_free(ma);
        return false;
    }
    gsl_spline_init(ma->spline_g, xg, yg, leng);

    if (m->spline_f) {
        gsl_spline_free(m->spline_f);
    }
    m->spline_f = gsl_spline_alloc(gsl_interp_steffen, lenf);
    if (!m->spline_f) {
        morph_aux_free(ma);
        return false;
    }
    gsl_spline_init(m->spline_f, xf, yf, lenf);

    xf_min = xf[0];
    xf_max = xf[lenf - 1];
    xg_min = xg[0];
    xg_max = xg[leng - 1];
    m->xmin = MAX2(xf_min, xg_min);
    m->xmax = MIN2(xf_max, xg_max);

    for (unsigned int i = 0; i < m->np; i++) {
        double x = m->xmin + i*(m->xmax - m->xmin)/(m->np - 1);
        if (x > m->xmax) {
            x = m->xmax;
        }

        /* store the grid */
        ma->x[i] = x;

        /* calculate CDFs */
        ma->F[i] = gsl_spline_eval_integ(m->spline_f, m->xmin, x, m->acc_f);
        ma->G[i] = gsl_spline_eval_integ(ma->spline_g, m->xmin, x, ma->acc_g);
    }

    /* normalize CDFs to unity */
    m->norm_f = ma->F[m->np - 1];
    m->norm_g = ma->G[m->np - 1];
    for (unsigned int i = 0; i < m->np; i++) {
        ma->F[i] /= m->norm_f;
        ma->G[i] /= m->norm_g;
    }

    /* prepare spline for the quantile of F */
    gsl_spline_init(ma->spline_f_inv, ma->F, ma->x, m->np);

    /* calculate M on the grid */
    for (unsigned int i = 0; i < m->np; i++) {
        ma->M[i] = gsl_spline_eval(ma->spline_f_inv, ma->G[i], ma->acc_f_inv);
    }

    /* prepare spline for M */
    gsl_spline_init(m->spline_M, ma->x, ma->M, m->np);

    morph_aux_free(ma);

    return true;
}

double morph_eval(const morph_t *m, double t, double x, bool normalize)
{
    double nfactor, T, M, dM_dx, dT_dx, r;

    M     = gsl_spline_eval(m->spline_M, x, m->acc_M);
    dM_dx = gsl_spline_eval_deriv(m->spline_M, x, m->acc_M);

    T     = (1 - t)*x + t*M;
    dT_dx = (1 - t)   + t*dM_dx;

    if (normalize) {
        nfactor = 1/m->norm_f;
    } else {
        /* interpolate integral values */
        nfactor = (1 - t) + t*m->norm_g/m->norm_f;
    }

    if (T >= m->xmin && T <= m->xmax) {
        r = nfactor*fabs(dT_dx)*gsl_spline_eval(m->spline_f, T, m->acc_f);
    } else {
        r = 0.0;
    }

    return r;
}

bool morph_get_domain(const morph_t *m, double *xmin, double *xmax)
{
    if (m) {
        *xmin = m->xmin;
        *xmax = m->xmax;

        return true;
    } else {
        return false;
    }
}

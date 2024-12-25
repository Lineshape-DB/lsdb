/* gcc -Wall -pedantic -O2 -o morph morph.c -lgsl -lm */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <getopt.h>

#include <assert.h>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>

#define NPOINTS 2001

#define SQR(x) ((x)*(x))
#define MAX2(a,b) ((a) > (b) ? (a):(b))
#define MIN2(a,b) ((a) < (b) ? (a):(b))

static bool read_in(FILE *fp, double **xap, double **yap, int *lenp)
{
    int in_allocated = 0, len = 0;
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
            return false;
        }
        if (y < 0) {
            fprintf(stderr, "y must be >= 0\n");
            return false;
        }

        if (len >= in_allocated) {
            xa = realloc(xa, sizeof(double)*(in_allocated + 128));
            ya = realloc(ya, sizeof(double)*(in_allocated + 128));

            if (!xa || !ya) {
                fprintf(stderr, "Memory allocation failed\n");
                return false;
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

    return true;
}

void regularize_f(double *x, double *y, int len, double *d, double *s)
{
    int i;
    double w, y2sum = 0.0, xy2sum = 0.0, x2y2sum = 0.0;

    for (i = 0; i < len; i++) {
        w = SQR(y[i]);
        y2sum  += w;
        xy2sum += x[i]*w;
    }

    *d = xy2sum/y2sum;

    // subtract the mean x and calculate "variance"
    for (i = 0; i < len; i++) {
        x[i] -= *d;
        w = SQR(y[i]);
        x2y2sum += SQR(x[i])*w;
    }

    *s = sqrt(x2y2sum/y2sum);

    // scale, preserving the integral norm
    for (i = 0; i < len; i++) {
        x[i] /= *s;
        y[i] *= *s;
    }
}


static void usage(const char *arg0, FILE *out)
{
    fprintf(out, "Usage: %s [options]\n", arg0);
    fprintf(out, "Available options:\n");
    fprintf(out, "  -i <filename> input initial spectrum [none]\n");
    fprintf(out, "  -f <filename> input final spectrum [none]\n");
    fprintf(out, "  -o <filename> output spectrum to filename [stdout]\n");
    fprintf(out, "  -t <val|n>    set the morphing value (0 - 1) or grid size (n > 1)\n");
    fprintf(out, "  -n            area-normalize output to unity\n");
    fprintf(out, "  -r            regularize the input spectra\n");
    fprintf(out, "  -d            enable some debugging\n");
    fprintf(out, "  -h            print this help\n");
}

typedef struct {
    size_t np;
    gsl_spline *spline_f, *spline_M;
    gsl_interp_accel *acc_f, *acc_M;
    double xmin, xmax;
    double norm_f, norm_g;
} morph_t;

typedef struct {
    gsl_spline *spline_g, *spline_f_inv;
    gsl_interp_accel *acc_g, *acc_f_inv;
    double *F, *G;
    double *x, *M;
} morph_aux_t;

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

int main(int argc, char **argv)
{
    double t = 0.0;
    double *xf, *yf, *xg, *yg;
    int lenf, leng, nt;

    morph_t *m;

    FILE *fp_out = stdout, *fp_f = NULL, *fp_g = NULL;

    double d_f = 0.0, s_f = 1.0, d_g = 0.0, s_g = 1.0;

    bool debug = false, normalize = false, regularize = false;

    int opt;

    while ((opt = getopt(argc, argv, "i:f:t:o:nrdh")) != -1) {
        switch (opt) {
        case 'i':
            fp_f = fopen(optarg, "rb");
            if (!fp_f) {
                fprintf(stderr, "Failed openning file %s\n", optarg);
                exit(1);
            }
            break;
        case 'f':
            fp_g = fopen(optarg, "rb");
            if (!fp_g) {
                fprintf(stderr, "Failed openning file %s\n", optarg);
                exit(1);
            }
            break;
        case 'o':
            fp_out = fopen(optarg, "wb");
            if (!fp_out) {
                fprintf(stderr, "Failed openning file %s\n", optarg);
                exit(1);
            }
            break;
        case 't':
            t = atof(optarg);
            break;
        case 'n':
            normalize = true;
            break;
        case 'r':
            regularize = true;
            break;
        case 'd':
            debug = true;
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

    if (t > 2.0 && rint(t) == t) {
        nt = rint(t);
    } else {
        nt = 1;
        if (t < 0.0 || t > 1.0) {
            fprintf(stderr, "t must be between 0 and 1\n");
            exit(1);
        }
    }

    if (!fp_f) {
        fprintf(stderr, "No initial spectrum defined\n");
        exit(1);
    }
    if (!fp_g) {
        fprintf(stderr, "No final spectrum defined\n");
        exit(1);
    }

    if (read_in(fp_f, &xf, &yf, &lenf) != true) {
        exit(1);
    }
    fclose(fp_f);

    if (read_in(fp_g, &xg, &yg, &leng) != true) {
        exit(1);
    }
    fclose(fp_g);

    if (regularize) {
        regularize_f(xf, yf, lenf, &d_f, &s_f);
        regularize_f(xg, yg, leng, &d_g, &s_g);
    }

    if (debug) {
        fprintf(stderr, "d_f = %g, s_f = %g\n", d_f, s_f);
        fprintf(stderr, "d_g = %g, s_g = %g\n", d_g, s_g);
    }

    m = morph_new(NPOINTS);
    if (!m) {
        fprintf(stderr, "Allocation failed\n");
        exit(1);
    }

    if (morph_init(m, xf, yf, lenf, xg, yg, leng) != true) {
        fprintf(stderr, "Initialization failed\n");
        exit(1);
    }

    free(xf);
    free(yf);
    free(xg);
    free(yg);

    for (int it = 0; it < nt; it++) {
        double ti, d, s;
        if (nt > 1) {
            ti = (double) it/(nt - 1);
        } else {
            ti = t;
        }

        d = (1 - ti)*d_f + ti*d_g;
        s = (1 - ti)*s_f + ti*s_g;

        for (unsigned int i = 0; i < NPOINTS; i++) {
            double x = m->xmin + i*(m->xmax - m->xmin)/(m->np - 1);

            double r = morph_eval(m, ti, x, normalize)/s;

            double x_dereg = x*s + d;

            fprintf(fp_out, "%g %g\n", x_dereg, r);
        }

        if (it < nt - 1) {
            fprintf(fp_out, "\n");
        }
    }

    fclose(fp_out);

    morph_free(m);

    exit(0);
}

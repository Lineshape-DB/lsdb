/* gcc -Wall -pedantic -O2 -o morph morph.c -lgsl -lm */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <getopt.h>

#include <assert.h>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>

#define EPSILON 1.0e-6
#define WORKSPACE   1000

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

int main(int argc, char **argv)
{
    double t = 0.0;
    double *xf, *yf, *xg, *yg;
    int lenf, leng, nt;
    double xf_min, xf_max, xg_min, xg_max, xmin, xmax;
    double x[NPOINTS], F[NPOINTS], G[NPOINTS], m[NPOINTS], dm_dx[NPOINTS];

    FILE *fp_out = stdout, *fp_f = NULL, *fp_g = NULL;

    double d_f = 0.0, s_f = 1.0, d_g = 0.0, s_g = 1.0;

    gsl_interp_accel *acc_f, *acc_g, *acc_f_inv;
    gsl_spline *spline_f, *spline_g, *spline_f_inv;

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

    xf_min = xf[0];
    xf_max = xf[lenf - 1];
    xg_min = xg[0];
    xg_max = xg[leng - 1];
    xmin = MAX2(xf_min, xg_min);
    xmax = MIN2(xf_max, xg_max);

    spline_f = gsl_spline_alloc(gsl_interp_steffen, lenf);
    spline_g = gsl_spline_alloc(gsl_interp_steffen, leng);
    acc_f = gsl_interp_accel_alloc();
    acc_g = gsl_interp_accel_alloc();
    acc_f_inv = gsl_interp_accel_alloc();

    if (!spline_f || !spline_g || !acc_f || !acc_g || !acc_f_inv) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    gsl_spline_init(spline_f, xf, yf, lenf);
    gsl_spline_init(spline_g, xg, yg, leng);

    /* these are already copied to the spline objects */
    free(xf);
    free(yf);
    free(xg);
    free(yg);

    for (int i = 0; i < NPOINTS; i++) {
        x[i] = xmin + i*(xmax - xmin)/(NPOINTS - 1);
        if (x[i] > xmax) {
            x[i] = xmax;
        }

        F[i] = gsl_spline_eval_integ(spline_f, xmin, x[i], acc_f);
        G[i] = gsl_spline_eval_integ(spline_g, xmin, x[i], acc_g);
    }

    /* normalize to unity */
    double norm_f = F[NPOINTS - 1];
    double norm_g = G[NPOINTS - 1];
    for (int i = 0; i < NPOINTS; i++) {
        F[i] /= norm_f;
        G[i] /= norm_g;
    }

    spline_f_inv = gsl_spline_alloc(gsl_interp_steffen, NPOINTS);
    gsl_spline_init(spline_f_inv, F, x, NPOINTS);

    double m_prev = 0.0;
    for (int i = 0; i < NPOINTS; i++) {
        m[i] = gsl_spline_eval(spline_f_inv, G[i], acc_f_inv);
        if (i == 0) {
            dm_dx[i] = 0.0;
        } else {
            dm_dx[i] = (m[i] - m_prev)/(x[i] - x[i - 1]);
        }
        m_prev = m[i];

        if (debug) {
            fprintf(fp_out, "%g %g %g %g %g\n",
                x[i], F[i], G[i], m[i], dm_dx[i]);
        }
    }

    for (int it = 0; it < nt; it++) {
        double ti, nfactor, d, s;
        if (nt > 1) {
            ti = (double) it/(nt - 1);
        } else {
            ti = t;
        }

        d = (1 - ti)*d_f + ti*d_g;
        s = (1 - ti)*s_f + ti*s_g;

        if (normalize) {
            nfactor = 1/norm_f;
        } else {
            nfactor = (1 - ti) + ti*norm_g/norm_f;
        }
        nfactor /= s;

        for (int i = 0; i < NPOINTS; i++) {
            double T, dT_dx, r, x_dereg;
            T     = (1 - ti)*x[i] + ti*m[i];
            dT_dx = (1 - ti)      + ti*dm_dx[i];

            x_dereg = x[i]*s + d;

            if (T >= xf_min && T <= xf_max) {
                r = nfactor*fabs(dT_dx)*gsl_spline_eval(spline_f, T, acc_f);
            } else {
                r = 0.0;
            }

            fprintf(fp_out, "%g %g\n", x_dereg, r);
        }

        if (it < nt - 1) {
            fprintf(fp_out, "\n");
        }
    }

    fclose(fp_out);

    gsl_interp_accel_free(acc_f);
    gsl_interp_accel_free(acc_g);
    gsl_interp_accel_free(acc_f_inv);
    gsl_spline_free(spline_f);
    gsl_spline_free(spline_g);
    gsl_spline_free(spline_f_inv);

    exit(0);
}

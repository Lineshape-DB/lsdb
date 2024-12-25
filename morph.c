#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <getopt.h>

#include <morph.h>

#define NPOINTS 2001

#define SQR(x) ((x)*(x))

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

static void regularize_f(double *x, double *y, int len, double *d, double *s)
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

    morph_t *m;

    FILE *fp_out = stdout, *fp_f = NULL, *fp_g = NULL;

    double d_f = 0.0, s_f = 1.0, d_g = 0.0, s_g = 1.0;
    double xmin, xmax;

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

    morph_get_domain(m, &xmin, &xmax);

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
            double x = xmin + i*(xmax - xmin)/(NPOINTS - 1);

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

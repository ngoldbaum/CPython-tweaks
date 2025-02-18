#define PY_SSIZE_T_CLEAN

#include<Python.h>

#include <stdio.h>
#include <sleef.h>
#include <sleefquad.h>
#include <omp.h>
#include <math.h>

typedef struct {
    unsigned char r, g, b;
} Color;

Sleef_quad get_abs_value(Sleef_quad *r, Sleef_quad *i) 
{
    Sleef_quad rr = Sleef_mulq1_u05(*r, *r);
    Sleef_quad ii = Sleef_mulq1_u05(*i, *i);
    return Sleef_addq1_u05(rr, ii);
}

Color get_color(double t) 
{
    Color color;
    t = fmod(t * 20, 1.0); // Adjust color cycle frequency for more bands

    if (t < 0.16) 
    {
        // Dark blue to light blue
        double v = t / 0.16;
        color.r = 0;
        color.g = (unsigned char)(150 * v);
        color.b = (unsigned char)(255 * (0.5 + 0.5 * v));
    } 
    else if (t < 0.42) 
    {
        // Light blue to white
        double v = (t - 0.16) / 0.26;
        color.r = (unsigned char)(255 * v);
        color.g = (unsigned char)(150 + 105 * v);
        color.b = 255;
    } 
    else if (t < 0.6425) 
    {
        // White to yellow
        double v = (t - 0.42) / 0.2225;
        color.r = 255;
        color.g = 255;
        color.b = (unsigned char)(255 * (1 - v));
    } 
    else if (t < 0.8575) 
    {
        // Yellow to orange
        double v = (t - 0.6425) / 0.215;
        color.r = 255;
        color.g = (unsigned char)(255 * (1 - 0.4 * v));
        color.b = 0;
    } 
    else 
    {
        // Orange to dark blue (cycle completion)
        double v = (t - 0.8575) / 0.1425;
        color.r = (unsigned char)(255 * (1 - v));
        color.g = (unsigned char)(153 * (1 - v));
        color.b = (unsigned char)(128 * v);
    }

    return color;
}

float mandelbrot(Sleef_quad *cr, Sleef_quad *ci, int MAX_ITER, Sleef_quad *RADIUS2) 
{
    Sleef_quad z_real = 0.0Q;
    Sleef_quad z_img = 0.0Q;
    Sleef_quad z_real2, z_img2;

    int i;

    for (i = 0; i < MAX_ITER; i++) 
    {
        z_real2 = Sleef_addq1_u05(Sleef_subq1_u05(Sleef_mulq1_u05(z_real, z_real), Sleef_mulq1_u05(z_img, z_img)), *cr);
        z_img2 = Sleef_addq1_u05(Sleef_mulq1_u05(2.0Q, Sleef_mulq1_u05(z_real, z_img)), *ci);

        if (Sleef_icmpgtq1(get_abs_value(&z_real2, &z_img2), *RADIUS2)) 
        {
            Sleef_quad log_zn = Sleef_logq1_u10(get_abs_value(&z_real2, &z_img2)) / 2.0Q;
            Sleef_quad nu = Sleef_logq1_u10(log_zn / Sleef_logq1_u10(2.0Q)) / Sleef_logq1_u10(2.0Q);
            
            float smooth_iter = i + 1 - Sleef_cast_to_doubleq1(nu);
            return smooth_iter;
        }
        
        z_real = z_real2;
        z_img = z_img2;
    }
    return MAX_ITER;
}


static PyObject* mandelbrot_set(PyObject* self, PyObject* args) 
{
    int width, height, max_iter;
    double center_r, center_i, zoom;

    if (!PyArg_ParseTuple(args, "iiiddd", &width, &height, &max_iter, &center_r, &center_i, &zoom))
        return NULL;

    unsigned char* img = malloc(width * height * 3);
    if (!img) {
        PyErr_NoMemory();
        return NULL;
    }

    Sleef_quad RADIUS = 2.0Q;
    Sleef_quad RADIUS2 = Sleef_mulq1_u05(RADIUS, RADIUS);
    Sleef_quad zoom_q = Sleef_cast_from_doubleq1(1.0 / zoom);

    #pragma omp parallel for
    for (int y = 0; y < height; y++) 
    {
        for (int x = 0; x < width; x++) 
        {
            Sleef_quad cr = Sleef_mulq1_u05(Sleef_divq1_u05(Sleef_subq1_u05(Sleef_cast_from_int64q1(x), Sleef_cast_from_int64q1(width / 2)), Sleef_cast_from_int64q1(width / 2)), RADIUS);
            Sleef_quad ci = Sleef_mulq1_u05(Sleef_divq1_u05(Sleef_subq1_u05(Sleef_cast_from_int64q1(y), Sleef_cast_from_int64q1(height / 2)), Sleef_cast_from_int64q1(height / 2)), RADIUS);
            
            cr = Sleef_addq1_u05(Sleef_mulq1_u05(cr, zoom_q), Sleef_cast_from_doubleq1(center_r));
            ci = Sleef_addq1_u05(Sleef_mulq1_u05(ci, zoom_q), Sleef_cast_from_doubleq1(center_i));

            float smooth_iter = mandelbrot(&cr, &ci, max_iter, &RADIUS2);
            
            Color color;
            if (smooth_iter == max_iter) 
            {
                color.r = color.g = color.b = 0;
            } 
            else 
            {
                double t = smooth_iter / max_iter;
                color = get_color(t);
            }
            
            img[(y * width + x) * 3 + 0] = color.r;
            img[(y * width + x) * 3 + 1] = color.g;
            img[(y * width + x) * 3 + 2] = color.b;
        }
    }

    PyObject* result = PyBytes_FromStringAndSize((char*)img, width * height * 3);
    free(img);
    return result;
}

static PyMethodDef MandelbrotMethods[] = 
{
    {"mandelbrot_set", mandelbrot_set, METH_VARARGS, "Calculate Mandelbrot set"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef mandelbrotmodule = 
{
    PyModuleDef_HEAD_INIT,
    "mandelbrot",
    NULL,
    -1,
    MandelbrotMethods
};

PyMODINIT_FUNC PyInit_mandelbrot(void) 
{
    return PyModule_Create(&mandelbrotmodule);
}
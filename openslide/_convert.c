/*
 * openslide-python - Python bindings for the OpenSlide library
 *
 * Copyright (c) 2015 Carnegie Mellon University
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <Python.h>

typedef unsigned char u8;

static void
argb2float(PY_UINT32_T *buf, float *tensor, Py_ssize_t len)
{
    Py_ssize_t cur;
    for (cur = 0; cur < len; cur++) {
        PY_UINT32_T val = buf[cur];
        u8 a = val >> 24;
        switch (a) {
        case 0:
            tensor[cur * 3] = 1.0;
            tensor[cur * 3 + 1] = 1.0;
            tensor[cur * 3 + 2] = 1.0;
            break;
        default:
            tensor[cur * 3] = (float) ((val & 0x00ff0000) >> 16) / 255;
            tensor[cur * 3 + 1] = (float) ((val & 0x0000ff00) >> 8) / 255;
            tensor[cur * 3 + 2] = (float) ((val & 0x000000ff)) / 255;
            break;
        }
    }
}

static void
argb2rgba(PY_UINT32_T *buf, Py_ssize_t len)
{
    Py_ssize_t cur;

    for (cur = 0; cur < len; cur++) {
        PY_UINT32_T val = buf[cur];
        u8 a = val >> 24;
        switch (a) {
        case 0:
            buf[cur] = 0xffffffff;
            break;
        case 255:
            val = (val << 8) | a;
#ifndef WORDS_BIGENDIAN
            // compiler should optimize this to bswap
            val = (((val & 0x000000ff) << 24) |
                   ((val & 0x0000ff00) <<  8) |
                   ((val & 0x00ff0000) >>  8) |
                   ((val & 0xff000000) >> 24));
#endif
            buf[cur] = val;
            break;
        default:
            ; // label cannot point to a variable declaration
            u8 r = 255 * ((val >> 16) & 0xff) / a;
            u8 g = 255 * ((val >>  8) & 0xff) / a;
            u8 b = 255 * ((val >>  0) & 0xff) / a;
#ifdef WORDS_BIGENDIAN
            val = r << 24 | g << 16 | b << 8 | a;
#else
            val = a << 24 | b << 16 | g << 8 | r;
#endif
            buf[cur] = val;
            break;
        }
    }
}

// Takes two arguments: contiguous buffer objects, RGBA uint32 array and RGB float array.
// Modifies the latter in-place.
static PyObject *
_convert_argb2float(PyObject *self, PyObject *args)
{
    PyObject *ret = NULL;
    Py_buffer image_argb;
    Py_buffer image_rgb_float;

    if (!PyArg_ParseTuple(args, "s*s*", &image_argb, &image_rgb_float))
        return NULL;
    if (!PyBuffer_IsContiguous(&image_argb, 'A')) {
        PyErr_SetString(PyExc_ValueError, "Argument is not contiguous");
        goto DONE;
    }
    if (!PyBuffer_IsContiguous(&image_rgb_float, 'A')) {
        PyErr_SetString(PyExc_ValueError, "Argument is not contiguous");
        goto DONE;
    }
    if (image_rgb_float.readonly) {
        PyErr_SetString(PyExc_ValueError, "Argument is not writable");
        goto DONE;
    }
    if (image_argb.len % 4) {
        PyErr_SetString(PyExc_ValueError, "Argument has invalid size");
        goto DONE;
    }
    if (image_rgb_float.len % 3) {
        PyErr_SetString(PyExc_ValueError, "Argument has invalid size");
        goto DONE;
    }
    if (image_argb.itemsize != 4) {
        PyErr_SetString(PyExc_ValueError, "Argument has invalid item size");
        goto DONE;
    }

    Py_BEGIN_ALLOW_THREADS
    argb2float(image_argb.buf, image_rgb_float.buf, image_argb.len / 4);
    Py_END_ALLOW_THREADS

    Py_INCREF(Py_None);
    ret = Py_None;

DONE:
    PyBuffer_Release(&image_argb);
    PyBuffer_Release(&image_rgb_float);
    return ret;
}

// Takes one argument: a contiguous buffer object.  Modifies it in place.
static PyObject *
_convert_argb2rgba(PyObject *self, PyObject *args)
{
    PyObject *ret = NULL;
    Py_buffer view;

    if (!PyArg_ParseTuple(args, "s*", &view))
        return NULL;
    if (!PyBuffer_IsContiguous(&view, 'A')) {
        PyErr_SetString(PyExc_ValueError, "Argument is not contiguous");
        goto DONE;
    }
    if (view.readonly) {
        PyErr_SetString(PyExc_ValueError, "Argument is not writable");
        goto DONE;
    }
    if (view.len % 4) {
        PyErr_SetString(PyExc_ValueError, "Argument has invalid size");
        goto DONE;
    }
    if (view.itemsize != 4) {
        PyErr_SetString(PyExc_ValueError, "Argument has invalid item size");
        goto DONE;
    }

    Py_BEGIN_ALLOW_THREADS
    argb2rgba(view.buf, view.len / 4);
    Py_END_ALLOW_THREADS

    Py_INCREF(Py_None);
    ret = Py_None;

DONE:
    PyBuffer_Release(&view);
    return ret;
}

static PyMethodDef ConvertMethods[] = {
    {"argb2rgba", _convert_argb2rgba, METH_VARARGS,
        "Convert aRGB to RGBA in place."},
    {"argb2float", _convert_argb2float, METH_VARARGS,
        "Convert aRGB to RGB float array."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef convertmodule = {
    PyModuleDef_HEAD_INIT,
    "_convert",
    NULL,
    0,
    ConvertMethods
};

PyMODINIT_FUNC
PyInit__convert(void)
{
    return PyModule_Create(&convertmodule);
}

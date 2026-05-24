%{
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <Python.h>
#include <numpy/arrayobject.h>
#include <string.h>
#include <utility>

%}

%init %{
import_array();
%}



/* ================== unified float* typemap ================== */

%typemap(in) const float* {
    if (!PyArray_Check($input))
        SWIG_exception_fail(SWIG_TypeError, "Expected NumPy array");

    PyArrayObject* arr = (PyArrayObject*)$input;

    if (PyArray_TYPE(arr) != NPY_FLOAT32)
        SWIG_exception_fail(SWIG_TypeError, "Expected float32 array");

    if (!PyArray_IS_C_CONTIGUOUS(arr))
        SWIG_exception_fail(SWIG_TypeError, "Array must be C contiguous");

    /* Enforce writable only for OUTPUT buffers */
    if (strcmp("$name", "OUTPUT") == 0) {
        if (!PyArray_ISWRITEABLE(arr))
            SWIG_exception_fail(SWIG_TypeError, "Output array must be writable");
    }

    $1 = (float*)PyArray_DATA(arr);
}

%typemap(in) float* {
    if (!PyArray_Check($input))
        SWIG_exception_fail(SWIG_TypeError, "Expected NumPy array");

    PyArrayObject* arr = (PyArrayObject*)$input;

    if (PyArray_TYPE(arr) != NPY_FLOAT32)
        SWIG_exception_fail(SWIG_TypeError, "Expected float32 array");

    if (!PyArray_IS_C_CONTIGUOUS(arr))
        SWIG_exception_fail(SWIG_TypeError, "Array must be C contiguous");

    /* Enforce writable only for OUTPUT buffers */
    if (strcmp("$name", "OUTPUT") == 0) {
        if (!PyArray_ISWRITEABLE(arr))
            SWIG_exception_fail(SWIG_TypeError, "Output array must be writable");
    }

    $1 = (float*)PyArray_DATA(arr);
}

%typemap(out) float& {
    $result = PyFloat_FromDouble((double)*$1);
}

%typemap(out) double& {
    $result = PyFloat_FromDouble((double)*$1);
}

%typemap(out) (float out_real, float out_imag) {
    $result = PyComplex_FromDoubles($1, $2);
}


%typemap(in) const double* {
    if (!PyArray_Check($input))
        SWIG_exception_fail(SWIG_TypeError, "Expected NumPy array");

    PyArrayObject* arr = (PyArrayObject*)$input;

    if (PyArray_TYPE(arr) != NPY_FLOAT64)
        SWIG_exception_fail(SWIG_TypeError, "Expected double array");

    if (!PyArray_IS_C_CONTIGUOUS(arr))
        SWIG_exception_fail(SWIG_TypeError, "Array must be C contiguous");

    /* Enforce writable only for OUTPUT buffers */
    if (strcmp("$name", "OUTPUT") == 0) {
        if (!PyArray_ISWRITEABLE(arr))
            SWIG_exception_fail(SWIG_TypeError, "Output array must be writable");
    }

    $1 = (double*)PyArray_DATA(arr);
}

%typemap(in) double* {
    if (!PyArray_Check($input))
        SWIG_exception_fail(SWIG_TypeError, "Expected NumPy array");

    PyArrayObject* arr = (PyArrayObject*)$input;

    if (PyArray_TYPE(arr) != NPY_FLOAT64)
        SWIG_exception_fail(SWIG_TypeError, "Expected double array");

    if (!PyArray_IS_C_CONTIGUOUS(arr))
        SWIG_exception_fail(SWIG_TypeError, "Array must be C contiguous");

    /* Enforce writable only for OUTPUT buffers */
    if (strcmp("$name", "OUTPUT") == 0) {
        if (!PyArray_ISWRITEABLE(arr))
            SWIG_exception_fail(SWIG_TypeError, "Output array must be writable");
    }

    $1 = (double*)PyArray_DATA(arr);
}

/* ================== int* ================== */

%typemap(in) int* {
    if (!PyArray_Check($input))
        SWIG_exception_fail(SWIG_TypeError, "Expected NumPy array");

    PyArrayObject* arr = (PyArrayObject*)$input;

    if (PyArray_TYPE(arr) != NPY_INT32)
        SWIG_exception_fail(SWIG_TypeError, "Expected int32 array");

    if (!PyArray_IS_C_CONTIGUOUS(arr))
        SWIG_exception_fail(SWIG_TypeError, "Array must be C contiguous");

    /* Enforce writable only for OUTPUT buffers */
    if (strcmp("$name", "OUTPUT") == 0) {
        if (!PyArray_ISWRITEABLE(arr))
            SWIG_exception_fail(SWIG_TypeError, "Output array must be writable");
    }

    $1 = (int*)PyArray_DATA(arr);
}

/* ================== size_t ================== */

%typemap(in) size_t {
    $1 = (size_t)PyLong_AsUnsignedLong($input);
}

%typemap(in) size_t* {
    if (!PyArray_Check($input))
        SWIG_exception_fail(SWIG_TypeError, "Expected NumPy array");

    PyArrayObject* arr = (PyArrayObject*)$input;

    if (PyArray_TYPE(arr) != NPY_UINTP)
        SWIG_exception_fail(SWIG_TypeError, "Expected NumPy uintp array");

    if (!PyArray_IS_C_CONTIGUOUS(arr))
        SWIG_exception_fail(SWIG_TypeError, "Array must be C contiguous");

    if (!PyArray_ISWRITEABLE(arr))
        SWIG_exception_fail(SWIG_TypeError, "Index/output array must be writable");

    $1 = (size_t*)PyArray_DATA(arr);
}

%typemap(out) uint32_t {
    $result = PyLong_FromUnsignedLong((unsigned long)$1);
}

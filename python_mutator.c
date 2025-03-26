#include <Python.h>
#include <stdint.h>

typedef unsigned char (*common_fuzz_stuff_t)(char **, unsigned char *, unsigned int);

unsigned char has_inited = 0; // Have we inited the python environment??
PyObject *pModule = NULL; // Custom mutator module

void init_python(void) {
    Py_Initialize();
    PyObject* pName = PyUnicode_FromString("mutator");
    pModule = PyImport_Import(pName);
    Py_XDECREF(pName);

    if (!pModule) {
        memcpy(0, 00, 10);
    }
    has_inited = 1;
    return;
}

void spam_string(const char* string) {
    for (int i = 0; i < 100; i++) {
        fprintf(stderr, string);
    }
    return;
}

//unsigned char dll_mutate_testcase(char **argv, u8 *buf, u32 len, u8 (*common_fuzz_stuff)(char**, u8*, u32)) {
__declspec(dllexport) unsigned char dll_mutate_testcase(char** argv, unsigned char *buf, unsigned int buf_size, common_fuzz_stuff_t common_fuzz_stuff) {   // void* common_fuzz_stuff) { // common_fuzz_stuff is just some bullshit stuff. 
    char* out_buf = NULL; // Output buffer. Avoid uninitialized memory here.
    unsigned char bail = 0; // Init at zero
    unsigned int* out_buf_size;
    if (!has_inited) {
        init_python();
    }
    if (pModule == NULL) {
        memcpy(0, buf, 10);
    }

    PyObject *pFuzzFunc = PyObject_GetAttrString(pModule, "fuzz");
    PyObject* pResult;
    if (pFuzzFunc && PyCallable_Check(pFuzzFunc)) {
        for (int i = 0; i < 1000; i++) {
            out_buf = NULL;
            pResult = NULL;
            pResult = PyObject_CallFunction(pFuzzFunc, "(y#)", buf, buf_size);
            if (pResult) {
                char* new_buf;
                Py_ssize_t new_size;
                // Correct usage of PyBytes_AsStringAndSize
                if (PyBytes_AsStringAndSize(pResult, &new_buf, &new_size) == -1) {
                    PyErr_Print();
                    memcpy(0, buf, 10);
                    Py_XDECREF(pResult);
                    return 0;
                }

                out_buf = malloc(new_size);
                if (out_buf == NULL) {
                    Py_XDECREF(pResult);
                    memcpy(0, buf, 10);
                    return 0;
                }

                memcpy(out_buf, new_buf, new_size);
                if (common_fuzz_stuff(argv, out_buf, new_size)) {
                    bail = 1; // skip the rest of the mutation per common_fuzz_stuff
                    Py_DECREF(pResult);
                    break;
                }

                if (out_buf) {
                    free(out_buf);
                    out_buf == NULL; // Set to null pointer such that we do not crash in the later thing. If this is not here, then we may do a double free when the loop finally exits.
                }
                Py_DECREF(pResult);
                //return bail;
            }
        }
    }

    //Py_XDECREF(pResult);
    Py_XDECREF(pFuzzFunc);
    // Py_XDECREF(pModule);
    
    return bail;


}

// Cleanup
__declspec(dllexport) void afl_custom_deinit(void *data) {
    Py_Finalize();
}
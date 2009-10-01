#include "rpmsystem-py.h"

#include <rpm/rpmmacro.h>

#include "rpmmacro-py.h"

#include "debug.h"

PyObject *
rpmmacro_AddMacro(PyObject * self, PyObject * args, PyObject * kwds)
{
    char * name, * val;
    char * kwlist[] = {"name", "value", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss:AddMacro", kwlist,
	    &name, &val))
	return NULL;

    addMacro(NULL, name, NULL, val, -1);

    Py_RETURN_NONE;
}

PyObject *
rpmmacro_DelMacro(PyObject * self, PyObject * args, PyObject * kwds)
{
    char * name;
    char * kwlist[] = {"name", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s:DelMacro", kwlist, &name))
	return NULL;

    delMacro(NULL, name);

    Py_RETURN_NONE;
}

PyObject * 
rpmmacro_ExpandMacro(PyObject * self, PyObject * args, PyObject * kwds)
{
    char *macro;
    PyObject *res;
    int num = 0;
    char * kwlist[] = {"macro", "numeric", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|i", kwlist, &macro, &num))
        return NULL;

    if (num) {
	res = Py_BuildValue("i", rpmExpandNumeric(macro));
    } else {
	char *str = rpmExpand(macro, NULL);
	res = Py_BuildValue("s", str);
	free(str);
    }
    return res;
}


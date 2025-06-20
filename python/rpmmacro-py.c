#include "rpmsystem-py.h"

#include <rpm/rpmmacro.h>

#include "header-py.h"	/* XXX for pyrpmError, doh */
#include "rpmmacro-py.h"

extern rpmmodule_state_t *modstate;  // TODO: Remove

PyObject *
rpmmacro_AddMacro(PyObject * self, PyObject * args, PyObject * kwds)
{
    const char * name, * val;
    char * kwlist[] = {"name", "value", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss:AddMacro", kwlist,
	    &name, &val))
	return NULL;

    rpmPushMacro(NULL, name, NULL, val, -1);

    Py_RETURN_NONE;
}

PyObject *
rpmmacro_DelMacro(PyObject * self, PyObject * args, PyObject * kwds)
{
    const char * name;
    char * kwlist[] = {"name", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s:DelMacro", kwlist, &name))
	return NULL;

    rpmPopMacro(NULL, name);

    Py_RETURN_NONE;
}

PyObject * 
rpmmacro_ExpandMacro(PyObject * self, PyObject * args, PyObject * kwds)
{
    const char *macro;
    PyObject *res = NULL;
    int num = 0;
    char * kwlist[] = {"macro", "numeric", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|i", kwlist, &macro, &num))
        return NULL;

    if (num) {
	res = Py_BuildValue("i", rpmExpandNumeric(macro));
    } else {
	char *str = NULL;
	if (rpmExpandMacros(NULL, macro, &str, 0) < 0)
	    PyErr_SetString(modstate->pyrpmError, "error expanding macro");
	else
	    res = utf8FromString(str);
	free(str);
    }
    return res;
}


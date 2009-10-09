#include "rpmsystem-py.h"

#include "spec-py.h"

#include "debug.h"

static char rpmb__doc__[] =
"";

void init_rpmb(void);	/* XXX eliminate gcc warning */

void init_rpmb(void)
{
    PyObject * d, *m;

    if (PyType_Ready(&spec_Type) < 0) return;

    m = Py_InitModule3("_rpmb", NULL, rpmb__doc__);
    if (m == NULL)
	return;

    d = PyModule_GetDict(m);

    Py_INCREF(&spec_Type);
    PyModule_AddObject(m, "spec", (PyObject *) &spec_Type);

}


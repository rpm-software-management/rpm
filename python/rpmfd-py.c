
#include "rpmsystem-py.h"
#include "rpmfd-py.h"

int rpmFdFromPyObject(PyObject *obj, FD_t *fdp)
{
    FD_t fd = NULL;

    if (PyInt_Check(obj)) {
	fd = fdDup(PyInt_AsLong(obj));
    } else if (PyFile_Check(obj)) {
	FILE *fp = PyFile_AsFile(obj);
	fd = fdDup(fileno(fp));
    } else {
	PyErr_SetString(PyExc_TypeError, "integer or file object expected");
	return 0;
    }
    if (fd == NULL || Ferror(fd)) {
	PyErr_SetFromErrno(PyExc_IOError);
	Fclose(fd);
	return 0;
    }
    *fdp = fd;
    return 1;
}

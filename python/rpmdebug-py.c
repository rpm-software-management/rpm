
/*@unchecked@*/
extern PyTypeObject PyCode_Type;
/*@unchecked@*/
extern PyTypeObject PyDictIter_Type;
/*@unchecked@*/
extern PyTypeObject PyFrame_Type;

#include "rpmbc-py.h"   /* XXX debug only */
#include <rpmcli.h>	/* XXX debug only */

#include "header-py.h"	/* XXX debug only */
#include "rpmal-py.h"	/* XXX debug only */
#include "rpmds-py.h"	/* XXX debug only */
#include "rpmfd-py.h"	/* XXX debug only */
#include "rpmfi-py.h"	/* XXX debug only */
#include "rpmfts-py.h"	/* XXX debug only */
#include "rpmmi-py.h"	/* XXX debug only */
#include "rpmmpw-py.h"	/* XXX debug only */
#include "rpmrc-py.h"	/* XXX debug only */
#include "rpmrng-py.h"	/* XXX debug only */
#include "rpmte-py.h"	/* XXX debug only */
#include "rpmts-py.h"	/* XXX debug only */

/**
 */
static const char * lbl(void * s)
	/*@*/
{
    PyObject * o = s;

    if (o == NULL)	return "null";

    if (o->ob_type == &PyType_Type)	return o->ob_type->tp_name;

    if (o->ob_type == &PyBuffer_Type)	return "Buffer";
    if (o->ob_type == &PyCFunction_Type)	return "CFunction";
    if (o->ob_type == &PyCObject_Type)	return "CObject";
    if (o->ob_type == &PyCell_Type)	return "Cell";
    if (o->ob_type == &PyClass_Type)	return "Class";
    if (o->ob_type == &PyCode_Type)	return "Code";
    if (o->ob_type == &PyComplex_Type)	return "Complex";
    if (o->ob_type == &PyDict_Type)	return "Dict";
    if (o->ob_type == &PyDictIter_Type)	return "DictIter";
    if (o->ob_type == &PyFile_Type)	return "File";
    if (o->ob_type == &PyFloat_Type)	return "Float";
    if (o->ob_type == &PyFrame_Type)	return "Frame";
    if (o->ob_type == &PyFunction_Type)	return "Function";
    if (o->ob_type == &PyInstance_Type)	return "Instance";
    if (o->ob_type == &PyInt_Type)	return "Int";
    if (o->ob_type == &PyList_Type)	return "List";
    if (o->ob_type == &PyLong_Type)	return "Long";
    if (o->ob_type == &PyMethod_Type)	return "Method";
    if (o->ob_type == &PyModule_Type)	return "Module";
    if (o->ob_type == &PyRange_Type)	return "Range";
    if (o->ob_type == &PySeqIter_Type)	return "SeqIter";
    if (o->ob_type == &PySlice_Type)	return "Slice";
    if (o->ob_type == &PyString_Type)	return "String";
    if (o->ob_type == &PyTuple_Type)	return "Tuple";
    if (o->ob_type == &PyType_Type)	return "Type";
    if (o->ob_type == &PyUnicode_Type)	return "Unicode";

    if (o->ob_type == &hdr_Type)	return "hdr";
    if (o->ob_type == &mpw_Type)	return "mpw";
    if (o->ob_type == &rng_Type)	return "rng";
    if (o->ob_type == &rpmal_Type)	return "rpmal";
    if (o->ob_type == &rpmbc_Type)	return "rpmbc";
    if (o->ob_type == &rpmds_Type)	return "rpmds";
    if (o->ob_type == &rpmfd_Type)	return "rpmfd";
    if (o->ob_type == &rpmfi_Type)	return "rpmfi";
    if (o->ob_type == &rpmfts_Type)	return "rpmfts";
    if (o->ob_type == &rpmmi_Type)	return "rpmmi";
    if (o->ob_type == &rpmrc_Type)	return "rpmrc";
    if (o->ob_type == &rpmte_Type)	return "rpmte";
    if (o->ob_type == &rpmts_Type)	return "rpmts";

    return "Unknown";
}

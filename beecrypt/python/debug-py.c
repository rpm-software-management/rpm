
/*@unchecked@*/
extern PyTypeObject PyCode_Type;
/*@unchecked@*/
extern PyTypeObject PyDictIter_Type;
/*@unchecked@*/
extern PyTypeObject PyFrame_Type;

#include "mpw-py.h"	/* XXX debug only */
#include "rng-py.h"	/* XXX debug only */

/**
 */
static const char * lbl(void * s)
	/*@*/
{
    PyObject * o = s;

    if (o == NULL)	return "null";

    if (o == Py_None)	return "None";

    if (o->ob_type == &PyType_Type)	return o->ob_type->tp_name;

    if (o->ob_type == &PyBaseObject_Type)	return "BaseObj";
    if (o->ob_type == &PyBuffer_Type)	return "Buffer";
    if (o->ob_type == &PyCFunction_Type)	return "CFunction";
    if (o->ob_type == &PyCObject_Type)	return "CObject";
    if (o->ob_type == &PyCell_Type)	return "Cell";
    if (o->ob_type == &PyClass_Type)	return "Class";
    if (o->ob_type == &PyClassMethod_Type)	return "ClassMethod";
    if (o->ob_type == &PyStaticMethod_Type)	return "StaticMethod";
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
    if (o->ob_type == &PyWrapperDescr_Type)	return "WrapperDescr";
    if (o->ob_type == &PyProperty_Type)	return "Property";
    if (o->ob_type == &PyModule_Type)	return "Module";
    if (o->ob_type == &PyRange_Type)	return "Range";
    if (o->ob_type == &PySeqIter_Type)	return "SeqIter";
    if (o->ob_type == &PyCallIter_Type)	return "CallIter";
    if (o->ob_type == &PySlice_Type)	return "Slice";
    if (o->ob_type == &PyString_Type)	return "String";
    if (o->ob_type == &PySuper_Type)	return "Super";
    if (o->ob_type == &PyTuple_Type)	return "Tuple";
    if (o->ob_type == &PyType_Type)	return "Type";
    if (o->ob_type == &PyUnicode_Type)	return "Unicode";

    if (o->ob_type == &mpw_Type)	return "mpw";
    if (o->ob_type == &rng_Type)	return "rng";

    return "Unknown";
}

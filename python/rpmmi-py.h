#ifndef H_RPMMI_PY
#define H_RPMMI_PY

/** \ingroup py_c
 * \file python/rpmmi-py.h
 */

/** \ingroup py_c
 */
typedef struct rpmmiObject_s rpmmiObject;

extern PyTypeObject rpmmi_Type;

rpmmiObject * rpmmi_Wrap(rpmdbMatchIterator mi, PyObject *s);

#endif

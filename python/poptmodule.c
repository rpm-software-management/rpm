/*
 *
 * 
 * 
 */

#define PY_POPT_VERSION "0.2"

static const char *rcs_id = "$Id: poptmodule.c,v 1.4 2001/07/21 19:44:22 jbj Exp $";

static char *module_doc = "Python bindings for the popt library\n\
\n\
The popt library provides useful command-line parsing functions.\n\
The latest version of the popt library is distributed with rpm\n\
and is always available from ftp://ftp.rpm.org/pub/rpm/dist";

#include <Python.h>
#include <popt.h>

#define DEBUG 1

#if defined(DEBUG)
#define debug(x, y) { printf("%s: %s\n", x, y); }
#else
#define debug(x, y) {}
#endif 

/* Functins and datatypes needed for the context object */
typedef struct poptContext_s {
    PyObject_HEAD;
    struct poptOption *options;
    int optionsNo;
    poptContext ctx;
    /* The index of the option retrieved with getNextOpt()*/
    int opt;
} poptContextObject;

/* The exception */
static PyObject *pypoptError;

/* Misc functions */
void __printPopt(struct poptOption *opts)
{
    printf("+++++++++++\n");
    printf("Long name: %s\n", opts->longName);
    printf("Short name: %c\n", opts->shortName);
    printf("argInfo: %d\n", opts->argInfo);
    printf("Val: %d\n", opts->val);
    printf("-----------\n");
}

static PyObject * __poptOptionValue2PyObject(const struct poptOption *option)
{
    if (option == NULL) {
        /* This shouldn't really happen */
        PyErr_BadInternalCall();
        return NULL;
    }
    if (option->arg == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    switch(option->argInfo) {
        case POPT_ARG_INCLUDE_TABLE:
            /* Do nothing */
            Py_INCREF(Py_None);
            return Py_None;
        case POPT_ARG_STRING: 
            if (*(char **)(option->arg) == NULL) {
                Py_INCREF(Py_None);
                return Py_None;
            }
            return PyString_FromString(*(char **)(option->arg));
            break;
        case POPT_ARG_DOUBLE:
            return PyFloat_FromDouble(*(double *)(option->arg));
            break;
        case POPT_ARG_LONG:
            return PyInt_FromLong(*(long *)(option->arg));
            break;
        case POPT_ARG_NONE:
        case POPT_ARG_VAL:
            return PyInt_FromLong(*(int *)(option->arg));
            break;
    }
    /* This shouldn't really happen */
    PyErr_BadInternalCall();
    return NULL;
}

static PyObject * ctxReset(poptContextObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    poptResetContext(self->ctx);
    self->opt = -1;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * ctxGetNextOpt(poptContextObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    self->opt = poptGetNextOpt(self->ctx);
    return PyInt_FromLong(self->opt);
}

static PyObject * ctxGetOptArg(poptContextObject *self, PyObject *args)
{
    const char *opt;
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    opt = poptGetOptArg(self->ctx);
    if (opt == NULL) {
	Py_INCREF(Py_None);
	return Py_None;
    }
    return PyString_FromString(opt);
}

static PyObject * ctxGetArg(poptContextObject *self, PyObject *args)
{
    const char *arg;
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    arg = poptGetArg(self->ctx);
    if (arg == NULL) {
	Py_INCREF(Py_None);
	return Py_None;
    }
    return PyString_FromString(arg);
}

static PyObject * ctxPeekArg(poptContextObject *self, PyObject *args)
{
    const char *arg;
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    arg = poptPeekArg(self->ctx);
    if (arg == NULL) {
	Py_INCREF(Py_None);
	return Py_None;
    }
    return PyString_FromString(arg);
}

static PyObject * ctxGetArgs(poptContextObject *self, PyObject *argsFoo)
{
    const char **args;
    PyObject *list;
    int size, i;
    if (!PyArg_ParseTuple(argsFoo, ""))
        return NULL;
    args = poptGetArgs(self->ctx);
    if (args == NULL) {
	Py_INCREF(Py_None);
	return Py_None;
    }
    /* Compute the list size */
    for (size = 0; args[size]; size++);
    /* Create the list */
    list = PyList_New(size);
    if (list == NULL)
	return NULL;
    for (i = 0; i < size; i++) 
	PyList_SetItem(list, i, PyString_FromString(args[i]));
    return list;
}

static PyObject * ctxBadOption(poptContextObject *self, PyObject *args)
{
    int flags = 0;
    const char *badOption;
    if (!PyArg_ParseTuple(args, "|i", &flags)) 
	return NULL;
    badOption = poptBadOption(self->ctx, flags);
    if (badOption == NULL) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    return PyString_FromString(badOption);
}

static PyObject * ctxReadDefaultConfig(poptContextObject *self, PyObject *args)
{
    int flags = 0;
    if (!PyArg_ParseTuple(args, "|i", &flags)) 
	return NULL;
    return PyInt_FromLong(poptReadDefaultConfig(self->ctx, flags));
}

static PyObject * ctxReadConfigFile(poptContextObject *self, PyObject *args)
{
    const char *filename;
    if (!PyArg_ParseTuple(args, "s", &filename))
	return NULL;
    return PyInt_FromLong(poptReadConfigFile(self->ctx, filename));
}

static PyObject * ctxSetOtherOptionHelp(poptContextObject *self, PyObject *args)
{
    const char *option;
    if (!PyArg_ParseTuple(args, "s", &option)) 
	return NULL;
    poptSetOtherOptionHelp(self->ctx, option);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * ctxPrintHelp(poptContextObject *self, PyObject *args)
{
    FILE *f;
    int flags = 0;
    PyObject *file;
    if (!PyArg_ParseTuple(args, "|O!i", &PyFile_Type, &file, &flags)) 
	return NULL;
    f = PyFile_AsFile(file);
    if (f == NULL)
        f = stderr;
    poptPrintHelp(self->ctx, f, flags);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * ctxPrintUsage(poptContextObject *self, PyObject *args)
{
    FILE *f;
    int flags = 0;
    PyObject *file;
    if (!PyArg_ParseTuple(args, "|O!i", &PyFile_Type, &file, &flags))
	return NULL;
        f = PyFile_AsFile(file);
    if (f == NULL)
        f = stderr;
    poptPrintUsage(self->ctx, f, flags);
    Py_INCREF(Py_None);
    return Py_None;
}

/* XXX addAlias */
/* XXX stuffArgs */
/* XXX callbackType */

/*******************************/
/* Added ctxGetOptValues */
/*******************************/
/* Builds a list of values corresponding to each option */
static PyObject * ctxGetOptValues(poptContextObject *self, PyObject *args)
{
    PyObject *list;
    int i;
    if (!PyArg_ParseTuple(args, ""))
	return NULL;
    /* Create the list */
    list = PyList_New(self->optionsNo);
    if (list == NULL) 
	return NULL;
    for (i = 0; i < self->optionsNo; i++) {
        PyObject *item;
        item = __poptOptionValue2PyObject(self->options + i);
        item = __poptOptionValue2PyObject(self->options + i);
        if (item == NULL)
            return NULL;
        PyList_SetItem(list, i, item);
    }
    return list;
}

static PyObject * ctxGetOptValue(poptContextObject *self, PyObject *args)
{
    int i;
    if (!PyArg_ParseTuple(args, ""))
	return NULL;
    if (self->opt < 0) {
        /* No processing */
        Py_INCREF(Py_None);
        return Py_None;
    }
    /* Look for the option that returned this value */
    for (i = 0; i < self->optionsNo; i++)
        if (self->options[i].val == self->opt) {
            /* Cool, this is the one */
            return __poptOptionValue2PyObject(self->options + i);
        }
    /* Not found */
    Py_INCREF(Py_None);
    return Py_None;
}

static struct PyMethodDef ctxMethods[] = {
    {"reset", (PyCFunction)ctxReset, METH_VARARGS},
    {"getNextOpt", (PyCFunction)ctxGetNextOpt, METH_VARARGS},
    {"getOptArg", (PyCFunction)ctxGetOptArg, METH_VARARGS},
    {"getArg", (PyCFunction)ctxGetArg, METH_VARARGS},
    {"peekArg", (PyCFunction)ctxPeekArg, METH_VARARGS},
    {"getArgs", (PyCFunction)ctxGetArgs, METH_VARARGS},
    {"badOption", (PyCFunction)ctxBadOption, METH_VARARGS},
    {"readDefaultConfig", (PyCFunction)ctxReadDefaultConfig, METH_VARARGS},
    {"readConfigFile", (PyCFunction)ctxReadConfigFile, METH_VARARGS},
    {"setOtherOptionHelp", (PyCFunction)ctxSetOtherOptionHelp, METH_VARARGS},
    {"printHelp", (PyCFunction)ctxPrintHelp, METH_VARARGS},
    {"printUsage", (PyCFunction)ctxPrintUsage, METH_VARARGS},
    /*
    {"addAlias", (PyCFunction)ctxAddAlias},
    {"stuffArgs", (PyCFunction)ctxStuffArgs},
    {"callbackType", (PyCFunction)ctxCallbackType},
    */
    {"getOptValues", (PyCFunction)ctxGetOptValues, METH_VARARGS},
    {"getOptValue", (PyCFunction)ctxGetOptValue, METH_VARARGS},
    {NULL, NULL}
};

static PyObject * ctxGetAttr(poptContextObject *s, char *name)
{
    return Py_FindMethod(ctxMethods, (PyObject *)s, name);
}

static void ctxDealloc(poptContextObject *self, PyObject *args)
{
    if (self->options != NULL) {
        int i;
	for (i = 0; i < self->optionsNo; i++) {
            struct poptOption *o = self->options + i;
	    if (o->argInfo != POPT_ARG_INCLUDE_TABLE && o->arg)
		free(o->arg);
        }
	free(self->options);
        self->options = NULL;
    }
    poptFreeContext(self->ctx);
    PyMem_DEL(self);
}

static PyTypeObject poptContextType = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,                                  /* ob_size */
    "poptContext",                      /* tp_name */
    sizeof(poptContextObject),          /* tp_basicsize */
    0,                                  /* tp_itemsize */
    (destructor)ctxDealloc,             /* tp_dealloc */
    (printfunc)NULL,                    /* tp_print */
    (getattrfunc)ctxGetAttr,            /* tp_getattr */
    (setattrfunc)NULL,                  /* tp_setattr */
    (cmpfunc)NULL,                      /* tp_compare */
    (reprfunc)NULL,                     /* tp_repr */
    NULL,                               /* tp_as_number */
    NULL,                               /* tp_as_sequence */
    NULL                                /* tp_as_mapping */
};

/* Functions and datatypes needed for the popt module */

#define AUTOHELP "autohelp"
static const struct poptOption __autohelp[] = {
    POPT_AUTOHELP
    POPT_TABLEEND
};

/* Misc functions */
int __setPoptOption(PyObject *list, struct poptOption *opt)
{
    int listSize;
    PyObject *o;
    const char *s;
    int objsize;
    /* Look for autohelp stuff first */
    if (PyString_Check(list)) {
        if (!strcmp(AUTOHELP, PyString_AsString(list))) {
            /* Autohelp */
            *opt = __autohelp[0];
            return 1;
        }
        PyErr_SetString(pypoptError, "Expected list or autohelp");
        return 0;
    }
    if (!PyList_Check(list)) {
        PyErr_SetString(pypoptError, "List expected");
        return 0;
    }
    listSize = PyList_Size(list);
    if (listSize < 3) {
        PyErr_SetString(pypoptError, "List is too short");
        return 0;
    }
    
    /* longName */
    o = PyList_GetItem(list, 0);
    /* Sanity check */
    if (o == Py_None)
        opt->longName = NULL;
    else {
        if (!PyString_Check(o)) {
            PyErr_SetString(pypoptError, "Long name should be a string");
            return 0;
        } 
        opt->longName = PyString_AsString(o);
    }
    
    /* shortName */
    o = PyList_GetItem(list, 1);
    /* Sanity check */
    if (o == Py_None)
        opt->shortName = '\0';
    else {
        if (!PyString_Check(o)) {
            PyErr_SetString(pypoptError, "Short name should be a string");
            return 0;
        }
        s = PyString_AsString(o);
        /* If s is the empty string, we set the short name to '\0', which is
         * the expected behaviour */
        opt->shortName = s[0];
    }

    /* Make sure they have specified at least one of the long name or short
     * name; we don't allow for table inclusions and callbacks for now, even
     * if it would be nice to have them. The table inclusion is broken anyway
     * unless I find a good way to pass the second table as a reference; I
     * would normally have to pass it thru the arg field, but I don't pass
     * that in the python table */
    if (opt->longName == NULL && opt->shortName == '\0') {
        PyErr_SetString(pypoptError, "At least one of the short name and long name must be specified");
        return 0;
    }
    
    /* argInfo */
    o = PyList_GetItem(list, 2);
    /* Sanity check */
    if (!PyInt_Check(o)) {
        PyErr_SetString(pypoptError, "argInfo is not an int");
        return 0;
    }
    opt->argInfo = PyInt_AsLong(o);
    
    /* Initialize the rest of the arguments with safe defaults */
    switch(opt->argInfo) {
	case POPT_ARG_STRING:
	    objsize = sizeof(char *);
	    break;
	case POPT_ARG_DOUBLE:
	    objsize = sizeof(double);
	    break;
	case POPT_ARG_NONE:
	case POPT_ARG_VAL:
	    objsize = sizeof(int);
            break;
	case POPT_ARG_LONG:
	    objsize = sizeof(long);
	    break;
	default:
            PyErr_SetString(pypoptError, "Wrong value for argInfo");
	    return 0;
    }
    opt->arg = (void *)malloc(objsize);
    if (opt->arg == NULL) {
        PyErr_NoMemory();
	return 0;
    }
    memset(opt->arg, '\0', objsize);
    opt->val = 0;
    opt->descrip = NULL;
    opt->argDescrip = NULL;
    /* If nothing left, end the stuff here */
    if (listSize == 3)
        return 1;
    
    /* val */
    o = PyList_GetItem(list, 3);
    /* Sanity check */
    if (o == Py_None)
        opt->val = 0;
    else {
        if (!PyInt_Check(o)) {
            PyErr_SetString(pypoptError, "Val should be int or None");
            return 0;
        }
        opt->val = PyInt_AsLong(o);
    }
    /* If nothing left, end the stuff here */
    if (listSize == 4)
        return 1;

    /* descrip */
    o = PyList_GetItem(list, 4);
    /* Sanity check */
    if (!PyString_Check(o) && o != Py_None) {
        PyErr_SetString(pypoptError, "Invalid value passed for the description");
        return 0;
    }
    if (o == Py_None)
        opt->descrip = NULL;
    else
        opt->descrip = PyString_AsString(o);
    /* If nothing left, end the stuff here */
    if (listSize == 5)
        return 1;

    /* argDescrip */
    o = PyList_GetItem(list, 5);
    /* Sanity check */
    if (!PyString_Check(o) && o != Py_None) {
        PyErr_SetString(pypoptError, "Invalid value passed for the argument description");
        return 0;
    }
    if (o == Py_None)
        opt->argDescrip = NULL;
    else
        opt->argDescrip = PyString_AsString(o);
    return 1;
}

struct poptOption * __getPoptOptions(PyObject *list, int *count)
{
    int listSize, item, totalmem;
    struct poptOption *opts;
    struct poptOption sentinel = POPT_TABLEEND;
    if (!PyList_Check(list)) {
        PyErr_SetString(pypoptError, "List expected");
        return NULL;
    }
    listSize = PyList_Size(list);
    /* Malloc exactly the size of the list */
    totalmem = (1 + listSize) * sizeof(struct poptOption);
    opts = (struct poptOption *)malloc(totalmem);
    if (opts == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    memset(opts, '\0', totalmem);
    for (item = 0; item < listSize; item++) {
	int ret;
        /* Retrieve the item */
        PyObject *o = PyList_GetItem(list, item);
        ret = __setPoptOption(o, opts + item);
	if (ret == 0) {
	    /* Presumably we pass the error from the previous level */
	    return NULL;
	}
        //__printPopt(opts + item);
    }
    /* Sentinel */
    opts[listSize] = sentinel;
    *count = listSize;
    return opts;
}

char ** __getArgv(PyObject *list, int *argc)
{
    int listSize, item, totalmem;
    char **argv;
    listSize = PyList_Size(list);
    /* Malloc exactly the size of the list */
    totalmem = (1 + listSize) * sizeof(char *);
    argv = (char **)malloc(totalmem);
    if (argv == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    memset(argv, '\0', totalmem);
    for (item = 0; item < listSize; item++) {
        /* Retrieve the item */
        PyObject *o = PyList_GetItem(list, item);
        if (!PyString_Check(o)) {
            PyErr_SetString(pypoptError, "Expected a string as value for the argument");
            return NULL;
        }
        argv[item] = PyString_AsString(o);
        //debug("getArgv", argv[item]);
    }
    /* Sentinel */
    argv[listSize] = NULL;
    *argc = listSize;
    return argv;
}


static PyObject * getContext(PyObject *self, PyObject *args)
{
    const char *name;
    PyObject *a, *o;
    char **argv;
    int argc, count, flags = 0;
    struct poptOption *opts;
    poptContextObject *c;
    /* We should receive name, argv and a list */
    if (!PyArg_ParseTuple(args, "zO!O!|i", &name, &PyList_Type, &a, 
        &PyList_Type, &o, &flags))
        return NULL;
    /* Parse argv */
    argv = __getArgv(a, &argc);
    if (argv == NULL)
        return NULL;
    /* Parse argv */
    /* Parse opts */
    opts = __getPoptOptions(o, &count);
    if (opts == NULL)
	/* Presumably they've set the exception at a previous level */
	return NULL;
    /* Parse argv */
    c = PyObject_NEW(poptContextObject, &poptContextType);
    c->options = opts;
    c->optionsNo = count;
    c->opt = -1;
    c->ctx = poptGetContext(name, argc, (const char **)argv, opts, flags);
    return (PyObject *)c;
}

struct _pyIntConstant {
    char *name;
    const int value;
};

static PyObject * _strerror(PyObject *self, PyObject *args)
{
    int error;
    if (!PyArg_ParseTuple(args, "i", &error)) {
	return NULL;
    }
    return PyString_FromString(poptStrerror(error));
}

/* Methods for the popt module */
static struct PyMethodDef poptModuleMethods[] = {
    {"getContext", (PyCFunction)getContext},
    {"strerror", (PyCFunction)_strerror},
    {NULL, NULL}
};

#define ADD_INT(NAME) {#NAME, NAME}
static const struct _pyIntConstant intConstants[] = {
    /* Arg info */
    ADD_INT(POPT_ARG_NONE),
    ADD_INT(POPT_ARG_STRING),
    {"POPT_ARG_INT", POPT_ARG_LONG},
    ADD_INT(POPT_ARG_VAL),
    {"POPT_ARG_FLOAT", POPT_ARG_DOUBLE},

    ADD_INT(POPT_ARGFLAG_OR),
    ADD_INT(POPT_ARGFLAG_AND),
    ADD_INT(POPT_ARGFLAG_XOR),
    ADD_INT(POPT_ARGFLAG_NOT),
    ADD_INT(POPT_ARGFLAG_ONEDASH),
    ADD_INT(POPT_ARGFLAG_DOC_HIDDEN),
    ADD_INT(POPT_ARGFLAG_OPTIONAL),
    /* Context flags*/
    ADD_INT(POPT_CONTEXT_NO_EXEC),
    ADD_INT(POPT_CONTEXT_KEEP_FIRST),
    ADD_INT(POPT_CONTEXT_POSIXMEHARDER),
    /* Errors */
    ADD_INT(POPT_ERROR_NOARG),
    ADD_INT(POPT_ERROR_BADOPT),
    ADD_INT(POPT_ERROR_OPTSTOODEEP),
    ADD_INT(POPT_ERROR_BADQUOTE),
    ADD_INT(POPT_ERROR_BADNUMBER),
    ADD_INT(POPT_ERROR_OVERFLOW),
    ADD_INT(POPT_ERROR_ERRNO),
    /* Misc */
    ADD_INT(POPT_BADOPTION_NOALIAS),
    {NULL}
};

void initpopt()
{
    PyObject *dict, *module;
    const struct _pyIntConstant *c;
    module = Py_InitModule3("popt", poptModuleMethods, module_doc);
    /* Init the constants */
    dict = PyModule_GetDict(module);
    PyDict_SetItemString(dict, "__version__", 
        PyString_FromString(PY_POPT_VERSION));
    PyDict_SetItemString(dict, "cvsid", PyString_FromString(rcs_id));
    for (c = intConstants; c->name; c++) {
        PyObject *val = PyInt_FromLong(c->value);
        PyDict_SetItemString(dict, c->name, val);
        Py_DECREF(val);
    }
    /* Add the autohelp stuff */
    {
        PyObject *val = PyString_FromString(AUTOHELP);
        PyDict_SetItemString(dict, "POPT_AUTOHELP", val);
        Py_DECREF(val);
    }
    /* The exception */
    pypoptError = PyErr_NewException("popt.error", NULL, NULL);
    PyDict_SetItemString(dict, "error", pypoptError);
}


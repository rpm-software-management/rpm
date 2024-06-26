#include "system.h"

#include <map>
#include <string>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "rpmhook.h"

#include "debug.h"

struct rpmhookItem {
    rpmhookFunc func;
    void *data;
};

using rpmhookTable = std::multimap<std::string,rpmhookItem>;

rpmhookArgs rpmhookArgsNew(int argc)
{
    rpmhookArgs args = new rpmhookArgs_s {};
    args->argv.resize(argc);
    args->argc = argc;
    return args;
}

rpmhookArgs rpmhookArgsFree(rpmhookArgs args)
{
    if (args != NULL)
	delete args;
    return NULL;
}

static void rpmhookTableAddItem(rpmhookTable *table, const char *name,
				rpmhookFunc func, void *data)
{
    table->insert({name, {func, data}});
}

static void rpmhookTableDelItem(rpmhookTable *table, const char *name,
		rpmhookFunc func, void *data,
		int matchfunc, int matchdata)
{
    auto range = table->equal_range(name);
    auto it = range.first;
    while (it != range.second) {
	const rpmhookItem & item = it->second;
	if ((!matchfunc || item.func == func) &&
	    (!matchdata || item.data == data))
	{
	    it = table->erase(it);
	} else {
	    ++it;
	}
    }
}

static rpmhookArgs rpmhookArgsParse(const char *argt, va_list ap)
{
    rpmhookArgs args = rpmhookArgsNew(strlen(argt));
    int i;

    args->argt = argt;
    for (i = 0; i != args->argc; i++) {
	switch (argt[i]) {
	    case 's':
		args->argv[i].s = va_arg(ap, char *);
		break;
	    case 'i':
		args->argv[i].i = va_arg(ap, int);
		break;
	    case 'f':
		args->argv[i].f = va_arg(ap, double);
		break;
	    case 'p':
		args->argv[i].p = va_arg(ap, void *);
		break;
	    default:
		fprintf(stderr, "error: unsupported type '%c' as "
				"a hook argument\n", argt[i]);
		break;
	}
    }
    return args;
}

static void rpmhookTableCallArgs(rpmhookTable *table, const char *name,
			rpmhookArgs args)
{
    auto range = table->equal_range(name);
    for (auto it = range.first; it != range.second; ++it) {
	const rpmhookItem & item = it->second;
	if (item.func(args, item.data) != 0)
	    break;
    }
}

static rpmhookTable *globalTable = NULL;

void rpmhookRegister(const char *name, rpmhookFunc func, void *data)
{
    if (globalTable == NULL)
	globalTable = new rpmhookTable {};
    rpmhookTableAddItem(globalTable, name, func, data);
}

void rpmhookUnregister(const char *name, rpmhookFunc func, void *data)
{
    if (globalTable != NULL)
	rpmhookTableDelItem(globalTable, name, func, data, 1, 1);
}

void rpmhookUnregisterAny(const char *name, rpmhookFunc func)
{
    if (globalTable != NULL)
	rpmhookTableDelItem(globalTable, name, func, NULL, 1, 0);
}

void rpmhookUnregisterAll(const char *name)
{
    if (globalTable != NULL)
	rpmhookTableDelItem(globalTable, name, NULL, NULL, 0, 0);
}

void rpmhookCall(const char *name, const char *argt, ...)
{
    if (globalTable != NULL) {
	rpmhookArgs args;
	va_list ap;
	va_start(ap, argt);
	args = rpmhookArgsParse(argt, ap);
	rpmhookTableCallArgs(globalTable, name, args);
	(void) rpmhookArgsFree(args);
	va_end(ap);
    }
}

void rpmhookCallArgs(const char *name, rpmhookArgs args)
{
    if (globalTable != NULL)
	rpmhookTableCallArgs(globalTable, name, args);
}

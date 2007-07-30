/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999,2007 Oracle.  All rights reserved.
 *
 * $Id: tcl_util.c,v 12.8 2007/05/17 15:15:54 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"
#ifdef HAVE_SYSTEM_INCLUDE_FILES
#include <tcl.h>
#endif
#include "dbinc/tcl_db.h"

/*
 * bdb_RandCommand --
 *	Implements rand* functions.
 *
 * PUBLIC: int bdb_RandCommand __P((Tcl_Interp *, int, Tcl_Obj * CONST*));
 */
int
bdb_RandCommand(interp, objc, objv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	static const char *rcmds[] = {
		"rand",	"random_int",	"srand",
		NULL
	};
	enum rcmds {
		RRAND, RRAND_INT, RSRAND
	};
	Tcl_Obj *res;
	int cmdindex, hi, lo, result, ret;

	result = TCL_OK;
	/*
	 * Get the command name index from the object based on the cmds
	 * defined above.  This SHOULD NOT fail because we already checked
	 * in the 'berkdb' command.
	 */
	if (Tcl_GetIndexFromObj(interp,
	    objv[1], rcmds, "command", TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));

	res = NULL;
	switch ((enum rcmds)cmdindex) {
	case RRAND:
		/*
		 * Must be 0 args.  Error if different.
		 */
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		ret = rand();
		res = Tcl_NewIntObj(ret);
		break;
	case RRAND_INT:
		/*
		 * Must be 4 args.  Error if different.
		 */
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "lo hi");
			return (TCL_ERROR);
		}
		if ((result =
		    Tcl_GetIntFromObj(interp, objv[2], &lo)) != TCL_OK)
			return (result);
		if ((result =
		    Tcl_GetIntFromObj(interp, objv[3], &hi)) != TCL_OK)
			return (result);
		if (lo < 0 || hi < 0) {
			Tcl_SetResult(interp,
			    "Range value less than 0", TCL_STATIC);
			return (TCL_ERROR);
		}

		_debug_check();
		ret = lo + rand() % ((hi - lo) + 1);
		res = Tcl_NewIntObj(ret);
		break;
	case RSRAND:
		/*
		 * Must be 1 arg.  Error if different.
		 */
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "seed");
			return (TCL_ERROR);
		}
		if ((result =
		    Tcl_GetIntFromObj(interp, objv[2], &lo)) == TCL_OK) {
			srand((u_int)lo);
			res = Tcl_NewIntObj(0);
		}
		break;
	}

	/*
	 * Only set result if we have a res.  Otherwise, lower functions have
	 * already done so.
	 */
	if (result == TCL_OK && res)
		Tcl_SetObjResult(interp, res);
	return (result);
}

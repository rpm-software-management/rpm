
dnl CHECK_READLINE checks for presence of readline on the
dnl system.

AC_DEFUN([CHECK_READLINE], [

AC_ARG_ENABLE(readline,
	[  --disable-readline      disable readline support ],
	[use_readline=$enableval],
	[use_readline=yes])  dnl Defaults to ON (if found)

if test "$use_readline" = "yes"; then
	AC_CHECK_LIB(curses, tputs, LIBS="$LIBS -lcurses",
		AC_CHECK_LIB(ncurses, tputs))
	AC_CHECK_LIB(readline, readline)

	AC_SEARCH_LIBS(add_history, history,
		AC_DEFINE(HAVE_ADD_HISTORY, 1, [Define if you have the add_history function])
	)

	AC_CHECK_HEADERS(history.h readline/history.h readline.h readline/readline.h)

	AC_MSG_CHECKING(whether filename_completion_function is present)

	AC_TRY_LINK([
/* need stdio.h since readline.h uses FILE * */
#include <stdio.h>

#if defined(HAVE_READLINE_H)
#include <readline.h>
#elif defined(HAVE_READLINE_READLINE_H)
#include <readline/readline.h>
#endif
],
[void (*foo)() = filename_completion_function;],
	AC_DEFINE(HAVE_FILENAME_COMPLETION_FUNCTION, 1,
		[Define if you have filename_completion_function in readline])
	AC_MSG_RESULT(yes), 
	AC_MSG_RESULT(no))
	
	msg_readline="enabled"
else
	msg_readline="disabled"
fi

])


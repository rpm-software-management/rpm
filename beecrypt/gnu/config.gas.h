#ifndef _CONFIG_GAS_H
#define _CONFIG_GAS_H

#include <gnu/config.gnu.h>

#ifndef C_FUNCTION_NAME
# if LEADING_UNDERSCORE
#  ifdef __STDC__
#   define C_FUNCTION_NAME(name)	_##name
#  else
#   define C_FUNCTION_NAME(name)	_/**/name
#  endif
# else
#  define C_FUNCTION_NAME(name)	name
# endif
#endif

#if defined(alpha)
# define ALIGNMENT	5
#elif defined(i386) || defined(i486) || defined(i586) || defined(i686) 
# define ALIGNMENT	8
#elif defined(ia64)
# define ALIGNMENT	32
#elif defined(powerpc)
# define ALIGNMENT	8
#elif defined(sparcv8plus) || defined(sparcv9)
# define ALIGNMENT	8
#else
# define ALIGNMENT	8
#endif

#if CYGWIN
# define C_FUNCTION(name)	\
	.align	ALIGNMENT;	\
	.globl	C_FUNCTION_NAME(name);	\
	.def	C_FUNCTION_NAME(name);	\
	.scl	2;	\
	.type	32;	\
	.endef;	\
C_FUNCTION_NAME(name):
#else
# define C_FUNCTION(name)			\
	.align	ALIGNMENT;				\
	.global	C_FUNCTION_NAME(name);	\
	.type	C_FUNCTION_NAME(name),@function;	\
C_FUNCTION_NAME(name):
#endif

#endif

#ifndef _CONFIG_GAS_H
#define _CONFIG_GAS_H

#include "config.h"

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

#if defined(OPTIMIZE_ALPHA)
# define ALIGNMENT	5
#elif defined(OPTIMIZE_I386) || defined(OPTIMIZE_I486) || defined(OPTIMIZE_I586) || defined(OPTIMIZE_I686) 
# define ALIGNMENT	8
#elif defined(OPTIMIZE_IA64)
# define ALIGNMENT	32
#elif defined(OPTIMIZE_ARM)
# define ALIGNMENT	4
#elif defined(OPTIMIZE_POWERPC)
# define ALIGNMENT	8
#elif defined(OPTIMIZE_SPARCV8PLUS) || defined(OPTIMIZE_SPARCV9)
# define ALIGNMENT	8
#else
# define ALIGNMENT	8
#endif

#define LABEL(name) C_FUNCTION_NAME(name):
#if DARWIN
# define LOCAL(name) L##name
#else
# if __STDC__
#  define LOCAL(name) .L##name
# else
#  define LOCAL(name) .L/**/name
# endif
#endif

#if CYGWIN
# define C_FUNCTION_BEGIN(name)	\
	.align	ALIGNMENT;	\
	.globl	C_FUNCTION_NAME(name);	\
	.def	C_FUNCTION_NAME(name);	\
	.scl	2;	\
	.type	32;	\
	.endef
# define C_FUNCTION_END(name, label)
#else
# if SOLARIS
#  define C_FUNCTION_TYPE	#function
# elif defined(OPTIMIZE_ARM)
#  define C_FUNCTION_TYPE	%function
# else
#  define C_FUNCTION_TYPE	@function
# endif
# if DARWIN
#  define C_FUNCTION_BEGIN(name) \
	.globl	C_FUNCTION_NAME(name)
#  define C_FUNCTION_END(name, label)
# elif defined(OPTIMIZE_IA64)
#  define C_FUNCTION_BEGIN(name) \
	.align	ALIGNMENT; \
	.global	name#; \
	.proc	name#
#  define C_FUNCTION_END(name) \
	.endp	name#
# else
#  define C_FUNCTION_BEGIN(name) \
	.align	ALIGNMENT; \
	.global	C_FUNCTION_NAME(name)
#  define C_FUNCTION_END(name, label) \
	label:	.size C_FUNCTION_NAME(name), label - C_FUNCTION_NAME(name);
# endif
#endif

#endif

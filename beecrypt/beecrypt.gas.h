#ifndef _BEECRYPT_GAS_H
#define _BEECRYPT_GAS_H

#include "config.h"

#ifndef SYMBOL_NAME
# if LEADING_UNDERSCORE
#  ifdef __STDC__
#   define SYMBOL_NAME(name)	_##name
#  else
#   define SYMBOL_NAME(name)	_/**/name
#  endif
# else
#  define SYMBOL_NAME(name)	name
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

#define LABEL(name) SYMBOL_NAME(name):
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
	.globl	SYMBOL_NAME(name);	\
	.def	SYMBOL_NAME(name);	\
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
	.globl	SYMBOL_NAME(name)
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
	.global	SYMBOL_NAME(name)
#  define C_FUNCTION_END(name, label) \
	label:	.size SYMBOL_NAME(name), label - SYMBOL_NAME(name);
# endif
#endif

#if defined(OPTIMIZE_POWERPC)
# if DARWIN
#  define LOAD_ADDRESS(reg,var) lis reg,ha16(var); la reg,lo16(var)(reg)
# else
#  define LOAD_ADDRESS(reg,var) lis reg,var@ha; la reg,var@l(reg)
#  define r0 %r0
#  define r1 %r1
#  define r2 %r2
#  define r3 %r3
#  define r4 %r4
#  define r5 %r5
#  define r6 %r6
#  define r7 %r7
#  define r8 %r8
#  define r9 %r9
#  define r10 %r10
#  define r11 %r11
#  define r12 %r12
#  define r13 %r13
#  define r14 %r14
#  define r15 %r15
#  define r16 %r16
#  define r17 %r17
#  define r18 %r18
#  define r19 %r19
#  define r20 %r20
#  define r21 %r21
#  define r22 %r22
#  define r23 %r23
#  define r24 %r24
#  define r25 %r25
#  define r26 %r26
#  define r27 %r27
#  define r28 %r28
#  define r29 %r29
#  define r30 %r30
#  define r31 %r31
# endif
#endif

#endif

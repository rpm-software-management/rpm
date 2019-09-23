#ifndef _H_MACRO_INTERNAL
#define	_H_MACRO_INTERNAL

/** \ingroup rpmio
 * \file rpmio/rpmmacro_internal.h
 *
 * Internal Macro API
 */

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmmacro
 * Find the end of a macro call
 * @param str           pointer to the character after the initial '%'
 * @return              pointer to the next character after the macro
 */
RPM_GNUC_INTERNAL
const char *findMacroEnd(const char *str);


#ifdef __cplusplus
}
#endif

#endif	/* _H_ MACRO_INTERNAL */

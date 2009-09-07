#ifndef _RPMUTIL_H
#define _RPMUTIL_H

/*
 * Miscellanous utility macros:
 * - portability wrappers for various gcc extensions like __attribute__()
 * - ...
 *
 * Copied from glib, names replaced to avoid clashing with glib.
 *
 */

/* Here we provide RPM_GNUC_EXTENSION as an alias for __extension__,
 * where this is valid. This allows for warningless compilation of
 * "long long" types even in the presence of '-ansi -pedantic'. 
 */
#if     __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 8)
#  define RPM_GNUC_EXTENSION __extension__
#else
#  define RPM_GNUC_EXTENSION
#endif

/* Provide macros to feature the GCC function attribute.
 */
#if    __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
#define RPM_GNUC_PURE                            \
  __attribute__((__pure__))
#define RPM_GNUC_MALLOC    			\
  __attribute__((__malloc__))
#else
#define RPM_GNUC_PURE
#define RPM_GNUC_MALLOC
#endif

#if     __GNUC__ >= 4
#define RPM_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#else
#define RPM_GNUC_NULL_TERMINATED
#endif

#if     __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define RPM_GNUC_PRINTF( format_idx, arg_idx )    \
  __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#define RPM_GNUC_SCANF( format_idx, arg_idx )     \
  __attribute__((__format__ (__scanf__, format_idx, arg_idx)))
#define RPM_GNUC_FORMAT( arg_idx )                \
  __attribute__((__format_arg__ (arg_idx)))
#define RPM_GNUC_NORETURN                         \
  __attribute__((__noreturn__))
#define RPM_GNUC_CONST                            \
  __attribute__((__const__))
#define RPM_GNUC_UNUSED                           \
  __attribute__((__unused__))
#define RPM_GNUC_NO_INSTRUMENT			\
  __attribute__((__no_instrument_function__))
#else   /* !__GNUC__ */
#define RPM_GNUC_PRINTF( format_idx, arg_idx )
#define RPM_GNUC_SCANF( format_idx, arg_idx )
#define RPM_GNUC_FORMAT( arg_idx )
#define RPM_GNUC_NORETURN
#define RPM_GNUC_CONST
#define RPM_GNUC_UNUSED
#define RPM_GNUC_NO_INSTRUMENT
#endif  /* !__GNUC__ */

#if    __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
#define RPM_GNUC_DEPRECATED                            \
  __attribute__((__deprecated__))
#else
#define RPM_GNUC_DEPRECATED
#endif /* __GNUC__ */

#if     __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)
#define RPM_GNUC_MAY_ALIAS __attribute__((may_alias))
#define RPM_GNUC_NONNULL( ... )	\
  __attribute__((__nonnull__ (__VA_ARGS__)))
#else
#define RPM_GNUC_MAY_ALIAS
#define RPM_GNUC_NONNULL( ... )
#endif

#if    __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define RPM_GNUC_WARN_UNUSED_RESULT 		\
  __attribute__((warn_unused_result))
#else
#define RPM_GNUC_WARN_UNUSED_RESULT
#endif /* __GNUC__ */

#if    __GNUC__ >= 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)
#  define RPM_GNUC_INTERNAL __attribute__((visibility("hidden")))
#else
#  define RPM_GNUC_INTERNAL
#endif


/* Guard C code in headers, while including them from C++ */
#ifdef  __cplusplus
# define RPM_BEGIN_DECLS  extern "C" {
# define RPM_END_DECLS    }
#else
# define RPM_BEGIN_DECLS
# define RPM_END_DECLS
#endif

#endif /* _RPMUTIL_H */


#undef BEGIN_NEON_DECLS
#undef END_NEON_DECLS
#ifdef __cplusplus
# define BEGIN_NEON_DECLS extern "C" {
# define END_NEON_DECLS }
#else
# define BEGIN_NEON_DECLS /* empty */
# define END_NEON_DECLS /* empty */
#endif

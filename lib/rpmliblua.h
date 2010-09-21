#ifndef _RPMLIBLUA_H
#define _RPMLIBLUA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize Lua subsystem & register all our extensions */
void rpmLuaInit(void);

/* Shutdown Lua subsystem */
void rpmLuaFree(void);

#ifdef __cplusplus
}
#endif

#endif /* _RPMLIBLUA_H */

#ifndef _RPMLIBLUA_H
#define _RPMLIBLUA_H

/* Initialize Lua subsystem & register all our extensions */
void rpmLuaInit(void);

/* Shutdown Lua subsystem */
void rpmLuaFree(void);

#endif /* _RPMLIBLUA_H */

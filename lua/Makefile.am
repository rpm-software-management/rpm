
LINT = splint

noinst_LTLIBRARIES = liblua.la
noinst_PROGRAMS = lua/lua luac/luac

EXTRA_DIST = \
	COPYRIGHT \
	HISTORY \
	local/modemuncher.c \
	local/userconfig.c \
	local/linit.lua

LDADD = liblua.la
INCLUDES = -I$(srcdir)/include -I$(srcdir)/local

lua_lua_SOURCES = lua/lua.c
lua_lua_CFLAGS = -DLUA_USERCONFIG='"$(srcdir)/local/userconfig.c"'
lua_lua_LDADD = $(LDADD) -lreadline -lhistory -lncurses
luac_luac_SOURCES = luac/luac.c luac/print.c lopcodes.c
luac_luac_CFLAGS = -DLUA_OPNAMES

liblua_la_CFLAGS = -DUSE_DLOPEN -DWITH_POSIX
liblua_la_LIBADD = -lm -ldl
liblua_la_SOURCES = \
		local/linit.c \
		local/linit.lch \
		local/lposix.h \
		local/lposix.c \
		local/lrexlib.h \
		local/lrexlib.c \
		include/lauxlib.h \
		include/lua.h \
		include/lualib.h \
		lib/lauxlib.c \
		lib/lbaselib.c \
		lib/ldblib.c \
		lib/liolib.c \
		lib/lmathlib.c \
		lib/loadlib.c \
		lib/lstrlib.c \
		lib/ltablib.c \
		lapi.c \
		lapi.h \
		lcode.c \
		lcode.h \
		ldebug.c \
		ldebug.h \
		ldo.c \
		ldo.h \
		ldump.c \
		lfunc.c \
		lfunc.h \
		lgc.c \
		lgc.h \
		llex.c \
		llex.h \
		llimits.h \
		lmem.c \
		lmem.h \
		lobject.c \
		lobject.h \
		lopcodes.c \
		lopcodes.h \
		lparser.c \
		lparser.h \
		lstate.c \
		lstate.h \
		lstring.c \
		lstring.h \
		ltable.c \
		ltable.h \
		ltests.c \
		ltm.c \
		ltm.h \
		lundump.c \
		lundump.h \
		lvm.c \
		lvm.h \
		lzio.c \
		lzio.h

local/linit.lch: local/linit.lua
	bin2c local/linit.lua > local/linit.lch

# XXX to avoid local/linit.lch syntax problem.
#
#		local/linit.c \
#		lib/lstrlib.c \
#
LUA_SPLINT_SOURCES = \
		lua/lua.c \
		include/lauxlib.h \
		include/lua.h \
		include/lualib.h \
		lib/lauxlib.c \
		lib/lbaselib.c \
		lib/ldblib.c \
		lib/liolib.c \
		lib/loadlib.c \
		lib/lmathlib.c \
		lib/ltablib.c \
		local/lposix.h \
		local/lposix.c \
		local/lrexlib.h \
		local/lrexlib.c \
		lapi.c \
		lapi.h \
		lcode.c \
		lcode.h \
		ldebug.c \
		ldebug.h \
		ldo.c \
		ldo.h \
		ldump.c \
		lfunc.c \
		lfunc.h \
		lgc.c \
		lgc.h \
		llex.c \
		llex.h \
		llimits.h \
		lmem.c \
		lmem.h \
		lobject.c \
		lobject.h \
		lopcodes.c \
		lopcodes.h \
		lparser.c \
		lparser.h \
		lstate.c \
		lstate.h \
		lstring.c \
		lstring.h \
		ltable.c \
		ltable.h \
		ltests.c \
		ltm.c \
		ltm.h \
		lundump.c \
		lundump.h \
		lvm.c \
		lvm.h \
		lzio.c \
		lzio.h

.PHONY:	sources
sources:
	@echo $(LUA_SPLINT_SOURCES:%=lua/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(LUA_SPLINT_SOURCES)


libluaext = static_library(
  'luaext',
  sources : [
    'lposix.c',
    'lrexlib.c',
  ],
  dependencies : [
    lua,
  ],
  c_args : [
    '-DWITH_POSIX',
  ],
  install : false,
)
luaext = declare_dependency(
  link_with : libluaext,
  include_directories : include_directories('.'),
)

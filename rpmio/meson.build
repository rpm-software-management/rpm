librpmio_sources = [
  'argv.c',
  'base64.c',
  'digest_@0@.c'.format(crypto_backend),
  'digest.c',
  'expression.c',
  'macro.c',
  'rpmfileutil.c',
  'rpmglob.c',
  'rpmhook.c',
  'rpmio.c',
  'rpmkeyring.c',
  'rpmlog.c',
  'rpmmalloc.c',
  'rpmpgp.c',
  'rpmsq.c',
  'rpmstring.c',
  'rpmstrpool.c',
  'rpmsw.c',
  'url.c',
]
librpmio_extra_deps = []
if lua.found()
  librpmio_sources += ['rpmlua.c']
  librpmio_extra_deps += [luaext]
endif
librpmio = library(
  'rpmio',
  librpmio_sources,
  version : libversion,
  link_with : libmisc,
  dependencies : [
    crypto_dep,
    bzip2,
    zlib,
    popt,
    lzma,
    zstd,
    threads,
    librpmio_extra_deps,
  ],
  include_directories : [
    include_directories('.'),
  ],
  c_args : [
    '-DRPMCONFIGDIR="@0@"'.format(rpmconfigdir),
  ],
  install : true,
)

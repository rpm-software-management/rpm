librpmsign_sources = [
  'rpmgensig.c',
]
librpmsign_extra_deps = []
if imaevm.found()
  librpmsign_sources += ['rpmsignfiles.c']
  librpmsign_extra_deps += [imaevm]
endif
librpmsign = library(
  'rpmsign',
  librpmsign_sources,
  version : libversion,
  link_with : [
    librpm,
    librpmio,
  ],
  dependencies : [
    popt,
    librpmsign_extra_deps,
  ],
  install : true,
)

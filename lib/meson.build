tagtbl_h = custom_target(
  'tagtbl.h',
  output : 'tagtbl.h',
  capture : true,
  command : [
    find_program('gentagtbl.sh'),
    files('rpmtag.h'),
  ],
)
librpm_sources = [
  'backend/dbi.c',
  'backend/dbiset.c',
  'backend/dummydb.c',
  'cpio.c',
  'depends.c',
  'formats.c',
  'fprint.c',
  'fsm.c',
  'header.c',
  'headerfmt.c',
  'headerutil.c',
  'manifest.c',
  'order.c',
  'package.c',
  'poptALL.c',
  'poptI.c',
  'poptQV.c',
  'psm.c',
  'query.c',
  'relocation.c',
  'rpmal.c',
  'rpmchecksig.c',
  'rpmchroot.c',
  'rpmdb.c',
  'rpmds.c',
  'rpmfi.c',
  'rpmfs.c',
  'rpmgi.c',
  'rpminstall.c',
  'rpmlead.c',
  'rpmlock.c',
  'rpmplugins.c',
  'rpmprob.c',
  'rpmps.c',
  'rpmrc.c',
  'rpmscript.c',
  'rpmtd.c',
  'rpmte.c',
  'rpmtriggers.c',
  'rpmts.c',
  'rpmug.c',
  'rpmvercmp.c',
  'rpmvs.c',
  'signature.c',
  'tagexts.c',
  'tagname.c',
  tagtbl_h,
  'transaction.c',
  'verify.c',
]
librpm_extra_deps = []
if libdb.found()
  librpm_sources += ['backend/db3.c']
  librpm_extra_deps += [libdb]
endif
if bdb_ro
  librpm_sources += ['backend/bdb_ro.c']
endif
if ndb
  librpm_sources += [
    'backend/ndb/glue.c',
    'backend/ndb/rpmidx.c',
    'backend/ndb/rpmpkg.c',
    'backend/ndb/rpmxdb.c',
  ]
endif
if sqlite.found()
  librpm_sources += ['backend/sqlite.c']
  librpm_extra_deps += [sqlite]
endif
if lua.found()
  librpm_sources += ['rpmliblua.c']
  librpm_extra_deps += [lua]
endif
librpm_sources += [tagtbl_h]
librpm = library(
  'rpm',
  librpm_sources,
  version : libversion,
  link_with : [
    librpmio,
  ],
  dependencies : [
    popt,
    libcap,
    threads,
    librpm_extra_deps,
  ],
  c_args : [
    '-DLIBRPMALIAS_FILENAME="rpmpopt-@0@"'.format(meson.project_version()),
    '-DLIBRPMALIAS_EXECPATH="@0@"'.format(
      join_paths(
        get_option('prefix'),
	get_option('bindir'),
      ),
    ),
    '-DLOCALSTATEDIR="@0@"'.format(
      join_paths(
	get_option('prefix'),
	get_option('localstatedir'),
      ),
    ),
    '-DSYSCONFDIR="@0@"'.format(
      join_paths(
        get_option('prefix'),
	get_option('sysconfdir'),
      ),
    ),
    '-DLOCALEDIR="@0@"'.format(
      join_paths(
        get_option('prefix'),
	get_option('localedir'),
      ),
    ),
    '-DRPMCANONVENDOR="@0@"'.format(get_option('vendor')),
  ],
  install : true,
)

project(
  'rpm', 'c',
  version : '4.15.90',
  license : 'GPL-2.0+',
  meson_version : '>= 0.53.0',
  default_options : [
    'c_std=c99',
  ],
)

libversion = '9.1.0'

i18n = import('i18n')
add_project_arguments('-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()), language : 'c')

cc = meson.get_compiler('c')

extra_flags = [
  '-fno-strict-aliasing',
  '-fstack-protector',
  '-Wempty-body',
  '-Wpointer-arith',
  '-Wmissing-prototypes',
  '-Wstrict-prototypes',
]
flags = cc.get_supported_arguments(extra_flags) + ['-D_REENTRANT', '-D_GNU_SOURCE', '-DHAVE_CONFIG_H']
add_project_arguments(flags, language : 'c')

zlib = dependency('zlib')
bzip2 = dependency('bzip2', required : get_option('bzip2'))
lzma = dependency('liblzma', required : get_option('lzma'))
zstd = dependency('libzstd', required : get_option('zstd'))
libmagic = cc.find_library('magic', has_headers : 'magic.h')
popt = dependency('popt')
libarchive = dependency('libarchive', required : get_option('archive'), disabler : true)
libcap = dependency('libcap', required : get_option('cap'))
lua = dependency('lua', version : '>= 5.1', required : get_option('lua'))
libdb = cc.find_library('db', has_headers : 'db.h', required : get_option('bdb'))
sqlite = dependency('sqlite3', version : '>= 3.22.0', required : get_option('sqlite'))
openmp = dependency('openmp', required : get_option('openmp'))
threads = dependency('threads')
libelf = dependency('libelf', required : false)
libdw = dependency('libdw', version : '>= 0.167', required : false)
imaevm = cc.find_library('imaevm', has_headers : 'imaevm.h', required : get_option('imaevm'))

conf_items = {}

crypto_backend = get_option('crypto')
if crypto_backend == 'libgcrypt'
  libgcrypt = dependency('libgcrypt')
  crypto_dep = libgcrypt
elif crypto_backend == 'openssl'
  openssl = dependency('libcrypto')
  foreach f : [
    'EVP_MD_CTX_new',
    'EVP_PKEY_CTX_new',
    'DSA_set0_key',
    'DSA_set0_pqg',
    'DSA_SIG_set0',
    'RSA_set0_key',
    'BN_bn2binpad',
  ]
    conf_items += {
      'HAVE_@0@'.format(f.to_upper()) : cc.has_function(f, dependencies : openssl)
    }
  endforeach
  crypto_dep = openssl
else
  error('Unsupported crypto backend: @0@'.format(crypto_backend))
endif
foreach f, data : {
  'putenv' : {},
  'mempcpy' : {},
  'fdatasync': {
    'prefix' : '#include <unistd.h>',
    'args' : '-D_GNU_SOURCE',
  },
  'lutimes' : {},
  'mergesort' : {},
  'getauxval' : {},
  'setprogname' : {
    'prefix' : '#include <stdlib.h>',
  },
  'syncfs' : {},
  'sched_getaffinity' : {
    'prefix' : '#include <sched.h>',
    'args' : '-D_GNU_SOURCE',
  },
  'unshare' : {
    'prefix' : '#include <sched.h>',
    'args' : '-D_GNU_SOURCE',
  },
  'stpcpy' : {},
  'stpncpy' : {},
  'iconv' : {
    'prefix' : '#include <iconv.h>',
  },
}
  conf_items += {
    'HAVE_@0@'.format(f.to_upper()) : cc.has_function(f, prefix : data.get('prefix', ''), args : data.get('args', []))
  }
endforeach
if libcap.found()
  conf_items += {
    'HAVE_CAP_COMPARE' : cc.has_function('cap_compare', dependencies : libcap)
  }
endif

conf = configuration_data()

ndb = not get_option('ndb').disabled()
if ndb
  # FIXME: is there some better way?
  has_mremap = cc.has_function('mremap')
  if not has_mremap and get_option('ndb').enabled()
    error('mremap() is needed for ndb')
  endif
  ndb = has_mremap
  conf.set10('HAVE_MREMAP', has_mremap)
endif

enable_plugins = get_option('plugins')
if get_option('default_library') == 'static' and enable_plugins.auto()
  enable_plugins = false
else
  enable_plugins = not enable_plugins.disabled()
endif
plugins = {}
foreach p : [
  'audit',
  'ima',
  'inhibit',
  'prioreset',
  'selinux',
  'syslog',
]
  opt = get_option('plugin-@0@'.format(p))
  enabled = not opt.disabled() and enable_plugins
  deps = []
  if enabled
    if p == 'audit'
      deps = [dependency('audit', required : opt)]
    elif p == 'inhibit'
      deps = [dependency('dbus-1', version : '>= 1.3', required : opt)]
    elif p == 'selinux'
      selinux = dependency('libselinux', required : opt)
      if selinux.found()
        conf.set10('HAVE_SETEXECFILECON', true)
      endif
      deps = [selinux]
    endif
    if opt.auto()
      foreach d : deps
        if not d.found()
          enabled = false
          break
        endif
      endforeach
    endif
  endif
  plugins += {
    p : {
      'enabled' : enabled,
      'dependencies' : deps,
    }
  }
endforeach

bdb_ro = not get_option('bdb-ro').disabled()

conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_BUGREPORT', 'rpm-maint@lists.rpm.org')
conf.set_quoted('VERSION', meson.project_version())
conf_items += {
  'ENABLE_NLS' : true,
  'HAVE___PROGNAME' : cc.links('extern const char *__progname; int main(void) { if (*__progname == 0) return 0; }'),
  'HAVE_BZLIB_H' : bzip2.found(),
  'HAVE_LZMA_H' : lzma.found(),
  'HAVE_ZSTD' : zstd.found(),
  'WITH_BDB' : libdb.found(),
  'ENABLE_OPENMP' : openmp.found(),
  'HAVE_LIBELF' : libelf.found(),
  'HAVE_STRUCT_DIRENT_D_TYPE' : cc.has_member('struct dirent', 'd_type', prefix : '#include <dirent.h>'),
  'WITH_LUA' : lua.found(),
  'ENABLE_NDB' : ndb,
  'WITH_BDB_RO' : bdb_ro,
}
# Most of the things need to be set to 1, or unset
foreach c, cond : conf_items
  if cond
    conf.set10(c, true)
  endif
endforeach
conf.set10('WITH_CAP', libcap.found())
conf.set10('HAVE_GZSEEK', cc.has_function('gzseek', dependencies : zlib))
# dwelf_elf_gnu_build_id was introduced in elfutils 0.159, we check for newer
conf.set10('HAVE_LIBDW', libdw.found())
if libdw.found()
  conf.set10('HAVE_DWELF_ELF_BEGIN', cc.has_function('dwelf_elf_begin', dependencies : libdw))
endif
foreach f : [
  'secure_getenv',
  '__secure_getenv',
]
  conf.set10('HAVE_@0@'.format(f.underscorify().to_upper()), cc.has_function(f))
endforeach

foreach h : [
  'getopt.h',
  'limits.h',
  'mcheck.h',
  'unistd.h',
  'sys/auxv.h',
  'sys/param.h',
  'sys/utsname.h',
]
  conf.set10(
    'HAVE_@0@'.format(h.underscorify().to_upper()),
    cc.has_header(h),
  )
endforeach

if cc.check_header('sys/mkdev.h')
  conf.set10('MAJOR_IN_MKDEV', true)
elif cc.check_header('sys/sysmacros.h')
  conf.set10('MAJOR_IN_SYSMACROS', true)
endif

foreach d : [
  meson.current_build_dir(),
  meson.current_source_dir(),
  join_paths(meson.current_source_dir(), 'include'),
]
  add_project_arguments('-I' + d, language : 'c')
endforeach

configure_file(
  output : 'config.h',
  configuration : conf,
)

rpmconfigdir = '@0@/lib/rpm'.format(get_option('prefix'))
# HACK: It probably should be $(libexecdir)/rpm or $(libdir)/rpm
rpmlibexecdir = '@0@/lib/rpm'.format(get_option('prefix'))

awk = find_program('awk')
uid0_r = run_command(
  awk,
  '-F:',
  '$3==0 {print $1; exit}',
  '/etc/passwd',
)
gid0_r = run_command(
  awk,
  '-F:',
  '$3==0 {print $1; exit}',
  '/etc/group',
)
uid0 = uid0_r.stdout().strip()
gid0 = gid0_r.stdout().strip()
add_project_arguments('-DUID_0_USER="@0@"'.format(uid0), language : 'c')
add_project_arguments('-DGID_0_GROUP="@0@"'.format(gid0), language : 'c')

install_subdir(
  'include/rpm',
  install_dir : get_option('includedir'),
)

summary({
  'Lua' : lua.found(),
}, bool_yn : true)
summary({
  'BDB' : libdb.found(),
  'BDB (ro)' : bdb_ro,
  'NDB' : ndb,
  'SQLite' : sqlite.found(),
}, section : 'Database formats', bool_yn : true)
summary({
  'bzdio' : bzip2.found(),
  'xzdio' : lzma.found(),
  'lzdio' : lzma.found(),
  'zstio' : zstd.found(),
}, section : 'Compression formats', bool_yn : true)
foreach p, data : plugins
  summary(p, data['enabled'], section : 'Plugins', bool_yn : true)
endforeach

subdir('po')
subdir('misc')
if lua.found()
  subdir('luaext')
endif
subdir('rpmio')
subdir('lib')
subdir('build')
subdir('sign')
subdir('scripts')
subdir('fileattrs')
subdir('doc')
subdir('plugins')

libcliutils = static_library(
  'cliutils',
  'cliutils.c',
  link_with : [
    librpm,
    librpmio,
  ],
  dependencies : [
    popt,
  ],
  install : false,
)
cliutils_dep = declare_dependency(
  link_with : libcliutils,
  dependencies : [
    popt,
  ],
)
foreach e, data : {
  'rpm' : {
    'link_with' : [
      librpm,
      librpmio,
    ],
  },
  'rpmdb' : {
    'link_with' : [
      librpm,
      librpmio,
    ],
  },
  'rpmkeys' : {
    'link_with' : [
      librpm,
      librpmio,
    ],
  },
  'rpmsign' : {
    'link_with' : [
      librpmsign,
      librpm,
      librpmio,
    ],
  },
  'rpmbuild' : {
    'link_with' : [
      librpmbuild,
      librpm,
      librpmio,
    ],
  },
  'rpmspec' : {
    'link_with' : [
      librpmbuild,
      librpm,
      librpmio,
    ],
  },
  'rpm2cpio' : {
    'link_with' : [
      librpm,
      librpmio,
    ],
  },
  'rpm2archive' : {
    'link_with' : [
      librpm,
      librpmio,
    ],
    'dependencies' : [
      libarchive,
    ],
  },
}
  executable(
    e,
    '@0@.c'.format(e),
    link_with : data.get('link_with', []),
    dependencies : [
      cliutils_dep,
      data.get('dependencies', []),
    ],
    install : true,
  )
endforeach

if libelf.found()
  if libdw.found()
    executable(
      'debugedit',
      sources : [
        'tools/debugedit.c',
        'tools/hashtab.c',
      ],
      link_with : [
        librpmio,
      ],
      dependencies : [
	popt,
        libelf,
	libdw,
      ],
      install : true,
      install_dir : rpmlibexecdir,
    )
  endif
  executable(
    'elfdeps',
    'tools/elfdeps.c',
    link_with : [
      librpmio,
    ],
    dependencies : [
      popt,
      libelf,
    ],
    install : true,
    install_dir : rpmlibexecdir,
  )
  executable(
    'sepdebugcrcfix',
    'tools/sepdebugcrcfix.c',
    dependencies : [
      libelf,
    ],
    install : true,
    install_dir : rpmlibexecdir,
  )
endif

executable(
  'rpmdeps',
  'tools/rpmdeps.c',
  link_with : [
    librpm,
    librpmio,
    librpmbuild,
  ],
  dependencies : [
    popt,
  ],
  install : true,
  install_dir : rpmlibexecdir,
)

executable(
  'rpmgraph',
  'tools/rpmgraph.c',
  link_with : [
    librpm,
    librpmio,
  ],
  dependencies : [
    popt,
  ],
  install : true,
)

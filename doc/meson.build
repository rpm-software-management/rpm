doxygen = find_program('doxygen', required : get_option('doc'))
if doxygen.found()
  dconf = configuration_data({
    'PACKAGE' : meson.project_name(),
    'VERSION' : meson.project_version(),
    'pkginclude_HEADERS' : join_paths(meson.source_root(), 'include', 'rpm'),
    'top_srcdir' : meson.source_root(),
  })
  if find_program('dot', required : false).found()
    dconf.set('HAVE_DOT', 'YES')
  else
    dconf.set('HAVE_DOT', 'NO')
  endif
  doxyfile = configure_file(
    input : 'librpm.doxy.in',
    output : 'librpm.doxy',
    configuration : dconf,
    install : false,
  )
  custom_target(
    'rpm-docs',
    input : doxyfile,
    output : 'html',
    command : [
      doxygen,
      doxyfile,
    ],
    install : false,
  )
endif

# TODO: do not install man packages for things which are not going to be built
manpages = [
  'gendiff.1',
  'rpm2archive.8',
  'rpm2cpio.8',
  'rpm.8',
  'rpmbuild.8',
  'rpmdb.8',
  'rpmdeps.8',
  'rpmgraph.8',
  'rpmkeys.8',
  'rpm-misc.8',
  'rpm-plugin-audit.8',
  'rpm-plugin-ima.8',
  'rpm-plugin-prioreset.8',
  'rpm-plugins.8',
  'rpm-plugin-selinux.8',
  'rpm-plugin-syslog.8',
  'rpm-plugin-systemd-inhibit.8',
  'rpmsign.8',
  'rpmspec.8',
  'fr/rpm.8',
  'ja/rpm2cpio.8',
  'ja/rpm.8',
  'ja/rpmbuild.8',
  'ja/rpmgraph.8',
  'ko/rpm2cpio.8',
  'ko/rpm.8',
  'pl/gendiff.1',
  'pl/rpm2cpio.8',
  'pl/rpm.8',
  'pl/rpmdeps.8',
  'pl/rpmgraph.8',
  'ru/rpm2cpio.8',
  'ru/rpm.8',
  'sk/rpm.8',
]
foreach m : manpages
  p = m.split('/')
  if p.length() > 1
    dir = p[0]
  else
    dir = ''
  endif
  n = m.split('.')[-1]
  install_man(
    m,
    install_dir : join_paths(
      get_option('mandir'),
      dir,
      'man@0@'.format(n),
    ),
  )
endforeach

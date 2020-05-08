foreach f : files([
  'debuginfo.attr',
  'desktop.attr',
  'elf.attr',
  'font.attr',
  'libtool.attr',
  'metainfo.attr',
  'ocaml.attr',
  'perl.attr',
  'perllib.attr',
  'pkgconfig.attr',
  'python.attr',
  'pythondist.attr',
  'script.attr',
])
  install_data(
    f,
    install_dir : join_paths(
      rpmconfigdir,
      'fileattrs',
    ),
  )
endforeach

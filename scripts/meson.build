foreach dir, scripts : {
  rpmconfigdir : [
    'brp-compress',
    'brp-java-gcjcompile',
    'brp-python-bytecompile',
    'brp-python-hardlink',
    'brp-strip',
    'brp-strip-comment-note',
    'brp-strip-shared',
    'brp-strip-static-archive',
    'check-buildroot',
    'check-files',
    'check-prereqs',
    'check-rpaths',
    'check-rpaths-worker',
    'find-lang.sh',
    'find-provides',
    'find-requires',
    'fontconfig.prov',
    'libtooldeps.sh',
    'ocamldeps.sh',
    'perl.prov',
    'perl.req',
    'pkgconfigdeps.sh',
    'pythondistdeps.py',
    'rpm2cpio.sh',
    'rpmdb_dump',
    'rpmdb_load',
    'script.req',
    'tgpg',
  ],
  get_option('bindir') : [
    'gendiff',
  ],
}
  foreach s : scripts
    install_data(
      s,
      install_dir : dir,
      install_mode : 'rwxr-xr-x',
    )
  endforeach
endforeach

foreach p, data : plugins
  if not data['enabled']
    continue
  endif

  if p == 'inhibit'
    p = 'systemd_inhibit'
  endif
  sources = files(['@0@.c'.format(p)])

  shared_module(
    p,
    '@0@.c'.format(p),
    name_prefix : '',
    link_with : [
      librpm,
      librpmio,
    ],
    dependencies : data['dependencies'],
    install : true,
    install_dir : join_paths(
      get_option('libdir'),
      'rpm-plugins',
    ),
  )
endforeach

#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright 2010 Per Øyvind Karlsen <proyvind@moondrake.org>
# Copyright 2015 Neal Gompa <ngompa13@gmail.com>
#
# This program is free software. It may be redistributed and/or modified under
# the terms of the LGPL version 2.1 (or later).
#
# RPM python (egg) dependency generator.
#

from __future__ import print_function
from getopt import getopt
from os.path import basename, dirname, isdir, sep, splitext
from sys import argv, stdin, version
from distutils.sysconfig import get_python_lib
from subprocess import Popen, PIPE, STDOUT
import os


opts, args = getopt(argv[1:], 'hPRrCOEb:',
        ['help', 'provides', 'requires', 'recommends', 'conflicts', 'obsoletes', 'extras','buildroot='])

Provides = False
Requires = False
Recommends = False
Conflicts = False
Obsoletes = False
Extras = False
buildroot = None

for o, a in opts:
    if o in ('-h', '--help'):
        print('-h, --help\tPrint help')
        print('-P, --provides\tPrint Provides')
        print('-R, --requires\tPrint Requires')
        print('-r, --recommends\tPrint Recommends')
        print('-C, --conflicts\tPrint Conflicts')
        print('-O, --obsoletes\tPrint Obsoletes (unused)')
        print('-E, --extras\tPrint Extras ')
        print('-b, --buildroot\tBuildroot for package ')
        exit(1)
    elif o in ('-P', '--provides'):
        Provides = True
    elif o in ('-R', '--requires'):
        Requires = True
    elif o in ('-r', '--recommends'):
        Recommends = True
    elif o in ('-C', '--conflicts'):
        Conflicts = True
    elif o in ('-O', '--obsoletes'):
        Obsoletes = True
    elif o in ('-E', '--extras'):
        Extras = True
    elif o in ('-b', '--buildroot'):
        buildroot = a

def is_exe(fpath):
    return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

typelib_check = False

if is_exe("/usr/lib/rpm/gi-find-deps.sh") and is_exe("/usr/bin/g-ir-dep-tool"):
    if not buildroot:
        pass
    else:
        typelib_check = True

if Requires:
    py_abi = True
else:
    py_abi = False
py_deps = {}
if args:
    files = args
else:
    files = stdin.readlines()

for f in files:
    f = f.strip()
    lower = f.lower()
    name = 'python(abi)'
    # add dependency based on path, versioned if within versioned python directory
    if py_abi and (lower.endswith('.py') or lower.endswith('.pyc') or lower.endswith('.pyo')):
        if not name in py_deps:
            py_deps[name] = []
        purelib = get_python_lib(standard_lib=1, plat_specific=0).split(version[:3])[0]
        platlib = get_python_lib(standard_lib=1, plat_specific=1).split(version[:3])[0]
        for lib in (purelib, platlib):
            if lib in f:
                spec = ('==',f.split(lib)[1].split(sep)[0])
                if not spec in py_deps[name]:
                    py_deps[name].append(spec)
        # Pipe files to find typelib requires 
        if typelib_check:
            p = Popen(['/usr/lib/rpm/gi-find-deps.sh', '-R',str(buildroot)], stdout=PIPE, stdin=PIPE, stderr=STDOUT)
            (stdoutdata, stderrdata) = p.communicate(input=str(f)+"\n")
            
            if stdoutdata and stdoutdata:
                py_deps[stdoutdata.strip()]= ""

    # XXX: hack to workaround RPM internal dependency generator not passing directories
    lower_dir = dirname(lower)
    if lower_dir.endswith('.egg') or \
            lower_dir.endswith('.egg-info') or \
            lower_dir.endswith('.egg-link'):
        lower = lower_dir
        f = dirname(f)
    # Determine provide, requires, conflicts & recommends based on egg metadata
    if lower.endswith('.egg') or \
            lower.endswith('.egg-info') or \
            lower.endswith('.egg-link'):
        # This import is very slow, so only do it if needed
        from pkg_resources import Distribution, FileMetadata, PathMetadata
        dist_name = basename(f)
        if isdir(f):
            path_item = dirname(f)
            metadata = PathMetadata(path_item, f)
        else:
            path_item = f
            metadata = FileMetadata(f)
        dist = Distribution.from_location(path_item, dist_name, metadata)
        # Get the Python major version
        pyver_major = dist.py_version.split('.')[0]
        if Provides:
            # If egg metadata says package name is python, we provide python(abi)
            if dist.key == 'python':
                name = 'python(abi)'
                if not name in py_deps:
                    py_deps[name] = []
                py_deps[name].append(('==', dist.py_version))
            name = 'python{}egg({})'.format(pyver_major, dist.key)
            if not name in py_deps:
                py_deps[name] = []
            if dist.version:
                spec = ('==', dist.version)
                if not spec in py_deps[name]:
                    py_deps[name].append(spec)
        if Requires or (Recommends and dist.extras):
            name = 'python(abi)'
            # If egg metadata says package name is python, we don't add dependency on python(abi)
            if dist.key == 'python':
                py_abi = False
                if name in py_deps:
                    py_deps.pop(name)
            elif py_abi and dist.py_version:
                if not name in py_deps:
                    py_deps[name] = []
                spec = ('==', dist.py_version)
                if not spec in py_deps[name]:
                    py_deps[name].append(spec)
            deps = dist.requires()
            if Recommends:
                depsextras = dist.requires(extras=dist.extras)
                if not Requires:
                    for dep in reversed(depsextras):
                        if dep in deps:
                            depsextras.remove(dep)
                deps = depsextras
            # add requires/recommends based on egg metadata
            for dep in deps:
                name = 'python{}egg({})'.format(pyver_major, dep.key)
                for spec in dep.specs:
                    if spec[0] != '!=':
                        if not name in py_deps:
                            py_deps[name] = []
                        if not spec in py_deps[name]:
                            py_deps[name].append(spec)
                if not dep.specs:
                    py_deps[name] = []
        # Unused, for automatic sub-package generation based on 'extras' from egg metadata
        # TODO: implement in rpm later, or...?
        if Extras:
            deps = dist.requires()
            extras = dist.extras
            print(extras)
            for extra in extras:
                print('%%package\textras-{}'.format(extra))
                print('Summary:\t{} extra for {} python egg'.format(extra, dist.key))
                print('Group:\t\tDevelopment/Python')
                depsextras = dist.requires(extras=[extra])
                for dep in reversed(depsextras):
                    if dep in deps:
                        depsextras.remove(dep)
                deps = depsextras
                for dep in deps:
                    for spec in dep.specs:
                        if spec[0] == '!=':
                            print('Conflicts:\t{} {} {}'.format(dep.key, '==', spec[1]))
                        else:
                            print('Requires:\t{} {} {}'.format(dep.key, spec[0], spec[1]))
                print('%%description\t{}'.format(extra))
                print('{} extra for {} python egg'.format(extra, dist.key))
                print('%%files\t\textras-{}\n'.format(extra))
        if Conflicts:
            # Should we really add conflicts for extras?
            # Creating a meta package per extra with recommends on, which has
            # the requires/conflicts in stead might be a better solution...
            for dep in dist.requires(extras=dist.extras):
                name = dep.key
                for spec in dep.specs:
                    if spec[0] == '!=':
                        if not name in py_deps:
                            py_deps[name] = []
                        spec = ('==', spec[1])
                        if not spec in py_deps[name]:
                            py_deps[name].append(spec)
names = list(py_deps.keys())
names.sort()
for name in names:
    if py_deps[name]:
        # Print out versioned provides, requires, recommends, conflicts
        for spec in py_deps[name]:
            print('{} {} {}'.format(name, spec[0], spec[1]))
    else:
        # Print out unversioned provides, requires, recommends, conflicts
        print(name)

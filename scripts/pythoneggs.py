#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2010 Per Øyvind Karlsen <proyvind@moondrake.org>
#
# This program is free software. It may be redistributed and/or modified under
# the terms of the LGPL version 2.1 (or later).
#
# RPM python (egg) dependency generator.
#

from getopt import getopt
from os.path import basename, dirname, isdir, sep, splitext
from sys import argv, stdin, version
from pkg_resources import Distribution, FileMetadata, PathMetadata
from distutils.sysconfig import get_python_lib
from subprocess import Popen, PIPE, STDOUT
import os


opts, args = getopt(argv[1:], 'hPRSCOEb:',
        ['help', 'provides', 'requires', 'suggests', 'conflicts', 'obsoletes', 'extras','buildroot='])

Provides = False
Requires = False
Suggests = False
Conflicts = False
Obsoletes = False
Extras = False
buildroot = None

for o, a in opts:
    if o in ('-h', '--help'):
        print('-h, --help\tPrint help')
        print('-P, --provides\tPrint Provides')
        print('-R, --requires\tPrint Requires')
        print('-S, --suggests\tPrint Suggests')
        print('-C, --conflicts\tPrint Conflicts')
        print('-O, --obsoletes\tPrint Obsoletes (unused)')
        print('-E, --extras\tPrint Extras ')
        print('-b, --buildroot\tBuildroot for package ')
        exit(1)
    elif o in ('-P', '--provides'):
        Provides = True
    elif o in ('-R', '--requires'):
        Requires = True
    elif o in ('-S', '--suggests'):
        Suggests = True
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
    dlower = dirname(lower)
    if dlower.endswith('.egg') or \
            dlower.endswith('.egg-info') or \
            dlower.endswith('.egg-link'):
        lower = dlower
        f = dirname(f)
    # Determine provide, requires, conflicts & suggests based on egg metadata
    if lower.endswith('.egg') or \
            lower.endswith('.egg-info') or \
            lower.endswith('.egg-link'):
        dist_name = basename(f)
        if isdir(f):
            path_item = dirname(f)
            metadata = PathMetadata(path_item, f)
        else:
            path_item = f
            metadata = FileMetadata(f)
        dist = Distribution.from_location(path_item, dist_name, metadata)
        if Provides:
            # If egg metadata says package name is python, we provide python(abi)
            if dist.key == 'python':
                name = 'python(abi)'
                if not name in py_deps:
                    py_deps[name] = []
                py_deps[name].append(('==', dist.py_version))
            if f.find('python3') > 0:
                name = 'python3egg(%s)' % dist.key
            else:
                name = 'pythonegg(%s)' % dist.key
            if not name in py_deps:
                py_deps[name] = []
            if dist.version:
                spec = ('==', dist.version)
                if not spec in py_deps[name]:
                    py_deps[name].append(spec)
        if Requires or (Suggests and dist.extras):
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
            if Suggests:
                depsextras = dist.requires(extras=dist.extras)
                if not Requires:
                    for dep in reversed(depsextras):
                        if dep in deps:
                            depsextras.remove(dep)
                deps = depsextras
            # add requires/suggests based on egg metadata
            for dep in deps:
                if f.find('python3') > 0:
                    name = 'python3egg(%s)' % dep.key
                else:
                    name = 'pythonegg(%s)' % dep.key
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
                print('%%package\textras-%s' % extra)
                print('Summary:\t%s extra for %s python egg' % (extra, dist.key))
                print('Group:\t\tDevelopment/Python')
                depsextras = dist.requires(extras=[extra])
                for dep in reversed(depsextras):
                    if dep in deps:
                        depsextras.remove(dep)
                deps = depsextras
                for dep in deps:
                    for spec in dep.specs:
                        if spec[0] == '!=':
                            print('Conflicts:\t%s %s %s' % (dep.key, '==', spec[1]))
                        else:
                            print('Requires:\t%s %s %s' % (dep.key, spec[0], spec[1]))
                print('%%description\t%s' % extra)
                print('%s extra for %s python egg' % (extra, dist.key))
                print('%%files\t\textras-%s\n' % extra)
        if Conflicts:
            # Should we really add conflicts for extras?
            # Creating a meta package per extra with suggests on, which has
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
        # Print out versioned provides, requires, suggests, conflicts
        for spec in py_deps[name]:
            print('%s %s %s' % (name, spec[0], spec[1]))
    else:
        # Print out unversioned provides, requires, suggests, conflicts
        print(name)

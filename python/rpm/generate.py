# -*- coding: utf-8 -*-
#
# Copyright 2016-2017 Per Øyvind Karlsen <proyvind@moondrake.org>
#
# This program is free software. It may be redistributed and/or modified under
# the terms of the LGPL version 2.1 (or later).
#
# Runtime python package generator for use with embedded python interpreter

import distutils.dist
from distutils.command.bdist_rpm import bdist_rpm
from distutils.command import sdist
from distutils.sysconfig import get_config_var
from distutils.filelist import FileList
import distutils.core
from tempfile import NamedTemporaryFile

import os, sys
from types import *
from glob import glob
import rpm
from subprocess import Popen, PIPE
from shutil import rmtree
from time import gmtime, strftime
from locale import resetlocale, setlocale, LC_TIME
import textwrap
import hashlib

import json
try:
    from urllib.request import urlopen
    from urllib.error import HTTPError, URLError
except ImportError:
    from urllib2 import urlopen, HTTPError, URLError

def _show_warning(message, category=Warning, filename=None, lineno=0, file=None, line=None):
    return

import json
try:
    from urllib.request import urlopen
    from urllib.error import HTTPError, URLError
except ImportError:
    from urllib2 import urlopen, HTTPError, URLError

_definedTags = {}

def _tag(tagname, value):
    if not tagname in _definedTags:
        defined = rpm.expandMacro('%{?'+tagname+'}')
        if defined:
            _definedTags[tagname] = True
            value = defined
        else:
            _definedTags[tagname] = False
    return value

def _pypi_source_url(pkg_name, version=None, suffix=None):
    """
    Get package filename, version and download url.
    If version=None, latest release available from PyPI will be used.
    If suffix=None, file suffix will be determined automatically, with the
    preferred format picked if multiple available
    """

    metadata_url = 'https://pypi.python.org/pypi/{pkg}/json'.format(pkg=pkg_name)
    preferred = ['', 'zip', 'tar.gz', 'tar.bz2', 'tar.xz']
    pypi_url = None
    full_pkg_name = None
    md5_digest = None
    try:
        pkg_json = json.loads(urlopen(metadata_url).read().decode())
        if not version:
            version = pkg_json['info']['version']
        release = pkg_json['releases'][version]
        for download in release:
            filename = download['filename']
            download_url = download['url']

            if not 'sdist' in download['packagetype'] or \
                    (suffix and not filename.endswith(suffix)):
               continue
            if not pypi_url:
                pypi_url = download_url
                full_pkg_name = filename
                md5_digest = download['md5_digest']
                continue

            previous = 0
            current = 0
            for ext in preferred:
                if filename.endswith(ext):
                    current = preferred.index(ext)
                if full_pkg_name.endswith(ext):
                    previous = preferred.index(ext)
            if current > previous:
                pypi_url = download_url

                md5_digest = download['md5_digest']
    except:
        pypi_url = None
        if not suffix:
            suffix = "tar.gz"
        full_pkg_name = '{name}-{version}.{suffix}'.format(name=pkg_name,
                                                    version=version,
                                                    suffix=suffix)
    return (full_pkg_name, version, pypi_url, md5_digest)

def pypi_url(pkg_name, version=None, suffix=None):
    """
    Get download url.
    If version=None, latest release available from PyPI will be used.
    If suffix=None, file suffix will be determined automatically, with the
    preferred format picked if multiple available
    """
    (full_pkg_name, version, url, md5_digest) = \
            _pypi_source_url(pkg_name, version=version, suffix=suffix)
    sys.stdout.write(url)

class _bdist_rpm(bdist_rpm):

    script_options = {
            'prep' : "%setup -qDTn %{module}-%{version}",
            'build' : "CFLAGS='%{optflags}' %{__python} %{py_setup} %{?py_setup_args} build --executable='%{__python} %{?py_shbang_opts}%{?!py_shbang_opts:-s}'",
            'install' : "CFLAGS='%{optflags}' %{__python} %{py_setup} %{?py_setup_args} install -O1 --skip-build --root %{buildroot}",
            'check': None,
            'pre' : None,
            'post' : None,
            'preun' :  None,
            'postun' : None,
    }

    def _make_spec_file(self, url, description, changelog):
        """Generate the text of an RPM spec file and return it as a
        list of strings (one per line).
        """
        sdist = self.reinitialize_command('sdist')

        sdist.warn = _show_warning
        sdist.finalize_options()
        sdist.filelist = FileList()
        sdist.get_file_list()
        manifest = sdist.filelist.files

        build_py = sdist.get_finalized_command('build_py')
        name = self.distribution.get_name()
        version = self.distribution.get_version().replace('-','_')
        release = self.release.replace('-','_')
        summary = _tag('summary', self.distribution.get_description().strip().strip('.'))

        spec_file = [
            '%define\tmodule\t'+name,
            ]
        module = '%{module}'

        spec_file.extend([
            '',
            'Name:\t\t' + _tag('name', 'python-' + module),
            'Version:\t' + version,
            'Release:\t' + release,
            'Summary:\t' + summary,
            'Source0:\t' + url,
            ])

        license = self.distribution.get_license()
        if license == "UNKNOWN":
                classifiers = self.distribution.get_classifiers()
                for classifier in classifiers:
                        values = classifier.split(" :: ")
                        if values[0] == "License":
                                license = values[-1]
        license = _tag('license', license.replace("GPL ", "GPLv").strip())
        spec_file.extend([
            'License:\t' + license,
            'Group:\t\t'+ _tag('group', 'Development/Python'),])# + self.group,])
        if self.distribution.get_url() != 'UNKNOWN':
            spec_file.append('Url:\t\t' + _tag(url, self.distribution.get_url()))

        doc_names = ['README', 'CHANGES','ChangeLog', 'NEWS', 'THANKS',
                'HISTORY', 'AUTHORS', 'BUGS', 'ReleaseNotes', 'DISCLAIMER',
                'TODO', 'TROUBLESHOOTING', 'IDEAS', 'HACKING', 'WISHLIST',
                'CREDITS', 'PROJECTS', 'LEGAL', 'KNOWN_BUGS',
                'MISSING_FEATURES', 'FAQ', 'ANNOUNCE', 'FEATURES', 'WHATSNEW']
        license_names = ['LICENSE', 'COPYRIGHT', 'COPYING']
        common_licenses = glob('/usr/share/common-licenses/*')
        for i in range(len(common_licenses)):
            common_licenses[i] = os.path.basename(common_licenses[i])
        doc_names.extend(license_names)
        doc_suffixes = ('.doc', '.htm', '.txt', '.pdf', '.odt')

        #print(self.distribution.get_command_list())
        self.doc_files = []
        self.license_files = []
        all_files = []
        if self.distribution.data_files:
            all_files.extend(self.distribution.data_files)
        if manifest:
            all_files.extend(manifest)
        if all_files:
            for data_file in all_files:
                done = False
                for doc_name in doc_names:
                    if doc_name.lower() in data_file.lower():
                        # skip licenses already shipped with common-licenses package
                        if doc_name in license_names:
                            if license not in common_licenses:
                                self.license_files.append(data_file)
                                all_files.remove(data_file)
                                done = True
                                break
                        if data_file in doc_names or data_file.endswith(".md"):
                            self.doc_files.append(data_file)
                            all_files.remove(data_file)
                            done = True
                            break
                if done:
                    continue
                for doc_suffix in doc_suffixes:
                    ext = os.path.splitext(data_file.lower())[1]
                    if ext.lower().startswith(doc_suffix.lower()):
                        #self.doc_files.append(data_file)
                        break
        if not self.force_arch:
            # noarch if no extension modules
            if not self.distribution.has_ext_modules():
                spec_file.append('BuildArch:\tnoarch')
        else:
            spec_file.append('BuildArch:\t%s' % self.force_arch)

        for field in ('Provides',
                      'Requires',
                      'Conflicts',
                      'Obsoletes',
                      ):
            val = getattr(self, field.lower())
            if type(val) is list:
                spec_file.append('%s: %s' % (field, " ".join(val)))
            elif val is not None:
                spec_file.append('%s: %s' % (field, val))

        build_requires = []
        if self.distribution.has_ext_modules():
            build_requires.append('python-devel')
        # Ugly, but should mostly work... :p
        if 'setuptools' in str(self.distribution.__dict__) or 'setuptools' in str(sdist.__dict__):
            build_requires.append('python-setuptools')
        if build_requires:
            spec_file.append('BuildRequires:\t' +
                             " ".join(build_requires))

        if self.build_requires:
            spec_file.append('BuildRequires:\t' +
                             " ".join(self.build_requires))

        descr = self.distribution.get_long_description().strip().split("\n")
        if descr[0] == "UNKNOWN":
            if description and not rpm.expandMacro("%{?__firstpass}"):
                rpm.expandMacro(
                "%{warn:Warning: Metadata lacks long_description used for %%description\n"
                "falling back to short description used for summary tag.\n"
                "You might wanna disable auto-generated description with description=False\n"
                "and specify it yourself.\n}")
            descr[0] = self.distribution.get_description().strip()
        elif not description and not rpm.expandMacro("%{?__firstpass}"):
            rpm.expandMacro(
                "%{warn:Warning: Not using long_description for %%description despite "
                "being provided by metadata.\n}")

        if descr[0][-1] != ".":
            descr[0] += "."

        for i in range(0,len(descr)):
            descr[i] = textwrap.fill(descr[i], 79)

        self.description = descr

        spec_file.extend([
            '',
            '%description',
            "\n".join(descr)
            ])


        # insert contents of files

        # XXX this is kind of misleading: user-supplied options are files
        # that we open and interpolate into the spec file, but the defaults
        # are just text that we drop in as-is.  Hmmm.


        if not rpm.expandMacro("%{?__firstpass}"):
            if 'test_suite' in self.distribution.__dict__ and self.distribution.test_suite:
                self.script_options["check"] = "%{__python} setup.py test"

            if rpm.expandMacro("%{python_version}") >= "3.0":
                smp = rpm.expandMacro("%{_smp_mflags}")
                # unlimited number of jobs not supported
                if smp == "-j":
                    smp += "128"
                self.script_options["build"] = self.script_options["build"].replace("build ", "build %s "% smp)

            autopatch = rpm.expandMacro("%autopatch").lstrip().rstrip().replace("\n\n", "\n")
            if autopatch:
                self.script_options["prep"] += "\n" + autopatch

        for script in ('prep', 'build', 'install', 'check', 'pre', 'post', 'preun', 'postun'):
            # Insert contents of file referred to, if no file is referred to
            # use 'default' as contents of script
            val = self.script_options[script]
            if val:
                spec_file.extend([
                    '',
                    '%' + script,
                    val])


        self.files = []
        for license_file in self.license_files:
            self.files.append('%license ' + license_file)
        for doc_file in self.doc_files:
            self.files.append('%doc ' + doc_file)

        if self.distribution.has_ext_modules():
            site_pkgs = '%{python_sitearch}'
        else:
            site_pkgs = '%{python_sitelib}'
        if self.distribution.has_scripts():
            for script in self.distribution.scripts:
                if type(script) == str:
                    self.files.append(os.path.join('%{_bindir}', os.path.basename(script)))
        site_pkgs_files = []
        if self.distribution.data_files:
            for data_file in self.distribution.data_files:
                site_pkgs_files.append(os.path.join(site_pkgs, data_file))
        if 'entry_points' in self.distribution.__dict__ and self.distribution.entry_points:
            if type(self.distribution.entry_points) is dict:
                for entry_points in self.distribution.entry_points:
                    for entry_point in self.distribution.entry_points[entry_points]:
                        site_pkgs_files.append(os.path.join('%{_bindir}', os.path.basename(entry_point.split('=')[0])))
        if 'py_modules' in self.distribution.__dict__ and self.distribution.py_modules:
            for py_module in self.distribution.py_modules:
                py_module = py_module.replace('.', os.path.sep)
                site_pkgs_files.append(os.path.join(site_pkgs, py_module + '.py*'))
        if 'packages' in self.distribution.__dict__ and self.distribution.packages:
            for package in self.distribution.packages:
                package = package.replace('.', os.path.sep)
                self.files.append('%dir ' + os.path.join(site_pkgs, package))
                site_pkgs_files.append(os.path.join(site_pkgs, package, '*.py*'))
        if self.distribution.has_ext_modules():
            for ext_module in self.distribution.ext_modules:
                ext_module = ext_module.name.replace('.', os.path.sep)
                site_pkgs_files.append(os.path.join(site_pkgs, ext_module + get_config_var('SO')))

        site_pkgs_files.sort()
        for f in site_pkgs_files:
            self.files.append(f)

        self.files.append(os.path.join(site_pkgs, name.replace('-', '_') + '*.egg-info'))

        # files section
        spec_file.extend([
            '',
            '%files',
            ] + self.files)

        if changelog:
            packager = rpm.expandMacro('%{?packager}%{?!packager:Unnamed Loser <foo@bar.cum>}')

            setlocale(LC_TIME, locale="C")
            spec_file.extend([
                    '',
                    '%changelog',
                    '* ' + strftime("%a %b %d %Y", gmtime()) + ' %s %s-%s' % (packager, version, release),
                    '- Initial release',])
            resetlocale()

        return spec_file

def pyspec(module, version=_tag('version', None), release=_tag('release', '1'),
        suffix=None, python=rpm.expandMacro("%{__python}"), description=True,
        prep=True, build=True, install=True, check=True, files=True, changelog=False):
    filename, version, url, md5_digest = _pypi_source_url(module, version, suffix)

    _builddir = rpm.expandMacro("%{_builddir}")
    os.chdir(_builddir)
    builddir = "%s-%s" % (module, version)

    if not rpm.expandMacro("%{?__firstpass}"):
        if os.path.exists(builddir):
            rmtree(builddir)

        filepath = rpm.expandMacro("%{_sourcedir}/" + filename)
        blocksize = 65536
        md5 = hashlib.md5()
        if not (os.path.exists(filepath) and os.path.isfile(filepath) and os.stat(filepath).st_size != 0):
            download = urlopen(url)
            f = open(filepath, "wb")
            buf = download.read(blocksize)
            while len(buf) > 0:
                md5.update(buf)
                f.write(buf)
                buf = download.read(blocksize)
            f.close()
        elif md5_digest:
            f = open(filepath, "rb")
            buf = f.read(blocksize)
            while len(buf) > 0:
                md5.update(buf)
                buf = f.read(blocksize)

        if md5_digest != md5.hexdigest():
            raise Exception("MD5 Sums do not match. Wanted: '%s' Got: '%s'" % (md5_digest, md5.hexdigest()))

        uncompress = Popen(rpm.expandMacro("%{uncompress: " + filepath + "}").split(), stdout=PIPE)
        if filepath.endswith("zip"):
            uncompress.wait()
        else:
            untar = Popen(rpm.expandMacro("%{__tar} -xo").split(), stdin=uncompress.stdout, stdout=PIPE)
            output = untar.communicate()[0]
            untar.wait()

    os.chdir(builddir)

    sys.path.append(os.path.join(_builddir,builddir))
    sys.argv = [rpm.expandMacro("%{__python}"), "setup.py"]
    try:
        dist = distutils.core.run_setup(sys.argv[1], stop_after="config")
    except RuntimeError:
        # some setup.py files has setup() under if __name__ == '__main__',
        # resulting in run_setup() failing due to __name__ == '__builtin__'
        f = open(sys.argv[1], "r")
        setup_py = f.read()
        f.close()

        f = NamedTemporaryFile(mode="w", suffix=".py", prefix="setup", dir='.', delete=True)
        sys.argv[1] = f.name
        f.write(setup_py.replace('"__main__"', '__name__').replace("'__main__'", '__name__'))
        f.flush()
        dist = distutils.core.run_setup(sys.argv[1], stop_after="config")
        f.close()

    p = Popen([python, "setup.py", "egg_info"], stdout=PIPE, stderr=PIPE)
    output = p.communicate()[0]
    rc = p.wait()

    egginfo = "PKG-INFO"
    if rc == 0:
        for line in output.split("\n"):
            if ".egg-info/PKG-INFO" in line:
                for text in line.split():
                    if os.path.exists(text):
                        egginfo = text
                        break

    distmeta = distutils.dist.DistributionMetadata(path=egginfo)
    distmeta.keywords = {"name" : module, "version" : version}
    dist.version = version
    dist.release = release
    dist.script_name = "setup.py"

    dist.metadata = distmeta

    for basename in dist.metadata._METHOD_BASENAMES:
        method_name = "get_" + basename
        setattr(dist, method_name, getattr(dist.metadata, method_name))

    specdist = _bdist_rpm(dist)
    specdist.spec_only = True
    specdist.initialize_options()
    specdist.finalize_options()
    specdist.finalize_package_data()
    specdist.distribution = dist
    specfile = specdist._make_spec_file(url, description=description, changelog=changelog)

    lines = "\n".join(specfile)
    tmp = NamedTemporaryFile(mode="w", suffix=".spec", prefix=module, dir=rpm.expandMacro("%{_tmppath}"), delete=True)
    tmp.write(lines)
    tmp.flush()

    rpm.addMacro("__firstpass", "1")

    spec = rpm.spec(tmp.name)
    tmp.close()
    output = ""
    parsed = spec.parsed

    # XXX: %_specfile would make it possible to automatically detect without
    #      having to pass individual arguments for each section...
    if not description:
        parsed = parsed.replace("\n%%description\n%s\n" % str("\n".join(specdist.description)),"")

    if not prep:
        parsed = parsed.replace("\n%%prep\n%s\n" % rpm.expandMacro(specdist.script_options["prep"]), "")

    if not build:
        parsed = parsed.replace("\n%%build\n%s\n" % rpm.expandMacro(specdist.script_options["build"]), "")

    if not install:
        parsed = parsed.replace("\n%%install\n%s\n" % rpm.expandMacro(specdist.script_options["install"]), "")

    if not check:
        if specdist.script_options["check"]:
            parsed = parsed.replace("\n%%check\n%s\n" % rpm.expandMacro(specdist.script_options["check"]), "")

    if not files:
        parsed = parsed.replace("\n%%files\n%s" % rpm.expandMacro(str("\n".join(specdist.files))), "")

    lines = parsed.split("\n")
    emptyheader = True
    preamble = True
    for line in lines:
        if emptyheader:
            if len(line) > 0:
                emptyheader = False
            else:
                continue

        if line.startswith('%'):
            preamble = False
        tag = line.split(':')
        if not (preamble and len(tag) > 1 and tag[0].lower() in _definedTags \
                and _definedTags[tag[0].lower()]):
            output += "%s\n" % line

    sys.stdout.write(output.rstrip("\n"))

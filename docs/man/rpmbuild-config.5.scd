RPMBUILD-CONFIG(5)

# NAME
*rpmbuild-config* - rpmbuild macro configuration

# SYNOPSIS
_NAME_ _VALUE_

# DESCRIPTION
The primary configuration mechanism in *rpmbuild* is *rpm-macros*(7).
See *rpm-config*(5) for the description of the general mechanism,
this manual only describes the configurables affecting *rpmbuild*
operation.

# CONFIGURATION
The following configurables are supported for the *rpm* build operation,
typically invoked through the *rpmbuild* command.

## Locations and filenames

*%\_builddir* _DIRECTORY_
	The directory where the per-package build directories will be placed
	when building.

*%\_fileattrsdir* _DIRECTORY_
	The directory where the file classifier configuration is located.

*%\_rpmdir* _DIRECTORY_
	The directory where newly built binary packages will be written.

*%\_rpmfilename* _TEMPLATE_
	The output binary package filename query format used when building
	binary packages, as described by *rpm-queryformat*(7). The filename is
	considered relative to *%\_rpmdir*.

*%\_sourcedir* _DIRECTORY_
	The directory where the sources and patches are found when building.

*%\_srcrpmdir* _DIRECTORY_
	The directory where newly built source packages will be written.

*%\_topdir* _DIRECTORY_
	The toplevel directory under which the package build directories
	reside by default.

## Informational tags
These are a special group of macros that if defined, behave as if the
corresponding tag had been used in a specfile. This allows changing
the values without modifying thousands of specfiles.

*%bugurl* _URL_
	Configurable bug URL, same as BugURL: tag in a specfile.
	If set, should point to a website for searching for existing
	and filing new issues for this package.

*%distribution* _STRING_
	Configurable distribution informationsame as Distribution: tag in a
	specfile.

*%disturl* _URL_
	Configurable distribution URL, same as DistURL: tag in a specfile.

*%disttag* _STRING_
	Configurable distribution tag, same as DistTag: tag in a specfile.

*%modularitylabel* _STRING_
	Configurable modularity label, same as ModularityLabel: in a specfile.
	Obsolete.

*%packager* _STRING_
	Configurable packager information, same as Packager: in a specfile.

*%translationurl* _URL_
	Configurable translation URL, same as TranslationURL: in a specfile.

*%upstreamreleases* _URL_
	Configurable URL for checking for newer upstream releases, same as
	UpstreamReleases: in a specfile.

*%vendor* _STRING_
	Configurable vendor information, same as Vendor: in a specfile.

## Build operation
These settings affect various aspects of the build and can cause a build
to fail or succeed, but have no direct impact on the produced packages.

*%\_build_pkgcheck* _EXECUTABLE_
	A program to call for each successfully built and written binary
	package, such as *rpmlint*.
	The path to the package is passed to the program as a command-line
	argument.

*%\_build_pkgcheck_set* _EXECUTABLE_
	A program to call for the whole binary package set after a successfully
	build, such as *rpmlint*.
	The paths to the package set are passed to the program as
	command-line arguments.

*%\_build_pkgcheck_srpm* _EXECUTABLE_
	A program to call for each successfully built and written source
	package, such as *rpmlint*.
	The path to the package is passed to the program as a command-line
	argument.

*%\_default_patch_flags* _STRING_
	Set of default options on all *%patch* applications.

*%\_default_patch_fuzz* _NUMBER_
	Default fuzz level for patch application in spec file.
	See *patch*(1) for details.

*%\_smp_ncpus_max* _NUMBER_
	A hard limit for maximum number of CPU's to use in parallel
	during a package build. Zero means unlimited.

*%\_smp_nthread_max* _NUMBER_
	A hard limit for maximum number of threads to use in parallel
	during a package build. Zero means unlimited.

*%\_smp_tasksize_proc* _NUMBER_
	Assumed task size of build processes (during compilation).
	Used for tuning the amount of parallelism based on available memory.

*%\_smp_tasksize_thread* _NUMBER_
	Assumed task size of threads (during package generation).
	Used for tuning the amount of parallelism based on available memory.

## Package generation
These settings affect various aspects of the produced binary and source
packages.

*%build_mtime_policy* _POLICY_
	Define how file timestamps are handled in built packages. Clamping
	means ensuring the value does not exceed the threshold value.
	- *clamp_to_buildtime*: Clamp file timestamps to package buildtime.
	- *clamp_to_source_date_epoch*: Clamp file timestamps to the
	  *SOURCE_DATE_EPOCH* environment variable.

*%source_date_epoch_from_changelog* _BOOLEAN_
	Whether to set the *SOURCE_DATE_EPOCH* environment variable from
	the timestamp of the topmost *%changelog* entry.

*%use_source_date_epoch_as_buildtime* _BOOLEAN_
	Whether to use the *SOURCE_DATE_EPOCH* environment variable value
	as the buildtime of packages.

*%\_binary_filedigest_algorithm* _HASHALGO_
	The algorithm to use for generating per-file checksums (aka digests)
	of binary packages.  See *rpm-config*(5) for the supported values.

*%\_binary_payload* _IOFLAGS_
	The IO method and compression to use for generating the payload
	of binary packages. See *rpm-payloadflags*(7).

*%\_buildhost* _HOSTNAME_
	Use _HOSTNAME_ as the package buildhost instead of acquiring
	the value from *gethostname*(2).

*%\_buildtime* _TIMESTAMP_
	Use _TIMESTAMP_ as the package buildtime instead of acquiring
	the value from *time*(2).

*%\_changelog_trimage* _TIMESTAMP_
	Maximum age of preserved changelog entries in binary packages,
	relative to newest existing entry. Unix timestamp format.
	The value of zero disables trimming by age.

*%\_changelog_trimtime* _TIMESTAMP_
	An alternative strategy for changelog trimming: trim all changelog
	entries older than the specified timestamp. Unix timestamp format.
	The value of zero disables trimming by time.

*%\_docdir_fmt* _TEMPLATE_
	A template for the per-package documentation directory name,
	ie. the files packaged with *%doc*.

*%\_\_docdir_path* _PATH_
	A colon separated list of directories whose contents should be
	always considered as documentation.

*%\_openpgp_autosign_id* _KEYID_
	The OpenPGP key id or fingerprint to use for automatically signing
	packages after a successful build. See also *rpmsign*(1).

*%\_rpmformat* _VERSION_
	The RPM package format to produce. Supported values are:
	- *4*: RPM v4 format
	- *6*: RPM v6 format

*%\_source_filedigest_algorithm* _HASHALGO_
	The algorithm to use for generating per-file checksums (aka digests)
	of source packages. See *rpm-config*(5) for the supported values.

*%\_source_payload*
	The IO method and compression to use for generating the payload
	of source packages. See *rpm-payloadflags*(7).

*%\_\_gpg_reserved_space* _NUMBER_
	The number of bytes to reserve for signatures in the signature header.
	This reserve helps to speed up package signing significantly.

## Debuginfo generation
Debuginfo sub-packages contain information necessary to debug otherwise
release-specification builds with a debugger. These settings determine
whether debuginfo packages are generated and various aspects of the
generated debugging information.

*%\_build_id_links* _MODE_
	Defines how and if build_id links are generated for ELF files.
	The following settings are supported:
	- *none*: No build_id links are generated.
	- *alldebug*: Generate rpm < 4.14 style build_id links, ie everything
	  in the -debuginfo package.
	- *separate*: Generate build_id links in binary packages.
	- *compat*: Same as *separate* but with a compatibility link(s)
	  in the -debuginfo packages.

*%\_debuginfo_subpackages* _BOOLEAN_
	Whether rpm should create separate debuginfo packages for each
	subpackage.

*%\_debugsources_packages* _BOOLEAN_
	Whether rpm should put debug source files into their own subpackage.

*%\_enable_debug_packages* _BOOLEAN_
	Whether rpm should generate debuginfo subpackages.

*%\_include_gdb_index* _BOOLEAN_
	Include a .gdb_index section in the .debug files.
	Requires *%\_enable_debug_packages* to be enabled and
	*gdb-add-index*(1) to be installed.

*%\_include_minidebuginfo* _BOOLEAN_
	Include minimal debug information in build binaries.
	Requires *%\_enable_debug_packages* to be enabled.

*%\_no_recompute_build_ids* _BOOLEAN_
	Disable recomputation of build-ids.
	Cannot be used with *%\_unique_build_ids*.

*%\_unique_build_ids* _BOOLEAN_
	Whether build-ids should be made unique between package
	version/releases when generating debuginfo packages.

*%\_unique_debug_names* _BOOLEAN_
	Whether .debug files should be made unique between package version,
	release and architecture. Requires *%\_unique_build_ids* to be enabled.

*%\_unique_debug_srcs* _BOOLEAN_
	Whether the /usr/debug/src/<package> directories should be unique
	between package version, release and architecture.

*%\_\_find_debuginfo* _EXECUTABLE_
	The location of the debuginfo helper executable.

## Legacy compatibility
These settings deal with several decades of backwards compatibility. Most
of them determine whether a particular issues is considered an error or
just a warning.
Sometimes also useful for temporarily working around issues while packaging.

*%\_binaries_in_noarch_packages_terminate_build* _BOOLEAN_
	Whether ELF binaries in noarch packages should terminate a build.

*%\_duplicate_files_terminate_build* _BOOLEAN_
	Whether duplicate files in *%files* section should terminate a build.

*%\_empty_manifest_terminate_build* _BOOLEAN_
	Whether an empty *%files* manifest file should terminate a build.

*%\_\_find_provides* _EXECUTABLE_
	The executable to use for rpm 3.x style provides generation.
	Do not use. Deprecated and scheduled for removal.

*%\_\_find_requires* _EXECUTABLE_
	The executable to use for rpm 3.x style requires generation.
	Do not use. Deprecated and scheduled for removal.

*%\_missing_doc_files_terminate_build* _BOOLEAN_
	Whether missing %doc files in the build directory should terminate
	a build.

*%\_unpackaged_files_terminate_build* _BOOLEAN_
	Whether unpackaged files in a build root should terminate a build.

*%\_nonzero_exit_pkgcheck_terminate_build* _BOOLEAN_
	Whether the build of packages should fail if package checker
	(if defined) returns an error? See the *%\_build_pkgcheck_\** macros.

*%\_use_internal_dependency_generator* _BOOLEAN_
	If enabled, use rpm 3.x style dependency generation.
	Do not use. Deprecated and scheduled for removal.

*%\_use_weak_usergroup_deps* _BOOLEAN_
	If enabled, dilute user() and group() requires into recommends.
	Useful when transitioning to sysusers.d based user/group management.

*%\_wrong_version_format_terminate_build* _BOOLEAN_
	Whether invalid version format in dependencies etc should
	terminate a build.

# SEE ALSO
*rpmbuild*(1), *rpm-common*(8), *rpm-macrofile*(5),
*rpm-rpmrc*(5), *rpm-config*(5), *rpm-payloadflags*(7)
*rpm-macros*(7)

---
layout: default
title: rpm.org - Package Build Process
---
# Package Build Process

## Overview

* Unpack srpm/tar (optional)
* Parse the [spec](spec.md)
  * If  buildarch detected parse spec multiple times - once for each arch with `_target_cpu` macro set
  * Build will iterate over all the spec variants and build multiple versions
* Execute internal `%mkbuilddir` script to create `%builddir`
* Check static build requires
* Execute present [build scriptlets](spec.md#build-scriptlets)
  * `%prep`
  * `%generate_buildrequires`
    * re-check build requires - stop build on errors
  * `%conf`
  * `%build`
  * `%install`
  * Read [dynamic spec parts](dynamic_specs.md)
  * `%check`
* Process files
  * Turn [%files](spec.md#files-section) lines into actual files (evaluate globs)
    * Read from -f param
  * Run [file classifier](dependency_generators.md#file-attributes)
  * Run [dependency generators](dependency_generators.md)
  * Check packaged files against install root 
* Create packages
* Clean up
  * %clean

## Tunables

The build process can be tuned and tweaked in lots of ways via macro
configurables.

### Parallelism

Various aspects of run in parallel on SMP systems. Rpm estimates the
number of processes/threads to use based on detected resources and
normally there's no need to touch these but sometimes it's necessary
to work around buggy make-files, tune the parameters for an unusual
compilation resource usage or such.

Macro name              | Description
------------------------|------------
`%_smp_ncpus_max`       | Maximum number of CPUs
`%_smp_nthreads_max`    | Maximum number of threads
`%_smp_build_ncpus`     | Number of CPUs to use for building
`%_smp_build_nthreads`  | Number of threads to use for building
`%_smp_tasksize_proc`   | Assumed task size of a process
`%_smp_tasksize_thread` | Assumed task size of a thread

### Package generation

Macro name                      | Description
--------------------------------|------------
`%_binary_filedigest_algorithm` | Compression algorithm of binary packages
`%_source_filedigest_algorithm` | Compression algorithm of source packages
`%_changelog_trimage`           | Maximum age of `%changelog` entries
`%_changelog_trimtime`          | Cut-off date of `%changelog` entries

### Debuginfo

Macro name                 | Description
---------------------------|-------------
`%_enable_debug_packages`  | Enable generation of debuginfo packages
`%_include_minidebuginfo`  | Include minimal debuginfo in binaries directly
`%_include_gdb_index`      | Include a `.gdb_index` section to `.debug` files
`%_build_id_links`         | BuildID link generation: `none`/`alldebug`/`separate`/`compat`
`%_unique_build_ids`       | Use version info to seed the BuildIDs
`%_unique_debug_names`     | Use version info to differentiate .debug files
`%_unique_debug_srcs`      | Use version info to differentiate debug sources
`%_debugsource_packages`   | Generate a separate sub-package for debug sources
`%_debuginfo_subpackages`  | Generate debuginfo packages per sub-package
`%_no_recompute_build_ids` | Preserve pre-existing BuildIDs

### Packaging hygiene

Various packaging sanity check overrides for dealing with legacy content.

Macro name                                      | Description
------------------------------------------------|------------
`%_unpackaged_files_terminate_build`            | Excess files in buildroot terminates build
`%_duplicate_files_terminate_build`             | Duplicates in `%files` terminates build
`%_missing_doc_files_terminate_build`           | Missing `%doc` files terminates build
`%_empty_manifest_terminate_build`              | Empty `%files -f` manifest terminates build
`%_binaries_in_noarch_packages_terminate_build` | ELF binaries in noarch packages terminates build
`%_invalid_encoding_terminates_build`           | Non-UTF8 encoding in package data terminates build
`%_wrong_version_format_terminate_build`        | Invalid version format in dependencies terminates build
`%_missing_build_ids_terminate_build`           | ELF files without BuildIDs terminates builds (if debuginfo creation enabled)
`%_nonzero_exit_pkgcheck_terminate_build`       | Package check program failing terminates build
`%_build_pkgcheck`                              | Progam to run on each generated binary package
`%_build_pkcheck_set`                           | Program to run on the generated binary package set

### Reproducability

Macro name                            | Description
--------------------------------------|-----------
`%source_date_epoch_from_changelog`   | Set `SOURCE_DATE_EPOCH` from latest `%changelog` entry
`%use_source_date_epoch_as_buildtime` | Set package BuildTime to `SOURCE_DATE_EPOCH`
`%clamp_mtime_to_source_date_epoch`   | Ensure file timestamps are not newer than `SOURCE_DATE_EPOCH` (deprecated)
`%build_mtime_policy`                 | Defines file timestamp handling

The supported values for `%build_mtime_policy` are:

* (unset)
  leave the mtimes as is
* `clamp_to_source_date_epoch`
  clamp the mtimes so that they are not bigger than the `SOURCE_DATE_EPOCH`
* `clamp_to_buildtime`
  clamp the mtimes so that they are not bigger than the package build time (i.e. the time that
  is used for the `BUILDTIME` tag)

### Vendor defaults

These can be used to automatically populate some common spec tags without
changing the spec itself.

Macro name      | Description
----------------|------------
`%bugurl`       | BugURL tag
`%distribution` | Distribution tag
`%disturl`      | DistURL tag
`%disttag`      | DistTag tag
`%packager`     | Packager tag
`%vendor`       | Vendor tag

### Dependencies

See [dependency generators](dependency_generators.md).

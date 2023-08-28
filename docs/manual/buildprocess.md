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

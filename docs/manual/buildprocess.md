---
layout: default
title: rpm.org - Package Build Process
---
# Package Build Process

* Unpack srpm/tar (optional)
* [Parse spec](https://github.com/rpm-software-management/rpm/blob/master/build/parseSpec.c)  - see also [build/parse*.c](https://github.com/rpm-software-management/rpm/blob/master/build/)
  * If  buildarch detected parse spec multiple times - once for each arch with `_target_cpu` macro set
  * Build will iterate over all the spec variants and build multiple versions
* Check static build requires
* Execute build scripts (see [doScript()](https://github.com/rpm-software-management/rpm/blob/master/build/build.c#L95)
  * %prep
  * %generate_buildrequires if present
    * re-check build requires - stop build on errors
  * %build
  * %install
  * %check - if present
 * Process files
   * Turn %files lines into actual files (evaluate globs)
     * Read from -f param
   * Run [file classifiers](https://github.com/rpm-software-management/rpm/blob/master/build/rpmfc.c) 
   * Generate automatic dependencies
   * Check packaged files against install root 
 * Create packages
 * %clean
 * Clean up

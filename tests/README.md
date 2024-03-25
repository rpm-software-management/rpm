# Tests

## Introduction

This test-suite builds an OCI image matching the host OS with a fresh RPM
installation layered on top (with `make install`), ready to be tested in a
container, either automatically by a script (via `make check`) or in an
interactive shell (via `make shell`).

## Prerequisites

To use this test-suite, you need either of:

1. [Podman](https://github.com/containers/podman/)
2. [Docker](https://github.com/docker/)

> [!IMPORTANT]
> CMake integration (*native* mode) is currently only available on **Fedora
> Linux** hosts with **Podman** installed, on other systems a fresh build of
> RPM will be done as part of a Fedora-based image (*non-native* mode).

## Running all tests

To build the image (if not built yet) and perform automated testing, run:

    make check

The number of tests performed depends on features enabled at configure time, at
least the CMake option `ENABLE_PYTHON=<ON|OFF>`.  See also the
[INSTALL](../INSTALL) file for more information.

If the test-suite is configured in native mode, it may sometimes be useful to
verify that everything passes in non-native mode too as that's what our CI
uses.  To do that, run:

    make ci

## Selective testing

To run *single tests*, you can run the commands:

    make check TESTOPTS="$NNN $MMM"

where _NNN_ and _MMM_ are the numbers of the tests to be run.

You can also select *tests by keywords* used in their description by using the
command:

     make check TESTOPTS="-k $KEYWORD"

Use multiple `-k` parameters to have tests with different keywords run.  Use
`-k $KEYWORD1,$KEYWORD2` to execute tests matching both _KEYWORD1_ and
_KEYWORD2_.

For all available options, see the output of the command:

	./rpmtests --help

By default, tests are executed in parallel using all available cores, pass a
specific `-jN` value to limit.

## Interactive testing

To drop into an interactive GNU Autotest shell, run:

    make atshell

This is like a singular, empty `RPMTEST_CHECK` with a shell running in it and a
writable tree available at the path stored in `$RPMTEST`.  From this shell, you
can run the same commands as a normal test would, such as `runroot rpm`.  This
can be used to quickly prototype (or debug) a test.

You can also drop straight into the `$RPMTEST` container like so:

    make shell

This is just a shorthand for `make atshell` followed by `runroot_other bash`.

To factory-reset the `$RPMTEST` tree, run:

    make reset

To only build the OCI image, use:

    make tree

You can also just use Podman or Docker directly to manage containers (the image
is named `rpm` when built).  For example, to run a shell:

    podman run -it rpm

## Understanding the tests

### Optimizations

Since the test-suite consists of several hundreds of tests and is meant to be
run repeatedly during development, it's optimized for speed.

Instead of running each test in a separate OCI container, *all* the tests run
in a single, immutable OCI container.  Mutable, disposable *snapshots* of its
root filesystem are then created on demand for tests that need write access to
it (such as those installing or removing packages).  These tests perform the
desired operation through a lightweight, nested container with the snapshot set
as its root directory.

This reduces the number of containers needed while still ensuring that each
test operates in a pristine environment, without affecting any other test or
the test logic itself.  Snapshots are created with
[OverlayFS](https://docs.kernel.org/filesystems/overlayfs.html) and nested
containers with [Bubblewrap](https://github.com/containers/bubblewrap) which
heavily reduces the startup time of each test when compared to a full-blown OCI
runtime.

Since this is done *from* the immutable OCI container, that container needs to
run as `--privileged` in order to allow the creation of nested namespaces.

### Layout

The OCI image is built from a `Dockerfile.<osname>` where `<osname>` is the
host OS name (e.g. `fedora`).  Currently, we only have `Dockerfile.fedora`,
contributions for other distros are welcome.  The `Dockerfile` symlink points
to the file that should be used in non-native mode (see above).

The test script that's executed in the OCI container is written in [GNU
Autotest](https://www.gnu.org/savannah-checkouts/gnu/autoconf/manual/autoconf-2.71/autoconf.html#Using-Autotest)
and is compiled from the `rpmtests.at` file.  The individual tests are split
into several files with the `.at` suffix, each covering a specific area within
RPM, such as package building, transactions or configuration, which are then
included by the top-level `rpmtests.at` script.

## Writing tests

For the typical structure of a single test, consult GNU Autotest's
[documentation](https://www.gnu.org/savannah-checkouts/gnu/autoconf/manual/autoconf-2.71/autoconf.html#Writing-Testsuites)
as well as the existing tests.  Below are the specifics of RPM's test-suite:

* Use `RPMTEST_CHECK` instead of `AT_CHECK`
* Use `RPMTEST_CLEANUP` instead of `AT_CLEANUP`
* Use `RPMTEST_SETUP` or `RPMDB_INIT` to create a mutable snapshot (optional)
    * The absolute path to the snapshot's root is stored in the `$RPMTEST`
      environment variable, modify the directory tree as you wish
    * To run RPM inside the snapshot, use the `runroot` prefix, e.g. `runroot
      rpm ...`
    * To run any other binary inside the snapshot, use `runroot_other` instead
    * By default, `runroot` and `runroot_other` will operate in a clean
      environment with only a handful of variables preset.  To pass your own
      variable(s), use `--setenv` (once for each variable), e.g. `runroot
      --setenv FOO "foo" rpm ...`
* If no snapshot was used, just call the RPM binaries normally
* Store any working files in the current directory (it's always writable)

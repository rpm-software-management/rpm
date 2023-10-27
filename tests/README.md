To run these tests, you need either of:

1.    [Podman](https://github.com/containers/podman)
1.    [Docker](https://github.com/docker)

The test-suite runs in a Podman/Docker container and supports two modes:

1. **Native** - Exercises the local build of RPM against a minimal image of the
   Linux distribution running on the host.  Currently, only Fedora Linux is
   supported.  This mode is optimized for local RPM development and requires
   Podman.

1. **Standalone** - Exercises a custom, fresh build of RPM (including cmake
   configuration) against a Fedora-based image.  This mode is optimized for
   portability (CI environment) and works with both Podman and Docker.

The mode is selected automatically during cmake configuration based on the host
distribution and the container engine installed, with native mode being
preferred whenever possible, and is reported in the cmake output as follows:

    -- Using mktree backend: oci (native: <yes|no>)

Then run the command

    make check

The number of tests performed depends on features enabled at configure time,
at least `--with-`/`--without-lua` and `--enable-`/`--disable-python`.
See also the [INSTALL](../INSTALL) file for more information.

To run *single tests*, you can run the commands:

    make check TESTOPTS="$NNN $MMM"

where _NNN_ and _MMM_ are the numbers of the tests to be run.

You can also select *tests by keywords* used in their description by using the command:

     make check TESTOPTS="-k $KEYWORD"

Use multiple `-k` parameters to have tests with different keywords run.
Use `-k $KEYWORD1,$KEYWORD2` to execute tests matching both _KEYWORD1_ and _KEYWORD2_.

For all available options, see the output of the command:

	./rpmtests --help

By default, tests are executed in parallel using all available cores, pass
a specific -jN value to limit.

To drop into an interactive Autotest-like shell, run:

    make atshell

This is like a singular, empty `RPMTEST_CHECK()` with a shell running in it and
a writable tree available at the path stored in `$RPMTEST`.  From this shell,
you can run the same commands as a normal test would, such as `runroot rpm`.
This can be used to quickly prototype (or debug) a test.

You can also drop straight into the `$RPMTEST` container like so:

    make shell

This is just a shorthand for `make atshell` followed by `runroot_other bash`.

To factory-reset the `$RPMTEST` container, run:

    make reset

Sometimes, if the test-suite is configured in native mode, you may want to try
it in standalone mode before submitting a pull request since that's how our CI
is set up.  You can do that with:

    make ci

This target accepts the same `TESTOPTS` variable as `make check`.

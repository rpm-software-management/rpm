To run these tests, you need at least these dependencies on the host:

1.    [fakechroot](https://github.com/dex4er/fakechroot/wiki)
1.    [gdb](https://www.gnu.org/software/gdb/)
1.    [gnupg](https://www.gnupg.org/) >= 2.0

Then run the command

    make check

The number of tests performed depends on features enabled at configure time,
at least `--with-`/`--without-lua` and `--enable-`/`--disable-python`.
See also the [INSTALL](../INSTALL) file for more information.

To run *single tests*, you can run the commands:

    make populate_testing && ./rpmtests $NNN $MMM ...

where _NNN_ and _MMM_ are the numbers of the tests to be run.

To get the *list of available tests*, use the command:

      ./rpmtests -l

You can also select *tests by keywords* used in their description by using the command:

     ./rpmtests -k $KEYWORD

Use multiple `-k` parameters to have tests with different keywords run.
Use `-k $KEYWORD1,$KEYWORD2` to execute tests matching both _KEYWORD1_ and _KEYWORD2_.

See the output of the command:

	./rpmtests --help

for more options. But be aware some (like `-j`) need not work properly with
the test suite unless a very restricted set of test is run.

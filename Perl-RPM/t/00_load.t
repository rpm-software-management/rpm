#!/usr/bin/perl

# Verify that the indivual modules will load

@MODULES = qw(RPM RPM::Constants RPM::Database RPM::Header RPM::Error);

printf "1..%d\n", scalar(@MODULES);

$count = 0;
for (@MODULES)
{
    eval "use $_";

    printf "%sok %d\n", ($@) ? 'not ' : '', ++$count;
}

exit 0;

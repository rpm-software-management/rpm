#!/usr/bin/perl

# Verify that the indivual modules will load

@MODULES = qw(RPM RPM::Constants RPM::Database RPM::Header);

printf "1..%d\n", scalar(@MODULES);

for $idx (0 .. $#MODULES)
{
    eval "use $MODULES[$idx]";

    printf "%sok %d\n", ($@) ? 'not ' : '', $idx + 1;
}

exit 0;

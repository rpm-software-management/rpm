# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test;
use strict;
BEGIN { plan tests => 6 };
use RPM2;
ok(1); # If we made it this far, we're ok.

#########################

# Insert your test code below, the Test module is use()ed here so read
# its man page ( perldoc Test ) for help writing this test script.

ok(RPM2::rpmvercmp("1.0", "1.1") == -1);
ok(RPM2::rpmvercmp("1.1", "1.0") == 1);
ok(RPM2::rpmvercmp("1.0", "1.0") == 0);
ok(RPM2::rpmvercmp("1.a", "1.0") == RPM2::rpmvercmp("1.0", "1.a"));

my $db = RPM2->open_rpm_db(-read_only => 1);
ok(defined $db);

while(1) {
  my @h;
  push @h, [ RPM2->open_package_file($_) ]
    foreach <~/rhn/RPMS/*.rpm>;

  print $_->[0]->as_nvre, "\n" foreach @h;
}

exit;

my $i = $db->iterator();
while (my $h = $i->next) {
  my $epoch = $h->tag('epoch');
  my $epoch_str = '';
  $epoch_str = "$epoch:" if defined $epoch;

  print $epoch_str . join("-", map { $h->tag($_) } qw/name version release/);
  my @files = $h->files;
  my $n = scalar @files;
  print " ($n files)";
  print "\n";
}

$db->close_rpm_db();

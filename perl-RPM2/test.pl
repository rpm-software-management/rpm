# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';
# comment to test checkin

use Test;
use strict;
BEGIN { plan tests => 26 };
use RPM2;
ok(1); # If we made it this far, we're ok.

#########################

# Insert your test code below, the Test module is use()ed here so read
# its man page ( perldoc Test ) for help writing this test script.

ok(RPM2::rpmvercmp("1.0", "1.1") == -1);
ok(RPM2::rpmvercmp("1.1", "1.0") == 1);
ok(RPM2::rpmvercmp("1.0", "1.0") == 0);

# this is a bug case in rpmvervmp; good one for testing
ok(RPM2::rpmvercmp("1.a", "1.0") == RPM2::rpmvercmp("1.0", "1.a"));

my $db = RPM2->open_rpm_db();
ok(defined $db);

my @pkg;
my $i = $db->find_all_iter();

ok($i);
while (my $pkg = $i->next) {
  push @pkg, $pkg;
}
ok(@pkg);
ok($pkg[0]->name);

@pkg = ();

$i = $db->find_by_name_iter("kernel");
ok($i);
while (my $pkg = $i->next) {
  push @pkg, $pkg;
}
if (@pkg) {
  ok($pkg[0]->name);
}

@pkg = ();

$i = $db->find_by_provides_iter("kernel");
ok($i);
while (my $pkg = $i->next) {
  push @pkg, $pkg;
}
if (@pkg) {
  ok($pkg[0]->name);
}

@pkg = ();
foreach my $pkg ($db->find_by_file("/bin/sh")) {
  push @pkg, $pkg;
}
if (@pkg) {
  ok($pkg[0]->name);
}

@pkg = ();
foreach my $pkg ($db->find_by_requires("/bin/bash")) {
  push @pkg, $pkg;
}
if (@pkg) {
  ok($pkg[0]->name);
}

my $pkg = RPM2->open_package("test-rpm-1.0-1.noarch.rpm");
ok($pkg);
ok($pkg->name eq 'test-rpm');
ok(!$pkg->is_source_package);

$pkg = RPM2->open_package("test-rpm-1.0-1.src.rpm");
ok($pkg);
ok($pkg->name eq 'test-rpm');
ok($pkg->is_source_package);

my $pkg2 = RPM2->open_package("test-rpm-1.0-1.noarch.rpm");
ok($pkg->compare($pkg2) == 0);
ok(($pkg <=> $pkg2) == 0);
ok(!($pkg < $pkg2));
ok(!($pkg > $pkg2));

# another rpm, handily provided by the rpmdb-redhat package
my $other_rpm_dir = "/usr/lib/rpmdb/i386-redhat-linux/redhat";
if (-d $other_rpm_dir) {
  my $db2 = RPM2->open_rpm_db(-path => $other_rpm_dir);
  ok(defined $db2);
}
else {
  print "Install the rpmdb-redhat package to test two simultaneous open databases\n";
  ok(1);
}


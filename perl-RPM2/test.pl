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

my $db = RPM2->open_rpm_db;
ok(defined $db);

if (1) {
  my @h;
  push @h, [ RPM2->open_package_file($_) ]
    foreach <~/rhn/bs/6.2/RPMS/*.rpm>;

  print $_->[0]->name, " ", $_->[0]->as_nvre, "\n" foreach @h;
}

#exit;

if (1) {
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
}

my $i = $db->find_all_iter();
print "The following packages are installed (aka, 'rpm -qa'):\n";
while (my $pkg = $i->next) {
  print $pkg->as_nvre, "\n";
}

$i = $db->find_by_name_iter("kernel");
print "The following kernels are installed (aka, 'rpm -q kernel'):\n";
while (my $pkg = $i->next) {
  print $pkg->as_nvre, " ", int($pkg->size()/1024), "k\n";
}

$i = $db->find_by_provides_iter("kernel");
print "The following packages provide 'kernel' (aka, 'rpm -q --whatprovides kernel'):\n";
while (my $pkg = $i->next) {
  print $pkg->as_nvre, " ", int($pkg->size()/1024), "k\n";
}

print "The following packages are installed (aka, 'rpm -qa' once more):\n";
foreach my $pkg ($db->find_by_file("/bin/sh")) {
  print $pkg->as_nvre, "\n";
}

my $pkg = RPM2->open_package_file("/home/cturner/XFree86-4.1.0-15.src.rpm");
print "Package opened: ", $pkg->as_nvre(), ", is source: ", $pkg->is_source_package, "\n";

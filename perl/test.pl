# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..6\n"; }
END {print "not ok 1\n" unless $loaded;}
use rpm;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

my $valid_package = "foo.i386.rpm";

# we should be able to open a valid file
print rpm::Header($valid_package) ? "ok 2" : "not ok 2", "\n";

# we should not be able to read stuff from an invalid file
print rpm::Header("this is not a valid package") ? "not ok 3" : "ok 3", "\n";

# in the test file we have there are exactly 42 headers
print scalar rpm::Header($valid_package)->Tags() == 42 ? "ok 4" : "not ok 4", "\n";

# there are exactly 4 files in the package
print scalar rpm::Header($valid_package)->ItemByName('Filenames') == 4 ? "ok 5" : "not ok 5", "\n";

# item 1000 should be the name of the package
print scalar rpm::Header($valid_package)->ItemByVal(1000) eq "xemacs-extras" ? "ok 6" : "not ok 6", "\n";


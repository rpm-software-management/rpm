use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

my $extra_libs = $ENV{EXTRALIBS};
my $libs = "$extra_libs -lrpm -lrpmdb -lrpmio -lpopt";

# try to be smart about which shared libs we should load.
# use the .la file.  probably linux only.

my $filename = '/usr/lib/librpm.la';
$filename = '/usr/lib64/librpm.la' unless -e $filename;

if (open FH, "<$filename") {
  my ($line, @rest) = grep { /^dependency_libs=/ } <FH>;
  if ($line and not @rest) {
    if ($line =~ /^dependency_libs='(.*)'$/) {
      my $l = $1;
      my @l = split /\s+/, $l;
      @l = grep { /^-l/ } @l;
      $libs = "$libs -lrpm @l";
    }
  }
}

my @defines;
# detect which rpm is running.  ugly but necessary... for now.
if (-e '/usr/include/rpm/rpmts.h') {
  push @defines, '-DRPM2_RPM41';
}
else {
  push @defines, '-DRPM2_RPM40';
}

WriteMakefile(
    'NAME'		=> 'RPM2',
    'VERSION_FROM'	=> 'RPM2.pm', # finds $VERSION
    'PREREQ_PM'		=> {}, # e.g., Module::Name => 1.1
    'LIBS'		=> [ $libs ], # e.g., '-lm'
    'DEFINE'		=> join(" ", @defines), # e.g., '-DHAVE_SOMETHING'
    'INC'		=> '-I/usr/include/rpm', # e.g., '-I/usr/include/other'
    'TYPEMAPS'          => [ 'typemap' ],
    'OPTIMIZE'          => '-g'
);

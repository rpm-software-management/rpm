use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'		=> 'RPM2',
    'VERSION_FROM'	=> 'RPM2.pm', # finds $VERSION
    'PREREQ_PM'		=> {}, # e.g., Module::Name => 1.1
    'LIBS'		=> ['-lpopt -lrpm -lrpmio -lrpmdb'], # e.g., '-lm'
    'DEFINE'		=> '', # e.g., '-DHAVE_SOMETHING'
    'INC'		=> '-I/usr/include/rpm', # e.g., '-I/usr/include/other'
    'TYPEMAPS'          => [ 'typemap' ],
    'OPTIMIZE'          => '-g'
);

use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
	      'NAME'	=> 'rpm',
	      'OBJECT'	=> 'rpm.o constant.o',
	      'VERSION_FROM' => 'rpm.pm', # finds $VERSION
	      'LIBS'	=> ['-lrpm -lz'],   # e.g., '-lm' 
	      'DEFINE'	=> '-Wall',     # e.g., '-DHAVE_SOMETHING' 
	      'INC'	=> '-I/usr/include/rpm',     # e.g., '-I/usr/include/other' 
);

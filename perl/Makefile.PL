# Generated automatically from Makefile.PL.in by configure.
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

WriteMakefile(
	      'NAME'	=> 'rpm',
	      'OBJECT'	=> 'rpm.o constant.o',
	      'VERSION_FROM' => 'rpm.pm',		# finds $VERSION
	      'MAKEFILE'=> 'PMakefile',
	      'LIBS'	=> [' -L/usr/local/lib  -lz -lbz2'],			# e.g., '-lm' 
	      'CCFLAGS' => '-O2 -pg -D_GNU_SOURCE -Wall -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wno-char-subscripts',
	      'OPTIMIZE'=> '-g',
	      'DEFINE'	=> '-Dbool=char -DHAS_BOOL',
	      'INC'	=> ' -I../build -I../lib -I../popt -I/usr/include/rpm',
	      'depend'	=> { 'rpm.c' => 'transaction.xs header.xs db.xs' },
);


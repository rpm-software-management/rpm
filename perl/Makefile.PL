use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

WriteMakefile(
	      'NAME'	=> 'rpm',
	      'OBJECT'	=> 'rpm.o constant.o /usr/lib/libdb1.a',
	      'VERSION_FROM' => 'rpm.pm', # finds $VERSION
	      'LIBS'	=> ['-lrpm -lz -ldb1'],   # e.g., '-lm' 
	      'CCFLAGS' => '-Dbool=char -DHAS_BOOL',
	      'OPTIMIZE'=> '-g',
	      'DEFINE'	=> '-Wall',
	      'INC'	=> '-I/usr/include/rpm',     # e.g., '-I/usr/include/other'
	      'depend'	=> { 'rpm.c' => 'transaction.xs header.xs db.xs' },
);

# $self->{'MAP_PRELIBS'} = ['-lnsl -lndbm -lgdbm -ldb1 -ldl -lm -lc -lposix -lcrypt -lrpm -lz'];


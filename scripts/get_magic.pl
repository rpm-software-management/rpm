#!/usr/bin/perl

# Given a filename on the command line or on stdin this script returns
# the (single) interpreter that is required to run the executable.  We
# need this information to pick the best dependency parser for this
# file.

# Usually this is extracted from the #! line of the file
# but we also handle the various 'exec' tricks that people use to
# start the interpreter via an intermediate shell.  


# These have all been seen on our system or are "recommended" in
# various man pages.

# Examples:

#   #!/bin/sh
#   # the next line restarts using wish \
#   exec wish "$0" "$@"


#   #!/bin/sh -- # -*- perl -*- -p
#     eval 'exec /usr/bin/perl -wS $0 ${1+"$@"}'
#       if $running_under_some_shell;


#   #!/bin/sh -- # -*- perl -*- -p
#   eval '(exit $?0)' && eval 'exec /usr/bin/perl -wS $0 ${1+"$@"}'


#   #!/bin/sh -- # -*- perl -*- -p
#   & eval 'exec /usr/bin/perl -wS $0 $argv:q'
#     if $running_under_some_shell;


#   #! /usr/bin/env python


use File::Basename;

if ("@ARGV") {
  foreach (@ARGV) {
    process_file($_);
  }
} else {
  
  # notice we are passed a list of filenames NOT as common in unix the
  # contents of the file.
  
  foreach (<>) {
    process_file($_);
  }
}

foreach $prog (sort keys %require) {

  $prog=basename($prog);

  # ignore variable interpolation and any program whose name is made
  # up only of non word characters ('<', '&&', etc).

  ( ( $prog != /\$/ ) || ( $prog =~ /^\W+$/ ) ) && 
    next;

  print "exectuable($prog)\n";

}

exit 0;


sub process_file {
  
  my ($file) = @_;
  chomp $file;
  
  my ($version, $magic) = ();
  
  (-f $file) || return ;  

  open(FILE, "<$file")||
    die("$0: Could not open file: '$file' : $!\n");
  
  my $rc = sysread(FILE,$line,1000);

  $rc =~ s/\#.*\n//g;

  # Ignore all parameter substitution.
  # I have no hope of parsing something like: 
  #  exec ${SHELL:-/bin/sh}

  $rc =~ s/\$\{.*\}//g;
  $rc =~ s/echo\s+.*[\n;]//g;
  
  if  ( ($rc > 1) && ($line =~ m/^\#\!\s*/) )  {
    
    if ($line =~ m/\b(exec|env)\s+([\'\"\`\\]+)?([^ \t\n\r]+)/) {
      $require{$3} = 1;
      last;
    }
    
    # strip off extra lines and any arguments
    if ($line =~ m/^\#\!\s*([^ \t\n\r]+)/) {
      $require{$1} = 1;
      last;
    }

  }

  close(FILE) ||
    die("$0: Could not close file: '$file' : $!\n");
  
  return ; 
}

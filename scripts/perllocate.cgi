#!/usr/bin/perl

# perllocate.cgi - a web interface to a perl version of the Unix
# locate command.  This script makes it easy to query the RPM
# repository and find out what packages are availible using Perl5
# Patterns.

# written by Ken Estes kestes@staff.mail.com

use CGI ':standard';
use File::Basename;
use File::stat;



sub usage {

  # If they are asking for help then they are clueless so reset all
  # their parameters for them, in case they are in a bad state.

  param(-name=>'Defaults', -value=>'on');
  my $rpmdiff_version = `perllocate --version`;

  $usage =<<EOF;

  $0          version: $VERSION
  $perllocate_version

This is a web interface into the perllocate command.


EOF
    print pre($usage);

  return ;
}


sub set_static_vars {

# This functions sets all the static variables which are often
# configuration parameters.  Since it only sets variables to static
# quantites it can not fail at run time. Some of these variables are
# adjusted by parse_args() but asside from that none of these
# variables are ever written to. All global variables are defined here
# so we have a list of them and a comment of what they are for.


  $VERSION = ( qw$Revision: 1.1 $ )[1];
  
  @ORIG_ARGV = @ARGV;
  
  $LOCATEDB = '/net/master-mm/export/rpms/redhat/rpmarchive.locatedb';

  # a pattern which matches something inside the rpm archive mount point.

  $MOUNT_DIR_PATTERN = '/redhat/';

  # a pattern to limit the files which are displayed.

  $FILES_TO_DISPLAY_PATTERN = '/RPMS/';


  # set a known path.
  
  # the correct path has not been finalized yet, but this is close.
  
  $ENV{'PATH'}= (
		 '/usr/local/bin'.
		 ':/usr/bin'.
		 ':/bin'.
		 ':/usr/apache/cgibins/cgi-forms'.
		 '');
  
  # taint perl requires we clean up these bad environmental
  # variables.
  
  delete @ENV{'IFS', 'CDPATH', 'ENV', 'BASH_ENV'};
  
  return 1;
} #set_static_vars




sub get_env {

# this function sets variables similar to set_static variables.  This
# function may fail only if the OS is in a very strange state.  after
# we leave this function we should be all set up to give good error
# handling, should things fail.

  umask 0022; 
  $| = 1; 
  $PID = $$; 
  $PROGRAM = basename($0); 
  $TIME = time();
  $LOCALTIME = localtime($main::TIME); 
  $START_TIME = $TIME;

  { 

    # We show the results as if they were located in the same
    # directory as the locatedb appears to the cgi script.  The
    # directory is exported from the repository machine in a slightly
    # different place so that all its outputs would look wrong if we
    # displayed it raw.  This hack is what $DISPLAY_PREFIX is used
    # for, we try and compute it automatically.

    $LOCATEDB =~ m!$MOUNT_DIR_PATTERN! || 
      die("db file: $LOCATEDB must be located in a '$MOUNT_DIR_PATTERN' directory");
    
    $DISPLAY_PREFIX = $LOCATEDB;
    $DISPLAY_PREFIX =~ s!$MOUNT_DIR_PATTERN(.*)!$MOUNT_DIR_PATTERN!;
    
    (-r $LOCATEDB) || 
      die("The file: $LOCATEDB, must exists and be readable.");

    my ($mtime) = stat($LOCATEDB)->mtime;
    
    $DB_UPDATE_TIME = localtime($mtime);
  }

  return 1;
} # get_env


# fatal errors need to be valid HTML

sub fatal_error {
  my  @error = @_;

  print header;

  foreach $_ (@error) {
    print  $_;
  }

  print end_html;
  print "\n\n\n";

  die("\n");
}




sub print_query_page {

  my @out;

  push @out, start_form;  

  push @out, (
              "This page allows you to search for all packages ".
	      "in the RPM repository which match a particular pattern.",
	      p(),
	     );

  push @out, (
	      
              h3("Pattern",),
              "Enter a valid Perl5 Pattern: ",
              textfield(-name=>'pattern',
                        -size=>30,),
              p(),
             );
  
  
  push @out, (
	      defaults(-name=>'Defaults'),
	      submit(-name=>'Submit'),
	      p(),
	     );


  push @out, (
	      "Locate database created at: $DB_UPDATE_TIME\n",
	      p(),
	      "The time is now: $LOCALTIME\n",
	      p(),
	     );

  push @out, ( 
	      end_form(),
	     );

  print @out;

  return ;
}



# given a pattern remove any "tainted" characters.

sub clean_pattern {
  my ($data) = @_;
  my $out = '(none)';

  # we do not allow single quotes in the pattern because of the way we
  # invoke perllocate.  If we allowed \' then users could introduce
  # strings like "'; rm -rf /' echo 'done".  Unfortunatly we can not
  # be too strict about other characters because most characters are
  # needed to specify regular expressions.

  $data =~ s/\'//g;

  if ( $data =~ m/(.*)/ ) {
    $out = $1;
  }
  return $out;
}



# show the results of running perllocate on the chosen pattern.

sub print_perllocate {
  my($pattern, @args) = @_;

  $pattern =~ s/\'/\\\'/g;

  my $cmd = ( 
	     "perllocate -d $LOCATEDB '$pattern' 2>&1 ".
	     " | sed -e 's!.*$MOUNT_DIR_PATTERN!$DISPLAY_PREFIX!' ".
	     " | grep '$FILES_TO_DISPLAY_PATTERN' ".
	    "");

  print $cmd, p();

  my $result = "\n".qx{$cmd}."\n";

  print pre($result);

  return ;
}


#       Main        
{

  set_static_vars();
  get_env();

  my ($pattern) = clean_pattern(param("pattern"));

  my (@perllocate_args) = param("perllocate arguments");
  @perllocate_args = split(/\s+/, 
			   '--'.(join(" --", @perllocate_args)));
  push @perllocate_args, '--';
 
  
  print (
	 header(),
	 start_html(-title=>"perllocate"),
	 h2({-align=>'CENTER'},"perllocate"),
	 p(),
	);


  if (param("Help Screen")) {

    usage();

  } elsif ( grep {/^(\-\-)((help)|(version))$/} @perllocate_args ) {
       
    print_perllocate(@perllocate_args);

  } else {
 
    print_query_page();

      ($pattern) &&
	print_perllocate($pattern);

  }

  print (
	 end_html(),
	 "\n\n\n",
	);


  exit 0;
}


#!/usr/bin/perl

# a web interface to 'cvs rdiff'.  This script makes it easy to query
# the tags which are created by the build script.


use CGI ':standard';
use File::Basename;
use File::stat;
use Data::Dumper;

# the big datastructures are:

#    $RPM_FILE_BY_FQN{$fqn} is the full path rpm wich is discribed by the fqn

#    keys %SORTED_RECENT_FQN is the set of all package names

#    $SORTED_RECENT_FQN{$name} is an ordered list of the most recent
#				    versions of this package

# for a short time there are these datastrutures but they are large
# and expensive to save to disk.


# An rpm_package is a hash of:
#     $package{'fqn'}="perl-5.00502-3"
#     $package{'rpm_file'}="$RPMS_DIR/".
#                "./sparc/perl-5.00502-3.solaris2.6-sparc.rpm"
#     $package{'srpm_file'}="$SRPMS_DIR/".
#                           "./perl-5.00502-3.src.rpm"
#     $package{'name'}="perl"
#     $package{'version'}="5.00502"
#     $package{'release'}="3"

# fqn is "fully qualified name"

# while the $pkg structure exists we find the pkg we want by looking
# it up in this structure.  This will hold many more packages then the
# web page ever knows about.
#	$BY_NAME{$name}{$version}{$release};


sub usage {

  # If they are asking for help then they are clueless so reset all
  # their parameters for them, in case they are in a bad state.

  param(-name=>'Defaults', -value=>'on');
  my $rpmdiff_version = `rpmdiff --version`;

  $usage =<<EOF;

  $0          version: $VERSION
  $rpmdiff_version

This is a web interface into the rpmdiff command.

The user is requested to enter two different packages to diff after
any one of the multiple submit buttons is pressed the difference will
be the next webpage loaded.  For convenience each package name is
listed once (in alphabetical order) and below it is checkbox of the
most recent $MAX_PICK_LIST versions of this package.  Any pick list
which is not actively picked by the user contains the string '(none)'.

The user should pick one package in the first column (this represents
the "old package") and one package in the second column (this
represents the "new package").  When the user wants to run the
difference any 'submit' button can be pressed.  The multiple submit
buttons are listed only for convenience to reduce hunting for a button
on the page.

Error reporting is very minimal and if an incorrect number of packages
is picked then the main page is displayed again.  It is suggested that
the user hit the default button if any problems are encountered using
the program.

Most users are only interested in differences in the contents of files
and the contents of soft links.  The defaults for the program reflect
this interest.  However sometimes users are also interested in changes
in permissions or ownership.  Alternatively it may happen that a user
is only interested in the set of files whose size changes and changes
to files which keep the same size should be ignored.  To acomidate all
possible uses we gave the user great flexibility in determining what
set of changes are significant.  There is a pick list at the top of
the main screen which displays the current criterion for a difference
to be displayed.  A file which has changes made to properties which
are not picked will not be considered different and will not be
displayed.  Of special note the options:

help	will display the help screen for rpmdiff which contains an
	explanation of how to read the diff format.

all	will require that all differences are considered important.
	This is the same as checking all the boxes of differences

version will display the version of rpmdiff that is being used by
	this webpage.

The organization of the pick list page keeps the total number of
packages hidden from the user.  The pick list page takes a long time
to load because the number of choices is very large.  To save time the
set of package pick lists is not regenerated each time the page is
loaded.  There may have been new packages added to the package
repository since the page was generated and these packages will not be
displayed until the page is regenerated again.  The page will never be
more then one day old.  If you need to use the latest contents of the
package repository check the box at the bottom of the page marked
"Flush Cache" this will increase the loading time of the page but
ensure the freshness of the data.

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


  $ARCHIVE = "/devel/java_repository";
  $RCS_REVISION = ' $Revision: 1.1 $ ';
  
  @ORIG_ARGV= @ARGV;
  
  # The pattern for parsing fqn into ($name, $version, $release).
  # This is difficult to parse since some hyphens are significant and
  # others are not, some packages have alphabetic characters in the
  # version number.

  $PACKAGE_PAT ='(.*)-([^-]+)-([^-]+).solaris2.6-\w*.rpm';

  # packages which will end up in the picklists  match this pattern

  $PICKLIST_PAT = '/((htdocs)|(djava)|(devel))';

  # only show the most recent packages
  
  $MAX_PICK_LIST = 20;

  # the list of allowable arguments to rpmdiff

  @RPMDIFF_ARGS= qw(
		    version help all 
		    size mode md5 dev link user group mtime 
		   );

  @RPMDIFF_ARGS_DEFAULT = qw(size md5 link);

  # the list of  directories where rpms are stored
  @RPM_ARCHIVES = ('/net/master-mm/export/rpms/redhat',);

  $CACHE_DIR = "/tmp/webtools"; 

  # In an effort to make the cache update atomic we write to one file
  # name and only move it into the gobally known name when the whole
  # file is ready.

  $TMP_CACHE_FILE= "$CACHE_DIR/rpmfiles.cache.$UID"; 
  $CACHE_FILE= "$CACHE_DIR/rpmfiles.cache"; 
 
  # set a known path.
  
  # the correct path has not been finalized yet, but this is close.
  
  $ENV{'PATH'}= (
		 '/usr/local/bin'.
		 ':/usr/bin'.
		 ':/bin'.
		 ':/usr/apache/cgibins/cgi-forms'.
		 ':/tmp'.
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

  $| = 1; 
  $PID = $$; 
  $PROGRAM = basename($0); 
  $TIME = time();
  $LOCALTIME = localtime($main::TIME); 
  $START_TIME = $TIME;

  {
    my ($sec,$min,$hour,$mday,$mon,
	$year,$wday,$yday,$isdst) =
	  localtime(time());
    
    # convert confusing perl time vars to what users expect
    
    $year += 1900;
    $mon++;
    
    $CVS_DATE_STR = sprintf("%02u/%02u/%02u", $mday, $mon, $year, );
    $TAG_DATE_STR = sprintf("%02u%02u%02u", $year, $mon, $mday, );
    $TIME_STR = sprintf("%02u%02u", $hour, $min);
  }
  # a unique id for cache file generation
  $UID = "$TAG_DATE_STR.$TIME_STR.$PID";
  $VERSION = 'NONE';
  if ( $RCS_REVISION =~ m/([.0-9]+)/ ) {
    $VERSION = $1;
  }

  (-d $CACHE_DIR) ||
    mkdir($CACHE_DIR, 0664) ||
      die("Could not mkdir: $CACHE_DIR: $!\n");

  return 1;
} # get_env



sub parse_fqn {

  # This is difficult to parse since some hyphens are significant and
  # others are not, some packages have alphabetic characters in the
  # version number. 

  # Also remember that the format of the file is dependent on how RPM
  # is configured so this may not be portable to all RPM users.

  (!("@_" =~ m/^$PACKAGE_PAT$/)) &&
    die("rpm_package_name: '$@_' is not in a valid format");
  
  return ($1, $2, $3);
}


sub new_rpm_package {

# An rpm_package is a hash of:
#     $package{'fqn'}="perl-5.00502-3"
#     $package{'rpm_file'}="$RPMS_DIR/".
#                "./sparc/perl-5.00502-3.solaris2.6-sparc.rpm"
#     $package{'srpm_file'}="$SRPMS_DIR/".
#                           "./perl-5.00502-3.src.rpm"
#     $package{'name'}="perl"
#     $package{'version'}="5.00502"
#     $package{'release'}="3"

  my ($rpm_file) = @_;
  my $error = '';  
  my($name, $version, $release) = main::parse_fqn(basename($rpm_file));

  my ($package) = ();
  
  $package->{'fqn'}="$name-$version-$release";
  $package->{'name'}=$name;
  $package->{'version'}=$version;
  $package->{'release'}=$release;
  $package->{'rpm_file'}=$rpm_file;

  # these are needed to do proper sorting of major/minor numbers in
  # the version of the package

  $package->{'version_cmp'}=[split(/\./, $version)];
  $package->{'release_cmp'}=[split(/\./, $release)]; 

  return $package;
}


sub get_recent_fqn {
  my ($name) =(@_);

  my @out = ();

  foreach $version ( keys %{ $BY_NAME{$name} }) {
    foreach $release ( keys %{ $BY_NAME{$name}{$version} }) {

      push @out, $BY_NAME{$name}{$version}{$release};

    }
  }

  # the $BY_NAME datastructure is fairly good but the list can not be
  # sorted right. Sort again using the Schwartzian Transform as
  # discribed in perlfaq4

  my @sorted = sort {

    # compare the versions but make no assumptions
    # about how many elements there are
    
    my $i=0;
    my @a_version = @{ $a->{'version_cmp'} }; 
    my @b_version = @{ $b->{'version_cmp'} };
    while ( 
	   ($#a_version > $i) && 
	   ($#b_version > $i) && 
	   ($a_version[$i] == $b_version[$i]) 
	  ) {
      $i++;
    }
    
    my $j = 0;
    my @a_release = @{ $a->{'release_cmp'} }; 
    my @b_release = @{ $b->{'release_cmp'} };
    while ( 
	   ($#a_release > $j) && 
	   ($#b_release > $j) &&
	   ($a_release[$j] == $b_release[$j])
	  ) {
      $j++;
    }
    
    return (
	    ($b_version[$i] <=> $a_version[$i])
	    ||
	    ($b_release[$j] <=> $a_release[$j])
	   );
  }
  @out;
  
  ($#sorted > $MAX_PICK_LIST) &&
    (@sorted = @sorted[0 .. $MAX_PICK_LIST]);

  # dumping data to disk is expensive so we only save the data we
  # need.  Limit RPM_FILE_BY_FQN to only those packages which appear
  # in the picklist and this explains why we do not store the whole
  # pkg in a BY_FQN hash.

  foreach $pkg (@sorted) {
    $RPM_FILE_BY_FQN{$pkg->{'fqn'}}=$pkg->{'rpm_file'}
  }

  my @fqns = map { $_->{'fqn'} } @sorted;

  return @fqns;  
}



sub parse_package_names {

  $flush_cache = param("Flush Cache");
  if ( (!($flush_cache)) && (-e $CACHE_FILE) && ( -M $CACHE_FILE < 1 ) ) {
    my $st = stat($CACHE_FILE) ||
      die ("Could not stat: $CACHE_FILE: $!");
    $CACHE_LOCALTIME=localtime($st->mtime);
    require $CACHE_FILE;
    return ;
  }

  $CACHE_LOCALTIME=$LOCALTIME;

  foreach $archive (@RPM_ARCHIVES) {
    
    open(FILES, "-|") || 
      exec("find", $archive, "-print") ||
	die("Could not run find. $!\n");

    while ($filename = <FILES>) { 

      # we want only the binary rpm files of interest

      ($filename =~ m/\.rpm$/) || next;
      ($filename =~ m/\.src\.rpm$/) && next;
      ($filename =~ m/$PICKLIST_PAT/) || next;
      chomp $filename;

      $pkg = new_rpm_package($filename);
      $BY_NAME{$pkg->{'name'}}{$pkg->{'version'}}{$pkg->{'release'}} = $pkg;

    }

    close(FILES) || 
      die("Could not close find. $!\n");
    
  }

  foreach $group (keys %BY_NAME) {
    $SORTED_RECENT_FQN{$group} = [get_recent_fqn($group)];

  }

  open(FILE, ">$TMP_CACHE_FILE") ||
    die("Could not open filename: '$TMP_CACHE_FILE': $!\n");

  print FILE "# cache file created by $0\n";
  print FILE "# at $LOCALTIME\n\n";

  print FILE Data::Dumper->Dump( [\%RPM_FILE_BY_FQN,  \%SORTED_RECENT_FQN],
				 ["SAVED_FQN", "SAVED_SORTED",], );

  print FILE "\n\n";
  print FILE '%RPM_FILE_BY_FQN = %{ $SAVED_FQN };'."\n";
  print FILE '%SORTED_RECENT_FQN = %{ $SAVED_SORTED };'."\n";
  print FILE "1;\n";

  close(FILE) ||
    die("Could not close filename: '$TMP_CACHE_FILE': $!\n");

  # In an effort to make the cache update atomic we write to one file
  # name and only move it into the gobally known name when the whole
  # file is ready.

  (!(-e $CACHE_FILE)) ||
    unlink($CACHE_FILE) ||
      die("Could not unlink $CACHE_FILE: $!\n");

  rename($TMP_CACHE_FILE, $CACHE_FILE) ||
    die("Could not rename ($TMP_CACHE_FILE, $CACHE_FILE): $!\n");

  return ;
}





sub print_pkg_picklists {

  print start_form;  
  # create a set of picklists for the packages based on the package names.

  print h3("Choose the criterion for a difference"),
  checkbox_group( 
		 -name=>"rpmdiff arguments",
		 -value=>[ @RPMDIFF_ARGS ],
		 -default=>[ @RPMDIFF_ARGS_DEFAULT ],
		),p();
    
  print h3("Choose one package in each column then hit any submit"),p();
  
  my @rows = ();
  
  foreach $name (sort keys %SORTED_RECENT_FQN) {
    
    push @rows,
    # column A
    td(
       strong("$name "),
       p(),
       popup_menu( 
		  -name=>"old$name",
		  -value=>[
			   '(none)', 
			   @{ $SORTED_RECENT_FQN{$name} },
			  ],
		  -default=>'(none)',
		 ),
      ).
	# column B
	td(
	   strong("$name "),
	   p(),
	   popup_menu( 
		      -name=>"new$name",
		      -value=>[
			       '(none)', 
			       @{ $SORTED_RECENT_FQN{$name} },
			      ],
		      -default=>'(none)',
		     ),
	  ).
	    td(
	       defaults(-name=>'Defaults'),
	       submit(-name=>'Submit'),
	      ).
		'';
  }
  
  print table(Tr(\@rows));

  my $footer_info=<<EOF;

Try 'rpmdiff --help' for information about what constitues a
difference.  The output of rpmdiff is exactly the same as the output
of rpm verify, 'rpm -V'.  The --help option documents the format of
rpm verify and the format of rpmdiff and is a handy reference for this
terse table.  rpmdiff is included in the devel-build-tools package.


This web interface is for taking differences in the binary code.  To
take differences of the binaries use <a href="cvs_tag_diff.cgi">'cvs tag diff'</a>.  

EOF

  print pre($footer_info);

  print "This page generated with data cached at: $CACHE_LOCALTIME\n",p(),
        "The time is now: $LOCALTIME\n",p(),
        submit(-name=>"Flush Cache"),p(),
        submit(-name=>"Help Screen"),p();

  print end_form;  

  return ;
}



sub print_diff {
  my($oldpkg_file, $newpkg_file, @args) = @_;

  my $cmd = "rpmdiff @args $oldpkg_file $newpkg_file 2>&1";

  my $result = "\n".qx{$cmd}."\n";
  print pre($result);

  return ;
}


#       Main        
{

  set_static_vars();
  get_env();

  parse_package_names();

  my @picked_rpmdiff_args = param("rpmdiff arguments");
  @picked_rpmdiff_args = split(/\s+/, 
			       '--'.(join(" --", @picked_rpmdiff_args)));
  push @picked_rpmdiff_args, '--';
 
  foreach $name (sort keys %SORTED_RECENT_FQN) {
    
    if ( (param("old$name")) && (param("old$name") ne "(none)") ) {
      push @picked_oldpkg, param("old$name");
    }
    
    if ( (param("new$name")) && (param("new$name") ne "(none)") ) {
      push @picked_newpkg, param("new$name");
    }
    
  }

  print (header.
	 start_html(-title=>'rpmdiff'),
	 h2("rpmdiff"));
  
  if (param("Help Screen")) {

    usage();

  } elsif ( grep {/^(\-\-)((help)|(version))$/} @picked_rpmdiff_args ) {
       
    print_diff(
	       '/dev/null', 
	       '/dev/null', 
	       @picked_rpmdiff_args, 
	      );
    
  } elsif (
	   ($#picked_oldpkg == 0) &&
	   ($#picked_newpkg == 0)
	  ) {
    
    print_diff(
	       $RPM_FILE_BY_FQN{$picked_oldpkg[0]}, 
	       $RPM_FILE_BY_FQN{$picked_newpkg[0]}, 
	       @picked_rpmdiff_args, 
	      );
    
  } else {

    print_pkg_picklists();

    print end_html;
    print "\n\n\n";
  }

}


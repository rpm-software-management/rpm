#!/usr/bin/perl
#
# osgideps.pl -- Analyze dependencies of OSGi bundles.
#
# Kyu Lee (initial idea)
# Alphonse Van Assche <alcapcom@fedoraproject.org> (current maintainer)
#
# $Id: osgideps.pl,v 1.0 2009/06/08 12:12:12 mej Exp $

use Getopt::Long;
use File::Temp qw/ tempdir /;
use threads;
use Thread::Queue;

$MANIFEST_NAME = "META-INF/MANIFEST.MF";

# parse options
my ( $show_provides, $show_requires, $show_system_bundles, $debug );
my $result = GetOptions(
	"provides" => \$show_provides,
	"requires" => \$show_requires,
	"system" => \$show_system_bundles,
	"debug" => \$debug
);
exit(1) if ( not $result );

# run selected function
@allfiles = <STDIN>;
if ($show_provides) {
	getProvides(@allfiles);
}
if ($show_requires) {
	getRequires(@allfiles);
}
if ($show_system_bundles) {
	getSystemBundles(@allfiles);
}
exit(0);

# this function print provides of OSGi aware files
sub getProvides {
	
	my $queue = Thread::Queue->new;
	foreach $file (@_) {
		$queue->enqueue($file);
	}

	my @workers;
	push @workers, threads->create('getProvidesWorker');
	push @workers, threads->create('getProvidesWorker');
	push @workers, threads->create('getProvidesWorker');
	push @workers, threads->create('getProvidesWorker');

	map { $_->join } @workers;
	
	sub getProvidesWorker {
		while ( my $file = $queue->dequeue_nb ) {
			chomp($file);
			# we don't follow symlinks for provides
			next if ( -f $file && -r _ && -l _ );
			$file =~ s/[^[:print:]]//g;
			if ( $file =~ m/$MANIFEST_NAME$/ || $file =~ m/\.jar$/ ) {
				if ( $file =~ m/\.jar$/ ) {
					if ( `zipinfo -1 $file 2> /dev/null | grep -e \^$MANIFEST_NAME` eq "$MANIFEST_NAME\n" ) {
						# extract MANIFEST.MF file from jar to temporary directory
						$tmpdir = tempdir( CLEANUP => 1 );						
						`unzip -d $tmpdir -qqo $file $MANIFEST_NAME`;
						open( MANIFEST, "$tmpdir/$MANIFEST_NAME" );
					}
				} else {
					open( MANIFEST, "$file" );
				}
				my $bundleName = "";
				my $version = "";
				# parse Bundle-SymbolicName, Bundle-Version and Export-Package attributes
				while (<MANIFEST>) {
					# get rid of non-print chars (some manifest files contain weird chars)
					s/[^[:print]]//g;
					if ( m/(^(Bundle-SymbolicName): )(.*)$/ ) {
						$bundleName = "$3" . "\n";
						while (<MANIFEST>) {
							if ( m/^[[:upper:]][[:alpha:]]+-[[:upper:]][[:alpha:]]+: .*/ ) {
								$len = length $_;
								seek MANIFEST, $len * -1, 1;
								last;
							}
							$bundleName .= "$_";
						}
						$bundleName =~ s/\s+//g;
						$bundleName =~ s/;.*//g;
					}
					if ( m/(^Bundle-Version: )(.*)/ ) {
						$version = $2;
					}
					if ( m/(^(Export-Package): )(.*)$/ ) {
						my $bunlist = "$3" . "\n";
						while (<MANIFEST>) {
							if ( m/^[[:upper:]][[:alpha:]]+-[[:upper:]][[:alpha:]]+: .*/ ) {
								$len = length $_;
								seek MANIFEST, $len * -1, 1;
								last;
							}
							$bunlist .= "$_";
						}
						push @bundlelist, parsePkgString($bunlist, $file);
					}
				}	
	
				# skip this jar if no bundle name exists
				if ( !$bundleName eq "" ) {
					if ( !$version eq "" ) {
						$version = parseVersion($version);
						push @bundlelist, { FILE => "$file", NAME => "$bundleName", VERSION => "$version" };
					} else {	
						push @bundlelist, { FILE => "$file", NAME => "$bundleName", VERSION => "" };
					}
				}
				`rm -rf $tmpdir`;
			}
		}
		if ( !$debug ) { @bundlelist = prepareOSGiBundlesList(@bundlelist); }
		$list = "";
		for $bundle (@bundlelist) {
			if ( !$debug ) {
				$list .= "osgi(" . $bundle->{NAME} . ")" . $bundle->{VERSION} . "\n";
			} else {
				$list .= $bundle->{FILE} . " osgi(" . $bundle->{NAME} . ")" . $bundle->{VERSION} . "\n";
			}
		}
		print $list;
	}
}

# this function print requires of OSGi aware files
sub getRequires {

	my $queue = Thread::Queue->new;
	foreach $file (@_) {
		$queue->enqueue($file);
	}

	my @workers;
	push @workers, threads->create('getRequiresWorker');
	push @workers, threads->create('getRequiresWorker');
	push @workers, threads->create('getRequiresWorker');
	push @workers, threads->create('getRequiresWorker');

	map { $_->join } @workers;
	
	sub getRequiresWorker {
		while ( my $file = $queue->dequeue_nb ) {
			next if ( -f $file && -r _ );
			$file =~ s/[^[:print:]]//g;
			if ( $file =~ m/$MANIFEST_NAME$/ || $file =~ m/\.jar$/ ) {
				# we explicitly requires symlinked jars
				# _that_reside_outside_the_package_
				if (-l $file) {
					$exist = 0;
					$lnksrc = `readlink -qen $file`;
					foreach $exfile ( @allfiles ) {
						$exfile =~ s/[^[:print:]]//g;
						if ( $lnksrc =~ m/$exfile$/ ) {							
							$exist = 1;
							last;
						}
					}
					print "$lnksrc\n" if (!$exist);
					next;
				}	 
				
				if ( $file =~ m/\.jar$/ ) {
					if ( `zipinfo -1 $file 2> /dev/null | grep -e \^$MANIFEST_NAME` eq "$MANIFEST_NAME\n" ) {
						# extract MANIFEST.MF file from jar to temporary directory
						$tmpdir = tempdir( CLEANUP => 1 );
						`unzip -d $tmpdir -qqo $file $MANIFEST_NAME`;
						open( MANIFEST, "$tmpdir/$MANIFEST_NAME" );
					}
				} else {
					open( MANIFEST, "$file" );
				}
				while (<MANIFEST>) {
					if ( m/(^(Require-Bundle|Import-Package): )(.*)$/ ) {
						my $bunlist = "$3" . "\n";
						while (<MANIFEST>) {
							if (m/^[[:upper:]][[:alpha:]]+-[[:upper:]][[:alpha:]]+: .*/ ) {
								$len = length $_;
								seek MANIFEST, $len * -1, 1;
								last;
							}
							$bunlist .= "$_";
						}
						push @bundlelist, parsePkgString($bunlist, $file);
					}
					# we also explicitly require symlinked jars define by 
					# Bundle-ClassPath attribut
					if ( m/(^(Bundle-ClassPath): )(.*)$/ ) {
						$bunclp = "$3" . "\n";
						while (<MANIFEST>) {
							if ( m/^[[:upper:]][[:alpha:]]+-[[:upper:]][[:alpha:]]+: .*/ ) {
								$len = length $_;
								seek MANIFEST, $len * -1, 1;
								last;
							}
							$bunclp .= "$_";
						}
						$bunclp =~ s/\ //g;
						$bunclp =~ s/\n//g;
						$bunclp =~ s/[^[:print:]]//g;
						$dir = `dirname $file`;
						$dir =~ s/\n//g;
						@jars = split /,/, $bunclp;
						for $jarfile (@jars) {
							$jarfile = "$dir\/\.\.\/$jarfile";
							$jarfile = readlink $jarfile;
							if ( !$jarfile eq "" ) {							
								print "$jarfile" . "\n";
							}
						}
					}
				}
				`rm -rf $tmpdir`;
			}
		}
		if ( !$debug ) { @bundlelist = prepareOSGiBundlesList(@bundlelist); }
		$list = "";
		for $bundle (@bundlelist) {
			# replace '=' by '>=' because qualifiers are set on provides 
			# but not on requires.
			$bundle->{VERSION} =~ s/\ =/\ >=/g;
			if ( !$debug ) {
				$list .= "osgi(" . $bundle->{NAME} . ")" . $bundle->{VERSION} . "\n";
			} else {
				$list .= $bundle->{FILE} . " osgi(" . $bundle->{NAME} . ")" . $bundle->{VERSION} . "\n";
			}
		}
		print $list;
	}
}

# this function print system bundles of OSGi profile files.
sub getSystemBundles {
	foreach $file (@_) {
		if ( ! -f $file || ! -r _ ) {
			print "'$file' file not found or cannot be read!";
			next;
		} else {
			open( PROFILE, "$file" );
			while (<PROFILE>) {
				if ( $file =~ m/\.profile$/ ) {
					if (m/(^(org\.osgi\.framework\.system\.packages)[=|\ ]+)(.*)$/) {
						$syspkgs = "$3" . "\n";
						while (<PROFILE>) {
							if (m/^[a-z]/) {
								$len = length $_;
								seek MANIFEST, $len * -1, 1;
								last;
							}
							$syspkgs .= "$_";
						}
						$syspkgs =~ s/\s+//g;
						$syspkgs =~ s/\\//g;
						@bundles = split /,/, $syspkgs;
						foreach $bundle (@bundles) {
							print "osgi(" . $bundle . ")\n";
						}
					}
				}
			}
		}
	}
}

sub parsePkgString {
	my $bunstr = $_[0];
	my $file = $_[1];
	my @return;
	$bunstr =~ s/ //g;
	$bunstr =~ s/\n//g;
	$bunstr =~ s/[^[:print:]]//g;
	$bunstr =~ s/("[[:alnum:]|\-|\_|\.|\(|\)|\[|\]]+)(,)([[:alnum:]|\-|\_|\.|\(|\)|\[|\]]+")/$1 $3/g;
	# remove uses bundle from Export-Package attribute
	$bunstr =~ s/uses:="[[:alnum:]|\-|\_|\.|\(|\)|\[|\]|,]+"//g;
	# remove optional dependencies
	$bunstr =~ s/,.*;resolution:=optional//g;
	# remove x-friends
	$bunstr =~ s/;x-friends:="[[:alnum:]|\-|\_|\.|\(|\)|\[|\]|,]+"//g;
	# remove signatures 
	$bunstr =~ s/Name:.*SHA1-Digest:.*//g;
	@reqcomp = split /,/, $bunstr;
	foreach $reqelement (@reqcomp) {
		@reqelementfrmnt = split /;/, $reqelement;
		$name = "";
		$version = "";
		$name = $reqelementfrmnt[0];
		$name =~ s/\"//g;
		# ignoring OSGi 'system.bundle' 
		next if ( $name =~ m/^system\.bundle$/ );
		for $i ( 1 .. $#reqelementfrmnt ) {
			if ( $reqelementfrmnt[$i] =~ m/(^(bundle-|)version=")(.*)(")/ ) {
				$version = $3;
				last;
			}
		}
		$version = parseVersion($version);
		push @return, { FILE => "$file", NAME => "$name", VERSION => "$version" };
	}
	return @return;
}

sub parseVersion {
	my $ver = $_[0];
	if ( $ver eq "" ) { return ""; }
	if ( $ver =~ m/(^[\[|\(])(.+)\ (.+)([\]|\)]$)/ ) {
		if ( $1 eq "\[" ) {
			$ver = " >= $2";
		} else {
			$ver = " > $2";
		}
	} else {
		$ver = " = $ver";
	}
	# we always return a full OSGi version to be able to match 1.0
	# and 1.0.0 as equal in RPM.
	( $major, $minor, $micro, $qualifier ) = split( '\.', $ver );
	if ( !defined($minor) || !$minor ) {
		$minor = 0;
	}
	if ( !defined($micro) || !$micro ) {
		$micro = 0;
	}
	if ( !defined($qualifier) || !$qualifier ) {
		$qualifier = "";
	} else {
		$qualifier = "." . $qualifier;
	}
	$ver = $major . "." . $minor . "." . $micro . $qualifier;
	return $ver;
}

# this function put the max version on each bundles to be able to remove
# duplicate deps with 'sort -u' command.
sub prepareOSGiBundlesList {
	foreach $bundle (@_) {
		foreach $cmp (@_) {
			if ( $bundle->{NAME} eq $cmp->{NAME} ) {
				$result = compareVersion( $bundle->{VERSION}, $cmp->{VERSION} );
				if ( $result < 0 ) {
					$bundle->{VERSION} = $cmp->{VERSION};
				}
			}
		}
	}
	return @_; 
}

# this function returns a negative integer, zero, or a positive integer if 
# $ver1 is less than, equal to, or greater than $ver2.
#
# REMEMBER: we mimic org.osgi.framework.Version#compareTo method but
# *at this time* we don't take care of the qualifier part of the version.
sub compareVersion {
	my $ver1 = $_[0];
	my $ver2 = $_[1];
	
	$ver1 = "0.0.0" if ( $ver1 eq "" );
	$ver2 = "0.0.0" if ( $ver2 eq "" );

	$ver1 =~ m/([0-9]+)(\.)([0-9]+)(\.)([0-9]+)/;
	$major1 = $1;
	$minor1 = $3;
	$micro1 = $5;

	$ver2 =~ m/([0-9]+)(\.)([0-9]+)(\.)([0-9]+)/;
	$major2 = $1;
	$minor2 = $3;
	$micro2 = $5;

	$result = $major1 - $major2;
	return $result if ( $result != 0 );
	
	$result = $minor1 - $minor2;
	return $result if ( $result != 0 );
	
	$result = $micro1 - $micro2;
	return $result if ( $result != 0 );
	
	return $result;
}

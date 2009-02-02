#!/usr/bin/perl


use Cwd;
use Getopt::Long;


$cdir = getcwd();
$TEMPDIR="/tmp";
$MANIFEST_NAME="META-INF/MANIFEST.MF";


# prepare temporary directory
if (! (-d $TEMPDIR)) {
        if (($_ = `mkdir $TEMPDIR`) != 0) {exit 1;}
        elsif (! (-w $TEMPDIR) && (-x $TEMPDIR)) {exit 1;}
}

# parse options
my ($show_provides, $show_requires);

my $result = GetOptions("provides" => \$show_provides,
			"requires" => \$show_requires);

exit(1) if (not $result);



@allfiles = <STDIN>;

if ($show_provides) {
	do_provides(@allfiles);
}

if ($show_requires) {
	do_requires(@allfiles);
}


exit(0);



sub do_provides {

foreach $file (@_) {

	next if -f $file && -r $file && !-l $file;
	$file =~ s/[^[:print:]]//g;
	if ($file =~ m/$MANIFEST_NAME$/ || $file =~ m/\.jar$/ ) {
		if ($file =~ m/\.jar$/) {
        		# if this jar contains MANIFEST.MF file
	        	if (`jar tf $file | grep -e \^$MANIFEST_NAME` eq "$MANIFEST_NAME\n") {
		               	# extract MANIFEST.MF file from jar to temporary directory
	        	        chdir $TEMPDIR;
			        `jar xf $file $MANIFEST_NAME`;	
		               	open(MANIFEST, "$MANIFEST_NAME");
				chdir $cdir;
			}
	        } else  {
			open(MANIFEST, "$file");
	        }
        	my $bundleName = "";
	        my $version = "";
        	# parse bundle name and version
	        while(<MANIFEST>) {
        		# get rid of non-print chars (some manifest files contain weird chars)
                	s/[^[:print]]//g;
	                if (m/(^Bundle-SymbolicName: )((\w|\.)+)(\;*)(.*\n)/) {
        	        	$bundleName = $2;
                	}
	                if (m/(^Bundle-Version: )(.*)/) {
				$version = $2;
				$version = fixVersion($version);
			}
		        if (m/(^(Export-Package): )(.*)$/) {
	        		my $bunlist = "$3"."\n";
				while(<MANIFEST>) {
					if (m/^[[:upper:]][[:alpha:]]+-[[:upper:]][[:alpha:]]+: .*/) {
						$len = length $_;
						seek MANIFEST, $len*-1 , 1;
						last;
                        		}
					$bunlist.="$_";
				}
				push @bundlelist,  parsePkgString($bunlist);
			}
                }
                # skip this jar if no bundle name exists
                if (! $bundleName eq "") {
			if (! $version eq "") {
				print "osgi(".$bundleName.") = ".$version."\n";
	                } else {
                                print "osgi(".$bundleName.")\n";
                        }
                }
	}
}
$list = "";
for $bundle (@bundlelist) {
	$list .= "osgi(".$bundle->{NAME}.")".$bundle->{VERSION}."\n";
}
# For now we dont take Require-Bundle AND Import-Package in account
#print $list;
}


sub do_requires {

	foreach $file (@_) {

		next if -f $file && -r $file;
		$file =~ s/[^[:print:]]//g;
		if ($file =~ m/$MANIFEST_NAME$/ || $file =~ m/\.jar$/ ) {
			if ($file =~ m/\.jar$/) {
				# if this jar contains MANIFEST.MF file
		        	if (`jar tf $file | grep -e \^$MANIFEST_NAME` eq "$MANIFEST_NAME\n") {
					# extract MANIFEST.MF file from jar to temporary directory
			                chdir $TEMPDIR;
					`jar xf $file $MANIFEST_NAME`;	
					open(MANIFEST, "$MANIFEST_NAME");
					chdir $cdir;
				}
			} else  {
				open(MANIFEST, "$file");
			}
       			my %reqcomp = ();
	                while(<MANIFEST>) {
        	                if (m/(^(Require-Bundle|Import-Package): )(.*)$/) {
				my $bunlist = "$3"."\n";
                                while(<MANIFEST>) {
                                        if (m/^[[:upper:]][[:alpha:]]+-[[:upper:]][[:alpha:]]+: .*/) {
                                                $len = length $_;
                                                seek MANIFEST, $len*-1 , 1;
                                                last;
                                        }
					$bunlist.="$_";
                                }
				push @bundlelist,  parsePkgString($bunlist);
                        }
                }
	}
}

$list = "";
for $bundle (@bundlelist) {
	$list .= "osgi(".$bundle->{NAME}.")".$bundle->{VERSION}."\n";
}
# For now we dont take Require-Bundle AND Import-Package in account
#print $list;
}

sub parsePkgString {
        my $bunstr = $_[0];
        my @return;
	$bunstr =~ s/ //g;
        $bunstr =~ s/\n//g;
        $bunstr =~ s/[^[:print:]]//g;
        $bunstr =~ s/("[[:alnum:]|\-|\_|\.|\(|\)|\[|\]]+)(,)([[:alnum:]|\-|\_|\.|\(|\)|\[|\]]+")/$1 $3/g;
        @reqcomp = split /,/g, $bunstr;
        foreach $reqelement (@reqcomp) {
                @reqelementfrmnt = split /;/g, $reqelement;
                $name="";
                $version="";
                $name = $reqelementfrmnt[0];
                for $i (1 .. $#reqelementfrmnt) {
                        if ($reqelementfrmnt[$i] =~ m/(^(bundle-|)version=")(.*)(")/){
                                $version = $3;
                                last;
                        }
                }
                $version = parseVersion($version);
		$version = fixVersion($version);
		# dirty fix for provides that contain " char
		$name =~ s/\"//g;
                push @return, { NAME=>"$name", VERSION=>"$version"};
        }

        return @return;
}

sub parseVersion {
        my $ver = $_[0];
        if ($ver eq "") { return "";}
        if ($ver =~ m/(^[\[|\(])(.+)\ (.+)([\]|\)]$)/) {
		# FIXME: The right rpm match of osgi version [1,2) seems to be <= 2
		# but when you look at the requires >= look more permssive/correct?
		($1 eq "\[") ? return " >= $2" : return " > $2";
        } else {
                return " = $ver";
        }
        return $ver;
}

sub fixVersion {
        my $version = $_[0];
	# remove version qualifier.
	$version =~ s/\.v.[0-9]*.*//g;
	# We try to match RPM version, so remove last .0
	$version =~ s/\.0$//g;
	return $version;
}


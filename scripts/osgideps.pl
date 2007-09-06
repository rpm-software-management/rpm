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


foreach $jar (@_) {

next if -f $jar && -r $jar;
 $jar =~ s/[^[:print:]]//g;
        # if this jar contains MANIFEST.MF file
        if (`jar tf $jar | grep -e \^$MANIFEST_NAME` eq "$MANIFEST_NAME\n") {
                # extract MANIFEST.MF file from jar to temporary directory
                chdir $TEMPDIR;
                `jar xf $cdir/$jar $MANIFEST_NAME`;
                open(MANIFEST, "$MANIFEST_NAME");
                my $bundleName = "";
                my $bundleVersion = "";
                # parse bundle name and version
                while(<MANIFEST>) {
                        # get rid of non-print chars (some manifest files contain weird chars
                        s/[^[:print]]//g;
                        if (m/(^Bundle-SymbolicName: )((\w|\.)+)(\;*)(.*\n)/) {
                                $bundleName = $2;
                        }
                        if (m/(^Bundle-Version: )(.*)/) {
                                $bundleVersion = $2;
                        }
                }
                # skip this jar if no bundle name exists
                if (! $bundleName eq "") {
                        if (! $bundleVersion eq "") {
                                print "osgi(".$bundleName.") = ".$bundleVersion."\n";
                        } else {
                                print "osgi(".$bundleName.")\n";
                        }
                }
                chdir $cdir;
        }
	
}

}


sub do_requires {

foreach $jar (@_) {
next if -f $jar && -r $jar;
$jar =~ s/[^[:print:]]//g;
        if (`jar tf $jar | grep -e \^$MANIFEST_NAME` eq "$MANIFEST_NAME\n") {
                chdir $TEMPDIR;
                `jar xf $cdir/$jar $MANIFEST_NAME`;
                open(MANIFEST, "$MANIFEST_NAME") or die;
                my %reqcomp = ();
                while(<MANIFEST>) {
                        if (m/(^(Require-Bundle|Import-Package): )(.*)$/) {
                                my $reqlist = "$3"."\n";
                                while(<MANIFEST>) {
                                        if (m/^[[:upper:]][[:alpha:]]+-[[:upper:]][[:alpha:]]+: .*/) {
                                                $len = length $_;
                                                seek MANIFEST, $len*-1 , 1;
                                                last;
                                        }
                                        $reqlist.="$_";
                                }
                                push @requirelist,  parseReqString($reqlist);
                        }

                }
                chdir $cdir;
	}

}

$list = "";
for $require (@requirelist) {
        $list .= "osgi(".$require->{NAME}.")".$require->{VERSION}."\n";
}
#$abc = `echo \"$list\"|grep -e \^osgi\\(.*\\)| sort|uniq`;
print $list;

}

sub parseReqString {
        my $reqstr = $_[0];
        my @return;
        $reqstr =~ s/ //g;
        $reqstr =~ s/\n//g;
        $reqstr =~ s/[^[:print:]]//g;
        $reqstr =~ s/("[[:alnum:]|\-|\_|\.|\(|\)|\[|\]]+)(,)([[:alnum:]|\-|\_|\.|\(|\)|\[|\]]+")/$1 $3/g;
        @reqcomp = split /,/g, $reqstr;
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
                push @return, { NAME=>"$name", VERSION=>"$version"};
        }

        return @return;
}

sub parseVersion {
        my $ver = $_[0];
        if ($ver eq "") { return "";}
        if ($ver =~ m/(^[\[|\(])(.+)\ (.+)([\]|\)]$)/) {
                ($1 eq "\[") ? return " <= $2" : return " < $2";
        } else {
                return " = $ver";
        }
        return $ver;
}


#!/bin/sh

# This script reads filenames from STDIN and outputs any relevant provides
# information that needs to be included in the package.

PATH=/usr/bin:/usr/ccs/bin:/usr/sbin:/sbin:/usr/local/bin;
export PATH;

javadeps_args='--provides --rpmformat --keywords --starprov'


IGNORE_DEPS="@"
BUILDROOT="/"



# Loop over all args

while :
do

# Break out if there are no more args
	case $# in
	0)
		break
		;;
	esac

# Get the first arg, and shuffle
	option=$1
	shift

# Make all options have two hyphens
	orig_option=$option	# Save original for error messages
	case $option in
	--*) ;;
	-*) option=-$option ;;
	esac


	case $option in
	--buildroot)
		BUILDROOT=$1
		shift
		;;
	--ignore_deps)
		IGNORE_DEPS=$1
		shift
		;;
	--help)
		echo $usage
		exit 0
		;;
	*)
		echo "$0: Unrecognized option: \"$orig_option\"; use --help for usage." >&2
		exit 1
		;;
	esac
done







for file in `cat -`
do

# this section is for processing based on the interpreter specified in
# the '#!' line.

case `get_magic $file` in 

bash)
    print_deps --identifier executable $file
    print_deps --identifier executable --basename $file
;;

sh)
    print_deps --identifier executable $file
    print_deps --identifier executable --basename $file
;;

perl)
    perl.prov $file;
;;

wish)
    print_deps --identifier tcl $file
    print_deps --identifier tcl --basename $file
;;


esac


# this section is for processing based on filename matching.  It is
# crude but needed as many library types have no easily identifiable
# '#!' line

case $file in 

# We can not count on finding a SONAME in third party Libraries though
# they tend to include softlinks with the correct SONMAE name.  We
# must assume anything with a *\.so* and is of type 'dynamic lib' is a
# library.  This scriptlet works because 'file' follows soft links.


*lib*.so*)
    /usr/ucb/file -L $file 2>/dev/null | \
	grep "ELF.*dynamic lib" | cut -d: -f1 | \
    	xargs -n 1 basename | print_deps --identifier so;

    # keep this for backward compatibility till we have converted
    # everything.

    /usr/ucb/file -L $file 2>/dev/null | \
	grep "ELF.*dynamic lib" | cut -d: -f1 | \
    	xargs -n 1 basename;
;;

# Java jar files are just a special kind of zip files.
# Sun OS 5.5.1 does not understand zip archives, it calls them 'data'
# Sun OS 5.6 has this line in /etc/magic
# 0       string          PK\003\004      ZIP archive

*.jar)

    unzip -p $file |\
    javadeps $javadeps_args -;

;;

# there are enough jar files out there with zip extensions that we
# need to have a separate entry

*.zip)

    unzip -p $file |\
    javadeps $javadeps_args -;

;;

# Java Class files
# Sun OS 5.6 has this line in /etc/magic
# 0       string          \312\376\272\276        java class file

*.class) 

    javadeps $javadeps_args $file;

;;



# Perl libraries are hard to detect.  Use file endings.

*.pl) 

    perl.prov $file;

    # pl files are often required using the .pl extension
    # so provide that name as well

    print_deps --identifier perl --basename $file
;;

*.pm) 

    perl.prov $file;
;;

*.ph)

    # ph files do not use the package name inside the file.
    # perlmodlib  documentation says:

    #       the .ph files made by h2ph will probably end up as
    #       extension modules made by h2xs.

    # so do not expend much effort on these.

    print_deps --identifier perl --basename $file

;;

# tcl libraries are hard to detect.  Use file endings.

*.tcl) 

    print_deps --identifier tcl $file
    print_deps --identifier tcl --basename $file
;;



*)

    # Dependencies for html documenets are a bit ill defined. Lets try
    # using file endings like the browsers do.
    # precise globbing is hard so I use egrep instead of the case statement.

hfile=`basename $file | egrep '\.((cgi)|(ps)|(pdf)|(png)|(jpg)|(gif)|(tiff)|(tif)|(xbm)|(html)|(htm)|(shtml)|(jhtml))$'`;

	if [ "${hfile}" != "" ]
	then
	print_deps --identifier http --basename $file
	fi

	# all files are candidates for being an executable.  Let the
	# magic.prov script figure out what should be considered
	# execuables.

	magic.prov  --buildroot=$BUILDROOT $file

;;


esac

done | sort -u | egrep -v \'$IGNORE_DEPS\'


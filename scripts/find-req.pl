#!/bin/sh

# This script reads filenames from STDIN and outputs any relevant provides
# information that needs to be included in the package.

PATH=/usr/bin:/usr/ccs/bin:/usr/sbin:/sbin:/usr/local/bin;
export PATH;

javadeps_args='--requires --rpmformat --keywords'

ulimit -c 0;





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
    /usr/local/lib/rpm/bash --rpm-requires $file;
;;

sh)
    /usr/local/lib/rpm/bash --rpm-requires $file;
;;

perl)
    perl.req $file;
;;

wish)
    tcl.req $file;
;;

python)
    python.req $file;
;;

esac


# this section is for processing based on filename matching.  It is
# crude but needed as many library types have no easily identifiable
# '#!' line

case $file in 

# Shared libraries can depend on other shared libraries.

*lib*.so*)

    ldd $file 2>/dev/null | awk '/\=\>/ { print $1 }' \
    	| print_deps --identifier so;

    # keep this for backward compatibility till we have converted
    # everything.

    ldd $file 2>/dev/null | awk '/\=\>/ { print $1 }';

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


# Perl libraries are hard to detect.  Need to also Look for #!*perl

*.pl) 

    perl.req $file;

;;

*.pm) 

    perl.req $file;

;;



# tcl libraries are hard to detect.  Need to also Look for #!*wish #!*tclsh

*.tcl) 

    tcl.req $file;

;;

# python libraries are hard to detect.  Need to also Look for #!*python

*.py) 

    python.req $file;

;;

# Binary executables can have any filename so let file tell us which
# ones are binary filenames. Assume that users do not name ELF binary
# files with names like runme.java

# Dependencies for html documenets are a bit ill defined. Lets try
# extracting the basename of all strings within "'s 
# precise globbing is hard so I use egrep instead of the case statement.

*)

    /usr/ucb/file -L $file 2>/dev/null | grep executable | cut -d: -f1 |\
 xargs ldd 2>/dev/null | awk '/\=\>/ { print $1 }' | xargs -n 1 basename;

    echo $file | egrep '\.((cgi)|(ps)|(pdf)|(png)|(jpg)|(gif)|(tiff)|(tif)|(xbm)|(html)|(htm)|(shtml)|(jhtml))$' | xargs cat | httprequires


	# All files are candidates for being an executable.  Let the
	# magic.req script figure out what should be considered
	# execuables.

	magic.req $file

;;


esac

done | sort -u | egrep -v \'$IGNORE_DEPS\'


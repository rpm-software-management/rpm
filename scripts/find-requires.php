#!/bin/sh
#####################################################################
#                                                                   #
# Check system dependences between php-pear modules                 #
#                                                                   #
# Pawe³ Go³aszewski <blues@ds.pg.gda.pl>                            #
# ------------------------------------------------------------------#
# TODO:                                                             #
# - extension_loaded - dependencies.                                #
# - some clean-up...                                                #
#####################################################################
if [ $# -lt 1 ]; then
	echo "You have to specify input file"
	exit 1
fi

for files in `echo $@`; do
	files=`echo $files | grep "\.php$"`
	if [ -n "$files" ]; then
		# Requires trough  new call:
		j=`cat $files | grep -i new | egrep "(=|return)" | egrep -v "^[[:space:]*]*(\/\/|#|\*|/\*)" | tr -d "\r" | egrep "[;|(|)|{|}|,][[:space:]*]*$" | awk -F "new " '{ print $2 }' | sed "s/[(|;|.]/ /g" | cut -f 1 -d " " | sed "s/^$.*//"`
		if [ -n "$j" ]; then
			for feature in $j; do
				echo "pear($feature)"
			done
			j=""
		fi
		# requires trough class extension
		k=`cat $files | egrep -i "(^Class.*extends)" | awk -F " extends " '{ print $2 }' | sed "s/{.*/ /" | cut -f 1 -d " " | tr -d "\r"`
		if [ -n "$k" ]; then
			for feature in $k; do
				echo "pear($feature)"
			done
			k=""
		fi
		# requires trough class:: call
		l=`cat $files | grep "::" | egrep -v "^[[:space:]*]*(\/\/|#|\*|/\*)" | sed "s/[(|'|!|\"|&|@|;]/ /g" | awk -F "::" '{ print $1 }' | sed "s/.*\ \([:alphanum:]*\)/\1/" | sed "s/^$.*//" | sed "s/[.]//g" | tr -d "\r"`
		if [ -n "$l" ]; then
			for feature in $l; do
				echo "pear($feature)"
			done
			l=""
		fi
	fi
done


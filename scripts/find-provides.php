#!/bin/sh
if [ $# -lt 1 ]; then
	echo "You have to specify input file"
	exit 1
fi

filelist=`echo $@`
for i in $filelist; do
	i=`echo $i | grep "\.php$"`
	if [ -n "$i" ]; then
		j=`cat $i |egrep  -i "^Class" |cut -f 2 -d " "| tr -d "\r"`
		if [ -n "$j" ]; then
			for p in $j; do
				echo "pear($p)"
			done
			j=""
		fi
	fi
done

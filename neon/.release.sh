#!/bin/sh
major=`echo $1 | sed "s/\..*//g"`
minor=`echo $1 | sed "s/^[0-9]*\.\(.*\)\.[0-9]*$/\1/g"`
rel=`echo $1 | sed "s/^.*\./\1/g"`
version=$1

for f in config.hw; do
in=$f.in
out=$f
sed -e "s/@VERSION@/$version/g" \
    -e "s/@MAJOR@/$major/g" \
    -e "s/@MINOR@/$minor/g" \
    -e "s/@RELEASE@/$release/g" < $in > $out
done

---
layout: default
title: rpm.org - Large File Support in RPM
---
# Large File Support in RPM

 RPM originally used 32 bit variables and tags for storing various (file) sizes. With the increasing sizes of files and packages RPM has added support for 64 bit values step by step. If you are an RPM API user you need to go theses steps with us or your code will fail if it enounters packages making use of these new features.

Even if you are not an RPM API user you might need to care. RPM packages can be bigger than 4GB nowadays. So if you are just handling them as files without looking into them your code still needs to be able to handle 64 bit file sizes.

## 64 Bit Tags

Several RPM tags exist in both the old 32 bit versions and newer 64 bit versions - with an added "LONG" in front of the name:

* RPMTAG_LONGSIGSIZE - Header + compressed payload size
* RPMTAG_LONGARCHIVESIZE - uncompressed payload size
* RPMTAG_LONGFILESIZES - array of file sizes
* RPMTAG_LONGSIZE - Sum of all file sizes 

If you are using the RPM API the LONG variant are available as extension tags if they are not physically present in the header. To get them you have to load the header with HEADERGET_EXT when calling headerGet(). In this case you can just use the LONG variants and ignore the old 32 bit tags completely. In the Python bindings and on the command line query interface those extension tags are always enabled.

It should be obvious that the code dealing with the 64 bit variants need to use 64 bit variables through out all steps. Be aware that RPM also moved from a more sloppy use of signed vs unsigned variables to a more strict handling when increasing the supported sizes from <2GB to <4GB. This means your code needs to handle those data types correctly, too, or it will fail in the gap between signed 32 integers and 64 bit integers. Note, all integer RPM tags are unsigned!

When dealing with the binary header or when not using HEADERGET_EXT API users need to check for both variants (with and without "LONG" in the name) and use the one present in the header at hand.

For the case you are writing RPM headers you should use the old 32 bit variants if the actual values fit in there. This maximizes forward compatibility for old implementations and tools that do not support the 64 bit variants properly.

## 64 Bit Sizes on the Command line

When querying on the command line those extensions are always enabled. If you have scripts using `rpm -q --qf` you also should add "long" to the name of the tags in question e.g.

`rpm -qp --qf="[%{filenames} %{longfilesizes}\n]"` instead of

`rpm -qp --qf="[%{filenames} %{filesizes}\n]"`. 

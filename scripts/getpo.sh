#!/bin/bash
#

if [ -z "$1" ] ; then
    echo "Usage:"
    echo "   $0 <rpm>..."
    exit 0
fi

while [ -n "$1" ] ; do

    [ -f $1 ] || { 
	echo "$1 could not be found (or is not a file)"
	shift
	continue
    }

    desc=$(rpm -qp $1 --qf "%{DESCRIPTION}" | \
	sed -e 's/\"/\\\"/g' -e 's/^/\"/g' -e 's/$/\\\\n\"/g')

    rpm -qp $1 --qf \
"# ========================================================
#: %{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}.rpm:1005 %{SOURCERPM}:1005
msgid \"%{NAME}(Description)\"
msgstr \"\"
$desc

#: %{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}.rpm:1016 %{SOURCERPM}:1016
msgid \"%{NAME}(Group)\"
msgstr \"%{GROUP}\"

#: %{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}.rpm:1004 %{SOURCERPM}:1004
msgid \"%{NAME}(Summary)\"
msgstr \"%{SUMMARY}\"

"
    # go to the next file
    shift
done


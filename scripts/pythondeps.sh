#!/bin/bash

[ $# -ge 1 ] || {
    cat > /dev/null
    exit 0
}

case $1 in
-P|--provides)
    shift
    # Match buildroot/payload paths of the form
    #    /PATH/OF/BUILDROOT/usr/bin/pythonMAJOR.MINOR
    # generating a line of the form
    #    python(abi) = MAJOR.MINOR
    # (Don't match against -config tools e.g. /usr/bin/python2.6-config)
    egrep '/usr/(bin/|libexec/platform-)python.\..$' \
        | sed -r -e "s@.*/usr/(bin/|libexec/(platform-))python(.\..)@\2python(abi) = \3@"
    ;;
-R|--requires)
    shift
    # Match buildroot paths of the form
    #    /PATH/OF/BUILDROOT/usr/lib/pythonMAJOR.MINOR/  and
    #    /PATH/OF/BUILDROOT/usr/lib64/pythonMAJOR.MINOR/
    # generating (uniqely) lines of the form:
    #    python(abi) = MAJOR.MINOR
    egrep '/usr/lib[^/]*/(platform-|)python.\../.*' \
        | sed -r -e "s@.*/usr/lib[^/]*/(platform-|)python(.\..)/.*@\1python(abi) = \2@g" \
        | sort | uniq
    ;;
esac

exit 0

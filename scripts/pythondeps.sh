#!/bin/sh

[ $# -ge 1 ] || {
    cat > /dev/null
    exit 0
}

PYVER=`python -c "import sys; v=sys.version_info[:2]; print '%d.%d'%v"`
case $1 in
-P|--provides)
    shift
    grep /usr/bin/python 2> /dev/null && echo "python(abi) = ${PYVER}"
    exit 0
    ;;
-R|--requires)
    shift
    cat > /dev/null
    echo "python(abi) = ${PYVER}"
    exit 0
    ;;
esac

exit 0

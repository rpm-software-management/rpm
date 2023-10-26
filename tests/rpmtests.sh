#!/bin/bash
#
# Wrapper around rpmtests that adds a couple of useful options/overrides:
#   -C, --directory DIR
#       like original -C but loads atlocal from script's directory if missing
#   -L, --log
#       print the test log when done

SCRIPT_DIR=$(dirname $(readlink -f $0))
SCRIPT_FILES="atlocal mktree.common"

PRINT_LOG=0

while [ $# != 0 ]; do
    case $1 in
        -C | --directory )
            cd "$2"
            shift
        ;;
        -L | --log )
            PRINT_LOG=1
        ;;
        *)
            break
        ;;
    esac
    shift
done

# Symlink script files into $PWD, prefer local versions though
for file in $SCRIPT_FILES; do
    [ -f "$file" ] || ln -s $SCRIPT_DIR/$file .
done

# Run the test suite
$SCRIPT_DIR/rpmtests "$@"; RC=$?
[ $PRINT_LOG == 1 ] && cat rpmtests.log

# Clean up the symlinks
for file in $SCRIPT_FILES; do
    [ -L "$file" ] && rm "$file"
done

exit $RC

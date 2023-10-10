#!/bin/bash
#
# Wrapper for rpmtests that looks for atlocal in the script's directory instead
# of $PWD or the one specified with -C.  In addition, implements the -L | --log
# option to print the test log when done.

SCRIPT_DIR=$(dirname $(readlink -f $0))
SCRIPT_FILES="rpmtests atlocal mktree.common"

TARGET_DIR=$PWD
PRINT_LOG=0

cd "$SCRIPT_DIR"

while [ $# != 0 ]; do
    case $1 in
        -C | --directory )
            TARGET_DIR="$2"
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

# Symlink script files into $TARGET_DIR, prefer local versions though
for file in $SCRIPT_FILES; do
    [ -f "$TARGET_DIR/$file" ] || ln -s $PWD/$file $TARGET_DIR/
done

cd "$TARGET_DIR"

# Run the test suite
./rpmtests "$@"; RC=$?
[ $PRINT_LOG == 1 ] && cat rpmtests.log

# Clean up the symlinks
for file in $SCRIPT_FILES; do
    [ -L "$file" ] && rm "$file"
done

exit $RC

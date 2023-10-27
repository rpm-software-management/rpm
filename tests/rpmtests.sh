#!/bin/bash
#
# Wrapper around rpmtests that adds a couple of useful options/overrides:
#   -C, --directory DIR
#       like original -C but loads atlocal from script's directory if missing
#   -L, --log
#       print the test log when done
#   -S, --shell [CMD]
#       create an empty test and run a shell (or CMD) in it
#   -R, --reset
#       delete the test created with --shell

SCRIPT_DIR=$(dirname $(readlink -f $0))
SHELL_DIR=$PWD/rpmtests.dir/shell
PRINT_LOG=0
RUN_SHELL=0
RC=0

fixperms()
{
    chmod -Rf u+rwX "$@"
}

while [ $# != 0 ]; do
    case $1 in
        -C | --directory )
            cd "$2"
            shift
        ;;
        -L | --log )
            PRINT_LOG=1
        ;;
        -S | --shell )
            RUN_SHELL=1
        ;;
        -R | --reset )
            rm -rf "$SHELL_DIR"
            exit
        ;;
        *)
            break
        ;;
    esac
    shift
done

[ -f atlocal ] || ln -s $SCRIPT_DIR/atlocal .

# Run the test suite (or a shell)
if [ $RUN_SHELL == 0 ]; then
    $SCRIPT_DIR/rpmtests "$@"; RC=$?
    [ $PRINT_LOG == 1 ] && cat rpmtests.log
    fixperms rpmtests.dir/*/{diff,work}
else
    # Emulate a single, writable test
    set -a
    source ./atlocal
    trap : INT
    RPMTEST=$SHELL_DIR/tree
    snapshot mount $SHELL_DIR
    if [ $# == 0 ]; then
        $SHELL
    else
        "$@"
    fi
    snapshot umount
    fixperms $SHELL_DIR
fi

[ -L atlocal ] && rm atlocal

exit $RC

# Automatically built by dist/s_test; may require local editing.

set tclsh_path @TCL_TCLSH@
set tcllib .libs/libdb_tcl-@DB_VERSION_MAJOR@.@DB_VERSION_MINOR@@LIBTSO_MODSUFFIX@

set rpc_server localhost
set rpc_path .
set rpc_testdir $rpc_path/TESTDIR

set src_root @srcdir@/..
set test_path @srcdir@/../test

global testdir
set testdir ./TESTDIR

global dict
global util_path

global is_hp_test
global is_qnx_test
global is_windows_test

set KILL "@db_cv_path_kill@"

# Hide some symbols
/public class db_java/s!public!/* package */!
/public [^(]* delete/s!public!/* package */!
/public [^(]* [A-Za-z_]*0(/s!public!/* package */!

# Mark methods that don't throw exceptions
s!public [^(]*get_version_[a-z]*([^)]*)!& /* no exception */!
s!public [^(]*err[a-z_]*([^)]*)!& /* no exception */!
s!public [^(]*log_compare([^)]*)!& /* no exception */!
s!public [^(]* feedback([^)]*)!& /* no exception */!

# Mark methods that throw special exceptions
s!\(public [^(]* open([^)]*)\) {!\1 throws DbException, java.io.FileNotFoundException {!
s!\(public [^(]* dbremove([^)]*)\) {!\1 throws DbException, DbDeadlockException, DbLockNotGrantedException, java.io.FileNotFoundException {!
s!\(public [^(]* dbrename([^)]*)\) {!\1 throws DbException, DbDeadlockException, DbLockNotGrantedException, java.io.FileNotFoundException {!
s!\(public [^(]*remove([^)]*)\) {!\1 throws DbException, java.io.FileNotFoundException {!
s!\(public [^(]*rename([^)]*)\) {!\1 throws DbException, java.io.FileNotFoundException {!

# Everything else throws a DbException
s!\(public [^(]*([^)]*)\);!\1 throws DbException;!
s!\(public [^(]*([^)]*)\) *{!\1 throws DbException {!

# Add initialize methods for Java parts of Db and DbEnv
s!\.new_DbEnv(.*$!&\
    initialize();!
s!\.new_Db(.*$!&\
    initialize(dbenv);!

/*-
* See the file LICENSE for redistribution information.
*
* Copyright (c) 2002-2004
*	Sleepycat Software.  All rights reserved.
*
* $Id: SecondaryConfig.java,v 1.3 2004/08/06 21:56:40 mjc Exp $
*/

package com.sleepycat.db;

import com.sleepycat.db.internal.Db;
import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbEnv;
import com.sleepycat.db.internal.DbTxn;

public class SecondaryConfig extends DatabaseConfig implements Cloneable {
    /*
     * For internal use, to allow null as a valid value for
     * the config parameter.
     */
    public static final SecondaryConfig DEFAULT = new SecondaryConfig();

    /* package */
    static SecondaryConfig checkNull(SecondaryConfig config) {
        return (config == null) ? DEFAULT : config;
    }

    private boolean allowPopulate;
    private SecondaryKeyCreator keyCreator;

    public SecondaryConfig() {
    }

    public void setKeyCreator(final SecondaryKeyCreator keyCreator) {
        this.keyCreator = keyCreator;
    }

    public SecondaryKeyCreator getKeyCreator() {
        return keyCreator;
    }

    public void setAllowPopulate(final boolean allowPopulate) {
        this.allowPopulate = allowPopulate;
    }

    public boolean getAllowPopulate() {
        return allowPopulate;
    }

    /* package */
    Db openSecondaryDatabase(final DbEnv dbenv,
                             final DbTxn txn,
                             final String fileName,
                             final String databaseName,
                             final Db primary)
        throws DatabaseException, java.io.FileNotFoundException {

        int associateFlags = 0;
        associateFlags |= allowPopulate ? DbConstants.DB_CREATE : 0;
        if (getTransactional() && txn == null)
            associateFlags |= DbConstants.DB_AUTO_COMMIT;

        final Db db = super.openDatabase(dbenv, txn, fileName, databaseName);
        boolean succeeded = false;
        try {
            primary.associate(txn, db, keyCreator, associateFlags);
            succeeded = true;
            return db;
        } finally {
            if (!succeeded)
                try {
                    db.close(0);
                } catch (Throwable t) {
                    // Ignore it -- there is already an exception in flight.
                }
        }
    }

    /* package */
    SecondaryConfig(final Db db)
        throws DatabaseException {

        super(db);

        // XXX: There is no way to find out whether allowPopulate was set.
        allowPopulate = false;
        keyCreator = db.get_seckey_create();
    }
}


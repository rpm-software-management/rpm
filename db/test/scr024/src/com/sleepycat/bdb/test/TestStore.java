/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2003
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: TestStore.java,v 1.1 2003/12/15 21:44:54 jbj Exp $
 */

package com.sleepycat.bdb.test;

import com.sleepycat.bdb.bind.ByteArrayFormat;
import com.sleepycat.bdb.bind.DataBinding;
import com.sleepycat.bdb.bind.DataFormat;
import com.sleepycat.bdb.bind.EntityBinding;
import com.sleepycat.db.Db;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbException;
import com.sleepycat.bdb.CurrentTransaction;
import com.sleepycat.bdb.RecordNumberBinding;
import com.sleepycat.bdb.RecordNumberFormat;
import com.sleepycat.bdb.DataIndex;
import com.sleepycat.bdb.DataStore;
import com.sleepycat.bdb.PrimaryKeyAssigner;
import java.io.IOException;

/**
 * @author Mark Hayes
 */
class TestStore {

    static final ByteArrayFormat BYTE_FORMAT = new ByteArrayFormat();
    static final RecordNumberFormat RECNO_FORMAT = new RecordNumberFormat();
    static final TestKeyExtractor BYTE_EXTRACTOR = new TestKeyExtractor(false);
    static final TestKeyExtractor RECNO_EXTRACTOR = new TestKeyExtractor(true);
    static final DataBinding VALUE_BINDING = new TestDataBinding();
    static final DataBinding BYTE_KEY_BINDING = VALUE_BINDING;
    static final DataBinding RECNO_KEY_BINDING =
            new RecordNumberBinding(RECNO_FORMAT);
    static final EntityBinding BYTE_ENTITY_BINDING =
            new TestEntityBinding(false);
    static final EntityBinding RECNO_ENTITY_BINDING =
            new TestEntityBinding(true);
    static final PrimaryKeyAssigner BYTE_KEY_ASSIGNER =
            new TestKeyAssigner(false);
    static final PrimaryKeyAssigner RECNO_KEY_ASSIGNER =
            new TestKeyAssigner(true);

    static final TestStore BTREE_UNIQ =
        new TestStore("btree-uniq", Db.DB_BTREE, 0);
    static final TestStore BTREE_DUP =
        new TestStore("btree-dup", Db.DB_BTREE, Db.DB_DUP);
    static final TestStore BTREE_DUPSORT =
        new TestStore("btree-dupsort", Db.DB_BTREE, Db.DB_DUPSORT);
    static final TestStore BTREE_RECNUM =
        new TestStore("btree-recnum", Db.DB_BTREE, Db.DB_RECNUM);
    static final TestStore HASH_UNIQ =
        new TestStore("hash-uniq", Db.DB_HASH, 0);
    static final TestStore HASH_DUP =
        new TestStore("hash-dup", Db.DB_HASH, Db.DB_DUP);
    static final TestStore HASH_DUPSORT =
        new TestStore("hash-dupsort", Db.DB_HASH, Db.DB_DUPSORT);
    static final TestStore QUEUE =
        new TestStore("queue", Db.DB_QUEUE, 0);
    static final TestStore RECNO =
        new TestStore("recno", Db.DB_RECNO, 0);
    static final TestStore RECNO_RENUM =
        new TestStore("recno-renum", Db.DB_RECNO, Db.DB_RENUMBER);
    static {
        BTREE_UNIQ.indexOf = BTREE_UNIQ;
        BTREE_DUP.indexOf = null;   // indexes must use sorted duplicates
        BTREE_DUPSORT.indexOf = BTREE_UNIQ;
        BTREE_RECNUM.indexOf = BTREE_RECNUM;
        HASH_UNIQ.indexOf = HASH_UNIQ;
        HASH_DUP.indexOf = null;    // indexes must use sorted duplicates
        HASH_DUPSORT.indexOf = HASH_UNIQ;
        QUEUE.indexOf = QUEUE;
        RECNO.indexOf = RECNO;
        RECNO_RENUM.indexOf = null; // indexes must have stable keys
    }

    static final TestStore[] ALL = {
        BTREE_UNIQ,
        BTREE_DUP,
        BTREE_DUPSORT,
        BTREE_RECNUM,
        HASH_UNIQ,
        HASH_DUP,
        HASH_DUPSORT,
        QUEUE,
        RECNO,
        RECNO_RENUM,
        /*
        */
    };

    private String name;
    private int type;
    private int flags;
    private TestStore indexOf;
    private boolean isRecNumFormat;
    private DataFormat keyFormat;

    private TestStore(String name, int type, int flags) {

        this.name = name;
        this.type = type;
        this.flags = flags;

        isRecNumFormat = type == Db.DB_QUEUE || type == Db.DB_RECNO ||
                        (type == Db.DB_BTREE && (flags & Db.DB_RECNUM) != 0);
        if (isRecNumFormat) {
            keyFormat = RECNO_FORMAT;
        } else {
            keyFormat = BYTE_FORMAT;
        }
    }

    DataBinding getValueBinding() {

        return VALUE_BINDING;
    }

    DataBinding getKeyBinding() {

        return isRecNumFormat ? RECNO_KEY_BINDING : BYTE_KEY_BINDING;
    }

    EntityBinding getEntityBinding() {

        return isRecNumFormat ? RECNO_ENTITY_BINDING : BYTE_ENTITY_BINDING;
    }

    PrimaryKeyAssigner getKeyAssigner() {

        return isRecNumFormat ? RECNO_KEY_ASSIGNER : BYTE_KEY_ASSIGNER;
    }

    String getName() {

        return name;
    }

    boolean isOrdered() {

        return type != Db.DB_HASH;
    }

    boolean isQueueOrRecno() {

        return type == Db.DB_QUEUE || type == Db.DB_RECNO;
    }

    boolean areDuplicatesAllowed() {

        return (flags & (Db.DB_DUP | Db.DB_DUPSORT)) != 0;
    }

    boolean hasRecNumAccess() {

        return isRecNumFormat || (flags & Db.DB_RECNUM) != 0;
    }

    boolean areKeysRenumbered() {

        return hasRecNumAccess() &&
                (type == Db.DB_BTREE || (flags & Db.DB_RENUMBER) != 0);
    }

    TestStore getIndexOf() {

        return indexOf;
    }

    DataStore open(DbEnv env, String fileName)
        throws IOException, DbException {

        int fixedLen = (isQueueOrRecno() ? 1 : 0);
        Db db = openDb(env, fileName, fixedLen);
        PrimaryKeyAssigner keyAssigner =
            isQueueOrRecno() ?  null : getKeyAssigner();
        return new DataStore(db, keyFormat, BYTE_FORMAT, keyAssigner);
    }

    DataIndex openIndex(DataStore store, String fileName)
        throws IOException, DbException {

        int fixedLen = (isQueueOrRecno() ? 4 : 0);
        Db db = openDb(store.getEnv(), fileName, fixedLen);
        return new DataIndex(store, db, keyFormat,
                        (isRecNumFormat ? RECNO_EXTRACTOR : BYTE_EXTRACTOR));
    }

    private Db openDb(DbEnv env, String fileName, int fixedLen)
        throws IOException, DbException {

        Db db = new Db(env, 0);
        db.setFlags(flags);
        if (fixedLen > 0) {
            db.setRecordLength(fixedLen);
            db.setRecordPad(0);
        }
        int openFlags = Db.DB_DIRTY_READ | Db.DB_CREATE;
        if (CurrentTransaction.getInstance(env) != null)
            openFlags |= Db.DB_AUTO_COMMIT;
        db.open(null, fileName, null, type, openFlags, 0);
        return db;
    }
}

/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002-2006
 *	Oracle Corporation.  All rights reserved.
 *
 * $Id: DatabaseConfig.java,v 12.6 2006/08/24 14:46:07 bostic Exp $
 */

package com.sleepycat.db;

import com.sleepycat.db.internal.Db;
import com.sleepycat.db.internal.DbConstants;
import com.sleepycat.db.internal.DbEnv;
import com.sleepycat.db.internal.DbTxn;
import com.sleepycat.db.internal.DbUtil;

public class DatabaseConfig implements Cloneable {
    /*
     * For internal use, final to allow null as a valid value for
     * the config parameter.
     */
    public static final DatabaseConfig DEFAULT = new DatabaseConfig();

    /* package */
    static DatabaseConfig checkNull(DatabaseConfig config) {
        return (config == null) ? DEFAULT : config;
    }

    /* Parameters */
    private DatabaseType type = DatabaseType.UNKNOWN;
    private int mode = 0644;
    private int btMinKey = 0;
    private int byteOrder = 0;
    private long cacheSize = 0L;
    private int cacheCount = 0;
    private java.io.OutputStream errorStream = null;
    private String errorPrefix = null;
    private int hashFillFactor = 0;
    private int hashNumElements = 0;
    private java.io.OutputStream messageStream = null;
    private int pageSize = 0;
    private String password = null;
    private int queueExtentSize = 0;
    private int recordDelimiter = 0;
    private int recordLength = 0;
    private int recordPad = -1;       // Zero is a valid, non-default value.
    private java.io.File recordSource = null;

    /* Flags */
    private boolean allowCreate = false;
    private boolean btreeRecordNumbers = false;
    private boolean checksum = false;
    private boolean readUncommitted = false;
    private boolean encrypted = false;
    private boolean exclusiveCreate = false;
    private boolean multiversion = false;
    private boolean noMMap = false;
    private boolean queueInOrder = false;
    private boolean readOnly = false;
    private boolean renumbering = false;
    private boolean reverseSplitOff = false;
    private boolean sortedDuplicates = false;
    private boolean snapshot = false;
    private boolean unsortedDuplicates = false;
    private boolean transactional = false;
    private boolean transactionNotDurable = false;
    private boolean truncate = false;
    private boolean xaCreate = false;

    private java.util.Comparator btreeComparator = null;
    private BtreePrefixCalculator btreePrefixCalculator = null;
    private java.util.Comparator duplicateComparator = null;
    private FeedbackHandler feedbackHandler = null;
    private ErrorHandler errorHandler = null;
    private MessageHandler messageHandler = null;
    private Hasher hasher = null;
    private RecordNumberAppender recnoAppender = null;
    private PanicHandler panicHandler = null;

    public DatabaseConfig() {
    }

    public void setAllowCreate(final boolean allowCreate) {
        this.allowCreate = allowCreate;
    }

    public boolean getAllowCreate() {
        return allowCreate;
    }

    public void setBtreeComparator(final java.util.Comparator btreeComparator) {
        this.btreeComparator = btreeComparator;
    }

    public java.util.Comparator getBtreeComparator() {
        return btreeComparator;
    }

    public void setBtreeMinKey(final int btMinKey) {
        this.btMinKey = btMinKey;
    }

    public int getBtreeMinKey() {
        return btMinKey;
    }

    public void setByteOrder(final int byteOrder) {
        this.byteOrder = byteOrder;
    }

    public int getByteOrder() {
        return byteOrder;
    }

    public boolean getByteSwapped() {
        return byteOrder != 0 && byteOrder != DbUtil.default_lorder();
    }

    public void setBtreePrefixCalculator(
            final BtreePrefixCalculator btreePrefixCalculator) {
        this.btreePrefixCalculator = btreePrefixCalculator;
    }

    public BtreePrefixCalculator getBtreePrefixCalculator() {
        return btreePrefixCalculator;
    }

    public void setCacheSize(final long cacheSize) {
        this.cacheSize = cacheSize;
    }

    public long getCacheSize() {
        return cacheSize;
    }

    public void setCacheCount(final int cacheCount) {
        this.cacheCount = cacheCount;
    }

    public int getCacheCount() {
        return cacheCount;
    }

    public void setChecksum(final boolean checksum) {
        this.checksum = checksum;
    }

    public boolean getChecksum() {
        return checksum;
    }

    public void setReadUncommitted(final boolean readUncommitted) {
        this.readUncommitted = readUncommitted;
    }

    public boolean getReadUncommitted() {
        return readUncommitted;
    }

    /** @deprecated */
    public void setDirtyRead(final boolean dirtyRead) {
        setReadUncommitted(dirtyRead);
    }

    /** @deprecated */
    public boolean getDirtyRead() {
        return getReadUncommitted();
    }

    public void setDuplicateComparator(
            final java.util.Comparator duplicateComparator) {
        this.duplicateComparator = duplicateComparator;
    }

    public java.util.Comparator getDuplicateComparator() {
        return duplicateComparator;
    }

    public void setEncrypted(final String password) {
        this.password = password;
    }

    public boolean getEncrypted() {
        return (password != null);
    }

    public void setErrorHandler(final ErrorHandler errorHandler) {
        this.errorHandler = errorHandler;
    }

    public ErrorHandler getErrorHandler() {
        return errorHandler;
    }

    public void setErrorPrefix(final String errorPrefix) {
        this.errorPrefix = errorPrefix;
    }

    public String getErrorPrefix() {
        return errorPrefix;
    }

    public void setErrorStream(final java.io.OutputStream errorStream) {
        this.errorStream = errorStream;
    }

    public java.io.OutputStream getErrorStream() {
        return errorStream;
    }

    public void setExclusiveCreate(final boolean exclusiveCreate) {
        this.exclusiveCreate = exclusiveCreate;
    }

    public boolean getExclusiveCreate() {
        return exclusiveCreate;
    }

    public void setFeedbackHandler(final FeedbackHandler feedbackHandler) {
        this.feedbackHandler = feedbackHandler;
    }

    public FeedbackHandler getFeedbackHandler() {
        return feedbackHandler;
    }

    public void setHashFillFactor(final int hashFillFactor) {
        this.hashFillFactor = hashFillFactor;
    }

    public int getHashFillFactor() {
        return hashFillFactor;
    }

    public void setHasher(final Hasher hasher) {
        this.hasher = hasher;
    }

    public Hasher getHasher() {
        return hasher;
    }

    public void setHashNumElements(final int hashNumElements) {
        this.hashNumElements = hashNumElements;
    }

    public int getHashNumElements() {
        return hashNumElements;
    }

    public void setMessageHandler(final MessageHandler messageHandler) {
        this.messageHandler = messageHandler;
    }

    public MessageHandler getMessageHandler() {
        return messageHandler;
    }

    public void setMessageStream(final java.io.OutputStream messageStream) {
        this.messageStream = messageStream;
    }

    public java.io.OutputStream getMessageStream() {
        return messageStream;
    }

    public void setMode(final int mode) {
        this.mode = mode;
    }

    public long getMode() {
        return mode;
    }

    public void setMultiversion(final boolean Multiversion) {
        this.multiversion = multiversion;
    }

    public boolean getMultiversion() {
        return multiversion;
    }

    public void setNoMMap(final boolean noMMap) {
        this.noMMap = noMMap;
    }

    public boolean getNoMMap() {
        return noMMap;
    }

    public void setPageSize(final int pageSize) {
        this.pageSize = pageSize;
    }

    public int getPageSize() {
        return pageSize;
    }

    public void setPanicHandler(final PanicHandler panicHandler) {
        this.panicHandler = panicHandler;
    }

    public PanicHandler getPanicHandler() {
        return panicHandler;
    }

    public void setQueueExtentSize(final int queueExtentSize) {
        this.queueExtentSize = queueExtentSize;
    }

    public int getQueueExtentSize() {
        return queueExtentSize;
    }

    public void setQueueInOrder(final boolean queueInOrder) {
        this.queueInOrder = queueInOrder;
    }

    public boolean getQueueInOrder() {
        return queueInOrder;
    }

    public void setReadOnly(final boolean readOnly) {
        this.readOnly = readOnly;
    }

    public boolean getReadOnly() {
        return readOnly;
    }

    public void setRecordNumberAppender(
            final RecordNumberAppender recnoAppender) {
        this.recnoAppender = recnoAppender;
    }

    public RecordNumberAppender getRecordNumberAppender() {
        return recnoAppender;
    }

    public void setRecordDelimiter(final int recordDelimiter) {
        this.recordDelimiter = recordDelimiter;
    }

    public int getRecordDelimiter() {
        return recordDelimiter;
    }

    public void setRecordLength(final int recordLength) {
        this.recordLength = recordLength;
    }

    public int getRecordLength() {
        return recordLength;
    }

    public void setBtreeRecordNumbers(final boolean btreeRecordNumbers) {
        this.btreeRecordNumbers = btreeRecordNumbers;
    }

    public boolean getBtreeRecordNumbers() {
        return btreeRecordNumbers;
    }

    public void setRecordPad(final int recordPad) {
        this.recordPad = recordPad;
    }

    public int getRecordPad() {
        return recordPad;
    }

    public void setRecordSource(final java.io.File recordSource) {
        this.recordSource = recordSource;
    }

    public java.io.File getRecordSource() {
        return recordSource;
    }

    public void setRenumbering(final boolean renumbering) {
        this.renumbering = renumbering;
    }

    public boolean getRenumbering() {
        return renumbering;
    }

    public void setReverseSplitOff(final boolean reverseSplitOff) {
        this.reverseSplitOff = reverseSplitOff;
    }

    public boolean getReverseSplitOff() {
        return reverseSplitOff;
    }

    public void setSortedDuplicates(final boolean sortedDuplicates) {
        this.sortedDuplicates = sortedDuplicates;
    }

    public boolean getSortedDuplicates() {
        return sortedDuplicates;
    }

    public void setUnsortedDuplicates(final boolean unsortedDuplicates) {
        this.unsortedDuplicates = unsortedDuplicates;
    }

    public boolean getUnsortedDuplicates() {
        return unsortedDuplicates;
    }

    public void setSnapshot(final boolean snapshot) {
        this.snapshot = snapshot;
    }

    public boolean getSnapshot() {
        return snapshot;
    }

    public boolean getTransactional() {
        return transactional;
    }

    public void setTransactional(final boolean transactional) {
        this.transactional = transactional;
    }

    public void setTransactionNotDurable(final boolean transactionNotDurable) {
        this.transactionNotDurable = transactionNotDurable;
    }

    public boolean getTransactionNotDurable() {
        return transactionNotDurable;
    }

    public void setTruncate(final boolean truncate) {
        this.truncate = truncate;
    }

    public boolean getTruncate() {
        return truncate;
    }

    public void setType(final DatabaseType type) {
        this.type = type;
    }

    public DatabaseType getType() {
        return type;
    }

    public void setXACreate(final boolean xaCreate) {
        this.xaCreate = xaCreate;
    }

    public boolean getXACreate() {
        return xaCreate;
    }

    /* package */
    Db createDatabase(final DbEnv dbenv)
        throws DatabaseException {

        int createFlags = 0;

        createFlags |= xaCreate ? DbConstants.DB_XA_CREATE : 0;
        return new Db(dbenv, createFlags);
    }

    /* package */
    Db openDatabase(final DbEnv dbenv,
                    final DbTxn txn,
                    final String fileName,
                    final String databaseName)
        throws DatabaseException, java.io.FileNotFoundException {

        final Db db = createDatabase(dbenv);
        // The DB_THREAD flag is inherited from the environment
        // (defaulting to ON if no environment handle is supplied).
        boolean threaded = (dbenv == null ||
            (dbenv.get_open_flags() & DbConstants.DB_THREAD) != 0);

        int openFlags = 0;
        openFlags |= allowCreate ? DbConstants.DB_CREATE : 0;
        openFlags |= readUncommitted ? DbConstants.DB_READ_UNCOMMITTED : 0;
        openFlags |= exclusiveCreate ? DbConstants.DB_EXCL : 0;
        openFlags |= multiversion ? DbConstants.DB_MULTIVERSION : 0;
        openFlags |= noMMap ? DbConstants.DB_NOMMAP : 0;
        openFlags |= readOnly ? DbConstants.DB_RDONLY : 0;
        openFlags |= threaded ? DbConstants.DB_THREAD : 0;
        openFlags |= truncate ? DbConstants.DB_TRUNCATE : 0;

        if (transactional && txn == null)
            openFlags |= DbConstants.DB_AUTO_COMMIT;

        boolean succeeded = false;
        try {
            configureDatabase(db, DEFAULT);
            db.open(txn, fileName, databaseName, type.getId(), openFlags, mode);
            succeeded = true;
            return db;
        } finally {
            if (!succeeded)
                try {
                    db.close(0);
                } catch (Throwable t) {
                    // Ignore it -- an exception is already in flight.
                }
        }
    }

    /* package */
    void configureDatabase(final Db db, final DatabaseConfig oldConfig)
        throws DatabaseException {

        int dbFlags = 0;
        dbFlags |= checksum ? DbConstants.DB_CHKSUM : 0;
        dbFlags |= btreeRecordNumbers ? DbConstants.DB_RECNUM : 0;
        dbFlags |= queueInOrder ? DbConstants.DB_INORDER : 0;
        dbFlags |= renumbering ? DbConstants.DB_RENUMBER : 0;
        dbFlags |= reverseSplitOff ? DbConstants.DB_REVSPLITOFF : 0;
        dbFlags |= sortedDuplicates ? DbConstants.DB_DUPSORT : 0;
        dbFlags |= snapshot ? DbConstants.DB_SNAPSHOT : 0;
        dbFlags |= unsortedDuplicates ? DbConstants.DB_DUP : 0;
        dbFlags |= transactionNotDurable ? DbConstants.DB_TXN_NOT_DURABLE : 0;
        if (!db.getPrivateDbEnv())
                dbFlags |= (password != null) ? DbConstants.DB_ENCRYPT : 0;

        if (dbFlags != 0)
            db.set_flags(dbFlags);

        if (btMinKey != oldConfig.btMinKey)
            db.set_bt_minkey(btMinKey);
        if (byteOrder != oldConfig.byteOrder)
            db.set_lorder(byteOrder);
        if (cacheSize != oldConfig.cacheSize ||
            cacheCount != oldConfig.cacheCount)
            db.set_cachesize(cacheSize, cacheCount);
        if (errorStream != oldConfig.errorStream)
            db.set_error_stream(errorStream);
        if (errorPrefix != oldConfig.errorPrefix)
            db.set_errpfx(errorPrefix);
        if (hashFillFactor != oldConfig.hashFillFactor)
            db.set_h_ffactor(hashFillFactor);
        if (hashNumElements != oldConfig.hashNumElements)
            db.set_h_nelem(hashNumElements);
        if (messageStream != oldConfig.messageStream)
            db.set_message_stream(messageStream);
        if (pageSize != oldConfig.pageSize)
            db.set_pagesize(pageSize);
        if (password != oldConfig.password && db.getPrivateDbEnv())
            db.set_encrypt(password, DbConstants.DB_ENCRYPT_AES);
        if (queueExtentSize != oldConfig.queueExtentSize)
            db.set_q_extentsize(queueExtentSize);
        if (recordDelimiter != oldConfig.recordDelimiter)
            db.set_re_delim(recordDelimiter);
        if (recordLength != oldConfig.recordLength)
            db.set_re_len(recordLength);
        if (recordPad != oldConfig.recordPad)
            db.set_re_pad(recordPad);
        if (recordSource != oldConfig.recordSource)
            db.set_re_source(
                (recordSource == null) ? null : recordSource.toString());

        if (btreeComparator != oldConfig.btreeComparator)
            db.set_bt_compare(btreeComparator);
        if (btreePrefixCalculator != oldConfig.btreePrefixCalculator)
            db.set_bt_prefix(btreePrefixCalculator);
        if (duplicateComparator != oldConfig.duplicateComparator)
            db.set_dup_compare(duplicateComparator);
        if (feedbackHandler != oldConfig.feedbackHandler)
            db.set_feedback(feedbackHandler);
        if (errorHandler != oldConfig.errorHandler)
            db.set_errcall(errorHandler);
        if (hasher != oldConfig.hasher)
            db.set_h_hash(hasher);
        if (messageHandler != oldConfig.messageHandler)
            db.set_msgcall(messageHandler);
        if (recnoAppender != oldConfig.recnoAppender)
            db.set_append_recno(recnoAppender);
        if (panicHandler != oldConfig.panicHandler)
            db.set_paniccall(panicHandler);
    }

    /* package */
    DatabaseConfig(final Db db)
        throws DatabaseException {

        type = DatabaseType.fromInt(db.get_type());

        final int openFlags = db.get_open_flags();
        allowCreate = (openFlags & DbConstants.DB_CREATE) != 0;
        readUncommitted = (openFlags & DbConstants.DB_READ_UNCOMMITTED) != 0;
        exclusiveCreate = (openFlags & DbConstants.DB_EXCL) != 0;
        multiversion = (openFlags & DbConstants.DB_MULTIVERSION) != 0;
        noMMap = (openFlags & DbConstants.DB_NOMMAP) != 0;
        readOnly = (openFlags & DbConstants.DB_RDONLY) != 0;
        truncate = (openFlags & DbConstants.DB_TRUNCATE) != 0;

        final int dbFlags = db.get_flags();
        checksum = (dbFlags & DbConstants.DB_CHKSUM) != 0;
        btreeRecordNumbers = (dbFlags & DbConstants.DB_RECNUM) != 0;
        queueInOrder = (dbFlags & DbConstants.DB_INORDER) != 0;
        renumbering = (dbFlags & DbConstants.DB_RENUMBER) != 0;
        reverseSplitOff = (dbFlags & DbConstants.DB_REVSPLITOFF) != 0;
        sortedDuplicates = (dbFlags & DbConstants.DB_DUPSORT) != 0;
        snapshot = (dbFlags & DbConstants.DB_SNAPSHOT) != 0;
        unsortedDuplicates = (dbFlags & DbConstants.DB_DUP) != 0;
        transactionNotDurable = (dbFlags & DbConstants.DB_TXN_NOT_DURABLE) != 0;

        if (type == DatabaseType.BTREE) {
            btMinKey = db.get_bt_minkey();
        }
        byteOrder = db.get_lorder();
        // Call get_cachesize* on the DbEnv to avoid this error:
        //   DB->get_cachesize: method not permitted in shared environment
        cacheSize = db.get_env().get_cachesize();
        cacheCount = db.get_env().get_cachesize_ncache();
        errorStream = db.get_error_stream();
        errorPrefix = db.get_errpfx();
        if (type == DatabaseType.HASH) {
            hashFillFactor = db.get_h_ffactor();
            hashNumElements = db.get_h_nelem();
        }
        messageStream = db.get_message_stream();
        // Not available by design
        password = ((dbFlags & DbConstants.DB_ENCRYPT) != 0) ? "" : null;
        if (type == DatabaseType.QUEUE) {
            queueExtentSize = db.get_q_extentsize();
        }
        if (type == DatabaseType.QUEUE || type == DatabaseType.RECNO) {
            recordLength = db.get_re_len();
            recordPad = db.get_re_pad();
        }
        if (type == DatabaseType.RECNO) {
            recordDelimiter = db.get_re_delim();
            recordSource = (db.get_re_source() == null) ? null :
                new java.io.File(db.get_re_source());
        }
        transactional = db.get_transactional();

        btreeComparator = db.get_bt_compare();
        btreePrefixCalculator = db.get_bt_prefix();
        duplicateComparator = db.get_dup_compare();
        feedbackHandler = db.get_feedback();
        errorHandler = db.get_errcall();
        hasher = db.get_h_hash();
        messageHandler = db.get_msgcall();
        recnoAppender = db.get_append_recno();
        panicHandler = db.get_paniccall();
    }
}

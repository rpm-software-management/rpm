/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2003
 *      Sleepycat Software.  All rights reserved.
 *
 * $Id: StoredClassCatalog.java,v 1.1 2003/12/15 21:44:11 jbj Exp $
 */

package com.sleepycat.bdb;

import com.sleepycat.bdb.bind.serial.ClassCatalog;
import com.sleepycat.bdb.util.IOExceptionWrapper;
import com.sleepycat.bdb.util.UtfOps;
import com.sleepycat.db.Db;
import com.sleepycat.db.Dbc;
import com.sleepycat.db.DbEnv;
import com.sleepycat.db.DbException;
import com.sleepycat.db.DbTxn;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamClass;
import java.io.Serializable;
import java.math.BigInteger;
import java.util.HashMap;

/**
 * Java serialization catalog used for compact storage of database objects.
 *
 * @author Mark Hayes
 */
public class StoredClassCatalog implements ClassCatalog {

    private static final byte REC_LAST_CLASS_ID = (byte) 0;
    private static final byte REC_CLASS_FORMAT = (byte) 1;
    private static final byte REC_CLASS_INFO = (byte) 2;

    private static final byte[] LAST_CLASS_ID_KEY = {REC_LAST_CLASS_ID};

    /*
    Record types ([key] [data]):

    [0] [next class ID]
    [1 / class ID] [ObjectStreamClass (class format)]
    [2 / class name] [ClassInfo (has 8 byte class ID)]
    */

    private DataEnvironment env;
    private DataDb db;
    private HashMap classMap;
    private HashMap formatMap;

    /**
     * Open a catalog database. To save resources, only a single catalog object
     * should be used for each unique catalog file.
     *
     * @param env is the environment in which to open the catalog.
     *
     * @param file is the name of the catalog file.
     *
     * @param database the database name to be used within the specified
     * store.  If null then the filename is the database name.
     *
     * @param openFlags Flags for calling Db.open such as Db.DB_CREATE and
     * Db.DB_AUTO_COMMIT.
     */
    public StoredClassCatalog(DbEnv env, String file,
			      String database, int openFlags)
        throws FileNotFoundException, DbException {

        this.env = DataEnvironment.getEnvironment(env);

        DbTxn txn = this.env.getTxn();
        if (txn != null)
            openFlags &= ~Db.DB_AUTO_COMMIT;

        // open the catalog database

        Db db = new Db(env, 0);
        db.open(txn, file, database, Db.DB_BTREE, openFlags, 0);
        this.db = new DataDb(db);

        // create the class format and class info maps; note that these are not
        // synchronized, and therefore the methods that use them are
        // synchronized

        classMap = new HashMap();
        formatMap = new HashMap();
    }

    // javadoc is inherited
    public synchronized void close()
        throws IOException {

        try {
            if (db != null) {
                db.close();
                db = null;
            }
        } catch (DbException e) {
            throw new IOExceptionWrapper(e);
        }
        db = null;
        formatMap = null;
        classMap = null;
    }

    // javadoc is inherited
    public synchronized byte[] getClassID(String className)
        throws IOException, ClassNotFoundException {

        try {
            ClassInfo classInfo = getClassInfo(className);
            return classInfo.getClassID();
        } catch (DbException e) {
            throw new IOExceptionWrapper(e);
        }
    }

    // javadoc is inherited
    public synchronized ObjectStreamClass getClassFormat(String className)
        throws IOException, ClassNotFoundException {

        try {
            ClassInfo classInfo = getClassInfo(className);
            return classInfo.getClassFormat();
        } catch (DbException e) {
            throw new IOExceptionWrapper(e);
        }
    }

    // javadoc is inherited
    public ObjectStreamClass getClassFormat(byte[] classID)
        throws IOException, ClassNotFoundException {

        try {
            return getClassFormat(classID, newDbt());
        } catch (DbException e) {
            throw new IOExceptionWrapper(e);
        }
    }

    /**
     * Internal function for getting the class format.  Allows passing the Dbt
     * object for the data, so the bytes of the class format can be examined
     * afterwards.
     */
    private synchronized ObjectStreamClass getClassFormat(byte[] classID,
                                                          DataThang data)
        throws DbException, ClassNotFoundException, IOException {

        // first check the map and, if found, add class info to the map

        BigInteger classIDObj = new BigInteger(classID);
        ObjectStreamClass classFormat =
            (ObjectStreamClass) formatMap.get(classIDObj);
        if (classFormat == null) {

            // make the class format key

            byte[] keyBytes = new byte[classID.length + 1];
            keyBytes[0] = REC_CLASS_FORMAT;
            System.arraycopy(classID, 0, keyBytes, 1, classID.length);
            DataThang key = newDbt();
            key.setBytes(keyBytes);

            // read the class format

            int err = db.get(key, data, 0);
            if (err != 0) {
                throw new ClassNotFoundException("Catalog class ID not found");
            }
            ObjectInputStream ois =
                new ObjectInputStream(data.getByteStream());
            classFormat = (ObjectStreamClass) ois.readObject();

            // update the class format map

            formatMap.put(classIDObj, classFormat);
        }
        return classFormat;
    }

    /**
     * Get the ClassInfo for a given class name, adding it and its
     * ObjectStreamClass to the database if they are not already present, and
     * caching both of them using the class info and class format maps.  When a
     * class is first loaded from the database, the stored ObjectStreamClass is
     * compared to the current ObjectStreamClass loaded by the Java class
     * loader; if they are different, a new class ID is assigned for the
     * current format.
     */
    private ClassInfo getClassInfo(String className)
        throws IOException, ClassNotFoundException, DbException, DbException {

        // first check for a cached copy of the class info, which if
        // present always contains the class format object

        ClassInfo classInfo = (ClassInfo) classMap.get(className);
        if (classInfo != null) {
            return classInfo;
        } else {
            // get currently loaded class format

            Class cls = Class.forName(className);
            ObjectStreamClass classFormat = ObjectStreamClass.lookup(cls);

            // make class info key

            char[] nameChars = className.toCharArray();
            byte[] keyBytes = new byte[1 + UtfOps.getByteLength(nameChars)];
            keyBytes[0] = REC_CLASS_INFO;
            UtfOps.charsToBytes(nameChars, 0, keyBytes, 1, nameChars.length);
            DataThang key = newDbt();
            key.setBytes(keyBytes);

            // read class info

            DataThang data = newDbt();
            int err = db.get(key, data, 0);
            if (err != 0) {
                // not found in the database; write class info and class format

                classInfo = putClassInfo(new ClassInfo(), className, key,
                                         classFormat);
            } else {
                // read class info to get the class format key, then read class
                // format

                classInfo = new ClassInfo(data);
                DataThang formatData = newDbt();
                ObjectStreamClass storedClassFormat =
                    getClassFormat(classInfo.getClassID(), formatData);

                // compare the stored class format to the current class format,
                // and if they are different then generate a new class ID

                if (!areClassFormatsEqual(storedClassFormat,
                                          formatData.getBytes(),
                                          classFormat)) {
                    classInfo = putClassInfo(classInfo, className, key,
                                             classFormat);
                }

                // update the class info map

                classInfo.setClassFormat(classFormat);
                classMap.put(className, classInfo);
            }
        }
        return classInfo;
    }

    /**
     * Assign a new class ID (increment the current ID record), write the
     * ObjectStreamClass record for this new ID, and update the ClassInfo
     * record with the new ID also.  The ClassInfo passed as an argument is the
     * one to be updated; however, a different ClassInfo may be returned if
     * another process happens to update the catalog database before we do
     * (this is a rare concurrency issue).
     */
    private ClassInfo putClassInfo(ClassInfo classInfo, String className,
                                   DataThang classKey,
                                   ObjectStreamClass classFormat)
        throws DbException, ClassNotFoundException {

        // an intent-to-write cursor is needed for CDB

        Dbc cursor = db.openCursor(true);
        try {
            // get and lock the record containing the last assigned ID

            DataThang key = newDbt();
            key.setBytes(LAST_CLASS_ID_KEY);
            DataThang data = newDbt();
            int putFlag = Db.DB_CURRENT;
            int err = cursor.get(key, data,
                                 Db.DB_SET | env.getWriteLockFlag());
            if (err != 0) {
                // if this is a new database, set the initial ID record
                data.setBytes(new byte[1]); // zero ID
                putFlag = Db.DB_KEYLAST;
            }
            byte[] idBytes = data.getBytes();

            // check one last time to see if another thread
            // wrote the information before this thread

            Object anotherClassInfo = classMap.get(className);
            if (anotherClassInfo != null)
                return (ClassInfo) anotherClassInfo;

            // increment the ID by one and write the updated record

            idBytes = incrementID(idBytes);
            data.setBytes(idBytes);
            cursor.put(key, data, putFlag);

            // write the new class format record whose key is the ID just
            // assigned

            byte[] keyBytes = new byte[1 + idBytes.length];
            keyBytes[0] = REC_CLASS_FORMAT;
            System.arraycopy(idBytes, 0, keyBytes, 1, idBytes.length);
            key.setBytes(keyBytes);

            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            ObjectOutputStream oos;
            try {
                oos = new ObjectOutputStream(baos);
                oos.writeObject(classFormat);
            }
            catch (IOException e) {}
            data.setBytes(baos.toByteArray());

            db.put(key, data, 0);

            // write the new class info record, using the key passed in; this
            // is done last so that a reader who gets the class info record
            // first will always find the corresponding class format record

            classInfo.setClassID(idBytes);
            classInfo.toDbt(data);

            db.put(classKey, data, 0);

            // update the maps before closing the cursor, so that the cursor
            // lock prevents other writers from duplicating this entry

            classInfo.setClassFormat(classFormat);
            classMap.put(className, classInfo);
            formatMap.put(new BigInteger(idBytes), classFormat);
            return classInfo;
        } finally {
            db.closeCursor(cursor);
        }
    }

    private static byte[] incrementID(byte[] key) {

        BigInteger id = new BigInteger(key);
        id = id.add(BigInteger.valueOf(1));
        return id.toByteArray();
    }

    /**
     * Holds the class format key for a class, maintains a reference to the
     * ObjectStreamClass.  Other fields can be added when we need to store more
     * information per class.
     */
    private static class ClassInfo implements Serializable {

        private byte[] classID;
        private transient ObjectStreamClass classFormat;

        ClassInfo() {
        }

        ClassInfo(DataThang dbt) {

            byte[] data = dbt.getDataBytes();
            int len = data[0];
            classID = new byte[len];
            System.arraycopy(data, 1, classID, 0, len);
        }

        void toDbt(DataThang dbt) {

            byte[] data = new byte[1 + classID.length];
            data[0] = (byte) classID.length;
            System.arraycopy(classID, 0, data, 1, classID.length);
            dbt.setData(data, 0, data.length);
        }

        void setClassID(byte[] classID) {

            this.classID = classID;
        }

        byte[] getClassID() {

            return classID;
        }

        ObjectStreamClass getClassFormat() {

            return classFormat;
        }

        void setClassFormat(ObjectStreamClass classFormat) {

            this.classFormat = classFormat;
        }
    }

    /**
     * Return whether two class formats are equal.  This determines whether a
     * new class format is needed for an object being serialized.  Formats must
     * be identical in all respects, or a new format is needed.
     */
    private static boolean areClassFormatsEqual(ObjectStreamClass format1,
                                                byte[] format1Bytes,
                                                ObjectStreamClass format2) {
        try {
            if (format1Bytes == null) { // using cached format1 object
                format1Bytes = getObjectBytes(format1);
            }
            byte[] format2Bytes = getObjectBytes(format2);
            return java.util.Arrays.equals(format2Bytes, format1Bytes);
        } catch (IOException e) { return false; }
    }

    private static byte[] getObjectBytes(Object o)
        throws IOException {

        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        ObjectOutputStream oos = new ObjectOutputStream(baos);
        oos.writeObject(o);
        return baos.toByteArray();
    }

    private DataThang newDbt() {

        return new DataThang();
    }
}

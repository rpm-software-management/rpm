TESTCLASSES=\
    ./src/com/sleepycat/bdb/bind/serial/test/MarshalledObject.java\
    ./src/com/sleepycat/bdb/bind/serial/test/NullClassCatalog.java\
    ./src/com/sleepycat/bdb/bind/serial/test/SerialBindingTest.java\
    ./src/com/sleepycat/bdb/bind/serial/test/Suite.java\
    ./src/com/sleepycat/bdb/bind/serial/test/TestClassCatalog.java\
    ./src/com/sleepycat/bdb/bind/test/BindingSpeedTest.java\
    ./src/com/sleepycat/bdb/bind/test/Suite.java\
    ./src/com/sleepycat/bdb/bind/tuple/test/MarshalledObject.java\
    ./src/com/sleepycat/bdb/bind/tuple/test/Suite.java\
    ./src/com/sleepycat/bdb/bind/tuple/test/TupleBindingTest.java\
    ./src/com/sleepycat/bdb/bind/tuple/test/TupleFormatTest.java\
    ./src/com/sleepycat/bdb/bind/tuple/test/TupleOrderingTest.java\
    ./src/com/sleepycat/bdb/serial/test/StoredClassCatalogTest.java\
    ./src/com/sleepycat/bdb/serial/test/Suite.java\
    ./src/com/sleepycat/bdb/serial/test/TestSerial.java\
    ./src/com/sleepycat/bdb/serial/test/TupleSerialDbFactoryTest.java\
    ./src/com/sleepycat/bdb/test/CollectionTest.java\
    ./src/com/sleepycat/bdb/test/DbTestUtil.java\
    ./src/com/sleepycat/bdb/test/ForeignKeyTest.java\
    ./src/com/sleepycat/bdb/test/JoinTest.java\
    ./src/com/sleepycat/bdb/test/KeyRangeTest.java\
    ./src/com/sleepycat/bdb/test/NullTransactionRunner.java\
    ./src/com/sleepycat/bdb/test/Suite.java\
    ./src/com/sleepycat/bdb/test/TestDataBinding.java\
    ./src/com/sleepycat/bdb/test/TestEntity.java\
    ./src/com/sleepycat/bdb/test/TestEntityBinding.java\
    ./src/com/sleepycat/bdb/test/TestEnv.java\
    ./src/com/sleepycat/bdb/test/TestKeyAssigner.java\
    ./src/com/sleepycat/bdb/test/TestKeyExtractor.java\
    ./src/com/sleepycat/bdb/test/TestStore.java\
    ./src/com/sleepycat/bdb/test/TransactionTest.java\
    ./src/com/sleepycat/bdb/util/test/ExceptionWrapperTest.java\
    ./src/com/sleepycat/bdb/util/test/Suite.java\
    ./src/com/sleepycat/bdb/util/test/UtfTest.java

all: dbtest.jar

dbtest.jar: classesdir
	javac -classpath ${DB_JAR}:${REQUIRED_JARS} \
		 -d ./classes ${TESTCLASSES}
	jar cf ./dbtest.jar -C ./classes ./com/sleepycat
	jar uf ./dbtest.jar -C ./src ./com/sleepycat/bdb/serial/test/testdata

classesdir:
	[ -d ./classes ] || (mkdir ./classes)

clean:
	[ -d ./classes ] && rm -rf ./classes
	[ -f ./dbtest.jar ] && rm ./dbtest.jar


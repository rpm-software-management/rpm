export BINDIR=/bin
export MANDIR=/man
export INSTALL_ROOT=/usr/local

all:
	cd src/tests/tetj && make 
	cd src/tests/rpmchk && make 
	cd src/tests/pkgchk && make 

install:
	cd src/tests/pkgchk && make install

clean:
	cd src/tests/pkgchk && make clean
	cd src/tests/rpmchk && make clean
	cd src/tests/tetj && make clean


                   Using a Borland compiler product

The files in this directory support using both the free Borland
command-line compiler tools and the Borland C++ Builder IDE.  The
project files have been tested with both versions 5 and 6 of the C++
Builder product.

             Using the free BCC32 command line compiler

After downloading and installing the free C++ Builder commandline
version, perform the following steps (assuming it was installed under
C:\Borland\BCC55):

1) Add "C:\Borland\BCC55\BIN" to your path
2) Set the environment variable BCB to "C:\Borland\BCC55".
3) edit makefile.mak: enable or comment out the appropriate commands
   under clean & distclean, depending on whether your OS can use
   deltree /y or del /s/f/q.

After that, you should simply cd to the bcb5 directory in your Expat
directory tree (same structure as CVS) and run "make all".

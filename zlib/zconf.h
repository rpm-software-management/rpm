/*@-constuse -typeuse@*/
/* zconf.h -- configuration of the zlib compression library
 * Copyright (C) 1995-2003 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h 
 */

/* @(#) $Id: zconf.h,v 1.10 2003/05/18 18:33:40 jbj Exp $ */

#ifndef _ZCONF_H
#define _ZCONF_H

/*
 * If you *really* need a unique prefix for all types and library functions,
 * compile with -DZ_PREFIX. The "standard" zlib should be compiled without it.
 */
#define	Z_PREFIX
#ifdef Z_PREFIX
#  define deflateInit_	rpmz_deflateInit_
#  define deflate	rpmz_deflate
#  define deflateEnd	rpmz_deflateEnd
#  define inflateInit_ 	rpmz_inflateInit_
#  define inflate	rpmz_inflate
#  define inflateEnd	rpmz_inflateEnd
#  define deflateInit2_	rpmz_deflateInit2_
#  define deflateSetDictionary rpmz_deflateSetDictionary
#  define deflateCopy	rpmz_deflateCopy
#  define deflateReset	rpmz_deflateReset
#  define deflateParams	rpmz_deflateParams
#  define deflateBound	rpmz_deflateBound
#  define inflateInit2_	rpmz_inflateInit2_
#  define inflateSetDictionary rpmz_inflateSetDictionary
#  define inflateSync	rpmz_inflateSync
#  define inflateSyncPoint rpmz_inflateSyncPoint
#  define inflateCopy	rpmz_inflateCopy
#  define inflateReset	rpmz_inflateReset
#  define compress	rpmz_compress
#  define compress2	rpmz_compress2
#  define compressBound	rpmz_compressBound
#  define uncompress	rpmz_uncompress
#  define adler32	rpmz_adler32
#  define crc32		rpmz_crc32
#  define get_crc_table rpmz_get_crc_table

#  define Byte		rpmz_Byte
#  define uInt		rpmz_uInt
#  define uLong		rpmz_uLong
#  define Bytef	        rpmz_Bytef
#  define charf		rpmz_charf
#  define intf		rpmz_intf
#  define uIntf		rpmz_uIntf
#  define uLongf	rpmz_uLongf
#  define voidpf	rpmz_voidpf
#  define voidp		rpmz_voidp

#  define gzclose	rpmz_gzclose
#  define gzdopen	rpmz_gzdopen
#  define gzeof		rpmz_gzeof
#  define gzerror	rpmz_gzerror
#  define gzflush	rpmz_gzflush
#  define gzgetc	rpmz_gzgetc
#  define gzgets	rpmz_gzgets
#  define gz_magic	rpmz_gz_magic
#  define gzopen	rpmz_gzopen
#  define gz_open	rpmz_gz_open
#  define gzprintf	rpmz_gzprintf
#  define gzputc	rpmz_gzputc
#  define gzputs	rpmz_gzputs
#  define gzread	rpmz_gzread
#  define gzrewind	rpmz_gzrewind
#  define gzseek	rpmz_gzseek
#  define gzsetparams	rpmz_gzsetparams
#  define gztell	rpmz_gztell
#  define gzwrite	rpmz_gzwrite

#  define inflateBack		rpmz_inflateBack
#  define inflateBackEnd	rpmz_inflateBackEnd
#  define inflateBackInit_	rpmz_inflateBackInit_
#  define inflate_fast		rpmz_inflate_fast
#  define inflate_table		rpmz_inflate_table
#  define _tr_align		rpmz__tr_align
#  define _tr_flush_block	rpmz__tr_flush_block
#  define _tr_init		rpmz__tr_init
#  define _tr_stored_block	rpmz__tr_stored_block
#  define _tr_tally		rpmz__tr_tally
#  define zcalloc		rpmz_zcalloc
#  define zcfree		rpmz_zcfree
#  define z_errmsg		rpmz_z_errmsg
#  define zError		rpmz_zError
#  define zlibVersion		rpmz_zlibVersion

#  define deflate_copyright	rpmz_deflate_copyright
#  define inflate_copyright	rpmz_inflate_copyright
#  define _dist_code		rpmz__dist_code
#  define _length_code		rpmz__length_code

#endif

#if (defined(_WIN32) || defined(__WIN32__)) && !defined(WIN32)
#  define WIN32
#endif
#if defined(__GNUC__) || defined(WIN32) || defined(__386__) || defined(i386)
#  ifndef __32BIT__
#    define __32BIT__
#  endif
#endif
#if defined(__MSDOS__) && !defined(MSDOS)
#  define MSDOS
#endif

/*
 * Compile with -DMAXSEG_64K if the alloc function cannot allocate more
 * than 64k bytes at a time (needed on systems with 16-bit int).
 */
#if defined(MSDOS) && !defined(__32BIT__)
#  define MAXSEG_64K
#endif
#ifdef MSDOS
#  define UNALIGNED_OK
#endif

#if (defined(MSDOS) || defined(_WINDOWS) || defined(WIN32))  && !defined(STDC)
#  define STDC
#endif
#if defined(__STDC__) || defined(__cplusplus) || defined(__OS2__)
#  ifndef STDC
#    define STDC
#  endif
#endif

#if defined __HOS_AIX__
#  ifndef STDC
#    define STDC
#  endif
#endif

#ifndef STDC
#  ifndef const /* cannot use !defined(STDC) && !defined(const) on Mac */
#    define const	/* note: need a more gentle solution here */
#  endif
#endif

/* Some Mac compilers merge all .h files incorrectly: */
#if defined(__MWERKS__) || defined(applec) ||defined(THINK_C) ||defined(__SC__)
#  define NO_DUMMY_DECL
#endif

/* Old Borland C incorrectly complains about missing returns: */
#if defined(__BORLANDC__) && (__BORLANDC__ < 0x460)
#  define NEED_DUMMY_RETURN
#endif
#if defined(__TURBOC__) && !defined(__BORLANDC__)
#  define NEED_DUMMY_RETURN
#endif


/* Maximum value for memLevel in deflateInit2 */
#ifndef MAX_MEM_LEVEL
#  ifdef MAXSEG_64K
#    define MAX_MEM_LEVEL 8
#  else
#    define MAX_MEM_LEVEL 9
#  endif
#endif

/* Maximum value for windowBits in deflateInit2 and inflateInit2.
 * WARNING: reducing MAX_WBITS makes minigzip unable to extract .gz files
 * created by gzip. (Files created by minigzip can still be extracted by
 * gzip.)
 */
#ifndef MAX_WBITS
#  define MAX_WBITS   15 /* 32K LZ77 window */
#endif

/* The memory requirements for deflate are (in bytes):
            (1 << (windowBits+2)) +  (1 << (memLevel+9))
 that is: 128K for windowBits=15  +  128K for memLevel = 8  (default values)
 plus a few kilobytes for small objects. For example, if you want to reduce
 the default memory requirements from 256K to 128K, compile with
     make CFLAGS="-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7"
 Of course this will generally degrade compression (there's no free lunch).

   The memory requirements for inflate are (in bytes) 1 << windowBits
 that is, 32K for windowBits=15 (default value) plus a few kilobytes
 for small objects.
*/

                        /* Type declarations */

#ifndef OF /* function prototypes */
#  if defined(STDC) || defined(__LCLINT__)
#    define OF(args)  args
#  else
#    define OF(args)  ()
#  endif
#endif

/* The following definitions for FAR are needed only for MSDOS mixed
 * model programming (small or medium model with some far allocations).
 * This was tested only with MSC; for other MSDOS compilers you may have
 * to define NO_MEMCPY in zutil.h.  If you don't need the mixed model,
 * just define FAR to be empty.
 */
#if (defined(M_I86SM) || defined(M_I86MM)) && !defined(__32BIT__)
   /* MSC small or medium model */
#  define SMALL_MEDIUM
#  ifdef _MSC_VER
#    define FAR _far
#  else
#    define FAR far
#  endif
#endif
#if defined(__BORLANDC__) && (defined(__SMALL__) || defined(__MEDIUM__))
#  ifndef __32BIT__
#    define SMALL_MEDIUM
#    define FAR _far
#  endif
#endif

#if defined(WIN32) && (!defined(ZLIB_WIN32_NODLL)) && (!defined(ZLIB_DLL))
#  define ZLIB_DLL
#endif

/* Compile with -DZLIB_DLL for Windows DLL support */
#if defined(ZLIB_DLL)
#  if defined(_WINDOWS) || defined(WINDOWS) || defined(WIN32)
#    ifndef WINAPIV
#    ifdef FAR
#      undef FAR
#    endif
#    include <windows.h>
#    endif
#    ifdef WIN32
#      define ZEXPORT  WINAPI
#      define ZEXPORTVA  WINAPIV
#    else
#      define ZEXPORT  WINAPI _export
#      define ZEXPORTVA  FAR _cdecl _export
#    endif
#  endif
#  if defined (__BORLANDC__)
#    if (__BORLANDC__ >= 0x0500) && defined (WIN32)
#      include <windows.h>
#      define ZEXPORT __declspec(dllexport) WINAPI
#      define ZEXPORTVA __declspec(dllexport) WINAPIV
#    else
#      if defined (_Windows) && defined (__DLL__)
#        define ZEXPORT _export
#        define ZEXPORTVA _export
#      endif
#    endif
#  endif
#endif

#if defined (__BEOS__)
#  if defined (ZLIB_DLL)
#    define ZEXTERN extern __declspec(dllexport)
#  else
#    define ZEXTERN extern __declspec(dllimport)
#  endif
#endif

#ifndef ZEXPORT
#  define ZEXPORT
#endif
#ifndef ZEXPORTVA
#  define ZEXPORTVA
#endif
#ifndef ZEXTERN
#  define ZEXTERN extern
#endif

#ifndef FAR
#   define FAR
#endif

#if !defined(__MACTYPES__)
typedef unsigned char  Byte;  /* 8 bits */
#endif
typedef unsigned int   uInt;  /* 16 bits or more */
typedef unsigned long  uLong; /* 32 bits or more */

#ifdef SMALL_MEDIUM
   /* Borland C/C++ and some old MSC versions ignore FAR inside typedef */
#  define Bytef Byte FAR
#else
   typedef Byte  FAR Bytef;
#endif
typedef char  FAR charf;
typedef int   FAR intf;
typedef uInt  FAR uIntf;
typedef uLong FAR uLongf;

#ifdef STDC
   typedef void const *voidpc;
   typedef void FAR *voidpf;
   typedef void     *voidp;
#else
   typedef Byte const *voidpc;
   typedef Byte FAR *voidpf;
   typedef Byte     *voidp;
#endif

#ifdef HAVE_UNISTD_H
#  include <sys/types.h> /* for off_t */
#  include <unistd.h>    /* for SEEK_* and off_t */
#  ifdef VMS
#    include <unixio.h>   /* for off_t */
#  endif
#  define z_off_t  off_t
#endif
#ifndef SEEK_SET
#  define SEEK_SET        0       /* Seek from beginning of file.  */
#  define SEEK_CUR        1       /* Seek from current position.  */
#  define SEEK_END        2       /* Set file pointer to EOF plus "offset" */
#endif
#ifndef z_off_t
#  define  z_off_t long
#endif

/* MVS linker does not support external names larger than 8 bytes */
#if defined(__MVS__)
#   pragma map(deflateInit_,"DEIN")
#   pragma map(deflateInit2_,"DEIN2")
#   pragma map(deflateEnd,"DEEND")
#   pragma map(deflateBound,"DEBND")
#   pragma map(inflateInit_,"ININ")
#   pragma map(inflateInit2_,"ININ2")
#   pragma map(inflateEnd,"INEND")
#   pragma map(inflateSync,"INSY")
#   pragma map(inflateSetDictionary,"INSEDI")
#   pragma map(compressBound,"CMBND")
#   pragma map(inflate_table,"INTABL")
#   pragma map(inflate_fast,"INFA")
#   pragma map(inflate_copyright,"INCOPY")
#endif

#endif /* _ZCONF_H */
/*@=constuse =typeuse@*/

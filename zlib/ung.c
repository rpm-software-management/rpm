/* ung.c -- simple gunzip to give an example of the use if inflateBack()
 * Copyright (C) 2003 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
   ung [ -t ] [ name ... ]

   decompresses the data in the named gzip files.  If no arguments are given,
   ung will decompress from stdin to stdout.  The names must end in .gz, -gz,
   .z, -z, _z, or .Z.  The uncompressed data will be written to a file name
   with the suffix stripped.  On success, the original file is deleted.  On
   failure, the output file is deleted.  For most failures, the command will
   continue to process the remaining names on the command line.  A memory
   allocation failure will abort the command.  It -t is specified, then the
   listed files or stdin will be tested as gzip files for integrity (without
   checking for a proper suffix), no output will be written, and no files
   will be deleted.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "zlib.h"

#define local static

#define INSIZE 32768U
#define PIECE 16384	/* limits i/o chunks for 16-bit int case */

struct io {
    int infile;
    unsigned char *inbuf;
    int outfile;
    unsigned long crc;
    unsigned long total;
};

local unsigned char *in(void *opaque, unsigned *len)
{
    int ret;
    unsigned char *next;
    struct io *this = opaque;

    next = this->inbuf;
    *len = 0;
    do {
        ret = PIECE;
        if ((unsigned)ret > INSIZE - *len) ret = (int)(INSIZE - *len);
        ret = (int)read(this->infile, next, ret);
        if (ret == -1) {
            *len = 0;
            return Z_NULL;
        }
        next += ret;
        *len += ret;
    } while (ret != 0 && *len < INSIZE);
    return this->inbuf;
}

local int out(void *opaque, unsigned char *buf, unsigned len)
{
    int ret;
    struct io *this = opaque;

    this->crc = crc32(this->crc, buf, len);
    this->total += len;
    if (this->outfile != -1)
        do {
            ret = PIECE;
            if ((unsigned)ret > len) ret = (int)len;
            ret = (int)write(this->outfile, buf, ret);
            if (ret == -1) return 1;
            buf += ret;
            len -= ret;
        } while (len != 0);
    return 0;
}

local int gunpipe(z_stream *strm, int infile, int outfile)
{
    int ret;
    unsigned have, flags, len;
    unsigned char *next, *window;
    void *opaque;
    struct io io;

    /* clear allocations */
    io.inbuf = NULL;
    window = NULL;
    do {

        /* setup input */
        io.infile = infile;
        io.inbuf = malloc(INSIZE);
        if (io.inbuf == NULL) {
            ret = Z_MEM_ERROR;
            break;
        }
        opaque = (void *)(&io);
        next = in(opaque, &have);

        /* check and skip gzip header */
        ret = Z_BUF_ERROR;
        if (have < 10) break;
        have -= 10;
        if (*next++ != 31 || *next++ != 139) {
            strm->msg = "incorrect header check";
            ret = Z_DATA_ERROR;
            break;
        }
        if (*next++ != 8) {
            strm->msg = "unknown compression method";
            ret = Z_DATA_ERROR;
            break;
        }
        flags = *next++;
        next += 6;
        if (flags & 0xe0) {
            strm->msg = "unknown header flags set";
            ret = Z_DATA_ERROR;
            break;
        }
        if (flags & 4) {
            if (have < 2) break;
            have -= 2;
            len = *next++;
            len += *next++ << 8;
            while (have < len) {
                len -= have;
                next = in(opaque, &have);
                if (have == 0) break;
            }
            have -= len;
            next += len;
        }
        if (flags & 8)
            do {
                if (have == 0) {
                    next = in(opaque, &have);
                    if (have == 0) break;
                }
                have--;
            } while (*next++ != 0);
        if (flags & 16)
            do {
                if (have == 0) {
                    next = in(opaque, &have);
                    if (have == 0) break;
                }
                have--;
            } while (*next++ != 0);
        if (flags & 2) {
            len = 2;
            if (have < len) {
                len -= have;
                next = in(opaque, &have);
                if (have == 0) break;
            }
            have -= len;
            next += len;
        }

        /* set up output */
        io.outfile = outfile;
        io.crc = crc32(0L, Z_NULL, 0);
        io.total = 0;

        /* do inflate */
        strm->next_in = next;
        strm->avail_in = have;
        ret = inflateBack(strm, in, out, opaque);
        if (ret != Z_STREAM_END) break;
        next = strm->next_in;
        have = strm->avail_in;

        /* check trailer */
        ret = Z_BUF_ERROR;
        for (len = 0; len < 32; len += 8) {
            if (have == 0) {
                next = in(opaque, &have);
                if (have == 0) break;
            }
            have--;
            if (*next++ != (unsigned char)((io.crc >> len) & 0xff)) {
                strm->msg = "incorrect data check";
                ret = Z_DATA_ERROR;
                break;
            }
            ret = Z_OK;
        }
        if (ret != Z_OK) break;
        ret = Z_BUF_ERROR;
        for (len = 0; len < 32; len += 8) {
            if (have == 0) {
                next = in(opaque, &have);
                if (have == 0) break;
            }
            have--;
            if (*next++ != (unsigned char)((io.total >> len) & 0xff)) {
                strm->msg = "incorrect length check";
                ret = Z_DATA_ERROR;
                break;
            }
            ret = Z_OK;
        }
        if (ret != Z_OK) break;
        if (have == 0) next = in(opaque, &have);
        if (have != 0) ret = Z_ERRNO;

    /* done */
    } while (0);
    if (window != NULL) free(window);
    if (io.inbuf != NULL) free(io.inbuf);
    return ret;
}

local int gunzip(z_stream *strm, char *inname, char *outname, int test)
{
    int ret;
    int infile, outfile;

    /* open files */
    if (inname == NULL || *inname == 0) {
        inname = "-";
        infile = 0;	/* stdin */
    }
    else {
        infile = open(inname, O_RDONLY, 0);
        if (infile == -1) {
            fprintf(stderr, "ung cannot open %s\n", inname);
            return 0;
        }
    }
    if (test)
        outfile = -1;
    else if (outname == NULL || *outname == 0) {
        outname = "-";
        outfile = 1;	/* stdout */
    }
    else {
        outfile = open(outname, O_CREAT | O_TRUNC | O_WRONLY, 0666);
        if (outfile == -1) {
            close(infile);
            fprintf(stderr, "ung cannot create %s\n", outname);
            return 0;
        }
    }
    errno = 0;

    /* decompress */
    ret = gunpipe(strm, infile, outfile);
    if (outfile > 2) close(outfile);
    if (infile > 2) close(infile);

    /* interpret result */
    switch (ret) {
    case Z_OK:
        if (infile > 2 && outfile > 2) unlink(inname);
        return 0;
    case Z_ERRNO:
        if (infile > 2 && outfile > 2) unlink(inname);
        fprintf(stderr, "ung warning: trailing garbage ignored\n");
        break;
    case Z_DATA_ERROR:
        if (outfile > 2) unlink(outname);
        fprintf(stderr, "ung data error on %s: %s\n", inname, strm->msg);
        break;
    case Z_MEM_ERROR:
        if (outfile > 2) unlink(outname);
        fprintf(stderr, "ung out of memory error--aborting\n");
        return 1;
    case Z_BUF_ERROR:
        if (outfile > 2) unlink(outname);
        if (strm->next_in != Z_NULL) {
            fprintf(stderr, "ung write error on %s: %s\n",
                    outname, strerror(errno));
        }
        else if (errno) {
            fprintf(stderr, "ung read error on %s: %s\n",
                    inname, strerror(errno));
        }
        else {
            fprintf(stderr, "ung unexpected end of file on %s\n",
                    inname);
        }
        break;
    default:
        if (outfile > 2) unlink(outname);
        fprintf(stderr, "ung internal error--aborting\n");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int ret, len, test;
    char *outname;
    unsigned char *window;
    z_stream strm;

    /* initialize inflateBack state for repeated use */
    window = malloc(32768U);	/* alloc failure caught by init */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = inflateBackInit(&strm, 15, window);
    if (ret != Z_OK) {
        if (window != NULL) free(window);
        fprintf(stderr, "ung out of memory error--aborting\n");
        return 1;
    }

    /* decompress each file to the same name with the suffix removed */
    argc--;
    argv++;
    test = 0;
    if (argc && strcmp(*argv, "-t") == 0) {
        test = 1;
        argc--;
        argv++;
    }
    if (argc)
        do {
            if (test)
                outname = NULL;
            else {
                len = (int)strlen(*argv);
                if (strcmp(*argv + len - 3, ".gz") == 0 ||
                    strcmp(*argv + len - 3, "-gz") == 0)
                    len -= 3;
                else if (strcmp(*argv + len - 2, ".z") == 0 ||
                    strcmp(*argv + len - 2, "-z") == 0 ||
                    strcmp(*argv + len - 2, "_z") == 0 ||
                    strcmp(*argv + len - 2, ".Z") == 0)
                    len -= 2;
                else {
                    fprintf(stderr, "ung error: no gzip suffix on %s--skipping\n",
                            *argv);
                    continue;
                }
                outname = malloc(len + 1);
                if (outname == NULL) {
                    fprintf(stderr, "ung out of memory error--aborting\n");
                    ret = 1;
                    break;
                }
                memcpy(outname, *argv, len);
                outname[len] = 0;
            }
            ret = gunzip(&strm, *argv, outname, test);
            if (outname != NULL) free(outname);
            if (ret) break;
        } while (argv++, --argc);
    else
        ret = gunzip(&strm, NULL, NULL, test);

    /* clean up */
    inflateBackEnd(&strm);
    free(window);
    return ret;
}

/*
 * Copyright (c) Christos Zoulas 2003.
 * All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _MAGIC_H
#define _MAGIC_H

#include <sys/types.h>

#define	MAGIC_NONE		0x000	/* No flags */
#define	MAGIC_DEBUG		0x001	/* Turn on debugging */
#define	MAGIC_SYMLINK		0x002	/* Follow symlinks */
#define	MAGIC_COMPRESS		0x004	/* Check inside compressed files */
#define	MAGIC_DEVICES		0x008	/* Look at the contents of devices */
#define	MAGIC_MIME		0x010	/* Return a mime string */
#define	MAGIC_CONTINUE		0x020	/* Return all matches */
#define	MAGIC_CHECK		0x040	/* Print warnings to stderr */
#define	MAGIC_PRESERVE_ATIME	0x080	/* Restore access time on exit */
#define	MAGIC_RAW		0x100	/* Don't translate unprintable chars */
#define	MAGIC_ERROR		0x200	/* Handle ENOENT etc as real errors */

#ifdef __cplusplus
extern "C" {
#endif

typedef /*@abstract@*/ struct magic_set *magic_t;

/*@null@*/
magic_t magic_open(int flags)
	/*@globals errno @*/
	/*@modifies errno @*/;
void magic_close(/*@only@*/ magic_t ms)
	/*@globals fileSystem @*/
	/*@modifies ms, fileSystem @*/;

/*@null@*/ /*@observer@*/
const char *magic_file(magic_t ms, /*@null@*/ const char * inname)
	/*@globals fileSystem, internalState @*/
	/*@modifies ms, fileSystem, internalState @*/;
/*@null@*/ /*@observer@*/
const char *magic_buffer(magic_t ms, const void * buf, size_t nb)
	/*@globals fileSystem, internalState @*/
	/*@modifies ms, fileSystem, internalState @*/;

/*@observer@*/
const char *magic_error(magic_t ms)
	/*@modifies ms @*/;
int magic_setflags(magic_t ms, int)
	/*@modifies ms @*/;

int magic_load(magic_t ms, const char * magicfile)
	/*@globals fileSystem, internalState @*/
	/*@modifies ms, fileSystem, internalState @*/;
int magic_compile(magic_t ms, const char * magicfile)
	/*@globals fileSystem, internalState @*/
	/*@modifies ms, fileSystem, internalState @*/;
int magic_check(magic_t ms, const char * magicfile)
	/*@globals fileSystem, internalState @*/
	/*@modifies ms, fileSystem, internalState @*/;
int magic_errno(magic_t ms)
	/*@modifies ms @*/;

#ifdef __cplusplus
};
#endif

#endif /* _MAGIC_H */

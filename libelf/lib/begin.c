/*@+boundswrite@*/
/*
begin.c - implementation of the elf_begin(3) and elf_memory(3) functions.
Copyright (C) 1995 - 2001 Michael Riepe <michael@stud.uni-hannover.de>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <private.h>

#ifndef lint
static const char rcsid[] = "@(#) Id: begin.c,v 1.12 2001/10/15 21:38:29 michael Exp ";
#endif /* lint */

#if HAVE_AR_H
#include <ar.h>
#else /* HAVE_AR_H */

#define ARMAG	"!<arch>\n"
#define SARMAG	8

struct ar_hdr {
    char    ar_name[16];
    char    ar_date[12];
    char    ar_uid[6];
    char    ar_gid[6];
    char    ar_mode[8];
    char    ar_size[10];
    char    ar_fmag[2];
};

#define ARFMAG	"`\n"

#endif /* HAVE_AR_H */

/*@unchecked@*/
static const Elf _elf_init = INIT_ELF;
/*@unchecked@*/ /*@observer@*/
static const char fmag[] = ARFMAG;

static unsigned long
getnum(const char *str, size_t len, int base, int *err)
	/*@modifies *err @*/
	/*@requires maxSet(err) >= 0 @*/
{
    unsigned long result = 0;

/*@-boundsread@*/
    while (len && *str == ' ') {
	str++; len--;
    }
    while (len && *str >= '0' && (*str - '0') < base) {
	result = base * result + *str++ - '0'; len--;
    }
    while (len && *str == ' ') {
	str++; len--;
    }
/*@=boundsread@*/
    if (len) {
	*err = len;
    }
    return result;
}

static void
_elf_init_ar(Elf *elf)
	/*@modifies *elf @*/
{
    struct ar_hdr *hdr;
    size_t offset;
    size_t size;
    int err = 0;

    elf->e_kind = ELF_K_AR;
    elf->e_idlen = SARMAG;
    elf->e_off = SARMAG;

    /* process special members */
    offset = SARMAG;
    while (!elf->e_strtab && offset + sizeof(*hdr) <= elf->e_size) {
	hdr = (struct ar_hdr*)(elf->e_data + offset);
	if (memcmp(hdr->ar_fmag, fmag, sizeof(fmag) - 1)) {
	    break;
	}
/*@-boundsread@*/
	if (hdr->ar_name[0] != '/') {
	    break;
	}
/*@-boundswrite@*/
	size = getnum(hdr->ar_size, sizeof(hdr->ar_size), 10, &err);
/*@=boundswrite @*/
/*@=boundsread@*/
	if (err || !size) {
	    break;
	}
	offset += sizeof(*hdr);
	if (offset + size > elf->e_size) {
	    break;
	}
/*@-boundsread@*/
	if (hdr->ar_name[1] == '/' && hdr->ar_name[2] == ' ') {
	    elf->e_strtab = elf->e_data + offset;
	    elf->e_strlen = size;
	    break;
	}
	if (elf->e_symtab || hdr->ar_name[1] != ' ') {
	    break;
	}
/*@=boundsread@*/
	elf->e_symtab = elf->e_data + offset;
	elf->e_symlen = size;
	offset += size + (size & 1);
    }
}

/*@null@*/
static Elf_Arhdr*
_elf_arhdr(Elf *arf)
	/*@globals _elf_errno @*/
	/*@modifies _elf_errno @*/
{
    struct ar_hdr *hdr;
    Elf_Arhdr *arhdr;
    size_t namelen;
    size_t tmp;
    char *name;
    int err = 0;

    if (arf->e_off == arf->e_size) {
	/* no error! */
	return NULL;
    }
    if (arf->e_off < 0 || arf->e_off > arf->e_size) {
	seterr(ERROR_OUTSIDE);
	return NULL;
    }
    if (arf->e_off + sizeof(*hdr) > arf->e_size) {
	seterr(ERROR_TRUNC_ARHDR);
	return NULL;
    }
    elf_assert(arf->e_data != NULL);
    hdr = (struct ar_hdr*)(arf->e_data + arf->e_off);
    if (memcmp(hdr->ar_fmag, fmag, sizeof(fmag) - 1)) {
	seterr(ERROR_ARFMAG);
	return NULL;
    }

    name = hdr->ar_name;
/*@-boundsread@*/
    for (namelen = sizeof(hdr->ar_name); namelen > 0; namelen--) {
	if (name[namelen - 1] != ' ') {
	    break;
	}
    }
    if (name[0] == '/') {
	if (name[1] >= '0' && name[1] <= '9') {
	    if (!arf->e_strtab) {
		seterr(ERROR_ARSTRTAB);
		return NULL;
	    }
/*@-boundswrite@*/
	    tmp = getnum(&name[1], namelen - 1, 10, &err);
/*@=boundswrite@*/
	    if (err) {
		seterr(ERROR_ARSPECIAL);
		return NULL;
	    }
	    if (tmp < 0 || tmp >= arf->e_strlen) {
		seterr(ERROR_ARSTRTAB);
		return NULL;
	    }
	    for (namelen = tmp; namelen < arf->e_strlen; namelen++) {
		if (arf->e_strtab[namelen] == '/') {
		    break;
		}
	    }
	    if (namelen == arf->e_strlen) {
		seterr(ERROR_ARSTRTAB);
		return NULL;
	    }
	    name = arf->e_strtab + tmp;
	    namelen -= tmp;
	}
	else if (namelen != 1 && !(namelen == 2 && name[1] == '/')) {
	    seterr(ERROR_ARSPECIAL);
	    return NULL;
	}
    }
    else if (namelen > 0 && name[namelen - 1] == '/') {
	namelen--;
    }
/*@=boundsread@*/
    /* XXX some broken software omits the trailing slash */
#if 0
    else {
	namelen = 0;
    }
#endif

    if (!(arhdr = (Elf_Arhdr*)malloc(sizeof(*arhdr) +
		     sizeof(hdr->ar_name) + namelen + 2))) {
	seterr(ERROR_MEM_ARHDR);
	return NULL;
    }

    arhdr->ar_name = NULL;
    arhdr->ar_rawname = (char*)(arhdr + 1);
/*@-boundsread -boundswrite @*/
    arhdr->ar_date = getnum(hdr->ar_date, sizeof(hdr->ar_date), 10, &err);
    arhdr->ar_uid = getnum(hdr->ar_uid, sizeof(hdr->ar_uid), 10, &err);
    arhdr->ar_gid = getnum(hdr->ar_gid, sizeof(hdr->ar_gid), 10, &err);
    arhdr->ar_mode = getnum(hdr->ar_mode, sizeof(hdr->ar_mode), 8, &err);
    arhdr->ar_size = getnum(hdr->ar_size, sizeof(hdr->ar_size), 10, &err);
/*@=boundsread =boundswrite @*/
    if (err) {
	free(arhdr);
	seterr(ERROR_ARHDR);
	return NULL;
    }
    if (arf->e_off + sizeof(struct ar_hdr) + arhdr->ar_size > arf->e_size) {
	free(arhdr);
	seterr(ERROR_TRUNC_MEMBER);
	return NULL;
    }

/*@-boundswrite@*/
    memcpy(arhdr->ar_rawname, hdr->ar_name, sizeof(hdr->ar_name));
    arhdr->ar_rawname[sizeof(hdr->ar_name)] = '\0';

    if (namelen) {
	arhdr->ar_name = arhdr->ar_rawname + sizeof(hdr->ar_name) + 1;
	memcpy(arhdr->ar_name, name, namelen);
	arhdr->ar_name[namelen] = '\0';
    }
/*@=boundswrite@*/

    return arhdr;
}

static void
_elf_check_type(Elf *elf, size_t size)
	/*@modifies *elf @*/
{
    elf->e_idlen = size;
    if (size >= EI_NIDENT && !memcmp(elf->e_data, ELFMAG, SELFMAG)) {
	elf->e_kind = ELF_K_ELF;
	elf->e_idlen = EI_NIDENT;
/*@-boundsread@*/
	elf->e_class = elf->e_data[EI_CLASS];
	elf->e_encoding = elf->e_data[EI_DATA];
	elf->e_version = elf->e_data[EI_VERSION];
/*@=boundsread@*/
    }
    else if (size >= SARMAG && !memcmp(elf->e_data, ARMAG, SARMAG)) {
	_elf_init_ar(elf);
    }
}

Elf*
elf_begin(int fd, Elf_Cmd cmd, Elf *ref) {
    Elf_Arhdr *arhdr = NULL;
    size_t size = 0;
    Elf *elf;

    elf_assert(_elf_init.e_magic == ELF_MAGIC);
    if (_elf_version == EV_NONE) {
	seterr(ERROR_VERSION_UNSET);
	return NULL;
    }
    else if (cmd == ELF_C_NULL) {
	return NULL;
    }
    else if (cmd == ELF_C_WRITE) {
	ref = NULL;
    }
    else if (cmd != ELF_C_READ && cmd != ELF_C_RDWR) {
	seterr(ERROR_INVALID_CMD);
	return NULL;
    }
    else if (ref) {
	elf_assert(ref->e_magic == ELF_MAGIC);
	if (!ref->e_readable || (cmd == ELF_C_RDWR && !ref->e_writable)) {
	    seterr(ERROR_CMDMISMATCH);
	    return NULL;
	}
	if (ref->e_kind != ELF_K_AR) {
	    ref->e_count++;
	    return ref;
	}
	if (cmd == ELF_C_RDWR) {
	    seterr(ERROR_MEMBERWRITE);
	    return NULL;
	}
	if (ref->e_memory) {
	    fd = ref->e_fd;
	}
	else if (fd != ref->e_fd) {
	    seterr(ERROR_FDMISMATCH);
	    return NULL;
	}
	if (!(arhdr = _elf_arhdr(ref))) {
	    return NULL;
	}
	size = arhdr->ar_size;
    }
    else if ((off_t)(size = lseek(fd, (off_t)0, SEEK_END)) == (off_t)-1) {
	seterr(ERROR_IO_GETSIZE);
	return NULL;
    }

    if (!(elf = (Elf*)malloc(sizeof(Elf)))) {
	seterr(ERROR_MEM_ELF);
	return NULL;
    }
/*@-boundswrite@*/ /* structure assignment */
    *elf = _elf_init;
/*@=boundswrite@*/
    elf->e_fd = fd;
    elf->e_parent = ref;
    elf->e_size = elf->e_dsize = size;

    if (cmd != ELF_C_READ) {
	elf->e_writable = 1;
    }
    if (cmd != ELF_C_WRITE) {
	elf->e_readable = 1;
    }
    else {
	return elf;
    }

    if (ref) {
	size_t offset = ref->e_off + sizeof(struct ar_hdr);
	Elf *xelf;

	elf_assert(arhdr);
	elf->e_arhdr = arhdr;
	elf->e_base = ref->e_base + offset;
	/*
	 * Share the archive's memory image. To avoid
	 * multiple independent elf descriptors if the
	 * same member is requested twice, scan the list
	 * of open members for duplicates.
	 *
	 * I don't know how SVR4 handles this case. Don't rely on it.
	 */
	for (xelf = ref->e_members; xelf; xelf = xelf->e_link) {
	    elf_assert(xelf->e_parent == ref);
	    if (xelf->e_base == elf->e_base) {
/*@-nullpass@*/
		free(arhdr);
/*@=nullpass@*/
		free(elf);
		xelf->e_count++;
		return xelf;
	    }
	}
	if (size == 0) {
	    elf->e_data = NULL;
	}
	else if (ref->e_data == ref->e_rawdata) {
	    elf_assert(!ref->e_cooked);
	    /*
	     * archive is frozen - freeze member, too
	     */
	    elf->e_data = elf->e_rawdata = ref->e_data + offset;
	}
	else {
	    elf_assert(!ref->e_memory);
	    elf->e_data = ref->e_data + offset;
	    /*
	     * The member's memory image may have been modified if
	     * the member has been processed before. Since we need the
	     * original image, we have to re-read the archive file.
	     * Will fail if the archive's file descriptor is disabled.
	     */
/*@-boundswrite@*/
	    if (!ref->e_cooked) {
		ref->e_cooked = 1;
	    }
	    else if (!_elf_read(ref, elf->e_data, offset, size)) {
/*@-nullpass@*/
		free(arhdr);
/*@=nullpass@*/
		free(elf);
		return NULL;
	    }
/*@=boundswrite@*/
	}
	elf->e_next = offset + size + (size & 1);
	elf->e_disabled = ref->e_disabled;
	elf->e_memory = ref->e_memory;
	/* parent/child linking */
	elf->e_link = ref->e_members;
	ref->e_members = elf;
	ref->e_count++;
	/* Slowaris compatibility - do not rely on this! */
	ref->e_off = elf->e_next;
    }
    else if (size) {
/*@-boundswrite@*/
#if HAVE_MMAP
	/*
	 * Using mmap on writable files will interfere with elf_update
	 */
	if (!elf->e_writable && (elf->e_data = _elf_mmap(elf))) {
	    elf->e_unmap_data = 1;
	}
	else
#endif /* HAVE_MMAP */
	if (!(elf->e_data = _elf_read(elf, NULL, 0, size))) {
	    free(elf);
	    return NULL;
	}
/*@=boundswrite@*/
    }

    _elf_check_type(elf, size);
    return elf;
}

Elf*
elf_memory(char *image, size_t size) {
    Elf *elf;

    elf_assert(_elf_init.e_magic == ELF_MAGIC);
    if (_elf_version == EV_NONE) {
	seterr(ERROR_VERSION_UNSET);
	return NULL;
    }
    else if (size == 0 || image == NULL) {
	/* TODO: set error code? */
	return NULL;
    }

    if (!(elf = (Elf*)malloc(sizeof(Elf)))) {
	seterr(ERROR_MEM_ELF);
	return NULL;
    }
/*@-boundswrite@*/ /* structure assignment */
    *elf = _elf_init;
/*@=boundswrite@*/
    elf->e_size = elf->e_dsize = size;
    elf->e_data = elf->e_rawdata = image;
    elf->e_readable = 1;
    elf->e_disabled = 1;
    elf->e_memory = 1;

    _elf_check_type(elf, size);
    return elf;
}

#if __LIBELF64

int
gelf_getclass(Elf *elf) {
    if (elf && elf->e_kind == ELF_K_ELF && valid_class(elf->e_class)) {
	return elf->e_class;
    }
    return ELFCLASSNONE;
}

#endif /* __LIBELF64 */
/*@=boundswrite@*/

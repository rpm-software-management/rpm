/*
errors.h - exhaustive list of all error codes and messages for libelf.
Copyright (C) 1995 Michael Riepe <riepe@ifwsn4.ifw.uni-hannover.de>

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

__err__(ERROR_OK,		"no error")
__err__(ERROR_UNKNOWN,		"unknown error")
__err__(ERROR_INTERNAL,		"Internal error: unknown reason")
__err__(ERROR_UNIMPLEMENTED,	"Internal error: not implemented")
__err__(ERROR_SCNLIST_BROKEN,	"Internal error: broken section list")
__err__(ERROR_WRONLY,		"Request error: update(ELF_C_WRITE) on read-only file")
__err__(ERROR_INVALID_CMD,	"Request error: ELF_C_* invalid")
__err__(ERROR_FDDISABLED,	"Request error: file descriptor disabled")
__err__(ERROR_NOTARCHIVE,	"Request error: not an archive")
__err__(ERROR_BADOFF,		"Request error: offset out of range")
__err__(ERROR_UNKNOWN_VERSION,	"Request error: unknown version")
__err__(ERROR_CMDMISMATCH,	"Request error: ELF_C_* mismatch with parent")
__err__(ERROR_MEMBERWRITE,	"Request error: archive member begin() for writing")
__err__(ERROR_FDMISMATCH,	"Request error: archive/member file descriptor mismatch")
__err__(ERROR_NOTELF,		"Request error: not an ELF file")
__err__(ERROR_CLASSMISMATCH,	"Request error: class file/memory mismatch")
__err__(ERROR_UNKNOWN_TYPE,	"Request error: ELF_T_* invalid")
__err__(ERROR_UNKNOWN_ENCODING,	"Request error: unknown data encoding")
__err__(ERROR_DST2SMALL,	"Request error: destination too small")
__err__(ERROR_NULLBUF,		"Request error: d_buf is NULL")
__err__(ERROR_UNKNOWN_CLASS,	"Request error: unknown ELF class")
__err__(ERROR_ELFSCNMISMATCH,	"Request error: scn/elf descriptor mismatch")
__err__(ERROR_NOSUCHSCN,	"Request error: no section at index")
__err__(ERROR_NULLSCN,		"Request error: can't manipulate null section")
__err__(ERROR_SCNDATAMISMATCH,	"Request error: data/scn mismatch")
__err__(ERROR_NOSTRTAB,		"Request error: no string table")
__err__(ERROR_BADSTROFF,	"Request error: string table offset out of range")
__err__(ERROR_RDONLY,		"Request error: update() for write on read-only file")
__err__(ERROR_IO_SEEK,		"I/O error: seek")
__err__(ERROR_IO_2BIG,		"I/O error: file too big for memory")
__err__(ERROR_IO_READ,		"I/O error: raw read")
__err__(ERROR_IO_GETSIZE,	"I/O error: get file size")
__err__(ERROR_IO_WRITE,		"I/O error: output write")
__err__(ERROR_IO_TRUNC,		"I/O error: output truncate")
__err__(ERROR_VERSION_UNSET,	"Sequence error: version not set")
__err__(ERROR_NOEHDR,		"Sequence error: must create ehdr first")
__err__(ERROR_OUTSIDE,		"Format error: reference outside file")
__err__(ERROR_TRUNC_ARHDR,	"Format error: archive haeder truncated")
__err__(ERROR_ARFMAG,		"Format error: archive fmag")
__err__(ERROR_ARHDR,		"Format error: archive header")
__err__(ERROR_TRUNC_MEMBER,	"Format error: archive member truncated")
__err__(ERROR_SIZE_ARSYMTAB,	"Format error: archive symbol table size")
__err__(ERROR_ARSTRTAB,		"Format error: archive string table")
__err__(ERROR_ARSPECIAL,	"Format error: archive special name unknown")
__err__(ERROR_TRUNC_EHDR,	"Format error: ehdr truncated")
__err__(ERROR_TRUNC_PHDR,	"Format error: phdr table truncated")
__err__(ERROR_TRUNC_SHDR,	"Format error: shdr table truncated")
__err__(ERROR_TRUNC_SCN,	"Format error: data region truncated")
__err__(ERROR_SCN2SMALL,	"Format error: section sh_size too small for data")
__err__(ERROR_MEM_ELF,		"Memory error: elf descriptor")
__err__(ERROR_MEM_ARSYMTAB,	"Memory error: archive symbol table")
__err__(ERROR_MEM_ARHDR,	"Memory error: archive member header")
__err__(ERROR_MEM_EHDR,		"Memory error: ehdr")
__err__(ERROR_MEM_PHDR,		"Memory error: phdr table")
__err__(ERROR_MEM_SHDR,		"Memory error: shdr table")
__err__(ERROR_MEM_SCN,		"Memory error: section descriptor")
__err__(ERROR_MEM_SCNDATA,	"Memory error: section data")
__err__(ERROR_MEM_OUTBUF,	"Memory error: output file space")

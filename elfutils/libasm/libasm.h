/* Interface for libasm.
   Copyright (C) 2002 Red Hat, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _LIBASM_H
#define _LIBASM_H 1

#include <stdbool.h>
#include <stdint.h>

#include <gelf.h>


/* Opaque type for the assembler context descriptor.  */
typedef struct AsmCtx AsmCtx_t;

/* Opaque type for a section.  */
typedef struct AsmScn AsmScn_t;

/* Opaque type for a section group.  */
typedef struct AsmScnGrp AsmScnGrp_t;

/* Opaque type for a symbol.  */
typedef struct AsmSym AsmSym_t;


#ifdef __cplusplus
extern "C" {
#endif

/* Create output file and return descriptor for assembler context.  If
   TEXTP is true the output is an assembler format text file.
   Otherwise an object file is created.  The MACHINE parameter
   corresponds to an EM_ constant from <elf.h>, KLASS specifies the
   class (32- or 64-bit), and DATA specifies the byte order (little or
   big endian).  */
extern AsmCtx_t *asm_begin (const char *fname, bool textp, int machine,
			    int klass, int data);

/* Abort the operation on the assembler context and free all resources.  */
extern int asm_abort (AsmCtx_t *ctx);

/* Finalize output file and free all resources.  */
extern int asm_end (AsmCtx_t *ctx);


/* Return handle for the named section.  If it was not used before
   create it.  */
extern AsmScn_t *asm_newscn (AsmCtx_t *ctx, const char *scnname,
			     GElf_Word type, GElf_Xword flags);


/* Similar to 'asm_newscn', but make it part of section group GRP.  */
extern AsmScn_t *asm_newscn_ingrp (AsmCtx_t *ctx, const char *scnname,
				   GElf_Word type, GElf_Xword flags,
				   AsmScnGrp_t *grp);

/* Create new subsection NR in the given section.  */
extern AsmScn_t *asm_newsubscn (AsmScn_t *asmscn, unsigned int nr);


/* Return handle for new section group.  The signature symbol can be
   set later.  */
extern AsmScnGrp_t *asm_newscngrp (AsmCtx_t *ctx, const char *grpname,
				   AsmSym_t *signature, Elf32_Word flags);

/* Set or overwrite signature symbol for group.  */
extern int asm_scngrp_newsignature (AsmScnGrp_t *grp, AsmSym_t *signature);


/* Add zero terminated string STR of size LEN to (sub)section ASMSCN.  */
extern int asm_addstrz (AsmScn_t *asmscn, const char *str, size_t len);

/* Add 8-bit signed integer NUM to (sub)section ASMSCN.  */
extern int asm_addint8 (AsmScn_t *asmscn, int8_t num);

/* Add 8-bit unsigned integer NUM to (sub)section ASMSCN.  */
extern int asm_adduint8 (AsmScn_t *asmscn, uint8_t num);

/* Add 16-bit signed integer NUM to (sub)section ASMSCN.  */
extern int asm_addint16 (AsmScn_t *asmscn, int16_t num);

/* Add 16-bit unsigned integer NUM to (sub)section ASMSCN.  */
extern int asm_adduint16 (AsmScn_t *asmscn, uint16_t num);

/* Add 32-bit signed integer NUM to (sub)section ASMSCN.  */
extern int asm_addint32 (AsmScn_t *asmscn, int32_t num);

/* Add 32-bit unsigned integer NUM to (sub)section ASMSCN.  */
extern int asm_adduint32 (AsmScn_t *asmscn, uint32_t num);

/* Add 64-bit signed integer NUM to (sub)section ASMSCN.  */
extern int asm_addint64 (AsmScn_t *asmscn, int64_t num);

/* Add 64-bit unsigned integer NUM to (sub)section ASMSCN.  */
extern int asm_adduint64 (AsmScn_t *asmscn, uint64_t num);


/* Define new symbol NAME for current position in given section ASMSCN.  */
extern AsmSym_t *asm_newsym (AsmScn_t *asmscn, const char *name,
			     GElf_Xword size, int type, int binding);


/* Define new common symbol NAME with given SIZE and alignment.  */
extern AsmSym_t *asm_newcomsym (AsmCtx_t *ctx, const char *name,
				GElf_Xword size, GElf_Addr align);

/* Define new common symbol NAME with given SIZE, VALUE, TYPE, and BINDING.  */
extern AsmSym_t *asm_newabssym (AsmCtx_t *ctx, const char *name,
				GElf_Xword size, GElf_Addr value,
				int type, int binding);


/* Align (sub)section offset according to VALUE.  */
extern int asm_align (AsmScn_t *asmscn, GElf_Word value);

/* Set the byte pattern used to fill gaps created by alignment.  */
extern int asm_fill (AsmScn_t *asmscn, void *bytes, size_t len);


/* Return ELF descriptor created for the output file of the given context.  */
extern Elf *asm_getelf (AsmCtx_t *ctx);


/* Return error code of last failing function call.  This value is kept
   separately for each thread.  */
extern int asm_errno (void);

/* Return error string for ERROR.  If ERROR is zero, return error string
   for most recent error or NULL is none occurred.  If ERROR is -1 the
   behaviour is similar to the last case except that not NULL but a legal
   string is returned.  */
extern const char *asm_errmsg (int __error);

#ifdef __cplusplus
}
#endif

#endif	/* libasm.h */

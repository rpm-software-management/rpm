/* Error handling in libelf.
   Copyright (C) 1998, 1999, 2000, 2002 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <libintl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "libelfP.h"


/* This is the key for the thread specific memory.  */
static tls_key_t key;

/* The error number.  Used in non-threaded programs.  */
static int global_error;
static bool threaded;
/* We need to initialize the thread-specific data.  */
once_define (static, once);

/* The initialization and destruction functions.  */
static void init (void);
static void free_key_mem (void *mem);


int
elf_errno (void)
{
  int result;

  /* If we have not yet initialized the buffer do it now.  */
  once_execute (once, init);

  if (threaded)
    {
    /* We do not allocate memory for the data.  It is only a word.
       We can store it in place of the pointer.  */
      result = (intptr_t) getspecific (key);

      setspecific (key, (void *) (intptr_t) ELF_E_NOERROR);
      return result;
    }

  result = global_error;
  global_error = ELF_E_NOERROR;
  return result;
}


void
__libelf_seterrno (value)
     int value;
{
  /* If we have not yet initialized the buffer do it now.  */
  once_execute (once, init);

  if (threaded)
    /* We do not allocate memory for the data.  It is only a word.
       We can store it in place of the pointer.  */
    setspecific (key, (void *) (intptr_t) value);

  global_error = value;
}


/* Return the appropriate message for the error.  */
static const char msgstr[] =
{
#define ELF_E_NOERROR_IDX 0
  N_("no error")
  "\0"
#define ELF_E_UNKNOWN_VERSION_IDX (ELF_E_NOERROR_IDX + sizeof "no error")
  N_("unknown version")
  "\0"
#define ELF_E_UNKNOWN_TYPE_IDX \
  (ELF_E_UNKNOWN_VERSION_IDX + sizeof "unknown version")
  N_("unknown type")
  "\0"
#define ELF_E_INVALID_HANDLE_IDX \
  (ELF_E_UNKNOWN_TYPE_IDX + sizeof "unknown type")
  N_("invalid `Elf' handle")
  "\0"
#define ELF_E_SOURCE_SIZE_IDX \
  (ELF_E_INVALID_HANDLE_IDX + sizeof "invalid `Elf' handle")
  N_("invalid size of source operand")
  "\0"
#define ELF_E_DEST_SIZE_IDX \
  (ELF_E_SOURCE_SIZE_IDX + sizeof "invalid size of source operand")
  N_("invalid size of destination operand")
  "\0"
#define ELF_E_INVALID_ENCODING_IDX \
  (ELF_E_DEST_SIZE_IDX + sizeof "invalid size of destination operand")
  N_("invalid encoding")
  "\0"
#define ELF_E_NOMEM_IDX \
  (ELF_E_INVALID_ENCODING_IDX + sizeof "invalid encoding")
  N_("out of memory")
  "\0"
#define ELF_E_INVALID_FILE_IDX \
  (ELF_E_NOMEM_IDX + sizeof "out of memory")
  N_("invalid file descriptor")
  "\0"
#define ELF_E_INVALID_OP_IDX \
  (ELF_E_INVALID_FILE_IDX + sizeof "invalid file descriptor")
  N_("invalid operation")
  "\0"
#define ELF_E_NO_VERSION_IDX \
  (ELF_E_INVALID_OP_IDX + sizeof "invalid operation")
  N_("ELF version not set")
  "\0"
#define ELF_E_INVALID_CMD_IDX \
  (ELF_E_NO_VERSION_IDX + sizeof "ELF version not set")
  N_("invalid command")
  "\0"
#define ELF_E_RANGE_IDX \
  (ELF_E_INVALID_CMD_IDX + sizeof "invalid command")
  N_("offset out of range")
  "\0"
#define ELF_E_ARCHIVE_FMAG_IDX \
  (ELF_E_RANGE_IDX + sizeof "offset out of range")
  N_("invalid fmag field in archive header")
  "\0"
#define ELF_E_INVALID_ARCHIVE_IDX \
  (ELF_E_ARCHIVE_FMAG_IDX + sizeof "invalid fmag field in archive header")
  N_("invalid archive file")
  "\0"
#define ELF_E_NO_ARCHIVE_IDX \
  (ELF_E_INVALID_ARCHIVE_IDX + sizeof "invalid archive file")
  N_("descriptor is not for an archive")
  "\0"
#define ELF_E_NO_INDEX_IDX \
  (ELF_E_NO_ARCHIVE_IDX + sizeof "descriptor is not for an archive")
  N_("no index available")
  "\0"
#define ELF_E_READ_ERROR_IDX \
  (ELF_E_NO_INDEX_IDX + sizeof "no index available")
  N_("cannot read data from file")
  "\0"
#define ELF_E_WRITE_ERROR_IDX \
  (ELF_E_READ_ERROR_IDX + sizeof "cannot read data from file")
  N_("cannot write data to file")
  "\0"
#define ELF_E_INVALID_CLASS_IDX \
  (ELF_E_WRITE_ERROR_IDX + sizeof "cannot write data to file")
  N_("invalid binary class")
  "\0"
#define ELF_E_INVALID_INDEX_IDX \
  (ELF_E_INVALID_CLASS_IDX + sizeof "invalid binary class")
  N_("invalid section index")
  "\0"
#define ELF_E_INVALID_OPERAND_IDX \
  (ELF_E_INVALID_INDEX_IDX + sizeof "invalid section index")
  N_("invalid operand")
  "\0"
#define ELF_E_INVALID_SECTION_IDX \
  (ELF_E_INVALID_OPERAND_IDX + sizeof "invalid operand")
  N_("invalid section")
  "\0"
#define ELF_E_INVALID_COMMAND_IDX \
  (ELF_E_INVALID_SECTION_IDX + sizeof "invalid section")
  N_("invalid command")
  "\0"
#define ELF_E_WRONG_ORDER_EHDR_IDX \
  (ELF_E_INVALID_COMMAND_IDX + sizeof "invalid command")
  N_("executable header not created first")
  "\0"
#define ELF_E_FD_DISABLED_IDX \
  (ELF_E_WRONG_ORDER_EHDR_IDX + sizeof "executable header not created first")
  N_("file descriptor disabled")
  "\0"
#define ELF_E_FD_MISMATCH_IDX \
  (ELF_E_FD_DISABLED_IDX + sizeof "file descriptor disabled")
  N_("archive/member fildes mismatch")
  "\0"
#define ELF_E_OFFSET_RANGE_IDX \
  (ELF_E_FD_MISMATCH_IDX + sizeof "archive/member fildes mismatch")
  N_("offset out of range")
  "\0"
#define ELF_E_NOT_NUL_SECTION_IDX \
  (ELF_E_OFFSET_RANGE_IDX + sizeof "offset out of range")
  N_("cannot manipulate null section")
  "\0"
#define ELF_E_DATA_MISMATCH_IDX \
  (ELF_E_NOT_NUL_SECTION_IDX + sizeof "cannot manipulate null section")
  N_("data/scn mismatch")
  "\0"
#define ELF_E_INVALID_SECTION_HEADER_IDX \
  (ELF_E_DATA_MISMATCH_IDX + sizeof "data/scn mismatch")
  N_("invalid section header")
  "\0"
#define ELF_E_INVALID_DATA_IDX \
  (ELF_E_INVALID_SECTION_HEADER_IDX + sizeof "invalid section header")
  N_("invalid data")
  "\0"
#define ELF_E_DATA_ENCODING_IDX \
  (ELF_E_INVALID_DATA_IDX + sizeof "invalid data")
  N_("unknown data encoding")
  "\0"
#define ELF_E_SECTION_TOO_SMALL_IDX \
  (ELF_E_DATA_ENCODING_IDX + sizeof "unknown data encoding")
  N_("section `sh_size' too small for data")
  "\0"
#define ELF_E_INVALID_ALIGN_IDX \
  (ELF_E_SECTION_TOO_SMALL_IDX + sizeof "section `sh_size' too small for data")
  N_("invalid section alignment")
  "\0"
#define ELF_E_INVALID_SHENTSIZE_IDX \
  (ELF_E_INVALID_ALIGN_IDX + sizeof "invalid section alignment")
  N_("invalid section entry size")
  "\0"
#define ELF_E_UPDATE_RO_IDX \
  (ELF_E_INVALID_SHENTSIZE_IDX + sizeof "invalid section entry size")
  N_("update() for write on read-only file")
  "\0"
#define ELF_E_NOFILE_IDX \
  (ELF_E_UPDATE_RO_IDX + sizeof "update() for write on read-only file")
  N_("no such file")
  "\0"
#define ELF_E_GROUP_NOT_REL_IDX \
  (ELF_E_NOFILE_IDX + sizeof "no such file")
  N_("only relocatable files can contain section groups")
  "\0"
#define ELF_E_INVALID_PHDR_IDX \
  (ELF_E_GROUP_NOT_REL_IDX \
   + sizeof "only relocatable files can contain section groups")
  N_("program header only allowed in executables and shared objects")
};


static const uint_fast16_t msgidx[ELF_E_NUM] =
{
  [ELF_E_NOERROR] = ELF_E_NOERROR_IDX,
  [ELF_E_UNKNOWN_VERSION] = ELF_E_UNKNOWN_VERSION_IDX,
  [ELF_E_UNKNOWN_TYPE] = ELF_E_UNKNOWN_TYPE_IDX,
  [ELF_E_INVALID_HANDLE] = ELF_E_INVALID_HANDLE_IDX,
  [ELF_E_SOURCE_SIZE] = ELF_E_SOURCE_SIZE_IDX,
  [ELF_E_DEST_SIZE] = ELF_E_DEST_SIZE_IDX,
  [ELF_E_INVALID_ENCODING] = ELF_E_INVALID_ENCODING_IDX,
  [ELF_E_NOMEM] = ELF_E_NOMEM_IDX,
  [ELF_E_INVALID_FILE] = ELF_E_INVALID_FILE_IDX,
  [ELF_E_INVALID_OP] = ELF_E_INVALID_OP_IDX,
  [ELF_E_NO_VERSION] = ELF_E_NO_VERSION_IDX,
  [ELF_E_INVALID_CMD] = ELF_E_INVALID_CMD_IDX,
  [ELF_E_RANGE] = ELF_E_RANGE_IDX,
  [ELF_E_ARCHIVE_FMAG] = ELF_E_ARCHIVE_FMAG_IDX,
  [ELF_E_INVALID_ARCHIVE] = ELF_E_INVALID_ARCHIVE_IDX,
  [ELF_E_NO_ARCHIVE] = ELF_E_NO_ARCHIVE_IDX,
  [ELF_E_NO_INDEX] = ELF_E_NO_INDEX_IDX,
  [ELF_E_READ_ERROR] = ELF_E_READ_ERROR_IDX,
  [ELF_E_WRITE_ERROR] = ELF_E_WRITE_ERROR_IDX,
  [ELF_E_INVALID_CLASS] = ELF_E_INVALID_CLASS_IDX,
  [ELF_E_INVALID_INDEX] = ELF_E_INVALID_INDEX_IDX,
  [ELF_E_INVALID_OPERAND] = ELF_E_INVALID_OPERAND_IDX,
  [ELF_E_INVALID_SECTION] = ELF_E_INVALID_SECTION_IDX,
  [ELF_E_INVALID_COMMAND] = ELF_E_INVALID_COMMAND_IDX,
  [ELF_E_WRONG_ORDER_EHDR] = ELF_E_WRONG_ORDER_EHDR_IDX,
  [ELF_E_FD_DISABLED] = ELF_E_FD_DISABLED_IDX,
  [ELF_E_FD_MISMATCH] = ELF_E_FD_MISMATCH_IDX,
  [ELF_E_OFFSET_RANGE] = ELF_E_OFFSET_RANGE_IDX,
  [ELF_E_NOT_NUL_SECTION] = ELF_E_NOT_NUL_SECTION_IDX,
  [ELF_E_DATA_MISMATCH] = ELF_E_DATA_MISMATCH_IDX,
  [ELF_E_INVALID_SECTION_HEADER] = ELF_E_INVALID_SECTION_HEADER_IDX,
  [ELF_E_INVALID_DATA] = ELF_E_INVALID_DATA_IDX,
  [ELF_E_DATA_ENCODING] = ELF_E_DATA_ENCODING_IDX,
  [ELF_E_SECTION_TOO_SMALL] = ELF_E_SECTION_TOO_SMALL_IDX,
  [ELF_E_INVALID_ALIGN] = ELF_E_INVALID_ALIGN_IDX,
  [ELF_E_INVALID_SHENTSIZE] = ELF_E_INVALID_SHENTSIZE_IDX,
  [ELF_E_UPDATE_RO] = ELF_E_UPDATE_RO_IDX,
  [ELF_E_NOFILE] = ELF_E_NOFILE_IDX,
  [ELF_E_GROUP_NOT_REL] = ELF_E_GROUP_NOT_REL_IDX,
  [ELF_E_INVALID_PHDR] = ELF_E_INVALID_PHDR_IDX
};


const char *
elf_errmsg (error)
     int error;
{
  int last_error;

  /* If we have not yet initialized the buffer do it now.  */
  once_execute (once, init);

  if ((error == 0 || error == -1) && threaded)
    /* We do not allocate memory for the data.  It is only a word.
       We can store it in place of the pointer.  */
    last_error = (intptr_t) getspecific (key);
  else
    last_error = global_error;

  if (error == 0)
    {
      assert (msgidx[last_error] < sizeof (msgstr));
      return last_error != 0 ? _(msgstr + msgidx[last_error]) : NULL;
    }
  else if (error < -1)
    return _("Unknown error");

  assert (msgidx[error == -1 ? last_error : error] < sizeof (msgstr));
  return _(msgstr + msgidx[error == -1 ? last_error : error]);
}


/* Free the thread specific data, this is done if a thread terminates.  */
static void
free_key_mem (void *mem)
{
  setspecific (key, NULL);
}


/* Initialize the key for the global variable.  */
static void
init (void)
{
  if (key_create (&key, free_key_mem) == 0)
    /* Creating the key succeeded.  */
    threaded = true;
}

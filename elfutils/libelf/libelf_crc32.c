/* Copyright (C) 2002 Red Hat, Inc.

  This program is Open Source software; you can redistribute it and/or
  modify it under the terms of the Open Software License version 1.0 as
  published by the Open Source Initiative.

  You should have received a copy of the Open Software License along
  with this program; if not, you may obtain a copy of the Open Software
  License version 1.0 from http://www.opensource.org/licenses/osl.php or
  by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
  3001 King Ranch Road, Ukiah, CA 95482.   */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define crc32 attribute_hidden __libelf_crc32
#define LIB_SYSTEM_H	1
#include <libelf.h>
#include "../lib/crc32.c"

/*
32.fsize.c - implementation of the elf32_fsize(3) function.
Copyright (C) 1995, 1996 Michael Riepe <michael@stud.uni-hannover.de>

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
#include <ext_types.h>

const size_t
_elf32_fmsize[EV_CURRENT - EV_NONE][ELF_T_NUM][2] = {
    {
	{ sizeof(unsigned char),    sizeof(unsigned char)	},
	{ sizeof(Elf32_Addr),	    sizeof(__ext_Elf32_Addr)	},
	{ sizeof(Elf32_Dyn),	    sizeof(__ext_Elf32_Dyn)	},
	{ sizeof(Elf32_Ehdr),	    sizeof(__ext_Elf32_Ehdr)	},
	{ sizeof(Elf32_Half),	    sizeof(__ext_Elf32_Half)	},
	{ sizeof(Elf32_Off),	    sizeof(__ext_Elf32_Off)	},
	{ sizeof(Elf32_Phdr),	    sizeof(__ext_Elf32_Phdr)	},
	{ sizeof(Elf32_Rela),	    sizeof(__ext_Elf32_Rela)	},
	{ sizeof(Elf32_Rel),	    sizeof(__ext_Elf32_Rel)	},
	{ sizeof(Elf32_Shdr),	    sizeof(__ext_Elf32_Shdr)	},
	{ sizeof(Elf32_Sword),	    sizeof(__ext_Elf32_Sword)	},
	{ sizeof(Elf32_Sym),	    sizeof(__ext_Elf32_Sym)	},
	{ sizeof(Elf32_Word),	    sizeof(__ext_Elf32_Word)	},
    },
};

size_t
elf32_fsize(Elf_Type type, size_t count, unsigned ver) {
    size_t n;

    if (!valid_version(ver)) {
	seterr(ERROR_UNKNOWN_VERSION);
    }
    else if (!valid_type(type)) {
	seterr(ERROR_UNKNOWN_TYPE);
    }
    else if (!(n = _fsize32(ver, type))) {
	seterr(ERROR_UNKNOWN_TYPE);
    }
    else if (count) {
	return count * n;
    }
    return 0;
}

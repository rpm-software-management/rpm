#include "system.h"

#ifdef BUILTIN_ELF
#include "file.h"
#include "readelf.h"
#include "debug.h"

FILE_RCSID("@(#)Id: readelf.c,v 1.22 2002/07/03 18:26:38 christos Exp ")

/*@access fmagic @*/

/*@-bounds@*/
static uint16_t
getu16(const fmagic fm, uint16_t value)
	/*@*/
{
	union {
		uint16_t ui;
		char c[2];
	} retval, tmpval;

	if (fm->swap) {
		tmpval.ui = value;

		retval.c[0] = tmpval.c[1];
		retval.c[1] = tmpval.c[0];
		
		return retval.ui;
	} else
		return value;
}

static uint32_t
getu32(const fmagic fm, uint32_t value)
	/*@*/
{
	union {
		uint32_t ui;
		char c[4];
	} retval, tmpval;

	if (fm->swap) {
		tmpval.ui = value;

		retval.c[0] = tmpval.c[3];
		retval.c[1] = tmpval.c[2];
		retval.c[2] = tmpval.c[1];
		retval.c[3] = tmpval.c[0];
		
		return retval.ui;
	} else
		return value;
}

static uint64_t
getu64(const fmagic fm, uint64_t value)
	/*@*/
{
	union {
		uint64_t ui;
		char c[8];
	} retval, tmpval;

	if (fm->swap) {
		tmpval.ui = value;

		retval.c[0] = tmpval.c[7];
		retval.c[1] = tmpval.c[6];
		retval.c[2] = tmpval.c[5];
		retval.c[3] = tmpval.c[4];
		retval.c[4] = tmpval.c[3];
		retval.c[5] = tmpval.c[2];
		retval.c[6] = tmpval.c[1];
		retval.c[7] = tmpval.c[0];
		
		return retval.ui;
	} else
		return value;
}
/*@=bounds@*/

#define sh_addr		(fm->cls == ELFCLASS32		\
			 ? (void *) &sh32		\
			 : (void *) &sh64)
#define sh_size		(fm->cls == ELFCLASS32		\
			 ? sizeof sh32			\
			 : sizeof sh64)
#define shs_type	(fm->cls == ELFCLASS32		\
			 ? getu32(fm, sh32.sh_type)	\
			 : getu32(fm, sh64.sh_type))
#define ph_addr		(fm->cls == ELFCLASS32		\
			 ? (void *) &ph32		\
			 : (void *) &ph64)
#define ph_size		(fm->cls == ELFCLASS32		\
			 ? sizeof ph32			\
			 : sizeof ph64)
#define ph_type		(fm->cls == ELFCLASS32		\
			 ? getu32(fm, ph32.p_type)	\
			 : getu32(fm, ph64.p_type))
#define ph_offset	(fm->cls == ELFCLASS32		\
			 ? getu32(fm, ph32.p_offset)	\
			 : getu64(fm, ph64.p_offset))
#define ph_align	(fm->cls == ELFCLASS32		\
			 ? (ph32.p_align ? getu32(fm, ph32.p_align) : 4) \
			 : (ph64.p_align ? getu64(fm, ph64.p_align) : 4))
#define nh_size		(fm->cls == ELFCLASS32		\
			 ? sizeof *nh32			\
			 : sizeof *nh64)
#define nh_type		(fm->cls == ELFCLASS32		\
			 ? getu32(fm, nh32->n_type)	\
			 : getu32(fm, nh64->n_type))
#define nh_namesz	(fm->cls == ELFCLASS32		\
			 ? getu32(fm, nh32->n_namesz)	\
			 : getu32(fm, nh64->n_namesz))
#define nh_descsz	(fm->cls == ELFCLASS32		\
			 ? getu32(fm, nh32->n_descsz)	\
			 : getu32(fm, nh64->n_descsz))
#define prpsoffsets(i)	(fm->cls == ELFCLASS32		\
			 ? prpsoffsets32[i]		\
			 : prpsoffsets64[i])

/*@-bounds@*/
static void
doshn(fmagic fm, off_t off, int num, size_t size)
	/*@globals fileSystem @*/
	/*@modifies fm, fileSystem @*/
{
	Elf32_Shdr sh32;
	Elf64_Shdr sh64;

	if (size != sh_size) {
		error(EXIT_FAILURE, 0, "corrupted program header size.\n");
		/*@notreached@*/
	}

	if (lseek(fm->fd, off, SEEK_SET) == -1) {
		error(EXIT_FAILURE, 0, "lseek failed (%s).\n", strerror(errno));
		/*@notreached@*/
	}

	for ( ; num; num--) {
		if (read(fm->fd, sh_addr, size) == -1) {
			error(EXIT_FAILURE, 0, "read failed (%s).\n", strerror(errno));
			/*@notreached@*/
		}
		if (shs_type == SHT_SYMTAB /* || shs_type == SHT_DYNSYM */) {
			file_printf(fm, ", not stripped");
			return;
		}
	}
	file_printf(fm, ", stripped");
}
/*@=bounds@*/

/*
 * Look through the program headers of an executable image, searching
 * for a PT_INTERP section; if one is found, it's dynamically linked,
 * otherwise it's statically linked.
 */
/*@-bounds@*/
static void
dophn_exec(fmagic fm, off_t off, int num, size_t size)
	/*@globals fileSystem @*/
	/*@modifies fm, fileSystem @*/
{
	Elf32_Phdr ph32;
	Elf32_Nhdr *nh32 = NULL;
	Elf64_Phdr ph64;
	Elf64_Nhdr *nh64 = NULL;
	char *linking_style = "statically";
	char *shared_libraries = "";
	char nbuf[BUFSIZ];
	int bufsize;
	size_t offset, nameoffset;

	if (size != ph_size) {
		error(EXIT_FAILURE, 0, "corrupted program header size.\n");
		/*@notreached@*/
	}

	if (lseek(fm->fd, off, SEEK_SET) == -1) {
		error(EXIT_FAILURE, 0, "lseek failed (%s).\n", strerror(errno));
		/*@notreached@*/
	}

  	for ( ; num; num--) {
  		if (read(fm->fd, ph_addr, size) == -1) {
  			error(EXIT_FAILURE, 0, "read failed (%s).\n", strerror(errno));
			/*@notreached@*/
		}

		switch (ph_type) {
		case PT_DYNAMIC:
			linking_style = "dynamically";
			/*@switchbreak@*/ break;
		case PT_INTERP:
			shared_libraries = " (uses shared libs)";
			/*@switchbreak@*/ break;
		case PT_NOTE:
			/*
			 * This is a PT_NOTE section; loop through all the notes
			 * in the section.
			 */
			if (lseek(fm->fd, (off_t) ph_offset, SEEK_SET) == -1) {
				error(EXIT_FAILURE, 0, "lseek failed (%s).\n", strerror(errno));
				/*@notreached@*/
			}
			bufsize = read(fm->fd, nbuf, sizeof(nbuf));
			if (bufsize == -1) {
				error(EXIT_FAILURE, 0, ": " "read failed (%s).\n",
				    strerror(errno));
				/*@notreached@*/
			}
			offset = 0;
			for (;;) {
				if (offset >= bufsize)
					/*@innerbreak@*/ break;
				if (fm->cls == ELFCLASS32)
					nh32 = (Elf32_Nhdr *)&nbuf[offset];
				else
					nh64 = (Elf64_Nhdr *)&nbuf[offset];
				offset += nh_size;
	
				if (offset + nh_namesz >= bufsize) {
					/*
					 * We're past the end of the buffer.
					 */
					/*@innerbreak@*/ break;
				}

				nameoffset = offset;
				offset += nh_namesz;
				offset = ((offset+ph_align-1)/ph_align)*ph_align;

				if ((nh_namesz == 0) && (nh_descsz == 0)) {
					/*
					 * We're out of note headers.
					 */
					/*@innerbreak@*/ break;
				}

				if (offset + nh_descsz >= bufsize)
					/*@innerbreak@*/ break;

				if (nh_namesz == 4 &&
				    strcmp(&nbuf[nameoffset], "GNU") == 0 &&
				    nh_type == NT_GNU_VERSION &&
				    nh_descsz == 16) {
					uint32_t *desc =
					    (uint32_t *)&nbuf[offset];

					file_printf(fm, ", for GNU/");
					switch (getu32(fm, desc[0])) {
					case GNU_OS_LINUX:
						file_printf(fm, "Linux");
						/*@switchbreak@*/ break;
					case GNU_OS_HURD:
						file_printf(fm, "Hurd");
						/*@switchbreak@*/ break;
					case GNU_OS_SOLARIS:
						file_printf(fm, "Solaris");
						/*@switchbreak@*/ break;
					default:
						file_printf(fm, "<unknown>");
						/*@switchbreak@*/ break;
					}
					file_printf(fm, " %d.%d.%d",
					    getu32(fm, desc[1]),
					    getu32(fm, desc[2]),
					    getu32(fm, desc[3]));
				}

				if (nh_namesz == 7 &&
				    strcmp(&nbuf[nameoffset], "NetBSD") == 0 &&
				    nh_type == NT_NETBSD_VERSION &&
				    nh_descsz == 4) {
					file_printf(fm, ", for NetBSD");
					/*
					 * Version number is stuck at 199905,
					 * and hence is basically content-free.
					 */
				}

				if (nh_namesz == 8 &&
				    strcmp(&nbuf[nameoffset], "FreeBSD") == 0 &&
				    nh_type == NT_FREEBSD_VERSION &&
				    nh_descsz == 4) {
					uint32_t desc = getu32(fm,
					    *(uint32_t *)&nbuf[offset]);
					file_printf(fm, ", for FreeBSD");
					/*
					 * Contents is __FreeBSD_version,
					 * whose relation to OS versions is
					 * defined by a huge table in the
					 * Porters' Handbook.  Happily, the
					 * first three digits are the version
					 * number, at least in versions of
					 * FreeBSD that use this note.
					 */

					file_printf(fm, " %d.%d", desc / 100000,
					    desc / 10000 % 10);
					if (desc / 1000 % 10 > 0)
						file_printf(fm, ".%d",
						    desc / 1000 % 10);
				}

				if (nh_namesz == 8 &&
				    strcmp(&nbuf[nameoffset], "OpenBSD") == 0 &&
				    nh_type == NT_OPENBSD_VERSION &&
				    nh_descsz == 4) {
					file_printf(fm, ", for OpenBSD");
					/* Content of note is always 0 */
				}
			}
			if ((lseek(fm->fd, ph_offset + offset, SEEK_SET)) == -1) {
			    error(EXIT_FAILURE, 0, "lseek failed (%s).\n", strerror(errno));
			    /*@notreached@*/
			}
			/*@switchbreak@*/ break;
		}
	}
	file_printf(fm, ", %s linked%s", linking_style, shared_libraries);
}
/*@=bounds@*/

#ifdef ELFCORE
/*@unchecked@*/ /*@observer@*/
static size_t	prpsoffsets32[] = {
	8,		/* FreeBSD */
	28,		/* Linux 2.0.36 */
	32,		/* Linux (I forget which kernel version) */
	84		/* SunOS 5.x */
};

/*@unchecked@*/ /*@observer@*/
static size_t	prpsoffsets64[] = {
       120		/* SunOS 5.x, 64-bit */
};

#define	NOFFSETS32	(sizeof prpsoffsets32 / sizeof prpsoffsets32[0])
#define NOFFSETS64	(sizeof prpsoffsets64 / sizeof prpsoffsets64[0])

#define NOFFSETS	(fm->cls == ELFCLASS32 ? NOFFSETS32 : NOFFSETS64)

/*
 * Look through the program headers of an executable image, searching
 * for a PT_NOTE section of type NT_PRPSINFO, with a name "CORE" or
 * "FreeBSD"; if one is found, try looking in various places in its
 * contents for a 16-character string containing only printable
 * characters - if found, that string should be the name of the program
 * that dropped core.  Note: right after that 16-character string is,
 * at least in SunOS 5.x (and possibly other SVR4-flavored systems) and
 * Linux, a longer string (80 characters, in 5.x, probably other
 * SVR4-flavored systems, and Linux) containing the start of the
 * command line for that program.
 *
 * The signal number probably appears in a section of type NT_PRSTATUS,
 * but that's also rather OS-dependent, in ways that are harder to
 * dissect with heuristics, so I'm not bothering with the signal number.
 * (I suppose the signal number could be of interest in situations where
 * you don't have the binary of the program that dropped core; if you
 * *do* have that binary, the debugger will probably tell you what
 * signal it was.)
 */

#define	OS_STYLE_SVR4		0
#define	OS_STYLE_FREEBSD	1
#define	OS_STYLE_NETBSD		2

/*@unchecked@*/ /*@observer@*/
static const char *os_style_names[] = {
	"SVR4",
	"FreeBSD",
	"NetBSD",
};

/*@-bounds@*/
static void
dophn_core(fmagic fm, off_t off, int num, size_t size)
	/*@globals fileSystem @*/
	/*@modifies fm, fileSystem @*/
{
	Elf32_Phdr ph32;
	Elf32_Nhdr *nh32 = NULL;
	Elf64_Phdr ph64;
	Elf64_Nhdr *nh64 = NULL;
	size_t offset, nameoffset, noffset, reloffset;
	unsigned char c;
	int i, j;
	char nbuf[BUFSIZ];
	int bufsize;
	int os_style = -1;

	if (size != ph_size) {
		error(EXIT_FAILURE, 0, "corrupted program header size.\n");
		/*@notreached@*/
	}

	/*
	 * Loop through all the program headers.
	 */
	for ( ; num; num--) {
		if (lseek(fm->fd, off, SEEK_SET) == -1) {
			error(EXIT_FAILURE, 0, "lseek failed (%s).\n", strerror(errno));
			/*@notreached@*/
		}
		if (read(fm->fd, ph_addr, size) == -1) {
			error(EXIT_FAILURE, 0, "read failed (%s).\n", strerror(errno));
			/*@notreached@*/
		}
		off += size;
		if (ph_type != PT_NOTE)
			continue;

		/*
		 * This is a PT_NOTE section; loop through all the notes
		 * in the section.
		 */
		if (lseek(fm->fd, (off_t) ph_offset, SEEK_SET) == -1) {
			error(EXIT_FAILURE, 0, "lseek failed (%s).\n", strerror(errno));
			/*@notreached@*/
		}
		bufsize = read(fm->fd, nbuf, BUFSIZ);
		if (bufsize == -1) {
			error(EXIT_FAILURE, 0, ": " "read failed (%s).\n", strerror(errno));
			/*@notreached@*/
		}
		offset = 0;
		for (;;) {
			if (offset >= bufsize)
				/*@innerbreak@*/ break;
			if (fm->cls == ELFCLASS32)
				nh32 = (Elf32_Nhdr *)&nbuf[offset];
			else
				nh64 = (Elf64_Nhdr *)&nbuf[offset];
			offset += nh_size;

			/*
			 * Check whether this note has the name "CORE" or
			 * "FreeBSD", or "NetBSD-CORE".
			 */
			if (offset + nh_namesz >= bufsize) {
				/*
				 * We're past the end of the buffer.
				 */
				/*@innerbreak@*/ break;
			}

			nameoffset = offset;
			offset += nh_namesz;
			offset = ((offset + 3)/4)*4;

			/*
			 * Sigh.  The 2.0.36 kernel in Debian 2.1, at
			 * least, doesn't correctly implement name
			 * sections, in core dumps, as specified by
			 * the "Program Linking" section of "UNIX(R) System
			 * V Release 4 Programmer's Guide: ANSI C and
			 * Programming Support Tools", because my copy
			 * clearly says "The first 'namesz' bytes in 'name'
			 * contain a *null-terminated* [emphasis mine]
			 * character representation of the entry's owner
			 * or originator", but the 2.0.36 kernel code
			 * doesn't include the terminating null in the
			 * name....
			 */
			if (os_style == -1) {
				if ((nh_namesz == 4 &&
				     strncmp(&nbuf[nameoffset],
					    "CORE", 4) == 0) ||
				    (nh_namesz == 5 &&
				     strcmp(&nbuf[nameoffset],
				     	    "CORE") == 0)) {
					os_style = OS_STYLE_SVR4;
				} else
				if ((nh_namesz == 8 &&
				     strcmp(&nbuf[nameoffset],
				     	    "FreeBSD") == 0)) {
					os_style = OS_STYLE_FREEBSD;
				} else
				if ((nh_namesz >= 11 &&
				     strncmp(&nbuf[nameoffset],
				     	     "NetBSD-CORE", 11) == 0)) {
					os_style = OS_STYLE_NETBSD;
				} else
					/*@innercontinue@*/ continue;
				file_printf(fm, ", %s-style", os_style_names[os_style]);
			}

			if (os_style == OS_STYLE_NETBSD &&
			    nh_type == NT_NETBSD_CORE_PROCINFO) {
				uint32_t signo;

				/*
				 * Extract the program name.  It is at
				 * offset 0x7c, and is up to 32-bytes,
				 * including the terminating NUL.
				 */
				file_printf(fm, ", from '%.31s'", &nbuf[offset + 0x7c]);
				
				/*
				 * Extract the signal number.  It is at
				 * offset 0x08.
				 */
				memcpy(&signo, &nbuf[offset + 0x08],
				    sizeof(signo));
				file_printf(fm, " (signal %u)", getu32(fm, signo));
			} else
			if (os_style != OS_STYLE_NETBSD &&
			    nh_type == NT_PRPSINFO) {
				/*
				 * Extract the program name.  We assume
				 * it to be 16 characters (that's what it
				 * is in SunOS 5.x and Linux).
				 *
				 * Unfortunately, it's at a different offset
				 * in varous OSes, so try multiple offsets.
				 * If the characters aren't all printable,
				 * reject it.
				 */
				for (i = 0; i < NOFFSETS; i++) {
					reloffset = prpsoffsets(i);
					noffset = offset + reloffset;
					for (j = 0; j < 16;
					    j++, noffset++, reloffset++) {
						/*
						 * Make sure we're not past
						 * the end of the buffer; if
						 * we are, just give up.
						 */
						if (noffset >= bufsize)
							goto tryanother;

						/*
						 * Make sure we're not past
						 * the end of the contents;
						 * if we are, this obviously
						 * isn't the right offset.
						 */
						if (reloffset >= nh_descsz)
							goto tryanother;

						c = nbuf[noffset];
						if (c == '\0') {
							/*
							 * A '\0' at the
							 * beginning is
							 * obviously wrong.
							 * Any other '\0'
							 * means we're done.
							 */
							if (j == 0)
								goto tryanother;
							else
								/*@innerbreak@*/ break;
						} else {
							/*
							 * A nonprintable
							 * character is also
							 * wrong.
							 */
#define isquote(c) (strchr("'\"`", (c)) != NULL)
							if (!isprint(c) ||
							     isquote(c))
								goto tryanother;
						}
					}

					/*
					 * Well, that worked.
					 */
					file_printf(fm, ", from '%.16s'",
					    &nbuf[offset + prpsoffsets(i)]);
					/*@innerbreak@*/ break;

				tryanother:
					;
				}
				/*@innerbreak@*/ break;
			}
			offset += nh_descsz;
			offset = ((offset + 3)/4)*4;
		}
	}
}
/*@=bounds@*/
#endif

/*@-bounds@*/
void
fmagicE(fmagic fm)
{
/*@-sizeoftype@*/
	union {
		int32_t l;
		char c[sizeof (int32_t)];
	} u;
/*@=sizeoftype@*/

	/*
	 * If we can't seek, it must be a pipe, socket or fifo.
	 */
	if((lseek(fm->fd, (off_t)0, SEEK_SET) == (off_t)-1) && (errno == ESPIPE))
		fm->fd = file_pipe2file(fm->fd, fm->buf, fm->nb);

	/*
	 * ELF executables have multiple section headers in arbitrary
	 * file locations and thus file(1) cannot determine it from easily.
	 * Instead we traverse thru all section headers until a symbol table
	 * one is found or else the binary is stripped.
	 */
	if (fm->buf[EI_MAG0] != ELFMAG0
	    || (fm->buf[EI_MAG1] != ELFMAG1 && fm->buf[EI_MAG1] != OLFMAG1)
	    || fm->buf[EI_MAG2] != ELFMAG2 || fm->buf[EI_MAG3] != ELFMAG3)
	    return;


	fm->cls = fm->buf[EI_CLASS];

	if (fm->cls == ELFCLASS32) {
		Elf32_Ehdr elfhdr;
		if (fm->nb <= sizeof (elfhdr))
			return;


		u.l = 1;
		(void) memcpy(&elfhdr, fm->buf, sizeof elfhdr);
/*@-sizeoftype@*/
		fm->swap = (u.c[sizeof(int32_t) - 1] + 1) != elfhdr.e_ident[EI_DATA];
/*@=sizeoftype@*/

		if (getu16(fm, elfhdr.e_type) == ET_CORE) 
#ifdef ELFCORE
			dophn_core(fm,
				   getu32(fm, elfhdr.e_phoff),
				   getu16(fm, elfhdr.e_phnum), 
				   getu16(fm, elfhdr.e_phentsize));
#else
			;
#endif
		else {
			if (getu16(fm, elfhdr.e_type) == ET_EXEC) {
				dophn_exec(fm,
					   getu32(fm, elfhdr.e_phoff),
					   getu16(fm, elfhdr.e_phnum), 
					   getu16(fm, elfhdr.e_phentsize));
			}
			doshn(fm,
			      getu32(fm, elfhdr.e_shoff),
			      getu16(fm, elfhdr.e_shnum),
			      getu16(fm, elfhdr.e_shentsize));
		}
		return;
	}

        if (fm->cls == ELFCLASS64) {
		Elf64_Ehdr elfhdr;
		if (fm->nb <= sizeof (elfhdr))
			return;

		u.l = 1;
		(void) memcpy(&elfhdr, fm->buf, sizeof elfhdr);
/*@-sizeoftype@*/
		fm->swap = (u.c[sizeof(int32_t) - 1] + 1) != elfhdr.e_ident[EI_DATA];
/*@=sizeoftype@*/

		if (getu16(fm, elfhdr.e_type) == ET_CORE) 
#ifdef ELFCORE
			dophn_core(fm,
#ifdef USE_ARRAY_FOR_64BIT_TYPES
				   getu32(fm, elfhdr.e_phoff[1]),
#else
				   getu64(fm, elfhdr.e_phoff),
#endif
				   getu16(fm, elfhdr.e_phnum), 
				   getu16(fm, elfhdr.e_phentsize));
#else
			;
#endif
		else
		{
			if (getu16(fm, elfhdr.e_type) == ET_EXEC) {
				dophn_exec(fm,
#ifdef USE_ARRAY_FOR_64BIT_TYPES
					   getu32(fm, elfhdr.e_phoff[1]),
#else
					   getu64(fm, elfhdr.e_phoff),
#endif
					   getu16(fm, elfhdr.e_phnum), 
					   getu16(fm, elfhdr.e_phentsize));
			}
			doshn(fm,
#ifdef USE_ARRAY_FOR_64BIT_TYPES
			      getu32(fm, elfhdr.e_shoff[1]),
#else
			      getu64(fm, elfhdr.e_shoff),
#endif
			      getu16(fm, elfhdr.e_shnum),
			      getu16(fm, elfhdr.e_shentsize));
		}
		return;
	}
}
/*@=bounds@*/
#endif	/* BUILTIN_ELF */

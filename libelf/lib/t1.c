#include <fcntl.h>
#include <stdio.h>
#include <libelf.h>
#include <stdlib.h>
#include <string.h>

static void
failure(void)
{
    (void) fprintf(stderr, "%s\n", elf_errmsg(elf_errno()));
     exit(1);
}

void
main(int argc, char ** argv)
{
    Elf32_Shdr *    shdr;
    Elf32_Ehdr *    ehdr;
    Elf *        elf;
    Elf_Scn *    scn;
    Elf_Data *    data;
    int        fd;
    unsigned int    cnt;

    /* Open the input file */
    if ((fd = open(argv[1], O_RDONLY)) == -1)
	exit(1);

    /* Obtain the ELF descriptor */
    (void) elf_version(EV_CURRENT);
    if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
	failure();

    /* Obtain the .shstrtab data buffer */
    if (((ehdr = elf32_getehdr(elf)) == NULL) ||
	((scn = elf_getscn(elf, ehdr->e_shstrndx)) == NULL) ||
	((data = elf_getdata(scn, NULL)) == NULL))
             failure();

    /* Traverse input filename, printing each section */
    for (cnt = 1, scn = NULL; scn = elf_nextscn(elf, scn); cnt++) {
	if ((shdr = elf32_getshdr(scn)) == NULL)
	    failure();
	(void) printf("[%d]    %s\n", cnt, (char *)data->d_buf + shdr->sh_name);
    }
} /* end main */

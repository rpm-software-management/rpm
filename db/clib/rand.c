/*
 * Copied from the ANSI C standard 4.10.2.2.
 */
#include "db_config.h"

#include "db_int.h"

/*
 * rand, srand --
 *
 * PUBLIC: #ifndef HAVE_RAND
 * PUBLIC: int rand __P((void));
 * PUBLIC: void srand __P((unsigned int));
 * PUBLIC: #endif
 */
static unsigned long int next = 1;

int rand(void)	/* RAND_MAX assumed to be 32767 */
{
	next = next * 1103515245 + 12345;
	return (unsigned int) (next/65536) % 32768;
}

void srand(unsigned int seed)
{
	next = seed;
}

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

char *
basename(const char *file)
{
	char *fn = strrchr(file, '/');
	return fn ? fn+1 : (char *)file;
}

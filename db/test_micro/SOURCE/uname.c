#include <sys/types.h>
#include <sys/utsname.h>

#include <stdio.h>

int
main()
{
	struct utsname name;

	if (uname(&name) == 0)
		printf("<p>%s, %s<br>\n%s, %s, %s</p>\n", name.nodename,
		name.machine, name.sysname, name.release, name.version);
	return (0);
}

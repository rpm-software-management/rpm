#include "system.h"
                                                                                
#include <rpmpgp.h>
#include "rpmio/base64.h"
#include <popt.h>

#include "debug.h"

const char * applechallenge = "09KF45soMYmvj6dpsUGiIg==";

#if 1
const char * rsaaeskey = "\
5QYIqmdZGTONY5SHjEJrqAhaa0W9wzDC5i6q221mdGZJ5ubO6Kg\
yhC6U83wpY87TFdPRdfPQl2kVC7+Uefmx1bXdIUo07ZcJsqMbgtje4w2JQw0b\
Uw2BlzNPmVGQOxfdpGc3LXZzNE0jI1D4conUEiW6rrzikXBhk7Y/i2naw13ayy\
xaSwtkiJ0ltBQGYGErbV2tx43QSNj7O0JIG9GrF2GZZ6/UHo4VH+ZXgQ4NZvP/\
QXPCsLutZsvusFDzIEq7TN1fveINOiwrzlN+bckEixvhXlvoQTWE2tjbmQYhMvO\
FIly5gNbZiXi0l5AdolX4jDC2vndFHqWDks/3sPikNg==\
";
#else
const char * rsaaeskey = "5QYIqmdZGTONY5SHjEJrqAhaa0W9wzDC5i6q221mdGZJ5ubO6KgyhC6U83wpY87TFdPRdfPQl2kVC7+Uefmx1bXdIUo07ZcJsqMbgtje4w2JQw0bUw2BlzNPmVGQOxfdpGc3LXZzNE0jI1D4conUEiW6rrzikXBhk7Y/i2naw13ayyxaSwtkiJ0ltBQGYGErbV2tx43QSNj7O0JIG9GrF2GZZ6/UHo4VH+ZXgQ4NZvP/QXPCsLutZsvusFDzIEq7TN1fveINOiwrzlN+bckEixvhXlvoQTWE2tjbmQYhMvOFIly5gNbZiXi0l5AdolX4jDC2vndFHqWDks/3sPikNg==";
#endif

const char * aesiv = "zcZmAZtqh7uGcEwPXk0QeA==";

const char * appleresponse = "\
u+msU8Cc7KBrVPjI/Ir8fOL8+C5D3Jsw1+acaW3MNTndrTQAeb/a\
5m10UVBX6wb/DYQGY+b28ksSwBjN0nFOk4Y2cODEf83FAh7B\
mkLpmpkpplp7zVXQ+Z9DcB6gC60ZsS3t98aoR7tSzVLKZNgi2X2sC+vGsz\
utQxX03HK008VjcdngHv3g1p2knoETd07T6eVfZCmPqp6Ga7Dj8VIIj/GEP3\
AjjDx3lJnQBXUDmxM484YXLXZjWFXCiY8GJt6whjf7/2c3rIoT3Z7PQpEvPmM\
1MXU9cv4NL59Y/q0OAVQ38foOz7eGAhfvjOsCnHU25aik7/7ToIYt1tyVtap/kA==\
";

static int doit(const char * msg, const char * sig)
{
    unsigned char * dec;
    size_t declen;
    int rc;

    if ((rc = b64decode(sig, (void **)&dec, &declen)) != 0) {
	fprintf(stderr, "*** b64decode returns %d\n", rc);
	return rc;
    }

    fprintf(stderr, "*** %p[%zd] %s\n", dec, declen, msg);
    if (declen == 256) {
	fprintf(stderr, "%s\n", pgpHexStr(dec, declen/2));
	fprintf(stderr, "%s\n", pgpHexStr(dec+declen/2, declen/2));
    } else {
	fprintf(stderr, "%s\n", pgpHexStr(dec, declen));
    }

    return 0;

}

int
main (int argc, char *argv[])
{

    doit("applechallenge", applechallenge);
    doit("rsaaeskey", rsaaeskey);
    doit("aesiv", aesiv);
    doit("appleresponse", appleresponse);
    return 0;
}

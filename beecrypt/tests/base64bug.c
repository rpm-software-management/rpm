static int _debug = 1;

#include "base64.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_TIME_H
# include <time.h>
#endif

#include <stdio.h>

/* This is the unarmored RPM-GPG-KEY public key. */
const char * gpgsig = "\
mQGiBDfqVDgRBADBKr3Bl6PO8BQ0H8sJoD6p9U7Yyl7pjtZqioviPwXP+DCWd4u8\n\
HQzcxAZ57m8ssA1LK1Fx93coJhDzM130+p5BG9mYSWShLabR3N1KXdXQYYcowTOM\n\
GxdwYRGr1Spw8QydLhjVfU1VSl4xt6bupPbWJbyjkg5Z3P7BlUOUJmrx3wCgobNV\n\
EDGaWYJcch5z5B1of/41G8kEAKii6q7Gu/vhXXnLS6m15oNnPVybyngiw/23dKjS\n\
ZVG7rKANEK2mxg1VB+vc/uUc4k49UxJJfCZg1gu1sPFV3GSa+Y/7jsiLktQvCiLP\n\
lncQt1dV+ENmHR5BdIDPWDzKBVbgWnSDnqQ6KrZ7T6AlZ74VMpjGxxkWU6vV2xsW\n\
XCLPA/9P/vtImA8CZN3jxGgtK5GGtDNJ/cMhhuv5tnfwFg4b/VGo2Jr8mhLUqoIb\n\
E6zeGAmZbUpdckDco8D5fiFmqTf5+++pCEpJLJkkzel/32N2w4qzPrcRMCiBURES\n\
PjCLd4Y5rPoU8E4kOHc/4BuHN903tiCsCPloCrWsQZ7UdxfQ5LQiUmVkIEhhdCwg\n\
SW5jIDxzZWN1cml0eUByZWRoYXQuY29tPohVBBMRAgAVBQI36lQ4AwsKAwMVAwID\n\
FgIBAheAAAoJECGRgM3bQqYOsBQAnRVtg7B25Hm11PHcpa8FpeddKiq2AJ9aO8sB\n\
XmLDmPOEFI75mpTrKYHF6rkCDQQ36lRyEAgAokgI2xJ+3bZsk8jRA8ORIX8DH05U\n\
lMH27qFYzLbT6npXwXYIOtVn0K2/iMDj+oEB1Aa2au4OnddYaLWp06v3d+XyS0t+\n\
5ab2ZfIQzdh7wCwxqRkzR+/H5TLYbMG+hvtTdylfqIX0WEfoOXMtWEGSVwyUsnM3\n\
Jy3LOi48rQQSCKtCAUdV20FoIGWhwnb/gHU1BnmES6UdQujFBE6EANqPhp0coYoI\n\
hHJ2oIO8ujQItvvNaU88j/s/izQv5e7MXOgVSjKe/WX3s2JtB/tW7utpy12wh1J+\n\
JsFdbLV/t8CozUTpJgx5mVA3RKlxjTA+On+1IEUWioB+iVfT7Ov/0kcAzwADBQf9\n\
E4SKCWRand8K0XloMYgmipxMhJNnWDMLkokvbMNTUoNpSfRoQJ9EheXDxwMpTPwK\n\
ti/PYrrL2J11P2ed0x7zm8v3gLrY0cue1iSba+8glY+p31ZPOr5ogaJw7ZARgoS8\n\
BwjyRymXQp+8Dete0TELKOL2/itDOPGHW07SsVWOR6cmX4VlRRcWB5KejaNvdrE5\n\
4XFtOd04NMgWI63uqZc4zkRa+kwEZtmbz3tHSdRCCE+Y7YVP6IUf/w6YPQFQriWY\n\
FiA6fD10eB+BlIUqIw80VgjsBKmCwvKkn4jg8kibXgj4/TzQSx77uYokw1EqQ2wk\n\
OZoaEtcubsNMquuLCMWijYhGBBgRAgAGBQI36lRyAAoJECGRgM3bQqYOhyYAnj7h\n\
VDY/FJAGqmtZpwVp9IlitW5tAJ4xQApr/jNFZCTksnI+4O1765F7tA==\n\
";

/* This is the unarmored RPM-PGP-KEY public key. */
const char * pgpsig = "\
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\n\
z15FuA==\n\
";

int
main (int argc, char *argv[])
{
    const char *sig = gpgsig;
    const char *s, *t;
    memchunk* dec;
    byte* d;
    char * enc;
    int rc;
    int len = 0;
    int i;


if (_debug)
fprintf(stderr, "*** sig is\n%s\n", sig);

    if ((dec = b64dec(sig)) == NULL) {
	fprintf(stderr, "*** b64dec failed\n");
	exit(1);
    }

    if ((enc = b64enc(dec)) == NULL) {
	fprintf(stderr, "*** b64enc failed\n");
	exit(4);
    }

if (_debug)
fprintf(stderr, "*** enc is\n%s\n", enc);

for (i = 0, s = sig, t = enc; *s & *t; i++, s++, t++) {
    if (*s == '\n') s++;
    if (*t == '\n') t++;
    if (*s == *t) continue;
fprintf(stderr, "??? %5d %02x != %02x '%c' != '%c'\n", i, (*s & 0xff), (*t & 0xff), *s, *t);
}

    return 0;
}

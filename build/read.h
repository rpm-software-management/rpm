#ifndef _READ_H_
#define _READ_H_

#include "spec.h"

#define STRIP_NOTHING       0
#define STRIP_TRAILINGSPACE 1

/* returns 0 - success */
/*         1 - EOF     */
/*        <0 - error   */

int readLine(Spec spec, int strip);
void closeSpec(Spec spec);

#endif

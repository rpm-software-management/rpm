/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * spec.h - routines for parsing are looking up info in a spec file
 */

#ifndef _SPEC_H_
#define _SPEC_H_

#include <stdio.h>

typedef struct SpecRec *Spec;

Spec parseSpec(FILE *f, char *specfile, char *prefixOverride);
void freeSpec(Spec s);

void dumpSpec(Spec s, FILE *f);

char *getSource(Spec s, int ispatch, int num);
char *getFullSource(Spec s, int ispatch, int num);

int verifySpec(Spec s);

#endif _SPEC_H_

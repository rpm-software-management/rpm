/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * spec.h - routines for parsing are looking up info in a spec file
 */

#ifndef _spec_h
#define _spec_h

typedef struct SpecRec *Spec;

Spec parseSpec(FILE *f, char *specfile);
void freeSpec(Spec s);

void dumpSpec(Spec s, FILE *f);

#define SOURCE 0
#define PATCH  1

char *getSource(Spec s, int ispatch, int num);
char *getFullSource(Spec s, int ispatch, int num);

#endif _spec_h

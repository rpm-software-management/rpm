/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * spec.h - routines for parsing are looking up info in a spec file
 */

#ifndef _spec_h
#define _spec_h

typedef struct SpecRec *Spec;

Spec parseSpec(FILE *f);
void freeSpec(Spec s);

#endif _spec_h

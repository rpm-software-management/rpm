#ifndef _TRIGGER_H_
#define _TRIGGER_H_

#include "specP.h"

int addTrigger(struct PackageRec *package,
	       int sense, char *script, char *args);

void generateTriggerEntries(Header h, struct PackageRec *p);

void freeTriggers(struct TriggerStruct t);

#endif _TRIGGER_H_

#ifndef _XML_TO_RPM_H_
#define _XML_TO_RPM_H_

#include "rpmbuild.h"

#include "xmlstruct.h"

Spec toRPMStruct(const t_structXMLSpec* pXMLSpec);

#endif

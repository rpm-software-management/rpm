#ifndef _RPMDS_INTERNAL_H
#define _RPMDS_INTERNAL_H

#include <rpm/rpmds.h>

#ifdef __cplusplus
extern "C" {
#endif

RPM_GNUC_INTERNAL
rpmsid rpmdsNIdIndex(rpmds ds, int i);

RPM_GNUC_INTERNAL
rpmsid rpmdsEVRIdIndex(rpmds ds, int i);

RPM_GNUC_INTERNAL
const char * rpmdsNIndex(rpmds ds, int i);

RPM_GNUC_INTERNAL
const char * rpmdsEVRIndex(rpmds ds, int i);

RPM_GNUC_INTERNAL
rpmsenseFlags rpmdsFlagsIndex(rpmds ds, int i);

RPM_GNUC_INTERNAL
rpm_color_t rpmdsColorIndex(rpmds ds, int i);

RPM_GNUC_INTERNAL
int rpmdsCompareIndex(rpmds A, int aix, rpmds B, int bix);

#ifdef __cplusplus
}
#endif

#endif /* _RPMDS_INTERNAL_H */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "RPM.h"

static char * const rcsid = "$Id: Transaction.xs,v 1.1 2000/08/06 08:57:09 rjray Exp $";

/*
  Use this define for deriving the saved Transaction struct, rather
  than coding it a dozen places. Note that the hv_fetch call is the
  no-magic one defined in RPM.h
*/
#define trans_from_object_ret(s_ptr, trans, object, err_ret) \
    hv_fetch_nomg((s_ptr), (object), STRUCT_KEY, STRUCT_KEY_LEN, FALSE); \
    (trans) = ((s_ptr) && SvOK(*(s_ptr))) ? (RPM_Transaction *)SvIV(*(s_ptr)) : NULL; \
    if (! (trans)) \
        return (err_ret);
/* And a no-return-value version: */
#define trans_from_object(s_ptr, trans, object) \
    hv_fetch_nomg((s_ptr), (object), STRUCT_KEY, STRUCT_KEY_LEN, FALSE); \
    (trans) = ((s_ptr) && SvOK(*(s_ptr))) ? (RPM_Transaction *)SvIV(*(s_ptr)) : NULL;


MODULE = RPM::Transaction    PACKAGE = RPM::Transaction    PREFIX = rpmtrans_

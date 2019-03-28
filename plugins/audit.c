#include "system.h"

#include <libaudit.h>

#include <rpm/rpmts.h>
#include "lib/rpmplugin.h"

struct teop {
    rpmte te;
    const char *op;
};

/*
 * Figure out the actual operations:
 * Install and remove are straightforward. Updates need to be discovered 
 * via their erasure element: locate the updating element, adjust it's
 * op to update and silence the erasure part. Obsoletion is handled as
 * as install + remove, which it technically is.
 */
static void getAuditOps(rpmts ts, struct teop *ops, int nelem)
{
    rpmtsi pi = rpmtsiInit(ts);
    rpmte p;
    int i = 0;
    while ((p = rpmtsiNext(pi, 0)) != NULL) {
	const char *op = NULL;
	if (rpmteType(p) == TR_ADDED) {
	    op = "install";
	} else {
	    op = "remove";
	    rpmte d = rpmteDependsOn(p);
	    /* Fixup op on updating elements, silence the cleanup stage */
	    if (d != NULL && rstreq(rpmteN(d), rpmteN(p))) {
		/* Linear lookup, but we're only dealing with a few thousand */
		for (int x = 0; x < i; x++) {
		    if (ops[x].te == d) {
			ops[x].op = "update";
			op = NULL;
			break;
		    }
		}
	    }
	}
	ops[i].te = p;
	ops[i].op = op;
	i++;
    }
    rpmtsiFree(pi);
}

/*
 * If enabled, log audit events for the operations in this transaction.
 * In the event values, 1 means true/success and 0 false/failure. Shockingly.
 */
static rpmRC audit_tsm_post(rpmPlugin plugin, rpmts ts, int res)
{
    if (rpmtsFlags(ts) & (RPMTRANS_FLAG_TEST|RPMTRANS_FLAG_BUILD_PROBS))
	goto exit;

    int auditFd = audit_open();
    if (auditFd < 0)
	goto exit;

    int nelem = rpmtsNElements(ts);
    struct teop *ops = xcalloc(nelem, sizeof(*ops));
    char *dir = audit_encode_nv_string("root_dir", rpmtsRootDir(ts), 0);
    int enforce = (rpmtsVfyLevel(ts) & RPMSIG_SIGNATURE_TYPE) != 0;

    getAuditOps(ts, ops, nelem);

    for (int i = 0; i < nelem; i++) {
	const char *op = ops[i].op;
	if (op) {
	    rpmte p = ops[i].te;
	    char *nevra = audit_encode_nv_string("sw", rpmteNEVRA(p), 0);
	    char *eventTxt = NULL;
	    int verified = (rpmteVerified(p) & RPMSIG_SIGNATURE_TYPE) ? 1 : 0;
	    int result = (rpmteFailed(p) == 0);

	    rasprintf(&eventTxt,
		    "op=%s %s sw_type=rpm key_enforce=%u gpg_res=%u %s",
		    op, nevra, enforce, verified, dir);
	    audit_log_user_comm_message(auditFd, AUDIT_SOFTWARE_UPDATE,
				    eventTxt, NULL, NULL, NULL, NULL, result);
	    free(nevra);
	    free(eventTxt);
	}
    }

    free(dir);
    free(ops);
    audit_close(auditFd);

exit:
    return RPMRC_OK;
}

struct rpmPluginHooks_s audit_hooks = {
    .tsm_post = audit_tsm_post,
};

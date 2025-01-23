#include "system.h"

#include <stdlib.h>

#include <dbus/dbus.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmplugin.h>

struct dbus_announce_data {
    DBusConnection * bus;
};

static rpmRC dbus_announce_init(rpmPlugin plugin, rpmts ts)
{
    struct dbus_announce_data * state = rcalloc(1, sizeof(*state));
    rpmPluginSetData(plugin, state);
    return RPMRC_OK;
}

static void dbus_announce_close_bus(struct dbus_announce_data * state)
{
    if (state->bus) {
	dbus_connection_close(state->bus);
	dbus_connection_unref(state->bus);
	state->bus = NULL;
    }
}

static rpmRC open_dbus(rpmPlugin plugin, rpmts ts)
{
    DBusError err;
    int rc = 0;
    int ignore = 0;
    struct dbus_announce_data * state = rpmPluginGetData(plugin);

    /* Already open */
    if (state->bus)
	return RPMRC_OK;

    /* ...don't notify on test transactions */
    if (rpmtsFlags(ts) & (RPMTRANS_FLAG_TEST|RPMTRANS_FLAG_BUILD_PROBS))
	return RPMRC_OK;

    /* ...don't notify on chroot transactions */
    if (!rstreq(rpmtsRootDir(ts), "/"))
	return RPMRC_OK;

    dbus_error_init(&err);

    state->bus = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err))
	goto err;

    if (state->bus)
	rc = dbus_bus_request_name(state->bus, "org.rpm.announce",
				   DBUS_NAME_FLAG_REPLACE_EXISTING , &err);

    if (dbus_error_is_set(&err))
	goto err;

    if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != rc) {
	dbus_announce_close_bus(state);
	goto err;
    }
    return RPMRC_OK;
 err:
    ignore = dbus_error_has_name(&err, DBUS_ERROR_NO_SERVER) ||
		 dbus_error_has_name(&err, DBUS_ERROR_FILE_NOT_FOUND);
    rpmlog(ignore ? RPMLOG_DEBUG : RPMLOG_WARNING,
	   "dbus_announce plugin: Error connecting to dbus (%s)\n",
	   err.message);
    dbus_error_free(&err);
    return RPMRC_OK;
}

static void dbus_announce_cleanup(rpmPlugin plugin)
{
    struct dbus_announce_data * state = rpmPluginGetData(plugin);
    dbus_announce_close_bus(state);
    free(state);
}

static rpmRC send_ts_message(rpmPlugin plugin,
			     const char * name,
			     DBusMessage* msg)
{
    struct dbus_announce_data * state = rpmPluginGetData(plugin);

    if (!state->bus)
	return RPMRC_OK;

    if (!dbus_connection_send(state->bus, msg, NULL))
	goto err;

    dbus_connection_flush(state->bus);

    return RPMRC_OK;

 err:
    rpmlog(RPMLOG_WARNING,
	   "dbus_announce plugin: Error sending message (%s)\n",
	   name);
    return RPMRC_OK;
}

static rpmRC send_ts_message_simple(rpmPlugin plugin,
				    const char * name,
				    rpmts ts,
				    int res)
{
    DBusMessage* msg = NULL;
    char * dbcookie = NULL;
    rpmRC rc = RPMRC_OK;

    msg = dbus_message_new_signal("/org/rpm/Transaction", /* object name */
				  "org.rpm.Transaction",  /* interface name */
				  name);                  /* signal name */
    if (msg != NULL) {
        dbcookie = rpmdbCookie(rpmtsGetRdb(ts));
        rpm_tid_t tid = rpmtsGetTid(ts);

        if (dbus_message_append_args(msg,
				     DBUS_TYPE_STRING, &dbcookie,
				     DBUS_TYPE_UINT32, &tid,
				     DBUS_TYPE_INVALID)) {
	    rc = send_ts_message(plugin, name, msg);
	} else {
	    rpmlog(RPMLOG_WARNING,
	           "dbus_announce plugin: Error setting message args (%s)\n",
	           name);
	}

	dbus_message_unref(msg);
    } else {
	rpmlog(RPMLOG_WARNING,
	       "dbus_announce plugin: Error creating signal message (%s)\n",
	       name);
    }

    if (dbcookie != NULL)
	dbcookie = _free(dbcookie);

    return rc;
}

static rpmRC send_ts_message_details(rpmPlugin plugin,
				     const char * name,
				     rpmts ts,
				     int res)
{
    DBusMessage* msg = NULL;
    char * dbcookie = NULL;
    rpmRC rc = RPMRC_OK;

    msg = dbus_message_new_signal("/org/rpm/Transaction", /* object name */
				  "org.rpm.Transaction",  /* interface name */
				  name);                  /* signal name */
    if (msg != NULL) {
	ARGV_t array;
	array = argvNew();
	if (array != NULL) {
	    int nElems, i;
	    nElems = rpmtsNElements(ts);
	    for (i = 0; i < nElems; i++) {
		rpmte te = rpmtsElement(ts, i);
		char *item = NULL;
		const char *op = "??";
		const char *nevra = rpmteNEVRA(te);
		if (nevra == NULL)
		    nevra = "";
		switch (rpmteType (te)) {
		case TR_ADDED:
		    op = "added";
		    break;
		case TR_REMOVED:
		    op = "removed";
		    break;
		case TR_RPMDB:
		    op = "rpmdb";
		    break;
		case TR_RESTORED:
		    op = "restored";
		    break;
		}
		/* encode as "operation SPACE nevra" */
		rasprintf(&item, "%s %s", op, nevra);
		argvAdd(&array, item);
		free(item);
	    }

            dbcookie = rpmdbCookie(rpmtsGetRdb(ts));
            rpm_tid_t tid = rpmtsGetTid(ts);

	    if (dbus_message_append_args(msg,
					 DBUS_TYPE_STRING, &dbcookie,
					 DBUS_TYPE_UINT32, &tid,
					 DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &array, argvCount(array),
					 DBUS_TYPE_INT32, &res,
					 DBUS_TYPE_INVALID)) {
		rc = send_ts_message(plugin, name, msg);
	    } else {
		rpmlog(RPMLOG_WARNING,
		       "dbus_announce plugin: Error setting message args (%s)\n",
		       name);
	    }

	    argvFree(array);
	} else {
	    rpmlog(RPMLOG_WARNING,
		   "dbus_announce plugin: Out of memory creating args array for message (%s)\n",
		   name);
        }

	dbus_message_unref(msg);
    } else {
	rpmlog(RPMLOG_WARNING,
	       "dbus_announce plugin: Error creating signal message (%s)\n",
	       name);
    }

    if (dbcookie != NULL)
	dbcookie = _free(dbcookie);

    return rc;
}

static rpmRC dbus_announce_tsm_pre(rpmPlugin plugin, rpmts ts)
{
    rpmRC rc;

    rc = open_dbus(plugin, ts);
    if (rc != RPMRC_OK)
	return rc;
    rc = send_ts_message_simple(plugin, "StartTransaction", ts, RPMRC_OK);
    if (rc == RPMRC_OK)
        rc = send_ts_message_details(plugin, "StartTransactionDetails", ts, RPMRC_OK);
    return rc;
}

static rpmRC dbus_announce_tsm_post(rpmPlugin plugin, rpmts ts, int res)
{
    rpmRC rc;

    rc = send_ts_message_simple(plugin, "EndTransaction", ts, res);
    if (rc == RPMRC_OK)
        rc = send_ts_message_details(plugin, "EndTransactionDetails", ts, res);
    return rc;
}

struct rpmPluginHooks_s dbus_announce_hooks = {
    .init = dbus_announce_init,
    .cleanup = dbus_announce_cleanup,
    .tsm_pre = dbus_announce_tsm_pre,
    .tsm_post = dbus_announce_tsm_post,
};

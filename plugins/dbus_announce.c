#include "system.h"

#include <dbus/dbus.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include "lib/rpmplugin.h"

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
    rpmlog(RPMLOG_WARNING,
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
			     rpmts ts,
			     int res)
{
    struct dbus_announce_data * state = rpmPluginGetData(plugin);
    DBusMessage* msg;
    char * dbcookie = NULL;

    if (!state->bus)
	return RPMRC_OK;

    msg = dbus_message_new_signal("/org/rpm/Transaction", /* object name */
				  "org.rpm.Transaction",  /* interface name */
				  name);                  /* signal name */
    if (msg == NULL)
	goto err;

    dbcookie = rpmdbCookie(rpmtsGetRdb(ts));
    rpm_tid_t tid = rpmtsGetTid(ts);
    if (!dbus_message_append_args(msg,
				  DBUS_TYPE_STRING, &dbcookie,
				  DBUS_TYPE_UINT32, &tid,
				  DBUS_TYPE_INVALID))
	goto err;

    if (!dbus_connection_send(state->bus, msg, NULL))
	goto err;

    dbus_connection_flush(state->bus);
    dbcookie = _free(dbcookie);

    return RPMRC_OK;

 err:
    rpmlog(RPMLOG_WARNING,
	   "dbus_announce plugin: Error sending message (%s)\n",
	   name);
    dbcookie = _free(dbcookie);
    return RPMRC_OK;
}

static rpmRC dbus_announce_tsm_pre(rpmPlugin plugin, rpmts ts)
{
    int rc;

    rc = open_dbus(plugin, ts);
    if (rc != RPMRC_OK)
	return rc;
    return send_ts_message(plugin, "StartTransaction", ts, RPMRC_OK);
}

static rpmRC dbus_announce_tsm_post(rpmPlugin plugin, rpmts ts, int res)
{
    return send_ts_message(plugin, "EndTransaction", ts, res);
}

struct rpmPluginHooks_s dbus_announce_hooks = {
    .init = dbus_announce_init,
    .cleanup = dbus_announce_cleanup,
    .tsm_pre = dbus_announce_tsm_pre,
    .tsm_post = dbus_announce_tsm_post,
};

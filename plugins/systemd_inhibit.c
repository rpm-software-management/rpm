#include <dbus/dbus.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>
#include "lib/rpmplugin.h"

static int lock_fd = -1;

static int inhibit(void)
{
    DBusError err;
    DBusConnection *bus = NULL;
    DBusMessage *msg = NULL;
    DBusMessage *reply = NULL;
    int fd = -1;

    dbus_error_init(&err);
    bus = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);

    if (bus) {
	msg = dbus_message_new_method_call("org.freedesktop.login1",
					   "/org/freedesktop/login1",
					   "org.freedesktop.login1.Manager",
					   "Inhibit");
    }

    if (msg) {
	const char *what = "idle:sleep:shutdown";
	const char *mode = "block";
	const char *who = "RPM";
	const char *reason = "Transaction running";

	dbus_message_append_args(msg,
				 DBUS_TYPE_STRING, &what,
				 DBUS_TYPE_STRING, &who,
				 DBUS_TYPE_STRING, &reason,
				 DBUS_TYPE_STRING, &mode,
				 DBUS_TYPE_INVALID);

	reply = dbus_connection_send_with_reply_and_block(bus, msg, -1, &err);
	dbus_message_unref(msg);
    }

    if (reply) {
	dbus_message_get_args(reply, &err,
			      DBUS_TYPE_UNIX_FD, &fd,
			      DBUS_TYPE_INVALID);
	dbus_message_unref(reply);
    }
    
    if (dbus_error_is_set(&err))
	dbus_error_free(&err);
    if (bus)
	dbus_connection_close(bus);

    return fd;
}

static rpmRC systemd_inhibit_init(rpmPlugin plugin, rpmts ts)
{
    struct stat st;

    if (lstat("/run/systemd/system/", &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return RPMRC_OK;
        }
    }

    return RPMRC_NOTFOUND;
}

static rpmRC systemd_inhibit_tsm_pre(rpmPlugin plugin, rpmts ts)
{
    if (rpmtsFlags(ts) & (RPMTRANS_FLAG_TEST|RPMTRANS_FLAG_BUILD_PROBS))
	return RPMRC_OK;

    lock_fd = inhibit();

    if (lock_fd < 0) {
	rpmlog(RPMLOG_WARNING,
	       "Unable to get systemd shutdown inhibition lock\n");
    } else {
	rpmlog(RPMLOG_DEBUG, "System shutdown blocked (fd %d)\n", lock_fd);
    }

    return RPMRC_OK;
}

static rpmRC systemd_inhibit_tsm_post(rpmPlugin plugin, rpmts ts, int res)
{
    if (lock_fd >= 0) {
	close(lock_fd);
	lock_fd = -1;
	rpmlog(RPMLOG_DEBUG, "System shutdown unblocked\n");
    }
    return RPMRC_OK;
}

struct rpmPluginHooks_s systemd_inhibit_hooks = {
    .init = systemd_inhibit_init,
    .tsm_pre = systemd_inhibit_tsm_pre,
    .tsm_post = systemd_inhibit_tsm_post,
};

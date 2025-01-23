RPM-PLUGIN-DBUS-ANNOUNCE(8)

# NAME

rpm-plugin-dbus-announce - DBus plugin for the RPM Package Manager

# DESCRIPTION

The plugin writes basic information about rpm transactions to the system
dbus - like packages installed or removed. Other programs can subscribe
to the signals to be notified of the packages on the system change.

# DBUS SIGNALS

Sends *StartTransaction*, *EndTransaction*, *StartTransactionDetails*
and *EndTransactionDetails* messages from the */org/rpm/Transaction*
object with the *org.rpm.Transaction* interface.

The first two signals pass the DB cookie as a string and the transaction id
as an unsigned 32 bit integer.

The second two signals, those "details", also pass the DB cookie as a string
and the transaction id as an unsigned 32 bit integer and then in addition
an array of strings containing space-separated string of values describing
the operation and the package NEVRA which are part of the transaction.
The last argument is a signed integer with the transaction result.

The package operation can be one of "added", "removed", "rpmdb" or "restored"
(quotes for clarity only).

# CONFIGURATION

There are currently no options for this plugin in particular. See
*rpm-plugins*(8) on how to control plugins in general.

# SEE ALSO

*dbus-monitor*(1), *rpm-plugins*(8)

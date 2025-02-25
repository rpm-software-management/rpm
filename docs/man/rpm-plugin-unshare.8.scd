RPM-PLUGIN-UNSHARE(8)

# NAME

rpm-plugin-unshare - Unshare plugin for the RPM Package Manager

# DESCRIPTION

This plugin allows using various Linux-specific namespace-related
technologies inside transactions, such as to harden and limit
scriptlet access to resources.

# CONFIGURATION

This plugin implements the following configurables:

%\_\_transaction_unshare_paths

	A colon-separated list of paths to privately mount during scriptlet
	execution. Typical examples would be `/tmp` to protect against
	insecure temporary file usage inside scriptlets, and `/home` to
	prevent scriptlets from accessing user home directories.
	When path unsharing is enabled, any mounts made from scriptlets
	are also private to the scriptlet (and vice versa, mount changes
	on the host are not visible to the scriptlet).

	Private mounts in chroot-operations is unimplemented.

%\_\_transaction_unshare_nonet

	Non-zero value disables network access during scriptlet execution.

See *rpm-plugins*(8) on how to control plugins in general.

# SEE ALSO

*dbus-monitor*(1), *rpm-plugins*(8)

RPM-PLUGIN-SYSLOG(8)

# NAME

rpm-plugin-syslog - Syslog plugin for the RPM Package Manager

# DESCRIPTION

The plugin writes basic information about *rpm*(8) transactions to the syslog -
like transactions run and packages installed or removed.

# CONFIGURATION

*%\_\_transaction\_syslog\_target*
	Specify target output for the plugin. Supported values are:
	- *syslog*: system log (default)
	- *stderr*: standard error

See *rpm-plugins*(8) on how to control plugins in general.

# OUTPUT

# SEE ALSO

*rpm*(8), *rpm-plugins*(8)

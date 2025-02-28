RPM-PLUGIN-FAPOLICYD(8)

# NAME

rpm-plugin-fapolicyd - Fapolicyd plugin for the RPM Package Manager

# DESCRIPTION

The plugin gathers metadata of currently installed files. It sends the
information about files and about ongoing rpm transaction to the
fapolicyd daemon. The information is written to Linux pipe which is
placed in _/var/run/fapolicyd/fapolicyd.fifo_.

# CONFIGURATION

There are currently no options for this plugin in particular. See
*rpm-plugins*(8) on how to control plugins in general.

# SEE ALSO

*fapolicyd*(8), *rpm-plugins*(8)

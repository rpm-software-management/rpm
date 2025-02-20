RPM-PLUGIN-SELINUX(8)

# NAME

rpm-plugin-selinux - SELinux plugin for the RPM Package Manager

# DESCRIPTION

The plugin sets SELinux contexts for installed files and executed
scriptlets. It needs SELinux to be enabled to work but will work in both
enforcing and permissive mode.

# CONFIGURATION

The plugin can be disabled temporarily by passing *--nocontexts* at
the *rpm*(8) command line or setting the transaction flag
*RPMTRANS_FLAG_NOCONTEXTS* in the API.

See *rpm-plugins*(8) on how to control plugins in general.

# SEE ALSO

*rpm*(8), *rpm-plugins*(8)

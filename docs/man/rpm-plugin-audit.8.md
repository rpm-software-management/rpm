RPM-PLUGIN-AUDIT(8)

# NAME

rpm-plugin-audit - Audit plugin for the RPM Package Manager

# DESCRIPTION

The plugin writes basic information about rpm transactions to the audit
log - like packages installed or removed. The entries can be viewed with

*ausearch -m SOFTWARE_UPDATE*

## Data fields

The entries in the audit log have the following fields:

[[ *Field*
:< *Description*
:- *Possible values*
|  *op*
:  Package operation
:  install/update/remove
|  *sw*
:  Package identifier
:  name-version-release.arch
|  *key_enforce*
:  Are signatures being enforced (no/yes)
:  0/1
|  *gpg_res*
:  Signature check result (fail/success)
:  0/1
|  *root_dir*
:  Root directory of the operation (normally "/")
:  <path>
|  *sw_type*
:  Package format
:  rpm

# CONFIGURATION

There are currently no options for this plugin in particular. See
*rpm-plugins*(8) on how to control plugins in general.

# SEE ALSO

*ausearch*(8), *rpm-plugins*(8)

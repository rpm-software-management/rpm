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

The plugin uses *rpm* as its syslog identity.

The package level output during transactions is as follows:

_OPERATION_ _NEVRA_ [*(from:* _RELNEVRA_*)*]*:* _STATUS_

_OPERATION_ is one of:
- *cleanup*: cleanup of the previous version in *upgrade* or *downgrade*
- *downgrade*: downgrade an installed package to an older version
- *erase*: erase package from the system
- *install*: install a new package on the system
- *replace*: replace (obsolete) a previously installed package with another one
- *restore*: restore permissions and similar on an installed package
- *upgrade*: upgrade an installed package to a newer version

_NEVRA_ identifies the transaction element being operated on.
Note that operations *downgrade*, *replace* and *upgrade* consist of two
elements: install of one package version and removal of another.
In these cases, the related element identifier _RELNEVRA_ is indicated
in parentheses after *from:*.  _STATUS_ is one of *success* or *failure*.

# EXAMPLES

*journalctl -t rpm*
	Inspect RPM transaction history on a systemd-based OS.

*Package aaa upgrade from 1.2-1 to 2.0-1*
```
upgrade: aaa-2.0-1.noarch (from: aaa-1.2-1.noarch): success
cleanup: aaa-1.2-1.noarch (from: aaa-2.0-1.noarch): success
```

*Package bbb 1.3-5 obsoleting aaa 1.2-1*
```
replace: bbb-1.3-5.noarch (from: aaa-1.2-1.noarch): success
erase: aaa-1.2-1.noarch (from: bbb-1.3-5.noarch): success
```

*Package aaa 1.2-1 install failure*
```
install: aaa-1.2-1.noarch: failure
```

# SEE ALSO

*rpm*(8), *rpm-plugins*(8), *rpm-version*(7)
*journalctl*(1) *syslog*(3)

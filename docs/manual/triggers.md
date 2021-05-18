---
layout: default
title: rpm.org - Trigger scriptlets
---
# Trigger scriptlets

Triggers provide a well-defined method for packages to interact with one
another at package install and uninstall time. They are an extension
of the normal installation scripts (i.e. %pre) which allows one package
(the "source" of the trigger package [which I often think of as the 
"triggered package"]) to execute an action when the installation status
of another package (the "target" of the trigger) changes.

## A Simple Example

Say the package "mymailer" needs an /etc/mymailer/mailer symlink which points
to the mail transport agent to use. If sendmail is installed, the link should
point to /usr/bin/sendmail, but if vmail is installed, the link should
instead point to /usr/bin/vmail. If both packages are present, we don't care
where the link points (realistically, sendmail and vmail should conflict
with one another), while if neither package is installed the link should
not exist at all.

This can be accomplished by mymailer providing trigger scripts which 
move the symlink when any of the following occurs:

```
	1) sendmail is installed
	2) vmail is installed
	3) sendmail is removed
	4) vmail is removed
```

The first two of these scripts would look like this:

```
	%triggerin -- sendmail
	ln -sf /usr/bin/sendmail /etc/mymailer/mailer

	%triggerin -- vmail
	ln -sf /usr/bin/vmail /etc/mymailer/mailer
```

These are two installation triggers, triggered by one of sendmail or vmail.
They are run when:

```
	1) mymailer is already installed, and sendmail is installed or
	   upgraded
	2) mymailer is already installed, and vmail is installed or
	   upgraded
	3) sendmail is already installed, and mymailer is installed or
	   upgraded
	4) vmail is already installed, and mymailer is installed or
	   upgraded
```

For the upgrading, the strategy is a little different. Rather then
setting the link to point to the trigger, the link is set to point to
the *other* mailer (if it exists), as follows:

```
	%triggerun -- sendmail
	[ $2 = 0 ] || exit 0
	if [ -f /usr/bin/vmail ]; then
		ln -sf /usr/bin/vmail /etc/mymailer/mailer
	else
		rm -f /etc/mymailer/mailer

	fi

	%triggerun -- vmail
	[ $2 = 0 ] || exit 0
	if [ -f /usr/bin/sendmail ]; then
		ln -sf /usr/bin/sendmail /etc/mymailer/mailer
	else
		rm -f /etc/mymailer/mailer

	fi

	%postun
	[ $1 = 0 ] && rm -f /etc/mymailer/mailer
```

These trigger scripts get run when:

```
	1) sendmail is installed, and mymailer is removed
	2) vmail is installed, and mymailer is removed
	3) mymailer is installed, and sendmail gets removed
	4) mymailer is installed, and vmail gets removed
```

The %postun insures that /etc/mymailer/mailer is removed when mymailer
is removed (triggers get run at the same time as %preun scripts, so 
doing this in the %postun is safe). Note that the triggers are testing
$2 to see if any action should occur. Recall that the $1 passed to regular
scripts contains the number of instances of the package which will be 
installed when the operation has completed. $1 for triggers is exactly
the same -- it is the number of instances of the source (or triggered)
package which will remain when the trigger has completed. Similarly, $2
is the number of instances of the target package which will remain. In
this case, if any of the targets will remain after the uninstall, the
trigger doesn't do anything (as it's probably being triggered by an
upgrade).

## Trigger Syntax

Trigger specifications are of the form:

```
	%trigger{un|in|postun} [[-n] <subpackage>] [-p <program>] -- <trigger>
```

The -n and -p arguments are the same as for %post scripts.  The
\<trigger\> portion is syntactically equivalent to a "Requires"
specification (version numbers may be used). If multiple items are
given (comma separated), the trigger is run when *any* of those
conditions becomes true (the , can be read as "or"). For example:

```
	%triggerin -n package -p /usr/bin/perl -- fileutils > 3.0, perl < 1.2
	print "I'm in my trigger!\n";
```

Will put a trigger in package 'package' which runs when the installation
status of either fileutils > 3.0 or perl < 1.2 is changed. The script will
be run through /usr/bin/perl rather then /bin/sh (which is the default).

## An Unusual Case

There is one other type of trigger available -- %triggerpostun. These are
triggers that are run after their target package has been removed; they will
never be run when the package containing the trigger is removed. 

While this type of trigger is almost never useful, they allow a package to
fix errors introduced by the %postun of another package (or by an earlier 
version of that package).

## Order of Script Execution

For reference, here's the order in which scripts are executed on a single
package upgrade:

```
  all-%pretrans
  ...
  any-%triggerprein (%triggerprein from other packages set off by new install)
  new-%triggerprein
  new-%pre	for new version of package being installed
  ...		(all new files are installed)
  new-%post	for new version of package being installed

  any-%triggerin (%triggerin from other packages set off by new install)
  new-%triggerin
  old-%triggerun
  any-%triggerun (%triggerun from other packages set off by old uninstall)

  old-%preun	for old version of package being removed
  ...		(all old files are removed)
  old-%postun	for old version of package being removed

  old-%triggerpostun
  any-%triggerpostun (%triggerpostun from other packages set off by old un
		install)
  ...
  all-%posttrans
```

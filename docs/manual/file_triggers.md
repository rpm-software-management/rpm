---
layout: default
title: rpm.org - File triggers
---

## File triggers

File triggers are kind of rpm scriptlets. They could be defined in spec file of a package. Syntax of file trigger is following:

```
%file_trigger_tag [FILE_TRIGGER_OPTIONS] -- PATHPREFIX...
body_of_script
```

file_trigger_tag defines type of filetrigger. Allowed types are filetriggerin, filetriggerun, filetriggerpostun, transfiletriggerin, transfiletriggerun, transfiletriggerpostun. FILE_TRIGGER_OPTIONS are the same as options for other rpm scriptlets except for option -P. The priority of trigger is defined with number, the bigger number the sooner a file trigger script will be executed. Triggers with priority greater than 100000 will be executed before standard scriptlets and the other triggers will be executed after standard scriptlets. If you don't specify priority, the default priority is 1000000. Every file trigger of each type must contain one or more path prefixes and script. So example of file trigger can be:

```
%filetriggerin -- /usr/lib /lib
/usr/sbin/ldconfig
```

This file trigger will execute `/usr/bin/ldconfig` right after installation of a package that contains a file having a path starting with `/usr/lib` or `/lib`. The file trigger will be executed just once for one package no matter how many files in package starts with `/usr/lib` or `/lib`. But all file names starting with `/usr/lib` or `/lib` will be passed to standard input of trigger script so that you can do some filtering inside of your script:

```
%filetriggerin -- /usr/lib
grep "foo" && /usr/sbin/ldconfig
```

This file trigger will execute `/usr/bin/ldconfig` for each package containing files starting with `/usr/lib` and containing "foo" at the same time. Note that
the prefix-matched files includes all types of files - regular files,
directories, symlinks etc.

The file triggers are defined in spec files of packages. E.g. file trigger executing `ldconfig` could be defined in glibc package.

As was mentioned above there are more types of file triggers. We have two main types. File triggers execute once for package and file triggers executed once for whole transaction a.k.a transaction file triggers. Further file triggers are dived according to time of execution: before/after installation or erasure of a package or before/after a transaction.

Here is a list of all possible types:


### Excuted once per package ###

   %filetriggerin: Executed after installation of a package if that package contained file(s) that matches prefix of this trigger. Also executed after installation of a package that contains this file trigger and there is/are some file(s) matching prefix of this file trigger in rpmdb. 

   %filetriggerun: Executed before uninstallation of a package if that package contained file(s) that matches prefix of this trigger. Also executed before uninstallation of a package that contains this file trigger and there is/are some file(s) matching prefix of this file trigger in rpmdb.

   %filetriggerpostun: Executed after uninstallation of a package if that package contained file(s) that matches prefix of this trigger. 


### Excuted once per transaction

   %transfiletriggerin: Executed once after transaction for all installed packages that contained file(s) that matches prefix of this trigger. Also executed after transaction if there was a package containing this file trigger in that transaction and there is/are some files(s) matching prefix of this trigger in rpmdb.

   %transfiletriggerun: Executed once before transaction for all packages that will be uninstalled in this transaction and that contains file(s) that matches prefix of this trigger. Also execute before transaction if there is a package containing this file trigger in that transaction and there is/are some files(s) matching prefix of this trigger in rpmdb.

   %transfiletriggerpostun: Executed once after transaction for all uninstalled packages that contained file(s) that matches prefix of this trigger. Note: the list of triggering files is not available in this trigger type.


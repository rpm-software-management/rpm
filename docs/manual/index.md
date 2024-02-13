---
layout: default
title: rpm.org - RPM Reference Manual
---
# RPM Reference Manual

## Package Management

### Queries and RPM Meta Data
* [RPM Tags](tags.md)
* [Large File support](large_files.md)
* [Query formatting](queryformat.md)

## Macro subsystem
* [Macro syntax](macros.md)
* [Embedded Lua](lua.md)

## Package Building
* [Build Process](buildprocess.md)
* [Spec Syntax](spec.md)
  * [Declarative builds](buildsystem.md)
  * [Autosetup](autosetup.md)
  * Dependencies
    * [Dependencies Basics](dependencies.md)
    * [More on Dependencies](more_dependencies.md)
    * [Boolean Dependencies](boolean_dependencies.md)
    * [Architecture Dependencies](arch_dependencies.md)
    * [Installation Order](tsort.md)
    * [Automatic Dependency Generation](dependency_generators.md)
  * Install scriptlets
    * [Triggers](triggers.md)
    * [File Triggers](file_triggers.md)
    * [Scriptlet Expansion](scriptlet_expansion.md)
  * [Users and Groups](users_and_groups.md)
  * [Conditional Builds](conditionalbuilds.md)
  * [Relocatable Packages](relocatable.md)
* [Dynamic Spec Generation](dynamic_specs.md)

## Developer Information

### API
* [Plugin API](plugins.md)

### Package Format
* [RPM v6 file format (DRAFT)](format_v6.md)
* [RPM v4 file format](format_v4.md)
* [RPM v4 signatures and digests](signatures_digests.md)
* [RPM v3 file format](format_v3.md) (obsolete)

### Documentation
* [Write documentation](devel_documentation.md)

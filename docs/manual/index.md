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

### Signatures
* [Signatures](signatures.md)

## Macro subsystem
* [Macro syntax](macros.md)
* [Embedded Lua](lua.md)

## Package Building
* [Build Process](buildprocess.md)
* [Spec Syntax](spec.md)
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
  * [Conditional Builds](conditionalbuilds.md)
  * [Relocatable Packages](relocatable.md)
  * [Buildroot](buildroot.md) and [Multiple build areas](multiplebuilds.md)


## Developer Information

### API
* [Plugin API](plugins.md)

### Package Format
* [RPM v3 file format](format.md)
* [RPM v4 header regions](hregions.md)
* [RPM v4 signatures and digests](signatures_digests.md)


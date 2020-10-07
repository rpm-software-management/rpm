# RPM Reference Manual

## Package Management

### Transactions

### Verification

### Queries
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
  * [Dependencies](dependencies.md)
    * [More on Dependencies](more_dependencies.md)
    * [Boolean Dependencies](boolean_dependencies.md)
    * [Build Dependencies](builddependencies.md)
    * [Architecture Dependencies](arch_dependencies.md)
    * [Installation Order](tsort.md)
  * Install scriptlets
    * [Triggers](triggers.md)
    * [File Triggers](file_triggers.md)
    * [Scriptlet Expansion](scriptlet_expansion.md)
  * [Conditional Builds](conditionalbuilds.md)
  * [Relocatable Packages](relocatable.md)
  * [Buildroot](buildroot.md) and [Multiple build areas](multiplebuilds.md)
* [Dependency Generation](dependency_generators.md)


## Developer Information

### API
* [Plugin API](plugins.md)

### Package Format
* [RPM Tags](tags.md)
* [Large File support](large_files.md)
* [RPM v3 file format](format.md)
* [RPM v4 header regions](hregions.md)
* [RPM v4 signatures and digests](signatures_digests.md)


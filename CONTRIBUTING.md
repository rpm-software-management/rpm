# Submitting Patches

Patches should be submitted via GitHub pull requests (PRs).  Each PR should be a
single atomic change, meaning that it should be merged or rejected as a unit.
It is okay for a PR to contain multiple commits if later commits are dependent
on earlier ones.  If earlier commits do not depend on later ones, they
(should/should not) be submitted separately.

# Code Style

RPM uses 8-space tabs.  Indentation is 4 spaces, with each group of 8 or more
leading spaces replaced by a tab.

In VIM, this can be achieved with

```vim
set shiftwidth=4 tabstop=8 softtabstop=4
```

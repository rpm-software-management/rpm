GENDIFF(1)

# NAME
gendiff - Utility to aid in error-free diff file generation

# SYNOPSIS
*gendiff* _DIRECTORY_ _DIFF-EXTENSION_

# DESCRIPTION
*gendiff* is a tool for generating a _diff_ file from a single directory.
The generated _diff_ can be applied with the *patch*(1) program to recreate
the changes.

*gendiff* looks for any files with _DIFF-EXTENSION_ as their extension in the
specified _DIRECTORY_, and runs *diff*(1) on the files with _DIFF-EXTENSION_ and
the counterpart without it, in that order. This allows for generating diffs from
source code without making a full copy of the directory, which you'd otherwise
need when using the *diff*(1) utility directly.

*gendiff* is commonly used to creating patches for the purposes of
packaging software with *rpmbuild*(1).

# ARGUMENTS
_DIRECTORY_
	The directory to generate a diff from.

_DIFF-EXTENSION_
	The diff extension to look for.

# EXIT STATUS
On success, 0 is returned, a nonzero failure code otherwise.

# EXAMPLES
Assuming you need to modify a source file called _somefile.cpp_ and
have chosen the extension _fix_, copy it to _somefile.cpp.fix_ before
editing it. Then edit the original file (_somefile.cpp_).

After editing all the files you need to edit in this fashion, enter the
directory one level above where the source code resides, and run
*gendiff* on it:

```
gendiff myproject-1.0 .fix > myproject-1.0-fix.patch
```

# SEE ALSO
*diff*(1), *patch*(1)

RPM-MACROFILE(5)

# NAME
*rpm-macrofile* - rpm macro file format

# SYNOPSIS
%_NAME_[([_OPTS_]) _BODY_

# DESCRIPTION
Rpm macro files are used to define *rpm-macros*(7) in the global macro context.
The two primary uses of macros are assisting packaging work, and configuring rpm
behavior. A pre-determined set of macro files is read upon *rpm* library
initialization as described in *rpm-config*(5) but they can also be loaded via
the *%load* macro separately.

The format of a macro file is: the macro _NAME_ prefixed with *%*, followed
by whitespace and then the macro body, each new definition separated with
a newline. The syntax is exactly the same as when using *%define*.
See *rpm-macros*(7) for details on the macro syntax.

A trailing *\\* indicates line continuation, but can be omitted inside
macro bodies wrapped in a *%{macro:...}* block.

Lines starting with *\#* or consisting solely of whitespace
are ignored.

Reading a macro file is always fully declarative: no macros are ever
expanded when reading a macro file.

# EXAMPLES
## Example 1. Simple macro
```
%mytool	/usr/bin/mytool-with-annoying-name
```

## Example 2. Multiline macro with a preceding comment
```
# Trailing %{nil} is handy for ensuring a newline at the end
%mycmd %{mytool} \\
    --one \\
    --with \\
    --too \\
    --many arguments \\
%{nil}
```

## Example 3. Multiline parametric Lua macro:
```
%myhelper() %{lua:
    function dostuff(arg)
        return 'do stuff to '..arg
    end
    print(dostuff(arg[1]))
}
```

# SEE ALSO
*rpm-config*(5) *rpmbuild-config*(5)
*rpm-macros*(7)

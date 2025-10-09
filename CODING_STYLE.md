# RPM CODING STYLE

## Indentation

RPM code uses 8-space tabs.  Indentation is 4 spaces, with each group of
8 or more leading spaces replaced by a tab. If in doubt, 'indent -kr'
generally produces acceptable formatting.

In Vim, this can be achieved with

```vim
set shiftwidth=4 tabstop=8 softtabstop=4
```

In Emacs use
```
(setq-default c-basic-offset 4
              indent-tabs-mode t)

```

The Markdown documentation uses 4-space indents.

An `.editorconfig` file is provided at the root of the repository with
these settings defined. Any supporting editor should pick them up
automatically. Editorconfig support is available (built-in or via
the appropriate plugin/extension) for nearly all text editors.

## Comments

Comments should be used sparingly and for subtle and
non-obvious things only.  One-liners are prefered for in-line comments,
unless there is something really special or complicated. Longer
comments can go into the doc strings of functions, but background
information should generally go into the commit message instead.

Always prefer self-explanatory code over comments! Many times well-named
helper variables can make even complicated code read almost like plain
English.

Comments are always in `/* ... */`, never `//`. The two comment styles
in rpm codebase are:

`/* Short and sweet */`

```
/*
 * Multi-line comments should be formatted
 * like this.
 */
```

## Error handling

Avoid multiple exit points from functions, these are an invitation to
resource leaks, cause code duplication and require touching unrelated
code when adding new features.

As a ballpark rule: If you need to free resource in more than one place,
think again, and if you need to free it in more than two places you're
doing it wrong.

For local variables, always initialize pointers on entry and free them
on central exit point, using `goto exit` or `goto err` style jumps to get
there from errors. Assuming failure and only setting to success just
before the exit label often works well and is widely used in the codebase.

RPM uses custom allocator functions in place of standard library malloc()
and friends. These never return NULL (they abort the process on failure),
so do not check for it. Also all "destructor" type functions in rpm accept
NULL arguments, don't check for it separately.

On the C++ side, don't catch `std::bad_alloc`, it should abort the process
just like the C allocators do. Other specific exceptions such as those
coming from the filesystem should be caught to avoid leaking them into
C code which cannot handle it - or use the non-throwing versions to begin
with.

## Miscellaneous

While many details differ and lot of it does not apply at all, the
[Linux kernel coding style document](https://www.kernel.org/doc/html/latest/process/coding-style.html)
contains lots of excellent guidance on good C programming practices if you
filter out what is kernel specific.

## API/ABI considerations

Most of RPM functionality resides in public libraries, whose API and ABI
need to be considered on in all changes. There are multiple levels of
visibility in RPM:

1. Public API, which is described by our public C headers. If a symbol
   is not in a public header, it's not considered public regardless of
   its visibility.
2. Internal cross-library API, which is described by various non-public C
   headers but whose symbols are visible in our public ABI.
3. Internal per-library APIs, which are described by various non-public
   C headers and whose symbols are hidden from the ABI on platforms
   where supported (marked by `RPM_GNUC_INTERNAL`)
4. Statically scoped symbols in sources.

Internals are generally free to change, but public API must be carefully
preserved. Adding new public APIs is generally not a problem, removing
and changing existing ones can only be done in major version bumps and
even there should be done conservatively. People are annoyed when their
code stops compiling. ABI compatibility in RPM is tracked with
[libtool versioning](https://www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html)

Newly added symbols in the public ABI must prefixed with `rpm` except to
maintain consistency where another pre-existing prefix (eg `header`) is
used for historical reasons.

All public APIs must be documented using Doxygen annotation.

## Portability

RPM aims to be portable within POSIX compliant systems, the exact version
requirements are documented in INSTALL. However, the primary platform
of RPM is Linux, and that is the only platform supported by the RPM main
development team. Patches to support other OS'es within the POSIX version
requirements are generally accepted though.

## Commit Messages
Changes must be adequately explained in commit messages. This
includes not only what is changed, but also why. The commit message needs to
be self-contained. While references to GitHub tickets or external bug
trackers are welcome the important information needs to be in
the commit message itself.

The rationale behind the change is often *more* important than the
accompanying code! When changing existing code, it's equally important
to understand why the code is the way it is now. Always check for
history when suggesting change of behavior.


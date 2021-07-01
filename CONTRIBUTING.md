# Contributing to RPM

RPM has a history that goes back over two decades and is used by many
different distributions - some providing support for a decade or
more. There are hundreds of thousands RPM packages in use out
there. This makes changes to RPM difficult and they often require thorough
consideration on the implications and on compatibility.

So while we invite contributions from all sides - especially from the
multitude of RPM based distributions - RPM by its very nature is not an
easy project to get into.

## Submitting Patches

Patches should be submitted via GitHub pull requests (PRs) or via `git
send-mail` to the rpm-maint@lists.rpm.org mailing list.  Each PR should be a
single atomic change, meaning that it should be merged or rejected as a unit.
It is okay for a PR to contain multiple commits if later commits are dependent
on earlier ones.  If earlier commits do not depend on later ones, they
(should/should not) be submitted separately.

Pull requests should clearly state if the code is "ready for
inclusion" or if further work is needed. Proof of concept or request
for comment pull requests are fine but need to be labled as such.

## Comments and Commit Messages

Information about the change should go into the commit message. This
includes not only what is changed but also why. The commit message needs to
be self contained. While references to GitHub tickets or external bug
trackers are welcome the important information needs to be (also) in
the commit message itself.
For many changes a justification why the change is needed should also
be given in the commit message.

Comments on the other hand should be used sparingly and for subtle and
non-obvious things only. One-liners are prefered for in-line comments,
unless there is something really special or complicated. Longer
comments can go into the doc strings of functions. But back ground
information should go into the commit message instead.

## Code Style

RPM uses 8-space tabs.  Indentation is 4 spaces, with each group of 8 or more
leading spaces replaced by a tab. If in doubt, 'indent -kr' generally
produces acceptable formatting.

In VIM, this can be achieved with

```vim
set shiftwidth=4 tabstop=8 softtabstop=4
```

In emacs use
```
(setq-default c-basic-offset 4
              indent-tabs-mode t)

```

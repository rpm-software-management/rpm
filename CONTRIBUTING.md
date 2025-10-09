# Contributing to RPM

RPM has a history that goes back over two decades and is used by many
different distributions - some providing support for a decade or
more. There are hundreds of thousands RPM packages in use out
there. This makes changes to RPM difficult and they often require thorough
consideration on the implications and on compatibility.

So while we invite contributions from all sides - especially from the
multitude of RPM based distributions - RPM by its very nature is not an
easy project to get into.

To be included in RPM, contributions need to
* solve a generic problem, not just for one user's special use-case
* needs to have a future - the project does not add support for legacy
  technologies
* align with [RPM's philosophy](https://rpm-software-management.github.io/rpm/manual/philosophy.html) and design
* be implemented following the [RPM coding style](CODING_STYLE.md), include
  documentation and tests and submitted according to the guidelines in
  [submitting patches](#Submitting-Patches)

**When planning on any non-trivial amount of work on RPM, please check with us
first to avoid situations where efforts go wasted!**

## Translations

Translations should be submitted through [Fedora Weblate](https://translate.fedoraproject.org/projects/rpm/),
the upstream project cannot review translations.

## AI Policy

As a policy, the RPM project does not accept use of generative AI
in contributions.

## Submitting Patches

Patches should be submitted via GitHub pull requests (PRs) or via `git
send-email` to the rpm-maint@lists.rpm.org mailing list. Use your real
name as the commit author (git `user.name` config option) in a format
suitable for the CREDITS file.
Patches from pseudonyms or anonymous submitters will not be accepted.

Each PR should be a single atomic change, meaning that it should be merged
or rejected as a unit. It is okay for a PR to contain multiple commits if
later commits are dependent on earlier ones.  If earlier commits do not
depend on later ones, they (should/should not) be submitted separately.

All new features and bugfixes must include test(s). In the rare cases
where testing is not possible due to technical limitations of the test-suite,
this must be explicitly stated and the technical limitation explained.

All new features must be documented via man pages, using the style
and conventions documented in docs/man/rpm-man-template.scd.

Pull requests should clearly state if the code is "ready for
inclusion" or if further work is needed. Proof of concept or request
for comment pull requests are fine but need to be labeled as such.

When using GitHub pull requests enable "Allow edits from maintainers"
to allow changes to your branch. Often changes are needed and allowing
them in the branch in your repository keeps the discussion and changes
within one place.

Avoid updating the branch of the pull request if not necessary. On
every change notifications are sent out to all maintainers and
everyone contributing to the pull request. Keep your changes local or
in another branch and only push to the branch of the pull request when
you are ready for further feedback.

This is a project with limited resources, please do not flood us.
Please ensure "make ci" passes locally before submitting a PR.
A pull request with failing CI tests will generally not be looked at,
so keep on eye on the checks until you get all green.


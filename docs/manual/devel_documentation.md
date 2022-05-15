---
layout: default
title: rpm.org - Writing Documentation
---
# Writing Documentation

There is a lot of 3rd party documentation on RPM and RPM
packaging. We are collecting it on the [Documentation
page](https://rpm.org/documentation.html) on rpm.org. Many of those
are unfortunately not quite up-to-date.

We are currently working on an in-tree Reference Manual. While still
being less complete than some 3rd party documents we do update it for
all relevant changes and new features.

## Reference Manual

The [Reference
Manual](https://rpm-software-management.github.io/rpm/manual/) is
supposed to cover all user facing aspects of RPM but has still a long
way to go. Additions are welcome. Proposals can also be made by
posting the content in a ticket without bothering about markup and the
details of the rendering.

The markdown sources of the Reference Manual are found in
`docs/manual`. They are not built into html or any other format by the
Makefile. Instead the `master` branch is turned in to GitHub
Pages. By sharing the header with the [rpm.org](https://rpm.org) web
page that links to both it looks like they're are one web site.

For working on the Reference Manual it is useful to render the page
locally. To do that one needs to reproduce the relevant infrastructure
of GitHub Pages - mainly the [Jekyll](https://jekyllrb.com/) static
website generator. This can be installed from a distribution
repository (named such as `rubygem-jekyll` in Fedora) or via the Ruby
mechanisms e.g. with `gem install jekyll`.

GitHub Pages use a number of Jekyll plugins and do more things in the
background that are not that relevant for our use case. The plugins can
be found in `docs/_config.yml`. For convenience most are commented
out. The remaining ones need also to be installed (currently
`jekyll-titles-from-headings` and `jekyll-relative-links`). The former
is not strictly necessary as we do set titles explicitly. The latter
makes link between the pages work and though navigation much more
convenient.

Both can also be installed with `gem install
jekyll-titles-from-headings jekyll-relative-links`.

To render the Reference Manual, `jekyll build` can be executed in the
`docs` dir. It will create a `docs/_site` directory with the rendered
web page - including HTML versions of the man pages. Alternatively
`jekyll serve` will open a web server on localhost.

## Man Pages

The sources of the man pages are found in `docs/man/` in markdown
format and are converted to troff format as used by the `man` tool by
the Makefile in this directory (or building the whole of RPM). They
are also rendered to HTML together with the Reference Manual.

## Rpm.org

The [rpm.org](https://rpm.org) web page is maintained in a separate
git project:
[rpm-web](https://github.com/rpm-software-management/rpm-web). This
allows storing data like release tar balls that are not acceptable in
the rpm repository. It is rendered as GitHub Pages and made available
under the [rpm.org](https://rpm.org) domain.

Rendering it locally can be done similarly to Reference Manual (see
above) by executing `jekyll` in the top directory of the repository.

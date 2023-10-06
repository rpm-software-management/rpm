RPM Documentation
=================

There are multiple pieces of documentation that are processed and used differently.

RPM.org
-------

The [rpm.org web page](https://rpm.org/) is created from its own [GitHub repository](https://github.com/rpm-software-management/rpm-web).

Man Pages
---------

The man pages in *man/* are Markdown documents. They are rendered to the man page format during build. These pre-rendered pages are also shipped with the (release) tarball. That way building rpm from the tarball does not require the *pandoc* as long as the man pages are unchanged.

The man pages are also rendered to HTML together with the Reference Manual (see below).

API Reference
-------------

The API reference is rendered with *Doxygen*. The content is almost exclusively created from the RPM sources and the doc strings there in. The rendered HTML is shipped with the release tarball to be able to build RPM from that without *Doxygen*.

Reference Manual
----------------

The reference manual in the *manual/* dir is an GitHub Pages site. It is rendered automatically when pushed into the *master* branch in the GitHub repository. It is available online on [its own URL](https://rpm-software-management.github.io/rpm/manual/) but is intended to be used as part of the [RPM.org website](https://rpm.org/) were it is linked from the [Documentation page](https://rpm.org/documentation.html)

The Reference Manual is currently not shipped in rendered form in the tarball.

To render it locally one can follow [this article](https://docs.github.com/en/pages/setting-up-a-github-pages-site-with-jekyll/testing-your-github-pages-site-locally-with-jekyll) and render it with *Jekyll* with the use of the *github-pages* gem.

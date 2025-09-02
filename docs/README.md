RPM Documentation
=================

There are multiple pieces of documentation that are processed and used differently.

RPM.org
-------

The [rpm.org web page](https://rpm.org/) is created from its own [GitHub repository](https://github.com/rpm-software-management/rpm-web).

Man Pages
---------

The man pages in *man/* are scdoc sources. 
They are rendered to the man page format during build.

Consult *man/rpm-man-template.scd* for style guidance, and use it
as a template for new man pages.

The man pages are also rendered to HTML together with the Reference Manual (see below).

API Reference
-------------

The API reference is rendered with *Doxygen*. The content is almost exclusively created from the RPM sources and the doc strings there in. The rendered HTML is shipped with the release tarball to be able to build RPM from that without *Doxygen*.

Reference Manual
----------------

The reference manual in the *manual/* dir is an GitHub Pages site. It is rendered automatically when pushed into the *master* branch in the GitHub repository. It is available online on [its own URL](https://rpm-software-management.github.io/rpm/manual/) but is intended to be used as part of the [RPM.org website](https://rpm.org/) were it is linked from the [Documentation page](https://rpm.org/documentation.html)

The Reference Manual is currently not shipped in rendered form in the tarball.

To render the site locally, make sure you have Podman installed and that your
build is configured as follows:

    cmake -DWITH_WEBSITE=ON [...]

Then, from the build directory, run:

    make site

This builds the site in a container and then serves it locally at
`http://0.0.0.0:4000` for preview.  You can further tweak this behavior with
the following CMake options:

- `JEKYLL_SERVE`: Set to `OFF` to just build the site, not serve it.  Default:
  `ON`.
- `JEKYLL_SOURCE_DIR`: The source path to build, useful for building the
  [rpm-web](https://github.com/rpm-software-management/rpm-web) site locally
  (e.g. in a dedicated CMake build directory).  Default:
  `${CMAKE_BINARY_DIR}/site`.

If you wish to render the site manually, prepare the source pages by running
`make pages` and then follow the steps in [this
article](https://docs.github.com/en/pages/setting-up-a-github-pages-site-with-jekyll/testing-your-github-pages-site-locally-with-jekyll)
(the source directory to build is located at `${CMAKE_BINARY_DIR}/site`).

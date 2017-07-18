dnl ##
dnl ##  acinclude.m4 -- manually provided local Autoconf macros
dnl ##

dnl ##
dnl ##  NAME:
dnl ##    AC_MSG_TITLE -- Display a configuration title
dnl ##
dnl ##  USAGE:
dnl ##    AC_MSG_TITLE(<name>, <version>)
dnl ##

AC_DEFUN([AC_MSG_TITLE],[
    _AS_ECHO([Configuring $1, Version $2])
])

dnl ##
dnl ##  NAME:
dnl ##    AC_MSG_HEADER -- Display a configuration header
dnl ##
dnl ##  USAGE:
dnl ##    AC_MSG_HEADER(<text>)
dnl ##

AC_DEFUN([AC_MSG_HEADER],[
    _AS_ECHO([])
    _AS_ECHO([=== $1 ===])
])

dnl ##
dnl ##  NAME:
dnl ##    AC_MSG_VERBOSE -- Display a message under --verbose
dnl ##
dnl ##  USAGE:
dnl ##    AC_MSG_VERBOSE(<text>)
dnl ##

AC_DEFUN([AC_MSG_VERBOSE], [
    if test ".$verbose" = .yes; then
        _AS_ECHO([$1])
    fi
])

dnl ##
dnl ##  NAME:
dnl ##    RPM_CHECK_LIB -- Check for third-party libraries
dnl ##
dnl ##  COPYRIGHT
dnl ##    Copyright (c) 2007 Ralf S. Engelschall <rse@engelschall.com>
dnl ##
dnl ##  DESCRIPTION:
dnl ##    This is a rather complex Autoconf macro for sophisticated
dnl ##    checking the availability of third-party libraries and
dnl ##    extending the build environment for correctly building
dnl ##    against it.
dnl ##
dnl ##    It especially supports the following particular features:
dnl ##    - is aware of old-style [lib]<libname>-config style scripts
dnl ##    - is aware of new-style pkg-config(1) [lib]<libname>.pc configuration files
dnl ##    - searches under standard sub-directories "include", "lib", etc.
dnl ##    - searches under arbitrary sub-areas of a tree like ".libs", etc.
dnl ##    - searches in standard system locations (implicitly)
dnl ##    - supports searching for function in multiple libraries
dnl ##    - supports searching for multiple headers
dnl ##    - supports multiple search locations (fallbacks!)
dnl ##
dnl ##  USAGE:
dnl ##  - configure.in:
dnl ##    RPM_CHECK_LIB(
dnl ##        <lib-real-name>,        -- [$1] e.g. GNU bzip2
dnl ##        <lib-tag-name>,         -- [$2] e.g. bzip2
dnl ##        <lib-link-name>,        -- [$3] e.g. bz2
dnl ##        <lib-function-name>,    -- [$4] e.g. BZ2_bzlibVersion
dnl ##        <lib-header-filename>,  -- [$5] e.g. bzlib.h
dnl ##        <with-arg-default>[,    -- [$6] e.g. yes,external:internal:none
dnl ##        <internal-subdir>[,     -- [$7] e.g. lib/bzip2:include:src
dnl ##        <action-success>[,      -- [$8] e.g. AC_DEFINE(USE_BZIP2, 1, [...])
dnl ##        <action-failure>        -- [$9] e.g. AC_MSG_ERROR([...])
dnl ##    ]]])
dnl ##
dnl ##  - Makefile.in:
dnl ##    top_srcdir                 = @top_srcdir@
dnl ##    srcdir                     = @srcdir@
dnl ##    WITH_<LIB-TAG-NAME>        = @WITH_<LIB-TAG-NAME>@
dnl ##    WITH_<LIB-TAG-NAME>_SUBDIR = @WITH_<LIB-TAG-NAME>_SUBDIR@
dnl ##    CPPFLAGS                   = @CPPFLAGS@
dnl ##    CFLAGS                     = @CFLAGS@
dnl ##    LDFLAGS                    = @LDFLAGS@
dnl ##    LIBS                       = @LIBS@
dnl ##
dnl ##  - CLI:
dnl ##    $ ./configure \
dnl ##      --with-<lib-tag-name>[=<with-arg>]
dnl ##      [...]
dnl ##
dnl ##  SYNTAX:
dnl ##    <with-arg>           ::= <with-arg-mode> |   <with-arg-location>
dnl ##    <with-arg-default>   ::= <with-arg-mode> "," <with-arg-location>
dnl ##    <with-arg-mode>      ::= "yes" | "no"
dnl ##    <with-arg-location>  ::= <with-arg-location> ":" <with-arg-location>
dnl ##                           | <directory-path>
dnl ##                           | "system"
dnl ##                           | "external"
dnl ##                           | "internal"
dnl ##                           | "none"
dnl ##    <directory-path>     ::= [...] /* valid arg for test(1) option "-d" */
dnl ##
dnl ##

dnl # public API macro
AC_DEFUN([RPM_CHECK_LIB], [
    dnl ##
    dnl ## PROLOG
    dnl ##

    dnl # parse <with-arg-default> into default enable mode and default locations
    m4_define([__rcl_default_enable], [m4_substr([$6], 0, m4_index([$6], [,]))])
    m4_define([__rcl_default_locations], [m4_substr([$6], m4_eval(m4_index([$6], [,]) + 1))])

    dnl # provide defaults
    if test ".${with_$2+set}" != .set; then
        dnl # initialize to default enable mode
        with_$2="__rcl_default_enable"
        AC_MSG_VERBOSE([++ assuming --with-$2=$with_$2])
    fi
    if test ".${with_$2}" = .yes; then
        dnl # map simple "--with-foo=yes" to an enabled default location path
        with_$2="__rcl_default_locations"
        AC_MSG_VERBOSE([++ mapping --with-$2=yes to --with-$2="$with_$2"])
    fi

    dnl ##
    dnl ## HANDLING
    dnl ##

    __rcl_result_hint=""
    __rcl_location_$2=""
    __rcl_location_last=""
    m4_if([$7],,, [
        WITH_[]m4_translit([$2],[a-z],[A-Z])[]_SUBDIR=""
        WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS=""
        WITH_[]m4_translit([$2],[a-z],[A-Z])[]_LDFLAGS=""
        WITH_[]m4_translit([$2],[a-z],[A-Z])[]_LIBS=""
    ])
    AC_ARG_WITH($2,
        AS_HELP_STRING([--with-$2=ARG], [build with $1 library (__rcl_default_enable) (location path: "__rcl_default_locations")]), [dnl
        if test ".${with_$2}" != .no; then
            dnl # iterate over location path specification for searching purposes
            __rcl_IFS="${IFS}"; IFS=":"
            for __rcl_location in ${with_$2}; do
                IFS="${__rcl_IFS}"
                __rcl_location_last="${__rcl_location}"
                AC_MSG_VERBOSE([++ searching location: ${__rcl_location}])
                if test ".${__rcl_location}" = .none; then
                    dnl # no operation in loop, ignore failure later, too.
                    AC_MSG_VERBOSE([-- no operation])
                m4_if([$7],,, [ elif test ".${__rcl_location}" = .internal; then
                    dnl # optional support for <internal-subdir> feature
                    m4_define([__rcl_subdir],
                              [m4_if(m4_index([$7], [:]), -1, [$7],
                                     m4_substr([$7], 0, m4_index([$7], [:])))])
                    if test -d ${srcdir}/__rcl_subdir; then
                        AC_MSG_VERBOSE([-- activating local sub-directory: __rcl_subdir])
                        if test -f ${srcdir}/__rcl_subdir/configure; then
                            AC_CONFIG_SUBDIRS(__rcl_subdir)
                        fi
                        dnl # NOTICE: an internal copy of the third-party library is a tricky thing
                        dnl # because for the following two major reasons we cannot just extend
                        dnl # CPPFLAGS, LDFLAGS and LIBS in this case:
                        dnl # 1. at _this_ "configure" time at least the library file (libfoo.a)
                        dnl #    is still not existing, so extending LIBS with "-lfoo" would cause
                        dnl #    following Autoconf checks to fail.
                        dnl # 2. even deferring the extension of LIBS doesn't work, because although
                        dnl #    this works around the problem under (1), it will fail if more than
                        dnl #    one internal third-party library exists: LIBS would contains "-lfoo
                        dnl #    -lbar" and if build "foo", "bar" usually still isn't built (or vice
                        dnl #    versa). Hence, the linking of programs (tools, tests, etc) in "foo"
                        dnl #    would fail.
                        dnl # 3. information in at least LDFLAGS and LIBS is usually passed-through
                        dnl #    to applications via xxx-config(1) scripts or pkg-config(1)
                        dnl #    specifications. As the path to the internal copy is usually just a
                        dnl #    temporary path, this will break there, too.
                        dnl # So, internal copies of third-party libraries inherently have to be
                        dnl # handled explicitly by the build environment and for this we can only
                        dnl # provide the necessary information in dedicated per-library variables.
                        WITH_]m4_translit([$2],[a-z],[A-Z])[_SUBDIR="__rcl_subdir"
                        __rcl_location_$2=internal
                        AC_MSG_VERBOSE([++ post-adjustments for --with-$2 (${__rcl_location_$2})])
                        __rcl_dirs_inc=`echo '$7' | sed -e 's/^[[^:]]*://' -e 's/:[[^:]]*[$]//'`
                        __rcl_dirs_lib=`echo '$7' | sed -e 's/^[[^:]]*:[[^:]]*://'`
                        __rcl_srcdir="\[$](top_srcdir)/\[$](WITH_[]m4_translit([$2],[a-z],[A-Z])[]_SUBDIR)"
                        __rcl_builddir="\[$](top_builddir)/\[$](WITH_[]m4_translit([$2],[a-z],[A-Z])[]_SUBDIR)"
                        __rcl_firstlib="m4_if(m4_index([$3], [ ]), -1, [$3], m4_substr([$3], 0, m4_index([$3], [ ])))"
                        if test ".${__rcl_dirs_inc}" != ".$7"; then
                            __rcl_IFS="${IFS}"; IFS=","
                            for __rcl_dir in ${__rcl_dirs_inc}; do
                                IFS="${__rcl_IFS}"
                                test ".${__rcl_dir}" = . && continue
                                AC_MSG_VERBOSE([-- extending WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS: -I${__rcl_srcdir}/${__rcl_dir}])
                                WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS="${WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS} -I${__rcl_srcdir}/${__rcl_dir}"
                                AC_MSG_VERBOSE([-- extending WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS: -I${__rcl_builddir}/${__rcl_dir}])
                                WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS="${WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS} -I${__rcl_builddir}/${__rcl_dir}"
                            done
                            IFS="${__rcl_IFS}"
                        fi
                        AC_MSG_VERBOSE([-- extending WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS: -I${__rcl_srcdir}])
                        WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS="${WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS} -I${__rcl_srcdir}"
                        AC_MSG_VERBOSE([-- extending WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS: -I${__rcl_builddir}])
                        WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS="${WITH_[]m4_translit([$2],[a-z],[A-Z])[]_CPPFLAGS} -I${__rcl_builddir}"
                        if test ".${__rcl_dirs_lib}" != ".$7"; then
                            __rcl_IFS="${IFS}"; IFS=","
                            for __rcl_dir in ${__rcl_dirs_lib}; do
                                IFS="${__rcl_IFS}"
                                test ".${__rcl_dir}" = . && continue
                                AC_MSG_VERBOSE([-- extending WITH_[]m4_translit([$2],[a-z],[A-Z])[]_LDFLAGS: -L${__rcl_builddir}/${__rcl_dir}])
                                WITH_[]m4_translit([$2],[a-z],[A-Z])[]_LDFLAGS="${WITH_[]m4_translit([$2],[a-z],[A-Z])[]_LDFLAGS} -L${__rcl_builddir}/${__rcl_dir}"
                            done
                            IFS="${__rcl_IFS}"
                        fi
                        AC_MSG_VERBOSE([-- extending WITH_[]m4_translit([$2],[a-z],[A-Z])[]_LDFLAGS: -L${__rcl_builddir}])
                        WITH_[]m4_translit([$2],[a-z],[A-Z])[]_LDFLAGS="${WITH_[]m4_translit([$2],[a-z],[A-Z])[]_LDFLAGS} -L${__rcl_builddir}"
                        AC_MSG_VERBOSE([-- extending WITH_[]m4_translit([$2],[a-z],[A-Z])[]_LIBS: -l${__rcl_firstlib}])
                        WITH_[]m4_translit([$2],[a-z],[A-Z])[]_LIBS="${WITH_[]m4_translit([$2],[a-z],[A-Z])[]_LIBS} -l${__rcl_firstlib}"
                        __rcl_result_hint="internal: sub-directory __rcl_subdir"
                        break
                    else
                        AC_MSG_VERBOSE([-- skipping not existing local sub-directory: __rcl_subdir])
                    fi
                ])
                elif test ".${__rcl_location}" = .external; then
                    dnl # detection of library in arbitrary external location
                    if (pkg-config --exists $2 || pkg-config --exists lib$2) 2>/dev/null; then
                        dnl # via pkg-config(1) script in PATH
                        m4_define([__rcl_query_pkgconfig], [
                            __rcl_flags=""
                            for __rcl_query in $][2; do
                                if test ".$__rcl_flags" != .; then
                                    __rcl_flags="${__rcl_flags} `($][1 $__rcl_query) 2>/dev/null`"
                                else
                                    __rcl_flags="`($][1 $__rcl_query) 2>/dev/null`"
                                fi
                            done
                            if test ".${__rcl_flags}" != .; then
                                AC_MSG_VERBOSE(-- extending $][3: ${__rcl_flags})
                                $][3="${$][3} ${__rcl_flags}"
                            fi
                        ])
                        if (pkg-config --exists $2) 2>/dev/null; then
                            __rcl_query_pkgconfig([pkg-config $2], [--cflags-only-other], [CFLAGS])
                            __rcl_query_pkgconfig([pkg-config $2], [--cflags-only-I], [CPPFLAGS])
                            __rcl_query_pkgconfig([pkg-config $2], [--libs-only-L --libs-only-other], [LDFLAGS])
                            __rcl_query_pkgconfig([pkg-config $2], [--libs-only-l], [LIBS])
                            __rcl_result_hint="external: via pkg-config $2"
                        else
                            __rcl_query_pkgconfig([pkg-config lib$2], [--cflags-only-other], [CFLAGS])
                            __rcl_query_pkgconfig([pkg-config lib$2], [--cflags-only-I], [CPPFLAGS])
                            __rcl_query_pkgconfig([pkg-config lib$2], [--libs-only-L --libs-only-other], [LDFLAGS])
                            __rcl_query_pkgconfig([pkg-config lib$2], [--libs-only-l], [LIBS])
                            __rcl_result_hint="external: via pkg-config lib$2"
                        fi
                        break
                    elif test ".`($2-config --version; lib$2-config --version) 2>/dev/null`" != .; then
                        dnl # via config script in PATH
                        m4_define([__rcl_query_config], [
                            __rcl_flags="`($][1 $][2) 2>/dev/null`"
                            if test ".${__rcl_flags}" != .; then
                                AC_MSG_VERBOSE(-- extending $][3: ${__rcl_flags})
                                $][3="${$][3} ${__rcl_flags}"
                            fi
                        ])
                        if test ".`($2-config --version) 2>/dev/null`" != .; then
                            __rcl_query_config([$2-config], [--cppflags], [CPPFLAGS])
                            __rcl_query_config([$2-config], [--cflags],   [CFLAGS])
                            __rcl_query_config([$2-config], [--ldflags],  [LDFLAGS])
                            __rcl_query_config([$2-config], [--libs],     [LIBS])
                            __rcl_result_hint="external: via $2-config"
                        else
                            __rcl_query_config([lib$2-config], [--cppflags], [CPPFLAGS])
                            __rcl_query_config([lib$2-config], [--cflags],   [CFLAGS])
                            __rcl_query_config([lib$2-config], [--ldflags],  [LDFLAGS])
                            __rcl_query_config([lib$2-config], [--libs],     [LIBS])
                            __rcl_result_hint="external: via lib$2-config"
                        fi
                        break
                    elif test ".${__rcl_found}" = .no; then
                        dnl # via implicit flags attribution of previous checks or
                        dnl # in standard system locations (usually /usr/include and /usr/lib)
                        __rcl_found_hdr=no
                        __rcl_found_lib=no
                        AC_PREPROC_IFELSE([AC_LANG_SOURCE([@%:@include <$5>])], [ __rcl_found_hdr=yes ])
                        m4_foreach_w([__rcl_lib], [$3], [
                            __rcl_safe_LIBS="${LIBS}"
                            LIBS="-l[]m4_defn([__rcl_lib]) ${LIBS}"
                            AC_LINK_IFELSE([AC_LANG_CALL([], [$4])], [ __rcl_found_lib=yes ])
                            LIBS="${__rcl_safe_LIBS}"
                        ])
                        if test ".${__rcl_found_hdr}:${__rcl_found_lib}" = ".yes:yes"; then
                            __rcl_result_hint="external: implicit or default location"
                            break
                        fi
                    fi
                elif test ".${__rcl_location}" = .system; then
                    dnl # detection of library in external locations controlled
                    dnl # by explicit build flags or in standard system locations
                    dnl # (usually /usr/include and /usr/lib)
                    __rcl_found_hdr=no
                    __rcl_found_lib=no
                    AC_PREPROC_IFELSE([AC_LANG_SOURCE([@%:@include <$5>])], [ __rcl_found_hdr=yes ])
                    m4_foreach_w([__rcl_lib], [$3], [
                        __rcl_safe_LIBS="${LIBS}"
                        LIBS="-l[]m4_defn([__rcl_lib]) ${LIBS}"
                        AC_LINK_IFELSE([AC_LANG_CALL([], [$4])], [ __rcl_found_lib=yes ])
                        LIBS="${__rcl_safe_LIBS}"
                    ])
                    if test ".${__rcl_found_hdr}:${__rcl_found_lib}" = ".yes:yes"; then
                        __rcl_result_hint="system: explicitly controlled or system location"
                        break
                    fi
                elif test -d "${__rcl_location}"; then
                    dnl # detection of library in particular external location
                    __rcl_found=no
                    dnl # via config script
                    for __rcl_dir in ${__rcl_location}/bin ${__rcl_location}; do
                        if test -f "${__rcl_dir}/$2-config" && test ! -f "${__rcl_dir}/$2-config.in"; then
                            if test ".`(${__rcl_dir}/$2-config --version) 2>/dev/null`" != .; then
                                __rcl_query_config([${__rcl_dir}/$2-config], [--cppflags], [CPPFLAGS])
                                __rcl_query_config([${__rcl_dir}/$2-config], [--cflags],   [CFLAGS])
                                __rcl_query_config([${__rcl_dir}/$2-config], [--ldflags],  [LDFLAGS])
                                __rcl_query_config([${__rcl_dir}/$2-config], [--libs],     [LIBS])
                                __rcl_result_hint="external: via ${__rcl_dir}/$2-config"
                                __rcl_found=yes
                                break
                            fi
                        elif test -f "${__rcl_dir}/lib$2-config" && test ! -f "${__rcl_dir}/lib$2-config.in"; then
                            if test ".`(${__rcl_dir}/lib$2-config --version) 2>/dev/null`" != .; then
                                __rcl_query_config([${__rcl_dir}/lib$2-config], [--cppflags], [CPPFLAGS])
                                __rcl_query_config([${__rcl_dir}/lib$2-config], [--cflags],   [CFLAGS])
                                __rcl_query_config([${__rcl_dir}/lib$2-config], [--ldflags],  [LDFLAGS])
                                __rcl_query_config([${__rcl_dir}/lib$2-config], [--libs],     [LIBS])
                                __rcl_result_hint="external: via ${__rcl_dir}/lib$2-config"
                                __rcl_found=yes
                                break
                            fi
                        fi
                    done
                    dnl # via pkg-config(1) script
                    if test ".${__rcl_found}" = .no; then
                        for __rcl_dir in ${__rcl_location}/bin ${__rcl_location}; do
                            if test -f "${__rcl_dir}/pkg-config"; then
                                if (${__rcl_dir}/pkg-config --exists $2) 2>/dev/null; then
                                    __rcl_query_pkgconfig([${__rcl_dir}/pkg-config $2], [--cflags-only-other], [CFLAGS])
                                    __rcl_query_pkgconfig([${__rcl_dir}/pkg-config $2], [--cflags-only-I], [CPPFLAGS])
                                    __rcl_query_pkgconfig([${__rcl_dir}/pkg-config $2], [--libs-only-L --libs-only-other], [LDFLAGS])
                                    __rcl_query_pkgconfig([${__rcl_dir}/pkg-config $2], [--libs-only-l], [LIBS])
                                    __rcl_result_hint="external: via ${__rcl_dir}/pkg-config $2"
                                    __rcl_found=yes
                                    break
                                elif (${__rcl_dir}/pkg-config --exists lib$2) 2>/dev/null; then
                                    __rcl_query_pkgconfig([${__rcl_dir}/pkg-config lib$2], [--cflags-only-other], [CFLAGS])
                                    __rcl_query_pkgconfig([${__rcl_dir}/pkg-config lib$2], [--cflags-only-I], [CPPFLAGS])
                                    __rcl_query_pkgconfig([${__rcl_dir}/pkg-config lib$2], [--libs-only-L --libs-only-other], [LDFLAGS])
                                    __rcl_query_pkgconfig([${__rcl_dir}/pkg-config lib$2], [--libs-only-l], [LIBS])
                                    __rcl_result_hint="external: via ${__rcl_dir}/pkg-config lib$2"
                                    __rcl_found=yes
                                    break
                                fi
                            fi
                        done
                    fi
                    dnl # in standard sub-areas
                    if test ".${__rcl_found}" = .no; then
                        for __rcl_dir in ${__rcl_location}/include/$2 ${__rcl_location}/include ${__rcl_location}; do
                            if test -f "${__rcl_dir}/$5"; then
                                if test ".${__rcl_dir}" != "./usr/include"; then
                                    AC_MSG_VERBOSE([-- extending CPPFLAGS: -I${__rcl_dir}])
                                    CPPFLAGS="${CPPFLAGS} -I${__rcl_dir}"
                                fi
                                __rcl_found=yes
                                break
                            fi
                        done
                        if test ".${__rcl_found}" = .yes; then
                            __rcl_found=no
                            for __rcl_dir in ${__rcl_location}/lib/$2 ${__rcl_location}/lib ${__rcl_location}; do
                                m4_foreach_w([__rcl_lib], [$3], [
                                    if  test -f "${__rcl_dir}/lib[]m4_defn([__rcl_lib]).la" && \
                                        test -d "${__rcl_dir}/.libs"; then
                                        if test ".${__rcl_dir}" != "./usr/lib"; then
                                            AC_MSG_VERBOSE([-- extending LDFLAGS: -L${__rcl_dir}])
                                            AC_MSG_VERBOSE([-- extending LDFLAGS: -L${__rcl_dir}/.libs])
                                            LDFLAGS="${LDFLAGS} -L${__rcl_dir} -L${__rcl_dir}/.libs"
                                        fi
                                        __rcl_found=yes
                                        break
                                    fi
                                    if  test -f "${__rcl_dir}/lib[]m4_defn([__rcl_lib]).a"  || \
                                        test -f "${__rcl_dir}/lib[]m4_defn([__rcl_lib]).so" || \
                                        test -f "${__rcl_dir}/lib[]m4_defn([__rcl_lib]).sl" || \
                                        test -f "${__rcl_dir}/lib[]m4_defn([__rcl_lib]).dylib"; then
                                        if test ".${__rcl_dir}" != "./usr/lib"; then
                                            AC_MSG_VERBOSE([-- extending LDFLAGS: -L${__rcl_dir}])
                                            LDFLAGS="${LDFLAGS} -L${__rcl_dir}"
                                        fi
                                        __rcl_found=yes
                                        break
                                    fi
                                ])
                            done
                        fi
                        if test ".${__rcl_found}" = .yes; then
                            __rcl_result_hint="external: directory ${__rcl_location}"
                        fi
                    fi
                    dnl # in any sub-area
                    if test ".${__rcl_found}" = .no; then
                        for __rcl_file in _ `find ${__rcl_location} -name "$5" -type f -print 2>/dev/null`; do
                            test .${__rcl_file} = ._ && continue
                            __rcl_dir=`echo ${__rcl_file} | sed -e 's;[[^/]]*[$];;' -e 's;\(.\)/[$];\1;'`
                            if test ".${__rcl_dir}" != "./usr/include"; then
                                AC_MSG_VERBOSE([-- extending CPPFLAGS: -I${__rcl_dir}])
                                CPPFLAGS="${CPPFLAGS} -I${__rcl_dir}"
                            fi
                            __rcl_found=yes
                            break
                        done
                        if test ".${__rcl_found}" = .yes; then
                            __rcl_found=no
                            m4_foreach_w([__rcl_lib], [$3], [
                                for __rcl_file in _ `find ${__rcl_location} -name "lib[]m4_defn([__rcl_lib]).*" -type f -print 2>/dev/null | \
                                    egrep '\.(a|so|sl|dylib)$'`; do
                                    test .${__rcl_file} = ._ && continue
                                    __rcl_dir=`echo ${__rcl_file} | sed -e 's;[[^/]]*[$];;' -e 's;\(.\)/[$];\1;'`
                                    if test ".${__rcl_dir}" != "./usr/lib"; then
                                        AC_MSG_VERBOSE([-- extending LDFLAGS: -L${__rcl_dir}])
                                        LDFLAGS="${LDFLAGS} -L${__rcl_dir}"
                                    fi
                                    __rcl_found=yes
                                    break
                                done
                            ])
                        fi
                        if test ".${__rcl_found}" = .yes; then
                            __rcl_result_hint="external: tree ${__rcl_location}"
                        fi
                    fi
                    if test ".${__rcl_found}" = .yes; then
                        break
                    fi
                else
                    AC_MSG_ERROR([Unknown location specification $__rcl_location])
                fi
            done
            IFS="${__rcl_IFS}"

            dnl # check for actual availability of library
            m4_if([$7],,, [ if test ".${__rcl_location_last}" = .internal; then
                dnl # special case: still not existing "internal" library
                dnl # cannot be checked (and usually has not to be checked anyway)
                with_$2=yes
                if test ".${__rcl_location_$2}" != .internal; then
                    AC_MSG_ERROR([unable to find internal $1 library])
                fi
            else ])
                dnl # regular case: use standard Autoconf facilities
                dnl # and try to make sure both header and library is found
                __rcl_found=yes
                dnl # check for C header (in set of optionally multiple possibilities)
                AC_CHECK_HEADERS([$5], [], [ __rcl_found=no ])
                dnl # check for C library (in set of optionally multiple possibilities)
                dnl # stop on first found library
                __rcl_found_lib=no
                while true; do
                  m4_foreach_w([__rcl_lib], [$3], [
                    AC_CHECK_LIB(m4_defn([__rcl_lib]), [$4])
                    dnl # manually check for success (do not use third argument to AC_CHECK_LIB
                    dnl # here as this would no longer set the LIBS variable (the default action)
                    test ".${m4_translit(ac_cv_lib_[]m4_defn([__rcl_lib])_$4,[.-,],[___])}" = .yes && __rcl_found_lib=yes && break
                  ])
                  break
                done
                test ".${__rcl_found_lib}" = .no && __rcl_found="no"
                dnl # determine final results
                with_$2=${__rcl_found}
            m4_if([$7],,, [ fi ])
            if test ".${with_$2}" = .yes && test ".${__rcl_result_hint}" = .; then
                dnl # was not found explicitly via searching above, but
                dnl # implicitly in standard location or via extended
                dnl # flags from previous searches
                __rcl_result_hint="external: implicitly"
            fi
        fi
    ])

    dnl ##
    dnl ## EPILOG
    dnl ##

    dnl # provide results
    if test ".${with_$2}" = .yes; then
        AC_DEFINE([WITH_]m4_translit([$2],[a-z],[A-Z]), 1, [Define as 1 if building with $1 library])
    fi
    [WITH_]m4_translit([$2],[a-z],[A-Z])="${with_$2}"
    AC_SUBST([WITH_]m4_translit([$2],[a-z],[A-Z]))
    m4_if([$7],,, [
        AC_SUBST([WITH_]m4_translit([$2],[a-z],[A-Z])[_SUBDIR])
        AC_SUBST([WITH_]m4_translit([$2],[a-z],[A-Z])[_CPPFLAGS])
        AC_SUBST([WITH_]m4_translit([$2],[a-z],[A-Z])[_LDFLAGS])
        AC_SUBST([WITH_]m4_translit([$2],[a-z],[A-Z])[_LIBS])
    ])

    dnl # report results
    AC_MSG_CHECKING(whether to build with $1 library)
    __rcl_msg="${with_$2}"
    if test ".${with_$2}" = .yes && test ".${__rcl_result_hint}" != .; then
        __rcl_msg="${__rcl_msg} (${__rcl_result_hint})"
    fi
    AC_MSG_RESULT([${__rcl_msg}])

    dnl # perform actions
    RPM_CHECK_LIB_LOCATION="${__rcl_location_last}"
    if test ".${with_$2}" = .yes; then
        AC_DEFINE([WITH_]m4_translit([$2],[a-z],[A-Z]), 1, [Define as 1 if building with $1 library])
        dnl # support optional <action-success>
        AC_MSG_VERBOSE([++ executing success action])
        m4_if([$8],, :, [$8])
    else
        dnl # support optional <action-failure>
        AC_MSG_VERBOSE([++ executing failure action])
        m4_if([$9],, [
            if  test ".${RPM_CHECK_LIB_LOCATION}" != . && \
                test ".${RPM_CHECK_LIB_LOCATION}" != .none; then
                AC_MSG_ERROR([unable to find usable $1 library])
            fi
        ], [$9])
    fi
    ${as_unset} RPM_CHECK_LIB_LOCATION
])

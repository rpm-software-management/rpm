# SSL macro from fetchmail
# (C) Eric S. Raymond <esr@thyrsus.com>

AC_DEFUN([NEON_SSL],[

###	use option --with-ssl to compile in the SSL support
AC_ARG_WITH(ssl,
	[  --with-ssl=[DIR]        enable SSL support using libraries in DIR],
	[with_ssl=$withval],
	[with_ssl=no])
test "$with_ssl" = "yes" && AC_DEFINE(SSL_ENABLE)

if test "$with_ssl" = "yes"
then
#	He didn't specify an SSL location.  Let's look at some common
#	directories where SSL has been found in the past and try and auto
#	configure for SSL.  OpenSSL determination will be made later.
#	This will screw up if an OpenSSL install is located in a later
#	directory than an older SSLeay install, but the user should fix that
#	anyways and he can override on the configure line.

    for ac_dir in \
      /usr/local/ssl \
      /usr/ssl \
      /local/ssl \
      /opt/ssl \
      ; \
    do
        if test -d "$ac_dir" ; then
            with_ssl=$ac_dir
            break;
        fi
    done
fi

if test -n "$with_ssl" -a "$with_ssl" != "no"
then
    # With the autoconfigure above, the only time this is going to be
    # true is going to be when we could not find the headers.  If they
    # are not in system standard locations, we are going to be broken.
    if test "$with_ssl" = "yes"
    then
# Let's just define the standard location for the SSLeay root
        with_ssl="/usr/local/ssl"
    fi
    if test -r $with_ssl/include/openssl/ssl.h
    then
###	ssl.h found under openssl.  Use openssl configuration preferentially
        echo "Enabling OpenSSL support in $with_ssl"
        CFLAGS="$CFLAGS -I$with_ssl/include -I$with_ssl/include/openssl"
###	OpenBSD comes with ssl headers
    elif test -r /usr/include/ssl/ssl.h
    then
        echo "Enabling SSLeay support in $with_ssl"
        CFLAGS="$CFLAGS -I/usr/include/ssl"
    else
        echo "Enabling SSLeay support in $with_ssl"
        CFLAGS="$CFLAGS -I$with_ssl/include"
    fi
    LDFLAGS="$LDFLAGS -L$with_ssl/lib"
    LIBS="$LIBS -lssl -lcrypto"
    AC_DEFINE(SSL_ENABLE)
else
    echo 'Disabling SSL support...'
fi

])


#!/bin/sh
rm -f config.cache
([ ! -d macros ] || (echo -n aclocal...\  && aclocal -I macros)) && \
([ ! -r acconfig.h ] || (echo -n autoheader...\  && autoheader)) && \
([ ! -r macros/acconfig.h ] || (echo -n autoheader...\  && autoheader -l macros)) && \
echo -n autoconf...\  && autoconf

#!/bin/bash -e

# Create a provides of the following form:
# rpm_macro(foobar)
# for each defined macro in a macros.* file in %_rpmmacrodir

# We reuse rpm itself for this by just loading just this macro file and then use
# the %dump macro to print all defined macros from that file. rpm gives us an
# output of the following form:
# ========================
# -11: _target    x86_64-linux
# -11= _target_cpu        x86_64
# -11= _target_os linux
# -13: py_build   %{expand:\
#  ... etc
# ===========================
#
# Everything starting with -11 are macros from rpmrc, those starting with -13
# are definitions from macrofiles (i.e. those are that we want), -15 are
# defaults and -20 are builtins (see rpmio/rpmmacro.h).
#
# => We grep for all lines starting with -13, as these are the macro defines
# from the file in question.
#
# The actual macro name is in the second column and is extracted via awk,
# optionally removing parameters declared in brackets. Also, we drop any macros
# that start with __ as these are considered internal/private and should not be
# exposed as a public API.

while read filename
do
    for macro in $(rpm --macros="${filename}" -E "%dump" 2>&1 | grep '^-13:' | awk '{print $2}' | sed -e 's|(.*)||' -e '/^__/d'); do
        echo "rpm_macro(${macro})"
    done
done

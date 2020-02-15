#!/bin/bash
# This is a helper for rpm which collects 'Provides' and 'Requires' information from OCaml files.
# It reads a list of filenames from STDIN.
# It expects as argument either '--provides|-P' or '--requires|-R'.
# Additional optional arguments are:
# -f "ocamlobjinfo command"
# -c # ignored, recognized just for compat reasons
# -i NAME # omit the Requires/Provides for this bytecode unit name
# -x NAME # omit the Requires/Provides for this native unit name
#
# OCaml object files contain either bytecode or native code.
# Each bytecode variant provides a certain interface, which is represented by a hash.
# Each native variant provides a certain interface and a certain implementation, which are represented by hashes.
# Each variant may also require a certain interface and/or implementation provided by other files.
# The details for each file can be inspected with 'ocamlobjinfo'.
#
# Each file contains at least one module.
# Information about each module follows after a line starting with "Name:" or "Unit name:":
#
# cma/cmi/cmo (bytecode):
#   Unit name: NAME
#   Interfaces imported:
#     HASH NAME
#     HASH NAME_FROM_OTHER_MODULE
#
# cmx/cmxa/cmxs (native):
#   Name: NAME
#   CRC of implementation: HASH
#   Interfaces imported:
#     HASH NAME
#     HASH NAME_FROM_OTHER_MODULE
#   Implementations imported:
#     HASH NAME_FROM_OTHER_MODULE
#
# The hash may contain just '-', in which case it is ignored.
#
# Output:
# ocaml(NAME) = HASH # for interfaces (bytecode and native)
# ocamlx(NAME) = HASH # for implementations (native)

set -e
#
OCAMLOBJINFO=ocamlobjinfo
rpm_prefix_interface='ocaml'
rpm_prefix_implementation='ocamlx'
#
parse() {
  local filename="$1"

  ${OCAMLOBJINFO} "${filename}" | awk '
  BEGIN {
    debug=0
    mode=ENVIRON["mode"]
    RPM_BUILD_ROOT=ENVIRON["RPM_BUILD_ROOT"]
    rpm_prefix_interface=ENVIRON["rpm_prefix_interface"]
    rpm_prefix_implementation=ENVIRON["rpm_prefix_implementation"]
    state="find"
    unit=""

    split(ENVIRON["ignore_implementation"], ignore_implementation_a)
    for (i in ignore_implementation_a) {
      val=ignore_implementation_a[i]
      if (debug)
        printf "INFO: ignore_implementation %s\n", val  > "/dev/stderr"
      ignore_implementation[val]=1
    }
    split(ENVIRON["ignore_interface"], ignore_interface_a)
    for (i in ignore_interface_a) {
      val=ignore_interface_a[i]
      if (debug)
        printf "INFO: ignore_interface %s\n", val  > "/dev/stderr"
      ignore_interface[val]=1
    }
  }

  /^File / {
    if (RPM_BUILD_ROOT != "" ) {
      file=substr($2,length(RPM_BUILD_ROOT)+1)
    } else {
      file=$2
    }
    state="file"
    next
  }
  /^Unit name:/ {
    unit=$3
    state="cma"
    next
  }
  /^Name:/ {
    unit=$2
    state="cmx"
    next
  }

  /^CRC of implementation:/ {
    if (state == "cmx") {
      if (ignore_implementation[unit] != "") {
        if (ignore_implementation[unit] != "seen") {
          printf "INFO: ignoring Provides %s(%s)=%s from %s\n", rpm_prefix_implementation, unit, $4, file  > "/dev/stderr"
          ignore_implementation[unit]="seen"
        }
      } else {
        implementation_provides[unit]=$4
      }
    } else {
      printf "WARN: state %s, expected cmx, got %s\n", state, $0 > "/dev/stderr"
    }
    state="crc"
    next
  }

  /^Interfaces imported:/ {
    state="interface"
    next
  }

  /^Implementations imported:/ {
    state="implementation"
    next
  }

  /^\t/ {
    if (state == "interface" && NF > 1 && match($1, "^-") == 0) {
      if (unit == $2) {
        if (ignore_interface[unit] != "") {
          if (ignore_interface[unit] != "seen") {
            printf "INFO: ignoring Provides %s(%s)=%s from %s\n", rpm_prefix_interface, unit, $1, file  > "/dev/stderr"
            ignore_interface[unit]="seen"
          }
        } else {
          interface_provides[unit]=$1
        }
      } else {
        if (ignore_interface[$2] != "") {
          if (ignore_interface[$2] != "seen") {
            printf "INFO: ignoring Requires %s(%s)=%s from %s\n", rpm_prefix_interface, $2, $1, file  > "/dev/stderr"
            ignore_interface[$2]="seen"
          }
        } else {
          interface_requires[$2]=$1
        }
      }
      next
    } else if (state == "implementation" && NF > 1 && match($1, "^-") == 0) {
      if (unit == $2) {
        if (ignore_implementation[unit] != "") {
          if (ignore_implementation[unit] != "seen") {
            printf "INFO: ignoring Provides %s(%s)=%s from %s\n", rpm_prefix_implementation, unit, $1, file  > "/dev/stderr"
            ignore_implementation[unit]="seen"
          }
        } else {
          implementation_provides[unit]=$1
        }
      } else {
        if (ignore_implementation[$2] != "") {
          if (ignore_implementation[$2] != "seen") {
            printf "INFO: ignoring Requires %s(%s)=%s from %s\n", rpm_prefix_implementation, $2, $1, file  > "/dev/stderr"
            ignore_implementation[$2]="seen"
          }
        } else {
          implementation_requires[$2]=$1
        }
      }
      next
    } else  {
      next
    }
  }
  /^.*/ {
    state="find"
  }

  END {
    if (mode == "provides") {
      for (i in interface_provides) {
        printf "%s(%s) = %s\n", rpm_prefix_interface, i, interface_provides[i]
      }
      for (i in implementation_provides) {
        printf "%s(%s) = %s\n", rpm_prefix_implementation, i, implementation_provides[i]
      }
    }
    if (mode == "requires") {
      for (i in interface_requires) {
        printf "%s(%s) = %s\n", rpm_prefix_interface, i, interface_requires[i]
      }
      for (i in implementation_requires) {
        printf "%s(%s) = %s\n", rpm_prefix_implementation, i, implementation_requires[i]
      }
    }
  }
  '
}
#
#
usage() {
    echo >&2 "Usage: ${0##*/} -provides|-requires [-f 'ocamlobjinfo cmd']"
}
#
mode=
ignore_implementation_a=()
ignore_interface_a=()
while test "$#" -gt 0
do
  : "${1}" "${2}"
  case "${1}" in
    -P|--provides) mode='provides' ;;
    -R|--requires) mode='requires' ;;
    -i) ignore_interface_a+=("$2") ; shift ;;
    -x) ignore_implementation_a+=("$2") ; shift ;;
    -f) OCAMLOBJINFO="$2"; shift ;;
    -h|--help) usage ; exit 0 ;;
    -c) ;; # ignored
    --) break ;;
    *) usage ; exit 1 ;;
  esac
  shift
done
if test -z "${mode}" 
then
  usage
  exit 1
fi
#
export rpm_prefix_interface
export rpm_prefix_implementation
export mode
export ignore_implementation="${ignore_implementation_a[@]}"
export ignore_interface="${ignore_interface_a[@]}"
#
while read filename
do
  case "${filename}" in
  *.cma)  parse "${filename}" ;;
  *.cmi)  parse "${filename}" ;;
  *.cmo)  parse "${filename}" ;;
  *.cmx)  parse "${filename}" ;;
  *.cmxa) parse "${filename}" ;;
  *.cmxs) parse "${filename}" ;;
  *) continue ;;
  esac
done
# vim: tw=666 ts=2 shiftwidth=2 et

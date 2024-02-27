#!/bin/bash
# -*- mode: shell-script; indent-tabs-mode: true; tab-width: 4; -*-

# Creates system users and groups, based on files in the format described
# in sysusers.d(5). Replacement for systemd-sysusers(8) for the use of RPM
#
# USAGE:
#
#   sysusers.sh [--root ROOTDIR] [--replace CONFIGFILE] [CONFIGFILE...]
#
# SEE ALSO systemd-sysusers(8)
#
# Based on sysusers.generate-pre.sh from the Fedora systemd package

SYSTEMDIR=/usr/lib/sysusers.d
RUNTIMEDIR=/run/sysusers.d
SYSTEMCONFDIR=/etc/sysusers.d

ROOT=/
REPLACE=#

while test $# -gt 0 ; do
	case "${1}" in
		--root )
			shift
			ROOT=${1}
			shift
			;;
		--replace )
			shift
			REPLACE=${1}
			shift
			;;
		--help )
			cat <<EOF

sysusers.sh - Creates system users and groups, based on files in the
format described in sysusers.d(5). Replacement for systemd-sysusers(8)
for the use of RPM.

USAGE:

	sysusers.sh [--root ROOTDIR] [--replace CONFIGFILE] [CONFIGFILE...]

SEE ALSO systemd-sysusers(8)

EOF
			shift
			;;
		* )
			break
			;;
	esac
done

hasuser() {
	grep "^${1}:" "$ROOT"/etc/passwd >/dev/null
}

hasuid() {
	cut -d: -f3 "$ROOT"/etc/passwd | grep "${1}"  >/dev/null
}

hasgroup() {
	grep "^${1}:" "$ROOT"/etc/group >/dev/null
}

user() {
	local user="$1"
	local uid="$2"
	local desc="$3"
	local group="$4"
	local home="$5"
	local shell="$6"

	[ "$desc" = '-' ] && desc=
	{ [ "$home" = '-' ] || [ "$home" = '' ]; } && home=/
	{ [ "$shell" = '-' ] || [ "$shell" = '' ]; } && shell=/sbin/nologin

	if [ "$uid" = '-' ] || [ "$uid" = '' ]; then
		if ! hasuser "$user" ; then
			useradd -R "$ROOT" -r -g "${group}" -d "${home}" -s "${shell}" -c "${desc}" "${user}" || :
		fi
	else
		if ! hasuser "${user}" ; then
			if ! hasuid "${uid}" ; then
				useradd -R "$ROOT" -r -u "${uid}" -g "${group}" -d "${home}" -s "${shell}" -c "${desc}" "${user}" || :
			else
				useradd -R "$ROOT" -r -g "${group}" -d "${home}" -s "${shell}" -c "${desc}" "${user}" || :
			fi
		fi
	fi
}

group() {
	local group="$1"
	local gid="$2"

	if [ "$gid" = '-' ]; then
		hasgroup "${group}" || groupadd -R "$ROOT" -f -r "${group}" || :
	else
		hasgroup "${group}" || groupadd -R "$ROOT" -f -g "${gid}" -r "${group}" || :
	fi
}


addtogroup() {
	local user="$1"
	local group="$2"
	
	group "${group}" "-"
	user "${user}" "-" "" "${group}" "" ""

	usermod -R "$ROOT" -a -G "${group}" "$user" || :
}

parse() {
	local line arr

	while read -r line || [ -n "$line" ] ; do
		{ [ "${line:0:1}" = '#' ] || [ "${line:0:1}" = ';' ]; } && continue
		line="${line## *}"
		[ -z "$line" ] && continue
		mapfile -t arr < <(xargs -n1 <<<"$line")

		case "${arr[0]}" in
			('u')
				if [[ "${arr[2]}" == *":"* ]]; then
					user "${arr[1]}" "${arr[2]%:*}" "${arr[3]}" "${arr[2]#*:}" "${arr[4]}" "${arr[5]}"
				else
					group "${arr[1]}" "${arr[2]}"
					user "${arr[1]}" "${arr[2]}" "${arr[3]}" "${arr[1]}" "${arr[4]}" "${arr[5]}"
				fi
				;;
			('g')
				group "${arr[1]}" "${arr[2]}"
				;;
			('m')
				addtogroup "${arr[1]}" "${arr[2]}"
				;;
		esac
	done
}


# Make sure etc and the user/group files exist
mkdir -p "$ROOT"/etc/
[ -e "$ROOT"/etc/passwd ] || touch "$ROOT"/etc/passwd
[ -e "$ROOT"/etc/shadow ] || touch "$ROOT"/etc/shadow
[ -e "$ROOT"/etc/group ] || touch "$ROOT"/etc/group

# read files from command line
for fn in "$@"; do
	# check if file is replaced by one with higher priority
	REPLACEDIR=$(dirname "$REPLACE")
	REPLACENAME=$(basename "$REPLACE")
	if [[ "$REPLACEDIR" == "${SYSTEMDIR}" ]] ; then
		if [ -f "${ROOT}${RUNTIMEDIR}/${REPLACENAME}" ] ; then
			continue
		fi
		if [ -f "${ROOT}${SYSTEMCONFDIR}/${REPLACENAME}" ] ; then
			continue
		fi
	fi

	if [[ "$REPLACEDIR" == "$RUNTIMEDIR" ]] ; then
		if [ -f "${ROOT}${SYSTEMCONFDIR}/${REPLACENAME}" ] ; then
			continue
		fi
	fi

	if [ "-" = "$fn" ]; then
		parse <&0
	else
		[ -r "$fn" ] || continue
		parse <"$fn"
	fi
done

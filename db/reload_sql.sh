#!/bin/bash
set -e -x

if [ -n "$1" ]; then
	[ ! -f "$1" ] && echo "USAGE: $0 [server_conf_file]" && exit -1
	conf_file="$1"
else
	conf_file="$(dirname $0)/../conf/server.conf"
	[ ! -f "${conf_file}" ] && echo "Unable to locate the config file ... please pass it as first parameter." && exit -1
fi

eval "$(cat "${conf_file}" | grep -A7 '[[]mysql]' | awk -F= '{ if(NF==2) print $1 "='"'"'" $2 "'"'"'"; }')"

(
echo "DROP DATABASE ${db};"
echo "CREATE DATABASE ${db};"
echo "\r ${db}"
echo "$(<$(dirname $0)/structure.sql)"
) | mysql --user="${user}" --password="${password}" --port="${port}" --host="${host}"

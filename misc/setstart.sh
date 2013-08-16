#!/bin/bash
set -e

function usage()
{
    cat 1>&2 <<EOF
Usage: setstart.sh|setstop.sh <time>

The time can be any time format understood by date.
EOF
}

if [ "$#" -ne 1 ]; then
    usage
    exit 1
fi

stime="$(date +%s --date="$1")"
if [ "$(basename $0)" = "setstart.sh" ]; then
    action=start
elif [ "$(basename $0)" = "setstop.sh" ]; then
    action=stop
else
    echo "Please call as either setstart.sh or setstop.sh" 1>&2
    exit 1
fi

admin_password="$(sed -n 's/\s*admin_password\s*=\s*\(.*\)/\1/p' conf/server.conf)"
bin/batch abacus.conf <<EOF
auth
user:admin
pass:$admin_password
?ok
?user:admin

startstop
action:$action
time:$stime
group:all
?ok
EOF

#! /bin/bash

nm=$1

[ ! -r conf/marker-$nm.conf ] && echo "USAGE: $0 marker_name" && exit 1

while true; do
	su - abacus -c "cd /usr/local/abacuscm; bin/markerd conf/marker-$nm.conf"
	sleep 5
done

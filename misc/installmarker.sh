#!/bin/sh
set -e

mkdir -p /etc/abacus
cp bin/markerd /usr/local/bin
cp bin/runlimit /usr/local/bin
cp conf/marker.conf.sample /etc/abacus/marker.conf
useradd -g markerd -m -k /etc/skel -s /bin/bash markerd
chown root:markerd /usr/local/bin/runlimit
chmod 4750 /usr/local/bin/runlimit

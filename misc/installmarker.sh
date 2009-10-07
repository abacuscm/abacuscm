#! /bin/sh

mkdir -p /etc/abacus
cp bin/{markerd,runlimit} /usr/local/bin
cp conf/marker.conf.sample /etc/abacus
useradd -g markerd -m -k /etc/skel -s /bin/bash markerd
chown root:markerd /usr/local/bin/runlimit
chmod 4750 /usr/local/bin/runlimit

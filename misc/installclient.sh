#!/bin/sh
set -e

mkdir -p "$DESTDIR/etc/abacus"

install bin/abacus "$DESTDIR/usr/local/bin"
install conf/client.conf.sample "$DESTDIR/etc/abacus/client.conf"

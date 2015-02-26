#!/bin/bash
set -e

CERT_DIR=/conf/certs
DAYS=3650

# If the abacus server key and CA do not exist, create them
if ! [ -d $CERT_DIR ]; then
    TMP_CERT_DIR=/conf/certs.tmp
    rm -rf "$TMP_CERT_DIR"
    mkdir "$TMP_CERT_DIR"
    old_umask=`umask`
    umask 0077
    openssl genrsa -out $TMP_CERT_DIR/ca.key 4096
    openssl genrsa -out $TMP_CERT_DIR/server.key 4096
    umask "$old_umask"
    openssl req -new -x509 -key $TMP_CERT_DIR/ca.key -out $TMP_CERT_DIR/cacert.crt -days $DAYS \
        -subj '/CN=abacus-cert'
    openssl req -subj '/CN=localhost' -new -key $TMP_CERT_DIR/server.key \
        -out $TMP_CERT_DIR/server.csr
    openssl x509 -req -days $DAYS -in $TMP_CERT_DIR/server.csr \
        -CA $TMP_CERT_DIR/cacert.crt -CAkey $TMP_CERT_DIR/ca.key \
        -CAcreateserial -out $TMP_CERT_DIR/server.crt
    mv "$TMP_CERT_DIR" "$CERT_DIR"
fi

# If there is no database, create it
if ! [ -d /data/mysql ]; then
    mysql_install_db --user mysql
    /usr/bin/mysqld_safe &
    sleep 2
    (echo \
        "create database abacus;" \
        "grant all privileges on abacus.* to abacus@localhost identified by 'abacus';" \
        "use abacus;"; cat /abacuscm/db/structure.sql) | mysql -u root --batch
    mysqladmin -u root shutdown
fi

# If there is no server configuration, create it
if ! [ -f /conf/server.conf ]; then
    cp /abacuscm/docker/server.conf /conf/server.conf
fi

# Build the keystore from the abacus certificate
keytool -importcert -alias abacuscert -file $CERT_DIR/cacert.crt \
    -keystore /etc/jetty8/abacus_keystore -storepass password -noprompt

# This isn't really used, but it needs to be there at startup
dd if=/dev/random of=/tmp/rijndael.key bs=1 count=32
dd if=/dev/random of=/tmp/rijndael.iv bs=1 count=16

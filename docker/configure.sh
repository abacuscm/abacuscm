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
        -CAcreateserial -out server.crt
    mv "$TMP_CERT_DIR" "$CERT_DIR"
fi

# Build the keystore from the abacus certificate
keytool -importcert -alias abacuscert -file $CERT_DIR/cacert.crt \
    -keystore /etc/jetty8/abacus_keystore -storepass password -noprompt

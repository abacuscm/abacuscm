#!/bin/bash
set -e

ABACUS_CERT_DIR=/conf/abacus-certs
DAYS=365    # Certificate validity for new certs

# If the abacus server key and CA do not exist, create them
function make_abacus_certs()
{
    local TMP_CERT_DIR
    if ! [ -d $ABACUS_CERT_DIR ]; then
        TMP_CERT_DIR=/conf/abacus-certs.tmp
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
        chown nobody:nogroup $TMP_CERT_DIR/server.key
        mv "$TMP_CERT_DIR" "$ABACUS_CERT_DIR"
    fi
}

# Initialise mysql db if it does not exist
function make_mysql()
{
    # If there is no database, create it
    if ! [ -d /data/mysql ]; then
        mysql_install_db --user mysql
        /usr/bin/mysqld_safe &
        sleep 4
        (echo \
            "create database abacus;" \
            "grant all privileges on abacus.* to abacus@localhost identified by 'abacus';" \
            "use abacus;"; cat /usr/src/abacuscm/db/structure.sql) | mysql -u root --batch
        mysqladmin -u root shutdown
    fi
}

# Copies in server configuration if it does not exist.
# Also changes password to $ABACUS_PASSWORD if it is set.
function make_server_conf()
{
    if ! [ -f /conf/server.conf ]; then
        cp /usr/src/abacuscm/docker/server.conf /conf/server.conf
        if [ -n "$ABACUS_PASSWORD" ]; then
            sed -i "s/^admin_password = admin$/admin_password = $ABACUS_PASSWORD/" /conf/server.conf
        fi
    fi
}

# Build the keystore from the abacus certificate, and generate IVs
function make_abacus_crypto()
{
    keytool -importcert -alias abacuscert -file $ABACUS_CERT_DIR/cacert.crt \
        -keystore /etc/jetty8/abacus_keystore -storepass password -noprompt
    # This isn't really used, but it needs to be there at server startup
    dd if=/dev/random of=/tmp/rijndael.key bs=1 count=32
    dd if=/dev/random of=/tmp/rijndael.iv bs=1 count=16
}

if [ "$#" -eq 0 ]; then
    exec /bin/bash -l
fi

case "$1" in
    server)
        make_abacus_certs
        make_mysql
        make_server_conf
        make_abacus_crypto
        exec supervisord -n -c /etc/supervisor/supervisord.conf
        ;;
    *)
        echo "Unrecognised command '$1'. Running a shell"
        exec /bin/bash -l
        ;;
esac

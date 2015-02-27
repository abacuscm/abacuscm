#!/bin/bash
set -e

JETTY_CERT_DIR=/conf/jetty-certs
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

function make_jetty_certs()
{
    local TMP_CERT_DIR
    if ! [ -d $JETTY_CERT_DIR ]; then
        if [ -z "$JETTY_HOSTNAME" ]; then
            echo "******************************************************" 1>&2
            echo "No Jetty certificate found and JETTY_HOSTNAME not set." 1>&2
            echo "The generated certificate will be for localhost." 1>&2
            echo "******************************************************" 1>&2
            JETTY_HOSTNAME=localhost
        fi
        TMP_CERT_DIR=/conf/jetty-certs.tmp
        rm -rf "$TMP_CERT_DIR"
        mkdir "$TMP_CERT_DIR"
        old_umask=`umask`
        umask 0077
        openssl genrsa -out $TMP_CERT_DIR/jetty.key 4096
        umask "$old_umask"
        openssl req -new -x509 -key $TMP_CERT_DIR/jetty.key -subj "/CN=$JETTY_HOSTNAME" -out $TMP_CERT_DIR/jetty.crt
        mv "$TMP_CERT_DIR" "$JETTY_CERT_DIR"
    fi
}

# Initialise mysql db if it does not exist
function make_mysql()
{
    # If there is no database, create it
    if ! [ -d /data/mysql ]; then
        mkdir /data/mysql
        mysql_install_db --user mysql
        /usr/bin/mysqld_safe --skip-syslog &
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

# Build the keystore from the abacus certificate, and generate IVs. This is done
# every time, because it is not stored persistently.
function make_server_crypto()
{
    keytool -importcert -alias abacuscert -file $ABACUS_CERT_DIR/cacert.crt \
        -keystore /etc/jetty8/abacus_keystore -storepass password -noprompt
    openssl pkcs12 -inkey $JETTY_CERT_DIR/jetty.key -in $JETTY_CERT_DIR/jetty.crt -export -out /tmp/jetty.pkcs12 -passout pass:password
    rm -f /etc/jetty8/keystore
    keytool -importkeystore -srckeystore /tmp/jetty.pkcs12 -srcstoretype PKCS12 -destkeystore /etc/jetty8/keystore -srcstorepass password -storepass password
    sed -i 's/OBF:[a-zA-Z0-9]*/password/g' /etc/jetty8/jetty-ssl.xml
    # This isn't really used, but it needs to be there at server startup
    dd if=/dev/random of=/tmp/rijndael.key bs=1 count=32
    dd if=/dev/random of=/tmp/rijndael.iv bs=1 count=16
}

function redirect_server_logs()
{
    mkdir -p /data/log/supervisor
    rmdir /var/log/supervisor
    ln -s /data/log/supervisor /var/log/supervisor
    mkdir -p /data/log/jetty8
    ln -sf /data/log/jetty8 /usr/share/jetty8/logs
}

if [ "$#" -eq 0 ]; then
    exec /bin/bash -l
fi

case "$1" in
    server)
        mkdir -p /data/log/supervisor /data/log/mysql /data/log/jetty8 /data/log/abacus
        chown nobody:nogroup /data/log/abacus
        make_abacus_certs
        make_mysql
        make_server_conf
        make_jetty_certs
        make_server_crypto
        exec supervisord -n -c /etc/supervisor/supervisord.conf
        ;;
    *)
        echo "Unrecognised command '$1'. Running a shell"
        exec /bin/bash -l
        ;;
esac

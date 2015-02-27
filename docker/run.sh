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
            -CAcreateserial -CAserial $TMP_CERT_DIR/ca.srl -out $TMP_CERT_DIR/server.crt
        mv "$TMP_CERT_DIR" "$ABACUS_CERT_DIR"
    fi
}

function make_jetty_certs()
{
    local TMP_CERT_DIR
    if ! [ -d $JETTY_CERT_DIR ]; then
        if [ -z "$ABACUS_SERVER" ]; then
            echo "**************************************************************" 1>&2
            echo "No Jetty certificate found and ABACUS_SERVER not set." 1>&2
            echo "The generated certificate will be for localhost." 1>&2
            echo "**************************************************************" 1>&2
            ABACUS_SERVER=localhost
        fi
        TMP_CERT_DIR=/conf/jetty-certs.tmp
        rm -rf "$TMP_CERT_DIR"
        mkdir "$TMP_CERT_DIR"
        old_umask=`umask`
        umask 0077
        openssl genrsa -out $TMP_CERT_DIR/jetty.key 4096
        umask "$old_umask"
        openssl req -new -x509 -key $TMP_CERT_DIR/jetty.key -subj "/CN=$ABACUS_SERVER" -out $TMP_CERT_DIR/jetty.crt
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
    local -a args
    args=(/usr/src/abacuscm/docker/server.conf)
    for filename in /conf/server.conf.override /contest/server.conf.override; do
        if [ -f "$filename" ]; then
            args+=("$filename")
        fi
    done
    if [ -n "$ABACUS_ADMIN_PASSWORD" ]; then
        args+=(-s "initialisation.admin_password=$ABACUS_ADMIN_PASSWORD")
    fi
    /usr/src/abacuscm/docker/inichange.py "${args[@]}" -o /data/server.conf
}

function make_marker_conf()
{
    local -a args
    args=(/usr/src/abacuscm/docker/marker.conf)
    if [ -f /conf/marker.conf.override ]; then
        args+=(/conf/marker.conf.override)
    fi
    if [ -n "$ABACUS_SERVER" ]; then
        args+=(-s "server.address=$ABACUS_SERVER")
    fi
    if [ -n "$ABACUS_MARKER_PASSWORD" ]; then
        args+=(-s "server.password=$ABACUS_MARKER_PASSWORD")
    fi
    /usr/src/abacuscm/docker/inichange.py "${args[@]}" -o /data/marker.conf
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
    # This needs to be readable by abacus, but we don't want the copy
    # accessible on the host to be readable by whichever random user
    # that corresponds to.
    cp $ABACUS_CERT_DIR/server.key /tmp/server.key
    chown abacus:abacus /tmp/server.key
}

# JETTY_SSL_PORT sets the externally visible port for Jetty, for http->https redirects
function adjust_jetty_ssl_port()
{
    if [ -z "$JETTY_SSL_PORT" ]; then
        JETTY_SSL_PORT=443
    fi
    sed -i 's!<Set name="confidentialPort">[0-9]*</Set>!<Set name="confidentialPort">'"$JETTY_SSL_PORT"'</Set>!' /etc/jetty8/jetty.xml
}

function suid_runlimit()
{
    chown abacus:abacus /usr/bin/runlimit
    chmod 4550 /usr/bin/runlimit
}

if [ "$#" -eq 0 ]; then
    exec /bin/bash -l
fi

case "$1" in
    server)
        mkdir -p /data/log/supervisor /data/log/mysql /data/log/jetty8 /data/log/abacus
        chown abacus:abacus /data/log/abacus
        make_abacus_certs
        make_mysql
        make_server_conf
        make_jetty_certs
        make_server_crypto
        adjust_jetty_ssl_port
        exec supervisord -n -c /etc/supervisor/supervisord.conf
        ;;
    marker)
        if ! [ -d /conf/abacus-certs ]; then
            echo "No abacus certificates found in /conf/abacus-certs." 1>&2
            echo "Please transfer them from the server first" 1>&2
            exit 1
        fi
        mkdir -p /data/log/abacus
        chown abacus:abacus /data/log/abacus
        make_marker_conf
        suid_runlimit
        exec /usr/bin/markerd /data/marker.conf
        ;;
    *)
        echo "Unrecognised command '$1'. Running a shell"
        exec /bin/bash -l
        ;;
esac

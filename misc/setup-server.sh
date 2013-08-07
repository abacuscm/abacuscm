#!/bin/bash
set -e

function die
{
    echo "$@" 1>&2
    exit 1
}

function usage
{
    die "Usage: $0 <target-dir> <host>"
}

function doit
{
    # echo "$@"
    "$@"
}

if [ "$#" -ne 2 ]; then
    usage
fi

base_dir="$(dirname $(dirname $(realpath -s $0)))"
target_dir="$1"
host="$2"
if [ -e "$target_dir" ]; then
    die "$target_dir already exists"
fi

if [ ! -d $base_dir/modules ]; then
    die "abacus directory not found or not built"
fi

doit mkdir -p "$target_dir"
trap 'rm -rf -- "$target_dir"' EXIT

for i in bin modules db lib misc doc; do
    doit ln -s "$base_dir/$i" "$target_dir/$i"
done
doit mkdir "$target_dir/conf"
doit mkdir "$target_dir/certs"
doit mkdir "$target_dir/users"
doit ln -s "$base_dir/conf/java.policy" "$target_dir/conf/java.policy"
doit ln -sf "$base_dir/certs/addserver.sh" "$target_dir/certs"
doit ln -sf "$base_dir/certs/makeca.sh" "$target_dir/certs"
doit ln -sf "$base_dir/certs/openssl.conf" "$target_dir/certs"
um="$(umask)"
umask 077
doit sed "s/chimera/$host/" "$base_dir/conf/server.conf.sample" > "$target_dir/conf/server.conf"
doit dd if=/dev/random of="$target_dir/conf/rijndael.key" bs=1 count=32
doit dd if=/dev/random of="$target_dir/conf/rijndael.iv" bs=1 count=16
(cd "$target_dir/certs" && ./makeca.sh)
(cd "$target_dir/certs" && ./addserver.sh "$host")
doit keytool -importcert -alias abacuscert -file "$target_dir/certs/cacert.crt" -keystore "$target_dir/certs/keystore" -storepass secret -noprompt
umask $um

# All done successfully, uninstall the nuking handler
trap '' EXIT

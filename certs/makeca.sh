#! /bin/bash

cadir=AbacusCA
cakey="${cadir}/private/cakey.pem"
cacert="${cadir}/cacert.pem"
cakeysize=4096
cavalidity=365

cd "$(dirname "$0")"

export OPENSSL_CONF="$(pwd)/openssl.conf"

function makedir() {
	echo "Making: $1"
	mkdir "$1" || exit -1
}

if [ -e "${cadir}" ]; then
	echo "The directory ${cadir} already exist.  rm it before attempting to run this script!"
	exit -1
fi

echo "Creating directory structure:"
makedir "${cadir}"
makedir "${cadir}/certs"
makedir "${cadir}/crl"
makedir "${cadir}/newcerts"
makedir "${cadir}/private"

echo "Initialising serial to 01"
echo 01 > "${cadir}/serial"

echo "Creating index file"
touch "${cadir}/index.txt"

echo "Generating ${cakeysize}-bit RSA key for CA."
um=$(umask)
umask 0077
openssl genrsa -out "${cakey}" 4096 || exit -1
umask ${um}
echo "Signing CA certificate for ${cavalidity} days."
openssl req -new -x509 -key "${cakey}" -out "${cacert}" -days "${cavalidity}" || exit -1

echo "Symlinking to cacert.crt"
ln -s "${cacert}" cacert.crt

echo "All done."

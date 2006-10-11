#! /bin/bash
# $Id$

sslserver="$(which sslserver 2>/dev/null)"
bdir="$(dirname $0)"

[ ! -x "${sslserver}" ] && echo "You need ucpsi-ssl (http://www.superscript.com/ucspi-ssl/intro.html) installed." >&2 && exit 1

[ -f "${CAFILE}" ] || CAFILE="${bdir}/cacert.crt"
[ -f "${CERTFILE}" ] || CERTFILE="${bdir}/server.crt"
[ -f "${KEYFILE}" ] || KEYFILE="${bdir}/server.key"
[ -f "${DHFILE}" ] || DHFILE="${bdir}/dh.pem"

bad=0
[ ! -f "${CAFILE}" ] && echo "You should symlink cacert.crt in the mitm directory to your cacert file, or export CAFILE to point to it." >&2 && bad=1
[ ! -f "${CERTFILE}" ] && echo "You should symlink server.crt in the mitm directory to the certificate you want to use, or export CERTFILE to point to it." >&2 && bad=1
[ ! -f "${KEYFILE}" ] && echo "You should symlink server.key in the mitm directory to the keyfile for your certificate, or export KEYFILE to point to it." >&2 && bad=1

[ "${bad}" -eq 1 ] && exit 1

if [ ! -f "${DHFILE}" ]; then
	echo "${DHFILE} does not exist, creating it."
	openssl dhparam -out "${DHFILE}" 512 || exit 1
fi

export CAFILE
export CERTFILE
export KEYFILE
export DHFILE
export CIPHERS=HIGH

"${sslserver}" -v -c 1 0.0.0.0 7369 "${bdir}/mitm_serv.sh" 2>&1

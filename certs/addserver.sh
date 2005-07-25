#! /bin/bash

req_dir=servers/requests
keys_dir=servers/keys
certs_dir=servers/certs
sname="${1}"
validity=30

# If you change this you will need to also update it in
# the C++ code.
keysize=1024

[ "${sname}" == "" ] && echo "You need to spec a 'server' name as the first parameter!" && exit -1

[ -d "${req_dir}" ] || mkdir -p "${req_dir}" || exit -1
[ -d "${keys_dir}" ] || mkdir -p "${keys_dir}" || exit -1
[ -d "${certs_dir}" ] || mkdir -p "${certs_dir}" || exit -1

cd "$(dirname "$0")"

export OPENSSL_CONF="$(pwd)/openssl.conf"

reqfile="${req_dir}/${sname}.req"
keyfile="${keys_dir}/${sname}.key"
crtfile="${certs_dir}/${sname}.crt"

if [ -r "${keyfile}" ]; then
	echo "Keyfile exists and is readable - skipping"
else
	echo "Generating key for ${sname}"
	um=$(umask)
	umask 0077
	openssl genrsa -out "${keyfile}" "${keysize}" || exit -1
	umask ${um}

	rm -f "${reqfile}" "${crtfile}"
fi

if [ -r "${reqfile}" ]; then
	echo "Request file exists and is readable - skippting"
else
	echo "Generating certificate request:"
	openssl req -new -key "${keyfile}" -out "${reqfile}" || exit -1
	rm -f "${crtfile}"
fi

if [ -r "${crtfile}" ]; then	
	echo "Certificate file exists and is readable - skipping"
else
	echo "Signing certificate."
	openssl ca -batch -in "${reqfile}" -out "${crtfile}" || exit -1
fi

echo "Symlinking for ease of use"
ln -s "${crtfile}" "${keyfile}" .

echo "All done"

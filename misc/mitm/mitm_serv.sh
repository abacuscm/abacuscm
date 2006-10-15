#! /bin/bash
# $Id$

tee /dev/stderr | openssl s_client -connect localhost:7368 -CAfile "${CAFILE}" -tls1 -quiet | tee /dev/stderr

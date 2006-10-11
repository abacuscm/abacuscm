#! /bin/bash
# $Id$

tee /dev/stderr | openssl s_client -connect localhost:7368 -CAfile "${CAFILE}" -quiet 2>/dev/null | tee /dev/stderr

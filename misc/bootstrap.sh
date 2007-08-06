#!/bin/sh
set -e
echo "There may be some warnings. You can probably ignore them"
echo
libtoolize --automake --copy --ltdl
autoreconf --install
echo
echo "Bootstrapping succeeded"

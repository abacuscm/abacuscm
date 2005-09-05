#! /bin/bash

db=$1
[ "${db}" == "" ] && db=abacus

echo "DROP DATABASE ${db};"
echo "CREATE DATABASE ${db};"
echo "\r ${db}"
echo "$(<$(dirname $0)/structure.sql)"

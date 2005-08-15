#! /bin/bash

echo "DROP DATABASE abacus;"
echo "CREATE DATABASE abacus;"
echo "$(<$(dirname $0)/structure.sql)"

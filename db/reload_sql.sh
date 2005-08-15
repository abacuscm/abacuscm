#! /bin/bash

echo "DROP DATABASE abacus;"
echo "CREATE DATABASE abacus;"
echo "$(<structure.sql)"

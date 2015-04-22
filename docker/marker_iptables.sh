#!/bin/bash
# Script to lock down a marker container so that it can only open connections
# to the abacus server on the marker port.
#
# NB: *before* running this script, one must run
#
# iptables -N SBITC
# iptables -I FORWARD 1 -j SBITC
#
# to ensure that the chain manipulated by this script is hooked up. Note that
# this script accumulates rules, so it can be used for multiple containers. To
# clear the rules, run
#
# iptables -F SBITC

set -e

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <container ID> <target IP>" 1>&2
    exit 2
fi
IP="$(docker inspect --format='{{.NetworkSettings.IPAddress}}' "$1")"

iptables -A SBITC -s "$IP" -d "$2" --dport 7368 -j ACCEPT
iptables -A SBITC -d "$IP" -s "$2" --sport 7368 -j ACCEPT
iptables -A SBITC -s "$IP" -m state --state related,established -j ACCEPT
iptables -A SBITC -s "$IP" -j DROP

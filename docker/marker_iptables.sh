#!/bin/bash
# Script to lock down a marker container so that it can only open connections
# to the abacus server on the marker port.

set -e

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <container ID> <target IP>" 1>&2
    exit 2
fi
IP="$(docker inspect --format='{{.NetworkSettings.IPAddress}}' "$1")"
DNS="$(docker inspect --format='{{range .HostConfig.Dns}}{{.}} {{end}}' "$1")"
PORTS="7368,8080"

if iptables -N SBITC 2>/dev/null; then
    iptables -I FORWARD 1 -j SBITC
fi
iptables -F SBITC
iptables -A SBITC -s "$IP" -d "$2" -p tcp -m multiport --dports $PORTS -j ACCEPT
iptables -A SBITC -d "$IP" -s "$2" -p tcp -m multiport --sports $PORTS -j ACCEPT
iptables -A SBITC -s "$IP" -p tcp -m state --state related,established -j ACCEPT
# Allow DNS
for server in $DNS; do
    iptables -A SBITC -s "$IP" -d $server -p udp --dport 53 -j ACCEPT
    iptables -A SBITC -d "$IP" -s $server -p udp -j ACCEPT
done
# Drop everything else
iptables -A SBITC -s "$IP" -j DROP
iptables -A SBITC -d "$IP" -j DROP

#!/bin/bash
set -e
CLIENTCONF=client.conf
SERVERCONF=conf/abacus/server.conf.override
ABACUSTOOL="../bin/abacustool -c $CLIENTCONF -s $SERVERCONF"
for i in {1..10}; do
    $ABACUSTOOL adduser contestant$i "Contestant $i" contestant$i contestant
    $ABACUSTOOL adduser judge$i "Judge $i" judge$i judge
    $ABACUSTOOL adduser observer$i "Observer $i" observer$i observer
    $ABACUSTOOL adduser proctor$i "Proctor $i" proctor$i proctor
done
for i in {1..4}; do
    $ABACUSTOOL addproblem tcprob shortname $i longname "Problem $i" time_limit 5 memory_limit 256 ignore_whitespace Yes multi_submit No testcase.input ../tests/batch/data.in testcase.output ../tests/batch/data.out
done
for i in 5; do
    $ABACUSTOOL addproblem tcprob shortname $i longname "Problem $i (checker)" time_limit 5 memory_limit 256 ignore_whitespace Yes multi_submit No testcase.input ../tests/batch/data.in testcase.output ../tests/batch/data.out checker ../tests/batch/checker.py
done
$ABACUSTOOL addtime start now

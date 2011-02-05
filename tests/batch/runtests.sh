#!/bin/bash
set -e

# Before using this script: on the server
# 1. Wipe out the server database:
# db/reload_sql.sh conf/server.conf
# 2. Start the server:
#
# Now run this script as
# tests/batch/run-tests.sh client.conf server.conf marker.conf

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 client.conf server.conf marker.conf" 1>&2
    exit 2
fi

client_conf="$1"
server_conf="$2"
marker_conf="$3"
if [ ! -f "$client_conf" ]; then
    echo "'$client_conf' not found" 1>&2
    exit 1
fi
if [ ! -f "$server_conf" ]; then
    echo "'$server_conf' not found" 1>&2
    exit 1
fi
if [ ! -f "$marker_conf" ]; then
    echo "'$marker_conf' not found" 1>&2
    exit 1
fi

function get_setting()
{
    local section="$1"
    local name="$2"
    local filename="$3"
    sed -n -e '/^\['"$section"'\]/,/^\[/s/^'"$name"' *= *\(.*\)/\1/p' "$filename"
}

admin_password="$(get_setting initialisation admin_password "$server_conf")"
marker_username="$(get_setting server username "$marker_conf")"
marker_password="$(get_setting server password "$marker_conf")"
duration="$(get_setting contest duration "$server_conf")"
blinds="$(get_setting contest blinds "$server_conf")"
now=$(date +%s)

# A time in the recent past, useful for starting the contest
let recent=now-1
# A time in the distant past, useful for putting the contest into the blinds
let old=now-duration+blinds/2
# A long time into the future
let future=now+duration

bin/batch "$client_conf" <<EOF
whatami
?ok

auth
user:admin
pass:$admin_password
?ok
?user:admin

whatami
?ok
?*:*

adduser
username:test1
friendlyname:<b>Unicode</b>: ēßõ±°½—£
passwd:test1
type:contestant
?ok

adduser
username:judge
friendlyname:HTML: <b>not bold</b>
passwd:judge
type:judge
?ok

# Not used in test, but allows a marker to be connected afterwards
adduser
username:$marker_username
friendlyname:Marker
passwd:$marker_password
type:marker
?ok

getusers
?ok
?id0:1
?username0:admin
?id1:33
?username1:judge
?id2:49
?username2:marker
?id3:17
?username3:test1

passwd
newpass:$admin_password
?ok

id_passwd
user_id:17
newpass:test1
?ok

getlanguages
?ok
?language0:C++
?language1:Java
?language2:Python

getprobtypes
?ok
?type0:interactiveprob
?type1:tcprob

getprobdescript
type:tcprob
?ok
?descript:shortname S, longname S, multi_submit {No,Yes}, testcase (input F, output F), ignore_whitespace {Yes,No}, time_limit I

setprobattrs
prob_id:0
prob_type:tcprob
time_limit:5
ignore_whitespace:Yes
shortname:test
longname:Test Problem
multi_submit:No
testcase.input<tests/batch/data.in
testcase.output<tests/batch/data.out
?ok

getprobattrs
prob_id:1
?ok
?ignore_whitespace:Yes
?longname:Test Problem
?multi_submit:No
?prob_type:tcprob
?shortname:test
?testcase.input:data.in
?testcase.output:data.out
?time_limit:5

setprobattrs
prob_id:0
prob_type:tcprob
time_limit:5
ignore_whitespace:Yes
shortname:test2
longname:<b>Unicode</b> £etterß
multi_submit:No
testcase.input<tests/batch/data.in
testcase.output<tests/batch/data.out
prob_dependencies:1
?ok

getprobattrs
prob_id:17
?ok
?ignore_whitespace:Yes
?longname:<b>Unicode</b> £etterß
?multi_submit:No
?prob_type:tcprob
?shortname:test2
?testcase.input:data.in
?testcase.output:data.out
?time_limit:5

getprobfile
prob_id:1
file:testcase.input
?ok
?<tests/batch/data.in

getproblems
?ok
?id0:1
?code0:test
?name0:Test Problem
?id1:17
?code1:test2
?name1:<b>Unicode</b> £etterß

getsubmissions
?ok

standings
?ok
?ncols:8
?nrows:1
?row_0_0:ID
?row_0_1:Team
?row_0_2:Name
?row_0_3:Contestant
?row_0_4:Solved
?row_0_5:Time
?row_0_6:test
?row_0_7:test2

clarificationrequest
prob_id:1
question:Question with <i>HTML</i> and unicode: £À´´µ¶?
?ok

clarification
clarification_request_id:1
answer:Public answer, with unicode: £À´´µ¶ and <b>HTML</b>
public:1
?ok

clarification
clarification_request_id:1
answer:Private answer.Second line.
public:0
?ok

getclarificationrequests
?ok
?id0:1
?user_id0:1
?time0:*
?question0:Question with <i>HTML</i> and unicode: £À´´µ¶?
?problem0:test

getclarifications
?ok
?id0:1
?req_id0:1
?problem0:test
?time0:*
?question0:Question with <i>HTML</i> and unicode: £À´´µ¶?
?answer0:Public answer, with unicode: £À´´µ¶ and <b>HTML</b>
?id1:17
?req_id1:1
?problem1:test
?time1:*
?question1:Question with <i>HTML</i> and unicode: £À´´µ¶?
?answer1:Private answer.Second line.

# A time in the past, so that contest will start
startstop
action:start
time:$recent
server_id:all
?ok

# Send same time again, to check that that doesn't break it
startstop
action:start
time:$recent
server_id:all
?ok

# Hopefully a long time in the future
startstop
action:stop
time:$future
server_id:all
?ok

contesttime
?ok
?running:yes
?remain:*
?time:*

subscribetime
?ok

eventmap
event:balloon
action:subscribe
?ok

eventmap
event:startstop
action:subscribe
?ok

eventmap
event:updateclarifications
action:subscribe
?ok

eventmap
event:updateclarificationrequests
action:subscribe
?ok

eventmap
event:updateclock
action:subscribe
?ok

eventmap
event:updatestandings
action:subscribe
?ok

eventmap
event:updatesubmissions
action:subscribe
?ok

getserverlist
?ok
?server0:*

# Still TODO:
# getsubmissionsource
# getsubmissions (with submissions)
# getstandings (with non-trivial standings)
# mark
# fetchfile
# subscribemark
# addserver

auth
user:test1
pass:test1
?ok
?user:test1

whatami
?ok
?permission0:auth
?permission1:submit
?permission2:clarification_request
?permission3:change_password
?permission4:in_standings
?permission5:see_standings

standings
?ok
?ncols:8
?nrows:1
?row_0_0:ID
?row_0_1:Team
?row_0_2:Name
?row_0_3:Contestant
?row_0_4:Solved
?row_0_5:Time
?row_0_6:test
?row_0_7:test2

submit
prob_id:1
lang:C++
<tests/bad_solutions/empty.cpp
?ok

submit
prob_id:1
lang:Java
<tests/bad_solutions/spin_forever.java
?ok

submit
prob_id:1
lang:Python
<tests/bad_solutions/exception.py
?ok

getproblems
?ok
?id0:1
?code0:test
?name0:Test Problem

auth
user:judge
pass:judge
?ok
?user:judge

getsubmissionsource
submission_id:1
?ok
?<tests/bad_solutions/empty.cpp
EOF

echo "**************************************************************"
echo "Waiting for marking of 3 submissions - press enter when marked"
echo "**************************************************************"
read

bin/batch "$client_conf" <<EOF
auth
user:test1
pass:test1
?ok
?user:test1

# Compilation failure must not count, the users must count
standings
?ok
?ncols:8
?nrows:2
?row_0_0:ID
?row_0_1:Team
?row_0_2:Name
?row_0_3:Contestant
?row_0_4:Solved
?row_0_5:Time
?row_0_6:test
?row_0_7:test2
?row_1_0:17
?row_1_1:test1
?row_1_2:<b>Unicode</b>: ēßõ±°½—£
?row_1_3:1
?row_1_4:0
?row_1_5:*
?row_1_6:-2
?row_1_7:0
EOF

echo "****************"
echo "Testing complete"
echo "****************"

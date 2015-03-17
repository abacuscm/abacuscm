#!/bin/bash
set -e

# Before using this script: on the server
# 1. Wipe out the server database:
# db/reload_sql.sh conf/server.conf
# 2. Start the server.
# 3. Now run this script as
#    tests/batch/run-tests.sh client.conf server.conf marker.conf
# 4. When prompted, start the marker.

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
if [ -z "$admin_password" ]; then
    admin_password="admin"
fi
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

nl_escape=$'\001'

bin/batch "$client_conf" <<EOF
whatami
?ok

# TEST: When first starting the server, the admin_password must apply if set.
auth
user:admin
pass:$admin_password
?ok
?user:admin

whatami
?ok
?*:*

# TEST: Adding a new group must succeed
addgroup
groupname:testgroup
?ok

# TEST: Adding an existing group name must fail
addgroup
groupname:testgroup
?err
?msg:*

# TEST: Adding a blank group name must fail
addgroup
groupname:
?err
?msg:*

# TEST: getgroups must return the groups
getgroups
?ok
?id0:1
?groupname0:default
?id1:17
?groupname1:testgroup

# TEST: Creating a new user must succeed, including support for Unicode in the team name.
adduser
username:test1
friendlyname:<b>Unicode</b>: ēßõ±°½—£
passwd:changeme_test1
type:contestant
group:1
?ok

adduser
username:judge
friendlyname:HTML: <b>not bold</b>
passwd:judge
type:judge
group:17
?ok

# TEST: Creating a new user with a blank name must fail.
adduser
username:
friendlyname:Blank
password:blank
type:contestant
group:1
?err
?msg:*

# TEST: Creating a new user must fail if the name is already in use.
adduser
username:judge
friendlyname:New judge
passwd:judge
type:judge
group:1

# TEST: omitting the group field must fail
adduser
username:nogroup
friendlyname:nogroup
password:nogroup
type:contestant
?err
?msg:*

# TEST: using a non-existent group must fail
adduser
username:badgroup
friendlyname:badgroup
password:badgroup
type:contestant
group:3
?err
?msg:*

# Not used in test, but allows a marker to be connected afterwards
adduser
username:$marker_username
friendlyname:Marker
passwd:$marker_password
type:marker
group:1
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

# TEST: Changing a password must succeed
# Later we test logging in with this new password.
id_passwd
user_id:17
newpass:test1
?ok

getlanguages
?ok
?language0:C++
?language1:Java
?language2:Python 2.x
?language3:Python 3.x

getprobtypes
?ok
?type0:interactiveprob
?type1:tcprob

getprobdescript
type:tcprob
?ok
?descript:shortname S, longname S, multi_submit {No,Yes}, testcase (input F, output F), ignore_whitespace {Yes,No}, time_limit I, memory_limit I

# TEST: creating a problem must succeed
setprobattrs
prob_id:0
prob_type:tcprob
time_limit:1
memory_limit:64
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
?time_limit:1
?memory_limit:64

# TEST: creating a problem with dependencies must succeed
setprobattrs
prob_id:0
prob_type:tcprob
time_limit:1
memory_limit:64
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
?time_limit:1
?memory_limit:64

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

# TEST: replacing a problem input and output must work
setprobattrs
prob_id:17
prob_type:tcprob
time_limit:1
memory_limit:64
ignore_whitespace:Yes
shortname:test2
longname:<b>Unicode</b> £etterß
multi_submit:No
testcase.input<tests/batch/data2.in
testcase.output<tests/batch/data2.out
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
?testcase.input:data2.in
?testcase.output:data2.out
?time_limit:1
?memory_limit:64

getprobfile
prob_id:17
file:testcase.input
?ok
?<tests/batch/data2.in

getprobfile
prob_id:17
file:testcase.output
?ok
?<tests/batch/data2.out

getsubmissions
?ok

standings
?ok
?ncols:9
?nrows:1
?row_0_0:ID
?row_0_1:Team
?row_0_2:Name
?row_0_3:Group
?row_0_4:Contestant
?row_0_5:Solved
?row_0_6:Time
?row_0_7:test
?row_0_8:test2

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
answer:Private answer.${nl_escape}Second line.
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
?answer1:Private answer.${nl_escape}Second line.

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

# TEST: Attempting to register for a non-existent event must not crash the server.
eventmap
event:bogus
action:subscribe
?err
?msg:*

# TEST: Attempting to deregister for a non-existent event must not crash the server.
eventmap
event:bogus
action:unsubscribe
?err
?msg:*

getserverlist
?ok
?server0:*

# Still TODO:
# mark
# fetchfile
# subscribemark
# addserver

auth
user:test1
pass:test1
?ok
?user:test1

# TEST: Normal users must be able to set their passwords
passwd
newpass:tmp_test1
?ok

# TEST: Normal users must not be able to set a blank password.
passwd
newpass:
?err
?msg:*

# Test the new password
auth
user:test1
pass:tmp_test1
?ok
?user:test1

# Change the password back
passwd
newpass:test1
?ok

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
?ncols:9
?nrows:1
?row_0_0:ID
?row_0_1:Team
?row_0_2:Name
?row_0_3:Group
?row_0_4:Contestant
?row_0_5:Solved
?row_0_6:Time
?row_0_7:test
?row_0_8:test2

# Submit assorted solutions
# Use gentests.py to regenerate
submit
prob_id:1
lang:Java
<tests/solutions/bad_class.java
?ok
?submission_id:1

submit
prob_id:1
lang:Java
<tests/solutions/bad_class2.java
?ok
?submission_id:17

submit
prob_id:1
lang:Java
<tests/solutions/bad_class3.java
?ok
?submission_id:33

submit
prob_id:1
lang:C++
<tests/solutions/compile_fail.cpp
?ok
?submission_id:49

submit
prob_id:1
lang:Java
<tests/solutions/compile_fail.java
?ok
?submission_id:65

submit
prob_id:1
lang:C++
<tests/solutions/do_nothing.cpp
?ok
?submission_id:81

submit
prob_id:1
lang:Python 2.x
<tests/solutions/do_nothing.py
?ok
?submission_id:97

submit
prob_id:1
lang:C++
<tests/solutions/do_nothing_unicode.cpp
?ok
?submission_id:113

submit
prob_id:1
lang:C++
<tests/solutions/empty.cpp
?ok
?submission_id:129

submit
prob_id:1
lang:Java
<tests/solutions/empty.java
?ok
?submission_id:145

submit
prob_id:1
lang:Python 2.x
<tests/solutions/empty.py
?ok
?submission_id:161

submit
prob_id:1
lang:C++
<tests/solutions/exception.cpp
?ok
?submission_id:177

submit
prob_id:1
lang:Java
<tests/solutions/exception.java
?ok
?submission_id:193

submit
prob_id:1
lang:Python 2.x
<tests/solutions/exception.py
?ok
?submission_id:209

submit
prob_id:1
lang:C++
<tests/solutions/infinite_stream.cpp
?ok
?submission_id:225

submit
prob_id:1
lang:Python 2.x
<tests/solutions/infinite_stream.py
?ok
?submission_id:241

submit
prob_id:1
lang:Java
<tests/solutions/package.java
?ok
?submission_id:257

submit
prob_id:1
lang:Java
<tests/solutions/pathclass.java
?ok
?submission_id:273

submit
prob_id:1
lang:C++
<tests/solutions/sleep_forever.cpp
?ok
?submission_id:289

submit
prob_id:1
lang:Python 2.x
<tests/solutions/sleep_forever.py
?ok
?submission_id:305

submit
prob_id:1
lang:C++
<tests/solutions/spin_forever.cpp
?ok
?submission_id:321

submit
prob_id:1
lang:Java
<tests/solutions/spin_forever.java
?ok
?submission_id:337

submit
prob_id:1
lang:Python 2.x
<tests/solutions/spin_forever.py
?ok
?submission_id:353

submit
prob_id:1
lang:C++
<tests/solutions/wrong_retcode.cpp
?ok
?submission_id:369

submit
prob_id:1
lang:Python 2.x
<tests/solutions/wrong_retcode.py
?ok
?submission_id:385

submit
prob_id:1
lang:Python 2.x
<tests/solutions/exact.py
?ok
?submission_id:401

submit
prob_id:1
lang:Python 2.x
<tests/solutions/whitespace.py
?ok
?submission_id:417

### END OF GENERATED TESTS

# TEST: Contestants must not be able to see problems with unmet dependencies.
getproblems
?ok
?id0:1
?code0:test
?name0:Test Problem

# TEST: It must not be possible to submit a problem until its dependencies are satisfied.
submit
prob_id:17
lang:C++
<tests/solutions/exception.cpp
?err
?msg:You are not allowed to submit a solution for this problem

auth
user:judge
pass:judge
?ok
?user:judge

# TEST: It must be possible for judges to submit clarification requests.
clarificationrequest
question:General judge clarification request.
?ok

# TEST: Judges must see all problems, even if they have dependencies.
getproblems
?ok
?id0:1
?code0:test
?name0:Test Problem
?id1:17
?code1:test2
?name1:<b>Unicode</b> £etterß

getsubmissionsource
submission_id:1
?ok
?<tests/solutions/bad_class.java
EOF

echo "***************************************************************"
echo "Waiting for marking of 27 submissions - press enter when marked"
echo "***************************************************************"
read

bin/batch "$client_conf" <<EOF
auth
user:test1
pass:test1
?ok
?user:test1

# Log in as judge to verify submission status
auth
user:judge
pass:judge
?ok
?user:judge

# TEST: Submitting a correct solution must succeed.
# TEST: Submitting a solution that outputs only different whitespace must succeed (should include leading, intermediate, and trailing whitespace, and blank lines after the end).
# TEST: Submitting a solution that throws an exception must return appropriate error.
# TEST: Submitting a solution that returns non-zero must return appropriate error.
# TEST: Submitting a solution that fails to compile must return appropriate error.
# TEST: Submitting a solution that runs forever must return appropriate error.
# TEST: Submitting a solution that sleeps forever must return appropriate error.
# TEST: Submitting a solution that produces an infinite stream of output must return appropriate error.
# TEST: Submitting an empty solution must return an appropriate error.
# TEST: Submitting a Java solution with a package statement must work.
# Use gentests.py to regenerate
getsubmissions
?ok
?comment0:Compilation failed
?contesttime0:*
?prob_id0:1
?problem0:test
?result0:4
?submission_id0:1
?group_id0:1
?time0:*
?comment1:Compilation failed
?contesttime1:*
?prob_id1:1
?problem1:test
?result1:4
?submission_id1:17
?group_id1:1
?time1:*
?comment2:Compilation failed
?contesttime2:*
?prob_id2:1
?problem2:test
?result2:4
?submission_id2:33
?group_id2:1
?time2:*
?comment3:Compilation failed
?contesttime3:*
?prob_id3:1
?problem3:test
?result3:4
?submission_id3:49
?group_id3:1
?time3:*
?comment4:Compilation failed
?contesttime4:*
?prob_id4:1
?problem4:test
?result4:4
?submission_id4:65
?group_id4:1
?time4:*
?comment5:Deferred to judge
?contesttime5:*
?prob_id5:1
?problem5:test
?result5:5
?submission_id5:81
?group_id5:1
?time5:*
?comment6:Deferred to judge
?contesttime6:*
?prob_id6:1
?problem6:test
?result6:5
?submission_id6:97
?group_id6:1
?time6:*
?comment7:Deferred to judge
?contesttime7:*
?prob_id7:1
?problem7:test
?result7:5
?submission_id7:113
?group_id7:1
?time7:*
?comment8:Compilation failed
?contesttime8:*
?prob_id8:1
?problem8:test
?result8:4
?submission_id8:129
?group_id8:1
?time8:*
?comment9:Compilation failed
?contesttime9:*
?prob_id9:1
?problem9:test
?result9:4
?submission_id9:145
?group_id9:1
?time9:*
?comment10:Deferred to judge
?contesttime10:*
?prob_id10:1
?problem10:test
?result10:5
?submission_id10:161
?group_id10:1
?time10:*
?comment11:Abnormal termination of program
?contesttime11:*
?prob_id11:1
?problem11:test
?result11:3
?submission_id11:177
?group_id11:1
?time11:*
?comment12:Abnormal termination of program
?contesttime12:*
?prob_id12:1
?problem12:test
?result12:3
?submission_id12:193
?group_id12:1
?time12:*
?comment13:Abnormal termination of program
?contesttime13:*
?prob_id13:1
?problem13:test
?result13:3
?submission_id13:209
?group_id13:1
?time13:*
?comment14:Abnormal termination of program
?contesttime14:*
?prob_id14:1
?problem14:test
?result14:3
?submission_id14:225
?group_id14:1
?time14:*
?comment15:Abnormal termination of program
?contesttime15:*
?prob_id15:1
?problem15:test
?result15:3
?submission_id15:241
?group_id15:1
?time15:*
?comment16:Deferred to judge
?contesttime16:*
?prob_id16:1
?problem16:test
?result16:5
?submission_id16:257
?group_id16:1
?time16:*
?comment17:Compilation failed
?contesttime17:*
?prob_id17:1
?problem17:test
?result17:4
?submission_id17:273
?group_id17:1
?time17:*
?comment18:Time limit exceeded
?contesttime18:*
?prob_id18:1
?problem18:test
?result18:2
?submission_id18:289
?group_id18:1
?time18:*
?comment19:Time limit exceeded
?contesttime19:*
?prob_id19:1
?problem19:test
?result19:2
?submission_id19:305
?group_id19:1
?time19:*
?comment20:Time limit exceeded
?contesttime20:*
?prob_id20:1
?problem20:test
?result20:2
?submission_id20:321
?group_id20:1
?time20:*
?comment21:Time limit exceeded
?contesttime21:*
?prob_id21:1
?problem21:test
?result21:2
?submission_id21:337
?group_id21:1
?time21:*
?comment22:Time limit exceeded
?contesttime22:*
?prob_id22:1
?problem22:test
?result22:2
?submission_id22:353
?group_id22:1
?time22:*
?comment23:Abnormal termination of program
?contesttime23:*
?prob_id23:1
?problem23:test
?result23:3
?submission_id23:369
?group_id23:1
?time23:*
?comment24:Abnormal termination of program
?contesttime24:*
?prob_id24:1
?problem24:test
?result24:3
?submission_id24:385
?group_id24:1
?time24:*
?comment25:Correct answer
?contesttime25:*
?prob_id25:1
?problem25:test
?result25:0
?submission_id25:401
?group_id25:1
?time25:*
?comment26:Correct answer
?contesttime26:*
?prob_id26:1
?problem26:test
?result26:0
?submission_id26:417
?group_id26:1
?time26:*

# TEST: Time penalties must not apply for failed compilation
standings
?ok
?ncols:9
?nrows:2
?row_0_0:ID
?row_0_1:Team
?row_0_2:Name
?row_0_3:Group
?row_0_4:Contestant
?row_0_5:Solved
?row_0_6:Time
?row_0_7:test
?row_0_8:test2
?row_1_0:17
?row_1_1:test1
?row_1_2:<b>Unicode</b>: ēßõ±°½—£
?row_1_3:default
?row_1_4:1
?row_1_5:1
?row_1_6:*
?row_1_7:13
?row_1_8:0

# TEST: Judges must be able to mark a solution as wrong
mark
submission_id:81
result:1
comment:Wrong answer
?ok

# TEST: Judges should NOT be able to override a previous decision
mark
submission_id:81
result:0
comment:Correct answer
?err
?msg:*

# TEST: Judges must be able to mark a solution as a format error
mark
submission_id:97
result:6
comment:Format error
?ok

# TEST: Judges must be able to mark a solution as correct
mark
submission_id:161
result:0
comment:Correct answer
?ok

EOF

echo "****************"
echo "Testing complete"
echo "****************"

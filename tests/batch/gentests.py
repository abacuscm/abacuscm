#!/usr/bin/env python3
import sys
import os.path
from textwrap import dedent

CORRECT = 0
WRONG = 1
TIME_EXCEEDED = 2
ABNORMAL = 3
COMPILE_FAILED = 4
JUDGE = 5
FORMAT_ERROR = 6

comments = {
        CORRECT: 'Correct answer',
        WRONG: 'Wrong answer',
        TIME_EXCEEDED: 'Time limit exceeded',
        ABNORMAL: 'Abnormal termination of program',
        COMPILE_FAILED: 'Compilation failed',
        JUDGE: 'Deferred to judge',
        FORMAT_ERROR: 'Format error'
}

exts = {
        '.cpp': 'C++',
        '.java': 'Java',
        '.py': 'Python'
}

files = [
        ('bad_class.java', COMPILE_FAILED),
        ('bad_class2.java', COMPILE_FAILED),
        ('bad_class3.java', COMPILE_FAILED),
        ('compile_fail.cpp', COMPILE_FAILED),
        ('compile_fail.java', COMPILE_FAILED),
        ('do_nothing.cpp', JUDGE),
        ('do_nothing.py', JUDGE),
        ('do_nothing_unicode.cpp', JUDGE),
        ('empty.cpp', COMPILE_FAILED),
        ('empty.java', COMPILE_FAILED),
        ('empty.py', JUDGE),
        ('exception.cpp', ABNORMAL),
        ('exception.java', ABNORMAL),
        ('exception.py', ABNORMAL),
        ('infinite_stream.cpp', ABNORMAL),
        ('infinite_stream.py', ABNORMAL),
        ('package.java', JUDGE),
        ('pathclass.java', COMPILE_FAILED),
        ('sleep_forever.cpp', TIME_EXCEEDED),
        ('sleep_forever.py', TIME_EXCEEDED),
        ('spin_forever.cpp', TIME_EXCEEDED),
        ('spin_forever.java', TIME_EXCEEDED),
        ('spin_forever.py', TIME_EXCEEDED),
        ('wrong_retcode.cpp', ABNORMAL),
        ('wrong_retcode.py', ABNORMAL),
        ('exact.py', CORRECT),
        ('whitespace.py', CORRECT)
        ]

submit_id = 1
for (filename, state) in files:
    (base, ext) = os.path.splitext(filename)
    print("submit")
    print("prob_id:1")
    print("lang:" + exts[ext])
    print("<tests/solutions/" + filename)
    print("?ok");
    print("?submission_id:{}".format(submit_id))
    print("");
    submit_id += 16

print("getsubmissions")
print("?ok");
idx = 0
submit_id = 1
for (filename, state) in files:
    print(dedent("""\
            ?comment{0}:{2}
            ?contesttime{0}:*
            ?prob_id{0}:1
            ?problem{0}:test
            ?result{0}:{1}
            ?submission_id{0}:{3}
            ?group_id{0}:1
            ?time{0}:*""").format(idx, state, comments[state], submit_id))
    idx += 1
    submit_id += 16

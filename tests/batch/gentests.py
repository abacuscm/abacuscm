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
        '.py': 'Python 2.x'
}

files = [
        (1, 'bad_class.java', COMPILE_FAILED),
        (1, 'bad_class2.java', COMPILE_FAILED),
        (1, 'bad_class3.java', COMPILE_FAILED),
        (1, 'compile_fail.cpp', COMPILE_FAILED),
        (1, 'compile_fail.java', COMPILE_FAILED),
        (1, 'do_nothing.cpp', JUDGE),
        (1, 'do_nothing.py', JUDGE),
        (1, 'do_nothing_unicode.cpp', JUDGE),
        (1, 'empty.cpp', COMPILE_FAILED),
        (1, 'empty.java', COMPILE_FAILED),
        (1, 'empty.py', JUDGE),
        (1, 'exception.cpp', ABNORMAL),
        (1, 'exception.java', ABNORMAL),
        (1, 'exception.py', ABNORMAL),
        (1, 'infinite_stream.cpp', ABNORMAL),
        (1, 'infinite_stream.py', ABNORMAL),
        (1, 'package.java', JUDGE),
        (1, 'pathclass.java', COMPILE_FAILED),
        (1, 'sleep_forever.cpp', TIME_EXCEEDED),
        (1, 'sleep_forever.py', TIME_EXCEEDED),
        (1, 'spin_forever.cpp', TIME_EXCEEDED),
        (1, 'spin_forever.java', TIME_EXCEEDED),
        (1, 'spin_forever.py', TIME_EXCEEDED),
        (1, 'wrong_retcode.cpp', ABNORMAL),
        (1, 'wrong_retcode.py', ABNORMAL),
        (1, 'exact.py', CORRECT),
        (1, 'whitespace.py', CORRECT),
        (33, 'do_nothing.cpp', JUDGE),
        (33, 'whitespace.py', CORRECT)
        ]

submit_id = 1
for (prob_id, filename, state) in files:
    (base, ext) = os.path.splitext(filename)
    print("submit")
    print("prob_id:{}".format(prob_id))
    print("lang:" + exts[ext])
    print("<tests/solutions/" + filename)
    print("?ok");
    print("?submission_id:{}".format(submit_id))
    print("");
    submit_id += 16

print("getsubmissions")
print("?ok");
idx = 0
names = {1: 'test', 33: 'test3'}
submit_id = 1
for (prob_id, filename, state) in files:
    print(dedent("""\
            ?comment{0}:{2}
            ?contesttime{0}:*
            ?prob_id{0}:{4}
            ?problem{0}:{5}
            ?result{0}:{1}
            ?submission_id{0}:{3}
            ?group_id{0}:1
            ?time{0}:*""").format(idx, state, comments[state], submit_id, prob_id, names[prob_id]))
    idx += 1
    submit_id += 16

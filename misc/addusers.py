#!/usr/bin/python
import sys
import os
import subprocess
import argparse
import re
import time

cmd = ['bin/abacustool', '-c', 'abacus.conf', '-s', 'conf/server.conf', 'adduser']

parser = argparse.ArgumentParser()
parser.add_argument('--group', '-g', metavar = 'GROUP')
parser.add_argument('userfile')
args = parser.parse_args()

userfile = args.userfile
if not os.path.isfile(userfile):
    usage('%s is not a file' % userfile)

if not os.path.isfile(cmd[0]):
    usage('Please run from the abacus top-level directory')

users = []
f = file(userfile, 'r')
for line in f:
    line = line.rstrip()
    matches = re.match('^([a-z0-9_]+) ([a-zA-Z0-9]+) ([a-z]+) (.+)$', line)
    if matches:
        new_username = matches.group(1)
        new_password = matches.group(2)
        new_type     = matches.group(3)
        new_realname = matches.group(4)

        argv = cmd + [new_username, new_realname, new_password, new_type]
        if args.group:
            argv.append(args.group)
        print 'Adding %s => %s identified by %s as type %s' % (new_username, new_realname, new_password, new_type)
        ret = subprocess.call(argv)
        if ret != 0:
            sys.exit(ret)
    else:
        print 'Skipping line \'%s\' which is not in a format that we expect.' % line

#!/usr/bin/env python3

"""Populates the abacus server with initial configuration.

This is complicated by the need to be idempotent. That is currently handled
by keeping a list of successfully executed commands and not executing them
again.
"""

import subprocess
import functools
import pickle
import dbm.ndbm
import string
import configparser
import os
import os.path
import random
import textwrap
import inichange

_cache = None
_cache_file = '/data/abacus/runcache'
STANDINGS_PWFILE = '/data/abacus/standings.pw'
STANDINGS_CONF = '/usr/src/abacuscm/docker/standings.conf'
STANDINGS_FILE = '/data/www/standings/standings.txt'
SERVER_CONF = '/data/abacus/server.conf'
ADMIN_CONF = '/usr/src/abacuscm/docker/admin.conf'

def open_cache():
    global _cache
    if _cache is None:
        _cache = dbm.ndbm.open(_cache_file, 'c', 0o600)

def run_once(func):
    """Decorator to ensure `func` is only run once with a particular
    set of arguments."""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        open_cache()
        key = pickle.dumps((func.__name__, args, kwargs))
        if key not in _cache:
            func(*args, **kwargs)
            _cache[key] = '1'
    return wrapper

def abacustool(*args):
    subprocess.check_call(['/usr/bin/abacustool',
        '-c', ADMIN_CONF, '-s', SERVER_CONF] + list(args))

@run_once
def add_user(username, name, password, usertype):
    abacustool('adduser', username, name, password, usertype)

def get_conf(filename, section, value):
    cp = configparser.RawConfigParser(
            delimiters=('=',),
            comment_prefixes=('#',),
            default_section=None)
    cp.optionxform = str
    with open(filename, 'r') as f:
        cp.read_file(f)
    return cp[section][value]

def generate_standings_passwd():
    if not os.path.exists(STANDINGS_PWFILE):
        r = random.SystemRandom()
        password = ''.join([r.choice(string.ascii_letters) for i in range(16)])
        with open(STANDINGS_PWFILE, 'w', 0o600) as f:
            f.write(password)

def get_standings_passwd():
    with open(STANDINGS_PWFILE, 'r') as f:
        return f.read()

def run_standings():
    os.execv('/usr/bin/abacustool', [
        '/usr/bin/abacustool', '-c', STANDINGS_CONF, '-P', STANDINGS_PWFILE,
        '-u', 'standings', 'standings', STANDINGS_FILE])

def main():
    generate_standings_passwd()
    add_user('standings', 'Standings bot', get_standings_passwd(), 'contestant')
    run_standings()

if __name__ == "__main__":
    main()

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
import os.path
import random
import textwrap
import inichange

_cache = None
_cache_file = '/data/abacus/runcache'
STANDINGS_CONF = '/data/abacus/standings.conf'
STANDINGS_CONF_SOURCE = '/usr/src/abacuscm/docker/standings.conf'
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

@run_once
def abacustool(*args):
    subprocess.check_call(['/usr/bin/abacustool',
        '-c', ADMIN_CONF, '-s', SERVER_CONF] + list(args))

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

def generate_standings_conf():
    if not os.path.exists(STANDINGS_CONF):
        r = random.SystemRandom()
        password = ''.join([r.choice(string.ascii_letters) for i in range(16)])
        cp = inichange.merge([STANDINGS_CONF_SOURCE], [('server', 'password', password)])
        with open(STANDINGS_CONF, 'w', 0o600) as f:
            cp.write(f)

def main():
    generate_admin_conf()
    generate_standings_conf()
    add_user('standings', 'Standings bot', get_conf(STANDINGS_CONF, 'server', 'password'), 'contestant')

if __name__ == "__main__":
    main()

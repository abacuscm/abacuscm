#!/usr/bin/env python3

import subprocess
import signal
import os
import os.path
import time
import sys
import argparse
import shutil
import string
import textwrap
import tempfile
import inichange
import warnings
import functools
import random
import pickle
import logging
import dbm.ndbm
from contextlib import contextmanager, closing

LIB_DIR = '/var/lib/abacuscm'
CONF_DIR = '/conf'
DATA_DIR = '/data'
CONTEST_DIR = '/contest'
JETTY_DIR = '/etc/jetty9'

JETTY_CERT_DIR = os.path.join(CONF_DIR, 'jetty-certs')
ABACUS_CERT_DIR = os.path.join(CONF_DIR, 'abacus-certs')
CA_CERT = os.path.join(ABACUS_CERT_DIR, 'cacert.crt')
SERVER_KEY = os.path.join(ABACUS_CERT_DIR, 'server.key')
JETTY_KEY = os.path.join(JETTY_CERT_DIR, 'jetty.key')
JETTY_CERT = os.path.join(JETTY_CERT_DIR, 'jetty.crt')
TRUSTSTORE = os.path.join(JETTY_DIR, 'abacus_keystore')
KEYSTORE = os.path.join(JETTY_DIR, 'keystore')
DEFAULT_DAYS = 365    # Certificate validity for new certs

MYSQL_DIR = os.path.join(DATA_DIR, 'mysql')
MYSQL_DB_DIR = os.path.join(MYSQL_DIR, 'db')
SERVER_CONF = os.path.join(DATA_DIR, 'abacus', 'server.conf')
MARKER_CONF = os.path.join(DATA_DIR, 'abacus', 'marker.conf')
SERVER_LOG = os.path.join(DATA_DIR, 'abacus', 'abacusd.log')
MARKER_LOG = os.path.join(DATA_DIR, 'abacus', 'markerd.log')
ADMIN_CONF = os.path.join(LIB_DIR, 'admin.conf')
STANDINGS_PWFILE = os.path.join(DATA_DIR, 'abacus', 'standings.pw')
STANDINGS_DIR = os.path.join(DATA_DIR, 'standings')

_CACHE_FILE = os.path.join(DATA_DIR, 'abacus', 'runcache')

def run_once(func):
    """Decorator to ensure `func` is only run once with a particular
    set of arguments."""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        cache = dbm.ndbm.open(_CACHE_FILE, 'c', 0o600)
        with closing(cache):
            key = pickle.dumps((func.__name__, args, kwargs))
            if key not in cache:
                func(*args, **kwargs)
                cache[key] = '1'
    return wrapper

def abacustool(*args):
    logging.debug('Running abacustool [%s]', args)
    subprocess.check_call(['/usr/bin/abacustool',
        '-c', ADMIN_CONF, '-s', SERVER_CONF] + list(args))

@contextmanager
def umask(mask):
    old = os.umask(mask)
    yield
    os.umask(old)

def mkdir_p(path, mode=0o0777):
    if not os.path.isdir(path):
        os.makedirs(path, mode)

def create_key(out, args):
    """Create x509 private key"""
    logging.debug('Creating x509 private key')
    subprocess.check_call(['openssl', 'genrsa', '-out', out, '4096'])

def create_self_cert(subj, key, out, args):
    """Create a self-signed certificate"""
    logging.debug('Creating self-signed certificate')
    subprocess.check_call([
        'openssl', 'req', '-new', '-x509', '-key', key,
        '-days', str(args.days), '-subj', subj, '-out', out])

def create_csr(subj, key, out, args):
    """Create a certificate signing request"""
    logging.debug('Creating certificate signing request')
    subprocess.check_call([
        'openssl', 'req', '-new', '-key', key, '-subj', subj, '-out', out])

def create_cert(csr, ca_key, ca_cert, ca_serial, out, args):
    logging.debug('Creating certificate')
    subprocess.check_call([
        'openssl', 'x509', '-req', '-in', csr,
        '-CAcreateserial', '-days', str(args.days),
        '-CA', ca_cert, '-CAkey', ca_key, '-CAserial', ca_serial,
        '-out', out])

def create_jks_truststore(cert, truststore, alias, password):
    logging.debug('Creating JKS trust store')
    subprocess.check_call([
        'keytool', '-importcert', '-alias', 'abacuscert',
        '-file', cert, '-keystore', truststore, '-storepass', password,
        '-noprompt'])

def create_jks_keystore(cert, key, keystore, password):
    logging.debug('Creating JKS key store')
    with umask(0o077):
        (handle, pkcs12) = tempfile.mkstemp('.pkcs12')
        try:
            os.close(handle)
            subprocess.check_call([
                'openssl', 'pkcs12', '-inkey', key, '-in', cert, '-export',
                '-out', pkcs12, '-passout', 'pass:' + password])
            subprocess.check_call([
                'keytool', '-importkeystore', '-srckeystore', pkcs12,
                '-srcstoretype', 'PKCS12', '-destkeystore', keystore,
                '-srcstorepass', password, '-storepass', password])
        finally:
            os.unlink(pkcs12)

def is_mysql_running():
    logging.debug('Checking whether mysql is running')
    with open('/dev/null', 'w') as devnull:
        ret = subprocess.call(['/usr/bin/mysqladmin', 'ping'],
                stdin=devnull, stdout=devnull, stderr=devnull)
    logging.debug('MySQL %s running', 'is' if ret == 0 else 'is not')
    return ret == 0

@contextmanager
def run_mysql():
    """Start mysql server and wait for it to be ready; on context
    exit, shut down the server again."""
    logging.debug('Starting MySQL')
    proc = subprocess.Popen(['/usr/bin/mysqld_safe', '--skip-syslog'],
            stdin=subprocess.PIPE, close_fds=True, cwd='/usr')
    running = False
    for i in range(20):
        if is_mysql_running():
            running = True
            break
        time.sleep(0.5)
    if not running:
        proc.terminate()
        raise RuntimeError('mysqld did not start successfully')
    logging.debug('MySQL running')

    yield

    logging.debug('Stopping MySQL')
    subprocess.check_call(['/usr/bin/mysqladmin', 'shutdown'])
    proc.wait()
    logging.debug('MySQL terminated')

def is_abacusd_running():
    """Log in as admin to check whether abacusd is running"""
    logging.debug('Check whether abacusd is running')
    proc = subprocess.Popen(['/usr/bin/batch', ADMIN_CONF],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    cp = inichange.merge([SERVER_CONF])
    commands = textwrap.dedent('''\
            auth
            user:admin
            pass:{password}
            ?ok
            ?user:admin

            '''.format(password=cp['initialisation']['admin_password']))
    (stdout, stderr) = proc.communicate(bytes(commands, 'utf-8'))
    logging.debug('abacusd %s running', 'is' if proc.returncode == 0 else 'is not')
    return proc.returncode == 0

@contextmanager
def run_abacusd():
    """Start abacusd and wait for it to be ready; on context exit, shut down
    the server again."""
    logging.debug('Starting abacusd')
    # See http://stackoverflow.com/questions/34337840/cant-terminate-a-sudo-process-created-with-python-in-ubuntu-15-10
    # for an explanation of why os.setpgrp is used.
    proc = subprocess.Popen(['sudo', '-u', 'abacus', '--', '/usr/bin/abacusd', SERVER_CONF],
            stdin=subprocess.PIPE, close_fds=True, cwd='/', preexec_fn=os.setpgrp)
    # Wait until the socket is open
    running = False
    for i in range(10):
        if is_abacusd_running():
            running = True
            break
        time.sleep(0.5)
    if not running:
        proc.terminate()
        raise RuntimeError('abacusd did not start successfully')
    logging.debug('abacusd running')

    yield

    logging.debug('Stopping abacusd')
    proc.send_signal(signal.SIGINT)
    proc.wait()
    logging.debug('abacusd shut down')

def exec_sql(commands):
    logging.debug('Executing SQL command')
    proc = subprocess.Popen(['/usr/bin/mysql', '-u', 'root', '--batch'],
            stdin=subprocess.PIPE)
    proc.communicate(bytes(commands, 'utf-8'))
    if proc.returncode:
        raise CalledProcessError(proc.retcode, 'mysql')

def make_abacus_certs(args):
    """If the abacus server key and CA do not exist, create them."""
    if not os.path.isdir(ABACUS_CERT_DIR):
        logging.debug('Creating abacus certificates')
        tmp_cert_dir = os.path.join(CONF_DIR, 'abacus-certs.tmp')
        if os.path.isdir(tmp_cert_dir):
            shutil.rmtree(tmp_cert_dir)
        os.makedirs(tmp_cert_dir)
        ca_key = os.path.join(tmp_cert_dir, 'ca.key')
        ca_cert = os.path.join(tmp_cert_dir, 'cacert.crt')
        ca_serial = os.path.join(tmp_cert_dir, 'ca.srl')
        server_key = os.path.join(tmp_cert_dir, 'server.key')
        server_cert = os.path.join(tmp_cert_dir, 'server.crt')
        server_csr = os.path.join(tmp_cert_dir, 'server.csr')
        with umask(0o077):
            create_key(ca_key, args)
            create_key(server_key, args)
        create_self_cert('/CN=abacus-cert', ca_key, ca_cert, args)
        create_csr('/CN=localhost', server_key, server_csr, args)
        create_cert(server_csr, ca_key, ca_cert, ca_serial, server_cert, args)
        os.rename(tmp_cert_dir, ABACUS_CERT_DIR)

def make_jetty_certs(args):
    """If no certificate exists for Jetty, create a self-signed one"""
    if not os.path.isdir(JETTY_CERT_DIR):
        logging.debug('Creating Jetty certificates')
        if args.server is None:
            msg = textwrap.dedent("""\
                No Jetty certificate found and --server not specified.
                The generated certificate will be for localhost.""")
            warnings.warn(msg)
            hostname = 'localhost'
        else:
            hostname = args.server
        tmp_cert_dir = os.path.join(CONF_DIR, 'jetty-certs.tmp')
        if os.path.isdir(tmp_cert_dir):
            shutil.rmtree(tmp_cert_dir)
        os.makedirs(tmp_cert_dir)
        jetty_key = os.path.join(tmp_cert_dir, 'jetty.key')
        jetty_cert = os.path.join(tmp_cert_dir, 'jetty.crt')
        with umask(0o077):
            create_key(jetty_key, args)
        create_self_cert('/CN=' + hostname, jetty_key, jetty_cert, args)
        os.rename(tmp_cert_dir, JETTY_CERT_DIR)

def random_password():
    r = random.SystemRandom()
    return ''.join([r.choice(string.ascii_letters) for i in range(16)])

def make_password(pwfile):
    if not os.path.exists(pwfile):
        logging.debug('Creating password in %s', pwfile)
        r = random.SystemRandom()
        password = ''.join([r.choice(string.ascii_letters) for i in range(16)])
        with open(pwfile, 'w', 0o600) as f:
            f.write(password)

def make_mysql(args):
    """Initialise mysql db if it does not exist"""
    if not os.path.isdir(os.path.join(MYSQL_DB_DIR)):
        logging.debug('Initialising MySQL')
        fix_permission(MYSQL_DIR, 'mysql', 'mysql')
        subprocess.check_call(['mysql_install_db', '--datadir', MYSQL_DB_DIR, '--user', 'mysql'])
        logging.debug('MySQL initialised')
    if not os.path.isdir(os.path.join(MYSQL_DB_DIR, 'abacus')):
        commands = [textwrap.dedent("""\
            create database abacus;
            grant all privileges on abacus.* to abacus@localhost identified by 'abacus';
            use abacus;
            """)]
        with open(os.path.join(LIB_DIR, 'structure.sql')) as structure:
            commands.extend(list(structure))
        commands = ''.join(commands)
        with run_mysql():
            subprocess.check_call(['mysql', '--execute', commands])
        logging.debug('Abacus database initialised')

def make_server_conf(args):
    """Merges override configuration with preconfigured values"""
    logging.debug('Creating server config')
    filenames = [os.path.join(LIB_DIR, 'server.conf')]
    for path in [os.path.join(CONF_DIR, 'abacus'), CONTEST_DIR]:
        filename = os.path.join(path, 'server.conf.override')
        if os.path.isfile(filename):
            filenames.append(filename)
    cp = inichange.merge(filenames)
    with umask(0o077), open(SERVER_CONF, 'w') as outf:
        cp.write(outf)
    shutil.chown(SERVER_CONF, 'abacus', 'abacus')

def make_marker_conf(args):
    """Merges override configuration for a marker with preconfigured values"""
    logging.debug('Creating marker config')
    filenames = [os.path.join(LIB_DIR, 'marker.conf')]
    override_file = os.path.join(CONF_DIR, 'abacus', 'marker.conf.override')
    overrides = []
    if os.path.isfile(override_file):
        filenames.append(override_file)
    if args.server is not None:
        overrides.append(('server', 'address', args.server))
    cp = inichange.merge(filenames, overrides)
    with umask(0o077), open(MARKER_CONF, 'w') as outf:
        cp.write(outf)
    shutil.chown(MARKER_CONF, 'abacus', 'abacus')

def make_server_crypto(args):
    """Build the keystore from the abacus certificate, and generate IVs. This
    is done every time, because it is not stored persistently.
    """
    logging.debug('Preparing server crypto')
    if os.path.exists(KEYSTORE):
        os.remove(KEYSTORE)
    if os.path.exists(TRUSTSTORE):
        os.remove(TRUSTSTORE)
    password = 'password'
    create_jks_truststore(CA_CERT, TRUSTSTORE, 'abacuscert', password)
    create_jks_keystore(JETTY_CERT, JETTY_KEY, KEYSTORE, password)
    shutil.chown(KEYSTORE, 'jetty', 'jetty')

    mkdir_p('/etc/abacus', 0o700)
    shutil.chown('/etc/abacus', 'abacus', 'abacus')
    for (filename, size) in [('/etc/abacus/rijndael.key', 32), ('/etc/abacus/rijndael.iv', 16)]:
        with open(filename, 'wb') as f:
            f.write(b'\0' * size)
    # This needs to be readable by abacus, but we don't want the copy
    # accessible on the host to be readable by whichever random user
    # that corresponds to.
    shutil.copy(SERVER_KEY, '/etc/abacus/server.key')
    shutil.chown('/etc/abacus/server.key', 'abacus', 'abacus')

@run_once
def add_user(username, name, password, usertype):
    abacustool('adduser', username, name, password, usertype)

def add_marker_user():
    # TODO: share this logic with make_marker_conf
    filenames = [os.path.join(LIB_DIR, 'marker.conf')]
    override_file = os.path.join(CONF_DIR, 'abacus', 'marker.conf.override')
    overrides = []
    if os.path.isfile(override_file):
        filenames.append(override_file)
    else:
        warnings.warn('No marker override file found: using default password for marker')
    cp = inichange.merge(filenames)
    username = cp['server']['username']
    password = cp['server']['password']
    add_user(username, 'Marker bot', password, 'marker')

def add_standings_user():
    with open(STANDINGS_PWFILE, 'r') as f:
        password = f.read()
    add_user('standings', 'Standings bot', password, 'contestant')

def adjust_https_port(args):
    logging.debug('Adjusting Jetty to use port %d on host', args.https_port)
    with open(os.path.join(JETTY_DIR, 'start.d', 'https-port.ini'), 'w') as f:
        print('jetty.secure.port={}'.format(args.https_port), file=f)

def suid_runlimit(args):
    logging.debug('Setting permissions on runlimit')
    shutil.chown('/usr/bin/runlimit', 'root', 'abacus')
    os.chmod('/usr/bin/runlimit', 0o4550)

def install_supervisor_conf(basename, args):
    logging.debug('Installing supervisord config')
    shutil.copy(os.path.join(LIB_DIR, basename),
                os.path.join('/etc/supervisor/conf.d', basename))

def mkdir_logs(services):
    for service in ['supervisor', 'mysql', 'jetty9', 'abacus']:
        mkdir_p(os.path.join(DATA_DIR, service, 'log'))

def mkdir_standings():
    mkdir_p(STANDINGS_DIR)

def fix_permission(path, user, group, mode='go-rwx'):
    if os.path.isdir(path):
        subprocess.check_call(['/bin/chown', '-R', '{}:{}'.format(user, group), '--', path])
        subprocess.check_call(['/bin/chmod', '-R', mode, '--', path])

def fix_permissions(args):
    shutil.chown(DATA_DIR, 'root', 'root')
    os.chmod(DATA_DIR, 0o755)
    fix_permission(os.path.join(DATA_DIR, 'abacus'), 'abacus', 'abacus')
    fix_permission(os.path.join(DATA_DIR, 'jetty9'), 'jetty', 'jetty')
    fix_permission(os.path.join(DATA_DIR, 'supervisor'), 'root', 'root')
    fix_permission(os.path.join(DATA_DIR, 'mysql'), 'mysql', 'mysql')

    shutil.chown(CONF_DIR, 'root', 'root')
    os.chmod(CONF_DIR, 0o755)
    fix_permission(os.path.join(CONF_DIR, 'abacus'), 'abacus', 'abacus')

    shutil.chown(CONTEST_DIR, 'abacus', 'abacus')
    os.chmod(CONTEST_DIR, 0o700)

    fix_permission(STANDINGS_DIR, 'abacus', 'abacus', 'a+rX')

def exec_supervisor(args):
    logging.debug('Launching supervisord')
    os.execv('/usr/bin/supervisord',
            ['/usr/bin/supervisord', '-n', '-c', '/etc/supervisor/supervisord.conf'])

def run_shell(args):
    os.execv('/bin/bash', ['/bin/bash'] + args.args)

def run_server(args):
    mkdir_logs(['supervisor', 'mysql', 'jetty9', 'abacus'])
    mkdir_standings()
    make_abacus_certs(args)
    make_jetty_certs(args)
    make_mysql(args)
    make_server_conf(args)
    make_server_crypto(args)
    make_password(STANDINGS_PWFILE)
    adjust_https_port(args)
    install_supervisor_conf('server-supervisord.conf', args)
    fix_permissions(args)
    with run_mysql():
        with run_abacusd():
            add_standings_user()
            add_marker_user()
    exec_supervisor(args)

def run_marker(args):
    if not os.path.isdir(ABACUS_CERT_DIR):
        print("No abacus certificates found in {}.".format(ABACUS_CERT_DIR),
                file=sys.stderr)
        sys.exit(1)

    mkdir_logs(['supervisor', 'abacus'])
    make_marker_conf(args)
    suid_runlimit(args)
    install_supervisor_conf('marker-supervisord.conf', args)
    fix_permissions(args)
    exec_supervisor(args)

def main():
    logging.basicConfig(level='INFO')
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    p = subparsers.add_parser('server')
    p.add_argument('--days', type=int, default=3650, help='Number of days validity for new certificates')
    p.add_argument('--server', metavar='HOSTNAME', help='Hostname for generated web server certificate')
    p.add_argument('--https-port', type=int, default=443, metavar='PORT', help='Port number for HTTPS on the host')
    p.set_defaults(func=run_server)

    p = subparsers.add_parser('marker')
    p.add_argument('--server', metavar='HOSTNAME', help='Abacus server to connect to')
    p.set_defaults(func=run_marker)

    p = subparsers.add_parser('shell')
    p.add_argument('args', nargs=argparse.REMAINDER)
    p.set_defaults(func=run_shell)

    args = parser.parse_args()
    if not hasattr(args, 'func'):
        run_shell(argparse.Namespace(args=[]))
    args.func(args)

if __name__ == '__main__':
    main()

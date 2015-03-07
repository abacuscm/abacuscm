#!/usr/bin/env python3

import subprocess
import os
import os.path
import time
import sys
import argparse
import shutil
import textwrap
import tempfile
import inichange
import warnings
from contextlib import contextmanager

SRC_DIR = '/usr/src/abacuscm'
CONF_DIR = '/conf'
DATA_DIR = '/data'
CONTEST_DIR = '/contest'
JETTY_DIR = '/etc/jetty8'

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
SERVER_CONF = os.path.join(DATA_DIR, 'abacus', 'server.conf')
MARKER_CONF = os.path.join(DATA_DIR, 'abacus', 'marker.conf')
SERVER_LOG = os.path.join(DATA_DIR, 'abacus', 'abacusd.log')
MARKER_LOG = os.path.join(DATA_DIR, 'abacus', 'markerd.log')

WWW_DIR = os.path.join(DATA_DIR, 'www')
STANDINGS_DIR = os.path.join(WWW_DIR, 'standings')

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
    subprocess.check_call(['openssl', 'genrsa', '-out', out, '4096'])

def create_self_cert(subj, key, out, args):
    """Create a self-signed certificate"""
    subprocess.check_call([
        'openssl', 'req', '-new', '-x509', '-key', key,
        '-days', str(args.days), '-subj', subj, '-out', out])

def create_csr(subj, key, out, args):
    """Create a certificate signing request"""
    subprocess.check_call([
        'openssl', 'req', '-new', '-key', key, '-subj', subj, '-out', out])

def create_cert(csr, ca_key, ca_cert, ca_serial, out, args):
    subprocess.check_call([
        'openssl', 'x509', '-req', '-in', csr,
        '-CAcreateserial', '-days', str(args.days),
        '-CA', ca_cert, '-CAkey', ca_key, '-CAserial', ca_serial,
        '-out', out])

def create_jks_truststore(cert, truststore, alias, password):
    subprocess.check_call([
        'keytool', '-importcert', '-alias', 'abacuscert',
        '-file', cert, '-keystore', truststore, '-storepass', password,
        '-noprompt'])

def create_jks_keystore(cert, key, keystore, password):
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

@contextmanager
def run_mysql():
    """Start mysql server and wait for it to be ready; on context
    exit, shut down the server again."""
    proc = subprocess.Popen(['/usr/bin/mysqld_safe', '--skip-syslog'],
            stdin=subprocess.PIPE, close_fds=True, cwd='/usr')
    running = False
    for i in range(20):
        with open('/dev/null', 'w') as sink:
            ret = subprocess.call(['/usr/bin/mysqladmin', 'ping'], stdout=sink, stderr=sink)
        if ret == 0:
            running = True
            break
        time.sleep(0.5)
    if not running:
        proc.terminate()
        raise RuntimeError('mysqld did not start successfully')

    yield

    subprocess.check_call(['/usr/bin/mysqladmin', 'shutdown'])
    proc.wait()

def exec_sql(commands):
    proc = subprocess.Popen(['/usr/bin/mysql', '-u', 'root', '--batch'],
            stdin=subprocess.PIPE)
    proc.communicate(bytes(commands, 'utf-8'))
    if proc.returncode:
        raise CalledProcessError(proc.retcode, 'mysql')

def make_abacus_certs(args):
    """If the abacus server key and CA do not exist, create them."""
    if not os.path.isdir(ABACUS_CERT_DIR):
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

def make_mysql(args):
    """Initialise mysql db if it does not exist"""
    if not os.path.isdir(os.path.join(MYSQL_DIR, 'mysql')):
        subprocess.check_call(['mysql_install_db', '--user', 'mysql'])
        with run_mysql():
            # TODO: maybe use Python MySQL bindings for this?
            commands = [textwrap.dedent("""\
                create database abacus;
                grant all privileges on abacus.* to abacus@localhost identified by 'abacus';
                use abacus;
                """)]
            with open(os.path.join(SRC_DIR, 'db', 'structure.sql')) as structure:
                commands.extend(list(structure))
            exec_sql(''.join(commands))

def make_server_conf(args):
    """Merges override configuration with preconfigured values"""
    filenames = [os.path.join(SRC_DIR, 'docker', 'server.conf')]
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
    filenames = [os.path.join(SRC_DIR, 'docker', 'marker.conf')]
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
    if os.path.exists(KEYSTORE):
        os.remove(KEYSTORE)
    if os.path.exists(TRUSTSTORE):
        os.remove(TRUSTSTORE)
    password = 'password'
    create_jks_truststore(CA_CERT, TRUSTSTORE, 'abacuscert', password)
    create_jks_keystore(JETTY_CERT, JETTY_KEY, KEYSTORE, password)
    shutil.chown(KEYSTORE, 'jetty', 'jetty')
    subprocess.check_call([
        'sed', '-i', 's/OBF:[a-zA-Z0-9]*/password/g', '/etc/jetty8/jetty-ssl.xml'])

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

def adjust_https_port(args):
    subprocess.check_call([
        'sed', '-i',
        's!<Set name="confidentialPort">[0-9]*</Set>' + \
            '!<Set name="confidentialPort">{}</Set>!'.format(args.https_port),
        os.path.join(JETTY_DIR, 'jetty.xml')])

def suid_runlimit(args):
    shutil.chown('/usr/bin/runlimit', 'abacus', 'abacus')
    os.chmod('/usr/bin/runlimit', 0o4550)

def install_supervisor_conf(basename, args):
    shutil.copy(os.path.join(SRC_DIR, 'docker', basename),
            os.path.join('/etc/supervisor/conf.d', basename))

def mkdir_logs(services):
    for service in ['supervisor', 'mysql', 'jetty8', 'abacus']:
        mkdir_p(os.path.join(DATA_DIR, service, 'log'))

def mkdir_www():
    mkdir_p(WWW_DIR)
    mkdir_p(STANDINGS_DIR)

def fix_permission(path, user, group, mode='go-rwx'):
    if os.path.isdir(path):
        subprocess.check_call(['/bin/chown', '-R', '{}:{}'.format(user, group), '--', path])
        subprocess.check_call(['/bin/chmod', '-R', mode, '--', path])

def fix_permissions(args):
    shutil.chown(DATA_DIR, 'root', 'root')
    os.chmod(DATA_DIR, 0o755)
    fix_permission(os.path.join(DATA_DIR, 'abacus'), 'abacus', 'abacus')
    fix_permission(os.path.join(DATA_DIR, 'jetty8'), 'jetty', 'jetty')
    fix_permission(os.path.join(DATA_DIR, 'supervisor'), 'root', 'root')
    fix_permission(os.path.join(DATA_DIR, 'mysql'), 'mysql', 'mysql')

    shutil.chown(CONF_DIR, 'root', 'root')
    os.chmod(CONF_DIR, 0o755)
    fix_permission(os.path.join(CONF_DIR, 'abacus'), 'abacus', 'abacus')

    shutil.chown(CONTEST_DIR, 'abacus', 'abacus')
    os.chmod(CONTEST_DIR, 0o700)

    fix_permission(WWW_DIR, 'jetty', 'jetty', 'a+rX')
    fix_permission(STANDINGS_DIR, 'abacus', 'abacus', 'a+rX')

def exec_supervisor(args):
    os.execv('/usr/bin/supervisord',
            ['/usr/bin/supervisord', '-n', '-c', '/etc/supervisor/supervisord.conf'])

def run_shell(args):
    os.execv('/bin/bash', ['/bin/bash'] + args.args)

def run_server(args):
    mkdir_logs(['supervisor', 'mysql', 'jetty8', 'abacus'])
    mkdir_www()
    make_abacus_certs(args)
    make_jetty_certs(args)
    make_mysql(args)
    make_server_conf(args)
    make_server_crypto(args)
    adjust_https_port(args)
    install_supervisor_conf('server-supervisord.conf', args)
    fix_permissions(args)
    exec_supervisor(args)

def run_marker(args):
    if not os.path.isdir(ABACUS_CERT_DIR):
        print("No abacus certificates found in {}.".format(ABACUS_CERTS_DIR),
                file=sys.stderr)
        sys.exit(1)

    mkdir_logs(['supervisor', 'abacus'])
    make_marker_conf(args)
    suid_runlimit(args)
    install_supervisor_conf('marker-supervisord.conf', args)
    fix_permissions(args)
    exec_supervisor(args)

def main():
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

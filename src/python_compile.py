#!/usr/bin/env python3

"""
Use PyInstaller to create a stand-alone directory for a Python script, which
can be run as a chroot jail. It thus requires all library files to be copied
in, including ones that PyInstaller doesn't handle (such as libc).
"""

from __future__ import print_function, division, absolute_import, unicode_literals
import re
import subprocess
import shutil
import sys
import argparse
import glob
import os
from distutils.spawn import find_executable


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('script')
    parser.add_argument('output_dir')
    args = parser.parse_args()

    pyinstaller = find_executable('pyinstaller')
    if pyinstaller is None:
        print('Could not find pyinstaller in PATH', file=sys.stderr)
        sys.exit(1)
    build_dir = os.path.join(args.output_dir, 'build')
    pyinstaller_env = dict(os.environ)
    pyinstaller_env['HOME'] = build_dir   # To keep cache files local
    subprocess.check_call([
        sys.executable, pyinstaller,
        '--clean', '--noconfirm', '--name=userprog', '--strip',
        '--workpath=' + build_dir,
        '--specpath=' + build_dir,
        '--distpath=' + args.output_dir,
        args.script], env=pyinstaller_env)

    dep_re = re.compile('^\t(?:(.+) => )?(/.*) \\(0x[a-f0-9]+\\)$')
    output_dir = os.path.join(args.output_dir, 'userprog')
    req = {}
    libs = glob.glob(os.path.join(output_dir, '*.so*'))
    for so in libs:
        deps = subprocess.check_output(['ldd', '--', so]).decode('utf-8')
        deps = deps.splitlines()
        for line in deps:
            match = dep_re.match(line)
            if match:
                filename = match.group(2)
                soname = match.group(1) if match.group(1) else filename
                req[soname] = filename
    # PyInstaller will have copied some of the libraries already
    for so in libs:
        soname = os.path.basename(so)
        if soname in req:
            del req[soname]
    # Now copy in the extra libraries
    for (soname, filename) in req.items():
        if os.path.isabs(soname):
            # This is typical for the dynamic linker itself
            dest = os.path.relpath(soname, '/')  # Turn into a relative path
        else:
            dest = soname
        dest = os.path.join(output_dir, dest)
        dest_dir = os.path.dirname(dest)
        if not os.path.isdir(dest_dir):
            os.makedirs(dest_dir)
        shutil.copy(filename, dest)


if __name__ == '__main__':
    main()

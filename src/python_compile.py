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

    cxfreeze = find_executable('cxfreeze')
    if cxfreeze is None:
        print('Could not find cxfreeze in PATH', file=sys.stderr)
        sys.exit(1)
    subprocess.check_call([
        sys.executable, cxfreeze,
        '--target-dir=' + args.output_dir,
        '--target-name=userprog',
        args.script])

    dep_re = re.compile('^\t(?:(.+) => )?(/.*) \\(0x[a-f0-9]+\\)$')
    req = {}
    libs = glob.glob(os.path.join(args.output_dir, '*.so*'))
    for so in libs:
        deps = subprocess.check_output(['ldd', '--', so]).decode('utf-8')
        deps = deps.splitlines()
        for line in deps:
            match = dep_re.match(line)
            if match:
                filename = match.group(2)
                soname = match.group(1) if match.group(1) else filename
                req[soname] = filename
    # cxfreeze will have copied some of the libraries already
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
        dest = os.path.join(args.output_dir, dest)
        dest_dir = os.path.dirname(dest)
        if not os.path.isdir(dest_dir):
            os.makedirs(dest_dir)
        shutil.copy(filename, dest)
    # Create a /dev so that runlimit will provide /dev/urandom
    os.mkdir(os.path.join(args.output_dir, "dev"))


if __name__ == '__main__':
    main()

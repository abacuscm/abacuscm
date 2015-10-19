#!/usr/bin/env python3

"""Combine settings from multiple config files, and from the command line. The
config files are in INI format, as understood by the Python configparser
module and customised to be as close to the Abacus config format as possible.
The output file will lose all comments and formatting.
"""

import configparser
import argparse
import sys

def merge(filenames, overrides=[]):
    cp = configparser.RawConfigParser(
            delimiters=('=',),
            comment_prefixes=('#',),
            default_section=None)
    cp.optionxform = str

    for filename in filenames:
        with open(filename, 'r') as f:
            cp.read_file(f)
    for (section, name, value) in overrides:
        cp.read_dict({section: {name: value}})
    return cp

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('filename', nargs='*')
    parser.add_argument('-s', '--set', action='append', default=[], metavar='SECTION.NAME=VALUE', help='override a setting from the command line')
    parser.add_argument('-o', '--output', metavar='FILENAME', help='write to file instead of stdout')
    args = parser.parse_args()

    overrides = []
    for extra in args.set:
        try:
            key, value = extra.split('=', maxsplit=1)
            section, name = key.split('.')
        except ValueError:
            print("Invalid formatting in", extra, file=sys.stderr)
            sys.exit(2)
        overrides.append((section, name, value))

    cp = merge(args.filename, overrides)
    if args.output is not None:
        with open(args.output, 'w') as outf:
            cp.write(outf)
    else:
        cp.write(sys.stdout)

if __name__ == '__main__':
    main()

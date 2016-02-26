#!/usr/bin/python
from __future__ import print_function, division, absolute_import
import sys
import os
import subprocess
import re
import time
import argparse


class NotProblemException(ValueError):
    pass

class InvalidProblemError(RuntimeError):
    pass


class Problem(object):
    def __init__(self, dirname, subdir, args):
        self.dirname = dirname
        self.subdir = subdir
        self.path = os.path.join(self.dirname, self.subdir)
        self.settings = {}
        self.settings['shortname'] = None
        self.settings['longname'] = None
        self.settings['time_limit'] = 5
        self.settings['memory_limit'] = 256
        self.settings['ignore_whitespace'] = 'Yes'
        self.settings['multi_submit'] = 'Yes' if args.multi_submit else 'No'
        self.probtype = 'tcprob'
        self.tex_lines = []

    def _load_tex(self, filename):
        with open(filename, 'r') as f:
            self.tex_lines = list(f)

    def _get_tex(self, command, type_, value_regex=None, default=None):
        if value_regex is None:
            value_regex = r'(\d+)' if type_ is int else '([^}]*)'
        regex = re.compile(r'\\' + command + r'\{' + value_regex + r'\}')
        ans = default
        for line in self.tex_lines:
            matches = regex.match(line)
            if matches:
                ans = type_(matches.group(1))
        return ans

    def _check_interactive(self, evaluator):
        if os.path.isfile(evaluator):
            # Interactive problem
            self.settings['evaluator'] = evaluator
            self.settings['multi_submit'] = 'Yes'
            del self.settings['ignore_whitespace']
            self.probtype = 'interactiveprob'

    def _check_checker(self, checker):
        if os.path.isfile(checker):
            self.settings['checker'] = checker

    def validate(self):
        for (key, value) in self.settings.iteritems():
            if value is None:
                raise InvalidProblemError('no value found for ' + key)
        for key in ['evaluator', 'checker', 'testcase.input', 'testcase.output']:
            if key in self.settings and not os.path.isfile(self.settings[key]):
                raise InvalidProblemError(self.settings[key] + ' not found')


class SBITCProblem(Problem):
    def __init__(self, dirname, subdir, args):
        super(SBITCProblem, self).__init__(dirname, subdir, args)
        matches = re.match('(\d+)([a-z]?)_', subdir)
        if not matches:
            raise NotProblemException()
        self._number = int(matches.group(1))
        self._letter = matches.group(2)
        self._load_tex(os.path.join(self.path, 'problem.tex'))
        self.settings['shortname'] = '{}{}'.format(self._number, self._letter)
        self.settings['longname'] = self._get_tex('problemtitle', str)
        self.settings['time_limit'] = self._get_tex('timelimit', int, default=self.settings['time_limit'])
        self.settings['memory_limit'] = self._get_tex('memorylimit', int, default=self.settings['memory_limit'])
        self._check_interactive(os.path.join(self.path, 'tools', 'eval.py'))
        self._check_checker(os.path.join(self.path, 'tools', 'checker.py'))
        if self.probtype == 'tcprob':
            self.settings['testcase.input'] = os.path.join(self.path, 'data.in')
            self.settings['testcase.output'] = os.path.join(self.path, 'data.out')
        if self._letter != '' and self._letter != 'a':
            prev_letter = chr(ord(self._letter) - 1)
            self.settings['dependencies'] = '{}{}'.format(self._number, prev_letter)

    def key(self):
        return (self._number, self._letter)


class ICPCSAProblem(Problem):
    def __init__(self, dirname, subdir, args):
        super(ICPCSAProblem, self).__init__(dirname, subdir, args)
        self._load_tex(os.path.join(self.path, self.subdir + '.tex'))
        self.settings['shortname'] = self._get_tex('problemcolour', str)
        self.settings['longname'] = self._get_tex('problemname', str)
        self.settings['time_limit'] = self._get_tex('timelimit', int, value_regex=r'(\d+)(?: second(?:s)?)?')
        self._number = self._get_tex('problemnumber', str)
        self._check_interactive(os.path.join(self.path, 'tools', 'eval.py'))
        self._check_checker(os.path.join(self.path, 'tools', 'checker.py'))
        if self.probtype == 'tcprob':
            self.settings['testcase.input'] = os.path.join(self.path, 'contest.in')
            self.settings['testcase.output'] = os.path.join(self.path, 'contest.out')

    def key(self):
        return self._number


PROBLEM_CLASSES = {
    'sbitc': SBITCProblem,
    'icpc-sa': ICPCSAProblem
}


def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--dryrun', action='store_true', default=False, help='Show commands but do not execute them')
    parser.add_argument('--multi-submit', action='store_true', default=False, help='Allow multiple submissions on batch problems (useful for testing)')
    parser.add_argument('-c', '--client-config', metavar='FILE', default='conf/client.conf', help='Client configuration file')
    parser.add_argument('-s', '--server-config', metavar='FILE', default='conf/server/config/abacus/server.conf.override', help='Server configuration file')
    parser.add_argument('--command', metavar='ABACUSTOOL', default='bin/abacustool', help='Path to abacustool')
    parser.add_argument('--mode', choices=PROBLEM_CLASSES.keys(), default='icpc-sa', help='Contest type')
    parser.add_argument('problemdir')
    args = parser.parse_args()

    problemdir = args.problemdir
    if not os.path.isfile(args.client_config):
        parser.error('%s is not a file' % args.client_config)
    if not os.path.isfile(args.server_config):
        parser.error('%s is not a file' % args.server_config)
    if not os.path.isdir(problemdir):
        parser.error('%s is not a directory' % problemdir)
    if not os.path.isfile(args.command):
        parser.error('Please specify the path to abacustool with --command')

    problems = []
    problem_class = PROBLEM_CLASSES[args.mode]
    for (dirname, dirs, files) in os.walk(problemdir):
        for d in dirs:
            try:
                problem = problem_class(dirname, d, args)
                problem.validate()
                problems.append(problem)
            except NotProblemException as e:
                pass
            except InvalidProblemError as e:
                print('WARNING: {}: {} (skipping)'.format(d, e.msg))

        # Prevent recursing into the subdirectories
        del dirs[:]

    # We've now gathered all the information, it's time to issue the commands
    # Add the problems in order since Abacus displays them in the order they are
    # added.
    problems.sort(key=lambda x: x.key())
    for problem in problems:
        argv = [args.command, '-c', args.client_config, '-s', args.server_config, 'addproblem', problem.probtype]
        for key, value in problem.settings.iteritems():
            argv.append(str(key))
            argv.append(str(value))
        print('Adding', problem.settings['shortname'])
        if args.dryrun:
            print(' '.join('"%s"' % s for s in argv))
        else:
            ret = subprocess.call(argv)
            if ret != 0:
                sys.exit(ret)

if __name__ == '__main__':
    main()

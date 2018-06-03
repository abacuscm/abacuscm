#!/usr/bin/env python
# Outputs exactly the data in data.out

# Import some packages to check that freezing works
import sys
import os
import collections
import contextlib

print """A data output file.
Some lines    have extra whitespace.
Some	lines have tabs.
 Some lines have leading whitespace.
Some lines have trailing whitespace.   

Some lines are blank, and there are trailing blank lines.



"""

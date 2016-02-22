#!/usr/bin/env python3
import sys
import itertools

def tokenize(filename):
    with open(filename, 'r') as f:
        for line in f:
            yield from line.split()

def token_name(token):
    return 'EOF' if token is None else "'" + token + "'"

in_file, out_file, ref_file = sys.argv[1:]
out_tokens = tokenize(out_file)
ref_tokens = tokenize(ref_file)
for ref_token, out_token in itertools.zip_longest(ref_tokens, out_tokens):
    if ref_token != out_token:
        print('Expected {} but found {}'.format(token_name(ref_token), token_name(out_token)))
        print('STATUS JUDGE Tokens differ')
        sys.exit(0)
print('STATUS CORRECT All tokens match')

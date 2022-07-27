from sys import stdin, stdout, stderr
from pytoml import load
from yaml import safe_dump
import argparse

parser = argparse.ArgumentParser(
     prog='generate_policy_from_toml_output.py',)
parser.add_argument('--defaults', help="Use 'callout: strategy' and 'logging: stderr_printf' for Loom.", action='store_true')
args = parser.parse_args()

t = load(stdin)
if args.defaults:
  # I could do this by inserting into t, but that would not give the order I want
  stdout.write('strategy: callout\n\n')
  stdout.write('logging: stderr_printf\n\n')
  #t['strategy'] = 'callout'
  #t['logging'] = 'stderr_printf'
else:
  stderr.write('\nYou should modify the policy file output: currently, it will not create any actual input\n\n')
safe_dump(t, stdout)

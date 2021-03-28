import os
import sys

args = sys.argv[1:]

print(os.getpid())
sys.stdin.readline()
os.execvp(args[0], args)

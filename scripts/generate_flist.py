#!/usr/bin/python3

# Author: Mark Sutherland
# Walk a Kraken subtree and generate a list of files matching the following:
#   - they have an old licence text (CMU)
#   - they have been changed since the initial import commit (hash to be provided)

import argparse
import os
import subprocess

def check_file_history(fname,h,ofile):
    # Run git log and check if there are any commit hashes other than the supplied root
    cmd_array = ["git","log","--oneline"] + [h+"..","--",fname]
    result = subprocess.run(cmd_array,stdout=subprocess.PIPE)
    res_lines = result.stdout.splitlines()
    if len(res_lines) > 0: # file has changed
        ofile.write(fname+'\n')

def main():
    parser = argparse.ArgumentParser(description='Generate files to rewrite licence text for in a Kraken subtree.')
    parser.add_argument('subtree',help="Absolute or relative path to the Kraken subtree to search.")
    parser.add_argument('hash',help='The commit hash containing the root commit to which all changes will be compared to.')
    parser.add_argument('ofile',type=argparse.FileType('w'),help='The output file.')

    args = parser.parse_args()

    curdir = os.getcwd()
    os.chdir(args.subtree)

    for dirpath, dirnames, filenames in os.walk('.'):
        # Only cover .hpp and .cpp files
        filtered_files = [ f for f in filenames if f.endswith(('.h','.c','.hpp','.cpp')) ]
        for f in filtered_files:
            check_file_history(os.path.join(dirpath,f),args.hash,args.ofile)

    os.chdir(curdir)

if __name__ == '__main__':
    main()

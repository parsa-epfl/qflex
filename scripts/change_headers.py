#!/usr/bin/python3

# Author: Mark Sutherland
# Replaces an old copyright block with a new one.

import argparse
from shutil import copy as cp
from os.path import join as join

licence_begin_string = 'DO-NOT-REMOVE begin-copyright-block'
licence_end_string = 'DO-NOT-REMOVE end-copyright-block'
qflex_licence = '**QFlex License' # LEARN TO SPELL, AMERICANS

def index_containing_substring(the_list, substring):
    for i, s in enumerate(the_list):
        if substring in s:
            return i
    return -1

def replace_header(fname,licence_text,delete_backups):
    # Strip newline
    fname = fname.strip('\n')
    # create backup
    cp(fname,fname+'.bak')
    with open(fname,'r') as ifh:
        # Split the file into prelude (old licence), and content
        lines = ifh.read().splitlines()
        # Check new licence already present
        if check_lines_for_qflex_licence(lines) is True:
            print("QFlex Licence already found in file",fname)
            return

        idx_begin = index_containing_substring(lines,licence_begin_string)
        idx_end = index_containing_substring(lines,licence_end_string)

        if idx_begin == -1 and idx_end == -1:
            # Prepend
            content = lines
        else:
            # Get rid of old text
            if idx_begin != 0:
                print('Old copyright block found not at beginning of file! Skipping....',fname)
                return
            content = lines[idx_end+1:]

        # Add new licence text
        new_content = licence_text + content
        content_with_newlines = [ t + '\n' for t in new_content ]

    with open(fname,'w') as ofh: # truncate
        ofh.writelines(content_with_newlines)

def main():
    parser = argparse.ArgumentParser(description='Change headers in a Kraken subtree.')
    parser.add_argument('ifiles',type=argparse.FileType('r'),help="A file containing a newline-separated list of files which will"
            " have their headers changed from CMU -> QFlex licence")
    parser.add_argument('licence',type=argparse.FileType('r'),help='The file containing the new licence text.')
    parser.add_argument('subtree',help="Absolute or relative path to the Kraken subtree.")
    parser.add_argument('--nobak',default=False,action='store_true',help='Whether to delete backups after the replacement is done.')

    args = parser.parse_args()

    # ifiles and licence are already open
    licence_text = args.licence.read().splitlines()
    CPP_COMMENT = '// '
    HASH_COMMENT = '# '
    def add_donotremoves(lines,comment_char):
        begin_text = [ comment_char + ' DO-NOT-REMOVE begin-copyright-block' ]
        end_text = [comment_char + ' DO-NOT-REMOVE end-copyright-block']
        return begin_text + lines + end_text
    licence_prepended_cpp = [ CPP_COMMENT + t for t in licence_text  ]
    licence_prepended_hash = [ HASH_COMMENT + t for t in licence_text ]
    licence_prepended_hash = add_donotremoves(licence_prepended_hash,HASH_COMMENT)
    licence_prepended_cpp = add_donotremoves(licence_prepended_cpp,CPP_COMMENT)

    for next_file in args.ifiles:
        print('Replacing header in',join(args.subtree,next_file))
        replace_header(join(args.subtree,next_file),licence_prepended_cpp,args.nobak)

if __name__ == '__main__':
    main()

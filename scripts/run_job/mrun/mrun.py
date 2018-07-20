#!/usr/bin/python

import argparse
from argparse import RawTextHelpFormatter

from mexec import executor, mrun_exit
import sys
import logging as log
import threading
import os

HELP_TEXT = \
    """
Multinode Qemu Script Version ALPHA

you can use this script for running multi-node setup and
for that you need to pass a setup file (XML) that would provide the script
with the location of your QEMU instance configurations. if you don't have the
XML files ready, you can use the generate command as explained below to generate
sample files that would make your life easier.
"""

GENHLP = \
    """
generate sample files for use with this script.
i.e. qemu_instance_sample_file and qemu_setup_sample_file
"""
CONHLP = \
    """
Convert the text provided into a XML instance file. this options takes two arguments.
the first is the text you wish to use - please use quotes for defining your command as spaces are
prone to causing misunderstandings. the second is your output file.
"""
RUNHLP = \
    """
run multinode emulation using the setup file provided
"""
OUTHLP = \
    """
Output the commands used by this script for running the instances. useful for debugging purposes
"""
LDHLP = \
    """
Loads the snapshot provided. it will automatically append the instance name for instance specific load.
NOTE: this uses loadext only
"""
QHLP = \
    """
used in conjucnction wih the -r command for setting a quantum
which will be used to determine the desired quantum lock-step execution
- this value represents the number of instruction that each qemu executes
before switching to the next instance.
"""
QMPHLP = \
    """
used in conjucnction wih the -r command for enabling qmp.
i will use the qmp option provided in instance files if any available.
if no qmp option provided, this script will generate one for each instance.
"""
NSHLP = \
    """
Setup NS3 network by passing in the location of your ns3 source file- e.g. tap-csma-virtual-machine-parsa.cc.
NOTE: NS3 shold be compiled with our PARSA script beforehand.
mrun will take care of setting up all the options for instances - so no need for providing them.
also, mrun will setup all the networks required. so no need to do that as well. setting up networks yourself
might cause conflict with using this option.
"""
NETHLP = \
    """
used in conduction with multi-node network. sets up host"""

MNHLP = \
    """
enable multinode protocol - currently in development."""

UPHLP = \
    """
This option can be used for updating your instance file. as this scripts can add to it and you might want to
see what the script has done to your file. useful for debugging"""


def setupParseArg(argv):
    parser = argparse.ArgumentParser(
        description=HELP_TEXT, formatter_class=RawTextHelpFormatter)

    parser.add_argument("-g", "--generate", help=GENHLP,
                        action='store_true', dest="gen")

    parser.add_argument("-d", "--debug", help=GENHLP,
                        action='store_true', dest="debug")


    parser.add_argument("-o", "--output", help=OUTHLP,
                        action='store_true', dest="output")

    parser.add_argument("-c", "--convert", help=CONHLP, nargs=2,
                        dest="con", metavar=("INPUT FILE/TEXT", "OUTXML"))

    parser.add_argument("-r", "--run", help=RUNHLP, dest="run", metavar="FILE")

    parser.add_argument("-n", "--mnet", help=NETHLP,
                        dest="mnet", metavar=("IP:PORT"))

    parser.add_argument("-ns", help=NSHLP, dest="ns",
                        metavar=("ns-script file"))

    parser.add_argument("-m", "--mprotocol", help=MNHLP,
                        action='store_true', dest="mprotocol")

    parser.add_argument("-qmp", help=QMPHLP,
                        action='store_true', dest="qmp")

    parser.add_argument("-q","--quantum", help=QHLP,
                        dest="quantum")

    parser.add_argument("-lo", "--load", help=LDHLP,
                        dest="load")

    parser.add_argument("-u","--update-file", help=UPHLP,
                        action='store_true', dest="update")

    parser.add_argument('-l','--log', dest='log', action='store',
                        choices=['NOTSET','DEBUG','INFO', 'WARNING', 'ERROR','CRITICAL'],
                        help='Logging events')
    args = parser.parse_args(argv)
    return args


def main(argv):

    args = setupParseArg(argv)
    if len(sys.argv) <= 1:
        log.critical("more than one argument is needed")
        exit(1)
    ex = executor(args)


    while mrun_exit.wait(None):
        if ex.exitRequested():
            log.critical("Exit Code:{0}".format(ex.getExitMessage()))
            break


if __name__ == "__main__":
    main(sys.argv[1:])

#!/usr/bin/python
#
# Low-level QEMU shell on top of QMP.
#
# Copyright (C) 2009, 2010 Red Hat Inc.
#
# Authors:
#  Luiz Capitulino <lcapitulino@redhat.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# Usage:
#
# Start QEMU with:
#
# # qemu [...] -qmp unix:./qmp-sock,server
#
# Run the shell:
#
# $ qmp-shell ./qmp-sock
#
# Commands have the following format:
#
#    < command-name > [ arg-name1=arg1 ] ... [ arg-nameN=argN ]
#
# For example:
#
# (QEMU) device_add driver=e1000 id=net1
# {u'return': {}}
# (QEMU)
#
# key=value pairs also support Python or JSON object literal subset notations,
# without spaces. Dictionaries/objects {} are supported as are arrays [].
#
#    example-command arg-name1={'key':'value','obj'={'prop':"value"}}
#
# Both JSON and Python formatting should work, including both styles of
# string literal quotes. Both paradigms of literal values should work,
# including null/true/false for JSON and None/True/False for Python.
#
#
# Transactions have the following multi-line format:
#
#    transaction(
#    action-name1 [ arg-name1=arg1 ] ... [arg-nameN=argN ]
#    ...
#    action-nameN [ arg-name1=arg1 ] ... [arg-nameN=argN ]
#    )
#
# One line transactions are also supported:
#
#    transaction( action-name1 ... )
#
# For example:
#
#     (QEMU) transaction(
#     TRANS> block-dirty-bitmap-add node=drive0 name=bitmap1
#     TRANS> block-dirty-bitmap-clear node=drive0 name=bitmap0
#     TRANS> )
#     {"return": {}}
#     (QEMU)
#
# Use the -v and -p options to activate the verbose and pretty-print options,
# which will echo back the properly formatted JSON-compliant QMP that is being
# sent to QEMU, which is useful for debugging and documentation generation.

import sys
import os

from qmp import qmp
import json
import ast
import readline
import Queue
import threading


class QMPCompleter(list):
    def complete(self, text, state):
        for cmd in self:
            if cmd.startswith(text):
                if not state:
                    return cmd
                else:
                    state -= 1

class QMPShellError(Exception):
    pass

class QMPShellBadPort(QMPShellError):
    pass

class FuzzyJSON(ast.NodeTransformer):
    '''This extension of ast.NodeTransformer filters literal "true/false/null"
    values in an AST and replaces them by proper "True/False/None" values that
    Python can properly evaluate.'''
    def visit_Name(self, node):
        if node.id == 'true':
            node.id = 'True'
        if node.id == 'false':
            node.id = 'False'
        if node.id == 'null':
            node.id = 'None'
        return node

# TODO: QMPShell's interface is a bit ugly (eg. _fill_completion() and
#       _execute_cmd()). Let's design a better one.
class QMPShell(qmp.QEMUMonitorProtocol):
    def __init__(self, address, pretty=False):
        qmp.QEMUMonitorProtocol.__init__(self, self.__get_address(address))
        self._greeting = None
        self._completer = None
        self._pretty = pretty
        self._transmode = False
        self._actions = list()

    def __get_address(self, arg):
        """
        Figure out if the argument is in the port:host form, if it's not it's
        probably a file path.
        """
        addr = arg.split(':')
        if len(addr) == 2:
            try:
                port = int(addr[1])
            except ValueError:
                raise QMPShellBadPort
            return ( addr[0], port )
        # socket path
        return arg

    def _fill_completion(self):
        for cmd in self.cmd('query-commands')['return']:
            self._completer.append(cmd['name'])

    def __completer_setup(self):
        self._completer = QMPCompleter()
        self._fill_completion()
        readline.set_completer(self._completer.complete)
        readline.parse_and_bind("tab: complete")
        # XXX: default delimiters conflict with some command names (eg. query-),
        # clearing everything as it doesn't seem to matter
        readline.set_completer_delims('')

    def __parse_value(self, val):
        try:
            return int(val)
        except ValueError:
            pass

        if val.lower() == 'true':
            return True
        if val.lower() == 'false':
            return False
        if val.startswith(('{', '[')):
            # Try first as pure JSON:
            try:
                return json.loads(val)
            except ValueError:
                pass
            # Try once again as FuzzyJSON:
            try:
                st = ast.parse(val, mode='eval')
                return ast.literal_eval(FuzzyJSON().visit(st))
            except SyntaxError:
                pass
            except ValueError:
                pass
        return val

    def __cli_expr(self, tokens, parent):
        for arg in tokens:
            (key, sep, val) = arg.partition('=')
            if sep != '=':
                raise QMPShellError("Expected a key=value pair, got '%s'" % arg)

            value = self.__parse_value(val)
            optpath = key.split('.')
            curpath = []
            for p in optpath[:-1]:
                curpath.append(p)
                d = parent.get(p, {})
                if type(d) is not dict:
                    raise QMPShellError('Cannot use "%s" as both leaf and non-leaf key' % '.'.join(curpath))
                parent[p] = d
                parent = d
            if optpath[-1] in parent:
                if type(parent[optpath[-1]]) is dict:
                    raise QMPShellError('Cannot use "%s" as both leaf and non-leaf key' % '.'.join(curpath))
                else:
                    raise QMPShellError('Cannot set "%s" multiple times' % key)
            parent[optpath[-1]] = value

    def __build_cmd(self, cmdline):
        """
        Build a QMP input object from a user provided command-line in the
        following format:

            < command-name > [ arg-name1=arg1 ] ... [ arg-nameN=argN ]
        """
        cmdargs = cmdline.split()

        # Transactional CLI entry/exit:
        if cmdargs[0] == 'transaction(':
            self._transmode = True
            cmdargs.pop(0)
        elif cmdargs[0] == ')' and self._transmode:
            self._transmode = False
            if len(cmdargs) > 1:
                raise QMPShellError("Unexpected input after close of Transaction sub-shell")
            qmpcmd = { 'execute': 'transaction',
                       'arguments': { 'actions': self._actions } }
            self._actions = list()
            return qmpcmd

        # Nothing to process?
        if not cmdargs:
            return None

        # Parse and then cache this Transactional Action
        if self._transmode:
            finalize = False
            action = { 'type': cmdargs[0], 'data': {} }
            if cmdargs[-1] == ')':
                cmdargs.pop(-1)
                finalize = True
            self.__cli_expr(cmdargs[1:], action['data'])
            self._actions.append(action)
            return self.__build_cmd(')') if finalize else None

        # Standard command: parse and return it to be executed.
        qmpcmd = { 'execute': cmdargs[0], 'arguments': {} }
        self.__cli_expr(cmdargs[1:], qmpcmd['arguments'])
        return qmpcmd

    def _print(self, qmp):
        indent = None
        if self._pretty:
            indent = 4
        jsobj = json.dumps(qmp, indent=indent)
        print str(jsobj)

    def _execute_cmd(self, cmdline):
        try:
            qmpcmd = self.__build_cmd(cmdline)
        except Exception as e:
            print 'Error while parsing command line: %s' % e
            print 'command format: <command-name> ',
            print '[arg-name1=arg1] ... [arg-nameN=argN]'
            return True
        # For transaction mode, we may have just cached the action:
        if qmpcmd is None:
            return True
        if self._verbose:
            self._print(qmpcmd)
        resp = self.cmd_obj(qmpcmd)
        if resp is None:
            print 'Disconnected'
            return False
        self._print(resp)
        return True

    def connect(self, activateCompleter = False):
        self._greeting = qmp.QEMUMonitorProtocol.connect(self)

        if activateCompleter:
            self.__completer_setup()

    def show_banner(self, name, msg='Welcome to the QMP low-level shell!'):
        #print str(name + ": " + msg)
        version = self._greeting['QMP']['version']['qemu']
        print 'Instance %s: Connected to QEMU %d.%d.%d\n' % (name, version['major'],version['minor'],version['micro'])

    def get_prompt(self):
        if self._transmode:
            return "TRANS> "
        return "(QEMU) "

    def read_exec_command(self, cmdline):
        """
        Read and execute a command.

        @return True if execution was ok, return False if disconnected.
        """
        # try:
        #     cmdline = raw_input(prompt)
        # except EOFError:
        #     print
        #     return False
        if cmdline == '':
            for ev in self.get_events():
                print ev
            self.clear_events()
            return True
        else:
            return self._execute_cmd(cmdline)


    def set_verbosity(self, verbose):
        self._verbose = verbose

class HMPShell(QMPShell):
    def __init__(self, address,name):
        QMPShell.__init__(self, address)
        self.__cpu_index = 0
        self.__name = name

    def __cmd_completion(self):
        for cmd in self.__cmd_passthrough('help')['return'].split('\r\n'):
            if cmd and cmd[0] != '[' and cmd[0] != '\t':
                name = cmd.split()[0] # drop help text
                if name == 'info':
                    continue
                if name.find('|') != -1:
                    # Command in the form 'foobar|f' or 'f|foobar', take the
                    # full name
                    opt = name.split('|')
                    if len(opt[0]) == 1:
                        name = opt[1]
                    else:
                        name = opt[0]
                self._completer.append(name)
                self._completer.append('help ' + name) # help completion

    def __info_completion(self):
        for cmd in self.__cmd_passthrough('info')['return'].split('\r\n'):
            if cmd:
                self._completer.append('info ' + cmd.split()[1])

    def __other_completion(self):
        # special cases
        self._completer.append('help info')

    def _fill_completion(self):
        self.__cmd_completion()
        self.__info_completion()
        self.__other_completion()

    def __cmd_passthrough(self, cmdline, cpu_index = 0):
        return self.cmd_obj({ 'execute': 'human-monitor-command', 'arguments':
                              { 'command-line': cmdline,
                                'cpu-index': cpu_index } })

    def _execute_cmd(self, cmdline):
        if cmdline.split()[0] == "cpu":
            # trap the cpu command, it requires special setting
            try:
                idx = int(cmdline.split()[1])
                if not 'return' in self.__cmd_passthrough('info version', idx):
                    print 'bad CPU index'
                    return True
                self.__cpu_index = idx
            except ValueError:
                print 'cpu command takes an integer argument'
                return True
        resp = self.__cmd_passthrough(cmdline, self.__cpu_index)
        if resp is None:
            print 'Disconnected'
            return False
        assert 'return' in resp or 'error' in resp
        if 'return' in resp:
            # Success
            if len(resp['return']) > 0:
                print self.__name + ": " + resp['return'],
        else:
            # Error
            print '%s: %s' % (resp['error']['class'], resp['error']['desc'])
        return True

    def show_banner(self, msg='Welcome to the HMP shell!'):
        QMPShell.show_banner(self, self.__name, msg)

def die(msg):
    sys.stderr.write('ERROR: %s\n' % msg)
    sys.exit(1)

def fail_cmdline(option=None):
    if option:
        sys.stderr.write('ERROR: bad command-line option \'%s\'\n' % option)
    sys.stderr.write('qemu-shell [ -v ] [ -p ] [ -H ] < UNIX socket path> | < TCP address:port >\n')
    sys.exit(1)

class qmp_shell():

    def __init__(self, args, name):
        self.__addr = ''
        self.__qemu = None
        self.__hmp = False
        self.__pretty = False
        self.__verbose = False
        self.__activateCompleter = False
        self.__queue = Queue.Queue()
        self.__name = name
        self.__args = args

    def enter(self):
        try:
            for arg in self.__args:
                if str(arg).lower() == "h":
                    if self.__qemu is not None:
                        fail_cmdline(arg)
                    self.__hmp = True
                elif str(arg).lower() == "p":
                    self.__pretty = True
                elif str(arg).lower() == "v":
                    self.__verbose = True
                elif str(arg).lower() == "c":
                    self.__activateCompleter = True
                else:
                    if self.__qemu is not None:
                        fail_cmdline(arg)
                    if self.__hmp:
                        self.__qemu = HMPShell(arg, self.__name)
                    else:
                        self.__qemu = QMPShell(arg, self.__pretty)
                    self.__addr = arg

            if self.__qemu is None:
                fail_cmdline()
        except QMPShellBadPort:
            die('bad port number in command-line')

        try:
            self.__qemu.connect(self.__activateCompleter)
        except qmp.QMPConnectError:
            die('Didn\'t get QMP greeting message')
        except qmp.QMPCapabilitiesError:
            die('Could not negotiate capabilities')
        except self.__qemu.error:
            die('Could not connect to %s' % self.__addr)

        self.__qemu.show_banner(self.__name)
        self.__qemu.set_verbosity(self.__verbose)

        #self.__startProcess()

    def executecmd(self, cmd):
        self.__qemu.read_exec_command(cmd)

    def __startProcess(self):
        while True:
            if not self.__queue.empty():
                self.__qemu.read_exec_command(self.__queue.get())

    def queueCommand(self, cmd):
        self.__queue.put(cmd)
#
    def close(self):
        self.__qemu.close()

# def main():
#     args = sys.argv[1:]
#     q = qmp_shell(args)
#
# if __name__ == "__main__":
#     main()

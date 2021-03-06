#  DO-NOT-REMOVE begin-copyright-block
# QFlex consists of several software components that are governed by various
# licensing terms, in addition to software that was developed internally.
# Anyone interested in using QFlex needs to fully understand and abide by the
# licenses governing all the software components.
#
# ### Software developed externally (not by the QFlex group)
#
#     * [NS-3] (https://www.gnu.org/copyleft/gpl.html)
#     * [QEMU] (http://wiki.qemu.org/License)
#     * [SimFlex] (http://parsa.epfl.ch/simflex/)
#     * [GNU PTH] (https://www.gnu.org/software/pth/)
#
# ### Software developed internally (by the QFlex group)
# **QFlex License**
#
# QFlex
# Copyright (c) 2020, Parallel Systems Architecture Lab, EPFL
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the documentation
#       and/or other materials provided with the distribution.
#     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
#       nor the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
# EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#  DO-NOT-REMOVE end-copyright-block

# QEMU Monitor Protocol Python class
#
# Copyright (C) 2009, 2010 Red Hat Inc.
#
# Authors:
#  Luiz Capitulino <lcapitulino@redhat.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.

from __future__ import print_function

import json
import errno
import socket
import sys
import logging as log
import os
import errno

def compat_input():
    return raw_input().decode(sys.stdin.encoding or
                              locale.getpreferredencoding(True)).encode('utf-8')

try:
    raw_input
    input = compat_input
except NameError:
    pass

class QMPError(Exception):
    pass


class QMPConnectError(QMPError):
    pass


class QMPCapabilitiesError(QMPError):
    pass


class QMPTimeoutError(QMPError):
    pass


class QEMUMonitorProtocol(object):

    #: Socket's error class
    error = socket.error
    #: Socket's timeout
    timeout = socket.timeout

    def __init__(self, address, server=False, debug=False):
        """
        Create a QEMUMonitorProtocol class.

        @param address: QEMU address, can be either a unix socket path (string)
                        or a tuple in the form ( address, port ) for a TCP
                        connection
        @param server: server mode listens on the socket (bool)
        @raise socket.error on socket connection errors
        @note No connection is established, this is done by the connect() or
              accept() methods
        """
        self.__log = log.getLogger(__name__)
        self.__events = []
        self.__address = address
        self._debug = debug
        self.__sock = self.__get_sock()
        self.__sockfile = None
        if server:
            self.__sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

            self.__sock.bind(self.__address)
            self.__log.debug("Binding socket to: {0}".format(self.__address))
            self.__sock.listen(1)

    def __get_sock(self):
        if isinstance(self.__address, tuple):
            family = socket.AF_INET
        else:
            family = socket.AF_UNIX
        return socket.socket(family, socket.SOCK_STREAM)

    def __negotiate_capabilities(self):
        greeting = self.__json_read()
        if greeting is None or "QMP" not in greeting:
            raise QMPConnectError
        # Greeting seems ok, negotiate capabilities
        resp = self.cmd('qmp_capabilities')
        if "return" in resp:
            return greeting
        raise QMPCapabilitiesError

    def __json_read(self, only_event=False):
        while True:
            data = self.__sockfile.readline()
            if not data:
                return
            resp = json.loads(data)
            if 'event' in resp:
                if self._debug:
                    print("QMP:<<< %s" % resp, file=sys.stderr)
                self.__events.append(resp)
                if not only_event:
                    continue
            return resp

    def __get_events(self, wait=False):
        """
        Check for new events in the stream and cache them in __events.

        @param wait (bool): block until an event is available.
        @param wait (float): If wait is a float, treat it as a timeout value.

        @raise QMPTimeoutError: If a timeout float is provided and the timeout
                                period elapses.
        @raise QMPConnectError: If wait is True but no events could be
                                retrieved or if some other error occurred.
        """

        # Check for new events regardless and pull them into the cache:
        self.__sock.setblocking(0)
        try:
            self.__json_read()
        except socket.error as err:
            if err[0] == errno.EAGAIN:
                # No data available
                pass
        self.__sock.setblocking(1)

        # Wait for new events, if needed.
        # if wait is 0.0, this means "no wait" and is also implicitly false.
        if not self.__events and wait:
            if isinstance(wait, float):
                self.__sock.settimeout(wait)
            try:
                ret = self.__json_read(only_event=True)
            except socket.timeout:
                raise QMPTimeoutError("Timeout waiting for event")
            except:
                raise QMPConnectError("Error while reading from socket")
            if ret is None:
                raise QMPConnectError("Error while reading from socket")
            self.__sock.settimeout(None)

    def connect(self, negotiate=True):
        """
        Connect to the QMP Monitor and perform capabilities negotiation.

        @return QMP greeting dict
        @raise socket.error on socket connection errors
        @raise QMPConnectError if the greeting is not received
        @raise QMPCapabilitiesError if fails to negotiate capabilities
        """

        self.__log.debug("trying to connect to {0}".format(self.__address))

        if self.__log.isEnabledFor(log.DEBUG):
            try:
                self.__sock.connect(self.__address)
                self.__log.debug("connected to socket {0} ".format(self.__address))
            except socket.error as exc:
                self.__log.debug("connecting to socket {0} and getting result: {1}".format(self.__address, exc))
        else:
            self.__sock.connect(self.__address)

        self.__sockfile = self.__sock.makefile()
        if negotiate:
            return self.__negotiate_capabilities()

    def accept(self):
        """
        Await connection from QMP Monitor and perform capabilities negotiation.

        @return QMP greeting dict
        @raise socket.error on socket connection errors
        @raise QMPConnectError if the greeting is not received
        @raise QMPCapabilitiesError if fails to negotiate capabilities
        """
        self.__sock.settimeout(15)
        self.__sock, _ = self.__sock.accept()
        self.__sockfile = self.__sock.makefile()
        return self.__negotiate_capabilities()

    def cmd_obj(self, qmp_cmd):
        """
        Send a QMP command to the QMP Monitor.

        @param qmp_cmd: QMP command to be sent as a Python dict
        @return QMP response as a Python dict or None if the connection has
                been closed
        """
        if self._debug:
            print("QMP:>>> %s" % qmp_cmd, file=sys.stderr)
        try:
            self.__sock.sendall(json.dumps(qmp_cmd).encode('utf-8'))
        except socket.error as err:
            if err[0] == errno.EPIPE:
                return
            raise socket.error(err)
        resp = self.__json_read()
        if self._debug:
            print("QMP:<<< %s" % resp, file=sys.stderr)
        return resp

    def cmd(self, name, args=None, cmd_id=None):
        """
        Build a QMP command and send it to the QMP Monitor.

        @param name: command name (string)
        @param args: command arguments (dict)
        @param cmd_id: command id (dict, list, string or int)
        """
        qmp_cmd = {'execute': name}
        if args:
            qmp_cmd['arguments'] = args
        if cmd_id:
            qmp_cmd['id'] = cmd_id
        return self.cmd_obj(qmp_cmd)

    def command(self, cmd, **kwds):
        """
        Build and send a QMP command to the monitor, report errors if any
        """
        ret = self.cmd(cmd, kwds)
        if "error" in ret:
            raise Exception(ret['error']['desc'])
        return ret['return']

    def pull_event(self, wait=False):
        """
        Pulls a single event.

        @param wait (bool): block until an event is available.
        @param wait (float): If wait is a float, treat it as a timeout value.

        @raise QMPTimeoutError: If a timeout float is provided and the timeout
                                period elapses.
        @raise QMPConnectError: If wait is True but no events could be
                                retrieved or if some other error occurred.

        @return The first available QMP event, or None.
        """
        self.__get_events(wait)

        if self.__events:
            return self.__events.pop(0)
        return None

    def get_events(self, wait=False):
        """
        Get a list of available QMP events.

        @param wait (bool): block until an event is available.
        @param wait (float): If wait is a float, treat it as a timeout value.

        @raise QMPTimeoutError: If a timeout float is provided and the timeout
                                period elapses.
        @raise QMPConnectError: If wait is True but no events could be
                                retrieved or if some other error occurred.

        @return The list of available QMP events.
        """
        self.__get_events(wait)
        return self.__events

    def clear_events(self):
        """
        Clear current list of pending events.
        """
        self.__events = []

    def close(self):
        self.__sock.close()
        self.__sockfile.close()

    def settimeout(self, timeout):
        self.__sock.settimeout(timeout)

    def get_sock_fd(self):
        return self.__sock.fileno()

    def is_scm_available(self):
        return self.__sock.family == socket.AF_UNIX

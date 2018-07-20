#!/usr/bin/python

import socket
import threading
import Queue
import time

class server:
    def __init__(self, hostaddress, hostport):
        self.__hostport = None
        self.__hostaddress = None
        self.__initialized = False
        self.__numclients = 0
        self.__clientsockets = []
        self.__queue = Queue.Queue()
        self.__stopserver = False
        self.__serversocket = None
        self.__cleanupRequested = False
        self._sPairs = []
        if not isinstance(hostport, int):
            self.__hostport = int(hostport)
        else:
            self.__hostport = hostport

        self.__hostaddress = hostaddress

        if self.__hostaddress == "0.0.0.0":
            print "setting up port on all available addresses"

    def __exit__(self, exc_type, exc_value, traceback):
        for c in self.__clientsockets:
            if c != None:
                c.close()

    def __clientProcess(self, cs):
        while True:
            if not self.__queue.empty():
                self.__sendMessageToAllClinets()

    def __sendMessageToAllClinets(self):
        msg = self.__queue.get()
        for a,b in self._sPairs:
            a.send(msg)
            cmsg = a.recv(4096)
            cmsg = "\n" + b + " sent: " + cmsg
            print cmsg

    def isInitialized(self):
        return self.__initialized

    def cleanup(self):
        self.__cleanupRequested = True
        while not self.__queue.empty():
            time.sleep(.1)
        for c in self.__clientsockets:
            c.close()
        self.__serversocket.close()

    def stopServer(self):
        if not self.__isStopped():
            self.__stopserver = True
        # stop clients
        for c in self.__clientsockets:
            c.close()

    def __isStopped(self):
        if self.__stopserver == True:
            return True
        return False

    def queueMessage(self, msg):
        self.__queue.put(msg)

    def setupServer(self):
        # create a socket object
        self.__serversocket = socket.socket(
            socket.AF_INET, socket.SOCK_STREAM)

        self.__serversocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        print "Setting up Server with IP: %s - port number: %i " % (self.__hostaddress, self.__hostport)

        # bind to the port
        self.__serversocket.bind((self.__hostaddress, self.__hostport))

        # queue up to 50 requests
        self.__serversocket.listen(50)

        while True:
            if self.__cleanupRequested:
                break
            # establish a connection
            clientsocket, addr = self.__serversocket.accept()



            if clientsocket != None:
                self.__initialized = True
                self.__numclients += 1
                self.__clientsockets.append(clientsocket)

                clientsocket.send("name");
                name = clientsocket.recv(4096)

                self._sPairs.append([clientsocket, name])


            print("Got a connection from %s" % str(addr))

            thread = threading.Thread(target=self.__clientProcess, args=(clientsocket,), name=str("client_process" + str(self.__numclients)))
            thread.daemon = True                            # Daemonize thread
            thread.start()

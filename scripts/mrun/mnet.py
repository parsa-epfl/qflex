#!/usr/bin/python

from subprocess import Popen, PIPE, call
import threading
import logging as log
import psutil
import time
import os

usingSudo = False

def POPEN(*args):
    returncode = POPEN_INNER(*args)
    log.debug("returncode: {0}".format(returncode))


def POPEN_INNER(*args):
    param = ""
    for i in args:
        param+=i+" "
    if usingSudo:
        param = "sudo " + param
        log.debug("executing: {0}".format(param))

        return call(param,
                    shell=True,
                    stdin=PIPE,
                    stdout=PIPE,
                    stderr=PIPE)
    else:
        return call(param,
                    shell=True,
                    stdin=PIPE,
                    stdout=PIPE,
                    stderr=PIPE)
class tap:
    def cleanup(self):
        # Bring down the taps
        POPEN('ifconfig',self.__name,'down')
        self.__log.debug("bringing down tap network: {0}".format(self.__name))

        #sudo tunctl -d tap-lef
        POPEN('tunctl', '-d', self.__name)
        self.__log.debug("deleting tap network: {0}".format(self.__name))


    def __init__(self, name):
        self.__name = name
        self.__addr = "0.0.0.0"
        self.__log = log.getLogger('mrun.logger')

        #  sudo tunctl -t tap-left
        POPEN('tunctl','-t', self.__name)
        self.__log.debug("creating tap network: {0}".format(self.__name))
        #sudo ifconfig tap-left 0.0.0.0 promisc up
        POPEN('ifconfig',self.__name,self.__addr,'promisc','up')
        self.__log.debug("setting up tap network: {0} with address {1}".format(self.__name, self.__addr))

    def getName(self):
        return self.__name

class bridge:

    def cleanup(self):
        # bring the bridge down
        POPEN('ifconfig',self.__name,'down')
        self.__log.debug("bringing down bridge network: {0}".format(self.__name))

        #Remove the taps from the bridge 
        for t in self.__taps:
            POPEN('brctl','delif',self.__name, t.getName())
            self.__log.debug("removing tap {0} associated with bridge {1}".format(t.getName(), self.__name))

        #Destroy the bridges
        POPEN('brctl','delbr',self.__name)
        self.__log.debug("deleting bridge network: {0}".format(self.__name))

    def __init__(self, name):
        self.__name = name
        self.__taps = []
        self.__log = log.getLogger('mrun.logger')

        #create the bridge
        # sudo brctl addbr br-left
        POPEN('brctl','addbr',self.__name)
        self.__log.debug("creating bridge network: {0}".format(self.__name))


    def addTap(self, t=tap):
        self.__taps.append(t)
        #add tap interface to the bridge
        # sudo brctl addif br-left tap-left
        POPEN('brctl','addif',self.__name, t.getName())
        self.__log.debug("adding tap {0} to bridge network: {1}".format(t.getName(), self.__name))


    def bridgeUp(self):
        #bring the bridge up
        POPEN('ifconfig',self.__name,'up')
        self.__log.debug("bringing up bridge network: {0}".format(self.__name))


class net:
    def __init__(self):
        self.__taps = []
        self.__bridges = []
        self.__netnames = None
        self.__nsScriptFile = None
        self.__ns_taps = []
        self.__log = log.getLogger('mrun.logger')
        self.__psutil = psutil.Process()
        self.__pid = None
        self.__proc = None

    def cleanup(self):
        self.__psutil.terminate()
        for b in self.__bridges:
            b.cleanup()
        for t in self.__taps:
            t.cleanup()



    def setScriptPath(self, script):
        self.__nsScriptFile = script

    def configureNetwork(self, names):
        self.__log.debug("creating NS3 network")
        self.__configureNetworkInner(names)
        # thread = threading.Thread(
        #     target=self.configureNetworkInner, args=(names,), name="ns3")
        # thread.daemon = True                            # Daemonize thread
        # thread.start()

    def __configureNetworkInner(self, names):

        self.__netNames = names

        for index, item in enumerate(names):
            ns_name = item + "-ns"

            t1 = tap(item)
            t2 = tap(ns_name)
            self.__taps.extend([t1, t2]) # create tap for qemu and ns3
            self.__ns_taps.append(ns_name)
            # create bridge for the above
            b = bridge("br-"+str(index))
            # add taps to the bridge
            b.addTap(t1)
            b.addTap(t2)

            #bring the bridge up
            b.bridgeUp()

            self.__bridges.append(b)

        for n in self.__ns_taps:
            self.__nsScriptFile += " {0}".format(n)

        os.environ["PATH"] += os.pathsep + "/usr/local/bin"

        self.__log.debug("running ns3 script...")
        self.__log.debug("performing... {0}".format(self.__nsScriptFile))

        try:
            os.environ["PATH"] = "/usr/local/bin" + os.pathsep + os.environ["PATH"]
        except KeyError:
            os.environ["PATH"] = "/usr/local/bin" + os.pathsep

        try:
            os.environ["LD_LIBRARY_PATH"] = "/usr/local/lib64" + os.pathsep + \
                                            "/usr/local/lib" + os.pathsep \
                                            + os.environ["LD_LIBRARY_PATH"]
        except KeyError:
            os.environ["LD_LIBRARY_PATH"] = "/usr/local/lib64" + os.pathsep + \
                                            "/usr/local/lib" + os.pathsep

        cmd = []
        cmd = self.__nsScriptFile.split()
        #cmd.insert(0, 'sudo')
        #start the CSMA network

        self.__proc = Popen(cmd,
                        shell=False,
                        stdin=PIPE, stdout=PIPE, stderr=PIPE)

        self.__pid = self.__proc.pid
        self.__log.info("starting instance: with pid: {0}".format(self.__pid))
        self.__psutil = psutil.Process(self.__pid)

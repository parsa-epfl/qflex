import os
import time
import psutil
import logging
import threading
import netifaces
from subprocess import Popen, PIPE, call, check_call


def CALL(*args):
    log = logging.getLogger()
    param = ""
    for i in args:
        param+=i+" "

    ret=check_call(param, shell=True, stdin=PIPE, stdout=PIPE, stderr=PIPE)
    log.debug("performing: {0} with return code= {1}".format(param,ret))
    return ret

class tap:
    def cleanup(self):
        addr = netifaces.interfaces()
        self.__log.debug("current interfaces = {0}".format(addr))

        for i in addr:
            if i == self.__name:
                # Bring down the taps
                CALL('ifconfig',self.__name,'down')
                self.__log.debug("bringing down tap interface: {0}".format(self.__name))

                #sudo tunctl -d tap-lef
                CALL('tunctl', '-d', self.__name)
                self.__log.debug("deleting tap network: {0}".format(self.__name))

    def __init__(self, name):
        self.__name = name
        self.__addr = "0.0.0.0"
        self.__log = logging.getLogger()

        #  sudo tunctl -t tap-left
        addr = netifaces.interfaces()
        self.__log.debug("current interfaces = {0}".format(addr))
        for i in addr:
            if i == self.__name:
                self.cleanup()

        CALL('tunctl','-t', self.__name)
        self.__log.debug("creating tap network: {0}".format(self.__name))


        #sudo ifconfig tap-left 0.0.0.0 promisc up
        CALL('ifconfig',self.__name,self.__addr,'promisc','up')
        self.__log.debug("setting up tap network: {0} with address {1}".format(self.__name, self.__addr))

    def getName(self):
        return self.__name

class bridge:

    def cleanup(self):
        # bring the bridge down
        addr = netifaces.interfaces()
        self.__log.debug("current interfaces = {0}".format(addr))
        self.__log.debug("cleaning interface = {0}".format(self.__name))

        for i in addr:
            if i == self.__name:

                CALL('ifconfig',self.__name,'down')
                self.__log.debug("bringing down bridge network: {0}".format(self.__name))

                #Remove the taps from the bridge
                for t in self.__taps:
                    CALL('brctl','delif',self.__name, t.getName())
                    self.__log.debug("removing tap {0} associated with bridge {1}".format(t.getName(), self.__name))

                #Destroy the bridges
                CALL('brctl','delbr',self.__name)
                self.__log.debug("deleting bridge network: {0}".format(self.__name))

    def __init__(self, name):
        self.__name = name
        self.__taps = []
        self.__log = logging.getLogger()

        addr = netifaces.interfaces()
        self.__log.debug("current interfaces = {0}".format(addr))

        for i in addr:
            if i == self.__name:
                self.cleanup()

        #create the bridge
        # sudo brctl addbr br-left
        CALL('brctl','addbr',self.__name)
        self.__log.debug("creating bridge network: {0}".format(self.__name))


    def addTap(self, t=tap):
        self.__taps.append(t)
        #add tap interface to the bridge
        # sudo brctl addif br-left tap-left
        CALL('brctl','addif',self.__name, t.getName())
        self.__log.debug("adding tap {0} to bridge network: {1}".format(t.getName(), self.__name))


    def bridgeUp(self):
        #bring the bridge up
        CALL('ifconfig',self.__name,'up')
        self.__log.debug("bringing up bridge network: {0}".format(self.__name))


class net:
    def __init__(self):
        self.__taps = []
        self.__bridges = []
        self.__netnames = None
        self.__nsScriptDir = ""
        self.__ns_taps = []
        self.__log = logging.getLogger()
        self.__psutil = psutil.Process()
        self.__pid = None
        self.__proc = None

    def cleanup(self):
        self.__psutil.terminate()
        for b in self.__bridges:
            b.cleanup()
        for t in self.__taps:
            t.cleanup()
        addr = netifaces.interfaces()
        self.__log.debug("Final interfaces after cleanup = {0}".format(addr))

    def setScriptPath(self, script):
        self.__nsScriptDir = script

    def configureNetwork(self, names):
        self.__log.debug("creating NS3 network")
        #try:
        self.__configureNetworkInner(names)
        #except Exception as e:
            #raise e
        # thread = threading.Thread(
        #     target=self.configureNetworkInner, args=(names,), name="ns3")
        # thread.daemon = True                            # Daemonize thread
        # thread.start()

    def __configureNetworkInner(self, names):

        self.__netNames = names
        try:
            for index, item in enumerate(names):
                ns_name = item + "-ns"

                t1 = tap(item)
                self.__taps.append(t1)
                t2 = tap(ns_name)
                self.__taps.append(t2) # create tap for qemu and ns3
                self.__ns_taps.append(ns_name)
                # create bridge for the above
                b = bridge("br-"+str(index))
                self.__bridges.append(b)

                # add taps to the bridge
                b.addTap(t1)
                b.addTap(t2)

                #bring the bridge up
                b.bridgeUp()
        except Exception as e:
            raise e

        os.environ["PATH"] += os.pathsep + "/usr/local/bin"

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

        self.__nsScriptDir = self.__nsScriptDir.strip()

        temp_taps = ""
        for n in self.__ns_taps:
            temp_taps += " {0}".format(n)
        command = "{0}{1}".format("tap-csma-virtual-machine-parsa", temp_taps)

        #start the CSMA network
        cmd = ['./waf', '--run', command]
        self.__log.debug("running ns3 script..")
        self.__log.debug("performing.. {0} . cwd={1}".format(' '.join(cmd), self.__nsScriptDir))
        self.__proc = Popen(cmd, shell=False, stdin=PIPE, stdout=PIPE, stderr=PIPE, cwd=self.__nsScriptDir)
        self.__pid = self.__proc.pid
        self.__psutil = psutil.Process(self.__pid)
        self.__log.debug("returncode: {0}".format(self.__proc.returncode))
        self.__log.info("starting instance: with pid: {0}".format(self.__pid))
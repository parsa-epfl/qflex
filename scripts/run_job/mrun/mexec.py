#!/usr/bin/python

import threading
import time
import inspect
import os.path

import mconf
import mserver
import mnet
import atexit
import mqmp
import logging as log
import sys

TIMEOUT = time.time() + 15  # 15 seconds

DEFAULT_HOST_ADRESS = "127.0.0.1"
DEFAULT_HOST_PORT = 8888

mrun_exit = threading.Event()

class exithandler():
    def __init__(self, errlist):
        self.__status = 0
        self.__status_list = {}  # dictionary AKA Hash
        self.__msg = None
        self.__info = None
        self.lock = threading.Lock()

        for i in errlist:
            self.__status_list[i] = errlist.index(i)

    def setStatus(self, status):
        self.lock.acquire()
        try:
            self.__status = self.__status_list[status]
            self.__msg = status

            # 0 represents this line - # 1 represents line at caller
            callerframerecord = inspect.stack()[1]
            frame = callerframerecord[0]
            self.__info = inspect.getframeinfo(frame)
            mrun_exit.set()
        finally:
            self.lock.release()

    def getStatus(self):
        return self.__status

    def getMessage(self):
        self.lock.acquire()
        try:
            return self.__msg, self.__info.filename, self.__info.function, self.__info.lineno
        finally:
            self.lock.release()

class executor:
    def __init__(self, args):

        self.__args = args

        self.__log = log
        self.__setLogger()

        self.__exitstatus = exithandler(
            ['None', 'GENDONE', 'CONVERTDONE', 'UPATE/OUTPUT DONE', 'GENERR', 'RUNERR', 'NETERR', 'KILL', 'DONE'])
        self.__instances = []
        self.__pids = []
        self.__updateFile = self.__cleanRequested = False
        self.__hostaddress = DEFAULT_HOST_ADRESS
        self.__hostport = DEFAULT_HOST_PORT
        self.__quantum = self.__server = __load = None

        self.__cfg = mconf.config()
        self.__ns = mnet.net()
        atexit.register(self.__cleanup)
        self.__configSetup()

    def __setLogger(self):
        if self.__args.log:
            self.__log.basicConfig(level=self.__args.log)
        else:
            self.__log.basicConfig(level=log.INFO)
        self.__log.basicConfig(format='%(asctime)s %(message)s', datefmt='%m/%d/%Y %I:%M:%S %p')
        self.__log.getLogger('mrun.logger')

        self.__log.debug("done setting up the executor")

    def __setUpdateFiles(self):
        if self.__args.update:
            self.__updateFile = True
            self.__log.debug('File update enabled')
            for i in self.__instances:
                i.updateFile()
                self.__log.debug("updating instance file {0}".format(i.getName()))

    def __cleanup(self):
        if self.__args.ns is not None:
            self.__log.debug("NS3 clean-up requested")
            self.__ns.cleanup()
        if self.__server is not None:
            self.__log.debug("server clean-up requested")
            self.__server.cleanup()
        for i in self.__instances:
            i.cleanup()

        time.sleep(2)
        self.__cleanRequested = True

    def __configSetup(self):
        if self.__args.gen:
            self.__cfg.generateSamples()
            self.__log.debug("done generating sample files")
            self.__exitstatus.setStatus('GENDONE')

        elif self.__args.con:
            self.__cfg.ConvertTextToFile(
                self.__args.con[0], self.__args.con[1])
            self.__log.debug("done converting text to file")
            self.__exitstatus.setStatus('CONVERTDONE')
        else:
            self.__run()


    def __mnetSetup(self):
        try:
            ip, port = self.__args.mnet.split(':')
        except ValueError:
            log.ERROR("Error type -h for help with mnetwork setup")
        else:
            if any(str.isalpha(c) for c in port):
                log.ERROR("Error port value can be numbers only")
                self.__exitstatus.setStatus('NETERR')
            if int(port) < 0 or int(port) > 65535:
                log.ERROR("Error port value has to be positive and below 65535")
                self.__exitstatus.setStatus('NETERR')

            (self.__hostaddress, self.__hostport) = (ip, port)
            (DEFAULT_HOST_ADRESS, DEFAULT_HOST_PORT) = (ip, port)
            self.__log.debug("setting up host at IP:{0} port:{1}".format(self.__hostaddress, self.__hostport))

    def __validateInstances(self):
        names = {}
        user_nets = {}
        for i in self.__instances:
            if i.getName() in names:
                self.__log.critical("found duplicate names in instance files and that's not allowed. - each instance has to have a unique name")
            names[i.getName()] = 1
            # if i.getUserNetwork() in user_nets:
            #     raise Exception, "found duplicate user networks in instance files and thats not allowed"
            #     user_nets[i.getUserNetwork()] = 1

    def __setupInstances(self):
        self.__instances = self.__cfg.parse(self.__args.run)
        if self.__instances is None:
            self.__log.critical('unable to parse instances or setup file')
        else:
            self.__log.debug('parsed {0} instances'.format(len(self.__instances)))
        self.__validateInstances()

    def __setupQMPOptions(self):
        if self.__args.qmp:
            for i in self.__instances:
                if self.__args.qmp:
                    self.__log.debug('QMP: enabling option for instance {0}'.format(i.getName()))
                    if not i.hasQMPCommand():
                        port = DEFAULT_HOST_PORT + i.getID() + 1
                        h, p = i.setQMPOpts("default", port)
                        self.__log.debug("No QMP option defined in file '{0}'".format(i.getName()))
                        self.__log.debug("creating default QMP options. client file '{0}' is listening on {1}:{2}".format(i.getName(),h,p))
                    # else:
                    #     port = DEFAULT_HOST_PORT + i.getID() + 1
                    #     i,p = i.setQMPOpts("default", port)
                    #     self.__log.warning("overwriting user-provided qmp options for {0}".format(i.getName))
                    #     self.__log.debug("creating default QMP options. client file '{0}' is listening on {1}:{2}".format(i.getName(),i,p))
                else:
                    self.__log.debug("using user-provided qmp options for {0}".format(i.getName()))

    def __setupNS3(self):
        # setup NS3 network for instances
        if self.__args.ns is not None:
            self.__log.debug("NS3 option enabled")
            # if not os.path.isfile(self.__args.ns):
            #     self.__log.critical("ns-3 script not found")
            #     self.__exitstatus.setStatus('NETERR')

            [i.setNetdev() for i in self.__instances]
            self.__log.debug("creating NS3 config for instance {0}".format(i.getName()))
            # create NS3 network for instances
            names = [i.getNetName() for i in self.__instances]
            self.__ns.setScriptPath(self.__args.ns)

            self.__ns.configureNetwork(names)
            # for i in self.__instances:
            #     i.activateSudo()

    def __setOutput(self):
        if self.__args.output:
            for i in self.__instances:
                i.outputCMD()
            self.__exitstatus.setStatus('GENDONE')

    def __setupQuantumOpts(self):
        if self.__args.quantum:
            self.__quantum = self.__args.quantum
            [i.setQuantum(self.__quantum) for i in self.__instances]

    def __setupLoadOpts(self):
        if self.__args.load:
            self.__load = self.__args.load
            [i.setLoadopts(self.__load) for i in self.__instances]

    def __run(self):
        self.__setupInstances()
        self.__setupQMPOptions()
        self.__setupQuantumOpts()
        [i.setextopts() for i in self.__instances]
        self.__setupLoadOpts()

        if self.__args.debug:
            [i.enableDebugOut() for i in self.__instances]

        self.__setUpdateFiles()
        self.__setupNS3()
        self.__setOutput()

        if self.__args.output or self.__args.update:
            self.__exitstatus.setStatus('UPATE/OUTPUT DONE')
        else:
            self.__setupExecution()
            time.sleep(1)

            if self.__args.qmp:
                for i in self.__instances:
                    i.startQMPshell()
                    time.sleep(1)
            else:
                self.__log.critical("no communication protocol defined")

            self.__getPids()
            self.__log.debug("All subprocesses have been started! {0}".format(self.__pids))

            if self.__args.mprotocol:
                self.__log.critical("multinode Protocol is not supported at the moment")

            self.__setupInputProcessing()

    def __startInstances(self):
        for i in self.__instances:
            i.start()

    def __startExecution(self):
        self.__log.debug("Execution: Starting Instances ...")
        self.__startInstances()

        num = 0;
        while True:
            if self.__cleanRequested:
                self.__log.debug("Execution: clean-up requested")
                break
            if self.__allTerminated():
                self.__log.debug("Execution: all instances terminated, requesting clean-up")
                self.__cleanup()
                self.__exitstatus.setStatus('DONE')
            if self.__allStopped():
                num = num +1
                self.__log.debug("Execution: Iterating {0}".format(num))
                self.__iterate()

    def __getPids(self):
        for i in self.__instances:
            self.__pids.append(i.getPid())

    def __setupExecution(self):
        thread = threading.Thread(
            target=self.__startExecution, args=(), name="execution")
        thread.daemon = True                            # Daemonize thread
        thread.start()

        # wait for processes to get started before getting their PIDs for the next function
        while not self.__allStarted():
            if time.time() > TIMEOUT:
                self.__exitstatus.setStatus('RUNERR')

    def __setupInputProcessing(self):
        thread = threading.Thread(
            target=self.__processInput, args=(), name="processinput")
        thread.daemon = True                            # Daemonize thread
        thread.start()

    def __startHost(self):
        print "Starting mserver ..."
        print "NOTE: This can only be used with the multi-node protocol"
        self.__server = mserver.server(self.__hostaddress, self.__hostport)
        self.__server.setupServer()

    def __sendMessageToClients(self, msg):
        if self.__server is None:
            raise Exception, 'server is none'
        else:
            if self.__server.isInitialized():
                self.__server.queueMessage(msg)

    def __setupHost(self):
        thread = threading.Thread(
            target=self.__startHost, args=(), name="server")
        thread.daemon = True  # Daemonize thread
        thread.start()

    def __processInput(self):
        while True:
            if self.__cleanRequested:
                break

            if self.__args.qmp:
                x = raw_input("qmp >>")
                if x == "kill":
                    self.__cleanup()
                    self.__exitstatus.setStatus('KILL')
                    break
                elif x.startswith('savevm') or x.startswith('savevm-ext'):
                    for i in self.__instances:
                        i.nodeSpecificRequest(x)
                elif x.startswith('loadvm') or x.startswith('loadvm-ext'):
                    for i in self.__instances:
                        i.nodeSpecificRequest(x)
                else:
                    for i in self.__instances:
                        i.executeQMP(x)

            elif self.__args.mprotocol:
                try:
                    x = raw_input("Your Wish Sire:")
                    if x == "kill":
                        self.__sendMessageToClients(x)
                        self.__cleanup()
                        self.__exitstatus.setStatus('KILL')
                        break
                    elif x == "name":
                        self.__sendMessageToClients(x)
                    else:
                        self.__sendMessageToClients(x)
                    words = x.split(' ')
                    if len(words) > 2 and len(words) < 2:
                        print "only two args are supported at the moment! not more not less"
                    else:
                        one, two = words
                        if one == "save":
                            self.__sendMessageToClients(one + " " + two)
                        if one == "load":
                            self.__sendMessageToClients(one + " " + two)
                        if one == "get":
                            if two == "output":
                                for i in self.__instances:
                                    out, err = i.getStdOutErr()
                                    print len(out)
                                    print len(err)
                                    print (out, err)
                        else:
                            self.__sendMessageToClients(one + " " + two)
                except ValueError:
                    print "need more than 1 word"
            else:
                pass

    def __allStopped(self):
        for i in self.__instances:
            if i is not None:
                if not i.isStopped():
                    return False
            else:
                self.__log.critical('instance {0} is None'.format(i.getName()))
        return True

    def __allStarted(self):
        for i in self.__instances:
            if i is not None:
                if not i.isStarted():
                    return False
            else:
                self.__log.critical('instance {0} is Noen'.format(i.getName()))
        return True

    def __allTerminated(self):
        for i in self.__instances:
            if i is not None:
                if not i.isTerminated():
                    return False
            else:
                self.__log.critical('instance {0} is None'.format(i.getName()))
        return True


    def __iterate(self):
        for i in self.__instances:
            if i is not None:
                i.performPendingCmd()
                time.sleep(2)
                for i in self.__instances:
                    self.__log.critical('instance {0} is taking its turn'.format(i.getName()))
                    i.takeTurn()
            else:
                self.__log.critical('instance {0} is None'.format(i))

    def exitRequested(self):
        return self.__exitstatus.getStatus()

    def getExitMessage(self):
        return self.__exitstatus.getMessage()

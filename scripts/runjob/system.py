import re
import os
import time
import atexit
import logging
import inspect
import threading
import subprocess
import ConfigParser

import instance
import validation
import ns3network

class QFSystem(object):
    def __init__(self, config_path):
        # Setup
        self.__config_path = config_path
        self.__log = logging
        # System parameters
        self.__instances = []
        self.__multinode = False
        self.__ns3_path = None
        self.__ns_network = None
        # Exit Handling
        self.__exit = False
        self.__exit_info = None
        self.__exit_lock = threading.Lock()
        atexit.register(self.exit)

    ## Configuration
    def parse_system(self):
        """
        Validate and parse system config file and associated instance config files
        """
        
        # Validate system config files
        # System validation includes per-instance validation
        if validation.validate_system(self.__config_path):
            # Set up config parser
            config = ConfigParser.ConfigParser()
            config.read(self.__config_path)
            # Parse Instances
            instances_list = config.items("Instances")
            for i, (instance_name, instance_path) in enumerate(instances_list):
                self.__instances.append(instance.instance(i, instance_name, instance_path))
                self.__instances[i].parse_instance()

            # Parse system parameters
            # ICount
            if config.has_section("System"):
                if config.has_option("System", "icount"):
                    icount = config.get("System", "icount")
                    if icount == "on":
                        icount_shift = config.get("System", "icount_shift")
                        icount_sleep = config.get("System", "icount_sleep")
                        for i in self.__instances:
                            i.set_icount(icount_shift, icount_sleep)

            # Parse multinode-specific system parameters
            # Multinode
            if len(self.__instances)>1:
                self.__multinode = True
                # NS3
                self.__ns3_path = config.get("Multinode", "ns3_path")
                for i in self.__instances:
                    i.set_net()
                # Quantum
                if config.has_option("Multinode", "quantum"):
                    quantum = config.get("Multinode", "quantum")
                    if quantum:
                        for i in self.__instances:
                            i.set_quantum(quantum)

            # Config files are valid
            return True
        else:
            self.__log.error("Config files are not valid")
            return False

    # Enable logging of the output of each qflex instance
    def set_save_output(self, output_path):
        if not os.path.isdir(output_path):
            os.makedirs(output_path)
        for i in self.__instances:
            i.set_save_output(output_path)

    # Set QMP dedicated to multi-system interface for all instances
    def set_qmp(self):
        DEFAULT_HOST_PORT = 8888
        for index, instance in enumerate(self.__instances):
            instance.set_qmp("default", DEFAULT_HOST_PORT + index + 1)

    # Print command of each qflex instance
    def print_cmd(self):
        for i in self.__instances:
            i.print_cmd()

    ## Execution
    # Start NS3 Network
    def start_network(self):
        self.__log.debug("Starting multinode network")
        names = [i.get_netname() for i in self.__instances]
        self.__ns_network = ns3network.net()
        self.__ns_network.setScriptPath(self.__ns3_path)
        try:
            self.__ns_network.configureNetwork(names)
        except subprocess.CalledProcessError as e:
            self.__log.error("Creating tap/bridge network failed\n CMD = {0}\n return code = {1}".format(e.cmd, e.returncode))
            self.exit()
            return

    # Start instances and maintain their execution and termination in a thread
    def start_execution(self):
        if self.__multinode:
            self.start_network()
        self.__log.debug("Starting Execution")
        thread = threading.Thread(target=self.start_execution_thread, args=(), name="start_execution")
        thread.daemon = True
        thread.start()
        # Wait for instances to launch, else qmp won't work
        # FixMe: this is an adhoc solution
        time.sleep(5)

    # Start instances and maintain their execution and termination
    def start_execution_thread(self):
        for i in self.__instances:
            i.start()
        iteration_count = 1;
        while True:
            if self.check_exit():
                self.__log.debug("Terminating execution")
                break
            if self.allStopped():
                iteration_count = iteration_count + 1
                self.__log.debug("Starting iteration {0}".format(iteration_count))
                self.iterate()
            if self.allTerminated():
                self.__log.debug("All instances terminated")
                self.exit()

    # Iterate between instances (when using quantum) and execute any pending QMP commands
    def iterate(self):
        for i in self.__instances:
            i.performPendingCmd()
            for i in self.__instances:
                i.takeTurn()

    ## Execution monitoring
    def allStopped(self):
        for i in self.__instances:
            if not i.stopped():
                return False
        return True

    def allTerminated(self):
        for i in self.__instances:
            if not i.terminated():
                return False
        return True

    ## QMP
    # Start qmp shell for each instance and process input for qmp shells in a thread
    def start_qmp(self):
        for i in self.__instances:
            i.startQMPshell()
        thread = threading.Thread(target=self.start_qmp_thread, args=(), name="start_qmp")
        thread.daemon = True
        thread.start()

    # Process input for qmp shells
    def start_qmp_thread(self):
        while True:
            if self.check_exit():
                break
            x = raw_input("qmp >>")
            if x == "kill":
                self.exit()
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

    ## Termination handling
    def exit(self):
        self.__exit_lock.acquire()
        try:
            if not self.__exit:
                self.__exit = True
                callerframerecord = inspect.stack()[1]
                frame = callerframerecord[0]
                self.__exit_info = inspect.getframeinfo(frame)
                self.cleanup()
        finally:
            self.__exit_lock.release()

    def get_exit_message(self):
        self.__exit_lock.acquire()
        try:
            return self.__exit_info.filename, self.__exit_info.function, self.__exit_info.lineno
        finally:
            self.__exit_lock.release()

    def check_exit(self):
        if self.__exit:
            return True
        else:
            return False

    def cleanup(self):
        for i in self.__instances:
            i.cleanup()
        try:
            if self.__multinode:
                if self.__ns_network:
                    self.__ns_network.cleanup()
        except subprocess.CalledProcessError as e:
            self.__log.critical("Bringing down tap/bridge network failed\n CMD = {0}\n return code = {1}".format(e.cmd, e.returncode))


    ## FixMe server code is under development
    # def start_server(self):
    #     thread = threading.Thread(target=self.start_server_thread, args=(), name="start_qmp")
    #     thread.daemon = True
    #     thread.start()

    # def start_server_thread(self):
    #     while True:
    #         try:
    #             x = raw_input("Your Wish Sire:")
    #             if x == "kill":
    #                 self.sendMessageToClients(x)
    #                 self.cleanup()
    #                 self.__exitstatus.setStatus('KILL')
    #                 # break
    #             elif x == "name":
    #                 self.sendMessageToClients(x)
    #             else:
    #                 self.sendMessageToClients(x)
    #             words = x.split(' ')
    #             if len(words) > 2 and len(words) < 2:
    #                 print "only two args are supported at the moment! not more not less"
    #             else:
    #                 one, two = words
    #                 if one == "save":
    #                     self.sendMessageToClients(one + " " + two)
    #                 if one == "load":
    #                     self.sendMessageToClients(one + " " + two)
    #                 if one == "get":
    #                     if two == "output":
    #                         for i in self.__instances:
    #                             out, err = i.getStdOutErr()
    #                             print len(out)
    #                             print len(err)
    #                             print (out, err)
    #                 else:
    #                     self.sendMessageToClients(one + " " + two)
    #         except ValueError:
    #             print "need more than 1 word"

    # def startHost(self):
    #     print "Starting server ..."
    #     print "NOTE: This can only be used with the multi-node protocol"
    #     self.__server = server.server(self.__hostaddress, self.__hostport)
    #     self.__server.setupServer()

    # def sendMessageToClients(self, msg):
    #     if self.__server is None:
    #         raise Exception, 'server is none'
    #     else:
    #         if self.__server.isInitialized():
    #             self.__server.queueMessage(msg)

    # def setupHost(self):
    #     thread = threading.Thread(target=self.startHost, args=(), name="server")
    #     thread.daemon = True # Daemonize thread
    #     thread.start()
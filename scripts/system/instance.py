import os
import Queue
import socket
import psutil
import logging
import threading
import ConfigParser
from subprocess import Popen, PIPE

import qmpmain
import helpers

qemu_path = "qemu/aarch64-softmmu/qemu-system-aarch64"

class instance:
    ## Constructor: create a new instance with an id, a name, and a config file
    def __init__(self, id, name, file):
        # Instance
        self.__id = id
        self.__name = name
        self.__file = file
        self.__log = logging
        # Instance output log
        self.__save_output = False
        self.__output_path = None
        self.__output_file = None
        # Instance process
        self.__cmd = []
        self.__process = None
        self.__pid = None
        self.__psutil = None
        # QMP
        self.__usingqmp = False
        self.__qmp = None
        self.__qmpshell = None
        self.__qmpip = None
        self.__qmpport = None
        self.__pendingCmdQueue = Queue.Queue()
        # NS3 Network
        self.__netname = None
        self.__netdev_tap = None

    ## Configuration
    # Set qflex command by parsing the config file
    def parse_instance(self):
        # Set up config parser
        config = ConfigParser.ConfigParser()
        config.read(self.__file)
        # qflex command
        qflex_command = []
        # qemu Executable
        qflex_path = config.get("Environment", "qflex_path")
        qflex_command.append(os.path.join(qflex_path, qemu_path))
        # Disk image
        image_repo_path = config.get("Environment", "image_repo_path")
        disk_image_path = config.get("Environment", "disk_image_path")
        disk_image_path = os.path.join(image_repo_path, disk_image_path)
        flash_path = os.path.join(os.path.dirname(disk_image_path),"flash")
        qflex_command.extend(["-global", "virtio-blk-device.scsi=off"])
        qflex_command.extend(["-device", "virtio-scsi-device,id=scsi"])
        qflex_command.extend(["-drive", "file=" + disk_image_path + ",id=hd0,if=none"])
        qflex_command.extend(["-device", "scsi-hd,drive=hd0"])
        qflex_command.extend(["-pflash", flash_path + "0.img"])
        qflex_command.extend(["-pflash", flash_path + "1.img"])
        # Number of cores
        smp = config.getint("Machine", "qemu_core_count")
        qflex_command.extend(["-smp", str(smp)])
        # Memory size
        mem = config.getint("Machine", "memory_size")
        qflex_command.extend(["-m", str(mem)])
        # Machine type
        machine = config.get("Machine", "machine")
        qflex_command.extend(["-machine", machine])
        # CPU type
        cpu = config.get("Machine", "cpu")
        qflex_command.extend(["-cpu", cpu])
        # Network
        # Only a limited set of the qemu network parameters is implemented
        # If more parameters are required, please request their addition
        if config.has_option("Machine", "user_network"):
            user_network = config.get("Machine", "user_network")
            if user_network == "on":
                net_id = config.get("Machine", "user_network_id")
                net_mac = config.get("Machine", "user_network_mac")
                net_hostfwd_protocol = config.get("Machine", "user_network_hostfwd_protocol")
                net_hostfwd_hostport = config.get("Machine", "user_network_hostfwd_hostport")
                net_hostfwd_guestport = config.get("Machine", "user_network_hostfwd_guestport")
                qflex_command.extend(["-netdev", "user,id=" + net_id + ",hostfwd=" + net_hostfwd_protocol + "::" + net_hostfwd_hostport + "-:" + net_hostfwd_guestport])
                qflex_command.extend(["-device", "virtio-net-device,mac=" + net_mac + ",netdev=" + net_id])
        # External Port Forwarding
        for port_option in ["serial", "parallel", "monitor", "qmp"]:
            if config.has_option("Machine", port_option):
                port_option_setting = config.get("Machine", port_option)
                if port_option_setting == "on":
                    port_option_dev = config.get("Machine", port_option + "_DEV")
                    qflex_command.extend(["-" + port_option, port_option_dev])
        # External snapshot
        if config.has_option("Environment", "starting_snapshot"):
            starting_snapshot = config.get("Environment", "starting_snapshot")
            if starting_snapshot:
                qflex_command.extend(["-loadext", starting_snapshot])
        # Simulation
        if config.has_section("Simulation"):
            # Simulation type
            simulation_type = config.get("Simulation", "simulation_type")
            # Phases
            if simulation_type == "phases":
                phases_name = config.get("Simulation", "phases_name")
                phases_length = config.get("Simulation", "phases_length")
                qflex_command.extend(["-phases", "steps=" + phases_length + ",name=" + phases_name])
            if simulation_type in {"checkpoints", "trace", "timing"}:
                # Flexus
                flexus_path = config.get("Simulation", "flexus_path")
                user_postload_path = config.get("Simulation", "user_postload_path")
                # Checkpoints
                if simulation_type == "checkpoints":
                    flexus_trace_path = config.get("Simulation", "flexus_trace_path")
                    flexus_trace_path = os.path.join(flexus_path, flexus_trace_path)
                    checkpoints_number = config.get("Simulation", "checkpoints_number")
                    checkpoints_length = config.get("Simulation", "checkpoints_length")
                    checkpoint_total_length = str(helpers.input_to_number(checkpoints_number) * helpers.input_to_number(checkpoints_length))
                    qflex_command.extend(["-flexus", "mode=trace,length=" + checkpoint_total_length + ",simulator=" + flexus_trace_path + ",config=" + user_postload_path])
                    qflex_command.extend(["-ckpt", "every=" + checkpoints_length + ",end=" + checkpoint_total_length])
                else:
                    # Simulation
                    simulation_length = config.get("Simulation", "simulation_length")
                    # Trace
                    if simulation_type == "trace":
                        flexus_trace_path = config.get("Simulation", "flexus_trace_path")
                        flexus_trace_path = os.path.join(flexus_path, flexus_trace_path)
                        qflex_command.extend(["-flexus", "mode=trace,length=" + simulation_length + ",simulator=" + flexus_trace_path + ",config=" + user_postload_path])
                    # Timing
                    elif simulation_type == "timing":
                        flexus_timing_path = config.get("Simulation", "flexus_timing_path")
                        flexus_timing_path = os.path.join(flexus_path, flexus_timing_path)
                        qflex_command.extend(["-flexus", "mode=timing,length=" + simulation_length + ",simulator=" + flexus_timing_path + ",config=" + user_postload_path])
                        # Hardwired features
                        qflex_command.extend(["-singlestep"])
                        qflex_command.extend(["-d", "nochain"])
                        qflex_command.extend(["-d", "in_asm"])
        # Name
        qflex_command.extend(["-name", self.__name])
        # Hardwired features
        qflex_command.append("-nographic")
        qflex_command.append("-exton")
        qflex_command.extend(["-accel", "tcg,thread=single"])
        qflex_command.extend(["-rtc", "driftfix=slew"])
        # qflex_command.extend(["-append", "'root=/dev/sda2'"])
        # qflex_command.extend(["-append", "'console=ttyAMA0'"])
        self.__cmd = qflex_command
        # Full command appears later before executing an instance
        # self.__log.debug("Command '{0}' set for instance '{1}'".format(' '.join(self.__cmd), self.__name))

    # Extend qflex command by a list of parameters
    def cmd_extend(self, x):
        self.__cmd.extend(x)

    # Add quantum parameter to qflex command
    def set_quantum(self, quantum):
        self.cmd_extend(["-quantum", "node=" + quantum])
        self.__log.debug("Quantum value {0} set for instance {1}".format(quantum, self.__name))

    # Add icount parameter to qflex command
    def set_icount(self, icount_shift, icount_sleep):
        self.cmd_extend(["-icount", "shift=" + icount_shift + ",sleep=" + icount_sleep])
        self.__log.debug("Icount set for instance {0}".format(self.__name))

    # Add network parameters, dedicated to NS3, to qflex command
    def set_net(self):
        self.__netname = "tap-{0}".format(self.__id)
        tap = "tap,id=network-{0},ifname={1},script=no,downscript=no".format(self.__id, self.__netname)
        self.__netdev_tap = tap
        self.cmd_extend(["-netdev", tap])
        tap_device = "virtio-net-device,mac=52:54:00:00:02:{0},netdev=network-{1}".format((self.__id+10),self.__id)
        self.cmd_extend(["-device", tap_device])
        self.__log.debug("NS3 network tap configured for instance {0}".format(self.__name))

    # Enable logging of the output of qflex
    def set_save_output(self, output_path):
        self.__save_output = True
        self.__output_path = os.path.join(output_path, self.__name) + ".log"

    # Add QMP shell, dedicated to multi-system interface, to qflex command
    def set_qmp(self, hostip, hostport):
        self.__usingqmp = True
        if (hostip == "default"):
            self.__qmpip = socket.gethostname()
        else:
            self.__qmpip = hostip
        self.__qmpport = hostport
        self.__qmp = "tcp:{0}:{1},server,nowait".format(self.__qmpip, self.__qmpport)
        self.cmd_extend(["-qmp", self.__qmp])
        self.__log.debug("QMP enabled for instance {0}, listening on {1}:{2}".format(self.__name, self.__qmpip, self.__qmpport))

    # Print the qflex command
    def print_cmd(self):
        print ("Instance '{0}': \n{1}".format(self.__name, ' '.join(self.__cmd)))

    # Get Net Name
    def get_netname(self):
        return self.__netname

    ## Manage Process
    # Start instance process
    def start(self):
        if self.__save_output:
            self.__output_file = open(self.__output_path, "w+")
            output = self.__output_file
        else:
            output = PIPE
        self.__process = Popen(self.__cmd, shell=False, stdin=PIPE, stdout=output, stderr=output)
        self.__pid = self.__process.pid
        self.__psutil = psutil.Process(self.__pid)
        self.__log.debug("Starting instance '{0}' with pid '{1}'".format(self.__name, self.__pid))
        self.__log.debug("Executing: {0}".format(' '.join(self.__cmd)))

    # Resume instance process after it stops (when using quantum)
    def takeTurn(self):
        if self.stopped():
            self.__log.debug("Resuming instance '{0}'".format(self.__name))
            self.__psutil.resume()

    def stopped(self):
        return self.__psutil.status() == psutil.STATUS_STOPPED

    def terminated(self):
        return self.__psutil.status() == psutil.STATUS_ZOMBIE

    ## QMP Functions
    def startQMPshell(self):
        if self.__usingqmp:
            adr = []
            adr.append('h')
            adr.append('v')
            if self.__id == 0:
                adr.append('c')
            adr.append("{0}:{1}".format(self.__qmpip, self.__qmpport))
            self.__qmpshell = qmpmain.qmp_shell(adr, self.__name)
            if self.__qmpshell is None:
                self.__log.critical("Unable to set up QMP for instance {0}".format(self.__name))
            else:
                self.__log.debug("QMP configured for instance '{0}'".format(self.__name))
            self.__qmpshell.enter()

    def performPendingCmd(self):
        while self.__pendingCmdQueue.not_empty:
            self.executeQMP(str(self.__pendingCmdQueue.get()+ "_" + self.__name))

    def executeQMP(self, cmd):
        if self.__qmpshell is not None:
            self.__log.debug("Instance '{0}' executing qmp command '{1}'".format(self.__name, cmd))
            thread = threading.Thread(target=self.executeQMP_thread, args=(cmd,), name="peding-req{0}".format(self.__name))
            thread.daemon = True
            thread.start()
        else:
            self.__log.debug("QMP is not set for instance '{0}', command '{1}' cannot execute".format(self.__name, cmd))

    def executeQMP_thread(self, cmd):
        try:
            self.__qmpshell.executecmd(cmd)
        except:
            self.__log.critical("Could not execute qmp command '{0}' for instance '{1}'".format(cmd, self.__name))

    def pushRequest(self, cmd):
        cmd = cmd.strip()
        self.__pendingCmdQueue.put(cmd)

    def nodeSpecificRequest(self, cmd):
        self.executeQMP(str(cmd.strip() + "_" + self.__name))

    ## Termination handling
    def cleanup(self):
        if self.__save_output:
            self.__output_file.close()

        if self.__usingqmp:
            self.__log.debug("Terminating instance '{0}' through QMP".format(self.__name))
            self.executeQMP('q')

        if psutil.pid_exists(self.__pid):
            self.__log.debug("Terminating instance '{0}'".format(self.__name))
            self.__psutil.terminate()
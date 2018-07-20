#!/usr/bin/python

import xml.etree.cElementTree as ET
import os.path
from subprocess import Popen, PIPE, check_output,STDOUT,CalledProcessError
import psutil
import re
import os
import mqmp
import threading
import Queue
import logging as log
import time
import socket

import mexec # for host address / port

DIR = os.path.dirname(os.path.realpath(__file__))
QFLEX_DIR = DIR.rsplit('/', 2)[0]

SAMPLE_TEXT = """{0}/qemu/aarch64-softmmu/qemu-system-aarch64 -machine virt -cpu cortex-a57 -smp 4
-m 2000 -kernel {0}/images/ubuntu-16.04-blank/vmlinuz-4.4.0-83-generic
-initrd {0}/images/ubuntu-16.04-blank/initrd.img-4.4.0-83-generic -append 'root=/dev/sda2'
-global virtio-blk-device.scsi=off -device virtio-scsi-device,id=scsi
-drive file={0}/images/ubuntu-16.04-blank/ubuntu-16.04-lts-blank.qcow2,id=rootimg,cache=unsafe,if=none
-device scsi-hd,drive=rootimg -rtc driftfix=slew -serial telnet:localhost:5555,server,nowait -name q1 -accel tcg,thread=single -nographic
-netdev user,id=net1,hostfwd=tcp::2220-:22 -device virtio-net-device,mac=52:54:00:00:02:12,netdev=net1""".format(QFLEX_DIR)


QEMU_SAMPLE_INSTANCE = str(DIR + "/qemu-instance-sample-file.xml")
QEMU_SAMPLE_SETUP = str(DIR + "/qemu-setup-sample-file.xml")

# class net:
#     def __init__(self):
#         self.__netdevice = self.__hostport = self.__guestport = self.__maddr = self.__id = None
#
#     def processDevice(self, text):
#         s = re.search("(.*),(.*),(.*)", text)
#         self.__netdevice= s.group(1)\
#         self.= s.group(2)
#             s.group(3)
#     def processNetdev(self,text):
# (.*),(.*)=(.*)::(\d+)-:(\d+)

class instance:
    def __init__(self):
        self.__cmd = []
        self.__psutil = psutil.Process()
        self.__proc = self.__pid = None
        self.__verbose = self.__usingqmp = self.__started = False
        self.__pendingCmdQueue = Queue.Queue()
        self.__debugOutput = self.__usingSudo = False

        #cmd opts
        self.__name = self.__netdev_tap = self.____netdev_user = self.__multinode = self.__quantum = self.__load = self.__exton = self.__accel = self.__icount = self.__loadvm = \
            self.__qmpip = self.__qmpport = self.__netname = self.__qmp = self.__optionalName = self.__id = \
            self.__filename = self.__appendcmd = self.__imageFile = self.__kernelFile = self.__initrdFile = None

        self.__qmpshell = None # qmpshell instance
        #self.__netdevice = net()
        self.__log = log.getLogger('mrun.logger')

    def getStdOutErr(self):
        out, err = self.__proc.communicate()
        return out, err

    def executeQMPInner(self,cmd):
        #self.__qmpshell.queueCommand(cmd)
        self.__qmpshell.executecmd(cmd)

    def cleanup(self):
        if self.__qmpshell is not None:
            self.__log.debug("sending clean-up request to instance {0}".format(self.getName()))
            self.executeQMP('q')

        self.__log.debug("instance: {0} Terminating" .format(self.__name))
        self.__psutil.terminate()

    def outputCMD(self):
        output = ""
        for i in self.__cmd:
            output+=i
            output += " "
        print self.__filename + ": " + output

    def executeQMP(self,cmd):
        if self.__qmpshell is not None:
            if (len(cmd) > 1):
                self.__log.debug("instance: {0} executing qmp command: {1}".format(self.__name, cmd))
            thread = threading.Thread(
                target=self.executeQMPInner, args=(cmd,), name="peding-req{0}".format(self.__name))
            thread.daemon = True  # Daemonize thread
            thread.start()
        else:
            self.__log.debug("instance: {0} qmp not enabled!: {1}".format(self.__name, cmd))

    def activateSudo(self):
        self.__log.debug("setting up sudo for NS3")
        self.__usingSudo = True

    def pushRequest(self, cmd):
        cmd = cmd.strip()
        #queue up requests
        self.__pendingCmdQueue.put(cmd)


    def nodeSpecificRequest(self, cmd):
        self.executeQMP(str(cmd.strip() + "_" + self.__name))

    def performPendingCmd(self):
        while self.__pendingCmdQueue.not_empty:
            self.executeQMP(str(self.__pendingCmdQueue.get()+ "_" + self.__name))

    def start(self):
        cmd = self.__cmd
        if self.__usingSudo:
            self.__log.debug("Execution: using sudo for instance{0}".format(self.__name))
            cmd.insert(0,'sudo')

        if self.__debugOutput:
            try:
                check_output(cmd)
            except CalledProcessError as e:
                print e.output
        else:
            self.__proc = Popen(cmd, shell=False,
                            stdin=PIPE, stdout=PIPE, stderr=PIPE)

            self.__log.debug("Executing: {0}".format(' '.join(cmd)))


            self.__pid = self.__proc.pid
            self.__log.info("starting instance: {0} with pid: {1}".format(self.__name, self.__pid))
            self.__psutil = psutil.Process(self.__pid)
            self.__log.debug("Execution: instance{0}: {1}".format(self.__name, self.__psutil.status()))
            self.__started = True

    def enableDebugOut(self):
        self.__debugOutput = True

    def __startQMPshellInner(self,adr):
        self.__qmpshell = mqmp.qmp_shell(adr, self.__name)
        if self.__qmpshell is None:
            self.__log.critical("unable to setup a qmp shell for instance {0}".format(self.__name))
        else:
            self.__log.debug("setting up a qmp shell for instance {0}".format(self.__name))

        self.__qmpshell.enter()

    def hasQMPCommand(self):
        if self.__qmp is not None and self.__qmpshell is not None:
            return True
        return False

    def startQMPshell(self):
        if self.__usingqmp:
            if self.__qmp is None:
                exit(1)
            s = []
            s.append('h')
            s.append('v')
            if self.__id == 0:
                s.append('c')

            s.append("{0}:{1}".format(self.__qmpip, self.__qmpport))

            self.__startQMPshellInner(s)

            # thread = threading.Thread(
            #     target=self.__startQMPshellInner, args=(s,), name="qmp-{0}".format(self.__name))
            # thread.daemon = True  # Daemonize thread
            # thread.start()

    def isStopped(self):
        return self.__psutil.status() == psutil.STATUS_STOPPED

    def isStarted(self):
        return self.__started

    def process_status(self):
        return self.__psutil.status()

    def isTerminated(self):
        return self.__psutil.status() == psutil.STATUS_ZOMBIE

    def getPid(self):
        return self.__pid

    def takeTurn(self):
        if self.isStopped:
            if self.__verbose:
                print "continuing. {0}".format(self.__name)
            self.__resume()

    def __resume(self):
        self.__psutil.resume()

    def append(self, x):
        self.__cmd.append(x)

    def valid(self):
        if self.__cmd[0] is None:
            return False
        return True

    def __hasName(self):
        return self.__name is not None

    def validateConfig(self):
        if self.__quantum is not None:
            if self.__accel is None:
                raise AssertionError(
                    "Quantum can only be used with single TCG and vice-versa - use '-accel tcg,thread=single'")
        if self.__accel is None and self.__icount is None:
            raise AssertionError(
                "Multinode is only supported in single TCG thread mode - use '-accel tcg,thread=single'")
        if str(self.__appendcmd).find("console") > 0:
            raise AssertionError(
                "Kernel Console is not supported in multi-node'")
        if self.__imageFile is not None:
            imgfile = str(self.__imageFile)[str(self.__imageFile).find("/"):str(self.__imageFile).find(","):]
            try:
                statinfo = os.stat(imgfile)
                if statinfo.st_size < (20*1024*1024): # less than 20mb
                    raise Exception, "your image File is too small{0}\n did you fetch/pull it?".format(imgfile)
            except OSError as e:
                raise Exception, "your image File is not valid...{0}".format(imgfile)

        if self.__kernelFile is not None:
            statinfo = os.stat(str(self.__kernelFile))
        if self.__initrdFile is not None:
            statinfo = os.stat(str(self.__initrdFile))


    def processConfigs(self):
        for index, item in enumerate(self.__cmd):
            #cleanup
            if item is None:
                continue
            # if item == "-device" and self.__cmd[index + 1] == "virtio-net-device":
            #     self.__netdevice.process(self.__cmd[index + 1])
            #
            #     else:
            #         raise Exception, "found dups"

            if (item == "-name"):
                self.__name = self.__cmd[index+1]
                self.__cmd[index+1] = self.__name
            elif (item == "-netdev" and self.__cmd[index+1].startswith("tap")):
                self.__netdev_tap = self.__cmd[index+1]
                self.__cmd[index+1] = self.__netdev_tap
                raise Exception, "Multi-node will define its own tap options. you dont need to provide them manually"
            elif (item == "-netdev" and self.__cmd[index+1].startswith("user")):
                self.__netdev_user = self.__cmd[index+1]
                self.__cmd[index+1] = self.__netdev_user
            elif (item == "-multinode"):
                self.__multinode = self.__cmd[index+1]
                self.__cmd[index+1] = self.__multinode
            elif (item == "-quantum"):
                self.__quantum= self.__cmd[index+1]
                self.__cmd[index+1] = self.__quantum
            elif (item == "-loadext"):
                self.__load= self.__cmd[index+1]
                self.__cmd[index+1] = self.__load
            elif (item == "-exton"):
                self.__exton= True
            elif (item == "-icount"):
                self.__icount = self.__cmd[index+1]
                self.__cmd[index+1] = self.__icount
            elif (item == "-loadvm"):
                self.__loadvm = self.__cmd[index+1]
                self.__cmd[index+1] = self.__loadvm
            elif (item == "-accel"):
                self.__accel = self.__cmd[index+1]
                self.__cmd[index+1] = self.__accel
            elif (item == "-qmp"):
                self.__qmp = self.__cmd[index + 1]
                self.__cmd[index + 1] = self.__qmp
                self.__usingqmp = True
                s = re.search("\w+:(.*):(\d+),",self.__cmd[index + 1])
                if s is not None:
                    self.__qmpip = s.group(1)
                    self.__qmpport = s.group(2)
                else:
                    raise Exception, "could not parse your qmp option in {0} - make sure its the correct format.".format(self.__filename)
            elif (item == "-append"):
                self.__appendcmd = self.__cmd[index+1]
                self.__cmd[index+1] = self.__appendcmd
            elif (item == "-drive"):
                self.__imageFile = self.__cmd[index + 1]
                self.__cmd[index + 1] = self.__imageFile
            elif (item == "-kernel"):
                self.__kernelFile = self.__cmd[index + 1]
                self.__cmd[index + 1] = self.__kernelFile
            elif (item == "-initrd"):
                self.__initrdFile = self.__cmd[index + 1]
                self.__cmd[index + 1] = self.__initrdFile

        sname = self.__filename.split('/')[-1]
        sname = sname.split('.')[0]
        self.__setOptionalName(sname)

        if (self.__name is None):
            self.setName(self.__optionalName)

    def hasQMPopts(self):
        return self.__usingqmp

    def setQMPOpts(self, hostip,hostport):
        if (hostip == "default"):
            self.__qmpip = socket.gethostname()
        else:
            self.__qmpip = hostip

        self.__qmpport = hostport

        if (self.__qmp is None):
            self.__qmp = "tcp:{0}:{1},server,nowait".format(self.__qmpip,self.__qmpport)
            self.__cmd.extend(["-qmp",self.__qmp])
        # else:
        #     self.__qmp = "tcp:{0}:{1},server,nowait".format(self.__qmpip, self.__qmpport)
        self.__usingqmp = True



        return self.__qmpip, self.__qmpport

    def setName(self, name):
        print "no name given! adding name to your command for instance {0}".format(self.__id)
        self.__name = name  # extracted from filename
        self.__cmd.extend(["-name", self.__name])

    def setMultinodeOpts(self, hostip, hostport, clientip=None, clientport=None):
        print "no multinode config given! adding multinode config to your command for instance {0}".format(self.__id)

        if clientip is not None and clientip is not None:
            self.__multinode = "hostip={0},hostport={1}, clientip={2}, clientport={3}".format(hostip, hostport, clientip, clientport)
        else:
            self.__multinode = "hostip={0},hostport={1}".format(hostip, hostport)

        if self.__multinode is None:
            self.__cmd.extend(["-multinode", self.__multinode])

    def __setupTapNetwork(self):
        if self.__netdev_tap is None:
            s = "tap,id=network-{0},ifname={1},script=no,downscript=no".format(self.__id, self.__netname)
            self.__cmd.extend(["-netdev", s])

            s = "virtio-net-device,mac=52:54:00:00:02:{0},netdev=network-{1}".format((self.__id+10),self.__id)
            self.__cmd.extend(["-device", s])
        else:
            s = "tap,id=network-{0},ifname={1},script=no,downscript=no".format(self.__id, self.__netname)
            self.__netdev_tap = s

            s = "virtio-net-device,mac=52:54:00:00:02:{0},netdev=network-{1}".format((self.__id+10),self.__id)
            self.__cmd.extend(["-device", s])

    def setNetdev(self):
        self.__log.debug("Setting NS3 network name for instance {0}".format(self.__name))
        self.__netname = "tap-{0}".format(self.__id)
        self.__setupTapNetwork()

    def setextopts(self):
        if self.__exton is False:
            self.__log.debug("Setting exton for instance {0}".format(self.__name))
            self.__cmd.extend(["-exton", None])
        else:
            self.__log.debug("NOT Setting exton for instance {0} as it has it".format(self.__name))

    def setLoadopts(self, val):
        self.__log.debug("Setting load value {0} name for instance {1}".format(val, self.__name))
        if self.__load is None:
            self.__load = "{0}_{1}".format(val, self.__name)
            self.__cmd.extend(["-loadext",self.__load])
        else:
            self.__load = "{0}-{1}".format(val, self.__name)
            self.__log.warning("overwriting existing load option for instance {0}".format(self.__name))

    def setQuantum(self, val):
        self.__log.debug("Setting quantum value {0} name for instance {1}".format(val, self.__name))
        if self.__quantum is None:
            self.__quantum = "node={0}".format(val)
            self.__cmd.extend(['-quantum',self.__quantum])
        else:
            self.__quantum = "node={0}".format(val)
            self.__log.warning("overwriting existing quantum option for instance {0}".format(self.__name))

    def getNetName(self):
        return self.__netname

    def updateFile(self):
        self.writeToFile()

    def writeToFile(self, file=None):
        if self.__filename is not None:
            out = ""
            for c in self.__cmd:
                if c is not None:
                    out += c + " "

            config().ConvertTextToFile(out, self.__filename if file is None else file)

    def setFilename(self, filename):
        self.__filename = filename

    def setID(self, id):
        self.__id = id

    def getID(self):
        return self.__id

    def getFilename(self):
        return self.__filename

    def getName(self):
        return self.__name

    def __setOptionalName(self, opt):
        self.__optionalName = opt

class config:
    def __init__(self):
        self.__instancefiles = []
        self.__setup = None

    def __generateInstanceFile(self, text=None, outfile=None):
        print "Generating instance file"
        if outfile is None:
            outfile = QEMU_SAMPLE_INSTANCE
        if text is None:
            text = SAMPLE_TEXT

        args = re.compile("(?<!\S)(-\w+)").split(text)

        inst = ET.Element("instance")
        qemu = ET.SubElement(inst,  "executable")

        for index,item in enumerate(args):
            if item == args[0]:
                ET.SubElement(qemu,  "binary").text = item
            elif item.startswith('-'):
                parameter = ET.SubElement(inst,  "parameter")
                ET.SubElement(parameter, "enabled").text = "on"
                ET.SubElement(parameter, "option").text = item
                ET.SubElement(parameter, "arg").text = args[index + 1]

        tree = ET.ElementTree(inst)
        tree.write(outfile)
        print "done generating %s" % outfile

    def __generateSetupFile(self, text=None, outfile=None):
        print "Generating setup file"

        if outfile is None:
            outfile = QEMU_SAMPLE_SETUP
        if text is None:
            text = QEMU_SAMPLE_INSTANCE

        setup = ET.Element("setup")
        ins = ET.SubElement(setup, "instance")
        ET.SubElement(ins, "file").text = text
        tree = ET.ElementTree(setup)
        tree.write(outfile)
        print "done generating %s" % outfile

    def ConvertTextToFile(self, intext, outfile):
        self.__generateInstanceFile(intext, outfile)

    def generateSamples(self):
        self.__generateSetupFile()
        self.__generateInstanceFile()


    def parse(self, filename):
        self.__parseSetup(filename)

        instances = []
        for index, item in enumerate(self.__instancefiles):
            inst = self.__parseInstance(item)
            inst.setID(index)
            inst.processConfigs()
            instances.append(inst)
        return instances

    def __parseInstance(self, filename):
        root = ET.parse(filename).getroot()

        if not root.findall('executable'):
            raise Exception, '%s is Not an appropirate instance file - I need an XML file - use generate to create a sample: %s' % filename

        inst = instance()

        for i in root.findall('executable'):
            inst.append((i.find('binary').text).strip())

            if not inst.valid():
                raise Exception, "{0} is Not an appropirate instance file - I need an XML file - use generate to create a sample".format(
                    filename)

        for i in root.findall('parameter'):
            if(i.find('enabled').text == "on"):
                inst.append((i.find('option').text).strip())
                try:
                    text = i.find('arg').text
                    if text is None or text == "" or text.isspace():
                        continue
                    else:
                        inst.append((i.find('arg').text).strip())
                except:
                    print "err"
                # if i.find('arg').text is not None or i.find('arg').text != "" or not i.find('arg').text.isspace():
                #     inst.append(i.find('arg').text)

        inst.setFilename(filename)
        return inst

    def __parseSetup(self, filename):
        self.__setup = filename
        root = ET.parse(filename).getroot()

        if not root.findall('instance'):
            raise Exception, '%s is Not an appropirate setup file - I need an XML file - use generate to create a sample: %s' % filename

        for i in root.findall('instance'):
            path = i.find('file').text
            if not os.path.exists(path):
                raise Exception, 'File does not exist: %s' % path
            self.__instancefiles.append(path)

        if len(self.__instancefiles) == 0:
            raise Exception, '%s is Not an appropirate setup file - i couldnt find any file paths: %s' % filename
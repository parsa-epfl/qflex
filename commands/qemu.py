import os
from commands.config import ExperimentContext


def wrap_with_gdb(qemu_invocation: str, use_gdb: bool,
                  interactive_tmux: bool = False) -> str:
    # `set confirm off` replaces the old `yes | gdb` shutdown-prompt workaround:
    # works for non-interactive runs and doesn't flood gdb with "y" when a user
    # Ctrl+Cs inside an interactive tmux pane.
    #
    # `< /dev/null` keeps qemu's stdin off the parent's tty so:
    #   - tcsetattr in a stdio-serial setup (init-warm / fw / run-* / Path A)
    #     returns ENOTTY instead of raising SIGTTOU on a background-pgid'd qemu.
    #   - any gdb-trapped signal (e.g. quit-time SIGSEGV in pdes_comm_send) hits
    #     a (gdb) prompt that reads EOF on /dev/null → gdb exits cleanly →
    #     bash unwinds.
    # Skip the redirect when interactive_tmux is True — Path B routes the tmux
    # pane's pty into qemu's stdin so the user can type into the serial console.
    redirect = "" if interactive_tmux else " < /dev/null"
    if not use_gdb:
        return f"{qemu_invocation}{redirect}"
    return f"gdb -ex 'set confirm off' -ex run --args {qemu_invocation}{redirect}"


class QemuCommonArgParser:
    def __init__(self, 
                 experiment_context: ExperimentContext,
                 use_stdio: bool = True):
        self.experiment_context = experiment_context
        self.simulation_context = self.experiment_context.simulation_context
        self.image_address = self.experiment_context.get_local_image_address()
        self.memory_size_mb = self.experiment_context.simulation_context.memory_gb * 1024
        self.cores = self.experiment_context.simulation_context.core_count
        self.double_cores = self.experiment_context.simulation_context.doubled_vcpu
        self.core_coeff = 1
        if self.double_cores:
            self.core_coeff = 2

        self.use_stdio = use_stdio
        self.node_number = self.experiment_context.node_number


    
        
        self.cd_rom = ''
        self.use_cd_rom = self.simulation_context.use_cd_rom
        if self.use_cd_rom:
            alpine_image_name = 'alpine-virt-3.22.1-aarch64.iso'
            experiment_folder = self.experiment_context.get_experiment_folder_address()
            if not os.path.isfile(f'{experiment_folder}/images/{alpine_image_name}'):
                alpine_url = f'https://dl-cdn.alpinelinux.org/alpine/v3.22/releases/aarch64/{alpine_image_name}'
                os.system(f'wget {alpine_url} -O {experiment_folder}/images/{alpine_image_name}')
            self.cd_rom = f'-cdrom {experiment_folder}/images/{alpine_image_name} '
        


    def get_load_vm(self) -> str:
        loadvm = ''
        if self.experiment_context.loadvm_name is not None and len(self.experiment_context.loadvm_name) > 0:
            loadvm = f' -loadvm {self.experiment_context.loadvm_name}'
        return loadvm

    def get_seed_image_arg(self) -> str:
        return f""" -drive if=virtio,file={self.experiment_context.seed_image_address},format=qcow2 """

    def get_base_image_arg(self) -> str:
        return f"""-drive if=virtio,file={self.image_address},format=qcow2 """

    def get_image_arg(self) -> str:
        image_arg = self.get_base_image_arg()
        if len(self.experiment_context.seed_image_name) > 0:
            print(f"Using seed image {self.experiment_context.seed_image_name} at address {self.experiment_context.seed_image_address}")
            image_arg += self.get_seed_image_arg()
        return image_arg
    
    def get_stdio(self):
        if self.use_stdio:
            return " -serial mon:stdio "

        # Non-stdio paths.
        exp = self.experiment_context
        parts = []
        if exp.interaction_script:
            # Path A (scripted boot/load): expose serial on telnet for the expect
            # script AND have qemu mirror everything to qemu_serial.log so the
            # boot/console output survives even when expect dies before it can
            # connect. `-chardev socket` with logfile=... is what lets us do
            # both at once; `-serial telnet:...` shorthand can't.
            log_path = f"{exp.get_experiment_folder_address()}/qemu_serial.log"
            parts.append(
                f"-chardev socket,id=qflex_serial,host=127.0.0.1,"
                f"port={exp.serial_telnet_port},server=on,wait=off,telnet=on,"
                f"logfile={log_path},logappend=on"
            )
            parts.append("-serial chardev:qflex_serial")
        else:
            # Default no-stdio behaviour: log serial to a file, no interactive monitor.
            parts.append("-serial file:serial.log")

        # Suppress the monitor only when it isn't routed to telnet — emitting both
        # `-monitor telnet:...` and `-monitor none` would conflict.
        if not exp.use_telnet_monitor:
            parts.append("-monitor none")

        return " " + " ".join(parts) + " "

    def get_qemu_base_args(self) -> str:

        # TODO REMOVE the hardcoded drive 
        # TODO remove second seed file
        # self.node_number = 1
        # self.image_address = '/mnt/sdb/pooria-multi-node/vanilla-image-node1.img'

        image_arg = self.get_image_arg()
        loadvm = self.get_load_vm()

        telnet_monitor_arg = ''
        # Tmux mode (Path B) reaches the monitor from the same pane via Ctrl-A C
        # on `-serial mon:stdio`, so a separate telnet endpoint is unnecessary.
        # Script mode (Path A) and non-interactive subprocess runs both still need
        # it — expect scripts and external automation can't multiplex via Ctrl-A.
        if (self.experiment_context.use_telnet_monitor
                and not self.experiment_context.interactive_tmux):
            print(f"Using telnet monitor at port {self.experiment_context.telnet_port}")
            # Same chardev-with-logfile pattern as get_stdio() so monitor traffic
            # is preserved on disk for post-mortem inspection.
            log_path = f"{self.experiment_context.get_experiment_folder_address()}/qemu_monitor.log"
            telnet_monitor_arg = (
                f" -chardev socket,id=qflex_monitor,host=127.0.0.1,"
                f"port={self.experiment_context.telnet_port},server=on,wait=off,telnet=on,"
                f"logfile={log_path},logappend=on"
                f" -monitor chardev:qflex_monitor "
            )
        
        
        qemu_args = f""" -M virt,gic-version=max,virtualization=off,secure=off \
        -smp {self.core_coeff * self.cores} \
        -cpu max,pauth=off -m {self.memory_size_mb} \
        -boot order=d,menu=on \
        -bios ./QEMU_EFI.fd \
        {image_arg} \
        -rtc clock=vm \
        {loadvm} \
        {self.cd_rom} \
        {self.simulation_context.qemu_nic} \
        {telnet_monitor_arg} \
        {self.get_stdio()} -nographic -no-reboot """

        # Time discipline (`-quantum` / `-icount`) is part of the qemu cmdline
        # this parser owns. Callers do not — and must not — append their own;
        # they get the right one for this parser's binary type via dispatch.
        qemu_args += self.quantum_args()

        print("="*50+"QEMU command arguments:"+"="*50)
        print(qemu_args)
        return qemu_args

    def quantum_args(self) -> str:
        # TODO move this to its own class
        check_period_quantum_coeff = self.simulation_context.check_period_quantum_coeff
        quantum_command = ''
        # TODO check why 53 : checked this is a check done to see whether or not we need to do checkpointing, with the assumption being it will usually be way less than the sampling interval
        if self.simulation_context.is_parallel:
            quantum_command = f'   -quantum size={self.simulation_context.quantum_size},check_period={int(self.simulation_context.quantum_size * check_period_quantum_coeff)} '
        else:
            quantum_command = f'   -icount shift=0,align=off,sleep=off,q={self.simulation_context.quantum_size},check_period={int(self.simulation_context.quantum_size * check_period_quantum_coeff)} '

        print("="*50+"Quantum command arguments:"+"="*50)
        print(quantum_command)
        return quantum_command


class VanillaQemuArgParser(QemuCommonArgParser):

    def __init__(self,
                 experiment_context: ExperimentContext,
                 idx: int,
                 total_cycles: int,
                 use_stdio: bool = True):
        super().__init__(experiment_context, use_stdio)
        self.idx = idx
        self.total_cycles = total_cycles



    def get_load_vm(self):
        return f"""-loadvm snapshot_{self.idx},on-demand"""
    
    def get_base_image_arg(self) -> str:
        return f"""-drive if=virtio,file={self.image_address},format=qcow2,snapshot=on,tmp-snapshot-name=snapshot_{self.idx} """
    
    def quantum_args(self) -> str:
        return f'   -icount shift=0,align=off,sleep=off '
    
    def get_qemu_base_args(self) -> str:
        # super() already appends self.quantum_args() (overridden above to
        # `-icount …` for the timing binary), so we only add the
        # timing-specific pieces here.
        base_args = super().get_qemu_base_args()
        single_step_command = f""" -singlestep -d nochain """
        log_command = f""" -D "qemu-timing.log" """
        lib_qflex_command = f""" -libqflex """
        lib_name = "libsemikraken" if self.double_cores else "libknottykraken"
        mode_command = f""" mode=timing,lib-path=../../lib/"{lib_name}".so,cfg-path=../../cfg/timing.cfg,cycles={self.total_cycles}:100000,debug=crit,ckpt-path=./snapshot_{self.idx}-flexus,freq={int(self.experiment_context.workload.IPC_info.machine_freq_ghz)} """

        qemu_args = base_args + \
        single_step_command + \
        log_command + \
        lib_qflex_command + \
        mode_command
        print("="*50+"QEMU command arguments:"+"="*50)
        print(qemu_args)
        return qemu_args
        


        











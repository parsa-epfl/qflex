THIS README IS OUTDATED

To run an instance of QEMU using predefined scripts you need to create **user.cfg** file in `scripts/` folder.
*This file acts as a user-specific configuration file which will be sourced to the script `run_instance.sh`.*

For help, one can copy **user_example.cfg** file and adjust the user-specific environmental variables.

There are two main scripts designed to run QEMU, **run_instance.sh** and **run_system.sh**.

**run_instance.sh** can be configured to run in different operating modes to boot or load a single instance of QEMU. All network configurations have to be done manually.

**run_system.sh** is an automation of `run_instance.sh` in which the entire booting, configuring, command and termination of the QEMU instance or instances is done automatically. To run commands in the remote hosts booted through run_system.sh there is a commands folder in which commands for the remote host can be declared in scripts with the name cmd_n.sh. Here the `n` stands for the last number of the port of the remote host you want the commands to be executed.

**test.sh** in the folder `test_run_system/` is used to test if all the basic functionalities of the `run_system.sh` script are working as expected.
...

### A list of possible configuration options are given below:

* `-mult` to set up multiple instances (default: single instance)
* `-lo=<snapshot>` to load a certain snapshot (default: boot)
* `-tr` to use with Flexus (trace)
* `--no_net` to use single instance without user network
* `--no_ns3` to run multiple instances without NS3
* `--kill` to kill the qemu instances after the automated run add the --kill option
* `-sf` to manually set the SIMULATE_TIME for Flexus (default $SIMULATE_TIME)
* `-uf` to specify the user file. e.g. -uf=user1.cfg to use user1.cfg (default: user.cfg)
* `-exp=<name>` to name your log directory (default name: run)
* `-ow` to overwrite in an existing log directory
* `-sn=<snapshot>` to take a snapshot with the specified name after commands in run_system.sh
* `-h` for help

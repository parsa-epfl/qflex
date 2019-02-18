# QFlex Automation Initiative
We provide a set of scripts to simplify launching simulations using QFlex. `run_system` is the top level script that launches simulation for a single or multiple QFlex instances. `run_job` allows running a series of simulations that follow the SMARTS methodology.

`run_system` requires a `system.ini` configuration file, which in turn requires one or more `instance.ini` configuration files.

`run_job` requires a `job.ini` configuration file which is an extension of a system configuration file.

Sample configuration files can be found under `config`.

## Instance
An instance is the simulator building block unit. It corresponds to a single QFlex simulated machine. Each instance can be configured using an `ini` configuration file. The configuration file is split to three sections: environment, machine, and simulation. Environment parameters are associated to files present on the host machine and required by the emulated machine (e.g., qflex repository path, image path, etc.). Machine parameters indicate the QEMU configuration parameters for the emulated machine (e.g., core count, memory size, network, external ports, etc.) Simulation parameters represent all parameters needed for simulation which includes both simulation files on the host machine (flexus path, trace and timing simulators, etc.) and simulation parameter.

### Environment Parameters
* **qflex_path**
Path to the QFlex repository, used to locate QEMU executable.
* **image_repo_path**
Base path to the images directory. All image-related files must be in the same directory to ease deployment.
* **disk_image_path**
Path to the disk image relative to `image_repo_path`. Flash images `flash0.img` and `flash1.img` must exist in the same path.
* **starting_snapshot**
Name of the starting snapshot, if any, which is an external snapshot from the base disk image.

### Machine Parameters
Details for QEMU parameters can be found at: https://qemu.weilnetz.de/doc/qemu-doc.html.
* **qemu_core_count**
Number of cores, integer value.
* **memory_size**
Size of memory, in bytes.
* **machine**
Machine parameters as specified in the documentation. Assigned to `virt` unless otherwise required.
* **cpu**
CPU model. `cortex-a57` is currently supported.
* **user_network**
Enable user network by setting this parameter to `on`. The below parameters must also be set to configure the network. If user network is not needed this parameter can be set to `off` or can be omitted (along with the following parameters).
    * **user_network_id**
    Name of the network id.
    * **user_network_mac**
    Network mac address.
    * **user_network_hostfwd_protocol**
    Network host forwarding protocol, as supported by QEMU.
    * **user_network_hostfwd_hostport**
    Network host forwarding port.
    * **user_network_hostfwd_guestport**
    Network guest forwarding port.
* **qmp**
Redirect external qmp monitor to given device. If qmp is not needed this parameter can be set to `off` or can be omitted (along with the following parameters).
    * **qmp_dev**
    Parameters for qmp device as specified in the documentation.
* **serial**
Redirect virtual serial port to given device. If serial is not needed this parameter can be set to `off` or can be omitted (along with the following parameters).
    * **serial_dev**
    Parameters for serial device as specified in the documentation.

### Simulation Parameters
* **flexus_path**
Base path to the flexus repository. All flexus-related files must be in the same directory to ease deployment.
* **flexus_trace_path**
Path to the trace simulator relative to `flexus_path`.
* **flexus_timing_path**
Path to the timing simulator relative to `flexus_path`.
* **user_postload_path**
Path to the flexus user postload configuration file.
* **simulation_type**
Simulation type, can take values {`emulation`, `trace`, `timing`, `phases`, `checkpoints`}.
The following simulation parameters are required depending on the simulation type.
    * **simulation_length**
    Length of the simulation for `trace` or `timing`.
    * **phases_name**
    Name of the phases external snapshots created.
    * **phases_length**
    The length of each phase separated by a column, e.g., `1:2:30:4k:5m:6b`, where large numbers can be expressed using numerical representations. The number of phases is infered by the number count in the value of the parameter.
    * **checkpoints_number**
    Number of checkpoints.
    * **checkpoints_length**
    The length of each checkpoint, where large numbers can be expressed using numerical representations.


## System
A system runs and controls a single or multiple instances of QFlex simulated machines. A system has an `ini` configuration file which specifies the instances, the system parameters, and the multinode network parameters; in each in a separate section.

### Instances
Each instance in the system must be specified under the `Instances` section in the system configuration file. The entry specifies the name of the instance and the configuration file of the instance. Each instance must have a unique name, however, multiple instances can share the same configuration file.
An example of an instance entry is:
```i0 = configuration/instance.ini```
The entry shows an instance named `i0` based on a configuration file `configuration/instance.ini`. This relative path is resolved based on Python's working directory.

### System
System parameters in this section are parameters that are instance parameters but must be associated to all instances in a system. To prevent any mis-configuration, these parameters are set in the system configuration file. Note that a system doesn't define how each instance is emulated. This is left to be specified per instance.
* **icount**
Enable icount for instances by setting this parameter to `on`. The below parameters must also be set. If icount is not needed this parameter can be set to `off` or can be omitted (along with the following parameters). More details can be found in QEMU documentation.
    * **icount_shift**
    icount shift amount, in integers.
    * **icount_sleep**
    Enable icount sleep mode by setting this parameter to `on` to enable it. Else `off` disables it.

### Multinode
In order to set network connections between multiple instances, we use an underlying network simulator. In this section we configure the details of this simulator.
* **ns3_path**
Path to ns3 network simulator executable.
* **quantum**
Value of quantum per instance, where large numbers can be expressed using numerical representations.


## Job
A job runs a simulation following the SMARTS methodology. A job takes a system and creates phases, generate checkpoints for each phase, and then runs timing simulation per checkpoint. A job configuration file is an extension to a system configuration file. It has the same sections with few additions. The major different factor is that the simulation details in all instances are ignored when running a job as the job redefines these details in the job section. It also adds a host section that allows kubernetes deployment. Please note that all instances in a job must have their flexus simulator and images in the same directory to ease deployment.

### Job
In a job, all instances are emulated except the master instance where the SMARTS methodology is applied. Three different simulation types are performed on a system undergoing a job: creating phases, generating checkpoints, and simulating in timing. The parameters needed for these simulation (defined for the `simulation` section for an `instance`) must be provided in the `job` section. This includes `flexus_path`, `flexus_trace_path`, `flexus_timing_path`, `user_postload_path`, `phases_name`, `phases_length`, `checkpoints_number`, `checkpoints_length`, and `simulation_length`.
* **master_instance**
The name of the master instance, selected from one of the instances listed under `Instances` section (desribed in `System`).
* **image_repo_path**
Base path to the images directory. All image-related files **for all instances** must be in the same directory to ease deployment. (This also applies to flexus reposistory).

### Host
Jobs can be deployed using kubernetes. Each simulation (running a system simulation once) is deployed on a container. Kubernetes insures that all simulations terminate and repeat them in case of failure. When deploying, all paths in configuration files refer to those in the container and not the host machine. However, for convenience, the images and flexus are mounted to the container rather than pre-installed, to allow modifications (creating phases and checkpoints, updating simulator, etc.). Hence, we require three essential parameters.
* **docker_image_name**
Name of the docker image which will spawn containers for deployment.
* **host_flexus_path**
Base path to the flexus repository on the host machine.
* **host_image_repo_path**
Base path to the images directory on the host machine.

## Note
It is advised to use full paths rather than relative paths in configuration files.

The scripts require two libraries from pip: psutil & netifaces.

## Understanding the Code Architecture

To understand the set of scripts in hand, we will first discuss `run_system` and its components. Then, we will discuss how `run_job` builds on top of `run_system`. Finally, we discuss how continuous integration is implemented using those scripts.

### Run System
`run_system` is the top level script that runs in a single simulation. A simulation is running a single or multiple instances of QFlex, which we call a system. `run_system` is a wrapper around `system` class. The `system` class has three main objectives: sets up the QFlex instances that consist the system, set up the connections between these instances, and run these instances in tandem. To set up instances, `system` uses the `instance` class. The `instance` class allows setting up a single instance by parsing its configuration file, maintaining resources to run the instance, and controlling these resources. To set up connections between instances, `system` uses `ns3network` class. The `ns3network` uses ns3 simulator to create bridges and taps between instances. To run instances, `system` use python `threading` library. It also allows issuing monitor commands to all instances through qmp. The qmp interface is implemented in `qmpmain` and in the `qmp` folder. All files for run_system can be found under `scripts/system` directory.

To validate configuration files, a special class `validation` is used. Validation is implemented outside `instance` and `system` classes to achieve modularity especially that validation can occur in various points in the scripts.

### Run Job
`run_job` runs a series of simulations following the SMARTS methodology, which we call a job. Each job uses the same instances, but changes the configuration slightly between one simulation and another. There are two main changes that occur across simulations: different initial snapshot  and different simulation parameters are used per instance. To achieve that, `run_job` writes a new set of configuration files to achieve these changes. All new configuration files are saved under the working directory.

#### Job Deployment
To accelerate simulation, the scripts support deployment in kubernetes. Given a docker image with QFlex repository and depedendencies installed, `run_job` runs each simulation in a docker container by mounting three folders: the new configuration files set for the experiment, the disk images folder, and the flexus simulator folder. Deployment is done by calling `deploy_system` function implemented under `deploy`. The function simply generates a `yml` file with needed parameters to issue a kubernetes task. **This feature has not been fully tested with multinode system simulation**.

A docker image suitable for this work was set up by Abhinav. A variation of his work can be found in this version of the repository. However this image is not final and requires revision.

### Testing
We use Travis for continuous integration. Every time we push a commit to the QFlex repository, Travis runs the test described in `.travis.yml`. The test runs two scripts: `build_qflex` and `test_qflex`. `build_qflex` installs qflex and its dependencies in the test environment. `test_qflex` runs a series of tests that checks if simulator runs in various modes.

Under `travis` directory, we have a new implemenation of `test_qflex` script (named `test_travis_new.sh`). This new implementation makes use of two scripts. The first is `config_generator` which is a python script that generates new configuration files for the required tests. The second is `test_system` which is has integrated features to test a system. These features include saving and loading snapshots, connecting to a snapshot through ssh, and running basic operations on the instances.

Please note that the new version of tests (`test_travis_new.sh`) is missing some test cases and has not been properly tested to be pushed to Travis CI. Please insure that before the Travis CI integration is done.

`build_qflex` script is outdated and requires modification to match the current state of the QFlex repository.